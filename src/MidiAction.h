#pragma once
#include <array>
#include <optional>
namespace tcb {
enum class MidiKind { noteOn = 0, noteOff = 1, controlChange = 2 };
struct MidiAction {
    MidiKind kind = MidiKind::noteOn;
    int channel = 1;
    int number = 60;
    int value = 100;
    int gateMs = 100; // Note On: 0 holds until explicit Off / disarm; otherwise automatic Off.
};
using MidiPacket = std::array<unsigned char,3>;
std::optional<MidiPacket> makeMidiPacket(const MidiAction& action);
}
