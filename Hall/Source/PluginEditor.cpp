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
    setSize (400, 300);
    
    directionWheel.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    directionWheel.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    directionWheel.setRange (0.0, 360.0, 1.0);
    directionWheel.setValue (0.0);

    addAndMakeVisible (directionWheel);
}

HallAudioProcessorEditor::~HallAudioProcessorEditor()
{
}

//==============================================================================
void HallAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawFittedText ("Direction", getLocalBounds().removeFromTop(40), juce::Justification::centred, 1);
}

void HallAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto area = getLocalBounds().reduced (40);
    area.removeFromTop (40);

    directionWheel.setBounds (area);
}
