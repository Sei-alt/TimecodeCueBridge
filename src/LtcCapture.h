#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <functional>
#include <vector>
class LtcCapture final : private juce::AudioIODeviceCallback, private juce::Thread {
public:
    using FrameCallback = std::function<void(int)>;
    explicit LtcCapture(FrameCallback callback, std::atomic<int>& fps);
    ~LtcCapture() override;
    juce::String start(const juce::XmlElement* savedState = nullptr);
    void stop();
    juce::AudioDeviceManager& manager() { return deviceManager; }
    int mostRecentFrame() const { return mostRecent.load(); }
    unsigned int lastSeenMillis() const { return lastSeen.load(); }
    bool sawRejectedFrame() const { return rejectedFrame.load(); }
    double inputSampleRate() const { return sampleRate.load(); }
    float inputPeak() const { return peak.load(); }
    unsigned int droppedSamplesCount() const { return droppedSamples.load(); }
    void invalidate() { generation.fetch_add(1); }
private:
    friend class LtcCaptureTestAccess;
    void run() override;
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const*, int, int,
        const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    juce::AudioDeviceManager deviceManager;
    juce::AbstractFifo fifo { 1 << 17 };
    std::vector<float> sampleRing = std::vector<float>(1 << 17, 0.0f);
    FrameCallback onFrame;
    std::atomic<int>& fpsSetting;
    std::atomic<double> sampleRate { 0.0 };
    std::atomic<int> mostRecent { -1 };
    std::atomic<unsigned int> lastSeen { 0 }, generation { 0 }, droppedSamples { 0 };
    std::atomic<bool> rejectedFrame { false };
    std::atomic<float> peak { 0.0f };
    bool started = false;
};
