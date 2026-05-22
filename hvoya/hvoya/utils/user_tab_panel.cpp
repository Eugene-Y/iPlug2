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

// Thin transparent border drawn on top of a control rect in edit mode.
// IsHit returns false so mouse events pass through to the controls beneath.
struct BorderOverlay : public IControl {
    IColor mBorderColor;
    BorderOverlay(const IRECT& r, IColor col) : IControl(r), mBorderColor(col) {}
    void Draw(IGraphics& g) override { g.DrawRect(mBorderColor, mRECT, nullptr, 1.f); }
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
                           std::string fileExt)
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
    _editBtnStyle.labelText.mSize   = _btnStyle.labelText.mSize * 2.f;
    _editBtnStyle.labelText.mVAlign = EVAlign::Middle;
    _editBtnStyle.labelText.mAlign  = EAlign::Center;
    _editBtnStyle.labelText.mFGColor = editColor;

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


void UserTabPanel::OnPopupMenuSelection(IPopupMenu* pMenu, int) {
    if (!pMenu || pMenu->GetChosenItemIdx() < 0) {
        rebuild();  // reset button pressed states after dismissed-without-selection
        return;
    }

    const int idx = pMenu->GetChosenItemIdx();
    if (idx >= (int)_pickerParamOrder.size()) { rebuild(); return; }

    const int paramId = _pickerParamOrder[idx];
    if (paramId == kPickerSepSentinel) { rebuild(); return; }  // separator — not selectable

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

void UserTabPanel::moveSlot(int slotIdx, int delta) {
    const int target = slotIdx + delta;
    if (target < 0 || target >= (int)_slots.size()) return;
    pushHistory();
    std::swap(_slots[slotIdx], _slots[target]);
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
    _pickerParamOrder.clear();

    // collect already-used params so duplicates in the target slot can be disabled
    std::set<int> usedInTargetSlot;
    if (slotIdx >= 0 && slotIdx < (int)_slots.size())
        for (int p : _slots[slotIdx].params)
            usedInTargetSlot.insert(p);

    _pickerMenu.Clear();

    // Real parameters first (non-padding, insertion order).
    for (auto& [paramId, desc] : _factories) {
        if (isPad(paramId)) continue;
        auto* item = _pickerMenu.AddItem(desc.displayName.c_str());
        bool noRoom = slotIdx >= 0 && !_slots[slotIdx].hasRoom(_factories, desc.heightFrac);
        if (usedInTargetSlot.count(paramId) || noRoom)
            item->SetEnabled(false);
        _pickerParamOrder.push_back(paramId);
    }

    // Padding spacers at the bottom, separated.
    // AddSeparator() occupies a menu index on macOS — push a sentinel so indices stay aligned.
    _pickerMenu.AddSeparator();
    _pickerParamOrder.push_back(kPickerSepSentinel);  // separator occupies a menu index on macOS
    for (int padId : { kPad1_8, kPad1_4, kPad1_2 }) {
        const auto& desc = _factories.at(padId);
        auto* item = _pickerMenu.AddItem(desc.displayName.c_str());
        if (slotIdx >= 0) {
            // Inside an existing slot: disable if no room or slot is already pad-only.
            bool padOnly = _slots[slotIdx].params.size() == 1 && isPad(_slots[slotIdx].params[0]);
            bool noRoom  = !_slots[slotIdx].hasRoom(_factories, desc.heightFrac);
            if (padOnly || noRoom) item->SetEnabled(false);
        }
        // slotIdx == -1 (new slot): padding creates a narrow column — always enabled.
        _pickerParamOrder.push_back(padId);
    }

    GetUI()->CreatePopupMenu(*this, _pickerMenu, fromRect);
}


// ──────────────────────────────────────────────
//  Child control factories

IControl* UserTabPanel::makeLockBtn(const IRECT& r) {
    return new IVButtonControl(r,
        [this](IControl*) { toggleLock(); },
        _unlocked ? "lock" : "edit",
        _btnStyle);
}

IControl* UserTabPanel::makePlusBtn(const IRECT& r, int slotIdx) {
    // Centre a fixed-size button inside the available area so "+" is mid-screen.
    const IRECT btnR = r.GetCentredInside(std::min(r.W(), 80.f), std::min(r.H(), 40.f));
    return new IVButtonControl(btnR,
        [this, slotIdx, btnR](IControl*) { showParamPicker(slotIdx, btnR); },
        "+",
        _editBtnStyle);
}

IControl* UserTabPanel::makeRemoveBtn(const IRECT& r, int slotIdx, int entryIdx) {
    return new IVButtonControl(r,
        [this, slotIdx, entryIdx](IControl*) { removeEntry(slotIdx, entryIdx); },
        "x",
        _editBtnStyle);
}

IControl* UserTabPanel::makeMoveBtn(const IRECT& r, int slotIdx, int delta, const char* label) {
    return new IVButtonControl(r,
        [this, slotIdx, delta](IControl*) { moveSlot(slotIdx, delta); },
        label,
        _editBtnStyle);
}

IControl* UserTabPanel::makeSaveBtn(const IRECT& r) {
    return new IVButtonControl(r,
        [this](IControl* pBtn) {
            pBtn->SetValue(0.);
            pBtn->SetDirty(false);
            WDL_String fileName("layout"), path;
            GetUI()->PromptForFile(fileName, path, EFileAction::Save, _fileExt.c_str(),
                [this](const WDL_String& fn, const WDL_String&) {
                    if (fn.GetLength())
                        saveLayout(fn.Get());
                });
        },
        "save",
        _btnStyle);
}

IControl* UserTabPanel::makeLoadBtn(const IRECT& r) {
    return new IVButtonControl(r,
        [this](IControl* pBtn) {
            pBtn->SetValue(0.);
            pBtn->SetDirty(false);
            WDL_String fileName, path;
            GetUI()->PromptForFile(fileName, path, EFileAction::Open, _fileExt.c_str(),
                [this](const WDL_String& fn, const WDL_String&) {
                    if (fn.GetLength())
                        loadLayout(fn.Get());
                });
        },
        "load",
        _btnStyle);
}

IControl* UserTabPanel::makeCopyBtn(const IRECT& r) {
    return new IVButtonControl(r,
        [this](IControl* pBtn) {
            pBtn->SetValue(0.);
            pBtn->SetDirty(false);
            copyToClipboard();
        },
        "copy",
        _btnStyle);
}

IControl* UserTabPanel::makePasteBtn(const IRECT& r) {
    return new IVButtonControl(r,
        [this](IControl* pBtn) {
            pBtn->SetValue(0.);
            pBtn->SetDirty(false);
            pasteFromClipboard();
        },
        "paste",
        _btnStyle);
}

IControl* UserTabPanel::makeClearBtn(const IRECT& r) {
    auto* btn = new IVButtonControl(r,
        [this](IControl* pBtn) {
            pBtn->SetValue(0.);
            pBtn->SetDirty(false);
            clearSlots();
        },
        "clear",
        _btnStyle);
    if (_slots.empty())
        btn->SetDisabled(true);
    return btn;
}

IControl* UserTabPanel::makeUndoBtn(const IRECT& r) {
    auto* btn = new IVButtonControl(r,
        [this](IControl* pBtn) {
            pBtn->SetValue(0.);
            pBtn->SetDirty(false);
            undo();
        },
        "undo",
        _btnStyle);
    if (_history.empty())
        btn->SetDisabled(true);
    return btn;
}


// ──────────────────────────────────────────────
//  Rebuild

void UserTabPanel::clearChildren() {
    auto* pG = GetUI();
    if (!pG) { mChildren.Empty(false); return; }
    for (int i = NChildren() - 1; i >= 0; --i) {
        pG->RemoveControl(GetChild(i));  // deletes the control
        mChildren.Delete(i, false);       // remove stale ptr from our list
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

    // Gap between adjacent slots (horizontal) and between stacked entries (vertical).
    constexpr float kGap = 5.f;

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
    const float availW = contentR.W() - kGap * (nCols - 1);
    std::vector<float> colWidths(nCols), colLefts(nCols);
    colLefts[0] = contentR.L;
    for (int s = 0; s < (int)_slots.size(); ++s)
        colWidths[s] = availW * slotWeights[s] / totalWeight;
    if (_unlocked)
        colWidths[nCols - 1] = availW * 1.f / totalWeight;
    for (int i = 1; i < nCols; ++i)
        colLefts[i] = colLefts[i-1] + colWidths[i-1] + kGap;

    auto colRect = [&](int col, const IRECT& row) -> IRECT {
        return { colLefts[col], row.T, colLefts[col] + colWidths[col], row.B };
    };

    // Outlines in the accent color at reduced opacity.
    const IColor kSlotOutlineColor  { 140, _editColor.R, _editColor.G, _editColor.B };
    const IColor kEntryOutlineColor {  80, _editColor.R, _editColor.G, _editColor.B };

    for (int s = 0; s < (int)_slots.size(); ++s) {
        const IRECT sliceHeader  = _unlocked ? colRect(s, headerRow) : IRECT();
        const IRECT sliceContent = colRect(s, contentR);
        const auto& slot = _slots[s];

        // Header row: [<]  [+ if room and not pad-only]  [>]
        const bool padOnly = slot.params.size() == 1 && isPad(slot.params[0]);
        if (_unlocked) {
            if (s > 0)
                AddChildControl(makeMoveBtn(
                    sliceHeader.SubRectHorizontal(3, 0), s, -1, "<"));
            if (!padOnly && slot.hasRoom(_factories, 0.125f))
                AddChildControl(makePlusBtn(
                    sliceHeader.SubRectHorizontal(3, 1), s));
            if (s < (int)_slots.size() - 1)
                AddChildControl(makeMoveBtn(
                    sliceHeader.SubRectHorizontal(3, 2), s, +1, ">"));
        }

        // Count valid entries and split frac totals: spacers take an absolute slice of
        // sliceContent.H(); real controls share the remainder proportionally.
        int   nValid        = 0;
        float spacerFrac    = 0.f;  // sum of padding heightFracs
        float realFrac      = 0.f;  // sum of real control heightFracs
        for (int p : slot.params) {
            if (!_factories.has(p)) continue;
            ++nValid;
            if (isPad(p)) spacerFrac += _factories.at(p).heightFrac;
            else          realFrac   += _factories.at(p).heightFrac;
        }
        if (realFrac < 1e-4f) realFrac = 1.f;  // pad-only slot: avoid div-by-zero

        const float slotH        = sliceContent.H();
        const float spacerPx     = spacerFrac * slotH;  // spacers: absolute fraction of slot
        const float availH       = slotH - kGap * std::max(nValid - 1, 0);
        const float availForReal = availH - spacerPx;   // real controls share what's left

        float yPx = 0.f;
        for (int e = 0; e < (int)slot.params.size(); ++e) {
            const int paramId = slot.params[e];
            if (!_factories.has(paramId)) continue;

            const auto& desc   = _factories.at(paramId);
            // Spacers:
            //   standalone slot (padOnly) — the column IS the spacer; entry fills full height.
            //   stacked inside a slot    — absolute fraction of slot height.
            // Real controls: proportional share of remaining height.
            const float entryH = isPad(paramId)
                ? (padOnly ? slotH : desc.heightFrac * slotH)
                : (desc.heightFrac / realFrac) * availForReal;
            const IRECT entryR {
                sliceContent.L,
                sliceContent.T + yPx,
                sliceContent.R,
                sliceContent.T + yPx + entryH
            };

            AddChildControl(desc.factory(entryR));

            if (_unlocked) {
                // For pad-only slots the entry rect equals the full slot rect, so the
                // entry outline would be identical to the slot outline — skip it to avoid
                // drawing two borders on top of each other.
                if (!padOnly)
                    AddChildControl(new BorderOverlay(entryR, kEntryOutlineColor));
                const IRECT removeR = entryR.GetFromTRHC(kSmallBtnW, kEditHeaderH);
                AddChildControl(makeRemoveBtn(removeR, s, e));
            }

            yPx += entryH + kGap;
        }

        // Slot outline drawn after all entries so it sits on top of the entry outlines.
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
