#pragma once

/* control_registry.hpp — generic ordered control registry for iPlug2 UIs
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
 * ControlRegistry  Ordered container mapping an int key to ControlDesc.
 *                  The key is typically a plugin param ID, but can also be:
 *                    - an alt control ID (e.g. EAltControls) for alternative visual
 *                      representations or controls binding multiple params (XY pads)
 *                    - a padding pseudo-ID (negative) registered by UserTabPanel
 *                  Entries come back in insertion order — related controls can be
 *                  grouped together and that order is preserved in all UIs (popups,
 *                  lists, etc.). Keyed lookup is O(n); fine for < 100 entries.
 *
 *                  The registry also maintains a tree of Node groups for nested picker
 *                  menus. Call group() with a lambda to open a named folder node; all
 *                  add() / addAlt() / group() calls inside the lambda are nested under it.
 *                  Folders appear as submenus in picker popups and can be nested to
 *                  arbitrary depth. Entries registered outside any group() go into the
 *                  root level. The tree is read-only after construction; access via root().
 *
 *                  Key methods:
 *                    add(id, desc)              -- append a param entry (asserts no duplicate);
 *                                                  inserts a leaf into the current group node
 *                    addAlt(id, desc, numParams) -- append an alt entry; validates id is outside
 *                                                   the real param range and is non-negative
 *                    group(name, fn)            -- nest fn's entries under a named folder node
 *                    has(id)                    -- keyed existence check
 *                    make(id, rect)             -- create the control at rect
 *                    at(id)                     -- returns ControlDesc for accessing metadata
 *                    root()                     -- root Node of the group tree (for menu building)
 *                    begin/end                  -- iterate Entries in insertion order
 *
 * USAGE
 * -----
 *   ControlRegistry reg;
 *
 *   // Simple param control — key = param ID:
 *   reg.add(kMyParam, {
 *       [](const IRECT& r) -> IControl* { return new IVKnobControl(r, kMyParam, "Label"); },
 *       1.0f, "Label"
 *   });
 *
 *   // Grouped entries — appear as a submenu folder in picker popups:
 *   reg.group("My Section", [&] {
 *       reg.add(kParamA, { ..., 0.5f, "Param A" });
 *       reg.group("Nested", [&] {
 *           reg.add(kParamB, { ..., 0.5f, "Param B" });  // arbitrary depth
 *       });
 *   });
 *
 *   // Alt control — alternative visual or multi-param (XY pad, slider, etc.).
 *   // Key must be non-negative and >= numParams to avoid collision.
 *   // Convention: define alt IDs in a plugin-local enum starting well above numParams.
 *   reg.addAlt(kAlt_MyXY, {
 *       [](const IRECT& r) -> IControl* { return new IVXYPadControl(r, {kParamA, kParamB}); },
 *       0.75f, "My XY"
 *   }, kNumParams);
 *
 *   // Create a control at a given rect:
 *   pG->AttachControl(reg.make(kMyParam, someRect), -1, kGroupAll);
 *
 *   // Iterate in insertion order:
 *   for (auto& [id, desc] : reg)
 *       pG->AttachControl(reg.make(id, someRect), -1, kGroupAll);
 *
 *   // Access metadata:
 *   float h = reg.at(kMyParam).heightFrac;
 */

#include <vector>
#include <functional>
#include <memory>
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
    // ── Group tree ────────────────────────────────────────────────────────────
    // Populated during the builder function (buildControlRegistry or equivalent);
    // read-only afterwards. Each node is a named folder containing children that
    // are either leaf entries (referencing a param/alt ID) or nested group nodes.
    struct Node {
        std::string name;   // empty string for the root node
        struct Child {
            bool                  isGroup;
            int                   leafId;  // valid when !isGroup
            std::shared_ptr<Node> group;   // valid when  isGroup
        };
        std::vector<Child> children;
    };

    // ── Construction / copy / move ────────────────────────────────────────────
    ControlRegistry() : _currentNode(&_root) {}

    // Copy — both registries share the same inner Node objects (safe: read-only after build).
    ControlRegistry(const ControlRegistry& o)
        : _entries(o._entries), _root(o._root), _currentNode(&_root) {}

    ControlRegistry& operator=(const ControlRegistry& o) {
        _entries     = o._entries;
        _root        = o._root;
        _currentNode = &_root;
        return *this;
    }

    // Move — _currentNode must always point into our own _root, not the moved-from object.
    ControlRegistry(ControlRegistry&& o) noexcept
        : _entries(std::move(o._entries)), _root(std::move(o._root)), _currentNode(&_root) {}

    ControlRegistry& operator=(ControlRegistry&& o) noexcept {
        _entries     = std::move(o._entries);
        _root        = std::move(o._root);
        _currentNode = &_root;
        return *this;
    }

    // ── Registration ──────────────────────────────────────────────────────────

    // Register a simple control keyed by a plugin param ID.
    // Inserts a leaf into the current group node so picker menus reflect structure.
    void add(int id, ControlDesc d) {
        assert(!has(id) && "duplicate key in ControlRegistry");
        _entries.push_back({id, std::move(d)});
        _currentNode->children.push_back({false, id, nullptr});
    }

    // Register an alternative control (different visual for one param, or multi-param
    // compound such as an XY pad) under a key that is NOT a real param ID.
    // numParams — the plugin's total param count; alt IDs must be >= this value
    //             to guarantee no overlap with the real param range [0 .. numParams-1].
    // Alt IDs must also be non-negative (negative IDs are reserved for padding/sentinels).
    void addAlt(int id, ControlDesc d, int numParams) {
        assert(id >= 0        && "alt ID must be non-negative — negative IDs are reserved for padding/sentinels");
        assert(id >= numParams && "alt ID must not overlap with the real param range [0 .. numParams-1]");
        add(id, std::move(d));
    }

    // Open a named folder node; all add() / addAlt() / group() calls made inside fn
    // become children of that node in the group tree. Folders appear as submenus in
    // picker popups. Nesting is handled by the call stack — no endGroup() needed.
    void group(std::string name, std::function<void()> fn) {
        auto node     = std::make_shared<Node>(Node{std::move(name), {}});
        Node* nodePtr = node.get();
        _currentNode->children.push_back({true, 0, std::move(node)});
        Node* parent  = _currentNode;
        _currentNode  = nodePtr;
        fn();
        _currentNode  = parent;
    }

    // Replace the factory of an already-registered id, keeping its heightFrac / displayName /
    // group-tree placement. Handy for a per-tab registry copy that needs the same controls
    // built differently (e.g. fit-in-bounds sizing in a clipped slot panel).
    void replaceFactory(int id, ControlFactory f) {
        auto it = std::find_if(_entries.begin(), _entries.end(),
            [id](const Entry& e) { return e.id == id; });
        assert(it != _entries.end() && "replaceFactory: key not found in ControlRegistry");
        it->desc.factory = std::move(f);
    }

    // ── Queries ───────────────────────────────────────────────────────────────

    bool has(int id) const {
        return std::find_if(_entries.begin(), _entries.end(),
            [id](const Entry& e) { return e.id == id; }) != _entries.end();
    }

    // Create the control for id sized to rect -- shorthand for at(id).factory(rect).
    IControl* make(int id, const IRECT& r) const { return at(id).factory(r); }

    const ControlDesc& at(int id) const {
        auto it = std::find_if(_entries.begin(), _entries.end(),
            [id](const Entry& e) { return e.id == id; });
        assert(it != _entries.end() && "key not found in ControlRegistry");
        return it->desc;
    }

    // Root of the group tree. Pass to UserTabPanel or any other consumer that
    // needs to build nested menus — e.g. buildPickerMenu(*this, root(), ...).
    const Node& root() const { return _root; }

    size_t size()  const { return _entries.size(); }
    bool   empty() const { return _entries.empty(); }

    struct Entry { int id; ControlDesc desc; };
    auto begin() const { return _entries.begin(); }
    auto end()   const { return _entries.end(); }

private:
    std::vector<Entry> _entries;
    Node               _root{};
    Node*              _currentNode;  // points into the tree; only valid during population
};

} // namespace hvoya::ui
