#include "MainComponent.h"
#include "AppIconData.h"
#include <juce_gui_extra/juce_gui_extra.h>
#if JUCE_WINDOWS
#include <windows.h>
#endif
class TimecodeCueBridgeApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Timecode Cue Bridge"; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise(const juce::String& args) override {
#if JUCE_WINDOWS
        runningMutex = CreateMutexW(nullptr, FALSE, L"Local\\TimecodeCueBridge.Running");
        if (!runningMutex || GetLastError() == ERROR_ALREADY_EXISTS) { quit(); return; }
#endif
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr) {
            setApplicationReturnValue(2); quit(); return;
        }
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
        if (args.contains("--smoke-test")) juce::Timer::callAfterDelay(1500, [] { juce::JUCEApplication::getInstance()->systemRequestedQuit(); });
    }
    void shutdown() override {
        mainWindow.reset();
#if JUCE_WINDOWS
        if (runningMutex) { CloseHandle(runningMutex); runningMutex = nullptr; }
#endif
    }
    void systemRequestedQuit() override { quit(); }
    class MainWindow final : public juce::DocumentWindow {
    public:
        explicit MainWindow(const juce::String& name)
            : juce::DocumentWindow(name, juce::Colours::black, juce::DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setIcon(juce::ImageCache::getFromMemory(TcbAssets::app_png,TcbAssets::app_pngSize));
            setContentOwned(new MainComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setResizable(true, false);
            setResizeLimits(1040, 790, 2000, 1400);
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
private:
    std::unique_ptr<MainWindow> mainWindow;
#if JUCE_WINDOWS
    HANDLE runningMutex = nullptr;
#endif
};
START_JUCE_APPLICATION(TimecodeCueBridgeApplication)
