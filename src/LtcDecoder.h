#pragma once
#include <ltc.h>
#include <functional>
#include <memory>
namespace tcb {
// Shared by the live audio worker and waveform/UDP integration tests.
class LtcDecoder {
public:
    using Callback = std::function<void(int)>; // -1 invalidates signal lock
    bool configure(double sampleRate, int fps);
    void write(float* samples, size_t count, const Callback& callback);
    bool rejectedFrame() const { return rejected; }
private:
    struct Free { void operator()(LTCDecoder* d) const { if (d) ltc_decoder_free(d); } };
    std::unique_ptr<LTCDecoder, Free> decoder;
    ltc_off_t offset = 0;
    int fps = 30;
    bool rejected = false;
};
}
