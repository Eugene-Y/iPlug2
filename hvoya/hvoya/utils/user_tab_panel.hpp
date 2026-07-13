#pragma once

/* user_tab_panel.hpp — user-configurable control panel (IContainerBase subclass)
 *
 * OVERVIEW
 * --------
 * UserTabPanel is an iPlug2 IContainerBase that lets the user build a personal
 * set of controls from any parameters available in a ControlRegistry.
 *
 * Layout: the panel area is divided into vertical columns (slots). A column is unit
 * width unless it carries a column-width modifier (a "vertical" spacer), which shrinks
 * it to a fraction of a unit column; all column widths are then shared proportionally.
 * Each column is divided vertically by the heightFrac of its entries: real controls
 * share the remaining height proportionally, horizontal spacers take an absolute
 * fraction of column height, and the width modifier takes no height.
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
 * Each "slot" line lists controls left-to-right. A control is written either as a raw
 * integer ID or, when a name codec is supplied (setLayoutTokenCodec), as a stable string
 * TOKEN:
 *
 *   slot par_main_cutoff_hz par_drive
 *   slot kAlt_BreakXY -2
 *
 * Tokens make the layout independent of the plugin's EParams enum order (a raw ID is
 * positional and breaks if params are reordered between versions); a control the codec
 * has no token for (e.g. structural spacers, negative IDs) falls back to its integer.
 * On read, a NUMERIC field is a legacy positional/alt ID and a STRING field is resolved
 * through the codec — so old integer-only files still load unchanged. loadLayout /
 * pasteFromClipboard read only the [user-tab] section and silently skip unknown controls.
 * Other sections (e.g. [midi-cc]) are ignored, making the format safe to extend without
 * breaking existing loaders.
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
#include <array>
#include <string_view>
#include <IControl.h>
#include "control_registry.hpp"
#include "glyph_label.hpp"

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
                 std::string fileExt = "hvoya",       // extension shown in PromptForFile dialogs
                 const char* iconFontName = nullptr,  // icon font for swap buttons; nullptr = use btnStyle font
                 const char* removeFontName = nullptr); // icon font for remove (✕) buttons; nullptr = use btnStyle font

    const std::vector<Slot>& slots()      const { return _slots; }
    bool                     isUnlocked() const { return _unlocked; }

    // ── Button labels (declarative, opt-in) ──────────────────────────────────────
    // Every menu button defaults to plain text (the IVButtonControl look other plugins
    // get). Pass a GlyphLabel (build with hvoya::ui::icon(glyph, font) + operator+) and
    // that button is drawn as a mixed-font GlyphButtonControl instead — same behaviour,
    // icon look. The lock button has one label per state (edit ↔ lock). runGap spaces
    // the runs of a mixed word+icon label.
    UserTabPanel& setEditLabel        (GlyphLabel l) { _editLabel        = std::move(l); return *this; }  // shown when locked (click → edit)
    UserTabPanel& setLockLabel        (GlyphLabel l) { _lockLabel        = std::move(l); return *this; }  // shown when unlocked (click → lock)
    UserTabPanel& setPlusLabel        (GlyphLabel l) { _plusLabel        = std::move(l); return *this; }  // add control / add slot
    UserTabPanel& setRemoveLabel      (GlyphLabel l) { _removeLabel      = std::move(l); return *this; }  // ✕ on an entry
    UserTabPanel& setSwapSlotsLabel   (GlyphLabel l) { _swapSlotsLabel   = std::move(l); return *this; }  // ◄► move column
    UserTabPanel& setSwapEntriesLabel (GlyphLabel l) { _swapEntriesLabel = std::move(l); return *this; }  // ▲▼ reorder in slot
    UserTabPanel& setSaveLabel        (GlyphLabel l) { _saveLabel        = std::move(l); return *this; }
    UserTabPanel& setLoadLabel        (GlyphLabel l) { _loadLabel        = std::move(l); return *this; }
    UserTabPanel& setCopyLabel        (GlyphLabel l) { _copyLabel        = std::move(l); return *this; }
    UserTabPanel& setPasteLabel       (GlyphLabel l) { _pasteLabel       = std::move(l); return *this; }
    UserTabPanel& setClearLabel       (GlyphLabel l) { _clearLabel       = std::move(l); return *this; }
    UserTabPanel& setUndoLabel        (GlyphLabel l) { _undoLabel        = std::move(l); return *this; }
    // Marker drawn in a column that carries a column-width modifier (a "vertical" spacer). It is
    // distinct from the ✕ entry-remove glyph on purpose — it reads as "this column has a width
    // spacer" and clicking it removes the modifier. Default is a plain rectangle fallback.
    UserTabPanel& setSpacerMarkerLabel(GlyphLabel l) { _spacerMarkerLabel = std::move(l); return *this; }
    UserTabPanel& setLabelRunGap      (float px)     { _labelRunGap      = px;           return *this; }

    // Width of the right-edge menu column (lock/edit + save/load/copy/paste/clear/undo)
    // and the new-slot "+" reservation. Defaults to kLockBtnW (fits text labels). With
    // single-icon labels it can be squeezed to ~kEditHeaderH so the menu barely narrows
    // the editable area — the unlocked layout then previews the locked one closely.
    UserTabPanel& setMenuButtonWidth  (float px)     { _menuBtnW         = px;           return *this; }
    // Show the clipboard copy/paste buttons in the edit menu (default). Off → only file save/load
    // (the remaining buttons move up to fill the gap). Lets a host gate clipboard behind an edition.
    UserTabPanel& setClipboardEnabled (bool e)       { _clipboardEnabled = e;            return *this; }

    // ── Opt-in name codec for the .hvoya layout (order-independent tokens) ────────
    // Without a codec, slots serialize control IDs as raw integers — positional param IDs
    // that break if the plugin reorders its EParams enum between versions. Supply a codec
    // and each control writes its stable TOKEN instead ("par_main_drive", "kAlt_BreakXY");
    // on read a token resolves back to this build's ID, so layouts survive enum reordering.
    //   idToToken(id)  → the token to write, or "" to fall back to the integer (spacers, etc.)
    //   tokenToId(tok) → the control ID for a token, or -1 if this build doesn't have it (skip)
    // Old integer-only files always load: a numeric field is read as a legacy integer ID.
    using IdToTokenFn = std::function<std::string(int id)>;
    using TokenToIdFn = std::function<int(std::string_view token)>;
    UserTabPanel& setLayoutTokenCodec (IdToTokenFn idToToken, TokenToIdFn tokenToId) {
        _idToToken = std::move (idToToken);
        _tokenToId = std::move (tokenToId);
        return *this;
    }

    // ── Opt-in hover tooltips for the edit-menu / column-chrome buttons (empty = none) ──
    // Applied inside the make*Btn factories, so they survive every rebuild. Other plugins that
    // don't set them get no tooltip. The edit/lock toggle takes two texts (locked vs unlocked).
    UserTabPanel& setEditTooltip     (std::string s) { _tipEdit     = std::move (s); return *this; }  // shown when locked (click → edit)
    UserTabPanel& setLockTooltip     (std::string s) { _tipLock     = std::move (s); return *this; }  // shown when unlocked (click → lock)
    UserTabPanel& setSaveTooltip     (std::string s) { _tipSave     = std::move (s); return *this; }
    UserTabPanel& setLoadTooltip     (std::string s) { _tipLoad     = std::move (s); return *this; }
    UserTabPanel& setCopyTooltip     (std::string s) { _tipCopy     = std::move (s); return *this; }
    UserTabPanel& setPasteTooltip    (std::string s) { _tipPaste    = std::move (s); return *this; }
    UserTabPanel& setClearTooltip    (std::string s) { _tipClear    = std::move (s); return *this; }
    UserTabPanel& setUndoTooltip     (std::string s) { _tipUndo     = std::move (s); return *this; }
    UserTabPanel& setAddControlTooltip (std::string s) { _tipAddCtrl = std::move (s); return *this; }  // "+" inside a column
    UserTabPanel& setAddColumnTooltip  (std::string s) { _tipAddCol  = std::move (s); return *this; }  // "+" that adds a column
    UserTabPanel& setRemoveTooltip   (std::string s) { _tipRemove   = std::move (s); return *this; }  // ✕ on an entry
    UserTabPanel& setSwapColumnTooltip (std::string s) { _tipSwapCol = std::move (s); return *this; }  // ◄► move column
    UserTabPanel& setReorderTooltip    (std::string s) { _tipReorder = std::move (s); return *this; }  // ▲▼ reorder in slot

    // In edit mode the panel grows UPWARD by `px` and the per-column edit chrome (slot-swap + add
    // buttons) moves into that gained band, sitting ABOVE the control cells instead of overlapping
    // them. Content keeps the original bounds (so it matches a sibling layout of that rect); the
    // lock/menu column stays anchored to the content, not the gained band. 0 (default) = no growth,
    // chrome overlays the content top. The host frees the space above (e.g. collapses a strip there).
    UserTabPanel& setEditExpandTop    (float px)     { _editExpandTop    = px;           return *this; }
    // In edit mode the panel grows DOWNWARD by `px` into a thin band below the control cells, where
    // each column's column-width-spacer marker is drawn — so the marker sits just UNDER its column
    // instead of overlapping the bottom control. 0 (default) = no band, the marker falls back inside
    // the column. The host grants only space it knows is free below the panel (e.g. a layout margin).
    UserTabPanel& setEditExpandBottom (float px)     { _editExpandBottom = px;           return *this; }

    // Hover color of the edit-mode action glyphs (+ / ✕ / swap ◄► / swap ▲▼). At rest the
    // glyphs draw in editColor (the accent); on mouse-over they recolor to `c` — the glyph
    // itself, not a background rectangle (the rect highlight is suppressed for these buttons).
    UserTabPanel& setHighlightColor (IColor c) {
        _highlightColor    = c;
        _hasHighlightColor = true;
        return *this;
    }

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

    // ── Layout constants — adjust these to tune the panel geometry ───────────────
    static constexpr float kEditHeaderH   = 18.f;   // height of the header row (lock/edit btn + slot headers)
    static constexpr float kEntryGap      = 2.f;    // px gap between entries within a slot (vertical)
    static constexpr float kSlotGap       = 0.f;    // px gap between slot columns (horizontal)
    static constexpr float kSwapBtnW      = 40.f;   // width of slot-swap (horizontal ◄►) button
    static constexpr float kSwapEntryBtnW = 40.f;   // width of entry-swap (vertical ▲▼) button — narrower for the upright glyph
    static constexpr float kSwapEntryBtnH = 24.f;   // height of entry-swap button — taller so the upright glyph isn't clipped
    static constexpr float kPlusBtnMaxW   = 80.f;   // max width of "+" button (clamped to available space)
    static constexpr float kPlusBtnMaxH   = 40.f;   // max height of "+" button

    // Spacer pseudo-param IDs (negative — never collide with real params ≥ 0). Two kinds:
    //   • Horizontal — an empty band INSIDE a column (a fraction of the column HEIGHT), pushing the
    //     controls below it down.
    //   • Vertical  — a column-WIDTH modifier: narrows the whole column to a fraction of a unit
    //     column, proportionally (4 unit columns + one ½ ⇒ the ½ is 0.5/4.5 = 1/9 of the width). It
    //     consumes no height and doesn't change how the controls lay out. A column with no modifier
    //     is unit width. At most one per column; alone in a slot it is an empty column.
    //
    // kSpacerDefs is the ONE source of truth: registry, picker, width maths and the tag offset all
    // read from it. To add a spacer, add an id constant and a row below — nothing else changes.
    static constexpr int kPad1_8 = -1, kPad1_4 = -2, kPad1_2 = -3;                   // horizontal
    static constexpr int kColWidth1_1 = -4, kColWidth3_4 = -5, kColWidth2_3 = -6,    // vertical
                         kColWidth1_2 = -7, kColWidth1_4 = -8, kColWidth1_8 = -9;

    enum class SpacerKind { Horizontal, Vertical };
    struct SpacerDef {
        int              id;
        float            frac;   // Horizontal → fraction of column height; Vertical → fraction of column width
        SpacerKind       kind;
        std::string_view name;   // picker label
    };
    static constexpr std::array<SpacerDef, 9> kSpacerDefs { {
        { kPad1_8,      0.125f, SpacerKind::Horizontal, "horizontal  1/8" },
        { kPad1_4,      0.25f,  SpacerKind::Horizontal, "horizontal  1/4" },
        { kPad1_2,      0.5f,   SpacerKind::Horizontal, "horizontal  1/2" },
        { kColWidth1_1, 1.0f,   SpacerKind::Vertical,   "vertical  1"     },
        { kColWidth3_4, 0.75f,  SpacerKind::Vertical,   "vertical  3/4"   },
        { kColWidth2_3, 0.66f,  SpacerKind::Vertical,   "vertical  2/3"   },
        { kColWidth1_2, 0.5f,   SpacerKind::Vertical,   "vertical  1/2"   },
        { kColWidth1_4, 0.25f,  SpacerKind::Vertical,   "vertical  1/4"   },
        { kColWidth1_8, 0.125f, SpacerKind::Vertical,   "vertical  1/8"   },
    } };

    // Every spacer id must be negative and unique (else the id/tag maths breaks silently).
    static_assert([] {
        for (std::size_t i = 0; i < kSpacerDefs.size(); ++i) {
            if (kSpacerDefs[i].id >= 0) return false;
            for (std::size_t j = i + 1; j < kSpacerDefs.size(); ++j)
                if (kSpacerDefs[i].id == kSpacerDefs[j].id) return false;
        }
        return true;
    }(), "UserTabPanel spacer ids must be negative and unique");

    // Most-negative spacer id — derived, so a more-negative spacer needs no other edit.
    static constexpr int kSpacerIdMin = [] {
        int m = 0;
        for (const auto& d : kSpacerDefs) if (d.id < m) m = d.id;
        return m;
    }();

    static constexpr const SpacerDef* spacerDef(int id) {
        for (const auto& d : kSpacerDefs) if (d.id == id) return &d;
        return nullptr;
    }
    static bool  isPad     (int id) { return id < 0; }   // any spacer id (real params are ≥ 0)
    static bool  isColWidth(int id) { auto* d = spacerDef(id); return d && d->kind == SpacerKind::Vertical; }
    // Fraction of a unit column a width modifier occupies (1 for non-modifiers / no modifier).
    static float columnWidthFrac(int id) { auto* d = spacerDef(id); return (d && d->kind == SpacerKind::Vertical) ? d->frac : 1.f; }

private:
    // Menu-item tag encodes the control id: tag = id + kPickerTagOffset. Offsetting by the most-
    // negative spacer id lifts every tag to ≥ 0 (iPlug's "no tag" sentinel is -1). Derived, so it
    // tracks kSpacerDefs automatically — never hand-tune it.
    static constexpr int   kPickerTagOffset = -kSpacerIdMin;
    static constexpr float kSwapEntryOffX   = 0.125f; // horizontal position of entry-swap btn (fraction of slot width)
    static constexpr float kSmallBtnW       = 20.f;   // width of ✕ remove button
    static constexpr float kLockBtnW        = 40.f;   // width of lock/edit button

    ControlRegistry                _factories;  // owned — lambdas inside capture mediator by ref (stable)
    std::vector<Slot>              _slots;
    std::vector<std::vector<Slot>> _history;   // undo stack; cleared on each edit-mode entry
    bool                           _unlocked;
    IVStyle                        _btnStyle;
    IVStyle                        _editBtnStyle;       // bigger, centered, accent-colored — used for +
    IVStyle                        _entryBtnStyle;      // same as _editBtnStyle but 0.75× font — used for x on entries
    IVStyle                        _swapBtnStyle;       // icon font variant of _editBtnStyle — used for <-> and ^v swap buttons
    IColor                         _editColor;          // accent color for edit-mode overlays and buttons
    OnChangedFn                    _onChanged;
    std::string                    _pluginName;
    std::string                    _pluginVersion;
    std::string                    _fileExt;
    IdToTokenFn                    _idToToken;   // layout codec: control ID → stable token ("" = write int)
    TokenToIdFn                    _tokenToId;   // layout codec: token → control ID (-1 = unknown, skip)

    // Opt-in icon / mixed-font button labels (empty → plain-text IVButtonControl).
    GlyphLabel _editLabel, _lockLabel, _plusLabel, _removeLabel,
               _swapSlotsLabel, _swapEntriesLabel,
               _saveLabel, _loadLabel, _copyLabel, _pasteLabel, _clearLabel, _undoLabel,
               _spacerMarkerLabel;
    float      _labelRunGap = 0.f;
    // Opt-in hover tooltips (empty = none) — see the set*Tooltip setters, applied in make*Btn.
    std::string _tipEdit, _tipLock, _tipSave, _tipLoad, _tipCopy, _tipPaste, _tipClear, _tipUndo,
                _tipAddCtrl, _tipAddCol, _tipRemove, _tipSwapCol, _tipReorder;
    float      _menuBtnW    = kLockBtnW;    // right-edge menu column width (see setMenuButtonWidth)
    bool       _clipboardEnabled = true;    // show copy/paste in the edit menu (see setClipboardEnabled)
    float      _editExpandTop = 0.f;        // upward growth in edit mode (see setEditExpandTop)
    float      _editExpandBottom = 0.f;     // downward growth in edit mode for width-spacer markers (see setEditExpandBottom)
    IRECT      _baseRECT;                   // attach bounds (the content area); edit mode grows upward from here

    IColor _highlightColor;                // hover color for action glyphs (see setHighlightColor)
    bool   _hasHighlightColor = false;

    int        _pendingPickerSlotIdx = -1;
    IPopupMenu _pickerMenu;  // must outlive CreatePopupMenu (async on macOS)

    void rebuild();        // clears and recreates all child controls from _slots
    void clearChildren();  // removes children from IGraphics and clears mChildren
    void notifyChanged();

    // ── Slot queries (spacer/width-modifier aware) ───────────────────────────────
    bool  slotHasWidthMod    (const Slot& s) const;   // does the slot carry a column-width modifier?
    bool  slotHasRealControl (const Slot& s) const;   // any registered non-spacer entry?
    int   widthModIndex      (const Slot& s) const;   // params index of the width modifier, or -1
    float slotWidthWeight    (const Slot& s) const;   // column-width weight (1 unless a modifier narrows it)
    bool  normalizeSlots     ();                      // pin the width modifier first, drop extra ones; true if changed

    void pushHistory();  // snapshot _slots onto _history before a mutation
    void toggleLock();
    void addSlot(int paramId);
    void addToSlot(int slotIdx, int paramId);
    void removeEntry(int slotIdx, int entryIdx);
    void resetColumnWidth(int slotIdx);  // drop the column-width modifier → unit width; deletes the column if it empties
    void swapSlots  (int slotIdx);              // swap slot[slotIdx] ↔ slot[slotIdx+1]
    void swapEntries(int slotIdx, int entryIdx); // swap entry[entryIdx] ↔ entry[entryIdx+1] within slot

    void showParamPicker(int slotIdx, IRECT fromRect);  // fromRect positions the popup

    // Recursively populate menu from the registry's group tree node.
    // Leaf entries become tagged items; group nodes become submenus.
    // Padding entries (isPad) are skipped — the caller appends them manually.
    void buildPickerMenu(IPopupMenu& menu, const ControlRegistry::Node& node,
                         int slotIdx, const std::set<int>& usedInSlot) const;

    // Builds a button: a mixed-font GlyphButtonControl when `label` is non-empty,
    // else the plain-text IVButtonControl (fallback text + style). `onClick` runs on
    // release for both paths. Caller may SetDisabled() on the returned control.
    //
    // `bgHighlight` picks the hover/press look of glyph buttons:
    //   false → the edit-mode action-glyph look: no background rect, the glyph itself
    //           recolors to the highlight color on hover (set via setHighlightColor).
    //   true  → the menu look: glyph stays light, the background fills with the style's
    //           kHL (hover) / kPR (press) — a highlight behind the glyph.
    IControl* makeButton(const IRECT& r, const GlyphLabel& label, const char* fallbackText,
                         const IVStyle& style, std::function<void(IControl*)> onClick,
                         bool bgHighlight = false);

    IControl* makeLockBtn   (const IRECT& r);
    IControl* makePlusBtn   (const IRECT& r, int slotIdx);  // r = available area; button centred inside
    IControl* makeRemoveBtn (const IRECT& r, int slotIdx, int entryIdx);
    IControl* makeWidthModBtn(const IRECT& r, int slotIdx);  // column-width spacer marker (resets the width)
    IControl* makeSwapSlotsBtn  (const IRECT& r, int slotIdx);
    IControl* makeSwapEntriesBtn(const IRECT& r, int slotIdx, int entryIdx);
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
