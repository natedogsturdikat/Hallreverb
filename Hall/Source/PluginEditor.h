/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"

//==============================================================================
/**
*/
class HallAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    HallAudioProcessorEditor (HallAudioProcessor&);
    ~HallAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    HallAudioProcessor& audioProcessor;
    
    juce::Slider directionWheel;
    juce::Slider delayKnob;
    juce::Slider feedbackKnob;
    juce::Slider mixKnob;
    juce::Slider toneKnob;
    juce::Slider widthKnob;
    juce::Label directionLabel;
    juce::Label delayLabel;
    juce::Label feedbackLabel;
    juce::Label mixLabel;
    juce::Label toneLabel;
    juce::Label widthLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> directionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttachment;

    CustomLookAndFeel customLookAndFeel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HallAudioProcessorEditor)
};
