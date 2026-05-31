#pragma once

/* preset_strip_control.hpp — horizontal preset strip for iPlug2 UIs
 *
 * Collapsed (default):
 *
 *   [presets >>]
 *
 * Expanded (click [presets] to toggle):
 *
 *   [<<]  [<]  [ group/name ]  [>]  [undo]  [save]  [load]  [dir]  [scan]
 *
 *   presets — collapse/expand toggle
 *   </>     — cycle through presets (factory + user)
 *   name    — current preset, or "user preset" when the patch is custom
 *             (edited, randomized or mutated)
 *   undo    — revert last preset switch (dimmed when unavailable)
 *   save    — PromptForFile(Save) → saveToFile
 *   load    — PromptForFile(Open) → loadFromFile
 *   dir     — shell-opens last-used preset directory
 *   scan    — PromptForDirectory → addFolder
 *
 * USAGE
 * -----
 *   pG->AttachControl(
 *       new hvoya::ui::PresetStripControl(stripRect, *_presetManager, style),
 *       -1, kGroupALL);
 *
 * The control does NOT own the PresetManager — it holds a reference.
 * Collapsed/expanded state is not serialized.
 */

#include <IControl.h>
#include <IControls.h>
#include <filesystem>
#include <functional>
#include <hvoya/utils/preset_manager.hpp>

namespace hvoya::ui {

class PresetStripControl : public IControl, public IVectorBase {
public:
    PresetStripControl(const IRECT&    bounds,
                       PresetManager&  manager,
                       const IVStyle&  style = DEFAULT_STYLE)
        : IControl   (bounds)
        , IVectorBase(style)
        , _manager   (manager)
    {
        AttachIControl(this, "");
    }

    // ── Color helpers ─────────────────────────────────────────────────────────

    PresetStripControl& setBackgroundColor (const IColor& c) { mStyle.colorSpec.mColors[kBG] = c; return *this; }
    PresetStripControl& setMouseOverColor  (const IColor& c) { mStyle.colorSpec.mColors[kHL] = c; return *this; }
    PresetStripControl& setPressedColor    (const IColor& c) { mStyle.colorSpec.mColors[kPR] = c; return *this; }
    PresetStripControl& setFrameColor      (const IColor& c) { mStyle.colorSpec.mColors[kFR] = c; return *this; }
    PresetStripControl& setTextColor       (const IColor& c) { mStyle.valueText.mFGColor = c;     return *this; }

    PresetStripControl& setCollapsedLabel        (const char* s) { _collapsedLabel    = s;  return *this; }
    PresetStripControl& setExpandedLabel         (const char* s) { _expandedLabel     = s;  return *this; }
    PresetStripControl& setCollapsedToggleWidth  (float px)      { _collapsedToggleW  = px; return *this; }
    PresetStripControl& setExpandedToggleWidth   (float px)      { _expandedToggleW   = px; return *this; }

    PresetStripControl& setShowSaveLoad    (bool v)          { _showSaveLoad = v;  return *this; }
    PresetStripControl& setShowUndo        (bool v)          { _showUndo     = v;  return *this; }
    PresetStripControl& setShowDir         (bool v)          { _showDir      = v;  return *this; }
    PresetStripControl& setShowScan        (bool v)          { _showScan     = v;  return *this; }

    // Called whenever the strip is toggled. `collapsed` reflects the NEW state.
    // Use to show/hide companion controls (e.g. CC save/load buttons).
    PresetStripControl& setOnToggle (std::function<void(bool collapsed)> fn) {
        _onToggle = std::move(fn); return *this;
    }

    // ── Drawing ───────────────────────────────────────────────────────────────

    void Draw(IGraphics& g) override {
        const auto zones = computeZones();

        g.FillRect(GetColor(kBG), mRECT, &mBlend);

        auto hov = [&](Zone z) { return _hoverZone   == z; };
        auto prs = [&](Zone z) { return _pressedZone == z; };

        // Toggle button — always visible
        const std::string& toggleLabel = _collapsed ? _collapsedLabel : _expandedLabel;
        drawBtn(g, zones.toggle, toggleLabel.c_str(), hov(Zone::Toggle), false, prs(Zone::Toggle));

        if (_collapsed) return;

        drawBtn(g, zones.prev, "<",  hov(Zone::Prev), false, prs(Zone::Prev));
        drawBtn(g, zones.next, ">",  hov(Zone::Next), false, prs(Zone::Next));
        if (_showUndo)    drawBtn(g, zones.undo,   "undo", hov(Zone::Undo),   !_manager.canUndo(), prs(Zone::Undo));
        if (_showSaveLoad){ drawBtn(g, zones.save, "save", hov(Zone::Save),   false, prs(Zone::Save));
                            drawBtn(g, zones.load, "load", hov(Zone::Load),   false, prs(Zone::Load)); }
        if (_showDir)     drawBtn(g, zones.folder, "dir",  hov(Zone::Folder), false, prs(Zone::Folder));
        if (_showScan)    drawBtn(g, zones.scan,   "scan", hov(Zone::Scan),   false, prs(Zone::Scan));

        // Name label — stretches between nav and action buttons. A custom patch
        // (edited / randomized / mutated) shows as "user preset".
        std::string label;
        if (_manager.isCustomPatch()) {
            label = "user preset";
        } else {
            const std::string name  = _manager.currentName();
            const std::string group = _manager.currentGroup();
            label = group.empty() ? name : group + "/" + name;
        }
        if (_hoverZone == Zone::Name)
            g.FillRect(GetColor(kHL), zones.name, &mBlend);
        g.DrawText(mStyle.valueText
                       .WithAlign(EAlign::Center)
                       .WithVAlign(EVAlign::Middle),
                   label.c_str(), zones.name, &mBlend);
    }

    // ── Mouse ─────────────────────────────────────────────────────────────────

    void OnMouseDown(float x, float y, const IMouseMod& mod) override {
        const Zone z = hitZone(x, y);
        if (_pressedZone != z) { _pressedZone = z; SetDirty(false); }
        switch (z) {
            case Zone::Toggle:
                _collapsed = !_collapsed;
                if (_onToggle) _onToggle(_collapsed);
                SetDirty(false);
                break;
            case Zone::Prev:   _manager.prev();          SetDirty(false); break;
            case Zone::Next:   _manager.next();          SetDirty(false); break;
            case Zone::Undo:   _manager.undo();          SetDirty(false); break;
            case Zone::Save:   promptSave();                              break;
            case Zone::Load:   promptLoad();                              break;
            case Zone::Folder: _manager.openFolder();                     break;
            case Zone::Scan:   promptScanFolder();                        break;
            default: break;
        }
    }

    void OnMouseUp(float x, float y, const IMouseMod&) override {
        if (_pressedZone != Zone::None) { _pressedZone = Zone::None; SetDirty(false); }
    }

    void OnMouseOver(float x, float y, const IMouseMod&) override {
        const Zone z = hitZone(x, y);
        if (z != _hoverZone) { _hoverZone = z; SetDirty(false); }
    }

    void OnMouseOut() override {
        if (_hoverZone   != Zone::None) { _hoverZone   = Zone::None; SetDirty(false); }
        if (_pressedZone != Zone::None) { _pressedZone = Zone::None; SetDirty(false); }
    }

    bool IsDirty() override { return true; }

private:
    // ── Zone geometry ─────────────────────────────────────────────────────────

    enum class Zone { None, Toggle, Prev, Name, Next, Undo, Save, Load, Folder, Scan };

    struct Zones {
        IRECT toggle, prev, name, next, undo, save, load, folder, scan;
        // Unset zones are default-constructed (zero-size IRECT), safe to ignore.
    };

    Zones computeZones() const {
        const float btnW            = mRECT.H() * 1.8f;   // square-ish nav buttons
        const float wideW           = mRECT.H() * 2.8f;   // save / load / scan / undo
        const float toggleCollapsed = _collapsedToggleW > 0 ? _collapsedToggleW : mRECT.H() * 4.5f;
        const float toggleExpanded  = _expandedToggleW  > 0 ? _expandedToggleW  : mRECT.H() * 1.2f;

        float x = mRECT.L;
        auto slice = [&](float w) -> IRECT {
            IRECT s(x, mRECT.T, x + w, mRECT.B);
            x += w;
            return s;
        };

        Zones z;
        z.toggle = slice(_collapsed ? toggleCollapsed : toggleExpanded);

        if (_collapsed) return z;   // remaining zones stay zero-size

        z.prev = slice(btnW);

        // Right-side buttons, reserved from right edge inward
        float rx = mRECT.R;
        auto rslice = [&](float w) -> IRECT {
            rx -= w;
            return IRECT(rx, mRECT.T, rx + w, mRECT.B);
        };
        if (_showScan)   z.scan   = rslice(wideW);
        if (_showDir)    z.folder = rslice(btnW);
        if (_showSaveLoad) { z.load = rslice(wideW); z.save = rslice(wideW); }
        if (_showUndo)   z.undo   = rslice(wideW);
        z.next = rslice(btnW);

        z.name = IRECT(x, mRECT.T, rx, mRECT.B);
        return z;
    }

    Zone hitZone(float x, float y) const {
        const auto z = computeZones();
        if (z.toggle.Contains(x, y)) return Zone::Toggle;
        if (_collapsed) return Zone::None;
        if (z.prev  .Contains(x, y)) return Zone::Prev;
        if (z.next  .Contains(x, y)) return Zone::Next;
        if (z.undo  .Contains(x, y)) return Zone::Undo;
        if (z.save  .Contains(x, y)) return Zone::Save;
        if (z.load  .Contains(x, y)) return Zone::Load;
        if (z.folder.Contains(x, y)) return Zone::Folder;
        if (z.scan  .Contains(x, y)) return Zone::Scan;
        if (z.name  .Contains(x, y)) return Zone::Name;
        return Zone::None;
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    void drawBtn(IGraphics& g, const IRECT& r, const char* label,
                 bool hover, bool disabled = false, bool pressed = false) const {
        if (!disabled) {
            if (pressed)    g.FillRect(GetColor(kPR), r, &mBlend);
            else if (hover) g.FillRect(GetColor(kHL), r, &mBlend);
        }
        const IColor frame = GetColor(kFR);
        if (frame.A > 0)
            g.DrawRect(disabled ? frame.WithOpacity(0.35f) : frame, r, &mBlend, 1.f);
        const float txtOpacity = disabled ? 0.35f : 1.f;
        g.DrawText(mStyle.valueText
                       .WithFGColor(mStyle.valueText.mFGColor.WithOpacity(txtOpacity))
                       .WithAlign(EAlign::Center)
                       .WithVAlign(EVAlign::Middle),
                   label, r, &mBlend);
    }

    void promptSave() {
        auto* pG = GetUI();
        if (!pG) return;
        std::filesystem::create_directories(_manager.presetDir());
        WDL_String fileName;
        WDL_String path; path.Set(_manager.browseDir().c_str());
        pG->PromptForFile(fileName, path, EFileAction::Save, "fxp",
            [this](const WDL_String& fn, const WDL_String&) {
                if (!fn.GetLength()) return;
                _manager.saveToFile(fn.Get());
                SetDirty(false);
            });
    }

    void promptLoad() {
        auto* pG = GetUI();
        if (!pG) return;
        WDL_String fileName;
        WDL_String path; path.Set(_manager.browseDir().c_str());
        pG->PromptForFile(fileName, path, EFileAction::Open, "fxp",
            [this](const WDL_String& fn, const WDL_String&) {
                if (!fn.GetLength()) return;
                _manager.loadFromFile(fn.Get());
                SetDirty(false);
            });
    }

    void promptScanFolder() {
        auto* pG = GetUI();
        if (!pG) return;
        WDL_String dir; dir.Set(_manager.browseDir().c_str());
        pG->PromptForDirectory(dir,
            [this](const WDL_String&, const WDL_String& d) {
                if (!d.GetLength()) return;
                _manager.addFolder(d.Get());
                SetDirty(false);
            });
    }

    PresetManager&                  _manager;
    std::function<void(bool)>       _onToggle;
    Zone                            _hoverZone      = Zone::None;
    Zone                            _pressedZone    = Zone::None;
    bool           _collapsed      = true;
    std::string    _collapsedLabel = "presets >>";
    std::string    _expandedLabel  = "<<";
    bool           _showSaveLoad    = true;
    bool           _showUndo        = true;
    bool           _showDir         = true;
    bool           _showScan        = true;
    float          _collapsedToggleW = 0;   // 0 = auto (H * 4.5)
    float          _expandedToggleW  = 0;   // 0 = auto (H * 1.2)
};

} // namespace hvoya::ui
