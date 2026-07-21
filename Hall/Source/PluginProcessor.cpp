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
    //juce::ignoreUnused (samplesPerBlock);

    currentSampleRate = sampleRate;
    
    /* old prototype implementation
    const int delayBufferSize = static_cast<int> (2.0 * sampleRate);
    delayBuffer.setSize (1, delayBufferSize);
    delayBuffer.clear();

    delayWritePosition = 0;
    wetFilterState = 0.0f;
    */

    hallReverb.prepare (sampleRate);

    monoReverbInput.setSize (1, samplesPerBlock);
    earlyReverbOutput.setSize (1, samplesPerBlock);
    lateReverbOutput.setSize (1, samplesPerBlock);

    monoReverbInput.clear();
    earlyReverbOutput.clear();
    lateReverbOutput.clear();

    hallReverb.setPreDelayMs (35.0f);
    hallReverb.setDecaySeconds (3.4f);
    hallReverb.setDampingHz (6500.0f);
}

void HallAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    hallReverb.reset();
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

void HallAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numChannels == 0)
        return;

    // Clear any output channels beyond stereo.
    for (int channel = 2; channel < numChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    auto* leftChannel = buffer.getWritePointer (0);

    auto* rightChannel = numChannels > 1
        ? buffer.getWritePointer (1)
        : nullptr;

    // The working buffers should have been allocated in prepareToPlay().
    jassert (monoReverbInput.getNumSamples() >= numSamples);
    jassert (earlyReverbOutput.getNumSamples() >= numSamples);
    jassert (lateReverbOutput.getNumSamples() >= numSamples);

    if (monoReverbInput.getNumSamples() < numSamples
        || earlyReverbOutput.getNumSamples() < numSamples
        || lateReverbOutput.getNumSamples() < numSamples)
    {
        return;
    }

    auto* monoInput = monoReverbInput.getWritePointer (0);
    auto* earlyMono = earlyReverbOutput.getWritePointer (0);
    auto* lateMono  = lateReverbOutput.getWritePointer (0);

    //==========================================================================
    // Read plugin parameters

    const float directionDegrees =
        apvts.getRawParameterValue ("direction")->load();

    const float preDelayMs =
        apvts.getRawParameterValue ("delayMs")->load();

    const float feedback =
        apvts.getRawParameterValue ("feedback")->load();

    const float mix =
        apvts.getRawParameterValue ("mix")->load();

    const float tone =
        apvts.getRawParameterValue ("tone")->load();

    const float width =
        apvts.getRawParameterValue ("width")->load();

    // Convert Feedback into an approximate hall decay time.
    const float decaySeconds = juce::jmap (
        feedback,
        0.0f,
        0.95f,
        0.8f,
        8.0f);

    // Higher Tone values create a brighter reverb tail.
    const float dampingHz = juce::jmap (
        tone,
        0.0f,
        1.0f,
        1800.0f,
        12000.0f);

    hallReverb.setPreDelayMs (preDelayMs);
    hallReverb.setDecaySeconds (decaySeconds);
    hallReverb.setDampingHz (dampingHz);

    //==========================================================================
    // Create the mono signal sent into the reverb engine

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inputLeft = leftChannel[sample];

        const float inputRight = rightChannel != nullptr
            ? rightChannel[sample]
            : inputLeft;

        monoInput[sample] =
            0.5f * (inputLeft + inputRight);
    }

    // Generate separate early-reflection and late-tail signals.
    hallReverb.processBlock (
        monoInput,
        earlyMono,
        lateMono,
        numSamples);

    //==========================================================================
    // Temporary stereo direction processing
    //
    // The late tail follows Direction.
    //
    // Width controls how far the early reflections move from the center
    // toward the position opposite the tail.
    //
    // This is still gain-based stereo panning. HRIR convolution will later
    // replace this section for true front/back positioning.

    const float directionRadians =
        juce::degreesToRadians (directionDegrees);

    // 90 degrees = right.
    // 270 degrees = left.
    const float tailPan =
        std::sin (directionRadians);

    // Constant-power tail panning.
    const float tailLeftGain =
        std::sqrt (0.5f * (1.0f - tailPan));

    const float tailRightGain =
        std::sqrt (0.5f * (1.0f + tailPan));

    //==========================================================================
    // Width response

    const float widthAmount =
        juce::jlimit (0.0f, 1.0f, width);

    // Stretch the useful range so low and middle Width values move gradually.
    //
    // Width 0.20 -> about 0.018
    // Width 0.50 -> about 0.177
    // Width 0.80 -> about 0.572
    // Width 1.00 -> 1.000
    const float shapedWidth =
        std::pow (widthAmount, 2.5f); //this float controls response curve of width knob, higher = more fine tune

    // The origin moves in the opposite direction from the tail.
    //
    // Width = 0:
    //     earlyPan = 0, so the early reflections are centered.
    //
    // Width = 1:
    //     earlyPan = -tailPan, so they are fully opposite.
    const float earlyPan =
        -tailPan * shapedWidth;

    // Constant-power early-reflection panning.
    const float earlyLeftGain =
        std::sqrt (0.5f * (1.0f - earlyPan));

    const float earlyRightGain =
        std::sqrt (0.5f * (1.0f + earlyPan));

    // Starting balance between the early reflections and dense late tail.
    constexpr float earlyLevel = 0.65f;
    constexpr float lateLevel  = 1.0f;

    const float dryGain = 1.0f - mix;
    const float wetGain = mix;

    //==========================================================================
    // Mix the dry signal with the hall reverb

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float dryLeft = leftChannel[sample];

        const float dryRight = rightChannel != nullptr
            ? rightChannel[sample]
            : dryLeft;

        const float wetLeft =
            earlyMono[sample] * earlyLeftGain * earlyLevel
            + lateMono[sample] * tailLeftGain * lateLevel;

        const float wetRight =
            earlyMono[sample] * earlyRightGain * earlyLevel
            + lateMono[sample] * tailRightGain * lateLevel;

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
                0.5f * (wetLeft + wetRight);

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
