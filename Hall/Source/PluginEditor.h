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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HallAudioProcessorEditor)
};
