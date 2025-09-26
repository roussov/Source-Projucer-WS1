//============================== PluginProcessor.cpp ===========================
#include "PluginProcessor.h"
#include "PluginEditor.h"

using juce::AudioBuffer;
using juce::MidiBuffer;
using juce::ScopedNoDenormals;

//==============================================================================
// Construction / Destruction
//==============================================================================

PluginAudioProcessor::PluginAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
   #if ! JucePlugin_IsMidiEffect
    #if ! JucePlugin_IsSynth
        // Un SEUL bus d'entrée par défaut (stéréo)
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
    #endif
        // Un SEUL bus de sortie par défaut (stéréo)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
   #endif
      ),
      apvts (*this, nullptr, "Params", createParameterLayout())
{
}

PluginAudioProcessor::~PluginAudioProcessor() = default;

//==============================================================================
// APVTS Layout
//==============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
PluginAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

   #if JUCE_MAJOR_VERSION >= 8
    auto range = NormalisableRange<float> (0.0f, 1.0f, 0.0001f);
    auto fAttr = AudioParameterFloatAttributes{}
                   .withStringFromValueFunction ([] (float v, int) { return String (v, 2); });

    params.push_back (std::make_unique<AudioParameterFloat>(
        ParameterID { "gain",   1 }, "Gain",   range, 0.5f, fAttr));
    params.push_back (std::make_unique<AudioParameterBool >(
        ParameterID { "bypass", 1 }, "Bypass", false));
   #else
    params.push_back (std::make_unique<AudioParameterFloat>(
        "gain", "Gain", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));
    params.push_back (std::make_unique<AudioParameterBool>(
        "bypass", "Bypass", false));
   #endif

    return { params.begin(), params.end() };
}

//==============================================================================
// AudioProcessor overrides
//==============================================================================

const juce::String PluginAudioProcessor::getName() const { return JucePlugin_Name; }

bool PluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}
bool PluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}
bool PluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}
double PluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

#ifndef JucePlugin_PreferredChannelConfigurations
bool PluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    auto inSet  = layouts.getMainInputChannelSet();
    auto outSet = layouts.getMainOutputChannelSet();

    // Autorise UNIQUEMENT mono ou stéréo
    if (! (outSet == juce::AudioChannelSet::mono()
        || outSet == juce::AudioChannelSet::stereo()))
        return false;

   #if ! JucePlugin_IsSynth
    // Entrée = sortie
    if (inSet != outSet)
        return false;
   #endif

    return true;
   #endif
}
#endif

//==============================================================================
// Préparation et traitement
//==============================================================================

void PluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    lastSampleRate = (float) sampleRate;

    gainSmoothed.reset (sampleRate, 0.005);
    wetSmoothed .reset (sampleRate, 0.002);

    const float g = apvts.getRawParameterValue ("gain")->load();
    const bool  b = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    gainSmoothed.setCurrentAndTargetValue (g);
    wetSmoothed .setCurrentAndTargetValue (b ? 0.0f : 1.0f);

    meterAttack  = std::exp (-1.0 / (0.005 * sampleRate));
    meterRelease = std::exp (-1.0 / (0.200 * sampleRate));
    meterIn  = 0.0f;
    meterOut = 0.0f;

    inLevel.store  (0.0f, std::memory_order_relaxed);
    outLevel.store (0.0f, std::memory_order_relaxed);

    dryBuffer.setSize (juce::jmax (1, getTotalNumInputChannels()), samplesPerBlock);
}

void PluginAudioProcessor::releaseResources() {}

void PluginAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int numSamples  = buffer.getNumSamples();
    const int numInCh     = getTotalNumInputChannels();
    const int numOutCh    = getTotalNumOutputChannels();

    // Nettoie sorties orphelines
    for (int ch = numInCh; ch < numOutCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    const float targetGain = apvts.getRawParameterValue ("gain")->load();
    const bool  bypassNow  = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    gainSmoothed.setTargetValue (targetGain);
    wetSmoothed .setTargetValue (bypassNow ? 0.0f : 1.0f);

    if (! midi.isEmpty())
        midiInFlash.store (0.35f, std::memory_order_relaxed);

    if (dryBuffer.getNumSamples() < numSamples || dryBuffer.getNumChannels() < numInCh)
        dryBuffer.setSize (juce::jmax (1, numInCh), numSamples, false, false, true);
    for (int ch = 0; ch < numInCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    float inPk = 0.0f;
    for (int ch = 0; ch < numInCh; ++ch)
        inPk = juce::jmax (inPk, buffer.getMagnitude (ch, 0, numSamples));
    {
        const float a = (inPk > meterIn ? (float) meterAttack : (float) meterRelease);
        meterIn = a * meterIn + (1.0f - a) * inPk;
        inLevel.store (juce::jlimit (0.0f, 1.0f, meterIn), std::memory_order_relaxed);
    }

    if (numInCh > 0)
    {
        auto* Ldry = dryBuffer.getReadPointer (0);
        auto* L    = buffer.getWritePointer (0);
        auto* Rdry = (numInCh > 1 ? dryBuffer.getReadPointer (1) : nullptr);
        auto* R    = (numInCh > 1 ? buffer.getWritePointer  (1) : nullptr);

        for (int i = 0; i < numSamples; ++i)
        {
            const float g = gainSmoothed.getNextValue();
            const float w = wetSmoothed .getNextValue();

            const float wetL = Ldry[i] * g;
            L[i] = Ldry[i] * (1.0f - w) + wetL * w;

            if (R)
            {
                const float wetR = Rdry[i] * g;
                R[i] = Rdry[i] * (1.0f - w) + wetR * w;
            }
        }
    }

    float outPk = 0.0f;
    for (int ch = 0; ch < numOutCh; ++ch)
        outPk = juce::jmax (outPk, buffer.getMagnitude (ch, 0, numSamples));
    {
        const float a = (outPk > meterOut ? (float) meterAttack : (float) meterRelease);
        meterOut = a * meterOut + (1.0f - a) * outPk;
        outLevel.store (juce::jlimit (0.0f, 1.0f, meterOut), std::memory_order_relaxed);
    }

   #if JucePlugin_ProducesMidiOutput
    if (midi.getNumEvents() > 0)
        midiOutFlash.store (0.35f, std::memory_order_relaxed);
   #else
    juce::ignoreUnused (midiOutFlash);
   #endif
}

//==============================================================================
// Editor
//==============================================================================

juce::AudioProcessorEditor* PluginAudioProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
// State save/load
//==============================================================================

void PluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 1, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        const juce::ValueTree vt = juce::ValueTree::fromXml (*xml);
        if (vt.isValid() && vt.hasType (apvts.state.getType()))
        {
            apvts.replaceState (vt);

            const float g = apvts.getRawParameterValue ("gain")->load();
            const bool  b = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
            gainSmoothed.setCurrentAndTargetValue (g);
            wetSmoothed .setCurrentAndTargetValue (b ? 0.0f : 1.0f);
        }
    }
}

//==============================================================================
// Factory (unique définition)
//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginAudioProcessor();
}
