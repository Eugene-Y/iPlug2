#include "user_tab_panel.hpp"
#include <IControls.h>
#include <IGraphics.h>
#include <IGraphicsPopupMenu.h>
#include <hvoya/utils/hvoya_file.hpp>
#include <set>
#include <algorithm>
#include <sstream>

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
    _editBtnStyle = _btnStyle;
    _editBtnStyle.labelText.mSize    = _btnStyle.labelText.mSize * 2.f;
    _editBtnStyle.labelText.mVAlign  = EVAlign::Middle;
    _editBtnStyle.labelText.mAlign   = EAlign::Center;
    _editBtnStyle.labelText.mFGColor = editColor;

    _entryBtnStyle = _editBtnStyle;
    _entryBtnStyle.labelText.mSize   = _editBtnStyle.labelText.mSize * 0.75f;

    _swapBtnStyle = _editBtnStyle;
    if (iconFontName)
        strncpy(_swapBtnStyle.labelText.mFont, iconFontName, sizeof(_swapBtnStyle.labelText.mFont) - 1);

    if (removeFontName)
        strncpy(_entryBtnStyle.labelText.mFont, removeFontName, sizeof(_entryBtnStyle.labelText.mFont) - 1);

    // Register built-in padding spacers. Negative IDs never conflict with real params.
    // heightFrac doubles as widthFrac when the entry is the sole occupant of a slot.
    auto addPad = [this](int id, float frac, const char* name) {
        _factories.add(id, { [](const IRECT& r) -> IControl* { return new PaddingControl(r); },
                             frac, name });
    };
    addPad(kPad1_8, 0.125f, "space  1/8");
    addPad(kPad1_4, 0.25f,  "space  1/4");
    addPad(kPad1_2, 0.5f,   "space  1/2");

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
    if (pruned) notifyChanged();
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
    _slots[slotIdx].params.push_back(paramId);
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

void UserTabPanel::swapSlots(int slotIdx) {
    if (slotIdx < 0 || slotIdx + 1 >= (int)_slots.size()) return;
    pushHistory();
    std::swap(_slots[slotIdx], _slots[slotIdx + 1]);
    notifyChanged();
    rebuild();
}

void UserTabPanel::swapEntries(int slotIdx, int entryIdx) {
    assert(slotIdx >= 0 && slotIdx < (int)_slots.size());
    auto& slot = _slots[slotIdx];
    if (entryIdx < 0 || entryIdx + 1 >= (int)slot.params.size()) return;
    pushHistory();
    std::swap(slot.params[entryIdx], slot.params[entryIdx + 1]);
    notifyChanged();
    rebuild();
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
        for (int p : slot.params)
            os << ' ' << p;
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
        int p;
        while (ss >> p)
            slot.params.push_back(p);
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

    // Padding spacers at the bottom, separated.
    // Items are created with an explicit tag so selection is identified via GetTag(),
    // not by menu index (which is fragile in the presence of separators and submenus).
    _pickerMenu.AddSeparator();
    for (int padId : { kPad1_8, kPad1_4, kPad1_2 }) {
        const auto& desc = _factories.at(padId);
        int flags = IPopupMenu::Item::kNoFlags;
        if (slotIdx >= 0) {
            // Inside an existing slot: disable if no room or the slot is already pad-only.
            bool padOnly = _slots[slotIdx].params.size() == 1 && isPad(_slots[slotIdx].params[0]);
            bool noRoom  = !_slots[slotIdx].hasRoom(_factories, desc.heightFrac);
            if (padOnly || noRoom) flags = IPopupMenu::Item::kDisabled;
        }
        // slotIdx == -1 (new slot): padding creates a narrow column spacer — always enabled.
        _pickerMenu.AddItem(new IPopupMenu::Item(desc.displayName.c_str(), flags, padId + kPickerTagOffset));
    }

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

IControl* UserTabPanel::makeButton(const IRECT& r, const GlyphLabel& label, const char* fallbackText,
                                   const IVStyle& style, std::function<void(IControl*)> onClick) {
    if (label.empty())
        return (new IVButtonControl(r, DefaultClickActionFunc, fallbackText, style))
            ->SetAnimationEndActionFunction(std::move(onClick));

    // GlyphButtonControl draws its text through style.valueText; the IVButton styles
    // configure labelText (the button caption), so mirror it across.
    IVStyle gstyle = style;
    gstyle.valueText = style.labelText;
    auto* b = new GlyphButtonControl(r, label, std::move(onClick), gstyle);
    b->setRunGap(_labelRunGap);
    return b;
}

IControl* UserTabPanel::makeLockBtn(const IRECT& r) {
    return makeButton(r, _unlocked ? _lockLabel : _editLabel, _unlocked ? "lock" : "edit", _btnStyle,
        [this](IControl*) { toggleLock(); });
}

IControl* UserTabPanel::makePlusBtn(const IRECT& r, int slotIdx) {
    const IRECT btnR = r.GetCentredInside(std::min(r.W(), kPlusBtnMaxW), std::min(r.H(), kPlusBtnMaxH));
    return makeButton(btnR, _plusLabel, "+", _editBtnStyle,
        [this, slotIdx, btnR](IControl*) { showParamPicker(slotIdx, btnR); });
}

IControl* UserTabPanel::makeRemoveBtn(const IRECT& r, int slotIdx, int entryIdx) {
    return makeButton(r, _removeLabel, "✕", _entryBtnStyle,
        [this, slotIdx, entryIdx](IControl*) { removeEntry(slotIdx, entryIdx); });
}

IControl* UserTabPanel::makeSwapSlotsBtn(const IRECT& r, int slotIdx) {
    return makeButton(r, _swapSlotsLabel, "◄►", _swapBtnStyle,
        [this, slotIdx](IControl*) { swapSlots(slotIdx); });
}

IControl* UserTabPanel::makeSwapEntriesBtn(const IRECT& r, int slotIdx, int entryIdx) {
    return makeButton(r, _swapEntriesLabel, "▲▼", _swapBtnStyle,
        [this, slotIdx, entryIdx](IControl*) { swapEntries(slotIdx, entryIdx); });
}

IControl* UserTabPanel::makeSaveBtn(const IRECT& r) {
    return makeButton(r, _saveLabel, "save", _btnStyle, [this](IControl*) {
        WDL_String fileName("layout"), path;
        GetUI()->PromptForFile(fileName, path, EFileAction::Save, _fileExt.c_str(),
            [this](const WDL_String& fn, const WDL_String&) {
                if (fn.GetLength()) saveLayout(fn.Get());
            });
    });
}

IControl* UserTabPanel::makeLoadBtn(const IRECT& r) {
    return makeButton(r, _loadLabel, "load", _btnStyle, [this](IControl*) {
        WDL_String fileName, path;
        GetUI()->PromptForFile(fileName, path, EFileAction::Open, _fileExt.c_str(),
            [this](const WDL_String& fn, const WDL_String&) {
                if (fn.GetLength()) loadLayout(fn.Get());
            });
    });
}

IControl* UserTabPanel::makeCopyBtn(const IRECT& r) {
    return makeButton(r, _copyLabel, "copy", _btnStyle, [this](IControl*) { copyToClipboard(); });
}

IControl* UserTabPanel::makePasteBtn(const IRECT& r) {
    return makeButton(r, _pasteLabel, "paste", _btnStyle, [this](IControl*) { pasteFromClipboard(); });
}

IControl* UserTabPanel::makeClearBtn(const IRECT& r) {
    auto* btn = makeButton(r, _clearLabel, "clear", _btnStyle, [this](IControl*) { clearSlots(); });
    if (_slots.empty()) btn->SetDisabled(true);
    return btn;
}

IControl* UserTabPanel::makeUndoBtn(const IRECT& r) {
    auto* btn = makeButton(r, _undoLabel, "undo", _btnStyle, [this](IControl*) { undo(); });
    if (_history.empty()) btn->SetDisabled(true);
    return btn;
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

    const IRECT b = GetRECT();

    if (_slots.empty() && !_unlocked) {
        // Placeholder hint centred in the content area.
        // Derive font from btnStyle so NanoVG is guaranteed to have it loaded.
        const IRECT contentR = b.GetReducedFromTop(kEditHeaderH);
        IText hintText = _btnStyle.labelText;
        hintText.mSize    = 22;
        hintText.mFGColor = IColor(80, 255, 255, 255);
        hintText.mAlign   = EAlign::Center;
        hintText.mVAlign  = EVAlign::Middle;
        AddChildControl(new ITextControl(contentR, "tap 'edit' to add your controls", hintText));
        // Lock button on top.
        AddChildControl(makeLockBtn(b.GetFromTRHC(kLockBtnW, kEditHeaderH)));
        pG->SetAllControlsDirty();
        return;
    }

    // Header row is always reserved (lock button lives there); edit controls shown when unlocked.
    const IRECT headerRow = b.GetFromTop(kEditHeaderH);
    const IRECT contentR  = b.GetReducedFromTop(kEditHeaderH);

    // Total columns: one per slot + optional "new slot" column in edit mode
    const int nCols = (int)_slots.size() + (_unlocked ? 1 : 0);
    if (nCols == 0) { pG->SetAllControlsDirty(); return; }

    // Column width weights: normal slot = 1.0; pad-only slot = its heightFrac (e.g. 0.25).
    std::vector<float> slotWeights(_slots.size(), 1.f);
    for (int s = 0; s < (int)_slots.size(); ++s) {
        const auto& ps = _slots[s].params;
        if (ps.size() == 1 && isPad(ps[0]) && _factories.has(ps[0]))
            slotWeights[s] = _factories.at(ps[0]).heightFrac;
    }
    float totalWeight = 0.f;
    for (float w : slotWeights) totalWeight += w;
    if (_unlocked) totalWeight += 1.f;  // new-slot column has weight 1

    // Pre-compute column left edges and widths proportional to weight.
    const float availW = contentR.W() - kSlotGap * (nCols - 1);
    std::vector<float> colWidths(nCols), colLefts(nCols);
    colLefts[0] = contentR.L;
    for (int s = 0; s < (int)_slots.size(); ++s)
        colWidths[s] = availW * slotWeights[s] / totalWeight;
    if (_unlocked)
        colWidths[nCols - 1] = availW * 1.f / totalWeight;
    for (int i = 1; i < nCols; ++i)
        colLefts[i] = colLefts[i-1] + colWidths[i-1] + kSlotGap;

    auto colRect = [&](int col, const IRECT& row) -> IRECT {
        return { colLefts[col], row.T, colLefts[col] + colWidths[col], row.B };
    };

    // Outlines in the accent color at reduced opacity.
    const IColor kSlotOutlineColor  { 140, _editColor.R, _editColor.G, _editColor.B };
    const IColor kEntryOutlineColor {  80, _editColor.R, _editColor.G, _editColor.B };

    // Swap-slots buttons: one per inter-slot gap, drawn in the header row.
    // The button is wider than kEntryGap and overlaps both adjacent column headers;
    // it is centred on the gap line so it doesn't obstruct the "+" buttons which are
    // centred inside their own (wider) columns.
    if (_unlocked) {
        for (int s = 0; s < (int)_slots.size() - 1; ++s) {
            const float cx = colLefts[s] + colWidths[s] + kSlotGap / 2.f;
            const IRECT swapR { cx - kSwapBtnW / 2.f, headerRow.T,
                                cx + kSwapBtnW / 2.f, headerRow.B };
            AddChildControl(makeSwapSlotsBtn(swapR, s));
        }
    }

    for (int s = 0; s < (int)_slots.size(); ++s) {
        const IRECT sliceContent = colRect(s, contentR);
        const auto& slot = _slots[s];
        const bool padOnly = slot.params.size() == 1 && isPad(slot.params[0]);

        // Header: "+" to add another entry to this slot (centred in the header cell).
        // Show the button if at least one registered (non-pad) item fits — don't use a
        // hardcoded threshold because the smallest item may be smaller than any fixed value.
        const bool canAddMore = std::any_of(
            _factories.begin(), _factories.end(),
            [&](const ControlRegistry::Entry& e) {
                return !isPad(e.id) && slot.hasRoom(_factories, e.desc.heightFrac);
            });
        if (_unlocked && !padOnly && canAddMore)
            AddChildControl(makePlusBtn(colRect(s, headerRow), s));

        // Count valid entries and split frac totals.
        int   nValid     = 0;
        float spacerFrac = 0.f;
        float realFrac   = 0.f;
        for (int p : slot.params) {
            if (!_factories.has(p)) continue;
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
            if (!_factories.has(paramId)) continue;

            const auto& desc   = _factories.at(paramId);
            const float entryH = isPad(paramId)
                ? (padOnly ? slotH : desc.heightFrac * slotH)
                : (desc.heightFrac / realFrac) * availForReal;
            const IRECT entryR {
                sliceContent.L, sliceContent.T + yPx,
                sliceContent.R, sliceContent.T + yPx + entryH
            };

            AddChildControl(desc.factory(entryR));

            if (_unlocked) {
                if (!padOnly)
                    AddChildControl(new BorderOverlay(entryR, kEntryOutlineColor));
                AddChildControl(makeRemoveBtn(entryR.GetFromTRHC(kSmallBtnW, kEditHeaderH), s, e));
            }

            yPx += entryH + kEntryGap;

            // Swap-entries button: same width as the slot swap button, centred horizontally
            // in the slot column and vertically on the gap — overlaps neighbours slightly.
            if (_unlocked && e < (int)slot.params.size() - 1) {
                const float cx = sliceContent.L + sliceContent.W() * kSwapEntryOffX;
                const float cy = sliceContent.T + yPx - kEntryGap / 2.f;
                const IRECT swapR { cx - kSwapBtnW / 2.f, cy - kEditHeaderH / 2.f,
                                    cx + kSwapBtnW / 2.f, cy + kEditHeaderH / 2.f };
                AddChildControl(makeSwapEntriesBtn(swapR, s, e));
            }
        }

        if (_unlocked)
            AddChildControl(new BorderOverlay(sliceContent, kSlotOutlineColor));
    }

    // Global "new slot" column — rightmost, edit mode only; "+" centred in full column
    if (_unlocked) {
        AddChildControl(makePlusBtn(colRect(nCols - 1, contentR), -1));
    }

    // Lock button added last → always on top of content controls in z-order.
    const IRECT lockR = b.GetFromTRHC(kLockBtnW, kEditHeaderH);
    AddChildControl(makeLockBtn(lockR));

    // Buttons below the lock button, grouped with half-button gaps between groups:
    //   lock  |  save / load  |  copy / paste  |  clear  |  undo
    if (_unlocked) {
        const float kHGap = 0.5f * kEditHeaderH;
        float y = kEditHeaderH + kHGap;       // gap after lock
        AddChildControl(makeSaveBtn (lockR.GetVShifted(y))); y += kEditHeaderH;
        AddChildControl(makeLoadBtn (lockR.GetVShifted(y))); y += kEditHeaderH + kHGap;
        AddChildControl(makeCopyBtn (lockR.GetVShifted(y))); y += kEditHeaderH;
        AddChildControl(makePasteBtn(lockR.GetVShifted(y))); y += kEditHeaderH + kHGap;
        AddChildControl(makeClearBtn(lockR.GetVShifted(y))); y += kEditHeaderH + kHGap;
        AddChildControl(makeUndoBtn (lockR.GetVShifted(y)));
    }

    pG->SetAllControlsDirty();
}

} // namespace hvoya::ui
