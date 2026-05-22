#pragma once

/* user_tab_panel.hpp — user-configurable control panel (IContainerBase subclass)
 *
 * OVERVIEW
 * --------
 * UserTabPanel is an iPlug2 IContainerBase that lets the user build a personal
 * set of controls from any parameters available in a ControlRegistry.
 *
 * Layout: the panel area is divided into vertical columns (slots). Normal slots
 * share width equally; a pad-only slot (a single padding spacer) gets a narrower
 * column proportional to its heightFrac. Each slot is divided vertically by the
 * heightFrac of its entries: real controls share remaining height proportionally,
 * padding entries take an absolute fraction of slot height.
 *
 *   locked view:     [ Cutoff knob | Drive knob | Sat switch ]  [edit]
 *                                                                  ↑ always visible
 *
 *   edit view:       [ < >  +  >   | < >  +  >  | < >  +  > ]  [lock]  ← per-slot header
 *                    [ Cutoff knob | Drive knob  | Sat switch]           [save]
 *                    [      +      |      +      |           ]           [load]
 *                                                                        [copy]
 *                                                                        [paste]
 *                                                                        [clear]
 *                                                                        [undo]
 *
 * EDIT MODE
 * ---------
 * The "edit"/"lock" button in the top-right corner toggles edit mode.
 * Entering edit mode resets the undo history.
 * In edit mode:
 *   - "< >" buttons at the top of each slot move the whole column left/right.
 *   - "x" button on each entry removes that entry (removes the slot if it empties).
 *   - "+" in the per-slot header adds another control to that slot (if room).
 *   - "+" column at the far right adds a new slot.
 *   - Thin outlines are drawn around each entry cell and each slot column.
 *   - A stack of buttons appears below "lock": save, load, copy, paste, clear, undo.
 *     clear and undo are greyed out when unavailable.
 * Clicking "+" opens a popup mirroring the group tree registered in ControlRegistry:
 * top-level groups appear as submenus (folders), entries inside them as leaf items.
 * Nesting can be arbitrary. Already-full or duplicate-in-slot entries are disabled.
 *
 * FILE FORMAT
 * -----------
 * Layouts are saved as .hvoya files (plain text, section-based):
 *
 *   # hvoya v1
 *   # plugin Melter 1.2.3
 *
 *   [user-tab]
 *   slot 12 5
 *   slot 7 -2
 *
 * Each "slot" line lists param IDs left-to-right (negative IDs = padding spacers).
 * loadLayout / pasteFromClipboard read only the [user-tab] section and silently
 * skip unknown param IDs. Other sections (e.g. [midi-cc]) are ignored, making the
 * format safe to extend without breaking existing loaders.
 *
 * UNDO HISTORY
 * ------------
 * Every mutation (add, remove, move, load, paste, clear) is undoable via undo().
 * The history is cleared when entering edit mode and is not persisted across sessions.
 *
 * PLUGIN-SIDE SERIALIZATION
 * -------------------------
 * The panel itself holds no persistent state between UI rebuilds. The plugin owns
 * the slot data as std::vector<std::vector<int>> and bool, and passes it in via
 * initialSlots / initialUnlocked. The onChanged callback is called on every
 * mutation so the plugin can update its members immediately (for serialization).
 *
 * USAGE IN A LAYOUT FUNCTION
 * --------------------------
 *   // 1. Build slot list from plugin's persisted state:
 *   std::vector<hvoya::ui::UserTabPanel::Slot> initSlots;
 *   for (const auto& s : _userSlots)
 *       initSlots.push_back({ s });   // s is std::vector<int>
 *
 *   // 2. Create and attach — attach to the tab group so hiding works correctly:
 *   auto* panel = new hvoya::ui::UserTabPanel(
 *       contentRect, fm,
 *       std::move(initSlots), _userTabUnlocked,
 *       btnStyle, editColor,
 *       [this](const std::vector<hvoya::ui::UserTabPanel::Slot>& slots, bool unlocked) {
 *           _userSlots.clear();
 *           for (const auto& s : slots) _userSlots.push_back(s.params);
 *           _userTabUnlocked = unlocked;
 *       },
 *       PLUG_NAME, PLUG_VERSION_STR);  // written to file header; fileExt defaults to "hvoya"
 *   pG->AttachControl(panel, -1, kGroupUSER);
 *
 * MULTIPLE PANELS PER PLUGIN
 * --------------------------
 * Each UserTabPanel is independent — its own ControlRegistry copy, its own slot
 * list, its own callback. To have two user panels (e.g. USER-A and USER-B tabs),
 * store _userSlotsA / _userSlotsB in the plugin and construct two panels with
 * separate callbacks and separate tab groups.
 *
 * MULTIPLE PLUGIN INSTANCES
 * -------------------------
 * Each plugin instance calls buildControlRegistry() capturing its own mediator by
 * reference. UserTabPanel stores a copy of the registry. There is no shared state
 * — concurrent instances are fully independent.
 */

#include <vector>
#include <functional>
#include <set>
#include <IControl.h>
#include "control_registry.hpp"

namespace hvoya::ui {

using iplug::igraphics::IContainerBase;
using iplug::igraphics::IColor;
using iplug::igraphics::IControl;
using iplug::igraphics::IPopupMenu;
using iplug::igraphics::IRECT;
using iplug::igraphics::IVStyle;
using hvoya::ui::ControlRegistry;

class UserTabPanel : public IContainerBase {
public:
    // One vertical column in the panel. params are stacked top-to-bottom;
    // their combined heightFrac must not exceed 1.0.
    struct Slot {
        std::vector<int> params;

        float usedHeight(const ControlRegistry& reg) const {
            float h = 0.f;
            for (int p : params)
                h += reg.has(p) ? reg.at(p).heightFrac : 1.f;
            return h;
        }

        bool hasRoom(const ControlRegistry& reg, float needed) const {
            return usedHeight(reg) + needed <= 1.f + 1e-4f;
        }
    };

    // Called on every mutation (add/remove/move/lock toggle).
    // Use this to persist slot state in the plugin for serialization.
    using OnChangedFn = std::function<void(const std::vector<Slot>&, bool unlocked)>;

    UserTabPanel(const IRECT& bounds,
                 ControlRegistry factories,   // copied into panel — safe after mLayoutFunc returns
                 std::vector<Slot> initialSlots,
                 bool initialUnlocked,
                 const IVStyle& btnStyle,
                 IColor editColor,            // accent color for edit-mode overlays and buttons
                 OnChangedFn onChanged,
                 std::string pluginName,      // written to file header, e.g. "Melter"
                 std::string pluginVersion,   // written to file header, e.g. "1.2.3"
                 std::string fileExt = "hvoya"); // extension shown in PromptForFile dialogs

    const std::vector<Slot>& slots()      const { return _slots; }
    bool                     isUnlocked() const { return _unlocked; }

    // Toggle edit mode programmatically — e.g. bind to a keyboard shortcut.
    // Entering edit mode (unlocked = true) resets the undo history.
    void setUnlocked(bool unlocked) {
        if (_unlocked == unlocked) return;
        _unlocked = unlocked;
        if (_unlocked) _history.clear();  // fresh history on each edit session
        notifyChanged();
        rebuild();
    }

    // Remove all slots (undoable).
    void clearSlots();

    // Undo the last mutation since entering edit mode.
    // No-op if the history is empty.
    void undo();

    // Write the current layout to a .hvoya file (header + [user-tab] section).
    // Returns false on I/O error; panel state is unchanged.
    bool saveLayout(const char* path) const;

    // Load layout from a .hvoya file; reads only the [user-tab] section.
    // Unknown param IDs are silently skipped. Lock/unlock state is NOT changed.
    // Pushes to undo history on success.
    // Returns false if the file can't be opened or contains no [user-tab] section.
    bool loadLayout(const char* path);

    // Copy the full .hvoya layout (header + [user-tab]) to the system clipboard.
    bool copyToClipboard();

    // Replace the current slots with the [user-tab] section from the clipboard.
    // Pushes to undo history on success.
    // Returns false if the clipboard is empty or contains no [user-tab] section.
    bool pasteFromClipboard();

    // Called by iPlug2 when a file is dropped onto the panel (or any empty area of it).
    void OnDrop(const char* str) override;

    void OnAttached() override { rebuild(); }
    void OnPopupMenuSelection(IPopupMenu* pMenu, int valIdx) override;

    // Expose so callers can reserve space in the padding row above the content area.
    static constexpr float kEditHeaderH = 18.f;

    // Padding pseudo-param IDs (negative — never conflict with real plugin params >= 0).
    // Added to an existing slot  → vertical spacer (fraction of slot height).
    // Added as a brand-new slot  → column spacer   (fraction of normal column width).
    static constexpr int kPad1_8 = -1;
    static constexpr int kPad1_4 = -2;
    static constexpr int kPad1_2 = -3;

    static bool isPad(int id) { return id < 0; }

private:
    // Tags on IPopupMenu::Item encode the control ID: tag = id + kPickerTagOffset.
    // kPickerTagOffset = -kPad1_2 = 3, so the most-negative valid ID (kPad1_2 = -3)
    // maps to tag 0. All valid tags are >= 0; iPlug2's default "no tag" sentinel is -1.
    static constexpr int kPickerTagOffset = 3;

private:
    static constexpr float kSmallBtnW    = 20.f;
    static constexpr float kLockBtnW     = 40.f;

    ControlRegistry                _factories;  // owned — lambdas inside capture mediator by ref (stable)
    std::vector<Slot>              _slots;
    std::vector<std::vector<Slot>> _history;   // undo stack; cleared on each edit-mode entry
    bool                           _unlocked;
    IVStyle                        _btnStyle;
    IVStyle                        _editBtnStyle;  // bigger, centered, accent-colored — used for +/x/</>
    IColor                         _editColor;     // accent color for edit-mode overlays and buttons
    OnChangedFn                    _onChanged;
    std::string                    _pluginName;
    std::string                    _pluginVersion;
    std::string                    _fileExt;

    int        _pendingPickerSlotIdx = -1;
    IPopupMenu _pickerMenu;  // must outlive CreatePopupMenu (async on macOS)

    void rebuild();        // clears and recreates all child controls from _slots
    void clearChildren();  // removes children from IGraphics and clears mChildren
    void notifyChanged();

    void pushHistory();  // snapshot _slots onto _history before a mutation
    void toggleLock();
    void addSlot(int paramId);
    void addToSlot(int slotIdx, int paramId);
    void removeEntry(int slotIdx, int entryIdx);
    void moveSlot(int slotIdx, int delta);  // delta = -1 (left) or +1 (right)

    void showParamPicker(int slotIdx, IRECT fromRect);  // fromRect positions the popup

    // Recursively populate menu from the registry's group tree node.
    // Leaf entries become tagged items; group nodes become submenus.
    // Padding entries (isPad) are skipped — the caller appends them manually.
    void buildPickerMenu(IPopupMenu& menu, const ControlRegistry::Node& node,
                         int slotIdx, const std::set<int>& usedInSlot) const;

    IControl* makeLockBtn   (const IRECT& r);
    IControl* makePlusBtn   (const IRECT& r, int slotIdx);  // r = available area; button centred inside
    IControl* makeRemoveBtn (const IRECT& r, int slotIdx, int entryIdx);
    IControl* makeMoveBtn   (const IRECT& r, int slotIdx, int delta, const char* label);
    IControl* makeSaveBtn   (const IRECT& r);
    IControl* makeLoadBtn   (const IRECT& r);
    IControl* makeCopyBtn   (const IRECT& r);
    IControl* makePasteBtn  (const IRECT& r);
    IControl* makeClearBtn  (const IRECT& r);
    IControl* makeUndoBtn   (const IRECT& r);

    // Serialization helpers.
    // serializeSlots  — slot lines as a vector of strings (one per slot).
    //                   Fed directly into HvoyaFile::setSection("user-tab", ...).
    // serializeLayout — full .hvoya text via HvoyaFile (header + [user-tab] section).
    // applySection    — parse slot lines from a [user-tab] section, prune, rebuild.
    //                   Called by both applySlots (clipboard/text) and loadLayout (file).
    // applySlots      — parse a full .hvoya text string and apply the [user-tab] section.
    std::vector<std::string> serializeSlots()  const;
    std::string              serializeLayout() const;
    bool applySection(const std::vector<std::string>& lines);
    bool applySlots(const std::string& text);
};

} // namespace hvoya::ui
