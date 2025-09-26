//============================== MainComponent.h ===========================
// Spectra — Standalone host for PluginAudioProcessor/PluginEditor
//
// SPDX-License-Identifier: MIT

#pragma once
#include <JuceHeader.h>

class PluginAudioProcessor;
class PluginEditor;

class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::Button::Listener,
                            private juce::ChangeListener,
                            private juce::KeyListener
{
public:
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void resized() override;

private:
    // Callbacks
    void buttonClicked(juce::Button* b) override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    bool keyPressed(const juce::KeyPress&, juce::Component*) override;
    void timerCallback() override;

    // Helpers
    void openSettingsDialog();
    void resetAudioToDefault();
    void toggleAllMidiInputs();
    void saveAudioState();
    void saveMidiEnabledList();
    void restoreOrEnableAllMidiInputs();
    void reapplyMidiCallbacks();
    void updateStatus();

    static int commandBarHeight();

    // Audio
    juce::AudioDeviceManager              deviceManager;
    juce::AudioProcessorPlayer            player;
    std::unique_ptr<PluginAudioProcessor> processor;

    // UI
    std::unique_ptr<PluginEditor>         editor;
    std::unique_ptr<juce::PropertiesFile> props;
    juce::TextButton                      btnSettings, btnReset, btnToggleMidi;
    juce::Label                           status;
    std::unique_ptr<juce::DialogWindow>   settingsDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
