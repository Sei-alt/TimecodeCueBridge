#pragma once
#include "CueEngine.h"
#include <juce_core/juce_core.h>
namespace tcb {
struct SceneSettings {
    std::vector<Cue> scenes;
    juce::String host="127.0.0.1", audioXml, midiIdentifier, midiName;
    int port=9000, fps=30;
};
bool validateScenes(const std::vector<Cue>& scenes,int fps,juce::String& error);
juce::var encodeSettings(const SceneSettings& settings);
bool decodeSettings(const juce::var& json,SceneSettings& settings,juce::String& error);
}
