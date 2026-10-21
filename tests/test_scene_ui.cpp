#include "SceneEditor.h"
#include <iostream>
// Actual message-thread controls, without audio devices, networking or user settings.
class SceneTestApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override{return "TCB Scene UI Tests";}
    const juce::String getApplicationVersion() override{return "0.5.1";}
    void initialise(const juce::String& args) override {
        if(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()==nullptr){setApplicationReturnValue(2);quit();return;}
        juce::Timer::callAfterDelay(50,[this,args]{
            ui::LookAndFeel look;SceneEditor editor;editor.setLookAndFeel(&look);editor.setSize(1068,588);
            int edits=0;editor.onChange=[&]{++edits;};
            const bool passed=editor.runControlTests() && edits>=10;
            std::cout<<(passed?"PASS":"FAIL")<<": actual + / - / duplicate / restore / name / numeric typing / invalid text / Escape / vertical drag / Shift drag / limits / FPS clamp / MIDI fields / fired display / row Rearm (unselected and recycled row) / per-scene rearm / empty list"<<std::endl;
            bool snapshotOk=true;
            if(args.startsWith("--snapshot ")) {
                editor.setScenes({{"01:00:00:00","Opening","/cue/1",1,true},{"01:00:30:00","Chorus","/cue/2",1,true},{"01:01:00:00","Outro","/cue/3",1,true}},30);
                editor.setSelectedRow(1);editor.setFiredStates({true,true,false},true);
                const auto picture=editor.createComponentSnapshot(editor.getLocalBounds());
                auto output=juce::File(args.substring(11).trim().unquoted()).createOutputStream();
                snapshotOk=output && juce::PNGImageFormat().writeImageToStream(picture,*output);
            }
            editor.setLookAndFeel(nullptr);
            setApplicationReturnValue(passed && snapshotOk?0:1);quit();
        });
    }
    void shutdown() override{}
};
START_JUCE_APPLICATION(SceneTestApplication)
