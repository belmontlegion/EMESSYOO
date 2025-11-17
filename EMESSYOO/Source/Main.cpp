#include <JuceHeader.h>
#include "MainComponent.h"

//==============================================================================
class MSU1PrepStudioApplication : public juce::JUCEApplication
{
public:
    //==============================================================================
    MSU1PrepStudioApplication() {}

    const juce::String getApplicationName() override { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override { return true; }

    //==============================================================================
    void initialise(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);

        // Create main window
        mainWindow.reset(new MainWindow(getApplicationName()));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        juce::ignoreUnused(commandLine);

        // Handle multiple instances if needed
    }

    //==============================================================================
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                juce::Colours::darkgrey,
                DocumentWindow::allButtons)
        {
                setUsingNativeTitleBar(true);
                auto* content = new MainComponent();
                setContentOwned(content, true);

#if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
#else
            setResizable(true, true);
                setResizeLimits(content->getMinimumWindowWidth(),
                        content->getMinimumWindowHeight(),
                        8192,
                        8192);
                setSize(content->getPreferredWindowWidth(),
                    content->getPreferredWindowHeight());
                centreWithSize(getWidth(), getHeight());
#endif

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
START_JUCE_APPLICATION(MSU1PrepStudioApplication)
