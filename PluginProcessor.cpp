//============================== PluginProcessor.cpp ===============================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
PluginAudioProcessor::PluginAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    gainParam = parameters.getRawParameterValue ("gain");
}

PluginAudioProcessor::~PluginAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PluginAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "gain", "Gain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.0001f, 1.0f),
        0.5f));
    return { params.begin(), params.end() };
}

//==============================================================================
bool PluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getChannelSet (true,  0);
    const auto& mainOut = layouts.getChannelSet (false, 0);
    return mainIn == mainOut && (! mainIn.isDisabled());
}

//==============================================================================
void PluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    sr = (sampleRate > 0.0 ? sampleRate : 48000.0);
    inLevel = 0.0f;
    outLevel = 0.0f;
}

//==============================================================================
// Gabarit processeur (float/double)
template <typename Sample>
void PluginAudioProcessor::processBlockT (juce::AudioBuffer<Sample>& buffer, juce::MidiBuffer&)
{
    const int numCh  = buffer.getNumChannels();
    const int numSm  = buffer.getNumSamples();
    const float g = *gainParam;

    float accIn = 0.0f;
    float accOut = 0.0f;

    // Traitement linéaire 0..1 + moyenne absolue (échantillonnage adjacent)
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* in  = buffer.getReadPointer  (ch);
        auto* out = buffer.getWritePointer (ch);

        for (int i = 0; i < numSm; ++i)
        {
            const float x = (float) in[i];
            const float y = x * g;          // application linéaire
            out[i] = (Sample) y;

            accIn  += std::abs (x);
            accOut += std::abs (y);
        }
    }

    const float denom = (float) (numSm * juce::jmax (1, numCh));
    const float tIn  = juce::jlimit (0.0f, 1.0f, accIn  / denom);
    const float tOut = juce::jlimit (0.0f, 1.0f, accOut / denom);

    // Lissage progressif pour affichage visuel stable
    const float aUp   = 1.0f - std::exp (-8.0f  * (float) numSm / (float) sr);
    const float aDown = 1.0f - std::exp (-1.2f * (float) numSm / (float) sr);

    auto lerp = [] (float a, float b, float t) noexcept { return a + t * (b - a); };
    inLevel  = tIn  > inLevel  ? lerp (inLevel,  tIn,  aUp) : lerp (inLevel,  tIn,  aDown);
    outLevel = tOut > outLevel ? lerp (outLevel, tOut, aUp) : lerp (outLevel, tOut, aDown);
}

//==============================================================================
void PluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    processBlockT (buffer, midi);
}

void PluginAudioProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    processBlockT (buffer, midi);
}

//==============================================================================
juce::AudioProcessorEditor* PluginAudioProcessor::createEditor()
{
    return new PluginAudioProcessorEditor (*this);
}

//==============================================================================
void PluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (parameters.state.getType()))
        parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginAudioProcessor();
}
