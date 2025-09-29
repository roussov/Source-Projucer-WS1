//============================== PluginProcessor.cpp ===========================
#include "PluginProcessor.h"
#include "PluginEditor.h"

using juce::AudioBuffer;
using juce::MidiBuffer;
using juce::ScopedNoDenormals;

static inline float clamp01 (float v) noexcept { return juce::jlimit (0.0f, 1.0f, v); }
static inline float quantize01 (float v, float step) noexcept
{
    if (step <= 0.0f) return clamp01 (v);
    const float q = std::floor (v / step + 0.5f) * step;
    return clamp01 (q);
}

PluginAudioProcessor::PluginAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
   #if ! JucePlugin_IsMidiEffect
    #if ! JucePlugin_IsSynth
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
    #endif
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
   #endif
      ),
      apvts (*this, nullptr, "Params", createParameterLayout())
{
}

PluginAudioProcessor::~PluginAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
PluginAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;
    constexpr float kStep = kParamStep;

   #if JUCE_MAJOR_VERSION >= 8
    auto range01 = NormalisableRange<float> (0.0f, 1.0f, kStep);
    auto fAttr = AudioParameterFloatAttributes{}
                   .withStringFromValueFunction ([] (float v, int) { return String (v, 2); });

    params.push_back (std::make_unique<AudioParameterFloat>(ParameterID{ "inTrim",   1 }, "In",     range01, 0.50f, fAttr));
    params.push_back (std::make_unique<AudioParameterFloat>(ParameterID{ "outVol",   1 }, "Out",    range01, 0.50f, fAttr));
    params.push_back (std::make_unique<AudioParameterBool > (ParameterID{ "bypass",  1 }, "Bypass", false));
    params.push_back (std::make_unique<AudioParameterInt  > (ParameterID{ "midiChan",1 }, "MIDI Ch", 0, 16, 0)); // 0=Omni
   #else
    params.push_back (std::make_unique<AudioParameterFloat>("inTrim",   "In",  NormalisableRange<float>(0.0f,1.0f,kStep), 0.50f));
    params.push_back (std::make_unique<AudioParameterFloat>("outVol",   "Out", NormalisableRange<float>(0.0f,1.0f,kStep), 0.50f));
    params.push_back (std::make_unique<AudioParameterBool >( "bypass",  "Bypass", false));
    params.push_back (std::make_unique<AudioParameterInt  >( "midiChan","MIDI Ch", 0, 16, 0));
   #endif

    return { params.begin(), params.end() };
}

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

    if (! (outSet == juce::AudioChannelSet::mono()
        || outSet == juce::AudioChannelSet::stereo()))
        return false;

   #if ! JucePlugin_IsSynth
    if (inSet != outSet)
        return false;
   #endif

    return true;
   #endif
}
#endif

void PluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    lastSampleRate = (float) sampleRate;

    preSmoothed .reset (sampleRate, 0.005);
    postSmoothed.reset (sampleRate, 0.005);
    wetSmoothed .reset (sampleRate, 0.002);

    constexpr float kStep = kParamStep;
    const float inV  = quantize01 (apvts.getRawParameterValue ("inTrim")->load(), kStep);
    const float outV = quantize01 (apvts.getRawParameterValue ("outVol")->load(), kStep);
    const bool  byp  = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    preSmoothed .setCurrentAndTargetValue (inV);
    postSmoothed.setCurrentAndTargetValue (outV);
    wetSmoothed .setCurrentAndTargetValue (byp ? 0.0f : 1.0f);

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

    for (int ch = numInCh; ch < numOutCh; ++ch)
        buffer.clear (ch, 0, numSamples);

    constexpr float kStep = kParamStep;
    const float inRaw   = apvts.getRawParameterValue ("inTrim")->load();
    const float outRaw  = apvts.getRawParameterValue ("outVol")->load();
    const bool  bypass  = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const int   wantCh  = (int) apvts.getRawParameterValue ("midiChan")->load(); // 0..16

    preSmoothed .setTargetValue (quantize01 (inRaw,  kStep));
    postSmoothed.setTargetValue (quantize01 (outRaw, kStep));
    wetSmoothed  .setTargetValue (bypass ? 0.0f : 1.0f);

    for (const auto meta : midi)
    {
        const auto& msg = meta.getMessage();
        if (! msg.isController()) continue;
        const int ch = msg.getChannel(); // 1..16
        if (wantCh != 0 && ch != wantCh) continue;

        const int cc  = msg.getControllerNumber();
        const int v   = msg.getControllerValue();
        const float q = quantize01 (v / 127.0f, kStep);

        if (cc == 11) // Expression -> IN
        {
            if (auto* p = apvts.getParameter ("inTrim")) { p->beginChangeGesture(); p->setValueNotifyingHost (q); p->endChangeGesture(); }
        }
        else if (cc == 7 || cc == 1) // Volume / Mod -> OUT
        {
            if (auto* p = apvts.getParameter ("outVol")) { p->beginChangeGesture(); p->setValueNotifyingHost (q); p->endChangeGesture(); }
        }
    }

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
        if (dryBuffer.getNumSamples() < numSamples || dryBuffer.getNumChannels() < numInCh)
            dryBuffer.setSize (juce::jmax (1, numInCh), numSamples, false, false, true);

        for (int ch = 0; ch < numInCh; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        auto* Ldry = dryBuffer.getReadPointer (0);
        auto* L    = buffer.getWritePointer (0);
        auto* Rdry = (numInCh > 1 ? dryBuffer.getReadPointer (1) : nullptr);
        auto* R    = (numInCh > 1 ? buffer.getWritePointer  (1) : nullptr);

        for (int i = 0; i < numSamples; ++i)
        {
            const float pre  = preSmoothed .getNextValue();
            const float post = postSmoothed.getNextValue();
            const float w    = wetSmoothed  .getNextValue();

            const float inL  = Ldry[i] * pre;
            const float wetL = inL;
            L[i] = (Ldry[i] * (1.0f - w) + wetL * w) * post;

            if (R)
            {
                const float inR  = Rdry[i] * pre;
                const float wetR = inR;
                R[i] = (Rdry[i] * (1.0f - w) + wetR * w) * post;
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
}

juce::AudioProcessorEditor* PluginAudioProcessor::createEditor()
{
    return new PluginEditor (*this);
}

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

            constexpr float kStep = kParamStep;
            const float inV  = quantize01 (apvts.getRawParameterValue ("inTrim")->load(), kStep);
            const float outV = quantize01 (apvts.getRawParameterValue ("outVol")->load(), kStep);
            const bool  byp  = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

            preSmoothed .setCurrentAndTargetValue (inV);
            postSmoothed.setCurrentAndTargetValue (outV);
            wetSmoothed .setCurrentAndTargetValue (byp ? 0.0f : 1.0f);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginAudioProcessor();
}
