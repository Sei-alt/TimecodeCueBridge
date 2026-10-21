#pragma once
#include "MidiAction.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <array>
#include <mutex>
namespace tcb {
// Owns only this app's notes. Never sends global CC123 to unrelated senders.
class MidiOutputService final : private juce::Thread {
public:
    using Sink = std::function<void(const MidiPacket&)>;
    MidiOutputService();
    ~MidiOutputService() override;
    bool open(const juce::String& identifier);
    bool send(const MidiAction& action);
    bool ready() const;
    void panic();
    // Uses the exact dispatch/scheduling path without requiring physical MIDI hardware.
    void setTestSink(Sink sink);
private:
    void run() override;
    void emit(const MidiPacket&);
    void stopNotes(); // mutex held
    struct Note { bool active=false; double offAt=0; };
    mutable std::mutex mutex;
    std::unique_ptr<juce::MidiOutput> device;
    Sink testSink;
    std::array<std::array<Note,128>,16> notes{};
};
}
