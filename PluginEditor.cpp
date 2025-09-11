#include "PluginEditor.h"
#include "PluginProcessor.h"

//==================== RoussovLookAndFeel ====================
RoussovLookAndFeel::RoussovLookAndFeel()
{
    // Mini texture bruitée pour effet “brushed/linen”
    knobNoise = juce::Image (juce::Image::ARGB, 64, 64, true);
    juce::Graphics ng (knobNoise);
    juce::Random rng (0x13579BDF); // graine fixe

    for (int y = 0; y < knobNoise.getHeight(); ++y)
        for (int x = 0; x < knobNoise.getWidth(); ++x)
        {
            const juce::uint8 n = (juce::uint8) rng.nextInt (15); // alpha discret 0..14
            ng.setColour (juce::Colour::fromRGBA (255, 255, 255, n));
            ng.fillRect (x, y, 1, 1);
        }
}

void RoussovLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                           float pos, float startAngle, float endAngle,
                                           juce::Slider& s)
{
    g.saveState();

    const auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced (4.0f);
    const auto centre = bounds.getCentre();
    const float rOuter = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float rInner = rOuter * 0.64f;
    const float rRing  = rOuter * 0.88f;
    const float angle  = startAngle + pos * (endAngle - startAngle);

    // Ombre douce
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (bounds.translated (0, 1.5f));

    // Corps du knob
    const auto bodyTop = juce::Colour::fromRGB (22, 24, 26);
    const auto bodyBot = juce::Colour::fromRGB (10, 11, 12);
    g.setGradientFill (juce::ColourGradient (bodyTop, bounds.getX(), bounds.getY(),
                                             bodyBot, bounds.getX(), bounds.getBottom(), false));
    g.fillEllipse (bounds);

    // Texture discrète
    g.setTiledImageFill (knobNoise, (int)bounds.getX(), (int)bounds.getY(), 0.08f);
    g.fillEllipse (bounds);

    // Chanfrein extérieur
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (bounds.reduced (0.5f), 1.0f);

    // Lueur interne qui augmente avec pos
    const float glowAmt = juce::jlimit (0.0f, 1.0f, 0.18f + 0.75f * pos);
    const auto  innerHi = juce::Colours::white.withAlpha (0.12f + 0.25f * glowAmt);
    const auto  innerLo = juce::Colours::black.withAlpha  (0.60f);
    g.setGradientFill (juce::ColourGradient (innerHi, centre.x, centre.y,
                                             innerLo, centre.x, centre.y + rInner, true));
    g.fillEllipse ({ centre.x - rInner, centre.y - rInner, rInner * 2.0f, rInner * 2.0f });

    // Reflet spéculaire
    juce::Path spec;
    spec.addPieSegment (centre.x - rInner, centre.y - rInner, rInner * 2.0f, rInner * 2.0f,
                        juce::degreesToRadians (-140.0f), juce::degreesToRadians (-40.0f), 0.70);
    g.setColour (juce::Colours::white.withAlpha (0.08f + 0.10f * glowAmt));
    g.fillPath (spec);

    // Anneau de progression
    const auto accent = s.findColour (juce::Slider::thumbColourId).withMultipliedBrightness (1.05f);
    juce::Path ring, track;
    ring.addCentredArc (centre.x, centre.y, rRing, rRing, 0.0f, startAngle, angle, true);
    track.addCentredArc(centre.x, centre.y, rRing, rRing, 0.0f, startAngle, endAngle, true);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.strokePath (track, juce::PathStrokeType (juce::jmax (1.5f, rOuter * 0.08f)));

    g.setColour (accent);
    g.strokePath (ring, juce::PathStrokeType (
        juce::jmax (1.5f, rOuter * 0.08f),
        juce::PathStrokeType::JointStyle::curved,
        juce::PathStrokeType::EndCapStyle::rounded
    ));

    // Repère
    juce::Path tick;
    const float tickLen = rOuter * 0.46f;
    tick.addRoundedRectangle (-1.5f, -tickLen, 3.0f, tickLen * 0.40f, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.90f));
    g.addTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.fillPath (tick);

    g.restoreState();
}

//==================== ActivityLED ====================
void ActivityLED::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (1.0f);
    const float r = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const auto  c = area.getCentre();

    const float a  = juce::jlimit (0.0f, 1.0f, brightness);
    const auto on  = juce::Colour::fromRGB (180, 255, 120).withAlpha (0.80f * a);
    const auto off = juce::Colours::black.withAlpha (0.70f);

    if (a > 0.01f)
    {
        juce::ColourGradient halo (on.withAlpha (0.28f), c.x, c.y,
                                   on.withAlpha (0.00f), c.x, c.y + r * 2.2f, true);
        g.setGradientFill (halo);
        g.fillEllipse (area.expanded (r * 0.55f));
    }

    g.setColour (off);
    g.fillEllipse (area);

    g.setColour (on);
    g.fillEllipse (area.reduced (r * 0.30f));

    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawEllipse (area, 1.0f);
}

//==================== TinyBarMeter ====================
void TinyBarMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r, 2.0f);

    const float v = juce::jlimit (0.0f, 1.0f, level);
    auto fill = r.withWidth (juce::jmax (2.0f, r.getWidth() * v));

    auto c1 = juce::Colours::deepskyblue.withAlpha (0.95f);
    auto c2 = juce::Colours::white.withAlpha (0.35f);
    g.setGradientFill (juce::ColourGradient (c1, fill.getX(), fill.getY(),
                                             c2, fill.getX(), fill.getBottom(), false));
    g.fillRoundedRectangle (fill.reduced (1.0f), 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f), 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
}

//==================== PluginEditor ====================
PluginEditor::PluginEditor (PluginAudioProcessor& p)
  : juce::AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);
    setResizable (true, true);
    setSize (420, 260);

    // Titre
    this->addAndMakeVisible (titleLabel);
    titleLabel.setText ("Roussov", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));


    this->addAndMakeVisible (subtitleLabel);
    subtitleLabel.setText ("Audio Unit", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colours::grey);

    // Volume
    this->addAndMakeVisible (volumeDial);
    volumeDial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    volumeDial.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    volumeDial.setRange (0.0, 1.0, 0.001);
    volumeDial.setDoubleClickReturnValue (true, 0.5);
    volumeDial.setColour (juce::Slider::thumbColourId, juce::Colours::deepskyblue.withAlpha (0.95f));

    this->addAndMakeVisible (valueLabel);
    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setText ("0.50", juce::dontSendNotification);
    valueLabel.setTooltip ("Gain (linéaire 0..1)");

    // Meters & LEDs
    this->addAndMakeVisible (inLabel);    inLabel.setText ("IN",  juce::dontSendNotification);
    this->addAndMakeVisible (outLabel);   outLabel.setText ("OUT", juce::dontSendNotification);
    this->addAndMakeVisible (inMeter);
    this->addAndMakeVisible (outMeter);

    this->addAndMakeVisible (midiInLabel);   midiInLabel.setText ("MIDI IN",  juce::dontSendNotification);
    this->addAndMakeVisible (midiOutLabel);  midiOutLabel.setText ("MIDI OUT", juce::dontSendNotification);
    this->addAndMakeVisible (midiInLed);
    this->addAndMakeVisible (midiOutLed);

    // Attachment (ID "gain" — ajuste si ton param diffère)
    if (auto* pParam = processor.getValueTreeState().getParameter ("gain"))
        volumeAttachment = std::make_unique<SliderAttachment> (
            processor.getValueTreeState(), "gain", volumeDial);

    // Affichage live
    volumeDial.onValueChange = [this]
    {
        valueLabel.setText (juce::String (volumeDial.getValue(), 2), juce::dontSendNotification);
    };

    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
    volumeAttachment.reset();
}

void PluginEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    // Dégradé sobre premium
    g.setGradientFill (juce::ColourGradient(
        juce::Colour::fromFloatRGBA (0.07f, 0.07f, 0.08f, 1.0f), b.getX(), b.getY(),
        juce::Colour::fromFloatRGBA (0.04f, 0.04f, 0.05f, 1.0f), b.getX(), b.getBottom(), false));
    g.fillRect (b);

    // Traits décoratifs subtils
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (juce::Rectangle<float> (b.getX() + 16.0f, b.getY() + 72.0f, b.getWidth() - 32.0f, 1.0f));
    g.fillRect (juce::Rectangle<float> (b.getX() + 16.0f, b.getBottom() - 56.0f, b.getWidth() - 32.0f, 1.0f));
}

void PluginEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    auto top = r.removeFromTop (48);
    titleLabel   .setBounds (top.removeFromLeft (200));
    subtitleLabel.setBounds (top);

    auto mid = r.removeFromTop (112);
    auto dialArea = mid.removeFromLeft (140);
    volumeDial.setBounds (dialArea.reduced (8));
    valueLabel.setBounds (mid.removeFromLeft (80));

    auto metersRow = r.removeFromTop (36);
    auto colW = metersRow.getWidth() / 2;

    auto inCol  = metersRow.removeFromLeft (colW);
    auto outCol = metersRow;

    const int labW = 40;
    inLabel .setBounds (inCol.removeFromLeft (labW));
    inMeter .setBounds (inCol.reduced (2, 8));

    outLabel.setBounds (outCol.removeFromLeft (labW));
    outMeter.setBounds (outCol.reduced (2, 8));

    auto bottom = r.removeFromBottom (40);
    const int ledSize = 16;

    auto leftBottom  = bottom.removeFromLeft (bottom.getWidth() / 2);
    midiInLabel.setBounds (leftBottom.removeFromLeft (70));
    midiInLed  .setBounds (leftBottom.removeFromLeft (ledSize));

    midiOutLabel.setBounds (bottom.removeFromLeft (70));
    midiOutLed  .setBounds (bottom.removeFromLeft (ledSize));
}

void PluginEditor::timerCallback()
{
    // Update meters (thread-safe)
    inMeter .setLevel (inLin .load (std::memory_order_relaxed));
    outMeter.setLevel (outLin.load (std::memory_order_relaxed));

    repaint();
}

void PluginEditor::setInputLevel (float linear01)  { inLin.store  (juce::jlimit (0.0f, 1.0f, linear01), std::memory_order_relaxed); }
void PluginEditor::setOutputLevel(float linear01)  { outLin.store (juce::jlimit (0.0f, 1.0f, linear01), std::memory_order_relaxed); }
void PluginEditor::notifyMidiIn  (float s)         { midiInLed .trigger (juce::jlimit (0.0f, 1.0f, s)); }
void PluginEditor::notifyMidiOut (float s)         { midiOutLed.trigger (juce::jlimit (0.0f, 1.0f, s)); }
