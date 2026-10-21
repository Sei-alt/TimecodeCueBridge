#pragma once
#include "LtcCapture.h"
#include "AppStyle.h"
#include "CueEngine.h"
#include "OscOutput.h"
#include "MidiOutput.h"
#include "SceneEditor.h"
#include "SceneSettings.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <mutex>
class MainComponent final : public juce::Component, private juce::Timer, private juce::ChangeListener {
public:
    explicit MainComponent(bool enableAudio = true);
    ~MainComponent() override;
    void resized() override;
    void paint(juce::Graphics&) override;
private:
    struct SceneViewport final : juce::Viewport {
        void resized() override {
            juce::Viewport::resized();
            if (auto* content = getViewedComponent())
                content->setSize(std::max(1, getWidth() - getScrollBarThickness()), std::max(568, getHeight()));
        }
    };
    struct SettingsPage final : juce::Component {
        std::function<void()> layout;
        void resized() override {if(layout)layout();}
    };
    void layoutSettings();
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void decodedFrame(int);
    bool applyRuntime(const tcb::SceneSettings&,juce::String& error);
    void saveAndApply();
    void loadSettings();
    void startAudioInput();
    juce::String savedAudioXml;
    bool audioStarted = false;
    void disarm(const juce::String&,bool dirty);
    void sendCue(const tcb::Cue&,bool manual);
    void setStatus(const juce::String&);
    void refreshMidiDevices(bool userAction);
    juce::String selectedMidiIdentifier() const;
    juce::String selectedMidiName() const;
    juce::File getConfigFile() const;
    juce::File getLogFile() const;
    ui::LookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips {this,800};
    std::mutex runtimeMutex;
    tcb::CueEngine cueEngine;
    tcb::OscOutput osc;
    tcb::MidiOutputService midi;
    std::atomic<int> fps {30};
    bool armed=false,applied=false,suppressChanges=false,backupOnSave=false,inputLocked=false;
    std::mutex statusMutex;
    juce::String lastStatus {"DISARMED"};
    std::unique_ptr<juce::FileLogger> logger;
    LtcCapture capture;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSelector;
    juce::TabbedComponent tabs {juce::TabbedButtonBar::TabsAtTop};
    SceneEditor editor;
    SceneViewport sceneViewport;
    SettingsPage settingsPage;
    juce::Array<juce::MidiDeviceInfo> midiDevices;
    juce::String wantedMidiId,wantedMidiName;
    juce::Label title,version,timecode,status,inputStatus,audioLabel,hostLabel,portLabel,fpsLabel,midiLabel;
    juce::TextEditor host,port;
    juce::ComboBox fpsChoice,midiChoice;
    juce::TextButton saveButton {"Save & Apply"},armButton {"Arm"},testButton {"Test Scene"},resetButton {"Reset All"},panicButton {"MIDI Panic"},folderButton {"Data Folder"},refreshMidiButton {"Refresh"};
};
