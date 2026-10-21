#pragma once
#include "MidiAction.h"
#include <string>
#include <vector>
#include <optional>
#include <unordered_set>

namespace tcb {
struct Cue {
    std::string timecode;
    std::string label;
    std::string address;
    int value = 1;
    bool enabled = true;
    bool oscEnabled = true;
    bool midiEnabled = false;
    MidiAction midi;
};
struct Trigger { Cue cue; int frame = 0; size_t index = 0; };

// 24/25/30 integer FPS only. Drop-frame and 29.97 are explicitly unsupported in MVP.
std::optional<int> parseTimecode(const std::string& text, int fps);
std::string formatTimecode(int absoluteFrame, int fps);

class CueEngine {
public:
    void configure(std::vector<Cue> newCues, int newFps);
    std::vector<Trigger> process(int absoluteFrame);
    void signalLost(); // Does not implicitly re-arm cues already triggered.
    void resetAll();   // Explicit operator re-arm.
    bool resetCue(size_t index); // Does not change other scenes or the LTC lock.
    bool hasFired(size_t index) const {return fired.count(index)!=0;}
    std::vector<bool> firedStates() const;
    int frameRate() const { return fps; }
    // Shared output cleanup policy: seek/rewind cancels held MIDI notes.
    bool isDiscontinuity(int frame) const {
        return previous && (frame < *previous || frame - *previous > 2);
    }
private:
    int fps = 30;
    std::vector<Cue> cues;
    std::optional<int> previous;
    int consecutive = 0;
    std::unordered_set<size_t> fired;
};
}
