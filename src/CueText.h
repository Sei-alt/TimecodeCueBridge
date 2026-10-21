#pragma once
#include "CueEngine.h"
namespace tcb {
bool validOscAddress(const std::string& address);
bool parseCueText(const std::string& text, int fps, std::vector<Cue>& cues, std::string& error);
}
