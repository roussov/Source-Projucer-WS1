#include "PluginEditor.h"
#include "PluginProcessor.h"

//==================== RoussovLookAndFeel ====================
RoussovLookAndFeel::RoussovLookAndFeel()
{
    knobNoise = juce::Image (juce::Image::ARGB, 64, 64, true);
    juce::Graphics ng (knobNoise);
    juce::Random rng (0x13579BDF);
    for (int y = 0; y < knobNoise.getHeight(); ++y)
        for (int x = 0; x < knobNoise.getWidth(); ++x)
        {
            const juce::uint8 n = (juce::uint8) rng.nextInt (15);
            ng.setColour (juce::Colour::fromRGBA (255, 255, 255, n));
            ng.fillRect (x, y, 1, 1);
        }
}

void RoussovLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                           float pos, float startAngle, float endAngle,
                                           juce::Slider& s)
{
    g.saveState();

    auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced (4.0f);
    auto centre = bounds.getCentre();
    const float rOuter = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float rInner = rOuter * 0.64f;
    const float rRing  = rOuter * 0.88f;
    const float angle  = startAngle + pos * (endAngle - startAngle);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (bounds.translated (0, 1.5f));

    auto bodyTop = juce::Colour::fromRGB (22, 24, 26);
    auto bodyBot = juce::Colour::fromRGB (10, 11, 12);
    g.setGradientFill (juce::ColourGradient (bodyTop, bounds.getX(), bounds.getY(),
                                             bodyBot, bounds.getX(), bounds.getBottom(), false));
    g.fillEllipse (bounds);

    g.setTiledImageFill (knobNoise, (int)bounds.getX(), (int)bounds.getY(), 0.08f);
    g.fillEllipse (bounds);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawEllipse (bounds.reduced (0.5f), 1.0f);

    const float glowAmt = juce::jlimit (0.0f, 1.0f, 0.18f + 0.75f * pos);
    auto innerHi = juce::Colours::white.withAlpha (0.12f + 0.25f * glowAmt);
    auto innerLo = juce::Colours::black.withAlpha  (0.60f);
    g.setGradientFill (juce::ColourGradient (innerHi, centre.x, centre.y,
                                             innerLo, centre.x, centre.y + rInner, true));
    g.fillEllipse ({ centre.x - rInner, centre.y - rInner, rInner * 2.0f, rInner * 2.0f });

    juce::Path spec;
    spec.addPieSegment (centre.x - rInner, centre.y - rInner, rInner * 2.0f, rInner * 2.0f,
                        juce::degreesToRadians (-140.0f), juce::degreesToRadians (-40.0f), 0.70);
    g.setColour (juce::Colours::white.withAlpha (0.08f + 0.10f * glowAmt));
    g.fillPath (spec);

    auto accent = s.findColour (juce::Slider::thumbColourId).withMultipliedBrightness (1.05f);
    juce::Path ring, track;
    ring.addCentredArc (centre.x, centre.y, rRing, rRing, 0.0f, startAngle, angle, true);
    track.addCentredArc(centre.x, centre.y, rRing, rRing, 0.0f, startAngle, endAngle, true);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.strokePath (track, juce::PathStrokeType (juce::jmax (1.5f, rOuter * 0.08f)));

    g.setColour (accent);
    g.strokePath (ring, juce::PathStrokeType (juce::jmax (1.5f, rOuter * 0.08f),
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));

    juce::Path tick;
    const float tickLen = rOuter * 0.46f;
    tick.addRoundedRectangle (-1.5f, -tickLen, 3.0f, tickLen * 0.40f, 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.90f));
    g.addTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.fillPath (tick);

    g.restoreState();
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
    setSize (520, 260);

    addAndMakeVisible (titleLabel);
    titleLabel.setText ("Roussov", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));

    addAndMakeVisible (subtitleLabel);
    subtitleLabel.setText ("Audio Unit", juce::dontSendNotification);
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colours::grey);

    auto prepDial = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
        s.setRange (0.0, 1.0, PluginAudioProcessor::kParamStep);
        s.setDoubleClickReturnValue (true, 0.5);
        s.setColour (juce::Slider::thumbColourId, juce::Colours::deepskyblue.withAlpha (0.95f));
    };
    prepDial (inDial);
    prepDial (outDial);

    inLabel.setText  ("IN",  juce::dontSendNotification);
    outLabel.setText ("OUT", juce::dontSendNotification);

    inValLabel.setJustificationType  (juce::Justification::centred);
    outValLabel.setJustificationType (juce::Justification::centred);
    inValLabel .setText ("0.50", juce::dontSendNotification);
    outValLabel.setText ("0.50", juce::dontSendNotification);
    inValLabel .setTooltip ("Pré-gain linéaire 0..1, pas 0.01");
    outValLabel.setTooltip ("Post-gain linéaire 0..1, pas 0.01");

    addAndMakeVisible (inLabel);   addAndMakeVisible (outLabel);
    addAndMakeVisible (inDial);    addAndMakeVisible (outDial);
    addAndMakeVisible (inValLabel);addAndMakeVisible (outValLabel);

    midiLabel.setText ("MIDI CH", juce::dontSendNotification);
    addAndMakeVisible (midiLabel);
    addAndMakeVisible (midiChanBox);
    midiChanBox.addItem ("Omni", 1);
    for (int ch = 1; ch <= 16; ++ch) midiChanBox.addItem ("Ch " + juce::String (ch), ch + 1);

    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    inAttachment  = std::make_unique<SliderAttachment>  (processor.getValueTreeState(), "inTrim",   inDial);
    outAttachment = std::make_unique<SliderAttachment>  (processor.getValueTreeState(), "outVol",   outDial);
    midiAttachment= std::make_unique<ComboBoxAttachment>(processor.getValueTreeState(), "midiChan", midiChanBox);

    inDial.onValueChange  = [this]{ inValLabel .setText (juce::String (inDial.getValue(),  2), juce::dontSendNotification); };
    outDial.onValueChange = [this]{ outValLabel.setText (juce::String (outDial.getValue(), 2), juce::dontSendNotification); };

    startTimerHz (30);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
    inAttachment.reset();
    outAttachment.reset();
    midiAttachment.reset();
}

void PluginEditor::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient(
        juce::Colour::fromFloatRGBA (0.07f, 0.07f, 0.08f, 1.0f), b.getX(), b.getY(),
        juce::Colour::fromFloatRGBA (0.04f, 0.04f, 0.05f, 1.0f), b.getX(), b.getBottom(), false));
    g.fillRect (b);

    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (juce::Rectangle<float>(b.getX() + 16.0f, b.getY() + 72.0f,        b.getWidth() - 32.0f, 1.0f));
    g.fillRect (juce::Rectangle<float>(b.getX() + 16.0f, b.getBottom() - 56.0f,   b.getWidth() - 32.0f, 1.0f));
}

void PluginEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    auto top = r.removeFromTop (48);
    titleLabel   .setBounds (top.removeFromLeft (200));
    subtitleLabel.setBounds (top);

    auto mid = r.removeFromTop (120);
    auto leftDial  = mid.removeFromLeft (180).reduced (8);
    auto rightDial = mid.removeFromLeft (180).reduced (8);

    inLabel     .setBounds (leftDial.removeFromTop (18));
    inDial      .setBounds (leftDial.removeFromTop (leftDial.getHeight() - 22));
    inValLabel  .setBounds (leftDial);

    outLabel    .setBounds (rightDial.removeFromTop (18));
    outDial     .setBounds (rightDial.removeFromTop (rightDial.getHeight() - 22));
    outValLabel .setBounds (rightDial);

    auto midiRow = r.removeFromTop (28);
    midiLabel   .setBounds (midiRow.removeFromLeft (70));
    midiChanBox .setBounds (midiRow.removeFromLeft (140));

    auto metersRow = r.removeFromTop (36);
    auto colW = metersRow.getWidth() / 2;
    auto inCol  = metersRow.removeFromLeft (colW);
    auto outCol = metersRow;

    inMeter .setBounds (inCol .reduced (2, 8));
    outMeter.setBounds (outCol.reduced (2, 8));
}

void PluginEditor::timerCallback()
{
    inMeter .setLevel (processor.getInputLevel());
    outMeter.setLevel (processor.getOutputLevel());
    repaint();
}

void PluginEditor::setInputLevel (float linear01)
{
    inLin.store (juce::jlimit (0.0f, 1.0f, linear01), std::memory_order_relaxed);
}

void PluginEditor::setOutputLevel (float linear01)
{
    outLin.store (juce::jlimit (0.0f, 1.0f, linear01), std::memory_order_relaxed);
}
