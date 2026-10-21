#pragma once
#include <juce_core/juce_core.h>
#include <functional>
namespace tcb {
#if JUCE_MAC
void requestMicrophoneAccess(std::function<void(bool)> completion);
#else
inline void requestMicrophoneAccess(std::function<void(bool)> completion) { completion(true); }
#endif
}
