#include "CueEngine.h"
#include "CueText.h"
#include <stdexcept>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace tcb;

// Do not use assert(): release builds define NDEBUG and would silently skip tests.
static void check(bool ok, const char* condition, int line) {
    if (!ok) {
        std::cerr << "FAIL line " << line << ": " << condition << '\n';
        std::exit(EXIT_FAILURE);
    }
}
#define CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

static Cue cue(const char* tc, const char* name, bool enabled=true) {
    return {tc, name, "/cue/test", 1, enabled};
}

static void testTimecode() {
    CHECK(parseTimecode("01:02:03:04", 30).value() == (((1*60+2)*60+3)*30+4));
    CHECK(parseTimecode("23:59:59:29", 30).has_value());
    CHECK(!parseTimecode("01:02:03:30", 30));
    CHECK(!parseTimecode("01:02:03;20", 30));
    CHECK(!parseTimecode("24:00:00:00", 30));
    CHECK(!parseTimecode("00:60:00:00", 30));
    CHECK(!parseTimecode("00:00:60:00", 30));
    CHECK(!parseTimecode("00:00:00:24", 24));
    CHECK(!parseTimecode("00:00:00:25", 25));
    CHECK(!parseTimecode("00:00:00:00", 29));
    CHECK(formatTimecode(111724, 30) == "01:02:04:04");
    CHECK(formatTimecode(-1, 30) == "--:--:--:--");
}

static void testForwardOneShot() {
    CueEngine e;
    e.configure({cue("00:00:00:04", "First"), cue("00:00:01:00", "Next")}, 30);
    for (int i=0;i<4;++i) CHECK(e.process(i).empty());
    auto first = e.process(4);
    CHECK(first.size() == 1 && first[0].cue.label == "First" && first[0].frame == 4);
    CHECK(e.process(4).empty());  // repeated LTC frame must never refire
    for (int i=5;i<30;++i) CHECK(e.process(i).empty());
    auto next = e.process(30);
    CHECK(next.size() == 1 && next[0].cue.label == "Next" && next[0].frame == 30);
    CHECK(e.process(31).empty());
    e.signalLost();
    CHECK(e.process(30).empty());
    CHECK(e.process(31).empty());
    CHECK(e.process(32).empty()); // no duplicate after audio input dropout
}

static void testRearmAndJump() {
    CueEngine e;
    e.configure({cue("00:00:00:04", "First"), cue("00:00:01:00", "Next")}, 30);
    for (int i=0;i<=31;++i) e.process(i);
    CHECK(e.process(3).empty());  // reverse seek preserves history and reacquires lock
    CHECK(e.process(4).empty());  // 1 stable frame is insufficient
    CHECK(e.process(5).empty());
    CHECK(e.process(6).empty());
    CHECK(e.process(100).empty()); // large forward jump must not trigger skipped cues
    e.resetAll();
    CHECK(e.process(0).empty());
    CHECK(e.process(1).empty());
    CHECK(e.process(2).empty());
    auto output = e.process(4);    // small 2-frame gap is permitted
    CHECK(output.size() == 1 && output[0].frame == 4);
}

static void testDisabledAndMissedCues() {
    CueEngine e;
    e.configure({cue("00:00:00:04", "Disabled", false),
                 cue("00:00:00:04", "Enabled")}, 25);
    for (int i=0;i<=4;++i) {
        const auto emitted = e.process(i);
        if (i == 4) CHECK(emitted.size() == 1 && emitted[0].cue.label == "Enabled");
        else CHECK(emitted.empty());
    }
    CueEngine skipped;
    skipped.configure({cue("00:00:02:00", "Skipped")}, 30);
    CHECK(skipped.process(0).empty());
    CHECK(skipped.process(1).empty());
    CHECK(skipped.process(2).empty());
    CHECK(skipped.process(120).empty());
    CHECK(skipped.process(121).empty());
    CHECK(skipped.process(122).empty());
    CHECK(skipped.process(123).empty());
}

static void testParserAndRates() {
    std::vector<Cue> out; std::string error;
    CHECK(parseCueText("# disabled\n00:00:00:04 | Japanese label | /cue/test | -2147483648\n",30,out,error));
    CHECK(out.size()==1 && out.front().value==(-2147483647-1));
    CHECK(!parseCueText("00:00:00:04 | x | /cue | 2147483648",30,out,error));
    CHECK(!parseCueText("00:00:00:04 | x | /cue | 1x",30,out,error));
    CHECK(!parseCueText("00:00:00:04 | x | /cue/* | 1",30,out,error));
    CHECK(!parseCueText("00:00:00:04 | x | /cue | ",30,out,error));
    CHECK(!parseCueText("# only comments",30,out,error));
    CHECK(!validOscAddress("/foo//bar")); CHECK(!validOscAddress("/foo\t"));
    CHECK(!validOscAddress("/foo/")); CHECK(validOscAddress("/foo/bar-2"));
    for(int f:{24,25,30}) {
        CueEngine engine; engine.configure({cue("00:00:00:04","rate")},f);
        int count=0; for(int i=0;i<8;++i) count+=static_cast<int>(engine.process(i).size());
        CHECK(count==1);
        CHECK(engine.process(-1).empty()); CHECK(engine.process(24*3600*f).empty());
        CHECK(formatTimecode(parseTimecode("23:59:59:"+std::to_string(f-1),f).value(),f)=="23:59:59:"+std::to_string(f-1));
    }
    CueEngine discontinuity;discontinuity.configure({},30);
    CHECK(!discontinuity.isDiscontinuity(100));discontinuity.process(100);
    CHECK(!discontinuity.isDiscontinuity(100));CHECK(!discontinuity.isDiscontinuity(102));
    CHECK(discontinuity.isDiscontinuity(99));CHECK(discontinuity.isDiscontinuity(103));
    discontinuity.signalLost();CHECK(!discontinuity.isDiscontinuity(0));
    bool threw=false; try { CueEngine e; e.configure({},29); } catch(const std::invalid_argument&) { threw=true; } CHECK(threw);
}
static void testMidiPackets() {
    CHECK(makeMidiPacket({MidiKind::noteOn,1,60,100,100}).value()==(MidiPacket{0x90,60,100}));
    CHECK(makeMidiPacket({MidiKind::noteOff,16,127,0,0}).value()==(MidiPacket{0x8f,127,0}));
    CHECK(makeMidiPacket({MidiKind::controlChange,16,0,127,0}).value()==(MidiPacket{0xbf,0,127}));
    CHECK(!makeMidiPacket({MidiKind::noteOn,0,60,100,100}));
    CHECK(!makeMidiPacket({MidiKind::noteOn,17,60,100,100}));
    CHECK(!makeMidiPacket({MidiKind::noteOn,1,128,100,100}));
    CHECK(!makeMidiPacket({MidiKind::noteOn,1,60,128,100}));
    CHECK(!makeMidiPacket({MidiKind::noteOn,1,60,100,-1}));
    CHECK(!makeMidiPacket({static_cast<MidiKind>(9),1,60,100,100}));
}
static void testSelectiveResetAndHistory() {
    CueEngine e;e.configure({cue("00:00:00:10","A"),cue("00:00:00:20","B"),cue("00:00:00:20","C")},30);
    for(int i=0;i<30;++i)e.process(i);
    CHECK(e.firedStates()==std::vector<bool>({true,true,true}));
    e.signalLost();CHECK(e.hasFired(0) && e.hasFired(1));
    for(int i=0;i<30;++i)CHECK(e.process(i).empty());
    // Rewind without signal loss also preserves every completed cue.
    for(int i=0;i<30;++i)CHECK(e.process(i).empty());
    CHECK(e.resetCue(1));CHECK(!e.resetCue(3));CHECK(!e.resetCue(static_cast<size_t>(-1)));
    CHECK(e.firedStates()==std::vector<bool>({true,false,true}));
    int triggered=0;for(int i=0;i<30;++i)for(const auto& t:e.process(i)){CHECK(t.index==1 && t.cue.label=="B");++triggered;}
    CHECK(triggered==1);CHECK(e.firedStates()==std::vector<bool>({true,true,true}));
    e.resetAll();CHECK(e.firedStates()==std::vector<bool>({false,false,false}));
    triggered=0;for(int i=0;i<30;++i)triggered+=static_cast<int>(e.process(i).size());CHECK(triggered==3);
    CHECK(e.resetCue(0));CHECK(e.process(30).empty()); // Rearm never immediately fires a past scene.
    CHECK(!e.hasFired(0) && e.hasFired(1));
}
int main() {
    testSelectiveResetAndHistory();
    testMidiPackets();
    testParserAndRates();
    testTimecode();
    testForwardOneShot();
    testRearmAndJump();
    testDisabledAndMissedCues();
    std::cout << "Cue engine release-safe tests passed (7 groups)\n";
}
