/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
HallAudioProcessor::HallAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
        apvts (*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

HallAudioProcessor::~HallAudioProcessor()
{
}

//==============================================================================
const juce::String HallAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool HallAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool HallAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool HallAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double HallAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int HallAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int HallAudioProcessor::getCurrentProgram()
{
    return 0;
}

void HallAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String HallAudioProcessor::getProgramName (int index)
{
    return {};
}

void HallAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void HallAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate;

    const int delayBufferSize = static_cast<int> (2.0 * sampleRate);
    delayBuffer.setSize (1, delayBufferSize);
    delayBuffer.clear();

    delayWritePosition = 0;
    wetFilterState = 0.0f;
}

void HallAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool HallAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void HallAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int channel = 2; channel < numChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    auto* leftChannel  = buffer.getWritePointer (0);
    auto* rightChannel = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto* delayData = delayBuffer.getWritePointer (0);
    const int delayBufferSize = delayBuffer.getNumSamples();

    auto* directionParam = apvts.getRawParameterValue ("direction");
    auto* delayMsParam   = apvts.getRawParameterValue ("delayMs");
    auto* feedbackParam  = apvts.getRawParameterValue ("feedback");
    auto* mixParam       = apvts.getRawParameterValue ("mix");
    auto* toneParam      = apvts.getRawParameterValue ("tone");

    const float directionDegrees = directionParam->load();
    const float delayMs          = delayMsParam->load();
    const float feedback         = feedbackParam->load();
    const float mix              = mixParam->load();
    const float tone             = toneParam->load();

    const float dry = 1.0f - mix;

    const float angleRadians = juce::degreesToRadians (directionDegrees);

    // Left/right position from angle:
    // 90° = right, 270° = left
    const float pan = std::sin (angleRadians);

    const float leftGain  = std::sqrt (0.5f * (1.0f - pan));
    const float rightGain = std::sqrt (0.5f * (1.0f + pan));

    // Front/back cue:
    // 0° = front, 180° = back
    const float frontness = std::cos (angleRadians);

    // Rear = darker
    const float brightness = juce::jmap (frontness, -1.0f, 1.0f, 0.45f, 1.0f);

    const int baseDelaySamples = static_cast<int> ((delayMs / 1000.0f) * currentSampleRate);

    // Tiny interaural time difference
    const int maxItdSamples = static_cast<int> (0.0006 * currentSampleRate);
    const int itdSamples = static_cast<int> (pan * (float) maxItdSamples);

    // Tone control for wet smoothing/darkness
    const float filterCoeff = juce::jmap (tone, 0.0f, 1.0f, 0.02f, 0.30f);

    int localWritePosition = delayWritePosition;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inL = leftChannel[sample];
        const float inR = (rightChannel != nullptr) ? rightChannel[sample] : inL;

        // Mono input to the wet path
        const float monoIn = 0.5f * (inL + inR);

        // Read delayed samples with tiny ear offset
        int leftReadPosition  = localWritePosition - baseDelaySamples + itdSamples;
        int rightReadPosition = localWritePosition - baseDelaySamples - itdSamples;

        while (leftReadPosition < 0)
            leftReadPosition += delayBufferSize;
        while (rightReadPosition < 0)
            rightReadPosition += delayBufferSize;

        leftReadPosition  %= delayBufferSize;
        rightReadPosition %= delayBufferSize;

        const float delayedLeftMono  = delayData[leftReadPosition];
        const float delayedRightMono = delayData[rightReadPosition];

        // Average the two for feedback path stability
        const float delayedMono = 0.5f * (delayedLeftMono + delayedRightMono);

        // Write input + feedback into delay line
        delayData[localWritePosition] = monoIn + (delayedMono * feedback);

        // Tone + rear darkening
        const float rawWet = delayedMono * brightness;
        wetFilterState += filterCoeff * (rawWet - wetFilterState);

        const float wetLeft  = wetFilterState * leftGain;
        const float wetRight = wetFilterState * rightGain;

        leftChannel[sample] = (inL * dry) + (wetLeft * mix);

        if (rightChannel != nullptr)
            rightChannel[sample] = (inR * dry) + (wetRight * mix);

        localWritePosition++;
        if (localWritePosition >= delayBufferSize)
            localWritePosition = 0;
    }

    delayWritePosition = localWritePosition;
}

//==============================================================================
bool HallAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* HallAudioProcessor::createEditor()
{
    return new HallAudioProcessorEditor (*this);
}

//==============================================================================
void HallAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void HallAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}
juce::AudioProcessorValueTreeState::ParameterLayout HallAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "direction", "Direction",
        juce::NormalisableRange<float> (0.0f, 360.0f, 1.0f), 0.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "delayMs", "Delay",
        juce::NormalisableRange<float> (20.0f, 250.0f, 1.0f), 120.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "feedback", "Feedback",
        juce::NormalisableRange<float> (0.0f, 0.95f, 0.01f), 0.35f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f));
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
    "tone", "Tone",
    juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
    0.6f));

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HallAudioProcessor();
}
