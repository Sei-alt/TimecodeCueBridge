#include "LtcStream.h"
namespace tcb {
bool LtcStream::configure(double sampleRate,int fps,std::uint32_t now) {
    rate=sampleRate;frameRate=fps;recoveryAt=now;samplesWithoutFrame=0;locked=false;rejected=false;
    configured=decoder.configure(rate,frameRate);return configured;
}
void LtcStream::recover(std::uint32_t now,const Callback& callback) {
    // Discard partial bits, old phase/speed tracking and stale frames. Repeat while
    // lost so noise during a pause cannot leave the parser permanently off speed.
    const bool notify=locked;
    configured=decoder.configure(rate,frameRate);recoveryAt=now;samplesWithoutFrame=0;locked=false;
    if(notify)callback(-1);
}
void LtcStream::poll(std::uint32_t now,const Callback& callback) {
    if(configured && static_cast<std::uint32_t>(now-recoveryAt)>=250)recover(now,callback);
}
void LtcStream::process(float* samples,size_t count,std::uint32_t now,const Callback& callback) {
    poll(now,callback);
    if(!configured)return;
    // Audio-time fallback covers queued audio and fast/offline input as well as
    // a frozen or coarse wall clock. Never depend on a user pressing Reset Cues.
    if(samplesWithoutFrame>=static_cast<size_t>(rate*.25))recover(now,callback);
    samplesWithoutFrame+=count;
    decoder.write(samples,count,[&](int frame){
        if(frame<0) {
            if(locked || !rejected)callback(-1);
            locked=false;rejected=true;return;
        }
        locked=true;rejected=false;samplesWithoutFrame=0;recoveryAt=now;callback(frame);
    });
}
}
