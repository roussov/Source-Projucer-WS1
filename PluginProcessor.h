#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PluginAudioProcessor : public juce::AudioProcessor
{
public:
    PluginAudioProcessor();
    ~PluginAudioProcessor() override = default; // pas de redéfinition dans le .cpp

    // ===== AudioProcessor overrides =====
    const juce::String getName() const override;
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // ===== APVTS =====
    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS apvts;

    APVTS& getValueTreeState() noexcept { return apvts; }
    static APVTS::ParameterLayout createParameterLayout();

    // ===== Editor =====
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // ===== State (implémentation dans .cpp) =====
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
};
