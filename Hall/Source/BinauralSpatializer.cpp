#include "BinauralSpatializer.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float minimumAngleChangeDegrees = 0.0001f;
    constexpr float angleSmoothingTimeSeconds = 0.025f;
}

void BinauralSpatializer::prepare (
    double sampleRate,
    int maximumBlockSize)
{
    juce::ignoreUnused (maximumBlockSize);

    currentSampleRate =
        std::max (1.0, sampleRate); //matches host sample rate

    // Resampling, interpolation, and allocation
    // never inside the audio callback.
    buildHostRateHrirTable (currentSampleRate);

    inputHistory.assign (
        static_cast<std::size_t> (hrirLength),
        0.0f);

    historyWritePosition = 0;

    angleSmoothingLengthSamples = std::max (
        1,
        static_cast<int> (
            std::round (
                currentSampleRate
                * angleSmoothingTimeSeconds)));

    currentUnwrappedAngle =
        targetUnwrappedAngle;

    angleSmoothingSamplesRemaining = 0;
    prepared = true;
}

void BinauralSpatializer::reset() noexcept
{
    std::fill (
        inputHistory.begin(),
        inputHistory.end(),
        0.0f);

    historyWritePosition = 0;

    currentUnwrappedAngle =
        targetUnwrappedAngle;

    angleSmoothingSamplesRemaining = 0;
}

void BinauralSpatializer::setAngleDegrees (
    float newAngleDegrees) noexcept
{
    const float wrappedNewAngle =
        HorizontalHrirDatabase::wrap360 (
            newAngleDegrees);

    const float wrappedCurrentTarget =
        HorizontalHrirDatabase::wrap360 (
            targetUnwrappedAngle);

    const float change =
        shortestAngleDifference (
            wrappedCurrentTarget,
            wrappedNewAngle);

    if (std::abs (change)
        < minimumAngleChangeDegrees)
    {
        return;
    }

    targetUnwrappedAngle += change;

    angleSmoothingSamplesRemaining =
        angleSmoothingLengthSamples;
}

void BinauralSpatializer::processMonoToStereo (
    const float* monoInput,
    float* leftOutput,
    float* rightOutput,
    int numSamples) noexcept
{
    if (! prepared
        || monoInput == nullptr
        || leftOutput == nullptr
        || rightOutput == nullptr
        || numSamples <= 0
        || hrirLength <= 0)
    {
        if (leftOutput != nullptr
            && rightOutput != nullptr
            && numSamples > 0)
        {
            std::fill_n (
                leftOutput,
                numSamples,
                0.0f);

            std::fill_n (
                rightOutput,
                numSamples,
                0.0f);
        }

        return;
    }

    for (int sample = 0;
         sample < numSamples;
         ++sample)
    {
        inputHistory[
            static_cast<std::size_t> (
                historyWritePosition)]
            = monoInput[sample];

        const float smoothedAngle =
            getNextSmoothedAngle();

        // Rounds to nearest precomputed whole-degree HRIR
        const int angleIndex =
            static_cast<int> (
                std::lround (smoothedAngle))
            % renderedAngleCount;

        const float* leftIr =
            getRenderedLeftIr (angleIndex);

        const float* rightIr =
            getRenderedRightIr (angleIndex);

        float outputLeft = 0.0f;
        float outputRight = 0.0f;

        int historyPosition =
            historyWritePosition;

        //FIR convolution
        for (int tap = 0;
             tap < hrirLength;
             ++tap)
        {
            const float inputSample =
                inputHistory[
                    static_cast<std::size_t> (
                        historyPosition)];

            outputLeft +=
                inputSample * leftIr[tap];

            outputRight +=
                inputSample * rightIr[tap];

            if (--historyPosition < 0)
                historyPosition = hrirLength - 1;
        }

        leftOutput[sample] = outputLeft;
        rightOutput[sample] = outputRight;

        if (++historyWritePosition >= hrirLength)
            historyWritePosition = 0;
    }
}

float BinauralSpatializer::shortestAngleDifference (
    float fromDegrees,
    float toDegrees) noexcept
{
    float difference = std::fmod (
        toDegrees - fromDegrees + 180.0f,
        360.0f);

    if (difference < 0.0f)
        difference += 360.0f;

    return difference - 180.0f;
}

void BinauralSpatializer::buildHostRateHrirTable (
    double destinationSampleRate)
{
    const double sampleRateRatio =
        destinationSampleRate
        / static_cast<double> (
            HrirData::kSourceSampleRate);

    // Maintains HRIR duration when at a sample rate other than 44.1 kHz.
    hrirLength = std::max (
        1,
        static_cast<int> (
            std::ceil (
                static_cast<double> (
                    HrirData::kHrirLength - 1)
                * sampleRateRatio))
            + 1);

    const auto hostRateTableSize =
        static_cast<std::size_t> (
            HrirData::kNumAzimuths
            * hrirLength);

    std::vector<float> hostRateLeft (
        hostRateTableSize,
        0.0f);

    std::vector<float> hostRateRight (
        hostRateTableSize,
        0.0f);

    //======================================================================
    // Resample every original HRIR to the host sample rate.

    for (int azimuth = 0;
         azimuth < HrirData::kNumAzimuths;
         ++azimuth)
    {
        for (int destinationIndex = 0;
             destinationIndex < hrirLength;
             ++destinationIndex)
        {
            const double sourcePosition =
                static_cast<double> (
                    destinationIndex)
                / sampleRateRatio;

            const double sourceFloor =
                std::floor (sourcePosition);

            const int sourceIndex0 =
                std::clamp (
                    static_cast<int> (
                        sourceFloor),
                    0,
                    HrirData::kHrirLength - 1);

            const int sourceIndex1 =
                std::min (
                    sourceIndex0 + 1,
                    HrirData::kHrirLength - 1);

            const float fraction =
                static_cast<float> (
                    sourcePosition
                    - sourceFloor);

            const float inverseFraction =
                1.0f - fraction;

            const auto destinationOffset =
                static_cast<std::size_t> (
                    azimuth * hrirLength
                    + destinationIndex);

            hostRateLeft[destinationOffset] =
                HrirData::kLeft
                    [azimuth]
                    [sourceIndex0]
                    * inverseFraction
                + HrirData::kLeft
                    [azimuth]
                    [sourceIndex1]
                    * fraction;

            hostRateRight[destinationOffset] =
                HrirData::kRight
                    [azimuth]
                    [sourceIndex0]
                    * inverseFraction
                + HrirData::kRight
                    [azimuth]
                    [sourceIndex1]
                    * fraction;
        }
    }

    //======================================================================
    // Build one interpolated filter pair for each UI degree.

    const auto renderedTableSize =
        static_cast<std::size_t> (
            renderedAngleCount
            * hrirLength);

    renderedLeftHrir.assign (
        renderedTableSize,
        0.0f);

    renderedRightHrir.assign (
        renderedTableSize,
        0.0f);

    constexpr float inverseAzimuthStep =
        1.0f
        / static_cast<float> (
            HrirData::kAzimuthStepDeg);

    for (int uiAngle = 0;
         uiAngle < renderedAngleCount;
         ++uiAngle)
    {
        // The SOFA data and plugin UI turn in opposite directions.
        //
        // Plugin:
        //   90 degrees = right
        //
        // SOFA table:
        //   positive azimuth moves toward the listener's left
        const float sofaAngle =
            HorizontalHrirDatabase::wrap360 (
                360.0f
                - static_cast<float> (uiAngle));

        const float tablePosition =
            sofaAngle * inverseAzimuthStep;

        const float tableFloor =
            std::floor (tablePosition); //round down to nearest table int

        const int lowerIndex =
            static_cast<int> (tableFloor)
            % HrirData::kNumAzimuths;

        const int upperIndex =
            (lowerIndex + 1)
            % HrirData::kNumAzimuths;

        const float interpolation =
            tablePosition - tableFloor;

        const float inverseInterpolation =
            1.0f - interpolation;

        const float* lowerLeft =
            hostRateLeft.data()
            + static_cast<std::size_t> (
                lowerIndex * hrirLength);

        const float* upperLeft =
            hostRateLeft.data()
            + static_cast<std::size_t> (
                upperIndex * hrirLength);

        const float* lowerRight =
            hostRateRight.data()
            + static_cast<std::size_t> (
                lowerIndex * hrirLength);

        const float* upperRight =
            hostRateRight.data()
            + static_cast<std::size_t> (
                upperIndex * hrirLength);

        float leftEnergy = 0.0f;
        float rightEnergy = 0.0f;

        for (int tap = 0;
             tap < hrirLength;
             ++tap)
        {
            const float leftCoefficient =
                lowerLeft[tap]
                    * inverseInterpolation
                + upperLeft[tap]
                    * interpolation;

            const float rightCoefficient =
                lowerRight[tap]
                    * inverseInterpolation
                + upperRight[tap]
                    * interpolation;

            const auto destinationOffset =
                static_cast<std::size_t> (
                    uiAngle * hrirLength
                    + tap);

            renderedLeftHrir[
                destinationOffset]
                = leftCoefficient;

            renderedRightHrir[
                destinationOffset]
                = rightCoefficient;

            leftEnergy +=        //sum of left HRIR coefficients squared
                leftCoefficient
                * leftCoefficient;

            rightEnergy +=
                rightCoefficient
                * rightCoefficient;
        }

        // applies gain to maintain equal loudness around the head
        const float combinedEnergy =
            leftEnergy + rightEnergy;

        const float directionGain =  
            combinedEnergy > 0.0f
            ? 1.0f
                / std::sqrt (combinedEnergy)
            : 1.0f;

        for (int tap = 0;
             tap < hrirLength;
             ++tap)
        {
            const auto destinationOffset =
                static_cast<std::size_t> (
                    uiAngle * hrirLength
                    + tap);

            renderedLeftHrir[
                destinationOffset]
                *= directionGain;

            renderedRightHrir[
                destinationOffset]
                *= directionGain;
        }
    }
}

float BinauralSpatializer::getNextSmoothedAngle() noexcept
{
    if (angleSmoothingSamplesRemaining > 0)
    {
        currentUnwrappedAngle +=
            (targetUnwrappedAngle
             - currentUnwrappedAngle)
            / static_cast<float> (
                angleSmoothingSamplesRemaining);

        --angleSmoothingSamplesRemaining;
    }
    else
    {
        currentUnwrappedAngle =
            targetUnwrappedAngle;
    }

    return HorizontalHrirDatabase::wrap360 (
        currentUnwrappedAngle);
}

const float* BinauralSpatializer::getRenderedLeftIr (
    int angleIndex) const noexcept
{
    return renderedLeftHrir.data()
        + static_cast<std::size_t> (
            angleIndex * hrirLength);
}

const float* BinauralSpatializer::getRenderedRightIr (
    int angleIndex) const noexcept
{
    return renderedRightHrir.data()
        + static_cast<std::size_t> (
            angleIndex * hrirLength);
}