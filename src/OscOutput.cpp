#include "OscOutput.h"
#include "CueText.h"
namespace tcb {
bool OscOutput::connect(const juce::String& host, int port) {
    sender.disconnect();
    connected = host.isNotEmpty() && port > 0 && port <= 65535 && sender.connect(host, port);
    return connected;
}
bool OscOutput::send(const Cue& cue) {
    if (!connected || !validOscAddress(cue.address)) return false;
    return sender.send(juce::OSCMessage(juce::OSCAddressPattern(juce::String(cue.address)), static_cast<juce::int32>(cue.value)));
}
}
