#include "CueEngine.h"
#include "CueText.h"
#include "LtcDecoder.h"
#include "LtcStream.h"
#include "LtcCapture.h"
#include "OscOutput.h"
#include "MidiOutput.h"
#include "SceneSettings.h"
#include <mutex>
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <vector>
static void check(bool ok,const char* s,int line) { if(!ok) { std::cerr<<"FAIL "<<line<<": "<<s<<std::endl; std::exit(1); } }
#define CHECK(x) check(static_cast<bool>(x),#x,__LINE__)
static std::vector<float> waveform(int rate,int fps,int start,int frames,bool drop=false) {
    std::unique_ptr<LTCEncoder,decltype(&ltc_encoder_free)> e(ltc_encoder_create(rate,fps,fps==25?LTC_TV_625_50:(fps==24?LTC_TV_FILM_24:LTC_TV_525_60),0),ltc_encoder_free);
    CHECK(e);
    SMPTETimecode tc{}; tc.hours=static_cast<unsigned char>(start/(3600*fps));
    tc.mins=static_cast<unsigned char>((start/(60*fps))%60); tc.secs=static_cast<unsigned char>((start/fps)%60); tc.frame=static_cast<unsigned char>(start%fps);
    ltc_encoder_set_timecode(e.get(),&tc);
    if(drop) { LTCFrame f{}; ltc_encoder_get_frame(e.get(),&f); f.dfbit=1; ltc_encoder_set_frame(e.get(),&f); }
    std::vector<float> out;
    for(int i=0;i<frames;++i) {
        ltc_encoder_encode_frame(e.get()); ltcsnd_sample_t* b=nullptr;
        const int n=ltc_encoder_get_bufferptr(e.get(),&b,1); CHECK(n>0);
        for(int j=0;j<n;++j) out.push_back((static_cast<float>(b[j])-128.f)/128.f);
        ltc_encoder_inc_timecode(e.get());
    }
    return out;
}
static void feed(tcb::LtcDecoder& decoder,std::vector<float>& wave,const tcb::LtcDecoder::Callback& callback) {
    // Deliberately split biphase edges/timecode frames at irregular audio block boundaries.
    const std::array<size_t,7> blocks{1,17,256,511,1024,2048,333};
    size_t offset=0,block=0;
    while(offset<wave.size()) { const auto n=std::min(blocks[block++%blocks.size()],wave.size()-offset); decoder.write(wave.data()+offset,n,callback); offset+=n; }
}
static std::vector<unsigned char> packet(const std::string& address,int32_t value) {
    std::vector<unsigned char> b(address.begin(),address.end()); b.push_back(0);
    while(b.size()%4) b.push_back(0);
    b.insert(b.end(),{',','i',0,0});
    const uint32_t u=static_cast<uint32_t>(value);
    for(int shift:{24,16,8,0}) b.push_back(static_cast<unsigned char>(u>>shift));
    return b;
}
static void checkDatagram(juce::DatagramSocket& socket,const std::string& address,int value) {
    CHECK(socket.waitUntilReady(true,2000)==1);
    std::array<unsigned char,1024> b{}; const int n=socket.read(b.data(),static_cast<int>(b.size()),false);
    const auto expected=packet(address,value); CHECK(n==static_cast<int>(expected.size()));
    CHECK(std::equal(expected.begin(),expected.end(),b.begin()));
}
static void roundTrip(int rate,int fps) {
    juce::DatagramSocket receiver; CHECK(receiver.bindToPort(0,"127.0.0.1"));
    tcb::OscOutput output; CHECK(output.connect("127.0.0.1",receiver.getBoundPort()));
    tcb::CueEngine engine;
    engine.configure({{"00:00:01:00","first","/test/one",1,true},
        {"00:00:01:00","disabled","/test/disabled",1,false},
        {"00:00:02:00","second","/test/two",-2147483647-1,true},
        {"00:00:02:00","same time","/test/three",2147483647,true}},fps);
    tcb::LtcDecoder decoder; CHECK(decoder.configure(rate,fps));
    int decoded=0,emitted=0;
    auto callback=[&](int frame) {
        if(frame<0) { engine.signalLost(); return; }
        ++decoded; for(const auto& t:engine.process(frame)) { CHECK(output.send(t.cue)); ++emitted; }
    };
    auto wave=waveform(rate,fps,0,3*fps); feed(decoder,wave,callback);
    CHECK(decoded>=3*fps-3); CHECK(emitted==3);
    checkDatagram(receiver,"/test/one",1); checkDatagram(receiver,"/test/two",-2147483647-1); checkDatagram(receiver,"/test/three",2147483647);
    CHECK(receiver.waitUntilReady(true,40)==0);
    // Lost input must retain the one-shot history. Reconnect just before cue 2.
    engine.signalLost(); CHECK(decoder.configure(rate,fps));
    auto reconnect=waveform(rate,fps,2*fps-5,10); feed(decoder,reconnect,callback);
    CHECK(emitted==3); CHECK(receiver.waitUntilReady(true,40)==0);
    // Seek forward across a cue: do not replay skipped triggers.
    engine.resetAll(); CHECK(decoder.configure(rate,fps));
    auto before=waveform(rate,fps,0,5); feed(decoder,before,callback);
    auto after=waveform(rate,fps,2*fps+2,8); feed(decoder,after,callback);
    CHECK(emitted==3);
    // Rewind after the explicit reset above keeps pending cues eligible, with pre-roll.
    CHECK(decoder.configure(rate,fps)); auto take=waveform(rate,fps,0,3*fps); feed(decoder,take,callback);
    CHECK(emitted==6); checkDatagram(receiver,"/test/one",1); checkDatagram(receiver,"/test/two",-2147483647-1); checkDatagram(receiver,"/test/three",2147483647);
    std::cout<<"PASS LTC waveform -> libltc -> CueEngine -> JUCE OSC -> UDP bytes: "<<rate<<" Hz / "<<fps<<" fps\n";
}
static void rejectedSignals() {
    tcb::LtcDecoder decoder; CHECK(!decoder.configure(0,30)); CHECK(!decoder.configure(48000,29));
    for(bool reverse:{false,true}) {
        CHECK(decoder.configure(48000,30));
        auto wave=waveform(48000,30,0,60,!reverse);
        if(reverse) std::reverse(wave.begin(),wave.end());
        int accepted=0,rejected=0;
        feed(decoder,wave,[&](int f){ if(f>=0) ++accepted; else ++rejected; });
        CHECK(accepted==0); CHECK(rejected>20); CHECK(decoder.rejectedFrame());
    }
    CHECK(decoder.configure(48000,30));
    std::vector<float> silence(48000,0); int callbacks=0;
    feed(decoder,silence,[&](int){++callbacks;}); CHECK(callbacks==0);
    std::mt19937 rng(42); for(auto& s:silence) s=static_cast<float>(static_cast<int>(rng()%200)-100)/100000.f;
    feed(decoder,silence,[&](int){++callbacks;}); CHECK(callbacks==0);
    tcb::OscOutput output; CHECK(!output.connect("127.0.0.1",0)); CHECK(!output.send({"00:00:00:00","x","bad",1,true}));
    std::cout<<"PASS drop-frame / reverse / silence / low noise rejection and invalid output\n";
}
static void testSettings() {
    tcb::SceneSettings settings;juce::String error;
    auto json=juce::JSON::parse(R"({"version":2,"host":"127.0.0.1","port":9000,"fps":30,"cues_text":"# general comment\n01:00:00:00 | Old scene | /old | 1\n# 01:01:00:00 | Disabled | /off | 0"})");
    CHECK(tcb::decodeSettings(json,settings,error));CHECK(settings.scenes.size()==2);CHECK(!settings.scenes[1].enabled);
    CHECK(settings.scenes[0].oscEnabled && !settings.scenes[0].midiEnabled);
    settings.scenes[0].label="日本語のシーン名";
    settings.midiIdentifier="test-port";settings.midiName="MIDI test";
    settings.scenes[0].midiEnabled=true;settings.scenes[0].midi={tcb::MidiKind::controlChange,16,127,127,0};
    settings.scenes[1].oscEnabled=false;settings.scenes[1].midiEnabled=true;
    const auto encoded=tcb::encodeSettings(settings);
    tcb::SceneSettings loaded;CHECK(tcb::decodeSettings(juce::JSON::parse(juce::JSON::toString(encoded)),loaded,error));
    CHECK(loaded.scenes.size()==2 && loaded.midiIdentifier=="test-port");
    CHECK(loaded.scenes[0].label==settings.scenes[0].label);
    CHECK(loaded.scenes[0].midi.channel==16 && loaded.scenes[0].midi.number==127 && loaded.scenes[0].midi.value==127);
    CHECK(!loaded.scenes[1].enabled && !loaded.scenes[1].oscEnabled && loaded.scenes[1].midiEnabled);
    settings.scenes.clear();CHECK(tcb::decodeSettings(tcb::encodeSettings(settings),loaded,error));CHECK(loaded.scenes.empty());
    auto bad=juce::JSON::parse(R"({"version":3,"port":9000,"fps":30,"scenes":[{"timecode":"00:00:01:00","name":"bad","midi_channel":17}]})");
    CHECK(!tcb::decodeSettings(bad,loaded,error));
    bad=juce::JSON::parse(R"({"version":3,"port":9000,"fps":30,"scenes":[{"timecode":"00:00:01:00","name":"bad","osc_value":2147483648}]})");CHECK(!tcb::decodeSettings(bad,loaded,error));
    bad=juce::JSON::parse(R"({"version":99,"scenes":[]})");CHECK(!tcb::decodeSettings(bad,loaded,error));
    bad=juce::JSON::parse(R"({"version":3,"port":9000,"fps":29,"scenes":[]})");CHECK(!tcb::decodeSettings(bad,loaded,error));
    std::cout<<"PASS v2 migration (including disabled rows), v3 roundtrip, empty document and corrupt-settings rejection\n";
}
static void testMidi() {
    struct Events {std::mutex mutex;std::vector<tcb::MidiPacket> packets;};
    auto events=std::make_shared<Events>();
    auto count=[&]{std::lock_guard<std::mutex> lock(events->mutex);return events->packets.size();};
    auto at=[&](size_t index){std::lock_guard<std::mutex> lock(events->mutex);CHECK(index<events->packets.size());return events->packets[index];};
    {
        tcb::MidiOutputService midi;midi.setTestSink([events](const auto& p){std::lock_guard<std::mutex> lock(events->mutex);events->packets.push_back(p);});
        CHECK(midi.ready());
        CHECK(midi.send({tcb::MidiKind::noteOn,1,60,100,30}));
        for(int i=0;i<100 && count()<2;++i)juce::Thread::sleep(5);
        CHECK(count()==2);CHECK(at(0)==(tcb::MidiPacket{0x90,60,100}));CHECK(at(1)==(tcb::MidiPacket{0x80,60,0}));
        CHECK(midi.send({tcb::MidiKind::noteOn,16,127,127,0}));
        CHECK(midi.send({tcb::MidiKind::noteOff,16,127,12,0}));
        CHECK(at(2)==(tcb::MidiPacket{0x9f,127,127}));CHECK(at(3)==(tcb::MidiPacket{0x8f,127,12}));
        CHECK(midi.send({tcb::MidiKind::controlChange,2,7,64,0}));CHECK(at(4)==(tcb::MidiPacket{0xb1,7,64}));
        CHECK(midi.send({tcb::MidiKind::noteOn,1,60,90,0}));CHECK(midi.send({tcb::MidiKind::noteOn,1,60,91,0}));
        CHECK(at(6)==(tcb::MidiPacket{0x80,60,0}));CHECK(at(7)==(tcb::MidiPacket{0x90,60,91}));
        midi.panic();CHECK(at(8)==(tcb::MidiPacket{0x80,60,0}));const auto prior=count();midi.panic();CHECK(count()==prior);
        CHECK(!midi.send({tcb::MidiKind::noteOn,0,60,127,0}));CHECK(count()==prior);
        // Drive the real MIDI service from decoded LTC at the same trigger boundary as OSC.
        tcb::LtcDecoder decoder;CHECK(decoder.configure(48000,30));tcb::CueEngine engine;
        tcb::Cue cue{"00:00:01:00","MIDI via LTC","/unused",1,true};cue.oscEnabled=false;cue.midiEnabled=true;cue.midi={tcb::MidiKind::controlChange,3,22,55,0};
        engine.configure({cue},30);auto wave=waveform(48000,30,0,60);int fired=0;
        feed(decoder,wave,[&](int frame){if(frame<0){engine.signalLost();return;}for(const auto& t:engine.process(frame)){CHECK(midi.send(t.cue.midi));++fired;}});
        CHECK(fired==1 && count()==prior+1);CHECK(at(prior)==(tcb::MidiPacket{0xb2,22,55}));
        tcb::CueEngine seek;seek.configure({},30);seek.process(100);
        CHECK(midi.send({tcb::MidiKind::noteOn,1,43,100,0}));
        if(seek.isDiscontinuity(200))midi.panic();
        CHECK(at(count()-1)==(tcb::MidiPacket{0x80,43,0}));
        CHECK(midi.send({tcb::MidiKind::noteOn,1,42,100,0}));
    }
    CHECK(at(count()-1)==(tcb::MidiPacket{0x80,42,0}));
    std::cout<<"PASS MIDI bytes, auto note-off, explicit off, retrigger, panic, destructor cleanup and LTC -> MIDI dispatch (injected sink)\n";
}
// Only the test harness injects audio. All production callback/FIFO/worker code is unchanged.
class LtcCaptureTestAccess {
public:
    static void start(LtcCapture& capture,double rate) {
        capture.sampleRate.store(rate);capture.started=true;capture.startThread();juce::Thread::sleep(15);
    }
    static void drain(LtcCapture& capture) {
        for(int i=0;i<1000 && capture.fifo.getNumReady()>0;++i)juce::Thread::sleep(2);
        CHECK(capture.fifo.getNumReady()==0);juce::Thread::sleep(15);
    }
    static void push(LtcCapture& capture,const std::vector<float>& wave) {
        std::array<float,512> out{};
        for(size_t offset=0;offset<wave.size();) {
            const int n=static_cast<int>(std::min(size_t(512),wave.size()-offset));
            const float* channels[]{nullptr,wave.data()+offset,nullptr};float* outputs[]{out.data()};
            out.fill(.8f);capture.audioDeviceIOCallbackWithContext(channels,3,outputs,1,n,{});
            CHECK(std::all_of(out.begin(),out.begin()+n,[](float f){return f==0.f;}));offset+=static_cast<size_t>(n);
            for(int i=0;i<1000 && capture.fifo.getNumReady()>8192;++i)juce::Thread::sleep(1);
        }
        drain(capture);
    }
    static void deviceStopped(LtcCapture& capture){capture.audioDeviceStopped();}
    static void deviceResumed(LtcCapture& capture,double rate){capture.sampleRate.store(rate);capture.invalidate();juce::Thread::sleep(15);}
};
static void testStreamRecovery(int rate,int fps) {
    tcb::LtcStream stream;const std::uint32_t start=0xffffff00u;CHECK(stream.configure(rate,fps,start));
    std::uint64_t samples=0;int accepted=0,losses=0;
    const auto callback=[&](int frame){if(frame>=0)++accepted;else ++losses;};
    auto push=[&](std::vector<float>& wave){
        for(size_t offset=0;offset<wave.size();) {
            const size_t n=std::min(size_t(511),wave.size()-offset);
            stream.process(wave.data()+offset,n,start+static_cast<std::uint32_t>(samples*1000/static_cast<unsigned int>(rate)),callback);
            samples+=n;offset+=n;
        }
    };
    auto first=waveform(rate,fps,0,fps);push(first);CHECK(accepted>=fps-3);
    for(int pause=0;pause<3;++pause) {
        std::vector<float> stopped(static_cast<size_t>(rate));
        for(size_t i=0;i<stopped.size();++i)stopped[i]=pause==0?0.f:pause==1?(i%2?.01f:-.01f):.12f;
        const int before=accepted;push(stopped);
        // A final frame may finish at the first edge of the stop interval.
        CHECK(accepted<=before+1);const int afterStop=accepted;
        auto resumed=waveform(rate,fps,fps,2*fps);push(resumed);CHECK(accepted-afterStop>=2*fps-12);
    }
    CHECK(losses>=3);
    const auto now=start+static_cast<std::uint32_t>(samples*1000/static_cast<unsigned int>(rate));
    const int before=losses;stream.poll(now+300,callback);CHECK(losses==before+1);
    stream.poll(now+600,callback);CHECK(losses==before+1); // One loss event, repeated parser recovery.
    auto resumed=waveform(rate,fps,0,fps);int afterPoll=0;
    stream.process(resumed.data(),resumed.size(),now+601,[&](int f){if(f>=0)++afterPoll;});CHECK(afterPoll>=fps-3);
    std::cout<<"PASS automatic LTC reacquisition: silence / high-frequency noise / DC / no callbacks / clock wrap: "<<rate<<" Hz / "<<fps<<" fps\n";
}
static void testLiveCaptureRecovery() {
    juce::DatagramSocket receiver;CHECK(receiver.bindToPort(0,"127.0.0.1"));
    tcb::OscOutput osc;CHECK(osc.connect("127.0.0.1",receiver.getBoundPort()));
    tcb::MidiOutputService midi;std::atomic<int> packets{0};midi.setTestSink([&](const auto&){++packets;});
    tcb::CueEngine engine;std::mutex mutex;std::atomic<int> calls{0};std::atomic<bool> armed{true};
    tcb::Cue a{"00:00:00:15","A","/live/a",1,true},b{"00:00:02:15","B","/live/b",2,true};
    a.midiEnabled=b.midiEnabled=true;a.midi=b.midi={tcb::MidiKind::controlChange,1,22,127,0};
    engine.configure({a,b},30);std::atomic<int> fps{30};
    LtcCapture capture([&](int frame){
        std::lock_guard<std::mutex> lock(mutex);
        if(frame<0){engine.signalLost();midi.panic();return;}
        if(!armed.load()){engine.signalLost();return;}
        if(engine.isDiscontinuity(frame))midi.panic();
        for(const auto& t:engine.process(frame)){CHECK(osc.send(t.cue));CHECK(midi.send(t.cue.midi));++calls;}
    },fps);
    LtcCaptureTestAccess::start(capture,48000);
    auto initial=waveform(48000,30,0,30);LtcCaptureTestAccess::push(capture,initial);
    CHECK(capture.mostRecentFrame()>=27);CHECK(calls==1);checkDatagram(receiver,"/live/a",1);
    // No callbacks at all: wall-clock watchdog must clear stale lock and allow resume.
    for(int i=0;i<200 && capture.lastSeenMillis()!=0;++i)juce::Thread::sleep(2);
    CHECK(capture.lastSeenMillis()==0 && capture.mostRecentFrame()==-1);
    std::vector<float> noise(48000);for(size_t i=0;i<noise.size();++i)noise[i]=i%2?.01f:-.01f;
    LtcCaptureTestAccess::push(capture,noise);
    auto resumed=waveform(48000,30,30,60);LtcCaptureTestAccess::push(capture,resumed);
    CHECK(capture.lastSeenMillis()!=0 && capture.mostRecentFrame()>=87);CHECK(calls==2);checkDatagram(receiver,"/live/b",2);
    // Repeated stop/rewind/resume must read LTC but never double-send completed cues.
    auto take=waveform(48000,30,0,90);
    for(int i=0;i<3;++i){LtcCaptureTestAccess::push(capture,noise);LtcCaptureTestAccess::push(capture,take);CHECK(capture.mostRecentFrame()>=87);CHECK(calls==2);}
    {std::lock_guard<std::mutex> lock(mutex);CHECK(engine.firedStates()==std::vector<bool>({true,true}));CHECK(engine.resetCue(0));}
    LtcCaptureTestAccess::push(capture,take);CHECK(calls==3);checkDatagram(receiver,"/live/a",1);
    {std::lock_guard<std::mutex> lock(mutex);CHECK(engine.hasFired(1));engine.resetAll();}
    LtcCaptureTestAccess::push(capture,take);CHECK(calls==5);checkDatagram(receiver,"/live/a",1);checkDatagram(receiver,"/live/b",2);
    CHECK(packets==5);CHECK(receiver.waitUntilReady(true,40)==0);
    armed=false;LtcCaptureTestAccess::push(capture,take);CHECK(capture.mostRecentFrame()>=87 && calls==5);
    LtcCaptureTestAccess::deviceStopped(capture);juce::Thread::sleep(20);CHECK(capture.lastSeenMillis()==0);
    LtcCaptureTestAccess::deviceResumed(capture,48000);LtcCaptureTestAccess::push(capture,take);CHECK(capture.mostRecentFrame()>=87);
    CHECK(capture.droppedSamplesCount()==0);capture.stop();
    std::cout<<"PASS LIVE callback -> FIFO -> worker -> automatic LTC recovery -> cue -> OSC UDP / MIDI sink; repeated pauses, selective/all reset, disarmed input, device restart\n";
}
int main() {
    for(int rate:{44100,48000,96000}) for(int fps:{24,25,30}) roundTrip(rate,fps);
    for(int rate:{44100,48000,96000})for(int fps:{24,25,30})testStreamRecovery(rate,fps);
    testLiveCaptureRecovery();
    rejectedSignals(); testSettings(); testMidi(); std::cout<<"All LTC / OSC / MIDI / settings integration scenarios passed\n";
}
