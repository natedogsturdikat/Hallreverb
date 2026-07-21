#pragma once

#include <JuceHeader.h>
#include "HorizontalHrirDatabase.h"

#include <vector>

// Horizontal headphone renderer using the embedded HRIR table.
//
// Public angle convention:
//   0 degrees   = front
//   90 degrees  = right
//   180 degrees = rear
//   270 degrees = left
class BinauralSpatializer
{
public:
    BinauralSpatializer() = default;
    ~BinauralSpatializer() = default;

    void prepare (double sampleRate, int maximumBlockSize);
    void reset() noexcept;

    void setAngleDegrees (float newAngleDegrees) noexcept;

    void processMonoToStereo (const float* monoInput,
                              float* leftOutput,
                              float* rightOutput,
                              int numSamples) noexcept;

private:
    static constexpr int renderedAngleCount = 360;

    static float shortestAngleDifference (
        float fromDegrees,
        float toDegrees) noexcept;

    void buildHostRateHrirTable (double destinationSampleRate);

    float getNextSmoothedAngle() noexcept;

    const float* getRenderedLeftIr (
        int angleIndex) const noexcept;

    const float* getRenderedRightIr (
        int angleIndex) const noexcept;

    // Contains one host-rate filter pair for each whole degree.
    std::vector<float> renderedLeftHrir;
    std::vector<float> renderedRightHrir;

    // Circular input buffer used by the FIR convolution.
    std::vector<float> inputHistory;

    int hrirLength = 0;
    int historyWritePosition = 0;

    double currentSampleRate =
        HrirData::kSourceSampleRate;

    float currentUnwrappedAngle = 0.0f;
    float targetUnwrappedAngle = 0.0f;

    int angleSmoothingLengthSamples = 1;
    int angleSmoothingSamplesRemaining = 0;

    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (
        BinauralSpatializer)
};