//============================== Main.cpp ====================================
// Spectra — Standalone Host (JUCE)
// - DocumentWindow native
// - Restauration/sauvegarde des bounds via PropertiesFile
// - Charge MainComponent (qui héberge PluginProcessor/PluginEditor)
// SPDX-License-Identifier: MIT

#include <JuceHeader.h>
#include <optional>
#include "MainComponent.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
constexpr const char* kAppName    = "Spectra Host";
constexpr const char* kAppVersion = "0.1.1";

std::unique_ptr<juce::PropertiesFile> makeProps()
{
    juce::PropertiesFile::Options o;
    o.applicationName      = kAppName;
    o.filenameSuffix       = ".settings";
    o.folderName           = kAppName;
    o.osxLibrarySubFolder  = "Application Support";
    o.ignoreCaseOfKeyNames = true;
    o.doNotSave            = false;
    return std::make_unique<juce::PropertiesFile>(o);
}

std::optional<juce::Rectangle<int>> readWindowBounds (juce::PropertiesFile& props)
{
    if (auto xml = props.getXmlValue ("windowBounds"))
    {
        if (xml->hasTagName ("B"))
        {
            return juce::Rectangle<int> {
                xml->getIntAttribute ("x", 100),
                xml->getIntAttribute ("y", 100),
                xml->getIntAttribute ("w", 960),
                xml->getIntAttribute ("h", 640)
            };
        }
    }
    return std::nullopt;
}

void writeWindowBounds (juce::PropertiesFile& props, const juce::Rectangle<int>& r)
{
    juce::XmlElement b ("B");
    b.setAttribute ("x", r.getX());
    b.setAttribute ("y", r.getY());
    b.setAttribute ("w", r.getWidth());
    b.setAttribute ("h", r.getHeight());
    props.setValue ("windowBounds", &b);
    props.saveIfNeeded();
}
} // namespace

//============================== MainWindow ==================================
class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow (juce::PropertiesFile& props)
        : DocumentWindow (kAppName,
                          juce::Desktop::getInstance().getDefaultLookAndFeel()
                              .findColour (juce::ResizableWindow::backgroundColourId),
                          juce::DocumentWindow::allButtons),
          properties (props)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, true);

        // Pas de helper global -> pas de conflit
        setContentOwned (new MainComponent(), true);
        setResizeLimits (360, 260, 4096, 2160);

        if (auto saved = readWindowBounds (properties))
            setBounds (*saved);
        else
            centreWithSize (960, 640);

       #if JUCE_WINDOWS && defined(BinaryData_juce_icon_png) && defined(BinaryData_juce_icon_pngSize)
        setIcon (juce::ImageCache::getFromMemory (BinaryData::juce_icon_png,
                                                  BinaryData::juce_icon_pngSize));
       #endif

        setVisible (true);
    }

    ~MainWindow() override
    {
        saveBoundsToProps();
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        saveBoundsToProps();
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    void saveBoundsToProps() { writeWindowBounds (properties, getBounds()); }

    juce::PropertiesFile& properties;
};

//============================== Application =================================
class SpectraHostApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return kAppName; }
    const juce::String getApplicationVersion() override { return kAppVersion; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        properties = makeProps();
        mainWindow = std::make_unique<MainWindow> (*properties);
    }

    void shutdown() override
    {
        mainWindow.reset();
        properties.reset();
    }

    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (mainWindow != nullptr)
            mainWindow->toFront (true);
    }

private:
    std::unique_ptr<juce::PropertiesFile> properties;
    std::unique_ptr<MainWindow>           mainWindow;
};

// Point d’entrée
START_JUCE_APPLICATION (SpectraHostApplication)
