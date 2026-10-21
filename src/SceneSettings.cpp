#include "SceneSettings.h"
#include "CueText.h"
#include <limits>
namespace tcb {
bool validateScenes(const std::vector<Cue>& scenes,int fps,juce::String& error) {
    if(fps!=24 && fps!=25 && fps!=30) { error="FPS must be 24, 25 or 30"; return false; }
    if(scenes.size()>10000) {error="Maximum 10000 scenes";return false;}
    for(size_t i=0;i<scenes.size();++i) {
        const auto& c=scenes[i];
        const juce::String prefix="Scene "+juce::String(static_cast<int>(i+1))+": ";
        if(!parseTimecode(c.timecode,fps)) {error=prefix+"invalid timecode";return false;}
        if(c.label.empty()) {error=prefix+"name is empty";return false;}
        if(c.oscEnabled && !validOscAddress(c.address)) {error=prefix+"invalid OSC address";return false;}
        if(c.midiEnabled && !makeMidiPacket(c.midi)) {error=prefix+"invalid MIDI parameters";return false;}
        if(c.enabled && !c.oscEnabled && !c.midiEnabled) {error=prefix+"choose OSC and/or MIDI";return false;}
    }
    error.clear();return true;
}
juce::var encodeSettings(const SceneSettings& s) {
    auto root=std::make_unique<juce::DynamicObject>();
    root->setProperty("version",3);root->setProperty("host",s.host);root->setProperty("port",s.port);root->setProperty("fps",s.fps);
    root->setProperty("audio_state_xml",s.audioXml);root->setProperty("midi_device_id",s.midiIdentifier);root->setProperty("midi_device_name",s.midiName);
    juce::Array<juce::var> rows;
    for(const auto& c:s.scenes) {
        auto row=std::make_unique<juce::DynamicObject>();
        row->setProperty("timecode",juce::String(c.timecode));row->setProperty("name",juce::String(c.label));row->setProperty("enabled",c.enabled);
        row->setProperty("osc_enabled",c.oscEnabled);row->setProperty("osc_address",juce::String(c.address));row->setProperty("osc_value",c.value);
        row->setProperty("midi_enabled",c.midiEnabled);row->setProperty("midi_type",static_cast<int>(c.midi.kind));row->setProperty("midi_channel",c.midi.channel);
        row->setProperty("midi_number",c.midi.number);row->setProperty("midi_value",c.midi.value);row->setProperty("midi_gate_ms",c.midi.gateMs);
        rows.add(juce::var(row.release()));
    }
    root->setProperty("scenes",rows); return juce::var(root.release());
}
static bool integer(const juce::var& value,int& out) {
    if(!value.isInt() && !value.isInt64()) return false;
    const auto v=static_cast<juce::int64>(value);
    if(v<std::numeric_limits<int>::min() || v>std::numeric_limits<int>::max()) return false;
    out=static_cast<int>(v);return true;
}
bool decodeSettings(const juce::var& json,SceneSettings& result,juce::String& error) {
    error="Invalid settings JSON";
    if(!json.isObject()) return false;
    SceneSettings s;
    int version=1;
    if(!integer(json.getProperty("version",1),version) || version<1 || version>3) {error="Unsupported settings version";return false;}
    if(!integer(json.getProperty("fps",30),s.fps) || !integer(json.getProperty("port",9000),s.port)) return false;
    s.host=json.getProperty("host","127.0.0.1").toString().trim();
    s.audioXml=json.getProperty("audio_state_xml","").toString();
    s.midiIdentifier=json.getProperty("midi_device_id","").toString();s.midiName=json.getProperty("midi_device_name","").toString();
    if(s.host.isEmpty() || s.port<1 || s.port>65535) {error="Invalid OSC destination";return false;}
    if(version<3) {
        const auto lines=juce::StringArray::fromLines(json.getProperty("cues_text","").toString());
        for(int i=0;i<lines.size();++i) {
            auto line=lines[i].trim();if(line.isEmpty())continue;
            const bool disabled=line.startsWithChar('#');
            if(disabled) {
                line=line.substring(1).trim();
                if(line.length()<11 || !parseTimecode(line.substring(0,11).toStdString(),s.fps))continue;
            }
            std::vector<Cue> row;std::string e;
            if(!parseCueText(line.toStdString(),s.fps,row,e)) {
                if(disabled)continue;
                error="Legacy line "+juce::String(i+1)+": "+juce::String(e);return false;
            }
            row.front().enabled=!disabled;if(row.front().label.empty())row.front().label="Scene";
            s.scenes.push_back(row.front());
        }
    } else {
        const auto rows=json.getProperty("scenes",juce::var());
        const auto* array=rows.getArray();if(!array || array->size()>10000) return false;
        for(const auto& row:*array) {
            if(!row.isObject()) return false;
            Cue c;c.timecode=row.getProperty("timecode","").toString().toStdString();c.label=row.getProperty("name","").toString().toStdString();
            const auto enabled=row.getProperty("enabled",true),osc=row.getProperty("osc_enabled",true),midi=row.getProperty("midi_enabled",false);
            if(!enabled.isBool() || !osc.isBool() || !midi.isBool()) return false;
            c.enabled=static_cast<bool>(enabled);c.oscEnabled=static_cast<bool>(osc);c.midiEnabled=static_cast<bool>(midi);
            c.address=row.getProperty("osc_address","/cue/1").toString().toStdString();
            int kind=0;
            if(!integer(row.getProperty("osc_value",1),c.value) || !integer(row.getProperty("midi_type",0),kind)
                || !integer(row.getProperty("midi_channel",1),c.midi.channel) || !integer(row.getProperty("midi_number",60),c.midi.number)
                || !integer(row.getProperty("midi_value",100),c.midi.value) || !integer(row.getProperty("midi_gate_ms",100),c.midi.gateMs)) return false;
            c.midi.kind=static_cast<MidiKind>(kind);
            // Validate MIDI even if disabled, so a corrupt payload cannot hide behind a checkbox.
            if(!makeMidiPacket(c.midi)) {error="Invalid saved MIDI parameters";return false;}
            s.scenes.push_back(std::move(c));
        }
    }
    if(!validateScenes(s.scenes,s.fps,error)) return false;
    result=std::move(s);error.clear();return true;
}
}
