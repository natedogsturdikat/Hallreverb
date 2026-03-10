/*
  ==============================================================================

    CustomLookAndFeel.h
    Created: 9 Mar 2026 9:17:22pm
    Author:  Nated

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::white);
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkgrey);
        setColour (juce::Label::textColourId, juce::Colours::white);
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        auto size = juce::jmin ((float) width, (float) height);
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, size, size)
                        .withCentre ({ x + width * 0.5f, y + height * 0.5f })
                        .reduced (8.0f);

        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto centre = bounds.getCentre();
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        g.setColour (juce::Colours::white);
        g.drawEllipse (bounds, 2.0f);

        juce::Point<float> dir (std::cos (angle - juce::MathConstants<float>::halfPi),
                                std::sin (angle - juce::MathConstants<float>::halfPi));

        auto tip  = centre + dir * (radius * 0.75f);
        auto base = centre + dir * (radius * 0.20f);
        juce::Point<float> perp (-dir.y, dir.x);

        juce::Path arrow;
        arrow.startNewSubPath (tip);
        arrow.lineTo (base + perp * 8.0f);
        arrow.lineTo (base - perp * 8.0f);
        arrow.closeSubPath();

        g.fillPath (arrow);
        g.fillEllipse (centre.x - 4.0f, centre.y - 4.0f, 8.0f, 8.0f);
    }
};