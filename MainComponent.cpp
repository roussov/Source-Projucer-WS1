//============================== MainComponent.cpp ===========================
// Spectra — Standalone host for PluginAudioProcessor/PluginEditor
// SPDX-License-Identifier: MIT

#include "MainComponent.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <JuceHeader.h>

namespace {
constexpr const char* kAppVendor   = "SpectraAudio";
constexpr const char* kAppName     = "SpectraStandalone";
constexpr const char* kKeyAudioXML = "audioDeviceXML";
constexpr const char* kKeyMidiList = "midiEnabled"; // CSV by device identifier

static std::unique_ptr<juce::PropertiesFile> createLocalProps()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = kAppName;
    o.filenameSuffix      = ".properties";
    o.osxLibrarySubFolder = "Application Support/" + juce::String(kAppVendor);
    o.commonToAllUsers    = false;
    o.storageFormat       = juce::PropertiesFile::storeAsXML;
    o.millisecondsBeforeSaving = 200;

    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile(kAppVendor).getChildFile(kAppName);
    dir.createDirectory();
    o.folderName = dir.getFullPathName();
    return std::make_unique<juce::PropertiesFile>(o);
}
} // namespace

//==============================================================================
// Construction / Destruction
//==============================================================================
MainComponent::MainComponent()
{
    props = createLocalProps();

    // Restaure AudioDeviceManager
    std::unique_ptr<juce::XmlElement> saved;
    if (auto s = props->getValue(kKeyAudioXML); s.isNotEmpty())
        saved = juce::XmlDocument::parse(s);

   #if JUCE_MAJOR_VERSION >= 6
    deviceManager.initialise(2, 2, saved.get(), true, juce::String(), nullptr);
   #else
    deviceManager.initialise(2, 2, saved.get(), true, juce::String(), nullptr);
   #endif

    deviceManager.addChangeListener(this);

    // Branche le processor + audio callback
    processor = std::make_unique<PluginAudioProcessor>();
    player.setProcessor(processor.get());
    deviceManager.addAudioCallback(&player);

    // MIDI
    restoreOrEnableAllMidiInputs();

    // UI
    editor = std::make_unique<PluginEditor>(*processor);
    addAndMakeVisible(editor.get());

    btnSettings.setButtonText("Audio/MIDI…");
    btnSettings.setTooltip("Ouvrir les réglages Audio/MIDI (⌘,)");
    btnSettings.addListener(this);
    addAndMakeVisible(btnSettings);

    btnReset.setButtonText("Reset Audio");
    btnReset.setTooltip("Réinitialiser la configuration audio (⌘R)");
    btnReset.addListener(this);
    addAndMakeVisible(btnReset);

    btnToggleMidi.setButtonText("MIDI All On/Off");
    btnToggleMidi.setTooltip("Activer/Désactiver tous les MIDI IN (⌘M)");
    btnToggleMidi.addListener(this);
    addAndMakeVisible(btnToggleMidi);

    status.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(status);

    const int w = juce::jmax(480, editor->getWidth());
    const int h = juce::jmax(320, editor->getHeight()) + commandBarHeight();
    setSize(w, h);

    addKeyListener(this);
    startTimerHz(15);
    updateStatus();
}

MainComponent::~MainComponent()
{
    stopTimer();
    saveAudioState();
    saveMidiEnabledList();

    deviceManager.removeChangeListener(this);

    for (auto& d : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback(d.identifier, &player.getMidiMessageCollector());

    deviceManager.removeAudioCallback(&player);
    player.setProcessor(nullptr);

    if (settingsDialog) settingsDialog->setVisible(false);
    settingsDialog.reset();
    editor.reset();
    processor.reset();
    props.reset();
}

//==============================================================================
// Layout
//==============================================================================
void MainComponent::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop(commandBarHeight()).reduced(8, 6);

    btnSettings.setBounds(top.removeFromLeft(180));
    btnReset.setBounds(top.removeFromLeft(140));
    btnToggleMidi.setBounds(top.removeFromLeft(160));
    status.setBounds(top.reduced(8, 0));

    if (editor) editor->setBounds(r);
}

//==============================================================================
// Callbacks UI / système
//==============================================================================
void MainComponent::buttonClicked(juce::Button* b)
{
    if (b == &btnSettings) openSettingsDialog();
    else if (b == &btnReset) resetAudioToDefault();
    else if (b == &btnToggleMidi) toggleAllMidiInputs();
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*)
{
    updateStatus();
    saveAudioState();
    reapplyMidiCallbacks();
}

bool MainComponent::keyPressed(const juce::KeyPress& k, juce::Component*)
{
   #if JUCE_MAC
    const bool cmd = k.getModifiers().isCommandDown();
   #else
    const bool cmd = k.getModifiers().isCtrlDown();
   #endif

    if (cmd && k.getKeyCode() == ',') { openSettingsDialog(); return true; }
    if (cmd && (k.getTextCharacter() == 'r' || k.getKeyCode() == juce::KeyPress::F5Key))
    { resetAudioToDefault(); return true; }
    if (cmd && (k.getTextCharacter() == 'm'))
    { toggleAllMidiInputs(); return true; }

    return false;
}

void MainComponent::timerCallback()
{
    if (editor)
    {
        const int ew = editor->getWidth();
        const int eh = editor->getHeight() + commandBarHeight();
        if (ew != getWidth() || eh != getHeight())
            setSize(juce::jmax(480, ew), juce::jmax(320, eh));
    }
    updateStatus();
}

//==============================================================================
// Helpers
//==============================================================================
int MainComponent::commandBarHeight() { return 44; }

void MainComponent::openSettingsDialog()
{
    if (settingsDialog) { settingsDialog->toFront(true); return; }

    auto* selector = new juce::AudioDeviceSelectorComponent(
        deviceManager, 0, 2, 0, 2,
        /*showMidiInputs*/ true,
        /*showChannelsAsStereoPairs*/ true,
        /*hideAdvancedOptions*/ false,
        /*showMidiOutputSelector*/ true
    );
    selector->setSize(520, 420);

    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle                   = "Réglages Audio/MIDI";
    o.content.setOwned(selector);
    o.componentToCentreAround       = this;
    o.dialogBackgroundColour        = juce::Colours::black.withAlpha(0.85f);
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = true;
    o.useBottomRightCornerResizer   = true;

    settingsDialog.reset(o.launchAsync());
}

void MainComponent::resetAudioToDefault()
{
    deviceManager.closeAudioDevice();
   #if JUCE_MAJOR_VERSION >= 6
    deviceManager.initialise(2, 2, nullptr, true, juce::String(), nullptr);
   #else
    deviceManager.initialise(2, 2, nullptr, true, juce::String(), nullptr);
   #endif
    reapplyMidiCallbacks();
    updateStatus();
}

void MainComponent::saveAudioState()
{
    if (!props) return;
    if (auto xml = deviceManager.createStateXml())
    {
        props->setValue(kKeyAudioXML, xml->toString());
        props->saveIfNeeded();
    }
}

void MainComponent::saveMidiEnabledList()
{
    if (!props) return;
    juce::StringArray enabled;
    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (deviceManager.isMidiInputDeviceEnabled(d.identifier))
            enabled.add(d.identifier);
    props->setValue(kKeyMidiList, enabled.joinIntoString(","));
    props->saveIfNeeded();
}

void MainComponent::restoreOrEnableAllMidiInputs()
{
    auto csv = props ? props->getValue(kKeyMidiList) : juce::String();
    auto all = juce::MidiInput::getAvailableDevices();

    if (csv.isNotEmpty())
    {
        juce::StringArray want; want.addTokens(csv, ",", "\"");
        for (auto& d : all)
        {
            const bool en = want.contains(d.identifier, true);
            deviceManager.setMidiInputDeviceEnabled(d.identifier, en);
            deviceManager.removeMidiInputDeviceCallback(d.identifier, &player.getMidiMessageCollector());
            if (en)
                deviceManager.addMidiInputDeviceCallback(d.identifier, &player.getMidiMessageCollector());
        }
    }
    else
    {
        for (auto& d : all)
        {
            deviceManager.setMidiInputDeviceEnabled(d.identifier, true);
            deviceManager.addMidiInputDeviceCallback(d.identifier, &player.getMidiMessageCollector());
        }
        saveMidiEnabledList();
    }
}

void MainComponent::reapplyMidiCallbacks()
{
    for (auto& d : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback(d.identifier, &player.getMidiMessageCollector());

    for (auto& d : juce::MidiInput::getAvailableDevices())
        if (deviceManager.isMidiInputDeviceEnabled(d.identifier))
            deviceManager.addMidiInputDeviceCallback(d.identifier, &player.getMidiMessageCollector());
}

void MainComponent::toggleAllMidiInputs()
{
    auto devices = juce::MidiInput::getAvailableDevices();
    bool anyDisabled = false;
    for (auto& d : devices)
        if (!deviceManager.isMidiInputDeviceEnabled(d.identifier)) { anyDisabled = true; break; }

    for (auto& d : devices)
    {
        deviceManager.setMidiInputDeviceEnabled(d.identifier, anyDisabled);
        deviceManager.removeMidiInputDeviceCallback(d.identifier, &player.getMidiMessageCollector());
        if (anyDisabled)
            deviceManager.addMidiInputDeviceCallback(d.identifier, &player.getMidiMessageCollector());
    }
    saveMidiEnabledList();
    updateStatus();
}

void MainComponent::updateStatus()
{
    juce::String s;
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        const auto nm  = dev->getName();
        const auto sr  = dev->getCurrentSampleRate();
        const auto bs  = dev->getCurrentBufferSizeSamples();
        const auto inC = dev->getActiveInputChannels().countNumberOfSetBits();
        const auto ouC = dev->getActiveOutputChannels().countNumberOfSetBits();

        int midiOn = 0;
        for (auto& d : juce::MidiInput::getAvailableDevices())
            if (deviceManager.isMidiInputDeviceEnabled(d.identifier)) ++midiOn;

        s << "Device: " << nm
          << "   SR: " << juce::String(sr, 0) << " Hz"
          << "   Buffer: " << bs << " samples"
          << "   I/O: " << inC << "/" << ouC
          << "   MIDI IN: " << midiOn;
    }
    else s = "Aucun périphérique audio actif";

    status.setText(s, juce::dontSendNotification);
}
