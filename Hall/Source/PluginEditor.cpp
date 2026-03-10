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
    
     //Sliders
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
    
    widthKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    widthKnob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible (widthKnob);

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
    
    widthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "width", widthKnob);
    
    //labels
    directionLabel.setText ("Direction", juce::dontSendNotification);
    directionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (directionLabel);

    delayLabel.setText ("Delay", juce::dontSendNotification);
    delayLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (delayLabel);

    feedbackLabel.setText ("Decay", juce::dontSendNotification);
    feedbackLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (feedbackLabel);

    mixLabel.setText ("Mix", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (mixLabel);

    toneLabel.setText ("Tone", juce::dontSendNotification);
    toneLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (toneLabel);
    
    widthLabel.setText ("Width", juce::dontSendNotification);
    widthLabel.setJustificationType (juce::Justification::centred);
    widthLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (widthLabel);
    
    directionLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    delayLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    feedbackLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    mixLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    toneLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    widthLabel.setColour (juce::Label::textColourId, juce::Colours::white);
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
    directionLabel.setBounds (topArea.removeFromTop (25));
    directionWheel.setBounds (topArea.reduced (40));

    auto bottomArea = area.reduced (10);
    int sectionWidth = bottomArea.getWidth() / 5;

    auto delayArea = bottomArea.removeFromLeft (sectionWidth).reduced (10);
    delayLabel.setBounds (delayArea.removeFromTop (20));
    delayKnob.setBounds (delayArea);

    auto feedbackArea = bottomArea.removeFromLeft (sectionWidth).reduced (10);
    feedbackLabel.setBounds (feedbackArea.removeFromTop (20));
    feedbackKnob.setBounds (feedbackArea);

    auto mixArea = bottomArea.removeFromLeft (sectionWidth).reduced (10);
    mixLabel.setBounds (mixArea.removeFromTop (20));
    mixKnob.setBounds (mixArea);

    auto toneArea = bottomArea.removeFromLeft (sectionWidth).reduced (10);
    toneLabel.setBounds (toneArea.removeFromTop (20));
    toneKnob.setBounds (toneArea);

    auto widthArea = bottomArea.reduced (10);
    widthLabel.setBounds (widthArea.removeFromTop (20));
    widthKnob.setBounds (widthArea);
}