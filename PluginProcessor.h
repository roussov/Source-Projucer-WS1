#pragma once
#include <JuceHeader.h>
#include <atomic>

class PluginEditor;

class PluginAudioProcessor : public juce::AudioProcessor
{
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    PluginAudioProcessor();
    ~PluginAudioProcessor() override;

    //== AudioProcessor overrides ==
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //== Editor ==
    bool hasEditor() const override;
    juce::AudioProcessorEditor* createEditor() override;

    //== State ==
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //== Params / State access ==
    APVTS& getValueTreeState() noexcept { return valueTreeState; }

    // Meters lus par l’éditeur (poll depuis Timer)
    float getInputMeter()  const noexcept { return meterIn.load (std::memory_order_relaxed); }
    float getOutputMeter() const noexcept { return meterOut.load(std::memory_order_relaxed); }

private:
    // Param layout
    static APVTS::ParameterLayout createParameterLayout();

    // Params
    APVTS valueTreeState;

    // getRawParameterValue() renvoie std::atomic<float>* — on stocke des pointeurs.
    std::atomic<float>* gainParam   { nullptr }; // 0..1
    std::atomic<float>* bypassParam { nullptr }; // 0/1

    // DSP — NB: le 2e paramètre est juce::ValueSmoothingTypes (namespace global juce)
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedGain;

    std::atomic<float> meterIn  { 0.0f }; // 0..1
    std::atomic<float> meterOut { 0.0f }; // 0..1
    double fs { 44100.0 };

    // Ballistics (calculés dans prepareToPlay)
    float meterRiseCoeff { 0.0f }; // montée rapide
    float meterFallCoeff { 0.0f }; // descente lente

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAudioProcessor)
};
