/*
  ==============================================================================

    CustomLookAndFeel.h
    Created: 9 Mar 2026 9:17:22pm
    Author:  Nated

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <cmath>

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        setColour (
            juce::Slider::rotarySliderFillColourId,
            juce::Colours::white);

        setColour (
            juce::Slider::rotarySliderOutlineColourId,
            juce::Colours::darkgrey);

        setColour (
            juce::Label::textColourId,
            juce::Colours::white);
    }

    void setWidthAmount (float newWidthAmount) noexcept
    {
        widthAmount = juce::jlimit (
            0.0f,
            1.0f,
            newWidthAmount);
    }

    void drawRotarySlider (
        juce::Graphics& g,
        int x,
        int y,
        int width,
        int height,
        float sliderPosProportional,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider& slider) override
    {
        juce::ignoreUnused (slider);

        const auto size =
            juce::jmin (
                static_cast<float> (width),
                static_cast<float> (height));

        const auto bounds =
            juce::Rectangle<float> (
                static_cast<float> (x),
                static_cast<float> (y),
                size,
                size)
                .withCentre (
                    {
                        x + width * 0.5f,
                        y + height * 0.5f
                    })
                .reduced (8.0f);

        const float radius =
            juce::jmin (
                bounds.getWidth(),
                bounds.getHeight())
            * 0.5f;

        const auto centre =
            bounds.getCentre();

        const float angle =
            rotaryStartAngle
            + sliderPosProportional
                * (rotaryEndAngle
                   - rotaryStartAngle);

        // Direction vector for the main tail arrow.
        const juce::Point<float> direction (
            std::cos (
                angle
                - juce::MathConstants<float>::halfPi),

            std::sin (
                angle
                - juce::MathConstants<float>::halfPi));

        // Draw the wheel outline first.
        g.setColour (juce::Colours::white);
        g.drawEllipse (bounds, 2.0f);

        //==================================================================
        // Early-reflection origin arrow
        //
        // It points opposite the tail arrow.
        // At Width 0 it has no length and is invisible.
        // At Width 1 it reaches its full length.

        const float shapedWidth =
            std::pow (widthAmount, 2.5f);

        if (shapedWidth > 0.001f)
        {
            const juce::Point<float> originDirection (
                -direction.x,
                -direction.y);

            const juce::Point<float> originPerpendicular (
                -originDirection.y,
                originDirection.x);

            const float originTipDistance =
                radius
                * 0.75f
                * shapedWidth;

            const float originBaseDistance =
                radius
                * 0.20f
                * shapedWidth;

            const float originHalfWidth =
                8.0f
                * shapedWidth;

            // Put the arrowhead closer to the center and its base farther out.
            // This makes the origin arrow point toward the main Direction arrow.
            const auto originTip =
                centre
                + originDirection
                    * originBaseDistance;

            const auto originBase =
                centre
                + originDirection
                    * originTipDistance;

            juce::Path originArrow;

            originArrow.startNewSubPath (
                originTip);

            originArrow.lineTo (
                originBase
                + originPerpendicular
                    * originHalfWidth);

            originArrow.lineTo (
                originBase
                - originPerpendicular
                    * originHalfWidth);

            originArrow.closeSubPath();

            // Maximum opacity remains lower than the primary arrow.
            const float originOpacity =
                0.42f
                * shapedWidth;

            g.setColour (
                juce::Colours::white.withAlpha (
                    originOpacity));

            g.fillPath (originArrow);
        }

        //==================================================================
        // Main Direction arrow

        const auto directionTip =
            centre
            + direction
                * (radius * 0.75f);

        const auto directionBase =
            centre
            + direction
                * (radius * 0.20f);

        const juce::Point<float> directionPerpendicular (
            -direction.y,
            direction.x);

        juce::Path directionArrow;

        directionArrow.startNewSubPath (
            directionTip);

        directionArrow.lineTo (
            directionBase
            + directionPerpendicular * 8.0f);

        directionArrow.lineTo (
            directionBase
            - directionPerpendicular * 8.0f);

        directionArrow.closeSubPath();

        g.setColour (juce::Colours::white);
        g.fillPath (directionArrow);

        // Center point is drawn last so it remains crisp.
        g.fillEllipse (
            centre.x - 4.0f,
            centre.y - 4.0f,
            8.0f,
            8.0f);
    }

private:
    float widthAmount = 0.0f;
};