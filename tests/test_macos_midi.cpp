#include "MidiOutput.h"
#include <condition_variable>
#include <iostream>
#include <vector>
#include <chrono>

// Uses an actual CoreMIDI virtual destination, not MidiOutputService's test sink.
class Receiver final : public juce::MidiInputCallback {
public:
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message) override {
        if (message.getRawDataSize() != 3) return;
        const auto* data = message.getRawData();
        { std::lock_guard<std::mutex> lock(mutex); packets.push_back({data[0], data[1], data[2]}); }
        changed.notify_all();
    }
    bool waitFor(size_t count) {
        std::unique_lock<std::mutex> lock(mutex);
        return changed.wait_for(lock, std::chrono::seconds(5), [&] { return packets.size() >= count; });
    }
    bool matches(const std::vector<tcb::MidiPacket>& expected) {
        std::lock_guard<std::mutex> lock(mutex); return packets == expected;
    }
private:
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<tcb::MidiPacket> packets;
};
int main() {
    juce::ScopedJuceInitialiser_GUI initialiser;
    Receiver receiver;
    const auto name = "TCB Test " + juce::Uuid().toString();
    auto input = juce::MidiInput::createNewDevice(name, &receiver);
    if (!input) { std::cerr << "Cannot create CoreMIDI virtual destination\n"; return 1; }
    input->start();
    juce::String identifier;
    for (int attempt = 0; attempt < 100 && identifier.isEmpty(); ++attempt) {
        for (const auto& port : juce::MidiOutput::getAvailableDevices())
            if (port.name == name) identifier = port.identifier;
        // JUCE refreshes its CoreMIDI endpoint cache on the message thread.
        // Sleeping here prevents discovery of the destination just created above.
        if (identifier.isEmpty()) juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    }
    tcb::MidiOutputService output;
    const bool opened = identifier.isNotEmpty() && output.open(identifier);
    if (!opened) std::cerr << "Virtual destination discovery/open failed: " << identifier << '\n';
    bool ok = opened && output.send({tcb::MidiKind::noteOn, 2, 60, 100, 40}) && receiver.waitFor(2);
    ok = ok && output.send({tcb::MidiKind::controlChange, 2, 10, 64, 0}) && receiver.waitFor(3);
    ok = ok && output.send({tcb::MidiKind::noteOn, 2, 61, 90, 0}) && receiver.waitFor(4);
    output.panic();
    ok = ok && receiver.waitFor(5) && receiver.matches({{0x91,60,100},{0x81,60,0},{0xb1,10,64},{0x91,61,90},{0x81,61,0}});
    input->stop();
    std::cout << (ok ? "PASS" : "FAIL") << ": CoreMIDI virtual routing, gate Note Off, CC, panic\n";
    return ok ? 0 : 1;
}
