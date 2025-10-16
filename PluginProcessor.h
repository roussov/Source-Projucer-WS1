//============================== PluginProcessor.h ===============================
#pragma once
#include <JuceHeader.h>

// Déclaration anticipée de l'éditeur
class PluginAudioProcessorEditor;

/**
 * Processeur audio principal.
 * Paramètre unique: "gain" (0..1, linéaire).
 * Expose niveaux IN/OUT lissés pour l'UI.
 */
class PluginAudioProcessor final : public juce::AudioProcessor
{
public:
    PluginAudioProcessor();
    ~PluginAudioProcessor() override;

    //==============================================================================
    // JUCE: infos plugin
    const juce::String getName() const override                       { return "Spectra"; }
    bool acceptsMidi() const override                                  { return false; }
    bool producesMidi() const override                                 { return false; }
    bool isMidiEffect() const override                                 { return false; }
    double getTailLengthSeconds() const override                       { return 0.0; }

    // Programmes (non utilisés)
    int getNumPrograms() override                                      { return 1; }
    int getCurrentProgram() override                                   { return 0; }
    void setCurrentProgram (int) override                              {}
    const juce::String getProgramName (int) override                   { return {}; }
    void changeProgramName (int, const juce::String&) override         {}

    //==============================================================================
    // Préparation / audio
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override                                   {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    //==============================================================================
    // Éditeur
    bool hasEditor() const override                                    { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    //==============================================================================
    // État
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Paramètres exposés à l'UI
    juce::AudioProcessorValueTreeState parameters;

    // Accès mètres lissés pour l'éditeur
    float getInLevel()  const noexcept { return inLevel;  }
    float getOutLevel() const noexcept { return outLevel; }

    // Fabrique de layout des paramètres
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    // Lissage mètres (exp)
    double sr = 48000.0;
    float  inLevel  = 0.0f;
    float  outLevel = 0.0f;

    // Cache pointeur sur le paramètre "gain" (0..1)
    std::atomic<float>* gainParam = nullptr;

    // Mesure moyenne absolue par bloc
    template <typename Sample>
    void processBlockT (juce::AudioBuffer<Sample>&, juce::MidiBuffer&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAudioProcessor)
};
