#include "MainComponent.h"
#include <algorithm>
namespace {
void label(juce::Component& parent,juce::Label& l,const juce::String& text,float size=14.f) {
    l.setText(text,juce::dontSendNotification);l.setFont(juce::Font(juce::FontOptions(size)));parent.addAndMakeVisible(l);
}
}
MainComponent::MainComponent() : capture([this](int frame){decodedFrame(frame);},fps) {
    setLookAndFeel(&lookAndFeel);
    suppressChanges=true;
    label(*this,title,"TIMECODE CUE BRIDGE",14.f);
    label(*this,version,"v"+juce::String(JUCE_APPLICATION_VERSION_STRING),12.f);version.setJustificationType(juce::Justification::centredRight);version.setColour(juce::Label::textColourId,ui::muted);
    title.setColour(juce::Label::textColourId,ui::muted);
    label(*this,timecode,"--:--:--:--",32.f);timecode.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),32.f,juce::Font::plain)));
    label(*this,status,"DISARMED",13.f);label(*this,inputStatus,"Waiting for LTC",12.f);
    inputStatus.setColour(juce::Label::textColourId,ui::muted);
    for(auto* b:{&saveButton,&armButton,&testButton,&resetButton,&panicButton,&folderButton})addAndMakeVisible(*b);
    armButton.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff785b32));
    saveButton.setTooltip("Save scenes and I/O settings. Automatic output remains disarmed.");
    armButton.setTooltip("Enable automatic cue output from LTC");testButton.setTooltip("Immediately send the selected scene, including while disarmed");
    resetButton.setTooltip("Rearm all cues for another pass");panicButton.setTooltip("Disarm and stop notes held by this app");folderButton.setTooltip("Open the settings and log folder");
    addAndMakeVisible(tabs);tabs.setTabBarDepth(34);tabs.setOutline(0);
    tabs.addTab("Scenes",ui::panel,&editor,false);tabs.addTab("I/O Settings",ui::panel,&settingsPage,false);
    audioSelector=std::make_unique<juce::AudioDeviceSelectorComponent>(capture.manager(),1,1,0,0,false,false,false,false);
    settingsPage.addAndMakeVisible(*audioSelector);
    label(settingsPage,audioLabel,"LTC INPUT",12.f);
    label(settingsPage,hostLabel,"OSC host");label(settingsPage,portLabel,"UDP port");label(settingsPage,fpsLabel,"Frame rate");
    label(settingsPage,midiLabel,"MIDI OUTPUT",12.f);
    for(auto* l:{&audioLabel,&hostLabel,&portLabel,&fpsLabel,&midiLabel})l->setColour(juce::Label::textColourId,ui::muted);
    midiChoice.setTooltip("Select a hardware or virtual MIDI output port");
    for(auto* e:{&host,&port})settingsPage.addAndMakeVisible(*e);
    for(auto* c:{&fpsChoice,&midiChoice})settingsPage.addAndMakeVisible(*c);
    settingsPage.addAndMakeVisible(refreshMidiButton);
    host.setText("127.0.0.1",false);port.setText("9000",false);
    for(int f:{24,25,30})fpsChoice.addItem(juce::String(f),f);
    fpsChoice.setSelectedId(30,juce::dontSendNotification);
    settingsPage.layout=[this]{layoutSettings();};
    setSize(1100,790);
    getLogFile().getParentDirectory().createDirectory();
    logger=std::make_unique<juce::FileLogger>(getLogFile(),"Timecode Cue Bridge 0.5.1 started",1024*1024);
    loadSettings();suppressChanges=false;
    editor.onChange=[this]{if(!suppressChanges)disarm(juce::String("Unsaved scene changes"),true);};
    editor.onRearmScene=[this](int row){
        std::lock_guard<std::mutex> lock(runtimeMutex);
        if(!applied){setStatus("Save & Apply before rearming a scene");return;}
        if(row>=0 && static_cast<size_t>(row)<editor.scenes().size() && editor.scenes()[static_cast<size_t>(row)].enabled
            && cueEngine.hasFired(static_cast<size_t>(row)) && cueEngine.resetCue(static_cast<size_t>(row))) {
            editor.setFiredStates(cueEngine.firedStates(),true);
            setStatus("Scene rearmed: "+juce::String(editor.scenes()[static_cast<size_t>(row)].label)+" - waiting for its next LTC crossing");
        }
    };
    editor.onUseCurrent=[this]{
        if(fpsChoice.getSelectedId()!=fps.load()){setStatus(juce::String("Save & Apply the new frame rate first"));return;}
        const auto seen=capture.lastSeenMillis();
        if(seen==0 || static_cast<unsigned int>(juce::Time::getMillisecondCounter()-seen)>=250){setStatus(juce::String("No live LTC input"));return;}
        editor.useCurrentTimecode(capture.mostRecentFrame());
    };
    for(auto* e:{&host,&port})e->onTextChange=[this]{if(!suppressChanges)disarm(juce::String("Unsaved destination changes"),true);};
    fpsChoice.onChange=[this]{if(!suppressChanges){disarm(juce::String("Frame rate changed; frame numbers adjusted"),true);editor.changeFps(fpsChoice.getSelectedId());}};
    midiChoice.onChange=[this]{if(!suppressChanges){wantedMidiId=selectedMidiIdentifier();wantedMidiName=selectedMidiName();disarm(juce::String("Unsaved MIDI output changes"),true);}};
    refreshMidiButton.onClick=[this]{refreshMidiDevices(true);};
    saveButton.onClick=[this]{saveAndApply();};
    armButton.onClick=[this]{
        std::lock_guard<std::mutex> lock(runtimeMutex);
        if(!applied){setStatus(juce::String("Save & Apply valid settings before arming"));return;}
        const bool hasActive=std::any_of(editor.scenes().begin(),editor.scenes().end(),[](const auto& c){return c.enabled;});
        if(!armed && !hasActive){setStatus(juce::String("No enabled scenes"));return;}
        armed=!armed;cueEngine.signalLost();capture.invalidate();if(!armed)midi.panic();
        armButton.setToggleState(armed,juce::dontSendNotification);
        armButton.setButtonText(armed?juce::String("Disarm"):juce::String("Arm"));
        setStatus(armed?juce::String("ARMED - waiting for stable LTC"):juce::String("DISARMED"));
    };
    testButton.onClick=[this]{
        const auto* scene=editor.selectedScene();if(!scene){setStatus(juce::String("Select a scene to test"));return;}
        std::lock_guard<std::mutex> lock(runtimeMutex);
        if(!applied){setStatus(juce::String("Save & Apply before testing"));return;}
        if(!scene->enabled){setStatus(juce::String("Selected scene is disabled"));return;}
        sendCue(*scene,true);
    };
    resetButton.onClick=[this]{std::lock_guard<std::mutex> lock(runtimeMutex);midi.panic();cueEngine.resetAll();editor.setFiredStates(cueEngine.firedStates(),applied);setStatus(juce::String("Cues reset - play LTC from before the trigger time"));};
    panicButton.onClick=[this]{disarm(juce::String("MIDI notes stopped"),false);};
    folderButton.onClick=[this]{getConfigFile().getParentDirectory().startAsProcess();};
    capture.manager().addChangeListener(this);startTimerHz(10);
}
MainComponent::~MainComponent(){
    stopTimer();capture.manager().removeChangeListener(this);
    {std::lock_guard<std::mutex> lock(runtimeMutex);armed=false;midi.panic();}
    capture.stop();setLookAndFeel(nullptr);
}
void MainComponent::paint(juce::Graphics& g){
    g.fillAll(ui::background);g.setColour(ui::line);g.drawHorizontalLine(91,16.f,static_cast<float>(getWidth()-16));
}
void MainComponent::resized(){
    const int w=getWidth(),h=getHeight();title.setBounds(20,13,290,24);timecode.setBounds(16,38,300,44);
    version.setBounds(w-126,13,106,24);status.setBounds(324,22,w-460,25);inputStatus.setBounds(324,50,w-344,25);
    tabs.setBounds(16,104,w-32,h-168);
    saveButton.setBounds(16,h-48,128,32);armButton.setBounds(152,h-48,92,32);testButton.setBounds(260,h-48,112,32);
    resetButton.setBounds(w-364,h-48,108,32);panicButton.setBounds(w-248,h-48,108,32);folderButton.setBounds(w-132,h-48,116,32);
    layoutSettings();
}
void MainComponent::layoutSettings(){
    if(!audioSelector)return;
    const int w=settingsPage.getWidth(),h=settingsPage.getHeight(),right=500;
    audioLabel.setBounds(24,20,420,24);audioSelector->setBounds(24,58,440,std::max(200,h-82));
    hostLabel.setBounds(right,20,w-right-160,24);host.setBounds(right,50,w-right-178,34);
    portLabel.setBounds(w-162,20,138,24);port.setBounds(w-162,50,138,34);
    fpsLabel.setBounds(right,110,200,24);fpsChoice.setBounds(right,140,150,34);
    midiLabel.setBounds(right,224,w-right-24,24);midiChoice.setBounds(right,258,w-right-124,34);refreshMidiButton.setBounds(w-108,258,84,34);
}
juce::File MainComponent::getConfigFile()const{return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("TimecodeCueBridge").getChildFile("cues.json");}
juce::File MainComponent::getLogFile()const{return getConfigFile().getSiblingFile("events.log");}
void MainComponent::setStatus(const juce::String& text){std::lock_guard<std::mutex> lock(statusMutex);if(text==lastStatus && text.startsWith("DISARMED"))return;lastStatus=text;if(logger)logger->logMessage(juce::Time::getCurrentTime().toISO8601(true)+" "+text);}
void MainComponent::disarm(const juce::String& reason,bool dirty){
    std::lock_guard<std::mutex> lock(runtimeMutex);armed=false;if(dirty)applied=false;
    midi.panic();cueEngine.signalLost();armButton.setToggleState(false,juce::dontSendNotification);armButton.setButtonText(juce::String("Arm"));setStatus(juce::String("DISARMED - ")+reason);
}
void MainComponent::changeListenerCallback(juce::ChangeBroadcaster*){if(!suppressChanges)disarm(juce::String("Unsaved audio settings"),true);}
juce::String MainComponent::selectedMidiIdentifier() const {
    const int index=midiChoice.getSelectedId()-2;
    return index>=0 && index<midiDevices.size()?midiDevices[index].identifier:(midiChoice.getSelectedId()==9999?wantedMidiId:juce::String());
}
juce::String MainComponent::selectedMidiName() const {
    const int index=midiChoice.getSelectedId()-2;
    return index>=0 && index<midiDevices.size()?midiDevices[index].name:(midiChoice.getSelectedId()==9999?wantedMidiName:juce::String());
}
void MainComponent::refreshMidiDevices(bool userAction){
    if(userAction){wantedMidiId=selectedMidiIdentifier();wantedMidiName=selectedMidiName();disarm(juce::String("MIDI ports refreshed - Save & Apply"),true);}
    const bool prior=suppressChanges;suppressChanges=true;midiDevices=juce::MidiOutput::getAvailableDevices();midiChoice.clear(juce::dontSendNotification);midiChoice.addItem(juce::String("None"),1);int selected=1;
    for(int i=0;i<midiDevices.size();++i){midiChoice.addItem(midiDevices[i].name,i+2);if(midiDevices[i].identifier==wantedMidiId)selected=i+2;}
    if(wantedMidiId.isNotEmpty() && selected==1){midiChoice.addItem(juce::String("Unavailable: ")+wantedMidiName,9999);selected=9999;}
    midiChoice.setSelectedId(selected,juce::dontSendNotification);suppressChanges=prior;
}
bool MainComponent::applyRuntime(const tcb::SceneSettings& s,juce::String& error){
    if(!tcb::validateScenes(s.scenes,s.fps,error))return false;
    const bool needsOsc=std::any_of(s.scenes.begin(),s.scenes.end(),[](const auto& c){return c.enabled && c.oscEnabled;});
    const bool needsMidi=std::any_of(s.scenes.begin(),s.scenes.end(),[](const auto& c){return c.enabled && c.midiEnabled;});
    if(needsMidi && (s.midiIdentifier.isEmpty() || midiChoice.getSelectedId()==9999)){error=juce::String("Select an available MIDI output for the enabled MIDI scenes");return false;}
    std::lock_guard<std::mutex> lock(runtimeMutex);armed=false;applied=false;midi.panic();
    if(needsOsc && !osc.connect(s.host,s.port)){error=juce::String("Cannot connect to OSC destination");return false;}
    if(!midi.open(needsMidi?s.midiIdentifier:juce::String())){error=juce::String("Cannot open MIDI output; it may be in use");return false;}
    fps.store(s.fps);capture.invalidate();cueEngine.configure(s.scenes,s.fps);applied=true;return true;
}
void MainComponent::saveAndApply(){
    disarm(juce::String("Checking settings"),true);
    if(!editor.commitPendingEdits()){setStatus("Invalid numeric field - enter a whole number or press Esc");return;}
    const auto ptext=port.getText().trim();const int p=ptext.getIntValue();
    if(p<1 || p>65535 || ptext!=juce::String(p) || host.getText().trim().isEmpty()){setStatus(juce::String("Invalid OSC host or UDP port"));return;}
    juce::String error;if(!editor.currentFieldsValid()){setStatus(juce::String("OSC value must be an integer from -2147483648 to 2147483647"));return;}
    tcb::SceneSettings s;s.scenes=editor.scenes();s.host=host.getText().trim();s.port=p;s.fps=fpsChoice.getSelectedId();s.midiIdentifier=selectedMidiIdentifier();s.midiName=selectedMidiName();
    if(auto xml=capture.manager().createStateXml())s.audioXml=xml->toString();
    if(!tcb::validateScenes(s.scenes,s.fps,error)){setStatus(error);return;}
    // Open outputs before committing. On failure automatic output stays disarmed.
    if(!applyRuntime(s,error)){setStatus(error);return;}
    const auto file=getConfigFile();file.getParentDirectory().createDirectory();
    if(backupOnSave && file.existsAsFile()){
        const auto backup=file.getSiblingFile("cues-before-v0.5.json").getNonexistentSibling();
        if(!file.copyFileTo(backup)){disarm(juce::String("Backup failed; settings were not saved"),true);return;}
        backupOnSave=false;
    }
    juce::TemporaryFile temp(file);
    if(!temp.getFile().replaceWithText(juce::JSON::toString(tcb::encodeSettings(s),true)) || !temp.overwriteTargetFileWithTemporary()){
        disarm(juce::String("Save failed; previous settings preserved"),true);return;
    }
    setStatus(juce::String("Settings saved - disarmed"));
}
void MainComponent::loadSettings(){
    tcb::SceneSettings s;juce::String error;const auto file=getConfigFile();bool valid=true;
    if(file.existsAsFile()){
        const auto json=juce::JSON::parse(file);valid=tcb::decodeSettings(json,s,error);
        backupOnSave=!valid || static_cast<int>(json.getProperty("version",1))<3;
    }else{
        s.scenes={{"01:00:00:00","Scene 1","/cue/1",1,true,true,false,{}},{"01:10:00:00","Scene 2","/cue/2",1,true,true,false,{}}};
    }
    host.setText(s.host,false);port.setText(juce::String(s.port),false);fpsChoice.setSelectedId(s.fps,juce::dontSendNotification);
    editor.setScenes(s.scenes,s.fps);wantedMidiId=s.midiIdentifier;wantedMidiName=s.midiName;refreshMidiDevices(false);
    const auto xml=juce::XmlDocument::parse(s.audioXml);const auto audioError=capture.start(xml.get());
    if(!valid){setStatus(juce::String("Cannot load settings: ")+error+juce::String(" (original file preserved)"));return;}
    if(!applyRuntime(s,error)){setStatus(error);return;}
    setStatus(file.existsAsFile()?juce::String("Settings loaded - disarmed"):juce::String("Ready - disarmed"));
    if(audioError.isNotEmpty())setStatus(juce::String("Check audio input: ")+audioError);
}
void MainComponent::sendCue(const tcb::Cue& c,bool manual){
    juce::String result;
    if(c.oscEnabled)result+=osc.send(c)?"OSC dispatched ":"OSC ERROR ";
    if(c.midiEnabled)result+=midi.send(c.midi)?"MIDI dispatched ":"MIDI ERROR ";
    setStatus(juce::String(manual?"TEST ":"AUTO ")+juce::String(c.label)+" @ "+juce::String(c.timecode)+" | "+result);
}
void MainComponent::decodedFrame(int frame){
    std::lock_guard<std::mutex> lock(runtimeMutex);
    if(frame<0){
        cueEngine.signalLost();midi.panic();
        if(inputLocked)setStatus("LTC lost - reacquiring");
        inputLocked=false;return;
    }
    if(!inputLocked)setStatus(armed?"LTC locked - armed":"LTC locked - disarmed");
    inputLocked=true;
    if(!armed){cueEngine.signalLost();return;}
    if(cueEngine.isDiscontinuity(frame))midi.panic();
    for(const auto& trigger:cueEngine.process(frame))sendCue(trigger.cue,false);
}
void MainComponent::timerCallback(){
    const auto seen=capture.lastSeenMillis();const bool fresh=seen!=0 && static_cast<unsigned int>(juce::Time::getMillisecondCounter()-seen)<250;
    timecode.setText(fresh?tcb::formatTimecode(capture.mostRecentFrame(),fps.load()):"--:--:--:--",juce::dontSendNotification);
    juce::String text;{std::lock_guard<std::mutex> lock(statusMutex);text=lastStatus;}
    if(capture.sawRejectedFrame())text=juce::String("Unsupported LTC (DF / reverse / invalid) | ")+text;
    status.setText(text,juce::dontSendNotification);
    {std::lock_guard<std::mutex> lock(runtimeMutex);editor.setFiredStates(cueEngine.firedStates(),applied);}
    inputStatus.setText(juce::String(fresh?"LOCKED | ":"NO LOCK | ")+"Input "+juce::String(capture.inputSampleRate(),0)+" Hz | Peak "+juce::String(capture.inputPeak(),3)+" | Dropped "+juce::String(capture.droppedSamplesCount())+" | "+juce::String(fps.load())+juce::String(" fps | Scenes ")+juce::String(static_cast<int>(editor.scenes().size())),juce::dontSendNotification);
}
