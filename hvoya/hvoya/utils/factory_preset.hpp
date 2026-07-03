#pragma once

/* factory_preset.hpp — author factory presets for iPlug2 plugins that override the
 * state-chunk format.
 *
 * WHY
 * ---
 * IPluginBase::MakePresetFromNamedParams stores a preset as a raw array of NParams
 * doubles. That layout matches ONLY the default UnserializeState (= UnserializeParams).
 * A plugin that overrides SerializeState / UnserializeState with its own layout (a
 * version header, a MIDI-CC block, count-prefixed params, extra UI / feature blocks, ...)
 * will MIS-PARSE such a preset on recall: it reads the raw param bytes as its header and
 * first block, the embedded counts come out as garbage, and unserialize typically asserts
 * or crashes.
 *
 * This helper instead builds the preset chunk with the plugin's OWN SerializeState — i.e.
 * it lets the plugin "write the chunk the way it needs" — so factory presets round-trip
 * through the same format as project / .fxp state. It keeps the readable "param -> value"
 * authoring style: pass an initializer list of { paramId, value } pairs.
 *
 * USAGE
 * -----
 *   void MyPlugin::initPresets() {
 *       using namespace hvoya;
 *       makeChunkPreset(*this, "clean", {
 *           { par_cutoff, 4078.1 },
 *           { par_reso,      0.5  },
 *           { par_enabled,   1.0  },   // bool/int/enum: pass the real value as a double
 *       });
 *   }
 *
 * PluginT must be an IPluginBase subclass (uses GetParam / NParams / SerializeState /
 * MakePresetFromChunk). Only the named params are set; the rest keep their current value
 * (typically the init defaults), exactly like MakePresetFromNamedParams. The live patch is
 * snapshotted and restored, so authoring presets never disturbs the running plugin.
 *
 * SHARED UTILITY: used by multiple plugins — keep it generic and backward-compatible.
 */

#include <IPlugPluginBase.h>

#include <cassert>
#include <cmath>
#include <format>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace hvoya {

    // One (param id, real value) authoring entry. `value` is the denormalized value;
    // bool/int/enum params take their integer value widened to double (IParam::Set maps it
    // back correctly). Mirrors the readable param -> value preset-dump style.
    struct PresetParamValue {
        int    id    = 0;
        double value = 0.0;
    };

    // Build a factory preset whose chunk is written by the plugin's own SerializeState.
    // Returns true if the state serialized successfully. See the file header for why this
    // exists (raw-double presets crash plugins with a custom chunk format).
    template <typename PluginT>
    bool makeChunkPreset (PluginT& plugin, std::string_view name,
                          std::initializer_list<PresetParamValue> values) {
        const int n = plugin.NParams();

        // Snapshot the live patch so authoring a preset never disturbs the running plugin.
        std::vector<double> saved (static_cast<size_t> (n));
        for (int i = 0; i < n; ++i)
            saved[static_cast<size_t> (i)] = plugin.GetParam (i)->Value();

        for (const auto& pv : values) {
            assert (pv.id >= 0 && pv.id < n);
            if (pv.id >= 0 && pv.id < n)
                plugin.GetParam (pv.id)->Set (pv.value);
        }

        iplug::IByteChunk chunk;
        const bool ok = plugin.SerializeState (chunk);
        const std::string nameStr (name);            // null-terminate for the iPlug API boundary
        plugin.MakePresetFromChunk (nameStr.c_str(), chunk);

        for (int i = 0; i < n; ++i)
            plugin.GetParam (i)->Set (saved[static_cast<size_t> (i)]);

        return ok;
    }

    // Format a param's current value for the makeChunkPreset source: bool/int/enum as an
    // integer, double with 6 decimals (matching the readable dump style).
    inline std::string formatPresetParamValue (const iplug::IParam* p) {
        switch (p->Type()) {
            case iplug::IParam::kTypeBool:                  return p->Bool() ? "1" : "0";
            case iplug::IParam::kTypeInt:
            case iplug::IParam::kTypeEnum:                  return std::to_string (p->Int());
            case iplug::IParam::kTypeDouble:
            default:                                        return std::format ("{:.6f}", p->Value());
        }
    }

    // Generate copy-paste-ready C++ source for a makeChunkPreset(...) call capturing the
    // plugin's CURRENT param values. Params still at their default are SKIPPED, so the
    // output is the minimal set of lines (unlisted params fall back to defaults on recall).
    // `paramName` maps a param index to its identifier token (e.g. par_main_mix).
    template <typename PluginT>
    std::string makeChunkPresetSrc (const PluginT& plugin,
                                    std::function<std::string_view(int)> paramName,
                                    std::string_view presetName = "TODO: name me") {
        std::string s = std::format ("makeChunkPreset(*this, \"{}\", {{\n", presetName);
        const int n = plugin.NParams();
        for (int i = 0; i < n; ++i) {
            const auto* p = plugin.GetParam (i);
            if (std::abs (p->Value() - p->GetDefault()) <= 1e-9) continue;   // skip defaults
            s += std::format ("    {{ {}, {} }},\n", paramName (i), formatPresetParamValue (p));
        }
        s += "});\n";
        return s;
    }

} // namespace hvoya
