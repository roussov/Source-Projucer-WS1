//============================== Main.cpp ======================================
// Spectra — Standalone entry (gainIn / gainOut ready)
//
// SPDX-License-Identifier: MIT

#include <JuceHeader.h>
#include "MainComponent.h"

class SpectraApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName()    override { return "SpectraStandalone"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed()          override { return true; }

    //==============================================================================
    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    //==============================================================================
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : juce::DocumentWindow (
                name,
                juce::Desktop::getInstance().getDefaultLookAndFeel()
                    .findColour (juce::ResizableWindow::backgroundColourId),
                juce::DocumentWindow::allButtons)
        {
           #if JUCE_IOS || JUCE_ANDROID
            setUsingNativeTitleBar (false);
           #else
            setUsingNativeTitleBar (true);
           #endif

            setResizable (true, true);
            setResizeLimits (480, 320, 1920, 1200);

            // Attache MainComponent
            setContentOwned (new MainComponent(), true);

            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION (SpectraApp)
