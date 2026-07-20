#pragma once

namespace HrirData
{
    inline constexpr int kAzimuthStepDeg = 5;
    inline constexpr int kNumAzimuths = 72;
    inline constexpr int kHrirLength = 128;
    inline constexpr float kSourceSampleRate = 44100.0f;
    inline constexpr float kGlobalScale = 1.00000000f;

    extern const int kAzimuthsDeg[kNumAzimuths];
    extern const float kLeft[kNumAzimuths][kHrirLength];
    extern const float kRight[kNumAzimuths][kHrirLength];
}
