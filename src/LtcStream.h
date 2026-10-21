#pragma once
#include "LtcDecoder.h"
#include <cstdint>
namespace tcb {
// Decoder recovery belongs to input acquisition, independently of cue history.
// Used by the live worker and by deterministic waveform tests.
class LtcStream {
public:
    using Callback=LtcDecoder::Callback;
    bool configure(double sampleRate,int fps,std::uint32_t now);
    void process(float* samples,size_t count,std::uint32_t now,const Callback&);
    void poll(std::uint32_t now,const Callback&); // Also works when audio callbacks stop.
    bool rejectedFrame() const {return rejected;}
private:
    void recover(std::uint32_t now,const Callback&);
    LtcDecoder decoder;
    double rate=0;
    int frameRate=30;
    std::uint32_t recoveryAt=0;
    size_t samplesWithoutFrame=0;
    bool configured=false,locked=false,rejected=false;
};
}
