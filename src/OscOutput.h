#pragma once
#include "CueEngine.h"
#include <juce_osc/juce_osc.h>
namespace tcb {
// Worker-thread only. Retains one UDP socket instead of reconnecting for every cue.
class OscOutput {
public:
    bool connect(const juce::String& host, int port);
    bool send(const Cue& cue);
private:
    juce::OSCSender sender;
    bool connected = false;
};
}
