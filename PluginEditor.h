#pragma once

#include <JuceHeader.h>
#include <atomic>

class PluginAudioProcessor;

//==================== RoussovLookAndFeel ====================
class RoussovLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RoussovLookAndFeel();
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;
private:
    juce::Image knobNoise;
};

//==================== ActivityLED (utilitaire générique) ====================
class ActivityLED : public juce::Component
{
public:
    void setBrightness (float a) { brightness = a; repaint(); }
    void trigger (float a)       { brightness = a; repaint(); }
    void paint (juce::Graphics& g) override;

private:
    float brightness { 0.0f };
};

//==================== TinyBarMeter ====================
class TinyBarMeter : public juce::Component
{
public:
    void setLevel (float v) { level = v; repaint(); }
    void paint (juce::Graphics& g) override;

private:
    float level { 0.0f }; // lin 0..1
};

//==================== PluginEditor ====================
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit PluginEditor (PluginAudioProcessor& p);
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Meters feed (thread-safe)
    void setInputLevel  (float linear01);
    void setOutputLevel (float linear01);

private:
    void timerCallback() override;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    PluginAudioProcessor& processor;

    RoussovLookAndFeel lnf;

    // UI
    juce::Label  titleLabel, subtitleLabel;
    juce::Slider volumeDial;
    juce::Label  valueLabel;

    juce::Label     inLabel, outLabel;
    TinyBarMeter    inMeter, outMeter;

    std::unique_ptr<SliderAttachment> volumeAttachment;

    // Meter state
    std::atomic<float> inLin  { 0.0f };
    std::atomic<float> outLin { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
