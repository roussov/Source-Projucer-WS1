#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Forward
class PluginEditor;

class PluginAudioProcessor final : public juce::AudioProcessor
{
public:
    PluginAudioProcessor();
    ~PluginAudioProcessor() override;          // <-- pas "= default" ici si tu le fais dans le .cpp

    // Identité
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // Programmes (obligatoires)
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;

    // Audio
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    // Editor
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // State
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // APVTS
    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "Params", createParameterLayout() };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAudioProcessor)
};
