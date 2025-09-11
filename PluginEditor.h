#pragma once

#include <juce_gui_basics/juce_gui_basics.h>      // ← nécessaire pour Component, Label, Slider, addAndMakeVisible, etc.
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h> // ← nécessaire pour AudioProcessorEditor + APVTS::SliderAttachment
#include <atomic>

class PluginAudioProcessor; // déclaré dans PluginProcessor.h

// ─────────────────── Helpers ───────────────────
using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

// ─────────────────── Look & Feel ───────────────────
class RoussovLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    RoussovLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

private:
    juce::Image knobNoise;
};

// ─────────────────── LED ───────────────────
class ActivityLED final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    // Appelle quand un événement survient (valeur 0..1)
    void trigger (float v) { brightness = juce::jmax (brightness, v); }

private:
    float brightness { 0.0f }; // 0..1
};

// ─────────────────── Mini Vu-Mètre ───────────────────
class TinyBarMeter final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void setLevel (float v) { level = juce::jlimit (0.0f, 1.0f, v); }

private:
    float level { 0.0f }; // 0..1
};

// ─────────────────── PluginEditor ───────────────────
class PluginEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit PluginEditor (PluginAudioProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // API thread-safe appelé par le Processor
    void setInputLevel  (float linear01);
    void setOutputLevel (float linear01);
    void notifyMidiIn   (float strength01);
    void notifyMidiOut  (float strength01);

private:
    void timerCallback() override;

    PluginAudioProcessor& processor;
    RoussovLookAndFeel lnf;

    // UI
    juce::Label  titleLabel, subtitleLabel, valueLabel, inLabel, outLabel, midiInLabel, midiOutLabel;
    juce::Slider volumeDial;
    std::unique_ptr<SliderAttachment> volumeAttachment;
    TinyBarMeter inMeter, outMeter;
    ActivityLED  midiInLed, midiOutLed;

    // niveaux affichage (alimentés par le Processor)
    std::atomic<float> inLin { 0.0f }, outLin { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
