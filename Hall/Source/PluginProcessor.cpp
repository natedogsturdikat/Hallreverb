/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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
void HallAudioProcessor::prepareToPlay (
    double sampleRate,
    int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    hallReverb.prepare (sampleRate);

    monoReverbInput.setSize (
        1,
        samplesPerBlock);

    earlyReverbOutput.setSize (
        1,
        samplesPerBlock);

    lateReverbOutput.setSize (
        1,
        samplesPerBlock);

    monoReverbInput.clear();
    earlyReverbOutput.clear();
    lateReverbOutput.clear();

    hallReverb.setPreDelayMs (35.0f);
    hallReverb.setDecaySeconds (3.4f);
    hallReverb.setDampingHz (6500.0f);

    // Build a separate FIR state for the early and late signals.
    earlySpatializer.prepare (
        sampleRate,
        samplesPerBlock);

    lateSpatializer.prepare (
        sampleRate,
        samplesPerBlock);

    const float initialDirection =
        apvts.getRawParameterValue (
            "direction")->load();

    lateSpatializer.setAngleDegrees (
        initialDirection);

    earlySpatializer.setAngleDegrees (
        initialDirection + 180.0f);

    // Move immediately to the restored parameter value rather than
    // sweeping there from zero when playback begins.
    earlySpatializer.reset();
    lateSpatializer.reset();

    earlySpatialOutput.setSize (
        2,
        samplesPerBlock);

    lateSpatialOutput.setSize (
        2,
        samplesPerBlock);

    earlySpatialOutput.clear();
    lateSpatialOutput.clear();
}

void HallAudioProcessor::releaseResources()
{
    hallReverb.reset();
    earlySpatializer.reset();
    lateSpatializer.reset();
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

void HallAudioProcessor::processBlock (
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples =
        buffer.getNumSamples();

    const int numChannels =
        buffer.getNumChannels();

    if (numChannels == 0)
        return;

    for (int channel = 2;
         channel < numChannels;
         ++channel)
    {
        buffer.clear (
            channel,
            0,
            numSamples);
    }

    auto* leftChannel =
        buffer.getWritePointer (0);

    auto* rightChannel =
        numChannels > 1
        ? buffer.getWritePointer (1)
        : nullptr;

    const bool workingBuffersAreLargeEnough =
        monoReverbInput.getNumSamples() >= numSamples
        && earlyReverbOutput.getNumSamples() >= numSamples
        && lateReverbOutput.getNumSamples() >= numSamples
        && earlySpatialOutput.getNumSamples() >= numSamples
        && lateSpatialOutput.getNumSamples() >= numSamples;

    jassert (workingBuffersAreLargeEnough);

    if (! workingBuffersAreLargeEnough)
        return;

    auto* monoInput =
        monoReverbInput.getWritePointer (0);

    auto* earlyMono =
        earlyReverbOutput.getWritePointer (0);

    auto* lateMono =
        lateReverbOutput.getWritePointer (0);

    auto* earlySpatialLeft =
        earlySpatialOutput.getWritePointer (0);

    auto* earlySpatialRight =
        earlySpatialOutput.getWritePointer (1);

    auto* lateSpatialLeft =
        lateSpatialOutput.getWritePointer (0);

    auto* lateSpatialRight =
        lateSpatialOutput.getWritePointer (1);

    //======================================================================
    // Read parameters

    const float directionDegrees =
        apvts.getRawParameterValue (
            "direction")->load();

    const float preDelayMs =
        apvts.getRawParameterValue (
            "delayMs")->load();

    const float feedback =
        apvts.getRawParameterValue (
            "feedback")->load();

    const float mix =
        apvts.getRawParameterValue (
            "mix")->load();

    const float tone =
        apvts.getRawParameterValue (
            "tone")->load();

    const float width =
        apvts.getRawParameterValue (
            "width")->load();

    const float decaySeconds =
        juce::jmap (
            feedback,
            0.0f,
            0.95f,
            0.8f,
            8.0f);

    const float dampingHz =
        juce::jmap (
            tone,
            0.0f,
            1.0f,
            1800.0f,
            12000.0f);

    hallReverb.setPreDelayMs (
        preDelayMs);

    hallReverb.setDecaySeconds (
        decaySeconds);

    hallReverb.setDampingHz (
        dampingHz);

    //======================================================================
    // Create the mono signal sent into the hall

    for (int sample = 0;
         sample < numSamples;
         ++sample)
    {
        const float inputLeft =
            leftChannel[sample];

        const float inputRight =
            rightChannel != nullptr
            ? rightChannel[sample]
            : inputLeft;

        monoInput[sample] =
            0.5f
            * (inputLeft + inputRight);
    }

    hallReverb.processBlock (
        monoInput,
        earlyMono,
        lateMono,
        numSamples);

    //======================================================================
    // Determine the two positions

    const float tailDirection =
        HorizontalHrirDatabase::wrap360 (
            directionDegrees);

    const float originDirection =
        HorizontalHrirDatabase::wrap360 (
            directionDegrees + 180.0f);

    // Tail follows Direction.
    lateSpatializer.setAngleDegrees (
        tailDirection);

    // Early-reflection origin stays opposite Direction.
    earlySpatializer.setAngleDegrees (
        originDirection);

    lateSpatializer.processMonoToStereo (
        lateMono,
        lateSpatialLeft,
        lateSpatialRight,
        numSamples);

    earlySpatializer.processMonoToStereo (
        earlyMono,
        earlySpatialLeft,
        earlySpatialRight,
        numSamples);

    //======================================================================
    // Width
    //
    // Width 0:
    //   centered early reflections
    //
    // Width 1:
    //   fully binaural early reflections opposite the tail

    const float widthAmount =
        juce::jlimit (
            0.0f,
            1.0f,
            width);

    const float shapedWidth =
        std::pow (
            widthAmount,
            2.5f);

    // These two signals are highly correlated because both originate
    // from earlyMono. A linear blend is preferable here; equal-power
    // blending could produce a noticeable volume boost in the middle.
    const float centredOriginAmount =
        1.0f - shapedWidth;

    const float binauralOriginAmount =
        shapedWidth;

    constexpr float centreGain =
        0.70710678f;

    constexpr float earlyLevel =
        0.65f;

    constexpr float lateLevel =
        1.0f;

    const float dryGain =
        1.0f - mix;

    const float wetGain =
        mix;

    //======================================================================
    // Output mix

    for (int sample = 0;
         sample < numSamples;
         ++sample)
    {
        const float dryLeft =
            leftChannel[sample];

        const float dryRight =
            rightChannel != nullptr
            ? rightChannel[sample]
            : dryLeft;

        const float centredEarly =
            earlyMono[sample]
            * centreGain;

        const float earlyLeft =
            centredEarly
                * centredOriginAmount
            + earlySpatialLeft[sample]
                * binauralOriginAmount;

        const float earlyRight =
            centredEarly
                * centredOriginAmount
            + earlySpatialRight[sample]
                * binauralOriginAmount;

        const float wetLeft =
            earlyLeft * earlyLevel
            + lateSpatialLeft[sample]
                * lateLevel;

        const float wetRight =
            earlyRight * earlyLevel
            + lateSpatialRight[sample]
                * lateLevel;

        if (rightChannel != nullptr)
        {
            leftChannel[sample] =
                dryLeft * dryGain
                + wetLeft * wetGain;

            rightChannel[sample] =
                dryRight * dryGain
                + wetRight * wetGain;
        }
        else
        {
            const float monoWet =
                0.5f
                * (wetLeft + wetRight);

            leftChannel[sample] =
                dryLeft * dryGain
                + monoWet * wetGain;
        }
    }
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
        juce::NormalisableRange<float> (0.0f, 359.9f, 1.0f), 0.0f));

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
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
    "width", "Width",
    juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
    0.5f));

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HallAudioProcessor();
}
