#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

// A compact mono hall-reverb generator intended to feed a binaural/HRIR
// spatialisation stage. It provides separate early-reflection and late-tail
// outputs so they can be positioned at different azimuths if desired.
class HallReverbEngine
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = std::max (1.0, newSampleRate);

        // Enough history for 250 ms predelay plus the latest early-reflection tap.
        inputHistory.prepare (millisecondsToSamples (350.0f));

        constexpr std::array<float, numDiffusers> diffuserTimesMs
        {
            5.0f, 7.1f, 9.3f, 12.1f
        };

        constexpr std::array<float, numLines> fdnTimesMs
        {
            29.7f, 34.9f, 40.3f, 46.1f,
            52.7f, 60.1f, 68.3f, 77.9f
        };

        for (std::size_t i = 0; i < numDiffusers; ++i)
        {
            diffusers[i].prepare (millisecondsToSamples (diffuserTimesMs[i]));
            diffusers[i].gain = 0.68f;
        }

        for (std::size_t i = 0; i < numLines; ++i)
        {
            const int delaySamples = millisecondsToSamples (fdnTimesMs[i]);
            fdnLines[i].prepare (delaySamples);
            fdnDelaySeconds[i] = static_cast<float> (delaySamples / sampleRate);
        }

        setPreDelayMs (35.0f);
        setDecaySeconds (3.4f);
        setDampingHz (6500.0f);
        reset();
    }

    void reset() noexcept
    {
        inputHistory.reset();

        for (auto& diffuser : diffusers)
            diffuser.reset();

        for (auto& line : fdnLines)
            line.reset();

        dampingState.fill (0.0f);
    }

    void setPreDelayMs (float milliseconds) noexcept
    {
        preDelayMs = std::clamp (milliseconds, 0.0f, 250.0f);
        preDelaySamples = preDelayMs <= 0.0f ? 0 : millisecondsToSamples (preDelayMs);
    }

    // Approximate broadband RT60. Damping makes the high-frequency RT60 shorter
    void setDecaySeconds (float seconds) noexcept
    {
        decaySeconds = std::clamp (seconds, 0.4f, 12.0f);

        for (std::size_t i = 0; i < numLines; ++i)
        {
            // Gain required for a -60 dB decay after decaySeconds
            feedbackGain[i] = std::pow (10.0f,
                                        -3.0f * fdnDelaySeconds[i] / decaySeconds);
        }
    }

    void setDampingHz (float cutoffHz) noexcept
    {
        dampingHz = std::clamp (cutoffHz,
                                400.0f,
                                static_cast<float> (0.45 * sampleRate));

        dampingCoefficient = 1.0f
            - std::exp (-2.0f * pi * dampingHz / static_cast<float> (sampleRate));
    }

    void processBlock (const float* monoInput,
                       float* earlyOutput,
                       float* lateOutput,
                       int numSamples) noexcept
    {
        if (monoInput == nullptr || earlyOutput == nullptr || lateOutput == nullptr)
            return;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float early = 0.0f;
            float late = 0.0f;
            processSample (monoInput[sample], early, late);
            earlyOutput[sample] = early;
            lateOutput[sample] = late;
        }
    }

    void processSample (float input, float& earlyOutput, float& lateOutput) noexcept
    {
        inputHistory.push (input);

        const float predelayed = inputHistory.read (preDelaySamples);

        // Sparse, irregular early reflections. Alternating signs help avoid a
        // single obvious slapback while preserving transient definition.
        constexpr std::array<float, numEarlyTaps> earlyTimesMs
        {
            7.3f, 12.7f, 19.1f, 27.7f, 39.1f, 54.1f
        };

        constexpr std::array<float, numEarlyTaps> earlyGains
        {
            0.54f, -0.40f, 0.31f, -0.24f, 0.18f, -0.13f
        };

        float early = 0.0f;
        for (std::size_t tap = 0; tap < numEarlyTaps; ++tap)
        {
            const int tapDelay = preDelaySamples + millisecondsToSamples (earlyTimesMs[tap]);
            early += inputHistory.read (tapDelay) * earlyGains[tap];
        }

        // Input diffusion before feedback
        float diffused = predelayed;
        for (auto& diffuser : diffusers)
            diffused = diffuser.process (diffused);

        std::array<float, numLines> delayed {};
        float delayedSum = 0.0f;
        float late = 0.0f;

        for (std::size_t i = 0; i < numLines; ++i)
        {
            const float lineOutput = fdnLines[i].read();

            // One-pole damping inside the feedback loop.
            dampingState[i] += dampingCoefficient * (lineOutput - dampingState[i]);
            delayed[i] = dampingState[i];
            delayedSum += delayed[i];
        }

        // Householder feedback matrix: orthogonal, energy-preserving before the per-line RT60 gains are applied
        constexpr float householderScale = 2.0f / static_cast<float> (numLines);

        for (std::size_t i = 0; i < numLines; ++i)
        {
            const float mixedFeedback = delayed[i] - householderScale * delayedSum;
            const float injected = inputSigns[i] * diffused * 0.22f;
            fdnLines[i].write (injected + mixedFeedback * feedbackGain[i]);
        }

        for (std::size_t i = 0; i < numLines; ++i)
            late += outputSigns[i] * delayed[i];

        // output scaling leaves room for later HRIR filtering and dry/wet mixing.
        earlyOutput = early * 0.42f;
        lateOutput = late * 0.18f;
    }

private:
    static constexpr float pi = 3.14159265358979323846f;
    static constexpr std::size_t numDiffusers = 4;
    static constexpr std::size_t numLines = 8;
    static constexpr std::size_t numEarlyTaps = 6;

    struct RingBuffer
    {
        void prepare (int requestedSize)
        {
            buffer.assign (static_cast<std::size_t> (std::max (2, requestedSize + 1)), 0.0f);
            writeIndex = 0;
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            writeIndex = 0;
        }

        void push (float sample) noexcept
        {
            if (buffer.empty())
                return;

            buffer[writeIndex] = sample;
            writeIndex = (writeIndex + 1) % buffer.size();
        }

        float read (int samplesAgo) const noexcept
        {
            if (buffer.empty())
                return 0.0f;

            const std::size_t clampedDelay = static_cast<std::size_t> (
                std::clamp (samplesAgo, 0, static_cast<int> (buffer.size()) - 2));

            const std::size_t newestIndex =
                (writeIndex + buffer.size() - 1) % buffer.size();

            const std::size_t readIndex =
                (newestIndex + buffer.size() - clampedDelay) % buffer.size();

            return buffer[readIndex];
        }

        std::vector<float> buffer;
        std::size_t writeIndex = 0;
    };

    struct FixedDelay
    {
        void prepare (int delaySamples)
        {
            buffer.assign (static_cast<std::size_t> (std::max (1, delaySamples)), 0.0f);
            index = 0;
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            index = 0;
        }

        float read() const noexcept
        {
            return buffer.empty() ? 0.0f : buffer[index];
        }

        void write (float value) noexcept
        {
            if (buffer.empty())
                return;

            buffer[index] = value;
            index = (index + 1) % buffer.size();
        }

        std::vector<float> buffer;
        std::size_t index = 0;
    };

    struct AllpassDiffuser
    {
        void prepare (int delaySamples)
        {
            delay.prepare (delaySamples);
        }

        void reset() noexcept
        {
            delay.reset();
        }

        float process (float input) noexcept
        {
            const float delayed = delay.read();
            const float output = delayed - gain * input;
            delay.write (input + gain * output);
            return output;
        }

        FixedDelay delay;
        float gain = 0.68f;
    };

    int millisecondsToSamples (float milliseconds) const noexcept
    {
        return std::max (1, static_cast<int> (
            std::lround (milliseconds * 0.001 * sampleRate)));
    }

    double sampleRate = 44100.0;

    float preDelayMs = 35.0f;
    float decaySeconds = 3.4f;
    float dampingHz = 6500.0f;
    int preDelaySamples = 1;
    float dampingCoefficient = 0.0f;

    RingBuffer inputHistory;
    std::array<AllpassDiffuser, numDiffusers> diffusers;
    std::array<FixedDelay, numLines> fdnLines;
    std::array<float, numLines> fdnDelaySeconds {};
    std::array<float, numLines> feedbackGain {};
    std::array<float, numLines> dampingState {};

    inline static constexpr std::array<float, numLines> inputSigns
    {
        1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f
    };

    inline static constexpr std::array<float, numLines> outputSigns
    {
        1.0f, 1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f
    };
};
