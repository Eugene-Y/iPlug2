#pragma once

/* mapper_hvoya_adapter.hpp
 *
 * Pure data conversion between CCtoParamMap_t and the [midi-cc] section
 * of a .hvoya file. Knows nothing about CCMediator or plugin state.
 *
 * Section format — one line per param mapping:
 *   cc paramId minVal maxVal channel
 *   74 5 0 1 0
 *   71 12 0.2 0.8 1
 */

#include "mapper.hpp"
#include <hvoya/utils/hvoya_file.hpp>

#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <vector>


namespace hvoya::midi_cc {

struct MapperHvoyaAdapter {

    static constexpr const char* kSectionName = "midi-cc";

    // ── CCtoParamMap_t → HvoyaFile section ───────────────────────────────────

    static std::vector<std::string> toLines (const CCtoParamMap_t& map) {
        std::vector<std::string> lines;
        for (const auto& [cc, params] : map) {
            for (const auto& p : params) {
                std::ostringstream oss;
                oss << cc << " " << p.paramId << " "
                    << std::setprecision(10) << p.minVal << " " << p.maxVal
                    << " " << p.channel;
                lines.push_back (oss.str());
            }
        }
        return lines;
    }

    static void writeSection (const CCtoParamMap_t& map, HvoyaFile& file) {
        file.setSection (kSectionName, toLines (map));
    }

    // ── HvoyaFile section → CCtoParamMap_t ───────────────────────────────────

    // Returns nullopt if the section is absent; otherwise a (possibly empty) map.
    // Silently skips malformed or out-of-range lines.
    static std::optional<CCtoParamMap_t> readSection (const HvoyaFile& file) {
        if (!file.hasSection (kSectionName))
            return std::nullopt;
        return fromLines (file.section (kSectionName));
    }

    static CCtoParamMap_t fromLines (const std::vector<std::string>& lines) {
        CCtoParamMap_t map;
        for (const auto& line : lines) {
            std::istringstream iss (line);
            CC_t   cc      = 0;
            PId_t  paramId = 0;
            double minVal  = 0.0;
            double maxVal  = 1.0;
            int    channel = 0;
            if (!(iss >> cc >> paramId >> minVal >> maxVal >> channel))
                continue;
            if (cc < 0 || cc >= 128 || paramId < 0)
                continue;
            ParamCCMapping pm (paramId, cc);
            pm.minVal  = minVal;
            pm.maxVal  = maxVal;
            pm.channel = channel;
            map[cc].push_back (pm);
        }
        return map;
    }

};

} // namespace hvoya::midi_cc
