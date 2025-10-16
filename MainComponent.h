//============================== MainComponent.h ===============================
#pragma once
#include <JuceHeader.h>

//=============================================================================
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
        g.setColour (juce::Colour::fromRGB (18,18,20));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colour::fromRGB (10,10,12));
        g.fillRoundedRectangle (r.reduced (2), 4.0f);

        auto f = r.reduced (3);
        f.setWidth (f.getWidth() * level);
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.fillRoundedRectangle (f, 4.0f);
    }

private:
    float level = 0.0f;
};

//=============================================================================
// Composant principal
class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    // Dimensions de référence (pour échelle responsive)
    static constexpr int   kWindowW   = 820;
    static constexpr int   kWindowH   = 520;
    static constexpr float kMinScale  = 0.85f;
    static constexpr float kMaxScale  = 1.75f;

    // Audio
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override {}

    // UI
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Halo “Lumière Dorée” — version sans anneau, rayonnement puissant
    static void drawGoldenLight (juce::Graphics& g,
                                 juce::Rectangle<float> around,
                                 float intensity,
                                 juce::Rectangle<float> fullArea);

    // Contrôles
    juce::Slider gain;
    juce::Label  gainReadout;       // valeur sous le knob
    juce::Label  gainReadoutRight;  // valeur à droite
    juce::Label  titleLeft  { "titleLeft",  "Roussov" };
    juce::Label  titleRight { "titleRight", "Audio Unit" };
    LinearMeter  meterIn, meterOut;

    // État audio/mètres
    float  inLevel      = 0.0f;
    float  outLevel     = 0.0f;
    float  meterAttack  = 0.25f;
    float  meterRelease = 0.04f;
    double sr           = 48000.0;

    // Timer
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
