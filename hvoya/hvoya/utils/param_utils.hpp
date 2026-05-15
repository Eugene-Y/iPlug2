#pragma once

#include <memory>
#include <vector>
#include <IPlugParameter.h>


namespace hvoya::params {

    // Describes one display segment for makeSegmentedDisplay.
    // Values < upTo are formatted as: (value / divisor) with decimals places, followed by unit.
    struct DisplaySegment {
        double      upTo;      // exclusive upper bound; use 1e9 (or similar) for the last segment
        double      divisor;   // divide value by this before formatting (1.0 = no change)
        const char* unit;      // suffix appended after a space ("ms", "s", "Hz", "kHz", …)
        int         decimals;  // decimal places
    };

    // Builds an IParam::DisplayFunc that switches format based on the parameter value.
    // Segments are checked in order; the first one with value < upTo is used.
    // If no segment matches, the last segment is used as a fallback.
    //
    // Example — time parameter stored in ms, range 0.1 – 3000 ms:
    //   makeSegmentedDisplay({
    //       {  10.0,    1.0, "ms", 1 },   // 0.1 – 9.9 ms  → "1.5 ms"
    //       {1000.0,    1.0, "ms", 0 },   // 10 – 999 ms    → "42 ms"
    //       {  1e9,  1000.0,  "s", 1 },   // 1000+ ms       → "1.2 s"
    //   })
    inline IParam::DisplayFunc makeSegmentedDisplay (std::initializer_list <DisplaySegment> segments)
    {
        std::vector <DisplaySegment> segs (segments);
        return [segs = std::move (segs)](double value, WDL_String& str) {
            for (const auto& seg : segs) {
                if (value < seg.upTo) {
                    str.SetFormatted(64, "%.*f %s", seg.decimals, value / seg.divisor, seg.unit);
                    return;
                }
            }
            const auto& last = segs.back();
            str.SetFormatted(64, "%.*f %s", last.decimals, value / last.divisor, last.unit);
        };
    }


    // ── Shape factories ───────────────────────────────────────────────────────
    // Return unique_ptr<Shape> for use in ParamDefDouble::shape.
    // Null shape (default) means ShapeLinear.

    inline std::unique_ptr<IParam::Shape> shapeLinear()        { return std::make_unique<IParam::ShapeLinear>(); }
    inline std::unique_ptr<IParam::Shape> shapeCurve(double v) { return std::make_unique<IParam::ShapePowCurve>(v); }
    inline std::unique_ptr<IParam::Shape> shapeExp()           { return std::make_unique<IParam::ShapeExp>(); }


    // ── ParamDefDouble ────────────────────────────────────────────────────────

    struct ParamDefDouble {
        const char*                    name;
        double                         defaultVal = 0.0;
        double                         min        = 0.0;
        double                         max        = 1.0;
        double                         step       = 0.01;
        const char*                    label      = "";
        const char*                    group      = "";
        int                            flags      = 0;
        IParam::EParamUnit             unit       = IParam::kUnitCustom;
        std::unique_ptr<IParam::Shape> shape      = nullptr;  // null → ShapeLinear
        IParam::DisplayFunc            display    = nullptr;
    };

    inline IParam* applyParam(IParam* p, ParamDefDouble def)
    {
        if (def.shape)
            p->InitDouble(def.name, def.defaultVal, def.min, def.max, def.step,
                          def.label, def.flags, def.group, *def.shape, def.unit, std::move(def.display));
        else
            p->InitDouble(def.name, def.defaultVal, def.min, def.max, def.step,
                          def.label, def.flags, def.group, IParam::ShapeLinear(), def.unit, std::move(def.display));
        return p;
    }


    // ── ParamDefBool ──────────────────────────────────────────────────────────

    struct ParamDefBool {
        const char* name;
        bool        defaultVal = false;
        const char* offText    = "off";
        const char* onText     = "on";
        const char* group      = "";
        int         flags      = 0;
    };

    inline IParam* applyParam(IParam* p, ParamDefBool def)
    {
        p->InitBool(def.name, def.defaultVal, "", def.flags, def.group, def.offText, def.onText);
        return p;
    }


    // ── ParamDefEnum ──────────────────────────────────────────────────────────
    // items can be initialized with a braced list: .items = {"a", "b", "c"}

    struct ParamDefEnum {
        const char*              name;
        int                      defaultVal = 0;
        std::vector<const char*> items;
        const char*              group      = "";
        int                      flags      = 0;
    };

    inline IParam* applyParam(IParam* p, ParamDefEnum def)
    {
        std::vector<std::string> items(def.items.begin(), def.items.end());
        p->InitEnum(def.name, def.defaultVal, items, def.flags, def.group);
        return p;
    }


    // ── Preset factories for common ParamDefDouble types ─────────────────────
    // Override any field after construction via designated initializers on the returned struct,
    // or chain ->SetShape() / ->SetDisplayFunc() on the IParam* returned by applyParam().

    inline ParamDefDouble freqParam(const char* name, double defaultVal,
                                    double min = 20., double max = 20000.)
    {
        return { .name = name, .defaultVal = defaultVal, .min = min, .max = max,
                 .step = 0.1, .label = "Hz", .unit = IParam::kUnitFrequency,
                 .shape = shapeExp() };
    }

    inline ParamDefDouble gainParam(const char* name, double defaultVal = 0.,
                                    double min = -70., double max = 24.)
    {
        return { .name = name, .defaultVal = defaultVal, .min = min, .max = max,
                 .step = 0.5, .label = "dB", .unit = IParam::kUnitDB };
    }

    inline ParamDefDouble msParam(const char* name, double defaultVal,
                                  double min = 0., double max = 1000.)
    {
        return { .name = name, .defaultVal = defaultVal, .min = min, .max = max,
                 .step = 1., .label = "ms", .unit = IParam::kUnitMilliseconds };
    }

    inline ParamDefDouble percentParam(const char* name, double defaultVal = 0.,
                                       double min = 0., double max = 100.)
    {
        return { .name = name, .defaultVal = defaultVal, .min = min, .max = max,
                 .step = 1., .label = "%", .unit = IParam::kUnitPercentage };
    }

} // ns hvoya::params
