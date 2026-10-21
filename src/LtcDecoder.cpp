#include "LtcDecoder.h"
#include <cmath>
namespace tcb {
bool LtcDecoder::configure(double rate, int newFps) {
    decoder.reset(); offset = 0; rejected = false; fps = newFps;
    if (rate < 8000 || rate > 384000 || (fps != 24 && fps != 25 && fps != 30)) return false;
    decoder.reset(ltc_decoder_create(static_cast<int>(std::lround(rate / fps)), 128));
    return decoder != nullptr;
}
void LtcDecoder::write(float* data, size_t count, const Callback& callback) {
    if (!decoder) return;
    ltc_decoder_write_float(decoder.get(), data, count, offset);
    offset += static_cast<ltc_off_t>(count);
    LTCFrameExt frame{};
    while (ltc_decoder_read(decoder.get(), &frame)) {
        SMPTETimecode tc{}; ltc_frame_to_time(&tc, &frame.ltc, 0);
        // Reverse playback must never re-arm/fire cues. A forward rewind/seek is handled by CueEngine.
        if (frame.reverse || frame.ltc.dfbit || tc.hours > 23 || tc.mins > 59 || tc.secs > 59 || tc.frame >= fps) {
            rejected = true; callback(-1); continue;
        }
        rejected = false;
        callback((((tc.hours * 60 + tc.mins) * 60 + tc.secs) * fps + tc.frame));
    }
}
}
