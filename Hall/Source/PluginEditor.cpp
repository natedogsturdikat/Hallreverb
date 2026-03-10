/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
HallAudioProcessorEditor::HallAudioProcessorEditor (HallAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
     setSize (500, 500);

    directionWheel.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    directionWheel.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 20);
    addAndMakeVisible (directionWheel);

    delayKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    delayKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible (delayKnob);

    feedbackKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    feedbackKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible (feedbackKnob);

    mixKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible (mixKnob);

    toneKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    toneKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible (toneKnob);

    directionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "direction", directionWheel);

    delayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "delayMs", delayKnob);

    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "feedback", feedbackKnob);

    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "mix", mixKnob);

    toneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "tone", toneKnob);
}

HallAudioProcessorEditor::~HallAudioProcessorEditor()
{
}

//==============================================================================
void HallAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour (juce::Colours::white);
    g.setFont (22.0f);
    g.drawFittedText ("Hall", getLocalBounds().removeFromTop (30),
                      juce::Justification::centred, 1);
}

void HallAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto area = getLocalBounds().reduced (20);

    auto topArea = area.removeFromTop (320);
    directionWheel.setBounds (topArea.reduced (60));

    auto bottomArea = area.reduced (10);
    int knobWidth = bottomArea.getWidth() / 4;

    delayKnob.setBounds    (bottomArea.removeFromLeft (knobWidth).reduced (10));
    feedbackKnob.setBounds (bottomArea.removeFromLeft (knobWidth).reduced (10));
    mixKnob.setBounds      (bottomArea.removeFromLeft (knobWidth).reduced (10));
    toneKnob.setBounds     (bottomArea.reduced (10));
}
