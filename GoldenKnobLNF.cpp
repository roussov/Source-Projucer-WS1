//============================== GoldenKnobLNF.cpp ===============================
#include "GoldenKnobLNF.h"
#include <cmath>

void GoldenKnobLNF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                      float sliderPos, const float rotaryStart,
                                      const float rotaryEnd, juce::Slider& s)
{
    juce::ignoreUnused (s);

    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const float r  = juce::jmin ((float) w, (float) h) * 0.5f - 2.0f;

    // Ombre portée douce
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (cx - r, cy - r + 2.0f, r * 2.0f, r * 2.0f);

    // Corps doré (réagit à intensity)
    {
        const float glow = juce::jlimit (0.0f, 1.0f, intensity);
        juce::Colour cMid  = goldMid();
        juce::Colour cDark = goldDark();
        juce::Colour cHi   = goldBright().withMultipliedBrightness (1.0f + 0.6f * glow);

        juce::ColourGradient body (cMid,  cx, cy,
                                   cDark, cx, cy - r, true);
        body.addColour (0.15, cHi);
        body.addColour (0.50, cDark);
        body.addColour (0.85, cHi);
        g.setGradientFill (body);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

        // Biseaux
        g.setColour (goldEdge().withAlpha (0.55f + 0.3f * glow));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (cx - r + 2.0f, cy - r + 2.0f, (r - 2.0f) * 2.0f, (r - 2.0f) * 2.0f, 1.0f);
    }

    // Piste passive
    const float trackTh = juce::jlimit (2.0f, 6.0f, r * 0.12f);
    {
        juce::Path ring;
        ring.addCentredArc (cx, cy, r - trackTh * 0.5f, r - trackTh * 0.5f,
                            0.0f, rotaryStart, rotaryEnd, true);
        g.setColour (juce::Colour::fromRGBA (255, 255, 255, 36));
        g.strokePath (ring, juce::PathStrokeType (trackTh, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Arc actif
    {
        juce::Path arc;
        arc.addCentredArc (cx, cy, r - trackTh * 0.5f, r - trackTh * 0.5f,
                           0.0f, rotaryStart, sliderPos, true);

        juce::ColourGradient glow (goldBright(), cx, cy, goldDark(), cx, cy, true);
        glow.addColour (0.20, goldEdge());
        g.setGradientFill (glow);
        g.strokePath (arc, juce::PathStrokeType (trackTh, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Repère (dot)
    {
        const float angle = sliderPos;
        const float innerR = r * 0.58f;
        const float dotR   = juce::jlimit (3.0f, 7.0f, r * 0.12f);
        const float dx = cx + std::cos (angle) * innerR;
        const float dy = cy + std::sin (angle) * innerR;

        g.setColour (goldEdge().withMultipliedBrightness (1.0f + 0.4f * intensity));
        g.fillEllipse (dx - dotR, dy - dotR, dotR * 2.0f, dotR * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (dx - dotR, dy - dotR, dotR * 2.0f, dotR * 2.0f, 1.0f);
    }

    // Reflet spéculaire supérieur
    {
        const float span  = juce::jmax (0.0001f, rotaryEnd - rotaryStart);
        const float norm  = juce::jlimit (0.0f, 1.0f, (sliderPos - rotaryStart) / span);
        const float specA = 0.08f + 0.20f * std::pow (norm * (0.6f + 0.4f * intensity), 1.25f);

        juce::Path highlight;
        const float hr = r * 0.78f;
        highlight.addPieSegment (cx - hr, cy - hr, hr * 2.0f, hr * 2.0f,
                                 juce::MathConstants<float>::pi * 1.15f,
                                 juce::MathConstants<float>::pi * 1.85f, 0.14f);
        g.setColour (juce::Colours::white.withAlpha (specA));
        g.fillPath (highlight);
    }
}
