#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

class PluginAudioProcessor final : public juce::AudioProcessor
{
public:
    PluginAudioProcessor();
    ~PluginAudioProcessor() override; // pas de "= default" ici

    // Identité
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // Programmes (implémentés pour éviter "abstract")
    int getNumPrograms() override               { return 1; }
    int getCurrentProgram() override            { return 0; }
    void setCurrentProgram (int) override       {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // Audio
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock; // pour double précision si besoin

    // Editor
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // State
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Accès aux paramètres pour l’Editor
    juce::AudioProcessorValueTreeState&       getValueTreeState()       noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept { return apvts; }

private:
    // Paramètres
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP (SmoothedValue est dans juce directement, pas juce::dsp)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> gainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> wetSmoothed;
    float lastSampleRate { 44100.0f };

    // Ballistique meters
    float  meterIn  { 0.0f };
    float  meterOut { 0.0f };
    double meterAttack  { 0.0 };
    double meterRelease { 0.0 };

    // Niveaux thread-safe pour l’UI
    std::atomic<float> inLevel  { 0.0f };
    std::atomic<float> outLevel { 0.0f };

    // Flash MIDI
    std::atomic<float> midiInFlash  { 0.0f };
    std::atomic<float> midiOutFlash { 0.0f };

    // Buffer dry pour crossfade (bypass clickless)
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAudioProcessor)
};

// Factory
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
