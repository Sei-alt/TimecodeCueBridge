#pragma once
#include "CueEngine.h"
#include "DragNumber.h"
#include <juce_gui_extra/juce_gui_extra.h>
class SceneEditor final : public juce::Component, private juce::ListBoxModel {
public:
    SceneEditor();
    void resized() override;
    void paint(juce::Graphics&) override;
    void setScenes(std::vector<tcb::Cue> cues,int fps);
    const std::vector<tcb::Cue>& scenes() const { return cues; }
    void changeFps(int newFps);
    void setFiredStates(std::vector<bool>,bool settingsApplied);
    const tcb::Cue* selectedScene() const;
    int selectedRow() const { return list.getSelectedRow(); }
    void setSelectedRow(int row) { list.selectRow(row); }
    void addScene();
    void removeScene();
    void duplicateScene();
    void useCurrentTimecode(int frame);
    void undoRemove();
    std::function<void()> onChange;
    std::function<void()> onUseCurrent;
    std::function<void(int)> onRearmScene;
    // Runs on the message thread; GUI regression tests drive actual controls.
    bool runControlTests();
    bool currentFieldsValid() const;
    bool commitPendingEdits();
private:
    class RowActions;
    int getNumRows() override;
    juce::Component* refreshComponentForRow(int,bool,juce::Component*) override;
    void paintListBoxItem(int,juce::Graphics&,int,int,bool) override;
    bool canRearm(int row) const;
    void requestRearm(int row);
    void selectedRowsChanged(int) override;
    void refreshFields();
    void changed();
    void updateRunState();
    void updateTimeFromParts();
    void refreshTimeControls();
    tcb::Cue* selected();
    std::vector<tcb::Cue> cues;
    std::vector<bool> fired;
    bool runStateApplied=false;
    int frameRate=30,displayedRow=-1,commitRow=-1;
    bool loading=false;
    std::optional<tcb::Cue> removed;
    int removedRow=0;
    juce::ListBox list {"Scenes",this};
    juce::TextButton addButton {"+"},removeButton {"-"},duplicateButton {"Duplicate"},undoButton {"Restore"},sortButton {"Sort"},currentButton {"Use LTC"},rearmButton {"Rearm Scene"};
    juce::Label listTitle,progressLabel,sceneState,nameLabel,timeLabel,hourLabel,minuteLabel,secondLabel,frameLabel,oscAddressLabel,oscValueLabel,midiTypeLabel,midiChannelLabel,midiNumberLabel,midiValueLabel,midiGateLabel;
    juce::TextEditor name,address,oscValue;
    juce::ToggleButton enabled {"Enabled"},oscEnabled {"OSC"},midiEnabled {"MIDI"};
    DragNumber hours,minutes,seconds,frames,midiChannel,midiNumber,midiValue,midiGate;
    juce::ComboBox midiType;
};
