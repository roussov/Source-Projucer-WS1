//============================== MainComponent.cpp ===========================
// Standalone host pro pour PluginAudioProcessor/PluginEditor
// SPDX-License-Identifier: MIT

#include "MainComponent.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <JuceHeader.h>

namespace {
#if JUCE_MAC
constexpr bool kIsMac = true;
#else
constexpr bool kIsMac = false;
#endif

inline bool isAccelDown (const juce::KeyPress& key) noexcept
{
    return kIsMac ? key.getModifiers().isCommandDown()
                  : key.getModifiers().isCtrlDown();
}

juce::File audioStateFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RoussovHost");
    dir.createDirectory();
    return dir.getChildFile ("audio_state.xml");
}
} // namespace

//============================== MainComponent ===============================
MainComponent::MainComponent()
{
    // Processor + player
    processor = std::make_unique<PluginAudioProcessor>();
    player.setProcessor (processor.get());

    // Audio init (2 in / 2 out). Charge l’état s’il existe.
    {
        juce::XmlDocument doc (audioStateFile());
        std::unique_ptr<juce::XmlElement> state = doc.getDocumentElement();
        if (state && state->hasTagName ("AUDIOSC"))
            deviceManager.initialise (2, 2, state.get(), true);
        else
            deviceManager.initialise (2, 2, nullptr, true);
    }

    deviceManager.addAudioCallback (&player);
    deviceManager.addChangeListener (this);

    // Editor
    editor.reset (processor->createEditor());
    jassert (editor != nullptr);
    addAndMakeVisible (editor.get());

    // UI: barre de statut
    addAndMakeVisible (status);
    status.setJustificationType (juce::Justification::centredLeft);
    status.setFont (juce::Font (juce::FontOptions (12.0f)));

    // Taille initiale
    const int pad = 16;
    setSize (juce::jlimit (360, 1600, editor->getWidth()  + pad * 2),
             juce::jlimit (260, 1000, editor->getHeight() + pad * 2 + 28));

    addKeyListener (this);
    setWantsKeyboardFocus (true);
    startTimerHz (4); // status/CPU poll
    refreshStatus();
}

MainComponent::~MainComponent()
{
    stopTimer();
    removeKeyListener (this);
    deviceManager.removeChangeListener (this);

    // Sauvegarde de l’état audio
    if (auto xml = deviceManager.createStateXml())
        xml->writeTo (audioStateFile());

    editor = nullptr;                 // détruire l’editor avant le processor
    deviceManager.removeAudioCallback (&player);
    player.setProcessor (nullptr);
    processor = nullptr;
}

//================== Component ==================
void MainComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient(
        juce::Colour::fromFloatRGBA (0.06f, 0.06f, 0.07f, 1.0f), b.getX(), b.getY(),
        juce::Colour::fromFloatRGBA (0.03f, 0.03f, 0.04f, 1.0f), b.getX(), b.getBottom(), false));
    g.fillRect (b);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    auto sb = r.removeFromBottom (24);
    status.setBounds (sb.reduced (8, 2));

    if (editor != nullptr)
    {
        auto avail = r.reduced (12);
        auto edW = editor->getWidth();
        auto edH = editor->getHeight();

        edW = juce::jmin (edW, avail.getWidth());
        edH = juce::jmin (edH, avail.getHeight());

        juce::Rectangle<int> ed (0, 0, edW, edH);
        ed.setCentre (avail.getCentre());
        editor->setBounds (ed);
    }
}

//================== Raccourcis =================
bool MainComponent::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    // Réglages audio (⌘, ou Ctrl+,)
    if (isAccelDown (key) && key.getTextCharacter() == ',')
    { openAudioSettings(); return true; }

    // Reset audio (⌘R / Ctrl+R ou F5)
    if (isAccelDown (key) && (key.getTextCharacter() == 'r' || key.getKeyCode() == juce::KeyPress::F5Key))
    { resetAudio(); return true; }

    // Toggle bypass (⌘B / Ctrl+B)
    if (isAccelDown (key) && (key.getTextCharacter() == 'b'))
    { toggleBypass(); return true; }

    // Gain = 0.5 (⌘0 / Ctrl+0)
    if (isAccelDown (key) && (key.getTextCharacter() == '0'))
    { setGain (0.5f); return true; }

    // Gain +0.02  (gère '+' ou '=' selon layout)
    const juce::juce_wchar c = key.getTextCharacter();
    if (c == '+' || c == '=')
    { nudgeGain (+0.02f); return true; }

    // Gain -0.02
    if (c == '-')
    { nudgeGain (-0.02f); return true; }

    // À propos (⌘I / Ctrl+I)
    if (isAccelDown (key) && (c == 'i'))
    { showAbout(); return true; }

    return false;
}

//================== Timer/Change =================
void MainComponent::timerCallback() { refreshStatus(); }
void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*) { refreshStatus(); }

//================== Ops ==========================
void MainComponent::openAudioSettings()
{
    auto* comp = new juce::AudioDeviceSelectorComponent (
        deviceManager,
        0, 256,   // min/max entrées
        0, 256,   // min/max sorties
        false, false, true, false);

    comp->setSize (560, 440);

    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (comp);
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.dialogTitle = "Réglages audio";
    o.componentToCentreAround = this;

    // API moderne: non bloquant
    o.launchAsync();

    // Sauvegarde immédiate (si l’utilisateur a modifié)
    if (auto xml = deviceManager.createStateXml())
        xml->writeTo (audioStateFile());

    refreshStatus();
}

void MainComponent::resetAudio()
{
    deviceManager.closeAudioDevice();
    if (auto* devType = deviceManager.getCurrentDeviceTypeObject())
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup (setup);
        deviceManager.setAudioDeviceSetup (setup, true);
    }
    deviceManager.restartLastAudioDevice();
    refreshStatus();
}

void MainComponent::toggleBypass()
{
    auto& vts = processor->getValueTreeState();
    if (auto* p = dynamic_cast<juce::AudioParameterBool*>(vts.getParameter ("bypass")))
        *p = ! *p;
}

void MainComponent::setGain (float g)
{
    auto& vts = processor->getValueTreeState();
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(vts.getParameter ("gain")))
        *p = juce::jlimit (0.0f, 1.0f, g);
}

void MainComponent::nudgeGain (float delta)
{
    auto& vts = processor->getValueTreeState();
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(vts.getParameter ("gain")))
        *p = juce::jlimit (0.0f, 1.0f, (float)*p + delta);
}

void MainComponent::refreshStatus()
{
    juce::String txt;

    if (auto* d = deviceManager.getCurrentAudioDevice())
    {
        const auto sr = d->getCurrentSampleRate();
        const auto bs = d->getCurrentBufferSizeSamples();
        const auto in = d->getActiveInputChannels().countNumberOfSetBits();
        const auto ou = d->getActiveOutputChannels().countNumberOfSetBits();
        const auto xr = d->getXRunCount();

        txt << "Device: " << d->getName()
            << "   " << (int)sr << " Hz"
            << "   " << bs << " smp"
            << "   I/O " << in << "/" << ou
            << "   CPU " << juce::String (deviceManager.getCpuUsage() * 100.0, 1) << "%";
        if (xr >= 0) txt << "   XRuns " << xr;
    }
    else
    {
        txt = "Aucun périphérique audio";
    }

    status.setText (txt, juce::dontSendNotification);
    // Clamp l’editor si la fenêtre change (DPI, etc.)
    resized();
}

void MainComponent::showAbout()
{
    juce::AlertWindow::showMessageBoxAsync (
        juce::AlertWindow::InfoIcon,
        "À propos",
        "Roussov Standalone Host\n"
        "Plugin: Roussov (gain, bypass)\n"
        "Raccourcis: ⌘, | ⌘R | ⌘B | ⌘0 | +/-",
        "OK");
}
