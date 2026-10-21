#include "LtcCapture.h"
#include "LtcStream.h"
#include <array>
#include <cstring>
#include <cmath>
LtcCapture::LtcCapture(FrameCallback callback, std::atomic<int>& fps)
    : juce::Thread("LTC Decode Worker"), onFrame(std::move(callback)), fpsSetting(fps) {}
LtcCapture::~LtcCapture() { stop(); }
juce::String LtcCapture::start(const juce::XmlElement* state) {
    if (started) return {};
    const auto error = deviceManager.initialise(1, 0, state, true);
    started = true;
    startThread();
    deviceManager.addAudioCallback(this);
    return error;
}
void LtcCapture::stop() {
    if (!started) return;
    deviceManager.removeAudioCallback(this);
    signalThreadShouldExit();
    // Join before callback targets (logger, status and engine) are destroyed.
    waitForThreadToExit(-1);
    deviceManager.closeAudioDevice();
    started = false;
}
void LtcCapture::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    sampleRate.store(device ? device->getCurrentSampleRate() : 0.0); invalidate();
}
void LtcCapture::audioDeviceStopped() { sampleRate.store(0.0); invalidate(); }
void LtcCapture::audioDeviceIOCallbackWithContext(const float* const* inputs, int nInputs,
    float* const* outputs, int nOutputs, int count, const juce::AudioIODeviceCallbackContext&) {
    for (int c = 0; c < nOutputs; ++c)
        if (outputs[c]) juce::FloatVectorOperations::clear(outputs[c], count);
    // JUCE supplies all hardware channels; inactive channels are null. Require exactly one active channel.
    const float* selected = nullptr;
    for (int c = 0; c < nInputs; ++c) if (inputs[c]) {
        if (selected) { peak.store(0); invalidate(); return; }
        selected = inputs[c];
    }
    if (!selected) { peak.store(0); return; }
    float level = 0;
    for (int i = 0; i < count; ++i) level = std::max(level, std::abs(selected[i]));
    peak.store(level);
    int a=0, n=0, b=0, m=0;
    fifo.prepareToWrite(count, a, n, b, m);
    if (n) std::memcpy(sampleRing.data()+a, selected, sizeof(float)*static_cast<size_t>(n));
    if (m) std::memcpy(sampleRing.data()+b, selected+n, sizeof(float)*static_cast<size_t>(m));
    fifo.finishedWrite(n+m);
    if (n+m < count) { droppedSamples.fetch_add(static_cast<unsigned int>(count-n-m)); invalidate(); }
}
void LtcCapture::run() {
    tcb::LtcStream stream;
    std::array<float, 4096> chunk{};
    double activeRate = -1; int activeFps = 0; unsigned int activeGeneration = ~0u;
    auto deliver=[this](int frame){
        mostRecent.store(frame);
        lastSeen.store(frame>=0?juce::Time::getMillisecondCounter():0);
        onFrame(frame);
    };
    while (!threadShouldExit()) {
        const auto now = juce::Time::getMillisecondCounter();
        const double rate = sampleRate.load(); const int fps = fpsSetting.load();
        const auto gen = generation.load();
        if (!juce::approximatelyEqual(rate,activeRate) || fps != activeFps || gen != activeGeneration) {
            activeRate = rate; activeFps = fps; activeGeneration = gen;
            // Consumer-only discard; never reset an AbstractFifo while its producer runs.
            fifo.finishedRead(fifo.getNumReady());
            stream.configure(rate, fps, now);
            mostRecent.store(-1); lastSeen.store(0); rejectedFrame.store(false);
            onFrame(-1);
        }
        stream.poll(now,deliver);
        int a=0,n=0,b=0,m=0;
        fifo.prepareToRead(static_cast<int>(chunk.size()), a,n,b,m);
        if (!n && !m) { juce::Thread::sleep(2); continue; }
        if (n) std::memcpy(chunk.data(), sampleRing.data()+a, sizeof(float)*static_cast<size_t>(n));
        if (m) std::memcpy(chunk.data()+n, sampleRing.data()+b, sizeof(float)*static_cast<size_t>(m));
        fifo.finishedRead(n+m);
        stream.process(chunk.data(), static_cast<size_t>(n+m), now, [&](int frame) {
            // Invalidate old decoder output if UI changed device/FPS during this chunk.
            if (generation.load() != activeGeneration || fpsSetting.load() != activeFps) return;
            deliver(frame);
        });
        rejectedFrame.store(stream.rejectedFrame());
    }
}
