//================================= Main.cpp ==================================
#include <JuceHeader.h>
#include <cmath>
#include "MainComponent.h"

class SpectraApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName()    override { return "SpectraStandalone"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed()          override { return true; }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override            { mainWindow = nullptr; }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Colours::black,
                              DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar (true);
            setColour (backgroundColourId, juce::Colour::fromRGB (14,14,16));

            setContentOwned (new MainComponent(), true);

            // Limites de redimensionnement basées sur MainComponent
            const int minW = (int) std::round (MainComponent::kWindowW * MainComponent::kMinScale);
            const int minH = (int) std::round (MainComponent::kWindowH * MainComponent::kMinScale);
            const int maxW = (int) std::round (MainComponent::kWindowW * MainComponent::kMaxScale);
            const int maxH = (int) std::round (MainComponent::kWindowH * MainComponent::kMaxScale);

            setResizable (true, true);
            setResizeLimits (minW, minH, maxW, maxH);

            // Taille initiale sûre à l’écran
            if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            {
                const auto area = d->userArea;
                const int w = juce::jlimit (minW, MainComponent::kWindowW, juce::jmin (maxW, area.getWidth()  - 80));
                const int h = juce::jlimit (minH, MainComponent::kWindowH, juce::jmin (maxH, area.getHeight() - 80));
                setBounds (area.getCentreX() - w/2, area.getCentreY() - h/2, w, h);
            }
            else
            {
                centreWithSize (MainComponent::kWindowW, MainComponent::kWindowH);
            }

            setVisible (true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (SpectraApp)
