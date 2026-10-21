#include "MidiOutput.h"
namespace tcb {
MidiOutputService::MidiOutputService() : juce::Thread("MIDI Note Off Scheduler") { startThread(); }
MidiOutputService::~MidiOutputService() {
    signalThreadShouldExit(); notify(); waitForThreadToExit(-1);
    std::lock_guard<std::mutex> lock(mutex); stopNotes(); device.reset();
}
bool MidiOutputService::open(const juce::String& id) {
    std::lock_guard<std::mutex> lock(mutex);
    stopNotes(); device.reset(); testSink={};
    if(id.isEmpty()) return true;
    device=juce::MidiOutput::openDevice(id);
    return device!=nullptr;
}
bool MidiOutputService::ready() const { std::lock_guard<std::mutex> lock(mutex); return device!=nullptr || static_cast<bool>(testSink); }
void MidiOutputService::emit(const MidiPacket& packet) {
    if(testSink) testSink(packet);
    else if(device) device->sendMessageNow(juce::MidiMessage(packet.data(),3,0.0));
}
bool MidiOutputService::send(const MidiAction& action) {
    const auto packet=makeMidiPacket(action); if(!packet) return false;
    std::lock_guard<std::mutex> lock(mutex);
    if(!device && !testSink) return false;
    auto& note=notes[static_cast<size_t>(action.channel-1)][static_cast<size_t>(action.number)];
    const bool off=action.kind==MidiKind::noteOff || (action.kind==MidiKind::noteOn && action.value==0);
    if(action.kind==MidiKind::noteOn && action.value>0 && note.active)
        emit({static_cast<unsigned char>(0x80+action.channel-1),static_cast<unsigned char>(action.number),0});
    emit(*packet);
    if(off) note={};
    else if(action.kind==MidiKind::noteOn) {
        note.active=true;
        note.offAt=action.gateMs>0 ? juce::Time::getMillisecondCounterHiRes()+action.gateMs : 0;
    }
    notify(); return true;
}
void MidiOutputService::stopNotes() {
    for(size_t ch=0;ch<notes.size();++ch) for(size_t n=0;n<notes[ch].size();++n) {
        if(notes[ch][n].active) emit({static_cast<unsigned char>(0x80+ch),static_cast<unsigned char>(n),0});
        notes[ch][n]={};
    }
}
void MidiOutputService::panic() { std::lock_guard<std::mutex> lock(mutex); stopNotes(); }
void MidiOutputService::setTestSink(Sink sink) {
    std::lock_guard<std::mutex> lock(mutex); stopNotes(); device.reset(); testSink=std::move(sink);
}
void MidiOutputService::run() {
    while(!threadShouldExit()) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            const double now=juce::Time::getMillisecondCounterHiRes();
            for(size_t ch=0;ch<notes.size();++ch) for(size_t n=0;n<notes[ch].size();++n) {
                auto& note=notes[ch][n];
                if(note.active && note.offAt>0 && now>=note.offAt) {
                    emit({static_cast<unsigned char>(0x80+ch),static_cast<unsigned char>(n),0}); note={};
                }
            }
        }
        wait(5);
    }
}
}
