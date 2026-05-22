#pragma once

/* control_registry.hpp — generic ordered control registry for iPlug2 UIs
 *
 * Intended to move to hvoya/utils once stable enough to reuse across plugins.
 *
 * TYPES
 * -----
 * ControlFactory   std::function<IControl*(const IRECT&)>
 *                  A callable that creates an iPlug2 control sized to a given rect.
 *                  Each lambda fully owns its style, label, param ID, etc.
 *                  The only thing that varies at call time is the rect.
 *
 * ControlDesc      { factory, heightFrac, displayName }
 *                  Bundles a factory with layout metadata and a human label.
 *                    factory     -- creates the control for a given rect
 *                    heightFrac  -- for real controls: height as a fraction of a UserTabPanel
 *                                   slot column (1.0 = full, 0.75 = three-quarters, 0.25 = quarter).
 *                                   For padding pseudo-params used as a standalone column slot,
 *                                   this value also drives the column width (0.25 = quarter-width
 *                                   column). See UserTabPanel padding constants.
 *                    displayName -- shown in param-picker popups
 *
 * ControlRegistry  Ordered container: int paramId to ControlDesc.
 *                  Entries come back in insertion order -- related parameters can be
 *                  grouped together and that order is preserved in all UIs (popups,
 *                  lists, etc.). Keyed lookup is O(n); fine for < 100 params.
 *
 *                  Key methods:
 *                    add(id, desc)   -- append entry (asserts no duplicate)
 *                    has(id)         -- keyed existence check
 *                    make(id, rect)  -- create the control at rect (shorthand for at(id).factory(rect))
 *                    at(id)          -- returns ControlDesc for accessing metadata
 *                    begin/end       -- iterate Entries in insertion order
 *
 * USAGE
 * -----
 *   ControlRegistry reg;
 *   reg.add(kMyParam, {
 *       [](const IRECT& r) -> IControl* { return new IVKnobControl(r, kMyParam, "Label"); },
 *       1.0f, "Label"
 *   });
 *
 *   // Create a control at a given rect -- preferred shorthand:
 *   pG->AttachControl(reg.make(kMyParam, someRect), -1, kGroupAll);
 *
 *   // Iterate in insertion order:
 *   for (auto& [paramId, desc] : reg)
 *       pG->AttachControl(reg.make(paramId, someRect), -1, kGroupAll);
 *
 *   // Access metadata alongside factory:
 *   float h = reg.at(kMyParam).heightFrac;
 */

#include <vector>
#include <functional>
#include <string>
#include <cassert>
#include <algorithm>
#include <IGraphicsStructs.h>

namespace iplug::igraphics { class IControl; }

namespace hvoya::ui {

using iplug::igraphics::IControl;
using iplug::igraphics::IRECT;

using ControlFactory = std::function<IControl*(const IRECT&)>;

struct ControlDesc {
    ControlFactory factory;
    float          heightFrac;   // fraction of a UserTabPanel slot column height
    std::string    displayName;  // shown in param-picker popups and any other UI lists
};

class ControlRegistry {
public:
    struct Entry { int paramId; ControlDesc desc; };

    void add(int paramId, ControlDesc d) {
        assert(!has(paramId) && "duplicate paramId in ControlRegistry");
        _entries.push_back({paramId, std::move(d)});
    }

    bool has(int paramId) const {
        return std::find_if(_entries.begin(), _entries.end(),
            [paramId](const Entry& e) { return e.paramId == paramId; }) != _entries.end();
    }

    // Create the control for paramId sized to rect -- shorthand for at(id).factory(rect).
    IControl* make(int paramId, const IRECT& r) const { return at(paramId).factory(r); }

    const ControlDesc& at(int paramId) const {
        auto it = std::find_if(_entries.begin(), _entries.end(),
            [paramId](const Entry& e) { return e.paramId == paramId; });
        assert(it != _entries.end() && "paramId not found in ControlRegistry");
        return it->desc;
    }

    size_t size()  const { return _entries.size(); }
    bool   empty() const { return _entries.empty(); }

    auto begin() const { return _entries.begin(); }
    auto end()   const { return _entries.end(); }

private:
    std::vector<Entry> _entries;
};

} // namespace hvoya::ui
