//============================== MainComponent.h ===========================
// Spectra — Standalone host for PluginAudioProcessor/PluginEditor
//
// - AudioDeviceManager persistant
// - Raccourcis (Cmd/Ctrl + , R B 0 +/- I)
// - Barre de statut (device, SR, buffer, CPU, XRuns)
// - Sans MIDI
// SPDX-License-Identifier: MIT

#pragma once
#include <JuceHeader.h>

class PluginAudioProcessor;
class PluginEditor;

class MainComponent final : public juce::Component,
                            private juce::KeyListener,
                            private juce::Timer,
                            private juce::ChangeListener
{
public:
    MainComponent();
    ~MainComponent() override;

    // JUCE Component
    void paint   (juce::Graphics& g) override;
    void resized() override;

private:
    // Callbacks
    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Ops / Helpers
    void openAudioSettings();   // ouvre AudioDeviceSelector (LaunchOptions async)
    void resetAudio();          // reset/restart device
    void toggleBypass();        // toggle param "bypass"
    void setGain (float g);     // force gain
    void nudgeGain (float d);   // +/- delta
    void refreshStatus();       // refresh barre statut
    void showAbout();           // fenêtre À propos

    // Members
    juce::AudioDeviceManager                    deviceManager;
    juce::AudioProcessorPlayer                  player;

    std::unique_ptr<PluginAudioProcessor>       processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    juce::Label                                 status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};


