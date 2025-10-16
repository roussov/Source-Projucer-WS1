//============================== PluginEditor.h ===============================
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GoldenKnobLNF.h"

// Barre de niveau linéaire 0..1
class LinearMeter final : public juce::Component
{
public:
    LinearMeter() = default;

    void setLevel (float v) noexcept
    {
        level = juce::jlimit (0.0f, 1.0f, v);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour::fromRGB (18,18,20));  g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour::fromRGB (10,10,12));  g.fillRoundedRectangle (r.reduced (2), 4.0f);
        auto f = r.reduced (3); f.setWidth (f.getWidth() * level);
        g.setColour (juce::Colours::white.withAlpha (0.45f)); g.fillRoundedRectangle (f, 4.0f);
    }

private:
    float level = 0.0f;
};

//==============================================================================
// Éditeur principal
class PluginAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit PluginAudioProcessorEditor (PluginAudioProcessor&);
    ~PluginAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Dimensions de référence + bornes de zoom
    static constexpr int   kW         = 820;
    static constexpr int   kH         = 520;
    static constexpr float kMinScale  = 0.85f;
    static constexpr float kMaxScale  = 1.75f;

private:
    // Halo doré puissant, sans anneau
    static void drawGoldenLight (juce::Graphics& g,
                                 juce::Rectangle<float> around,
                                 float intensity,
                                 juce::Rectangle<float> fullArea);

    // Référence processeur
    PluginAudioProcessor& proc;

    // UI
    GoldenKnobLNF knobLnf;
    juce::Slider  gain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach;

    juce::Label titleLeft  { "titleLeft",  "Roussov" };
    juce::Label titleRight { "titleRight", "Audio Unit" };
    juce::Label gainReadout      { "gainReadout",      "0.50" };
    juce::Label gainReadoutRight { "gainReadoutRight", "0.50" };

    LinearMeter meterIn, meterOut;

    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAudioProcessorEditor)
};
