//============================== PluginEditor.h ================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

class PluginAudioProcessor; // défini dans PluginProcessor.h

using SliderAttachment    = juce::AudioProcessorValueTreeState::SliderAttachment;
using ComboBoxAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

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

// ─────────────────── Mini Vu-Mètre ───────────────────
class TinyBarMeter final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void setLevel (float v) { level = juce::jlimit (0.0f, 1.0f, v); repaint(); }

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

    // API thread-safe
    void setInputLevel  (float linear01);
    void setOutputLevel (float linear01);

    void notifyMidiIn  (float) {}
    void notifyMidiOut (float) {}

private:
    void timerCallback() override;

    PluginAudioProcessor& processor;
    RoussovLookAndFeel   lnf;

    // UI
    juce::Label  titleLabel, subtitleLabel;
    juce::Label  inLabel, outLabel, inValLabel, outValLabel, midiLabel;
    juce::Slider inDial, outDial;               // In = "inTrim", Out = "outVol"
    juce::ComboBox midiChanBox;                 // 0=Omni, 1..16
    std::unique_ptr<SliderAttachment>   inAttachment, outAttachment;
    std::unique_ptr<ComboBoxAttachment> midiAttachment;
    TinyBarMeter inMeter, outMeter;

    // niveaux affichés (thread-safe)
    std::atomic<float> inLin { 0.0f }, outLin { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
