/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

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
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> directionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> toneAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HallAudioProcessorEditor)
};
