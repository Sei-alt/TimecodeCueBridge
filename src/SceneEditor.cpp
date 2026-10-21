#include "SceneEditor.h"
#include <algorithm>
#include <charconv>
#include <iostream>
namespace {
void label(juce::Component& parent,juce::Label& l,const juce::String& text,float size=14.f) {
    l.setText(text,juce::dontSendNotification);l.setFont(juce::Font(juce::FontOptions(size)));parent.addAndMakeVisible(l);
}
void step(juce::Component& parent,DragNumber& field,int low,int high,const juce::String& title) {
    field.setRange(low,high);field.setTitle(title);field.setDescription("Editable number; drag vertically to adjust");parent.addAndMakeVisible(field);
}
}
class SceneEditor::RowActions final : public juce::Component {
public:
    explicit RowActions(SceneEditor& parent) : editor(parent) {
        setInterceptsMouseClicks(false,true);addAndMakeVisible(rearm);
        rearm.setTooltip("Rearm this scene for its next LTC crossing");
        rearm.onClick=[this]{editor.requestRearm(row);};
    }
    void update(int newRow){row=newRow;rearm.setEnabled(editor.canRearm(row));}
    void resized() override {rearm.setBounds(getWidth()-78,33,66,23);}
    juce::TextButton rearm {"Rearm"};
private:
    SceneEditor& editor;
    int row=-1;
};
SceneEditor::SceneEditor() {
    addAndMakeVisible(list);list.setRowHeight(62);list.setColour(juce::ListBox::backgroundColourId,ui::background);
    list.setOutlineThickness(0);
    label(*this,listTitle,"SCENES",12);label(*this,progressLabel,"",12);label(*this,sceneState,"",12);label(*this,nameLabel,"Scene name",13);label(*this,timeLabel,"TRIGGER TIMECODE",12);
    label(*this,hourLabel,"Hours",12);label(*this,minuteLabel,"Minutes",12);label(*this,secondLabel,"Seconds",12);label(*this,frameLabel,"Frames",12);
    label(*this,oscAddressLabel,"Address",13);label(*this,oscValueLabel,"Integer value",13);
    label(*this,midiTypeLabel,"Message",13);label(*this,midiChannelLabel,"Channel",13);label(*this,midiNumberLabel,"Note / CC",13);
    label(*this,midiValueLabel,"Velocity / value",13);label(*this,midiGateLabel,"Note length (ms)",13);
    for(auto* l:{&listTitle,&progressLabel,&sceneState,&timeLabel,&hourLabel,&minuteLabel,&secondLabel,&frameLabel,&nameLabel,&oscAddressLabel,&oscValueLabel,&midiTypeLabel,&midiChannelLabel,&midiNumberLabel,&midiValueLabel,&midiGateLabel})l->setColour(juce::Label::textColourId,ui::muted);
    for(auto* e:{&name,&address,&oscValue}) {addAndMakeVisible(*e);e->setFont(juce::Font(juce::FontOptions(15.f)));e->setIndents(9,7);}
    for(auto* b:{&addButton,&removeButton,&duplicateButton,&undoButton,&sortButton,&currentButton,&rearmButton}) addAndMakeVisible(*b);
    addButton.setTooltip("Add scene after selection");removeButton.setTooltip("Delete selected scene");undoButton.setTooltip("Restore last deleted scene");sortButton.setTooltip("Sort by trigger time");currentButton.setTooltip("Copy the currently received LTC timecode");
    for(auto* b:{&enabled,&oscEnabled,&midiEnabled}) addAndMakeVisible(*b);
    step(*this,hours,0,23,"Hours");step(*this,minutes,0,59,"Minutes");step(*this,seconds,0,59,"Seconds");step(*this,frames,0,29,"Frames");
    for(auto* f:{&hours,&minutes,&seconds,&frames}){f->setDigits(2);f->setNumberFont(30.f);}
    step(*this,midiChannel,1,16,"MIDI channel");step(*this,midiNumber,0,127,"MIDI note or controller number");step(*this,midiValue,0,127,"MIDI velocity or value");step(*this,midiGate,0,60000,"Note length in milliseconds");
    midiGate.setTooltip("Note On duration in milliseconds. 0 holds until Note Off or disarm.");
    for(auto* f:{&hours,&minutes,&seconds,&frames,&midiChannel,&midiNumber,&midiValue,&midiGate})f->onEditStart=[this]{if(!loading)changed();};
    addAndMakeVisible(midiType);midiType.addItem("Note On",1);midiType.addItem("Note Off",2);midiType.addItem("Control Change",3);
    addButton.onClick=[this]{addScene();};removeButton.onClick=[this]{removeScene();};duplicateButton.onClick=[this]{duplicateScene();};undoButton.onClick=[this]{undoRemove();};
    sortButton.onClick=[this]{
        if(cues.size()<2 || !commitPendingEdits()) return;
        std::stable_sort(cues.begin(),cues.end(),[this](const auto& a,const auto& b){return tcb::parseTimecode(a.timecode,frameRate).value_or(0)<tcb::parseTimecode(b.timecode,frameRate).value_or(0);});
        list.updateContent();list.selectRow(0);refreshFields();changed();
    };
    rearmButton.setTooltip("Rearm only this scene for its next LTC crossing. Test Scene sends immediately.");
    rearmButton.onClick=[this]{requestRearm(selectedRow());};
    currentButton.onClick=[this]{if(onUseCurrent)onUseCurrent();};
    name.onTextChange=[this]{if(!loading)if(auto* c=selected()){c->label=name.getText().toStdString();changed();}};
    address.onTextChange=[this]{if(!loading)if(auto* c=selected()){c->address=address.getText().trim().toStdString();changed();}};
    oscValue.onTextChange=[this]{if(!loading)if(auto* c=selected()) {
        const auto text=oscValue.getText().trim().toStdString();int value=0;const auto p=std::from_chars(text.data(),text.data()+text.size(),value);
        if(!text.empty() && p.ec==std::errc{} && p.ptr==text.data()+text.size()) c->value=value;
        changed();
    }};
    for(auto* s:{&hours,&minutes,&seconds,&frames}) s->onValueChange=[this]{if(!loading)updateTimeFromParts();};
    enabled.onClick=[this]{if(!loading)if(auto* c=selected()){c->enabled=enabled.getToggleState();changed();}};
    oscEnabled.onClick=[this]{if(!loading)if(auto* c=selected()){c->oscEnabled=oscEnabled.getToggleState();changed();}};
    midiEnabled.onClick=[this]{if(!loading)if(auto* c=selected()){c->midiEnabled=midiEnabled.getToggleState();changed();}};
    midiType.onChange=[this]{if(!loading)if(auto* c=selected()){c->midi.kind=static_cast<tcb::MidiKind>(midiType.getSelectedId()-1);changed();}};
    for(auto* s:{&midiChannel,&midiNumber,&midiValue,&midiGate}) s->onValueChange=[this]{if(!loading)if(auto* c=selected()) {
        c->midi.channel=static_cast<int>(midiChannel.getValue());c->midi.number=static_cast<int>(midiNumber.getValue());
        c->midi.value=static_cast<int>(midiValue.getValue());c->midi.gateMs=static_cast<int>(midiGate.getValue());changed();
    }};
    refreshFields();
}
void SceneEditor::paint(juce::Graphics& g) {
    g.fillAll(ui::panel);g.setColour(ui::background);g.fillRect(0,0,292,getHeight());
    g.setColour(ui::line);g.drawVerticalLine(292,0.f,static_cast<float>(getHeight()));
    const int x=320,w=getWidth()-348;
    g.drawHorizontalLine(250,static_cast<float>(x),static_cast<float>(x+w));
    g.drawHorizontalLine(362,static_cast<float>(x),static_cast<float>(x+w));
    g.setColour(ui::muted);g.setFont(juce::Font(juce::FontOptions(26.f)));
    for(int i=0;i<3;++i)g.drawText(":",x+80+i*100,155,20,54,juce::Justification::centred);
}
void SceneEditor::resized() {
    const int x=320,w=getWidth()-348;
    listTitle.setBounds(16,15,78,26);progressLabel.setBounds(94,15,120,26);sortButton.setBounds(222,15,54,26);
    list.setBounds(0,53,291,getHeight()-112);
    addButton.setBounds(16,getHeight()-44,32,28);removeButton.setBounds(54,getHeight()-44,32,28);duplicateButton.setBounds(94,getHeight()-44,88,28);undoButton.setBounds(190,getHeight()-44,86,28);
    nameLabel.setBounds(x,18,w-120,20);enabled.setBounds(x+w-105,42,105,34);name.setBounds(x,42,w-120,34);
    timeLabel.setBounds(x,103,220,20);currentButton.setBounds(x+w-100,105,100,28);
    DragNumber* timeParts[]{&hours,&minutes,&seconds,&frames};juce::Label* partLabels[]{&hourLabel,&minuteLabel,&secondLabel,&frameLabel};
    for(int i=0;i<4;++i){partLabels[i]->setBounds(x+i*100,132,80,20);partLabels[i]->setJustificationType(juce::Justification::centred);timeParts[i]->setBounds(x+i*100,155,80,54);}
    rearmButton.setBounds(x,216,132,28);sceneState.setBounds(x+148,216,w-148,28);
    oscEnabled.setBounds(x,265,100,26);oscAddressLabel.setBounds(x,296,w-150,20);address.setBounds(x,320,w-150,32);
    oscValueLabel.setBounds(x+w-134,296,134,20);oscValue.setBounds(x+w-134,320,134,32);
    midiEnabled.setBounds(x,377,100,26);
    const int col=(w-32)/3;
    midiTypeLabel.setBounds(x,412,col,20);midiType.setBounds(x,437,col,32);
    midiChannelLabel.setBounds(x+col+16,412,col,20);midiChannel.setBounds(x+col+16,437,col,32);
    midiNumberLabel.setBounds(x+2*(col+16),412,col,20);midiNumber.setBounds(x+2*(col+16),437,col,32);
    midiValueLabel.setBounds(x,486,col,20);midiValue.setBounds(x,511,col,32);midiGateLabel.setBounds(x+col+16,486,col+50,20);midiGate.setBounds(x+col+16,511,col,32);
}
int SceneEditor::getNumRows(){return static_cast<int>(cues.size());}
bool SceneEditor::canRearm(int row) const {
    return runStateApplied && row>=0 && static_cast<size_t>(row)<cues.size()
        && static_cast<size_t>(row)<fired.size() && cues[static_cast<size_t>(row)].enabled && fired[static_cast<size_t>(row)];
}
void SceneEditor::requestRearm(int row) {if(canRearm(row) && onRearmScene)onRearmScene(row);}
juce::Component* SceneEditor::refreshComponentForRow(int row,bool,juce::Component* existing) {
    if(row<0 || static_cast<size_t>(row)>=cues.size()){delete existing;return nullptr;}
    auto* actions=dynamic_cast<RowActions*>(existing);
    if(!actions){delete existing;actions=new RowActions(*this);}
    actions->update(row);return actions;
}
void SceneEditor::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool isSelected) {
    if(row<0 || row>=static_cast<int>(cues.size())) return;
    const auto& c=cues[static_cast<size_t>(row)];
    const bool done=runStateApplied && static_cast<size_t>(row)<fired.size() && fired[static_cast<size_t>(row)];
    if(done)g.fillAll(juce::Colour(0xff293a30));
    if(isSelected)g.fillAll(juce::Colour(0xff3c494f));
    if(done){g.setColour(juce::Colour(0xff8bb59a));g.fillRect(0,0,3,height);}
    g.setColour(c.enabled?(done?juce::Colour(0xffb2d3bd):ui::text):ui::muted);g.setFont(juce::Font(juce::FontOptions(16.f)));
    g.drawText(juce::String(row+1)+"  "+juce::String(c.label),12,6,width-94,24,juce::Justification::centredLeft,true);
    g.setFont(juce::Font(juce::FontOptions(11.f)));g.setColour(done?juce::Colour(0xffa0cdaf):ui::muted);
    g.drawText(!c.enabled?"OFF":!runStateApplied?"EDITED":done?"FIRED":"READY",width-78,8,66,22,juce::Justification::centredRight);
    g.setFont(juce::Font(juce::FontOptions(13.f)));g.setColour(ui::muted);
    g.drawText(juce::String(c.timecode)+"   "+(c.oscEnabled?"OSC ":"")+(c.midiEnabled?"MIDI":""),12,32,width-102,22,juce::Justification::centredLeft,true);
    g.setColour(ui::line);g.drawHorizontalLine(height-1,0.f,static_cast<float>(width));
}
void SceneEditor::selectedRowsChanged(int){commitRow=displayedRow;commitPendingEdits();commitRow=-1;refreshFields();}
tcb::Cue* SceneEditor::selected(){const int row=commitRow>=0?commitRow:list.getSelectedRow();return row>=0 && row<static_cast<int>(cues.size())?&cues[static_cast<size_t>(row)]:nullptr;}
const tcb::Cue* SceneEditor::selectedScene() const {const int row=list.getSelectedRow();return row>=0 && row<static_cast<int>(cues.size())?&cues[static_cast<size_t>(row)]:nullptr;}
void SceneEditor::setScenes(std::vector<tcb::Cue> rows,int fps){for(auto* f:{&hours,&minutes,&seconds,&frames,&midiChannel,&midiNumber,&midiValue,&midiGate})f->cancel();fired.clear();runStateApplied=false;cues=std::move(rows);frameRate=fps;removed.reset();list.updateContent();list.selectRow(cues.empty()?-1:0);refreshFields();}
void SceneEditor::changed(){runStateApplied=false;fired.clear();updateRunState();list.updateContent();list.repaint();if(onChange)onChange();}
void SceneEditor::refreshTimeControls(){
    const bool wasLoading=loading;loading=true;
    const int absolute=selected()?tcb::parseTimecode(selected()->timecode,frameRate).value_or(0):0;
    hours.setValue(absolute/(frameRate*3600),juce::dontSendNotification);minutes.setValue((absolute/(frameRate*60))%60,juce::dontSendNotification);
    seconds.setValue((absolute/frameRate)%60,juce::dontSendNotification);frames.setRange(0,frameRate-1);frames.setValue(absolute%frameRate,juce::dontSendNotification);
    loading=wasLoading;
}
void SceneEditor::refreshFields(){
    for(auto* f:{&hours,&minutes,&seconds,&frames,&midiChannel,&midiNumber,&midiValue,&midiGate})f->cancel();
    displayedRow=list.getSelectedRow();
    loading=true;const auto* c=selected();const bool exists=c!=nullptr;
    for(auto* component:std::initializer_list<juce::Component*>{&name,&address,&oscValue,&enabled,&oscEnabled,&midiEnabled,&hours,&minutes,&seconds,&frames,&midiType,&midiChannel,&midiNumber,&midiValue,&midiGate,&currentButton,&removeButton,&duplicateButton}) component->setEnabled(exists);
    undoButton.setEnabled(removed.has_value());addButton.setEnabled(cues.size()<10000);
    name.setText(c?juce::String(c->label):"",false);address.setText(c?juce::String(c->address):"",false);oscValue.setText(c?juce::String(c->value):"1",false);
    enabled.setToggleState(c && c->enabled,juce::dontSendNotification);oscEnabled.setToggleState(c && c->oscEnabled,juce::dontSendNotification);midiEnabled.setToggleState(c && c->midiEnabled,juce::dontSendNotification);
    midiType.setSelectedId(c?static_cast<int>(c->midi.kind)+1:1,juce::dontSendNotification);
    midiChannel.setValue(c?c->midi.channel:1,juce::dontSendNotification);midiNumber.setValue(c?c->midi.number:60,juce::dontSendNotification);
    midiValue.setValue(c?c->midi.value:100,juce::dontSendNotification);midiGate.setValue(c?c->midi.gateMs:100,juce::dontSendNotification);
    refreshTimeControls();loading=false;updateRunState();
}
void SceneEditor::updateTimeFromParts(){if(auto* c=selected()){
    const int absolute=((static_cast<int>(hours.getValue())*60+static_cast<int>(minutes.getValue()))*60+static_cast<int>(seconds.getValue()))*frameRate+static_cast<int>(frames.getValue());
    c->timecode=tcb::formatTimecode(absolute,frameRate);refreshTimeControls();changed();
}}
void SceneEditor::addScene(){
    if(cues.size()>=10000 || !commitPendingEdits())return;
    const int row=list.getSelectedRow();const auto* prior=selected();tcb::Cue c;
    c.timecode=prior?prior->timecode:"00:00:00:00";
    size_t number=cues.size()+1;
    while(std::any_of(cues.begin(),cues.end(),[&](const auto& other){return other.label=="Scene "+std::to_string(number) || other.address=="/cue/"+std::to_string(number);}))++number;
    c.label="Scene "+std::to_string(number);c.address="/cue/"+std::to_string(number);
    const int insert=row>=0?row+1:static_cast<int>(cues.size());cues.insert(cues.begin()+insert,std::move(c));list.updateContent();list.selectRow(insert);refreshFields();changed();
}
void SceneEditor::duplicateScene(){if(cues.size()>=10000 || !selected() || !commitPendingEdits())return;const int row=list.getSelectedRow();auto c=*selected();c.label+=" (copy)";cues.insert(cues.begin()+row+1,c);list.updateContent();list.selectRow(row+1);refreshFields();changed();}
void SceneEditor::removeScene(){if(!selected() || !commitPendingEdits())return;removedRow=list.getSelectedRow();removed=*selected();cues.erase(cues.begin()+removedRow);list.updateContent();list.selectRow(cues.empty()?-1:std::min(removedRow,static_cast<int>(cues.size()-1)));refreshFields();changed();}
void SceneEditor::undoRemove(){if(!removed || cues.size()>=10000 || !commitPendingEdits())return;const int row=std::min(removedRow,static_cast<int>(cues.size()));cues.insert(cues.begin()+row,*removed);removed.reset();list.updateContent();list.selectRow(row);refreshFields();changed();}
void SceneEditor::changeFps(int newFps){if(newFps!=24 && newFps!=25 && newFps!=30)return;
    for(auto& c:cues){const auto old=tcb::parseTimecode(c.timecode,frameRate).value_or(0);c.timecode=tcb::formatTimecode((old/frameRate)*newFps+std::min(old%frameRate,newFps-1),newFps);}
    frameRate=newFps;refreshFields();changed();
}
void SceneEditor::useCurrentTimecode(int frame){if(frame<0 || !selected())return;selected()->timecode=tcb::formatTimecode(frame,frameRate);refreshTimeControls();changed();}
bool SceneEditor::runControlTests(){
    auto fail=[this](int line){std::cerr<<"Scene control failure at line "<<line<<" count="<<cues.size()<<" selected="<<selectedRow()<<" time="<<(selectedScene()?selectedScene()->timecode:"none")<<std::endl;return false;};
    setScenes({},30);addButton.onClick();if(cues.size()!=1)return fail(__LINE__);
    hours.setValue(1,juce::sendNotificationSync);minutes.setValue(2,juce::sendNotificationSync);seconds.setValue(3,juce::sendNotificationSync);frames.setValue(29,juce::sendNotificationSync);
    if(cues[0].timecode!="01:02:03:29")return fail(__LINE__);
    // Click-to-type commits, clamps to the selected FPS, rejects partial text, and supports Escape.
    seconds.beginEdit();seconds.input.setText("12",false);seconds.input.onReturnKey();
    if(cues[0].timecode!="01:02:12:29")return fail(__LINE__);
    frames.beginEdit();frames.input.setText("100",false);if(!commitPendingEdits() || frames.getValue()!=29)return fail(__LINE__);
    minutes.beginEdit();minutes.input.setText("1x",false);if(commitPendingEdits() || cues[0].timecode!="01:02:12:29")return fail(__LINE__);
    minutes.input.onEscapeKey();if(!commitPendingEdits())return fail(__LINE__);
    minutes.beginEdit();minutes.input.setText("",false);if(commitPendingEdits())return fail(__LINE__);minutes.input.onEscapeKey();
    minutes.beginEdit();minutes.input.setText("-1",false);minutes.input.onReturnKey();if(minutes.getValue()!=0)return fail(__LINE__);
    minutes.setValue(2); // Actual mouse drag on the numeric field: 6 pixels = 1 unit.
    const auto now=juce::Time::getCurrentTime();const auto source=juce::Desktop::getInstance().getMainMouseSource();
    auto event=[&](DragNumber& f,float y,int mods,bool dragged){return juce::MouseEvent(source,{20.f,y},juce::ModifierKeys(mods),1.f,0.f,0.f,0.f,0.f,&f,&f,now,{20.f,40.f},now,1,dragged);};
    minutes.mouseDown(event(minutes,40,juce::ModifierKeys::leftButtonModifier,false));
    minutes.mouseDrag(event(minutes,34,juce::ModifierKeys::leftButtonModifier,true));minutes.mouseUp(event(minutes,34,0,true));
    if(cues[0].timecode!="01:03:12:29" || minutes.input.isVisible())return fail(__LINE__);
    frames.mouseDown(event(frames,40,juce::ModifierKeys::leftButtonModifier,false));frames.mouseDrag(event(frames,1000,juce::ModifierKeys::leftButtonModifier,true));frames.mouseUp(event(frames,1000,0,true));if(frames.getValue()!=0)return fail(__LINE__);
    seconds.mouseDown(event(seconds,40,juce::ModifierKeys::leftButtonModifier,false));seconds.mouseDrag(event(seconds,16,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier,true));seconds.mouseUp(event(seconds,16,0,true));if(seconds.getValue()!=13)return fail(__LINE__);
    seconds.setValue(12);frames.setValue(9);
    name.setText(juce::String::fromUTF8("テストシーン"),false);name.onTextChange();if(cues[0].label!=std::string("テストシーン"))return fail(__LINE__);
    duplicateButton.onClick();if(cues.size()!=2 || selectedRow()!=1 || cues[1].label!=std::string("テストシーン (copy)"))return fail(__LINE__);
    setFiredStates({true,true},true);if(!rearmButton.isEnabled() || progressLabel.getText()!="2 / 2 fired" || sceneState.getText()!="Fired")return fail(__LINE__);
    int rearmed=-1;onRearmScene=[this,&rearmed](int row){rearmed=row;auto state=fired;state[static_cast<size_t>(rearmed)]=false;setFiredStates(state,true);};
    rearmButton.onClick();if(rearmed!=1 || !fired[0] || fired[1] || rearmButton.isEnabled() || progressLabel.getText()!="1 / 2 fired")return fail(__LINE__);
    setSelectedRow(0);setFiredStates({true,true},true);
    auto* rowAction=dynamic_cast<RowActions*>(list.getComponentForRowNumber(1));
    if(!rowAction || !rowAction->rearm.isEnabled())return fail(__LINE__);
    rowAction->rearm.onClick();
    if(rearmed!=1 || selectedRow()!=0 || !fired[0] || fired[1] || rowAction->rearm.isEnabled())return fail(__LINE__);
    // Visible row components are reused when scrolling: the callback must use the new row.
    RowActions recycled(*this);recycled.update(1);rearmed=-1;recycled.rearm.onClick();if(rearmed!=-1)return fail(__LINE__);
    recycled.update(0);recycled.rearm.onClick();if(rearmed!=0 || fired[0] || fired[1])return fail(__LINE__);
    setFiredStates({true,true},false);rearmed=-1;recycled.update(0);recycled.rearm.onClick();if(rearmed!=-1 || recycled.rearm.isEnabled())return fail(__LINE__);
    setSelectedRow(1);
    onRearmScene={};setFiredStates({false,false},true);if(sceneState.getText()!="Ready")return fail(__LINE__);
    seconds.beginEdit();seconds.input.setText("14",false);setSelectedRow(0);if(cues[1].timecode!="01:03:14:09" || cues[0].timecode!="01:03:12:09")return fail(__LINE__);setSelectedRow(1);seconds.setValue(12);
    removeButton.onClick();if(cues.size()!=1)return fail(__LINE__);undoButton.onClick();if(cues.size()!=2 || selectedRow()!=1)return fail(__LINE__);
    frames.setValue(29,juce::sendNotificationSync);changeFps(24);if(cues[1].timecode!="01:03:12:23")return fail(__LINE__);
    midiType.setSelectedId(3,juce::sendNotificationSync);midiChannel.setValue(16,juce::sendNotificationSync);midiNumber.setValue(127,juce::sendNotificationSync);
    if(cues[1].midi.kind!=tcb::MidiKind::controlChange || cues[1].midi.channel!=16 || cues[1].midi.number!=127)return fail(__LINE__);
    removeScene();removeScene();if(!cues.empty() || selectedScene())return fail(__LINE__);undoRemove();return cues.size()==1;
}

bool SceneEditor::currentFieldsValid() const {
    const auto* c=selectedScene();if(!c || !c->oscEnabled)return true;
    const auto text=oscValue.getText().trim().toStdString();int value=0;
    const auto p=std::from_chars(text.data(),text.data()+text.size(),value);
    return !text.empty() && p.ec==std::errc{} && p.ptr==text.data()+text.size();
}

bool SceneEditor::commitPendingEdits() {
    bool valid=true;for(auto* f:{&hours,&minutes,&seconds,&frames,&midiChannel,&midiNumber,&midiValue,&midiGate})if(!f->commit())valid=false;
    return valid;
}

void SceneEditor::setFiredStates(std::vector<bool> states,bool settingsApplied) {
    if(states.size()!=cues.size())settingsApplied=false;
    if(fired==states && runStateApplied==settingsApplied)return;
    fired=std::move(states);runStateApplied=settingsApplied;updateRunState();list.updateContent();list.repaint();
}
void SceneEditor::updateRunState() {
    int completed=0,active=0;
    for(size_t i=0;i<cues.size();++i)if(cues[i].enabled){++active;if(runStateApplied && i<fired.size() && fired[i])++completed;}
    progressLabel.setText(runStateApplied?juce::String(completed)+" / "+juce::String(active)+" fired":"",juce::dontSendNotification);
    const auto row=list.getSelectedRow();const auto* c=selectedScene();
    const bool done=runStateApplied && row>=0 && static_cast<size_t>(row)<fired.size() && fired[static_cast<size_t>(row)];
    sceneState.setText(!c?"":!c->enabled?"Disabled":!runStateApplied?"Settings not applied":done?"Fired":"Ready",juce::dontSendNotification);
    sceneState.setColour(juce::Label::textColourId,done?juce::Colour(0xffa0cdaf):ui::muted);
    rearmButton.setEnabled(c && c->enabled && runStateApplied && done);
}
