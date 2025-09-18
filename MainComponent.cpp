//============================== MainComponent.cpp ===========================
// Spectra — Standalone host for PluginAudioProcessor/PluginEditor
// Cible: JUCE "GUI Application" (ou target Standalone)
// - AudioDeviceManager avec persistance
// - MIDI IN auto + sélection
// - Panneau de réglages Audio/MIDI (dialog modal)
// - Raccourcis: ⌘,  ouvre réglages ; ⌘R réinit audio ; ⌘M toggle MIDI all
// - Barre de statut: device, SR, buffer, ch, midi-in count
//
// SPDX-License-Identifier: MIT

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
constexpr const char* kAppVendor   = "SpectraAudio";
constexpr const char* kAppName     = "SpectraStandalone";
constexpr const char* kKeyAudioXML = "audioDeviceXML";
constexpr const char* kKeyMidiList = "midiEnabled"; // CSV by device identifier
}

//==============================================================================
// Petit utilitaire PropertiesFile autonome (si l'app ne configure pas ApplicationProperties)
static std::unique_ptr<juce::PropertiesFile> createLocalProps()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = kAppName;
    o.filenameSuffix      = ".properties";
    o.osxLibrarySubFolder = "Application Support/" + juce::String(kAppVendor);
    o.commonToAllUsers    = false;
    o.storageFormat       = juce::PropertiesFile::storeAsXML;

    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile(kAppVendor).getChildFile(kAppName);
    dir.createDirectory();
    o.millisecondsBeforeSaving = 200;
    o.folderName = dir.getFullPathName();
    return std::make_unique<juce::PropertiesFile>(o);
}

//==============================================================================

class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private juce::Button::Listener,
                            private juce::ChangeListener,
                            private juce::KeyListener
{
public:
    MainComponent()
    {
        props = createLocalProps();

        // Restaure état AudioDeviceManager
        juce::XmlElement* saved = nullptr;
        {
            auto s = props->getValue(kKeyAudioXML);
            if (s.isNotEmpty())
                saved = juce::XmlDocument::parse(s).release();
        }

        const juce::XmlElement* xmlState = saved ? saved : nullptr;
        #if JUCE_MAJOR_VERSION >= 6
        deviceManager.initialise(2, 2, (const juce::XmlElement*) saved, true, juce::String(), nullptr);
#else
        deviceManager.initialise(2, 2, saved, true, juce::String(), nullptr);
#endif
        delete saved;
        deviceManager.addChangeListener(this);

        // Branche le processor
        processor = std::make_unique<PluginAudioProcessor>();
        player.setProcessor(processor.get());
        deviceManager.addAudioCallback(&player);

        // MIDI: restaure liste ou active tout par défaut
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

        // Layout initial: taille = éditeur + barre de commandes
        const int w = juce::jmax(480, editor->getWidth());
        const int h = juce::jmax(320, editor->getHeight()) + commandBarHeight();
        setSize(w, h);

        addKeyListener(this);
        startTimerHz(15); // statut live + suivi redimensionnement d'éditeur
        updateStatus();
    }

    ~MainComponent() override
    {
        stopTimer();
        saveAudioState();        // audio xml
        saveMidiEnabledList();   // midi csv

        // Débranchement callbacks avant destruction
        deviceManager.removeChangeListener(this);

        // MIDI off
        for (auto& dev : juce::MidiInput::getAvailableDevices())
            deviceManager.removeMidiInputDeviceCallback(dev.identifier, &player.getMidiMessageCollector());

        deviceManager.removeAudioCallback(&player);
        player.setProcessor(nullptr);

        // UI
        if (settingsDialog) settingsDialog->setVisible(false);
        settingsDialog.reset();
        editor.reset();
        processor.reset();
        props.reset();
    }

    //==============================================================================
    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(commandBarHeight()).reduced(8, 6);

        auto left = top.removeFromLeft(180);
        btnSettings.setBounds(left);

        left = top.removeFromLeft(140);
        btnReset.setBounds(left);

        left = top.removeFromLeft(160);
        btnToggleMidi.setBounds(left);

        status.setBounds(top.reduced(8, 0));

        // Le reste pour l'éditeur
        if (editor) editor->setBounds(r);
    }

private:
    //==============================================================================
    // UI helpers
    static int commandBarHeight() { return 44; }

    void buttonClicked(juce::Button* b) override
    {
        if (b == &btnSettings) openSettingsDialog();
        else if (b == &btnReset) resetAudioToDefault();
        else if (b == &btnToggleMidi) toggleAllMidiInputs();
    }

    // Dialog de réglages Audio/MIDI
    void openSettingsDialog()
    {
        if (settingsDialog != nullptr)
        {
            settingsDialog->toFront(true);
            return;
        }

        auto* selector = new juce::AudioDeviceSelectorComponent(
            deviceManager,
            /*minInputs*/ 0, /*maxInputs*/ 2,
            /*minOutputs*/ 0, /*maxOutputs*/ 2,
            /*showMidiInputs*/  true,
            /*showChannelsAsStereoPairs*/ true,
            /*hideAdvancedOptions*/       false,
            /*showMidiOutputSelector*/    true
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

    // Reset audio: device par défaut + SR/Buffer par défaut
    void resetAudioToDefault()
    {
        deviceManager.closeAudioDevice();
        deviceManager.initialise(2, 2, nullptr, true);
        // Rebranche MIDI selon politique actuelle (on conserve l’état courant)
        reapplyMidiCallbacks();
        updateStatus();
    }

    // Clavier: ⌘,  ⌘R  ⌘M
    bool keyPressed(const juce::KeyPress& k, juce::Component*) override
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

    // Suivi device changes → statut + persistance immédiate
    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        updateStatus();
        saveAudioState();
        // Recréé callbacks MIDI si l’OS a renommé/rouvert les devices
        reapplyMidiCallbacks();
    }

    // Timer: statut live + resize si l'éditeur change sa taille interne
    void timerCallback() override
    {
        if (editor != nullptr)
        {
            const int ew = editor->getWidth();
            const int eh = editor->getHeight() + commandBarHeight();
            if (ew != getWidth() || eh != getHeight())
                setSize(juce::jmax(480, ew), juce::jmax(320, eh));
        }
        updateStatus();
    }

    //==============================================================================
    // Persistance Audio
    void saveAudioState()
    {
        if (!props) return;
        if (auto* dev = deviceManager.getCurrentAudioDevice())
        {
            if (auto xml = deviceManager.createStateXml())
            {
                props->setValue(kKeyAudioXML, xml->toString());
                props->saveIfNeeded();
            }
        }
    }

    // Persistance MIDI: CSV d’identifiers activés
    void saveMidiEnabledList()
    {
        if (!props) return;
        juce::StringArray enabled;
        for (auto& d : juce::MidiInput::getAvailableDevices())
            if (deviceManager.isMidiInputDeviceEnabled(d.identifier))
                enabled.add(d.identifier);
        props->setValue(kKeyMidiList, enabled.joinIntoString(","));
        props->saveIfNeeded();
    }

    void restoreOrEnableAllMidiInputs()
    {
        // Si CSV présent, on l’applique. Sinon, on active tout.
        auto csv = props->getValue(kKeyMidiList);
        auto all = juce::MidiInput::getAvailableDevices();

        if (csv.isNotEmpty())
        {
            juce::StringArray want;
            want.addTokens(csv, ",", "\"");
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

    void reapplyMidiCallbacks()
    {
        // Nettoie puis réapplique selon état enabled courant
        for (auto& d : juce::MidiInput::getAvailableDevices())
            deviceManager.removeMidiInputDeviceCallback(d.identifier, &player.getMidiMessageCollector());

        for (auto& d : juce::MidiInput::getAvailableDevices())
            if (deviceManager.isMidiInputDeviceEnabled(d.identifier))
                deviceManager.addMidiInputDeviceCallback(d.identifier, &player.getMidiMessageCollector());
    }

    void toggleAllMidiInputs()
    {
        auto devices = juce::MidiInput::getAvailableDevices();
        // Si au moins un désactivé → activer tout. Sinon → tout couper.
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

    //==============================================================================
    void updateStatus()
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

    //==============================================================================
    juce::AudioDeviceManager                    deviceManager;
    juce::AudioProcessorPlayer                  player;
    std::unique_ptr<PluginAudioProcessor>       processor;
    std::unique_ptr<PluginEditor>               editor;

    std::unique_ptr<juce::PropertiesFile>       props;

    // UI
    juce::TextButton btnSettings, btnReset, btnToggleMidi;
    juce::Label      status;
    std::unique_ptr<juce::DialogWindow> settingsDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

