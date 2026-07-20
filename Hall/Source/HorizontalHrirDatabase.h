#pragma once
#include "HorizontalHrirData.h"
#include <cmath>

class HorizontalHrirDatabase
{
public:
    static float wrap360(float deg)
    {
        while (deg < 0.0f)   deg += 360.0f;
        while (deg >= 360.0f) deg -= 360.0f;
        return deg;
    }

    static int getNearestIndexForAngle(float angleDeg)
    {
        const float wrapped = wrap360(angleDeg);
        const int rawIndex = static_cast<int>(std::round(wrapped / static_cast<float>(HrirData::kAzimuthStepDeg)));
        return rawIndex % HrirData::kNumAzimuths;
    }

    static const float* getLeftIR(float angleDeg)
    {
        return HrirData::kLeft[getNearestIndexForAngle(angleDeg)];
    }

    static const float* getRightIR(float angleDeg)
    {
        return HrirData::kRight[getNearestIndexForAngle(angleDeg)];
    }
};