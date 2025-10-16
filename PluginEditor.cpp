//============================== PluginEditor.cpp =============================
#include "PluginEditor.h"
#include <cmath>

//=============================================================================
// Échelle UI bornée
static float uiScaleFor (const juce::Component& c)
{
    const float sx = (float) c.getWidth()  / (float) PluginAudioProcessorEditor::kW;
    const float sy = (float) c.getHeight() / (float) PluginAudioProcessorEditor::kH;
    return juce::jlimit (PluginAudioProcessorEditor::kMinScale,
                         PluginAudioProcessorEditor::kMaxScale,
                         juce::jmin (sx, sy));
}
static inline int SX (float s, int v) { return (int) std::round (v * s); }

//=============================================================================
// Halo “Lumière Dorée” intensifié, sans anneau visible
void PluginAudioProcessorEditor::drawGoldenLight (juce::Graphics& g,
                                                  juce::Rectangle<float> around,
                                                  float intensity,
                                                  juce::Rectangle<float> fullArea)
{
    intensity = juce::jlimit (0.0f, 1.0f, intensity);

    const auto c = around.getCentre();
    const float diag = std::sqrt (fullArea.getWidth()*fullArea.getWidth()
                                + fullArea.getHeight()*fullArea.getHeight());
    const float R = juce::jmap (intensity, 0.0f, 1.0f, 0.0f, diag * 0.75f);

    // Éclairage global
    g.setColour (juce::Colour::fromRGB (255,230,150).withAlpha (0.12f * intensity));
    g.fillRect (fullArea);

    // Noyau lumineux
    const auto c0 = juce::Colour::fromRGB (255,250,210).withAlpha (0.98f * intensity);
    const auto c1 = juce::Colour::fromRGB (255,236,160).withAlpha (0.75f * intensity);
    const auto c2 = juce::Colour::fromRGB (255,220,120).withAlpha (0.34f * intensity);
    const auto c3 = juce::Colour::fromRGB (255,220,120).withAlpha (0.00f);

    juce::ColourGradient grad (c0, c.x, c.y, c3, c.x, c.y, true);
    grad.addColour (0.00, c0);
    grad.addColour (0.25, c1);
    grad.addColour (0.70, c2);
    grad.addColour (1.00, c3);

    juce::Rectangle<float> halo (c.x - R, c.y - R, R*2.0f, R*2.0f);
    g.setGradientFill (grad);
    g.fillEllipse (halo);
}

//=============================================================================
PluginAudioProcessorEditor::PluginAudioProcessorEditor (PluginAudioProcessor& p)
    : AudioProcessorEditor (&p)
    , proc (p)
{
    setOpaque (true);
    setSize (kW, kH);

    // Redimensionnable + bornes
    const int minW = (int) std::round (kW * kMinScale);
    const int minH = (int) std::round (kH * kMinScale);
    const int maxW = (int) std::round (kW * kMaxScale);
    const int maxH = (int) std::round (kH * kMaxScale);
    setResizable (true, true);
    setResizeLimits (minW, minH, maxW, maxH);

    // Titres
    titleLeft .setFont (titleLeft .getFont().withHeight (28.0f).boldened());
    titleLeft .setColour (juce::Label::textColourId, juce::Colours::white);
    titleRight.setFont (titleRight.getFont().withHeight (24.0f).boldened());
    titleRight.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (titleLeft);
    addAndMakeVisible (titleRight);

    // Knob doré
    gain.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gain.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    gain.setLookAndFeel (&knobLnf);
    gain.setRange (0.0, 1.0, 0.001);
    gain.setValue (0.5);
    addAndMakeVisible (gain);

    // APVTS
    gainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (proc.parameters, "gain", gain);

    // Readouts
    for (auto* l : { &gainReadout, &gainReadoutRight })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (*l);
    }
    gainReadout.setBorderSize ({ 6, 16, 6, 16 });
    gainReadout.setColour (juce::Label::backgroundColourId, juce::Colour::fromRGB (20,20,22));
    gainReadout.setColour (juce::Label::outlineColourId, juce::Colours::white.withAlpha (0.25f));

    // Mètres
    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);

    // Lien entre volume et luminosité
    gain.onValueChange = [this]
    {
        const float v = (float) gain.getValue();
        const auto s = juce::String (v, 2);
        gainReadout.setText (s, juce::dontSendNotification);
        gainReadoutRight.setText (s, juce::dontSendNotification);
        knobLnf.setIntensity (std::pow (v, 1.8f));
        repaint();
    };

    startTimerHz (30);
}

//=============================================================================
PluginAudioProcessorEditor::~PluginAudioProcessorEditor()
{
    gain.setLookAndFeel (nullptr);
}

//=============================================================================
void PluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (14, 14, 16));
    const float s = uiScaleFor (*this);
    const auto area = getLocalBounds().toFloat();

    // Halo global doré selon volume
    const float v = (float) gain.getValue();
    const float intensity = std::pow (v, 1.8f);
    drawGoldenLight (g, gain.getBounds().toFloat(), intensity, area);

    // Titres
    g.setFont (16.0f * s);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawFittedText ("IN",  SX (s, 40), SX (s, 356), SX (s, 40), SX (s, 20), juce::Justification::centredLeft, 1);
    g.drawFittedText ("OUT", getWidth()/2 + SX (s, 80), SX (s, 356), SX (s, 40), SX (s, 20), juce::Justification::centredLeft, 1);
}

//=============================================================================
void PluginAudioProcessorEditor::resized()
{
    const float s = uiScaleFor (*this);

    // Titres
    titleLeft .setBounds (SX (s, 32),              SX (s, 64), SX (s, 240), SX (s, 40));
    titleRight.setBounds (getWidth() - SX (s,220), SX (s, 64), SX (s, 200), SX (s, 40));

    // Bouton centré
    const int knobSize = SX (s, 180);
    const int knobX = (getWidth()  - knobSize) / 2;
    const int knobY = (getHeight() - knobSize) / 2;
    gain.setBounds (knobX, knobY, knobSize, knobSize);

    gainReadout.setBounds      (getWidth()/2 - SX (s,60), knobY + knobSize + SX (s,20), SX (s,120), SX (s,36));
    gainReadoutRight.setBounds (getWidth()/2 + SX (s,140), knobY + knobSize/2 - SX (s,15), SX (s,80), SX (s,30));

    meterIn .setBounds (SX (s, 88),                 getHeight() - SX (s,140),
                        juce::jmax (SX (s,180), getWidth()/2 - SX (s,160)), SX (s,18));
    meterOut.setBounds (getWidth()/2 + SX (s,128),  getHeight() - SX (s,140),
                        juce::jmax (SX (s,180), getWidth()/2 - SX (s,168)), SX (s,18));
}

//=============================================================================
void PluginAudioProcessorEditor::timerCallback()
{
    const float s = uiScaleFor (*this);
    meterIn .setLevel (proc.getInLevel());
    meterOut.setLevel (proc.getOutLevel());
    repaint (juce::Rectangle<int> (0, SX (s, 340), getWidth(), SX (s, 80)));
}
