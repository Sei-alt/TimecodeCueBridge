#include "CueEngine.h"
#include <algorithm>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace tcb {
std::optional<int> parseTimecode(const std::string& s, const int fps) {
    if (fps != 24 && fps != 25 && fps != 30) return {};
    if (s.size() != 11 || s[2] != ':' || s[5] != ':' || s[8] != ':') return {};
    for (size_t i = 0; i < s.size(); ++i)
        if (i != 2 && i != 5 && i != 8 && (s[i] < '0' || s[i] > '9')) return {};
    const int h = (s[0] - '0') * 10 + (s[1] - '0');
    const int m = (s[3] - '0') * 10 + (s[4] - '0');
    const int sec = (s[6] - '0') * 10 + (s[7] - '0');
    const int f = (s[9] - '0') * 10 + (s[10] - '0');
    if (h >= 24 || m >= 60 || sec >= 60 || f >= fps) return {};
    return (((h * 60 + m) * 60 + sec) * fps + f);
}
std::string formatTimecode(const int n, const int fps) {
    if (n < 0 || fps < 1) return "--:--:--:--";
    const int f = n % fps;
    const int totalSeconds = n / fps;
    const int seconds = totalSeconds % 60;
    const int minutes = (totalSeconds / 60) % 60;
    const int hours = (totalSeconds / 3600) % 24;
    char out[32]{};
    std::snprintf(out, sizeof out, "%02d:%02d:%02d:%02d", hours, minutes, seconds, f);
    return out;
}
void CueEngine::configure(std::vector<Cue> newCues, int newFps) {
    if (newFps != 24 && newFps != 25 && newFps != 30) throw std::invalid_argument("FPS must be 24, 25 or 30");
    fps = newFps;
    cues = std::move(newCues);
    resetAll();
}
void CueEngine::resetAll() { previous.reset(); consecutive = 0; fired.clear(); }
void CueEngine::signalLost() { previous.reset(); consecutive = 0; }
bool CueEngine::resetCue(size_t index) {
    if(index>=cues.size())return false;
    fired.erase(index);return true;
}
std::vector<bool> CueEngine::firedStates() const {
    std::vector<bool> states(cues.size(),false);
    for(auto index:fired)states[index]=true;
    return states;
}
std::vector<Trigger> CueEngine::process(const int current) {
    std::vector<Trigger> result;
    if (current < 0 || current >= 24 * 3600 * fps) { signalLost(); return result; }
    if (!previous) { previous = current; consecutive = 1; return result; }
    if (current == *previous) return result;
    const int delta = current - *previous;
    if (delta < 0) {
        // Keep one-shot history on rewind, including a rewind across a dropout.
        // Operators explicitly choose Reset All or Rearm Scene for another take.
        previous = current;
        consecutive = 1;
        return result;
    }
    if (delta > 2) {
        // Jump forward: NEVER fire all skipped cues; require a new stable lock.
        previous = current;
        consecutive = 1;
        return result;
    }
    const int old = *previous;
    previous = current;
    consecutive = std::min(consecutive + 1, 3);
    if (consecutive < 3) return result;
    for (size_t i = 0; i < cues.size(); ++i) {
        const auto& cue = cues[i];
        if (!cue.enabled || fired.count(i)) continue;
        auto frame = parseTimecode(cue.timecode, fps);
        if (frame && old < *frame && *frame <= current) {
            fired.insert(i);
            result.push_back({ cue, *frame, i });
        }
    }
    return result;
}
}
