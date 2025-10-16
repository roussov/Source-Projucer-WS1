//============================== MainComponent.cpp ============================
#include "MainComponent.h"
#include <cmath>

//=============================================================================
// Échelle UI bornée
static float uiScaleFor (const juce::Component& c)
{
    const float sx = (float) c.getWidth()  / (float) MainComponent::kWindowW;
    const float sy = (float) c.getHeight() / (float) MainComponent::kWindowH;
    return juce::jlimit (MainComponent::kMinScale, MainComponent::kMaxScale, juce::jmin (sx, sy));
}
static inline int SX (float s, int v) { return (int) std::round (v * s); }

//=============================================================================
// Halo doré puissant, sans anneau. Le bouton agit comme source lumineuse.
void MainComponent::drawGoldenLight (juce::Graphics& g,
                                     juce::Rectangle<float> around,
                                     float intensity,
                                     juce::Rectangle<float> fullArea)
{
    intensity = juce::jlimit (0.0f, 1.0f, intensity);

    const auto c = around.getCentre();
    const float diag = std::sqrt (fullArea.getWidth()*fullArea.getWidth()
                                + fullArea.getHeight()*fullArea.getHeight());
    const float R = juce::jmap (intensity, 0.0f, 1.0f, 0.0f, diag * 0.75f); // portée accrue

    // Voile global doré qui teinte l’UI
    g.setColour (juce::Colour::fromRGB (255,230,150).withAlpha (0.10f * intensity));
    g.fillRect (fullArea);

    // Noyau et halo progressif (pas d’anneau)
    const auto c0 = juce::Colour::fromRGB (255,250,210).withAlpha (0.98f * intensity); // centre aveuglant
    const auto c1 = juce::Colour::fromRGB (255,236,160).withAlpha (0.75f * intensity);
    const auto c2 = juce::Colour::fromRGB (255,220,120).withAlpha (0.34f * intensity);
    const auto c3 = juce::Colour::fromRGB (255,220,120).withAlpha (0.00f);

    juce::ColourGradient grad (c0, c.x, c.y, c3, c.x, c.y, true);
    grad.addColour (0.00, c0);
    grad.addColour (0.30, c1);
    grad.addColour (0.70, c2);
    grad.addColour (1.00, c3);

    juce::Rectangle<float> halo (c.x - R, c.y - R, R*2.0f, R*2.0f);
    g.setGradientFill (grad);
    g.fillEllipse (halo);
}

//=============================================================================
MainComponent::MainComponent()
{
    setSize (kWindowW, kWindowH);

    // Slider principal
    gain.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gain.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    gain.setRange (0.0, 1.0, 0.001);
    gain.setValue (0.50);
    gain.onValueChange = [this]
    {
        const float v = (float) gain.getValue();
        const auto s = juce::String (v, 2);
        gainReadout.setText (s, juce::dontSendNotification);
        gainReadoutRight.setText (s, juce::dontSendNotification);
        repaint();
    };

    // Readouts
    gainReadout.setJustificationType (juce::Justification::centred);
    gainReadout.setColour (juce::Label::textColourId, juce::Colours::white);
    gainReadout.setBorderSize ({ 6, 16, 6, 16 });
    gainReadout.setColour (juce::Label::backgroundColourId, juce::Colour::fromRGB (20,20,22));
    gainReadout.setColour (juce::Label::outlineColourId, juce::Colours::white.withAlpha (0.25f));
    gainReadout.setText ("0.50", juce::dontSendNotification);

    gainReadoutRight.setJustificationType (juce::Justification::centred);
    gainReadoutRight.setColour (juce::Label::textColourId, juce::Colours::white);
    gainReadoutRight.setText ("0.50", juce::dontSendNotification);

    // Titres
    titleLeft .setFont (titleLeft .getFont().withHeight (28.0f).boldened());
    titleLeft .setColour (juce::Label::textColourId, juce::Colours::white);
    titleRight.setFont (titleRight.getFont().withHeight (24.0f).boldened());
    titleRight.setColour (juce::Label::textColourId, juce::Colours::grey);

    addAndMakeVisible (titleLeft);
    addAndMakeVisible (titleRight);
    addAndMakeVisible (gain);
    addAndMakeVisible (gainReadout);
    addAndMakeVisible (gainReadoutRight);
    addAndMakeVisible (meterIn);
    addAndMakeVisible (meterOut);

    setAudioChannels (2, 2);
    startTimerHz (30);
}

MainComponent::~MainComponent() { shutdownAudio(); }

//=============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);
    sr = sampleRate > 0.0 ? sampleRate : 48000.0;
}

//=============================================================================
void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    auto* buffer = info.buffer;
    const int n = info.numSamples;

    auto* in  = buffer->getReadPointer  (juce::jmin (0, buffer->getNumChannels()-1), info.startSample);
    auto* out = buffer->getWritePointer (juce::jmin (0, buffer->getNumChannels()-1), info.startSample);

    const float g = (float) gain.getValue();
    float accIn = 0.0f, accOut = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float x = in[i];
        const float y = x * g;
        out[i] = y;
        accIn  += std::abs (x);
        accOut += std::abs (y);
    }

    const float targetIn  = juce::jlimit (0.0f, 1.0f, accIn  / (float) n);
    const float targetOut = juce::jlimit (0.0f, 1.0f, accOut / (float) n);

    const float aUp   = 1.0f - std::exp (-meterAttack  * (float) n / (float) sr);
    const float aDown = 1.0f - std::exp (-meterRelease * (float) n / (float) sr);

    auto lerp = [] (float a, float b, float t) noexcept { return a + t * (b - a); };

    inLevel  = targetIn  > inLevel  ? lerp (inLevel,  targetIn,  aUp)
                                    : lerp (inLevel,  targetIn,  aDown);
    outLevel = targetOut > outLevel ? lerp (outLevel, targetOut, aUp)
                                    : lerp (outLevel, targetOut, aDown);

    meterIn.setLevel  (inLevel);
    meterOut.setLevel (outLevel);
}

//=============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (14, 14, 16));

    const auto area = getLocalBounds().toFloat();

    // Halo doré intensifié selon volume
    const float v = (float) gain.getValue();
    const float intensity = std::pow (v, 1.8f);
    drawGoldenLight (g, gain.getBounds().toFloat(), intensity, area);
}

//=============================================================================
void MainComponent::resized()
{
    const float s = uiScaleFor (*this);

    titleLeft .setBounds (SX (s, 32),              SX (s, 64), SX (s, 240), SX (s, 40));
    titleRight.setBounds (getWidth() - SX (s,220), SX (s, 64), SX (s, 200), SX (s, 40));
    titleLeft .setFont (titleLeft .getFont().withHeight (28.0f * s).boldened());
    titleRight.setFont (titleRight.getFont().withHeight (24.0f * s).boldened());

    // Bouton centré au milieu du halo
    const int knobSize = SX (s, 180);
    const int knobX = (getWidth()  - knobSize) / 2;
    const int knobY = (getHeight() - knobSize) / 2;
    gain.setBounds (knobX, knobY, knobSize, knobSize);

    gainReadout.setBounds      (getWidth()/2 - SX (s, 60), knobY + knobSize + SX (s, 20), SX (s,120), SX (s,36));
    gainReadoutRight.setBounds (getWidth()/2 + SX (s,140), knobY + knobSize/2 - SX (s,15), SX (s,80),  SX (s,30));

    meterIn .setBounds (SX (s, 88),                 getHeight() - SX (s,140),
                        juce::jmax (SX (s,180), getWidth()/2 - SX (s,160)), SX (s,18));
    meterOut.setBounds (getWidth()/2 + SX (s,128),  getHeight() - SX (s,140),
                        juce::jmax (SX (s,180), getWidth()/2 - SX (s,168)), SX (s,18));
}

//=============================================================================
void MainComponent::timerCallback()
{
    const float s = uiScaleFor (*this);
    repaint (juce::Rectangle<int> (0, SX (s, 340), getWidth(), SX (s, 80)));
}
