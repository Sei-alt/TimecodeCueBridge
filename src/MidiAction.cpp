#include "MidiAction.h"
namespace tcb {
std::optional<MidiPacket> makeMidiPacket(const MidiAction& a) {
    if(a.channel < 1 || a.channel > 16 || a.number < 0 || a.number > 127 || a.value < 0 || a.value > 127 || a.gateMs < 0 || a.gateMs > 60000) return {};
    unsigned char status=0;
    switch(a.kind) {
        case MidiKind::noteOn: status=0x90; break;
        case MidiKind::noteOff: status=0x80; break;
        case MidiKind::controlChange: status=0xb0; break;
        default: return {};
    }
    return MidiPacket{static_cast<unsigned char>(status+a.channel-1),static_cast<unsigned char>(a.number),static_cast<unsigned char>(a.value)};
}
}
