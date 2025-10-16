//============================== GoldenKnobLNF.h ===============================
#pragma once
#include <JuceHeader.h>

// Look&Feel du bouton rotatif doré
class GoldenKnobLNF final : public juce::LookAndFeel_V4
{
public:
    GoldenKnobLNF() = default;
    ~GoldenKnobLNF() override = default;

    // 0..1 contrôle la brillance
    void setIntensity (float v) noexcept { intensity = juce::jlimit (0.0f, 1.0f, v); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPos, const float rotaryStart,
                           const float rotaryEnd, juce::Slider& s) override;

private:
    float intensity = 0.0f;

    // Palette dorée
    static juce::Colour goldDark()   noexcept { return juce::Colour::fromRGB (130, 98, 38); }
    static juce::Colour goldMid()    noexcept { return juce::Colour::fromRGB (212,170,70); }
    static juce::Colour goldBright() noexcept { return juce::Colour::fromRGB (255,224,120); }
    static juce::Colour goldEdge()   noexcept { return juce::Colour::fromRGB (255,210,90); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GoldenKnobLNF)
};
