#include "CueText.h"
#include <charconv>
#include <sstream>
#include <cstdint>
namespace tcb {
static std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
bool validOscAddress(const std::string& s) {
    if (s.size() < 2 || s.front() != '/' || s.back() == '/') return false;
    for (const unsigned char c : s)
        if (c <= 32 || c >= 127 || std::string("*?[]{},#|").find(static_cast<char>(c)) != std::string::npos) return false;
    return s.find("//") == std::string::npos;
}
bool parseCueText(const std::string& text, int fps, std::vector<Cue>& cues, std::string& error) {
    cues.clear(); error.clear();
    std::istringstream lines(text); std::string line; int row = 0;
    while (std::getline(lines, line)) {
        ++row; line = trim(line);
        if (line.empty() || line.front() == '#') continue;
        std::vector<std::string> fields; size_t start = 0;
        for (;;) {
            const auto end = line.find('|', start);
            fields.push_back(trim(line.substr(start, end == std::string::npos ? end : end - start)));
            if (end == std::string::npos) break;
            start = end + 1;
        }
        auto fail = [&](const char* why) { error = "Line " + std::to_string(row) + ": " + why; cues.clear(); return false; };
        if (fields.size() != 4) return fail("expected timecode | label | /address | int32");
        if (!parseTimecode(fields[0], fps)) return fail("invalid timecode or FPS");
        if (!validOscAddress(fields[2])) return fail("invalid OSC address (ASCII literal path required)");
        int32_t value = 0;
        const auto parsed = std::from_chars(fields[3].data(), fields[3].data() + fields[3].size(), value);
        if (fields[3].empty() || parsed.ec != std::errc{} || parsed.ptr != fields[3].data() + fields[3].size()) return fail("int32 required (-2147483648 to 2147483647)");
        cues.push_back({fields[0], fields[1], fields[2], value, true});
        if (cues.size() > 10000) return fail("maximum 10000 cues");
    }
    if (cues.empty()) { error = "No cues configured"; return false; }
    return true;
}
}
