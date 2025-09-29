//============================== PluginProcessor.h =============================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

class PluginAudioProcessor final : public juce::AudioProcessor
{
public:
    // Pas brut linéaire (TB) pour les paramètres 0..1
    static constexpr float kParamStep = 0.01f;

    PluginAudioProcessor();
    ~PluginAudioProcessor() override;

    // Identité
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // Programmes
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
    using juce::AudioProcessor::processBlock; // version double

    // Editor
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // State
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Accès params/meters
    juce::AudioProcessorValueTreeState&       getValueTreeState()       noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept { return apvts; }
    float getInputLevel()  const noexcept { return inLevel.load (std::memory_order_relaxed); }
    float getOutputLevel() const noexcept { return outLevel.load (std::memory_order_relaxed); }

private:
    // Paramètres (APVTS: "inTrim", "outVol", "bypass", "midiChan")
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Smoothing linéaire 0..1 (IN / OUT / WET)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> preSmoothed;   // IN
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> postSmoothed;  // OUT
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> wetSmoothed;   // bypass→wet
    float  lastSampleRate { 44100.0f };

    // Meters
    float  meterIn  { 0.0f };
    float  meterOut { 0.0f };
    double meterAttack  { 0.0 };
    double meterRelease { 0.0 };

    // UI thread-safe
    std::atomic<float> inLevel  { 0.0f };
    std::atomic<float> outLevel { 0.0f };

    // Crossfade dry/wet
    juce::AudioBuffer<float> dryBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAudioProcessor)
};

// Factory
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
