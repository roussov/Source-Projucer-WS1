#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath> // std::exp, std::pow

namespace {
using APVTS = juce::AudioProcessorValueTreeState;

constexpr const char* kParamGain   = "gain";
constexpr const char* kParamBypass = "bypass";

static APVTS::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // Gain linéaire 0..1
    p.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { kParamGain, 1 }, "Gain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f, 1.0f),
        0.5f));

    // Bypass
    p.push_back (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { kParamBypass, 1 }, "Bypass", false));

    return { p.begin(), p.end() };
}

static float blockPeakAbs (const juce::AudioBuffer<float>& buf) noexcept
{
    const int chs = buf.getNumChannels();
    const int n   = buf.getNumSamples();
    float peak = 0.0f;
    for (int c = 0; c < chs; ++c)
        peak = juce::jmax (peak, buf.getMagnitude (c, 0, n));
    return peak;
}

inline float expCoeffForTau (double fs, double tauSeconds)
{
    return (tauSeconds > 0.0) ? std::exp (-(1.0 / (fs * tauSeconds))) : 0.0f;
}
} // namespace

//==================== PluginAudioProcessor ====================
PluginAudioProcessor::PluginAudioProcessor()
: juce::AudioProcessor (BusesProperties()
    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  valueTreeState (*this, nullptr, "PARAMS", createLayout())
{
    // getRawParameterValue() -> std::atomic<float>*
    gainParam   = valueTreeState.getRawParameterValue (kParamGain);
    bypassParam = valueTreeState.getRawParameterValue (kParamBypass);

    jassert (gainParam   != nullptr);
    jassert (bypassParam != nullptr);
}

PluginAudioProcessor::~PluginAudioProcessor() = default;

const juce::String PluginAudioProcessor::getName() const { return "Roussov"; }
bool   PluginAudioProcessor::acceptsMidi() const  { return false; }
bool   PluginAudioProcessor::producesMidi() const { return false; }
bool   PluginAudioProcessor::isMidiEffect() const { return false; }
double PluginAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int  PluginAudioProcessor::getNumPrograms() { return 1; }
int  PluginAudioProcessor::getCurrentProgram() { return 0; }
void PluginAudioProcessor::setCurrentProgram (int) {}
const juce::String PluginAudioProcessor::getProgramName (int) { return {}; }
void PluginAudioProcessor::changeProgramName (int, const juce::String&) {}

void PluginAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    fs = sampleRate;

    // Smoothed gain 10 ms
    smoothedGain.reset (fs, 0.010);
    smoothedGain.setCurrentAndTargetValue (gainParam ? gainParam->load() : 0.5f);

    // Ballistics des meters
    meterRiseCoeff = expCoeffForTau (fs, 0.010);
    meterFallCoeff = expCoeffForTau (fs, 0.300);

    meterIn  = 0.0f;
    meterOut = 0.0f;
}

void PluginAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in.isDisabled() || out.isDisabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}
#endif

void PluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals _;
    midi.clear();

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Peak avant traitement
    const float inPeak = blockPeakAbs (buffer);

    const bool isBypassed = (bypassParam && (bypassParam->load() >= 0.5f));
    const int n  = buffer.getNumSamples();
    const int ch = totalNumInputChannels;

    if (!isBypassed)
    {
        smoothedGain.setTargetValue (gainParam ? gainParam->load() : 0.5f);

        for (int s = 0; s < n; ++s)
        {
            const float g = smoothedGain.getNextValue();
            for (int c = 0; c < ch; ++c)
                buffer.getWritePointer (c)[s] *= g;
        }
    }

    // Peak après traitement
    const float outPeak = blockPeakAbs (buffer);

    // Meters (approx per-block)
    auto smoothPeak = [] (float prev, float x, float aRise, float aFall) -> float
    {
        const float a = (x > prev) ? aRise : aFall;
        return a * prev + (1.0f - a) * x;
    };

    const float aRiseBlock = std::pow (meterRiseCoeff, (float) n);
    const float aFallBlock = std::pow (meterFallCoeff, (float) n);

    meterIn.store  (smoothPeak (meterIn.load(),  juce::jlimit (0.0f, 1.0f, inPeak),  aRiseBlock, aFallBlock),
                    std::memory_order_relaxed);
    meterOut.store (smoothPeak (meterOut.load(), juce::jlimit (0.0f, 1.0f, outPeak), aRiseBlock, aFallBlock),
                    std::memory_order_relaxed);
}

bool PluginAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* PluginAudioProcessor::createEditor() { return new PluginEditor (*this); }

void PluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = valueTreeState.copyState();
    juce::MemoryOutputStream mos (destData, true);
    state.writeToStream (mos);
}

void PluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto vt = juce::ValueTree::readFromData (data, (size_t) sizeInBytes); vt.isValid())
        valueTreeState.replaceState (vt);
}

//==================== Entrée plugin requise par JUCE ====================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginAudioProcessor();
}
