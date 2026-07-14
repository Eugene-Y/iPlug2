#include "user_tab_panel.hpp"
#include <IControls.h>
#include <IGraphics.h>
#include <IGraphicsPopupMenu.h>
#include <hvoya/utils/hvoya_file.hpp>
#include <set>
#include <algorithm>
#include <sstream>
#include <charconv>

namespace hvoya::ui {

using namespace iplug;
using namespace igraphics;

namespace {

static constexpr float kBorderStroke = 1.f;  // edit-mode border overlay stroke width (px)

// Thin transparent border drawn on top of a control rect in edit mode.
// IsHit returns false so mouse events pass through to the controls beneath.
struct BorderOverlay : public IControl {
    IColor mBorderColor;
    BorderOverlay(const IRECT& r, IColor col) : IControl(r), mBorderColor(col) {}
    void Draw(IGraphics& g) override { g.DrawRect(mBorderColor, mRECT, nullptr, kBorderStroke); }
    bool IsHit(float, float) const override { return false; }
};

// Invisible spacer — draws nothing, passes all mouse events through.
struct PaddingControl : public IControl {
    explicit PaddingControl(const IRECT& r) : IControl(r) {}
    void Draw(IGraphics&) override {}
    bool IsHit(float, float) const override { return false; }
};

} // namespace


UserTabPanel::UserTabPanel(const IRECT& bounds,
                           ControlRegistry factories,
                           std::vector<Slot> initialSlots,
                           bool initialUnlocked,
                           const IVStyle& btnStyle,
                           IColor editColor,
                           OnChangedFn onChanged,
                           std::string pluginName,
                           std::string pluginVersion,
                           std::string fileExt,
                           const char* iconFontName,
                           const char* removeFontName)
    : IContainerBase(bounds)
    , _factories(std::move(factories))
    , _slots(std::move(initialSlots))
    , _unlocked(initialUnlocked)
    , _btnStyle(btnStyle)
    , _editColor(editColor)
    , _onChanged(std::move(onChanged))
    , _pluginName(std::move(pluginName))
    , _pluginVersion(std::move(pluginVersion))
    , _fileExt(std::move(fileExt))
{
    _baseRECT = bounds;   // content area; edit mode grows the panel upward from here

    _editBtnStyle = _btnStyle;
    _editBtnStyle.labelText.mSize    = _btnStyle.labelText.mSize * 2.f;
    _editBtnStyle.labelText.mVAlign  = EVAlign::Middle;
    _editBtnStyle.labelText.mAlign   = EAlign::Center;
    _editBtnStyle.labelText.mFGColor = editColor;

    _entryBtnStyle = _editBtnStyle;
    _entryBtnStyle.labelText.mSize   = _editBtnStyle.labelText.mSize * 0.75f;

    _moveGlyphStyle = _editBtnStyle;
    if (iconFontName)
        strncpy(_moveGlyphStyle.labelText.mFont, iconFontName, sizeof(_moveGlyphStyle.labelText.mFont) - 1);

    if (removeFontName)
        strncpy(_entryBtnStyle.labelText.mFont, removeFontName, sizeof(_entryBtnStyle.labelText.mFont) - 1);

    // Register every spacer from the single kSpacerDefs table (see the header). A horizontal spacer
    // takes its frac as the entry height; a width modifier consumes no height (its frac is a column
    // width, read via columnWidthFrac).
    for (const auto& d : kSpacerDefs)
        _factories.add(d.id, { [](const IRECT& r) -> IControl* { return new PaddingControl(r); },
                               d.kind == SpacerKind::Horizontal ? d.frac : 0.f,
                               std::string(d.name) });

    // Prune any param IDs from loaded slots that aren't in this registry
    // (e.g., params removed between plugin versions, or entries from a different preset).
    // This keeps _userSlots consistent with _factories from the very first rebuild().
    bool pruned = false;
    for (auto& slot : _slots) {
        const auto before = slot.params.size();
        slot.params.erase(
            std::remove_if(slot.params.begin(), slot.params.end(),
                [this](int p) { return !_factories.has(p); }),
            slot.params.end());
        if (slot.params.size() != before) pruned = true;
    }
    const auto slotsBefore = _slots.size();
    _slots.erase(
        std::remove_if(_slots.begin(), _slots.end(),
            [](const Slot& s) { return s.params.empty(); }),
        _slots.end());
    if (_slots.size() != slotsBefore) pruned = true;
    if (normalizeSlots()) pruned = true;
    if (pruned) notifyChanged();
}


// ──────────────────────────────────────────────
//  Slot queries (spacer / width-modifier aware)

bool UserTabPanel::slotHasWidthMod(const Slot& s) const {
    for (int p : s.params) if (isColWidth(p)) return true;
    return false;
}

bool UserTabPanel::slotHasRealControl(const Slot& s) const {
    for (int p : s.params) if (!isPad(p) && _factories.has(p)) return true;
    return false;
}

int UserTabPanel::widthModIndex(const Slot& s) const {
    for (int i = 0; i < (int)s.params.size(); ++i) if (isColWidth(s.params[i])) return i;
    return -1;
}

float UserTabPanel::slotWidthWeight(const Slot& s) const {
    const int i = widthModIndex(s);
    return i >= 0 ? columnWidthFrac(s.params[i]) : 1.f;
}

bool UserTabPanel::normalizeSlots() {
    bool changed = false;
    for (auto& s : _slots) {
        const int firstW = widthModIndex(s);
        if (firstW < 0) continue;
        // Drop any extra width modifiers after the first (at most one per column).
        for (int i = (int)s.params.size() - 1; i > firstW; --i)
            if (isColWidth(s.params[i])) { s.params.erase(s.params.begin() + i); changed = true; }
        // Pin the width modifier to the front so real-control entries stay contiguous
        // (keeps the vertical layout and entry-swap indices simple).
        if (firstW > 0) {
            const int id = s.params[firstW];
            s.params.erase(s.params.begin() + firstW);
            s.params.insert(s.params.begin(), id);
            changed = true;
        }
    }
    return changed;
}


namespace {

// Walk the chosen item chain into any submenu, returning the leaf item's tag.
// Returns -1 if no tag is set (separator, submenu title, or untagged item).
int findChosenTag(IPopupMenu* menu) {
    if (!menu) return -1;
    auto* chosen = menu->GetChosenItem();
    if (!chosen) return -1;
    if (auto* sub = chosen->GetSubmenu())
        return findChosenTag(sub);
    return chosen->GetTag();
}

} // namespace

void UserTabPanel::OnPopupMenuSelection(IPopupMenu* pMenu, int) {
    if (!pMenu || pMenu->GetChosenItemIdx() < 0) {
        rebuild();  // reset button pressed states after dismissed-without-selection
        return;
    }

    const int tag = findChosenTag(pMenu);
    if (tag < 0) { rebuild(); return; }  // no tag = separator, submenu header, or dismissed

    const int paramId = tag - kPickerTagOffset;
    if (!_factories.has(paramId)) { rebuild(); return; }

    if (_pendingPickerSlotIdx < 0)
        addSlot(paramId);
    else
        addToSlot(_pendingPickerSlotIdx, paramId);
}


// ──────────────────────────────────────────────
//  Mutations

void UserTabPanel::pushHistory() {
    _history.push_back(_slots);
}

void UserTabPanel::toggleLock() { setUnlocked(!_unlocked); }

void UserTabPanel::addSlot(int paramId) {
    pushHistory();
    _slots.push_back(Slot{{paramId}});
    notifyChanged();
    rebuild();
}

void UserTabPanel::addToSlot(int slotIdx, int paramId) {
    assert(slotIdx >= 0 && slotIdx < (int)_slots.size());
    pushHistory();
    auto& params = _slots[slotIdx].params;
    if (isColWidth(paramId))
        params.insert(params.begin(), paramId);   // width modifier pinned to the front
    else
        params.push_back(paramId);
    notifyChanged();
    rebuild();
}

void UserTabPanel::removeEntry(int slotIdx, int entryIdx) {
    assert(slotIdx >= 0 && slotIdx < (int)_slots.size());
    auto& slot = _slots[slotIdx];
    assert(entryIdx >= 0 && entryIdx < (int)slot.params.size());
    pushHistory();
    slot.params.erase(slot.params.begin() + entryIdx);
    if (slot.params.empty())
        _slots.erase(_slots.begin() + slotIdx);
    notifyChanged();
    rebuild();
}

void UserTabPanel::resetColumnWidth(int slotIdx) {
    if (slotIdx < 0 || slotIdx >= (int)_slots.size()) return;
    const int i = widthModIndex(_slots[slotIdx]);
    if (i < 0) return;
    pushHistory();
    auto& params = _slots[slotIdx].params;
    params.erase(params.begin() + i);          // back to unit width; controls (if any) stay
    if (params.empty())
        _slots.erase(_slots.begin() + slotIdx);  // a pure spacer column disappears
    notifyChanged();
    rebuild();
}

void UserTabPanel::moveColumnTo(int from, int toGap) {
    const int n = (int)_slots.size();
    if (from < 0 || from >= n) return;
    toGap = std::clamp(toGap, 0, n);
    Slot s = std::move(_slots[from]);
    _slots.erase(_slots.begin() + from);
    if (toGap > from) --toGap;                       // the erase shifted everything past `from` left
    toGap = std::clamp(toGap, 0, (int)_slots.size());
    _slots.insert(_slots.begin() + toGap, std::move(s));
}

void UserTabPanel::moveEntryWithinSlot(int slot, int fromEntry, int laidGap) {
    if (slot < 0 || slot >= (int)_slots.size() || slot >= (int)_cellRects.size()) return;
    auto& params = _slots[slot].params;
    if (fromEntry < 0 || fromEntry >= (int)params.size()) return;

    // laidGap is a gap in laid-out-cell space; map it to a params index (the params index of the cell
    // now occupying that gap, or one-past-the-end). Inserting among laid cells never lands before a
    // pinned width modifier (index 0), so the modifier stays put; normalizeSlots() re-pins regardless.
    const auto& cells = _cellRects[slot];
    laidGap = std::clamp(laidGap, 0, (int)cells.size());
    int toIdx = (laidGap < (int)cells.size()) ? cells[laidGap].first : (int)params.size();

    const int v = params[fromEntry];
    params.erase(params.begin() + fromEntry);
    if (toIdx > fromEntry) --toIdx;
    toIdx = std::clamp(toIdx, 0, (int)params.size());
    params.insert(params.begin() + toIdx, v);
}

void UserTabPanel::moveEntryAcross(int fromSlot, int fromEntry, int toSlot, int laidGap) {
    if (fromSlot == toSlot) { moveEntryWithinSlot(fromSlot, fromEntry, laidGap); return; }
    if (fromSlot < 0 || fromSlot >= (int)_slots.size()) return;
    if (toSlot   < 0 || toSlot   >= (int)_slots.size() || toSlot >= (int)_cellRects.size()) return;
    auto& from = _slots[fromSlot].params;
    if (fromEntry < 0 || fromEntry >= (int)from.size()) return;

    // Target insertion index in the destination column's params (from its pre-move laid-cell geometry).
    const auto& cells = _cellRects[toSlot];
    laidGap = std::clamp(laidGap, 0, (int)cells.size());
    const int toIdx = (laidGap < (int)cells.size()) ? cells[laidGap].first : (int)_slots[toSlot].params.size();

    const int v = from[fromEntry];
    from.erase(from.begin() + fromEntry);
    // Insert into the destination BEFORE erasing an emptied source slot (toSlot index still valid; the
    // two params vectors are distinct, so the source erase above didn't touch the destination's).
    auto& to = _slots[toSlot].params;
    to.insert(to.begin() + std::clamp(toIdx, 0, (int)to.size()), v);
    if (_slots[fromSlot].params.empty())
        _slots.erase(_slots.begin() + fromSlot);   // a column emptied by the move disappears
}

void UserTabPanel::moveEntryToNewColumn(int fromSlot, int fromEntry, int toGap) {
    if (fromSlot < 0 || fromSlot >= (int)_slots.size()) return;
    auto& from = _slots[fromSlot].params;
    if (fromEntry < 0 || fromEntry >= (int)from.size()) return;

    const int v = from[fromEntry];
    from.erase(from.begin() + fromEntry);
    const bool srcEmptied = from.empty();

    const int insAt = std::clamp(toGap, 0, (int)_slots.size());
    _slots.insert(_slots.begin() + insAt, Slot{ { v } });   // a fresh unit-width column holding just it

    if (srcEmptied) {
        // The insert shifted the (now-empty) source right by one when it landed at/before it.
        const int srcIdx = fromSlot + (insAt <= fromSlot ? 1 : 0);
        _slots.erase(_slots.begin() + srcIdx);
    }
}

// ── Drag-to-reorder lifecycle (called by CellOverlay) ────────────────────────
int UserTabPanel::columnAtX(float x) const {
    for (int s = 0; s < (int)_colContentRects.size(); ++s)
        if (x >= _colContentRects[s].L && x <= _colContentRects[s].R) return s;
    return -1;
}

void UserTabPanel::beginCellDrag(int slot, int entry, const IRECT& srcRect, bool forceColumn) {
    _drag = DragSession{};
    _drag.active      = true;
    _drag.srcSlot     = slot;
    _drag.srcEntry    = entry;
    _drag.srcRect     = srcRect;
    _drag.forceColumn = forceColumn;
}

void UserTabPanel::updateCellDrag(float x, float y, const IMouseMod& mod) {
    if (!_drag.active) return;
    _drag.columnMode = _drag.forceColumn || mod.S;   // Shift → move the whole column (live)
    _drag.newColumn  = false;

    if (_drag.columnMode) {
        // Whole-column move: insertion gap among the columns (how many column centres are left of x).
        int gap = 0;
        for (const auto& r : _colContentRects)
            if (r.MW() <= x) ++gap;
        gap = std::clamp(gap, 0, (int)_colContentRects.size());
        _drag.targetSlot = -1;
        _drag.targetGap  = gap;
        _drag.valid      = gap != _drag.srcSlot && gap != _drag.srcSlot + 1;  // not a drop back in place
    } else if (const int col = columnAtX(x); col < 0 || col >= (int)_cellRects.size()) {
        // Past the left/right edge (or over the add-column strip) → drop into a fresh column there.
        _drag.targetSlot = -1;
        if (_colContentRects.empty()) { _drag.valid = false; return; }
        const int gap = (x < _colContentRects.front().L) ? 0 : (int)_slots.size();
        _drag.newColumn = true;
        _drag.targetGap = gap;
        // No-op if the source is already a lone column landing back beside itself.
        const bool srcAlone = _drag.srcSlot < (int)_cellRects.size() && _cellRects[_drag.srcSlot].size() == 1;
        _drag.valid = !(srcAlone && (gap == _drag.srcSlot || gap == _drag.srcSlot + 1));
        if (auto* pG = GetUI()) pG->SetAllControlsDirty();
        return;
    } else {
        // Single-control move: destination = the column under the pointer, at the row gap under y.
        const auto& cells = _cellRects[col];
        int gap = 0;
        for (const auto& c : cells)
            if (c.second.MH() <= y) ++gap;
        gap = std::clamp(gap, 0, (int)cells.size());
        _drag.targetSlot = col;
        _drag.targetGap  = gap;
        if (col == _drag.srcSlot) {                       // reorder within the source column
            int srcPos = 0;
            for (int i = 0; i < (int)cells.size(); ++i)
                if (cells[i].first == _drag.srcEntry) { srcPos = i; break; }
            _drag.valid = gap != srcPos && gap != srcPos + 1;
        } else {                                          // move into another column → needs room
            const int   pid  = _slots[_drag.srcSlot].params[_drag.srcEntry];
            const float need = _factories.has(pid) ? _factories.at(pid).heightFrac : 1.f;
            _drag.valid = _slots[col].hasRoom(_factories, need);
        }
    }
    if (auto* pG = GetUI()) pG->SetAllControlsDirty();
}

void UserTabPanel::endCellDrag(float x, float y, const IMouseMod& mod) {
    updateCellDrag(x, y, mod);                          // resolve the final target
    const bool doMove = _drag.active && _drag.valid;
    const DragSession d = _drag;
    _drag = DragSession{};
    if (doMove) {
        pushHistory();
        if      (d.columnMode) moveColumnTo(d.srcSlot, d.targetGap);
        else if (d.newColumn)  moveEntryToNewColumn(d.srcSlot, d.srcEntry, d.targetGap);
        else                   moveEntryAcross(d.srcSlot, d.srcEntry, d.targetSlot, d.targetGap);
        normalizeSlots();
        notifyChanged();
        rebuild();
    } else if (auto* pG = GetUI()) {
        pG->SetAllControlsDirty();
    }
}

void UserTabPanel::clearSlots() {
    if (_slots.empty()) return;
    pushHistory();
    _slots.clear();
    notifyChanged();
    rebuild();
}

void UserTabPanel::undo() {
    if (_history.empty()) return;
    _slots = std::move(_history.back());
    _history.pop_back();
    notifyChanged();
    rebuild();
}

void UserTabPanel::notifyChanged() {
    if (_onChanged)
        _onChanged(_slots, _unlocked);
}


// ──────────────────────────────────────────────
//  Layout file save / load

std::vector<std::string> UserTabPanel::serializeSlots() const {
    std::vector<std::string> lines;
    lines.reserve(_slots.size());
    for (const auto& slot : _slots) {
        std::ostringstream os;
        os << "slot";
        for (int p : slot.params) {
            std::string tok = _idToToken ? _idToToken(p) : std::string{};
            if (!tok.empty()) os << ' ' << tok;   // stable token (param / alt-control)
            else              os << ' ' << p;      // no codec / spacer → raw integer id
        }
        lines.push_back(os.str());
    }
    return lines;
}

std::string UserTabPanel::serializeLayout() const {
    hvoya::HvoyaFile layout(_pluginName, _pluginVersion);
    layout.setSection("user-tab", serializeSlots());
    return layout.toString();
}

bool UserTabPanel::applySection(const std::vector<std::string>& lines) {
    std::vector<Slot> newSlots;
    for (const auto& line : lines) {
        std::istringstream ss(line);
        std::string key;
        if (!(ss >> key) || key != "slot") continue;
        Slot slot;
        std::string field;
        while (ss >> field) {
            int id = 0;
            const char* begin = field.data();
            const char* end   = begin + field.size();
            auto [ptr, ec] = std::from_chars(begin, end, id);
            if (ec == std::errc{} && ptr == end) {
                slot.params.push_back(id);          // legacy numeric field (positional / alt id)
            } else if (_tokenToId) {
                const int resolved = _tokenToId(field);
                if (resolved != -1) slot.params.push_back(resolved);  // named token; skip if unknown
            }
        }
        if (!slot.params.empty())
            newSlots.push_back(std::move(slot));
    }

    // Prune any param IDs this instance doesn't know about.
    for (auto& slot : newSlots)
        slot.params.erase(
            std::remove_if(slot.params.begin(), slot.params.end(),
                [this](int p) { return !_factories.has(p); }),
            slot.params.end());
    newSlots.erase(
        std::remove_if(newSlots.begin(), newSlots.end(),
            [](const Slot& s) { return s.params.empty(); }),
        newSlots.end());

    pushHistory();
    _slots = std::move(newSlots);
    normalizeSlots();
    notifyChanged();
    rebuild();
    return true;
}

bool UserTabPanel::applySlots(const std::string& text) {
    const auto layout = hvoya::HvoyaFile::fromText(text);
    if (!layout || !layout->hasSection("user-tab")) return false;
    return applySection(layout->section("user-tab"));
}

bool UserTabPanel::saveLayout(const char* path) const {
    hvoya::HvoyaFile layout(_pluginName, _pluginVersion);
    layout.setSection("user-tab", serializeSlots());
    return layout.toFile(path);
}

bool UserTabPanel::loadLayout(const char* path) {
    const auto layout = hvoya::HvoyaFile::fromFile(path);
    if (!layout || !layout->hasSection("user-tab")) return false;
    return applySection(layout->section("user-tab"));
}

bool UserTabPanel::copyToClipboard() {
    return GetUI()->SetTextInClipboard(serializeLayout().c_str());
}

bool UserTabPanel::pasteFromClipboard() {
    WDL_String str;
    if (!GetUI()->GetTextFromClipboard(str) || !str.GetLength())
        return false;
    return applySlots(str.Get());
}

void UserTabPanel::OnDrop(const char* str) {
    loadLayout(str);
}


// ──────────────────────────────────────────────
//  Param picker popup

void UserTabPanel::showParamPicker(int slotIdx, IRECT fromRect) {
    _pendingPickerSlotIdx = slotIdx;

    std::set<int> usedInTargetSlot;
    if (slotIdx >= 0 && slotIdx < (int)_slots.size())
        for (int p : _slots[slotIdx].params)
            usedInTargetSlot.insert(p);

    _pickerMenu.Clear();

    // Build the group tree into the menu — folders become submenus, leaves become items.
    // Padding entries are skipped here and appended manually below.
    buildPickerMenu(_pickerMenu, _factories.root(), slotIdx, usedInTargetSlot);

    // Spacers gathered into one "spacers" submenu: horizontal height-bands, then a separator,
    // then vertical column-width modifiers. Items carry an explicit tag so selection is identified
    // via GetTag(), not menu index (fragile with separators/submenus).
    auto* spacers = new IPopupMenu("spacers");
    const bool haveTarget   = slotIdx >= 0 && slotIdx < (int)_slots.size();
    const bool targetHasReal  = haveTarget && slotHasRealControl(_slots[slotIdx]);
    const bool targetHasWidth = haveTarget && slotHasWidthMod   (_slots[slotIdx]);

    // Horizontal spacers — a band inside a column; only meaningful in a column that holds controls.
    for (const auto& d : kSpacerDefs) {
        if (d.kind != SpacerKind::Horizontal) continue;
        const auto& desc = _factories.at(d.id);
        int flags = IPopupMenu::Item::kNoFlags;
        if (!haveTarget || !targetHasReal || !_slots[slotIdx].hasRoom(_factories, desc.heightFrac))
            flags = IPopupMenu::Item::kDisabled;
        spacers->AddItem(new IPopupMenu::Item(desc.displayName.c_str(), flags, d.id + kPickerTagOffset));
    }

    // Vertical spacers — column-width modifiers. As a new slot they make an empty column; in an
    // existing column they narrow it (one per column). They consume no height, so no room check.
    spacers->AddSeparator();
    for (const auto& d : kSpacerDefs) {
        if (d.kind != SpacerKind::Vertical) continue;
        const auto& desc = _factories.at(d.id);
        const int flags = targetHasWidth ? IPopupMenu::Item::kDisabled : IPopupMenu::Item::kNoFlags;
        spacers->AddItem(new IPopupMenu::Item(desc.displayName.c_str(), flags, d.id + kPickerTagOffset));
    }

    _pickerMenu.AddSeparator();
    _pickerMenu.AddItem(new IPopupMenu::Item("spacers", spacers));   // Item owns the submenu

    GetUI()->CreatePopupMenu(*this, _pickerMenu, fromRect);
}

void UserTabPanel::buildPickerMenu(IPopupMenu& menu, const ControlRegistry::Node& node,
                                   int slotIdx, const std::set<int>& usedInSlot) const {
    for (const auto& child : node.children) {
        if (child.isGroup) {
            // Group node → submenu. The submenu is owned by the Item (unique_ptr inside).
            auto* submenu = new IPopupMenu(child.group->name.c_str());
            buildPickerMenu(*submenu, *child.group, slotIdx, usedInSlot);
            menu.AddItem(new IPopupMenu::Item(child.group->name.c_str(), submenu));
        } else {
            const int id = child.leafId;
            if (isPad(id)) continue;  // padding spacers are appended separately at the bottom
            if (!_factories.has(id)) continue;

            const auto& desc = _factories.at(id);
            bool noRoom  = slotIdx >= 0 && !_slots[slotIdx].hasRoom(_factories, desc.heightFrac);
            bool disabled = usedInSlot.count(id) || noRoom;
            const int flags = disabled ? IPopupMenu::Item::kDisabled : IPopupMenu::Item::kNoFlags;
            menu.AddItem(new IPopupMenu::Item(desc.displayName.c_str(), flags, id + kPickerTagOffset));
        }
    }
}


// ──────────────────────────────────────────────
//  Child control factories

// Apply an opt-in hover tooltip (no-op when empty) and pass the control through for chaining.
static IControl* withTip(IControl* c, const std::string& t) {
    if (c && !t.empty()) c->SetTooltip(t.c_str());
    return c;
}

IControl* UserTabPanel::makeButton(const IRECT& r, const GlyphLabel& label, const char* fallbackText,
                                   const IVStyle& style, std::function<void(IControl*)> onClick,
                                   bool bgHighlight) {
    if (label.empty())
        return (new IVButtonControl(r, DefaultClickActionFunc, fallbackText, style))
            ->SetAnimationEndActionFunction(std::move(onClick));

    // GlyphButtonControl draws its text through style.valueText; the IVButton styles
    // configure labelText (the button caption), so mirror it across.
    IVStyle gstyle = style;
    gstyle.valueText = style.labelText;
    auto* b = new GlyphButtonControl(r, label, std::move(onClick), gstyle);
    b->setRunGap(_labelRunGap);
    if (_hasHighlightColor && !bgHighlight) {
        // Action-glyph look: no background-rect highlight; the glyph itself recolors on hover.
        b->setMouseOverColor(COLOR_TRANSPARENT);
        b->setPressedColor(COLOR_TRANSPARENT);
        b->setMouseOverTextColor(_highlightColor);
    }
    // bgHighlight → keep the style's kHL/kPR background fills, glyph stays light (default).
    return b;
}

IControl* UserTabPanel::makeLockBtn(const IRECT& r) {
    return withTip(makeButton(r, _unlocked ? _lockLabel : _editLabel, _unlocked ? "lock" : "edit", _btnStyle,
        [this](IControl*) { toggleLock(); }, /*bgHighlight*/ true),
        _unlocked ? _tipLock : _tipEdit);
}

IControl* UserTabPanel::makePlusBtn(const IRECT& r, int slotIdx) {
    // Target area = the drawn glyph box, not the whole header/column cell — a snug square the size of
    // the glyph (its font size), centred in r. So the click area matches what the user sees.
    const float side = std::min({ _entryBtnStyle.labelText.mSize, r.W(), r.H() });
    const IRECT btnR = r.GetCentredInside(side, side);
    return withTip(makeButton(btnR, _plusLabel, "+", _entryBtnStyle,
        [this, slotIdx, btnR](IControl*) { showParamPicker(slotIdx, btnR); }),
        slotIdx < 0 ? _tipAddCol : _tipAddCtrl);
}

IControl* UserTabPanel::makeWidthModBtn(const IRECT& r, int slotIdx) {
    // Marks a column-width spacer — a distinct glyph from the ✕ entry-remove. Centred inside r like
    // the "+". Clicking resets the column to unit width (keeping its controls); on a pure spacer
    // column that reset empties it, so the column disappears.
    const IRECT btnR = r.GetCentredInside(std::min(r.W(), kPlusBtnMaxW), std::min(r.H(), kPlusBtnMaxH));
    return withTip(makeButton(btnR, _spacerMarkerLabel, "▭", _entryBtnStyle,
        [this, slotIdx](IControl*) { resetColumnWidth(slotIdx); }),
        colWidthTip(_slots[slotIdx].params[widthModIndex(_slots[slotIdx])]));
}

// Short hover label for a column-width modifier, e.g. "1/4 width. click to remove" (the marker resets
// the column to unit width on click); "full width. click to remove" for a unit-width modifier.
std::string UserTabPanel::colWidthTip(int id) {
    const SpacerDef* d = spacerDef(id);
    if (!d || d->kind != SpacerKind::Vertical) return {};
    const std::string_view n = d->name;                 // "vertical  <frac>", e.g. "vertical  3/4"
    const std::string frac = d->frac >= 1.f - 1e-4f ? "full"
                                                    : std::string(n.substr(n.find_last_of(' ') + 1));
    return frac + " width. click to remove";
}

// The single interaction surface over a control cell in edit mode. It intercepts the mouse (IsHit is
// the default true), so the real control beneath is inert while editing. It draws the subtle rest
// outline, the ✥ move glyph centred while hovered, and the ✕ remove glyph in the top-right corner
// (light only when the pointer is directly over it). ALL the white chrome (hover frame, drag frames,
// insertion line) is drawn by the topmost DragIndicator, so a neighbour column's outline can never
// overpaint it. Down + drag past the threshold hands off to the panel's drag lifecycle (Shift decides
// column-vs-single); a click on the ✕ corner (no drag) removes the entry.
class UserTabPanel::CellOverlay : public IControl {
public:
    CellOverlay(UserTabPanel* panel, const IRECT& r, int slot, int entry, bool columnHandle)
        : IControl(r), _panel(panel), _slot(slot), _entry(entry), _isColumnHandle(columnHandle) {}

    void Draw(IGraphics& g) override {
        const IColor accent   = _panel->_editColor;
        const IColor glyphHi  = _panel->_hasHighlightColor ? _panel->_highlightColor : accent;
        const IColor entryCol { 80, accent.R, accent.G, accent.B };  // subtle "here's a cell" marker at rest

        g.DrawRect(entryCol, mRECT, &mBlend, kBorderStroke);   // white hover/drag frames come from the top

        // ✥ move glyph — only while hovered on the body (not when targeting the ✕), and only if the
        // cell is roomy enough to show it without colliding with the ✕ corner.
        if (_hover && !_overCross && !_panel->_moveLabel.empty()
            && mRECT.W() > kSmallBtnW * 2.f && mRECT.H() > kSmallBtnW * 2.f) {
            IText t = _panel->_moveGlyphStyle.labelText;   // icon font lives on labelText (see makeButton)
            t.mFGColor = glyphHi;
            drawGlyphLabel(g, _panel->_moveLabel, mRECT, t, &mBlend, _panel->_labelRunGap);
        }

        // ✕ remove — top-right corner; accent at rest, highlight only when directly hovered. A column
        // handle (pure spacer column) has no own entry to remove, so it shows no ✕.
        if (!_isColumnHandle && !_panel->_removeLabel.empty()) {
            IText t = _panel->_entryBtnStyle.labelText;
            t.mFGColor = _overCross ? glyphHi : accent;
            drawGlyphLabel(g, _panel->_removeLabel, crossRect(), t, &mBlend, _panel->_labelRunGap);
        }
    }

    void OnMouseOver(float x, float y, const IMouseMod&) override {
        _hover     = true;
        _overCross = !_isColumnHandle && crossRect().Contains(x, y);
        _panel->_hoverKey  = this;             // the top indicator paints the white hover frame here
        _panel->_hoverRect = mRECT;
        if (auto* pG = GetUI()) { pG->SetMouseCursor(_overCross ? ECursor::HAND : ECursor::SIZEALL); pG->SetAllControlsDirty(); }
    }
    void OnMouseOut() override {
        _hover = _overCross = false;
        if (_panel->_hoverKey == this) _panel->_hoverKey = nullptr;
        if (auto* pG = GetUI()) { pG->SetMouseCursor(ECursor::ARROW); pG->SetAllControlsDirty(); }
    }
    void OnMouseDown(float x, float y, const IMouseMod&) override {
        _downX = x; _downY = y;
        _dragging      = false;
        _pendingRemove = !_isColumnHandle && crossRect().Contains(x, y);
    }
    void OnMouseDrag(float x, float y, float, float, const IMouseMod& mod) override {
        if (_pendingRemove) return;                 // a press on the ✕ corner is a click, not a drag
        if (!_dragging) {
            if (std::hypot(x - _downX, y - _downY) < kDragStartThreshold) return;
            _dragging = true;
            _panel->beginCellDrag(_slot, _entry, mRECT, _isColumnHandle);
        }
        _panel->updateCellDrag(x, y, mod);
    }
    void OnMouseUp(float x, float y, const IMouseMod& mod) override {
        if (auto* pG = GetUI()) pG->SetMouseCursor(ECursor::ARROW);
        if (_dragging) {
            _panel->endCellDrag(x, y, mod);         // commits + rebuild() → this overlay is destroyed
        } else if (_pendingRemove && crossRect().Contains(x, y)) {
            _panel->removeEntry(_slot, _entry);     // rebuild() → this overlay is destroyed
        }
    }

private:
    IRECT crossRect() const { return mRECT.GetFromTRHC(kSmallBtnW, kEditHeaderH); }

    UserTabPanel* _panel;
    int  _slot, _entry;
    bool _hover        = false;
    bool _overCross    = false;
    bool _dragging     = false;
    bool _pendingRemove= false;
    bool _isColumnHandle;
    float _downX = 0.f, _downY = 0.f;
};

// Topmost, non-interactive: owns ALL the white edit-mode chrome — the hover frame, the drag frames,
// and the insertion line — so nothing beneath (a neighbour column outline, a real control) can
// overpaint it. Full-panel bounds; IsHit false so it never blocks the cells.
class UserTabPanel::DragIndicator : public IControl {
public:
    DragIndicator(UserTabPanel* panel, const IRECT& r) : IControl(r), _panel(panel) {}
    bool IsHit(float, float) const override { return false; }

    void Draw(IGraphics& g) override {
        const auto& d = _panel->_drag;
        if (d.active && d.valid) {
            if (d.columnMode) {
                const auto& cols = _panel->_colContentRects;
                if (cols.empty()) return;
                // The whole source column moves → frame it (thin); the insertion gap is a vertical line.
                if (d.srcSlot >= 0 && d.srcSlot < (int)cols.size())
                    g.DrawRect(COLOR_WHITE, cols[d.srcSlot], &mBlend, kBorderStroke);
                const float x = d.targetGap <= 0                 ? cols.front().L
                              : d.targetGap >= (int)cols.size()  ? cols.back().R
                              : 0.5f * (cols[d.targetGap - 1].R + cols[d.targetGap].L);
                g.DrawLine(COLOR_WHITE, x, cols.front().T, x, cols.front().B, &mBlend, kInsertionStroke);
            } else if (d.newColumn) {
                // One control breaks out into a fresh column at an edge → frame it (thin) + a vertical
                // insertion line at that edge, same as a column insert.
                const auto& cols = _panel->_colContentRects;
                if (cols.empty()) return;
                g.DrawRect(COLOR_WHITE, d.srcRect, &mBlend, kBorderStroke);
                const float x = d.targetGap <= 0 ? cols.front().L : cols.back().R;
                g.DrawLine(COLOR_WHITE, x, cols.front().T, x, cols.front().B, &mBlend, kInsertionStroke);
            } else {
                // One control moves into a column → frame it (thin); the destination row gap is a
                // horizontal line spanning the target column.
                g.DrawRect(COLOR_WHITE, d.srcRect, &mBlend, kBorderStroke);
                if (d.targetSlot < 0 || d.targetSlot >= (int)_panel->_cellRects.size()) return;
                const auto& cells = _panel->_cellRects[d.targetSlot];
                const IRECT& col  = _panel->_colContentRects[d.targetSlot];
                const float y = cells.empty()                        ? col.T
                              : d.targetGap <= 0                     ? cells.front().second.T
                              : d.targetGap >= (int)cells.size()     ? cells.back().second.B
                              : 0.5f * (cells[d.targetGap - 1].second.B + cells[d.targetGap].second.T);
                g.DrawLine(COLOR_WHITE, col.L, y, col.R, y, &mBlend, kInsertionStroke);
            }
        } else if (_panel->_hoverKey) {
            g.DrawRect(COLOR_WHITE, _panel->_hoverRect, &mBlend, kBorderStroke);   // hover frame on top
        }

        // White instruction hint in the gained bottom band (drawn here so it neither intercepts clicks
        // nor is overpainted). Centred on the WHOLE UI width (like the morph "editing …" caption), not
        // just the panel's tab rect.
        if (!_panel->_moveHint.empty() && _panel->_editExpandBottom > 0.f) {
            IText t = _panel->_btnStyle.labelText;
            t.mFGColor = COLOR_WHITE;
            const IRECT band = mRECT.GetFromBottom(_panel->_editExpandBottom);
            const IRECT ui   = g.GetBounds();
            drawGlyphLabel(g, GlyphLabel(_panel->_moveHint),
                           IRECT(ui.L, band.T, ui.R, band.B), t, &mBlend, _panel->_labelRunGap);
        }
    }

    // Repaint continuously while a drag or hover is live (so the chrome tracks the pointer); otherwise
    // defer to the base flag, so an explicit SetAllControlsDirty (drag end / rebuild) clears it.
    bool IsDirty() override { return _panel->_drag.active || _panel->_hoverKey != nullptr || IControl::IsDirty(); }

private:
    UserTabPanel* _panel;
};

IControl* UserTabPanel::makeCellOverlay(const IRECT& r, int slotIdx, int entryIdx, bool columnHandle) {
    auto* c = new CellOverlay(this, r, slotIdx, entryIdx, columnHandle);
    return columnHandle ? c : withTip(c, _tipRemove);   // a spacer-column handle has nothing to "remove"
}

IControl* UserTabPanel::makeSaveBtn(const IRECT& r) {
    return withTip(makeButton(r, _saveLabel, "save", _btnStyle, [this](IControl*) {
        WDL_String fileName("layout"), path;
        GetUI()->PromptForFile(fileName, path, EFileAction::Save, _fileExt.c_str(),
            [this](const WDL_String& fn, const WDL_String&) {
                if (fn.GetLength()) saveLayout(fn.Get());
            });
    }, /*bgHighlight*/ true), _tipSave);
}

IControl* UserTabPanel::makeLoadBtn(const IRECT& r) {
    return withTip(makeButton(r, _loadLabel, "load", _btnStyle, [this](IControl*) {
        WDL_String fileName, path;
        GetUI()->PromptForFile(fileName, path, EFileAction::Open, _fileExt.c_str(),
            [this](const WDL_String& fn, const WDL_String&) {
                if (fn.GetLength()) loadLayout(fn.Get());
            });
    }, /*bgHighlight*/ true), _tipLoad);
}

IControl* UserTabPanel::makeCopyBtn(const IRECT& r) {
    return withTip(makeButton(r, _copyLabel, "copy", _btnStyle, [this](IControl*) { copyToClipboard(); },
        /*bgHighlight*/ true), _tipCopy);
}

IControl* UserTabPanel::makePasteBtn(const IRECT& r) {
    return withTip(makeButton(r, _pasteLabel, "paste", _btnStyle, [this](IControl*) { pasteFromClipboard(); },
        /*bgHighlight*/ true), _tipPaste);
}

IControl* UserTabPanel::makeClearBtn(const IRECT& r) {
    auto* btn = makeButton(r, _clearLabel, "clear", _btnStyle, [this](IControl*) { clearSlots(); },
        /*bgHighlight*/ true);
    if (_slots.empty()) btn->SetDisabled(true);
    return withTip(btn, _tipClear);
}

IControl* UserTabPanel::makeUndoBtn(const IRECT& r) {
    auto* btn = makeButton(r, _undoLabel, "undo", _btnStyle, [this](IControl*) { undo(); },
        /*bgHighlight*/ true);
    if (_history.empty()) btn->SetDisabled(true);
    return withTip(btn, _tipUndo);
}


// ──────────────────────────────────────────────
//  Rebuild

void UserTabPanel::clearChildren() {
    auto* pG = GetUI();
    if (!pG) { mChildren.Empty(false); return; }
    for (int i = NChildren() - 1; i >= 0; --i) {
        auto* child = GetChild(i);
        // IContainerBase children are attached to IGraphics directly via AttachControl.
        // RemoveControl on the outer container only deletes the container itself;
        // the inner children stay in pG->mControls as invisible orphans.  Strip them first.
        if (auto* nested = dynamic_cast<IContainerBase*>(child))
            for (int j = nested->NChildren() - 1; j >= 0; --j)
                pG->RemoveControl(nested->GetChild(j));
        pG->RemoveControl(child);
        mChildren.Delete(i, false);
    }
}

void UserTabPanel::rebuild() {
    clearChildren();
    auto* pG = GetUI();
    if (!pG) return;

    // Content always fills the attach bounds (so it matches a sibling themed-tab layout of the same
    // rect — no lost vertical space in either mode). In edit mode the panel grows UPWARD by
    // _editExpandTop (that gained band hosts the per-column "+" chrome ABOVE the cells) and
    // DOWNWARD by _editExpandBottom (a thin band BELOW the cells that holds each column's width-spacer
    // marker, out of the column so it never overlaps a control). The lock/menu column is anchored to
    // the content, so it stays put.
    const bool  expandUp   = _unlocked && _editExpandTop    > 0.f;
    const bool  expandDown = _unlocked && _editExpandBottom > 0.f;
    const IRECT content    = _baseRECT;
    const IRECT b {
        _baseRECT.L,
        _baseRECT.T - (expandUp   ? _editExpandTop    : 0.f),
        _baseRECT.R,
        _baseRECT.B + (expandDown ? _editExpandBottom : 0.f)
    };
    SetTargetAndDrawRECTs(b);   // mRECT covers the gained bands so their chrome hit-tests
    // Header band for the per-column edit chrome: the gained band when expanded, else the content top.
    const IRECT chromeBand = expandUp ? b.GetFromTop(_editExpandTop) : content.GetFromTop(kEditHeaderH);

    if (_slots.empty() && !_unlocked) {
        // Placeholder hint centred in the content area. The "edit" token mirrors the
        // lock/edit button: its icon label when set (so the hint reads "tap [🔒] to …"),
        // else the plain word for text-button plugins.
        // Derive font from btnStyle so NanoVG is guaranteed to have it loaded.
        const IRECT contentR = content;
        IVStyle hintStyle = _btnStyle;
        hintStyle.valueText          = _btnStyle.labelText;
        hintStyle.valueText.mSize    = 22;
        hintStyle.valueText.mFGColor = IColor(80, 255, 255, 255);
        const GlyphLabel editToken = _editLabel.empty() ? GlyphLabel("'edit'") : _editLabel;
        const GlyphLabel hint = GlyphLabel("tap ") + editToken + GlyphLabel(" to add your controls");
        auto* hintCtl = new GlyphLabelControl(contentR, hint, hintStyle);
        hintCtl->setRunGap(_labelRunGap).setBackgroundColor(COLOR_TRANSPARENT);
        AddChildControl(hintCtl);
        // Lock button anchored to the content's top-right corner.
        AddChildControl(makeLockBtn(content.GetFromTRHC(_menuBtnW, kEditHeaderH)));
        pG->SetAllControlsDirty();
        return;
    }

    // Per-column edit chrome (the "+") lives in chromeBand (the gained band above the content when
    // expanded). Content fills the attach bounds. The lock/menu column anchors to the content top-right.
    const IRECT headerRow = chromeBand;
    const IRECT contentR  = content;

    // Total columns: one per slot + optional "new slot" column in edit mode
    const int nCols = (int)_slots.size() + (_unlocked ? 1 : 0);
    if (nCols == 0) { pG->SetAllControlsDirty(); return; }

    // Column width weights: unit (1.0) unless a column-width modifier narrows the column.
    std::vector<float> slotWeights(_slots.size(), 1.f);
    for (int s = 0; s < (int)_slots.size(); ++s)
        slotWeights[s] = slotWidthWeight(_slots[s]);
    float totalWeight = 0.f;
    for (float w : slotWeights) totalWeight += w;

    // The trailing "add a column" slot is a fixed-width strip (setAddColumnWidth), not a full weighted
    // column — the real columns share all the remaining width by weight. The strip reserves the menu
    // column it sits under, so _addColumnWidth is the VISIBLE width left of the menu (where the "+"
    // is centred, see the makePlusBtn call below).
    const float availW  = contentR.W() - kSlotGap * (nCols - 1);
    const float addColW = _unlocked ? std::min(_addColumnWidth + _menuBtnW, availW) : 0.f;
    const float slotsW  = availW - addColW;

    // Pre-compute column left edges and widths proportional to weight.
    std::vector<float> colWidths(nCols), colLefts(nCols);
    colLefts[0] = contentR.L;
    for (int s = 0; s < (int)_slots.size(); ++s)
        colWidths[s] = totalWeight > 0.f ? slotsW * slotWeights[s] / totalWeight : slotsW;
    if (_unlocked)
        colWidths[nCols - 1] = addColW;
    for (int i = 1; i < nCols; ++i)
        colLefts[i] = colLefts[i-1] + colWidths[i-1] + kSlotGap;

    auto colRect = [&](int col, const IRECT& row) -> IRECT {
        return { colLefts[col], row.T, colLefts[col] + colWidths[col], row.B };
    };

    // Slot-column outline in the accent color at reduced opacity (cell outlines are drawn by the
    // per-cell CellOverlay, which also carries the drag / remove interaction).
    const IColor kSlotOutlineColor { 140, _editColor.R, _editColor.G, _editColor.B };

    // Cell geometry for drag-to-reorder hit-testing + the insertion indicator; refilled every rebuild.
    _colContentRects.assign(_slots.size(), IRECT{});
    _cellRects.assign(_slots.size(), {});
    _hoverKey = nullptr;   // the overlays it pointed at are being recreated

    for (int s = 0; s < (int)_slots.size(); ++s) {
        const IRECT sliceContent = colRect(s, contentR);
        _colContentRects[s] = sliceContent;
        const auto& slot = _slots[s];
        const bool emptyColumn = !slotHasRealControl(slot);   // only spacers → a blank column
        const int  wIdx        = widthModIndex(slot);

        // Header "+": add a control OR a width modifier to this column. Shown whenever something can
        // be added — a control (there is room) or a width modifier (the column has none yet). So a
        // full column still shows it (for the width modifier), and a fresh empty spacer column still
        // shows it (so controls can be dropped in). The width-spacer marker is NOT here — it sits at
        // the bottom of the column (below), to keep the header uncluttered.
        const bool hasWidthMod = wIdx >= 0;
        const bool canAddMore = std::any_of(
            _factories.begin(), _factories.end(),
            [&](const ControlRegistry::Entry& e) {
                return !isPad(e.id) && slot.hasRoom(_factories, e.desc.heightFrac);
            });
        if (_unlocked && (canAddMore || !hasWidthMod))
            AddChildControl(makePlusBtn(colRect(s, headerRow), s));

        // Column outline UNDER the cells (added before them → cells draw on top), so a cell's white
        // hover outline is never overpainted by the accent column frame at a shared edge.
        if (_unlocked)
            AddChildControl(new BorderOverlay(sliceContent, kSlotOutlineColor));

        // Count valid, laid-out entries and split frac totals (the width modifier takes no height).
        int   nValid     = 0;
        float spacerFrac = 0.f;
        float realFrac   = 0.f;
        for (int p : slot.params) {
            if (!_factories.has(p) || isColWidth(p)) continue;
            ++nValid;
            if (isPad(p)) spacerFrac += _factories.at(p).heightFrac;
            else          realFrac   += _factories.at(p).heightFrac;
        }
        if (realFrac < 1e-4f) realFrac = 1.f;

        const float slotH        = sliceContent.H();
        const float spacerPx     = spacerFrac * slotH;
        const float availH       = slotH - kEntryGap * std::max(nValid - 1, 0);
        const float availForReal = availH - spacerPx;

        float yPx = 0.f;
        for (int e = 0; e < (int)slot.params.size(); ++e) {
            const int paramId = slot.params[e];
            if (!_factories.has(paramId) || isColWidth(paramId)) continue;  // width mod: no cell, no height

            const auto& desc   = _factories.at(paramId);
            const float entryH = isPad(paramId)
                ? (emptyColumn ? slotH : desc.heightFrac * slotH)
                : (desc.heightFrac / realFrac) * availForReal;
            const IRECT entryR {
                sliceContent.L, sliceContent.T + yPx,
                sliceContent.R, sliceContent.T + yPx + entryH
            };

            AddChildControl(desc.factory(entryR));

            // One CellOverlay per laid-out cell (real controls AND visible spacers alike, so the reorg
            // gesture is uniform): it outlines the cell, hosts the ✕ remove + ✥ move affordances, and
            // drives drag-to-reorder. Record the cell rect for the drag hit-testing / indicator.
            if (_unlocked) {
                _cellRects[s].push_back({ e, entryR });
                AddChildControl(makeCellOverlay(entryR, s, e));
            }

            yPx += entryH + kEntryGap;
        }

        // A spacer / empty column has no laid-out cell of its own, so nothing would carry the reorder
        // gesture. Give it a whole-column drag handle so it moves like any other column. Added BEFORE
        // the width-mod marker below, so that marker stays on top and clickable.
        if (_unlocked && _cellRects[s].empty())
            AddChildControl(makeCellOverlay(sliceContent, s, wIdx, /*columnHandle*/ true));

        // Column-width spacer marker: centred just BELOW the column (mirroring the top "+"), in the
        // gained bottom band so it never overlaps a control. Without a bottom band (host didn't grant
        // one) it falls back to the bottom edge inside the column.
        if (_unlocked && hasWidthMod) {
            const IRECT markerBand = expandDown
                ? IRECT(sliceContent.L, content.B, sliceContent.R, content.B + _editExpandBottom)
                : IRECT(sliceContent.L, sliceContent.B - std::min(sliceContent.H(), kPlusBtnMaxH),
                        sliceContent.R, sliceContent.B);
            AddChildControl(makeWidthModBtn(markerBand, s));
        }
    }

    // Global "new slot" "+" — edit mode only. With columns present it sits in the trailing add-column
    // strip (rightmost); the right-edge menu column (lock/save/…) overlays that strip, so reserve its
    // width first (shifts the glyph half the menu width left, into the visible area). With NO columns
    // yet, that strip would be a narrow band pinned at the left edge — instead centre the "+" in the
    // whole content area so an empty layout reads as "tap here to start".
    if (_unlocked) {
        const IRECT addArea = _slots.empty()
            ? contentR
            : colRect(nCols - 1, contentR).GetReducedFromRight(_menuBtnW);
        AddChildControl(makePlusBtn(addArea, -1));
    }

    // Lock button added last → always on top of content controls in z-order.
    const IRECT lockR = content.GetFromTRHC(_menuBtnW, kEditHeaderH);
    AddChildControl(makeLockBtn(lockR));

    // Buttons below the lock button, grouped with half-button gaps between groups:
    //   lock  |  save / load  |  [copy / paste]  |  clear  |  undo   (copy/paste gated by _clipboardEnabled)
    if (_unlocked) {
        const float kHGap = 0.5f * kEditHeaderH;
        float y = kEditHeaderH + kHGap;       // gap after lock
        AddChildControl(makeSaveBtn (lockR.GetVShifted(y))); y += kEditHeaderH;
        AddChildControl(makeLoadBtn (lockR.GetVShifted(y))); y += kEditHeaderH + kHGap;
        if (_clipboardEnabled) {   // clipboard copy/paste gated (paid); file save/load stay free
            AddChildControl(makeCopyBtn (lockR.GetVShifted(y))); y += kEditHeaderH;
            AddChildControl(makePasteBtn(lockR.GetVShifted(y))); y += kEditHeaderH + kHGap;
        }
        AddChildControl(makeClearBtn(lockR.GetVShifted(y))); y += kEditHeaderH + kHGap;
        AddChildControl(makeUndoBtn (lockR.GetVShifted(y)));
    }

    // Topmost: all the white drag/hover chrome + the "hold shift…" hint (non-interactive, so it never
    // blocks the cells or menu, nor a width-spacer marker sharing the bottom band).
    if (_unlocked)
        AddChildControl(new DragIndicator(this, b));

    // The control clones were just recreated → they start at 0. Push the current param values into them
    // (as OnUIOpen does) so a reorg/add/remove doesn't flash a control to zero — most visible under an
    // active morph, where a selected point's values live only in the base params until this re-sync.
    if (auto* dg = GetDelegate())
        dg->SendCurrentParamValuesFromDelegate();
    if (_onRebuilt)
        _onRebuilt();   // host layers effective values (e.g. morph blend) over the base just pushed

    pG->SetAllControlsDirty();
}

} // namespace hvoya::ui
