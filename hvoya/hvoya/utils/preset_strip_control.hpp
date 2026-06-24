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
 *
 * Button labels default to plain text. Any button can be relabelled — including
 * with icon-font glyphs or mixed-font GlyphLabels — via the set*Label setters
 * (the caller registers the fonts and passes the glyph strings).
 */

#include <IControl.h>
#include <IControls.h>
#include <filesystem>
#include <functional>
#include <hvoya/utils/glyph_label.hpp>
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

    PresetStripControl& setCollapsedLabel        (GlyphLabel l)  { _collapsedLabel    = std::move (l); return *this; }
    PresetStripControl& setExpandedLabel         (GlyphLabel l)  { _expandedLabel     = std::move (l); return *this; }
    PresetStripControl& setCollapsedToggleWidth  (float px)      { _collapsedToggleW  = px; return *this; }
    PresetStripControl& setCollapsed             (bool c)        { _collapsed         = c;  return *this; }
    PresetStripControl& setExpandedToggleWidth   (float px)      { _expandedToggleW   = px; return *this; }

    // Per-button labels — plain by default; pass a GlyphLabel for icon / mixed-font glyphs.
    PresetStripControl& setPrevLabel (GlyphLabel l) { _prevLabel = std::move (l); return *this; }
    PresetStripControl& setNextLabel (GlyphLabel l) { _nextLabel = std::move (l); return *this; }
    PresetStripControl& setUndoLabel (GlyphLabel l) { _undoLabel = std::move (l); return *this; }
    PresetStripControl& setRedoLabel (GlyphLabel l) { _redoLabel = std::move (l); return *this; }
    PresetStripControl& setSaveLabel (GlyphLabel l) { _saveLabel = std::move (l); return *this; }
    PresetStripControl& setLoadLabel (GlyphLabel l) { _loadLabel = std::move (l); return *this; }
    PresetStripControl& setDirLabel  (GlyphLabel l) { _dirLabel  = std::move (l); return *this; }
    PresetStripControl& setScanLabel (GlyphLabel l) { _scanLabel = std::move (l); return *this; }
    PresetStripControl& setRunGap    (float px)     { _runGap    = px;            return *this; }

    // Scales the width of the action buttons (undo/redo/save/load/dir/scan) — handy
    // when they carry narrow icon glyphs instead of words. 1.0 = default; <1 narrows.
    PresetStripControl& setActionButtonWidthScale (float s) { _actionWScale = s; return *this; }

    // Sets the action buttons (undo/redo/save/load/dir/scan) to an absolute pixel width,
    // overriding the height-relative scale — use to match a companion button row in the same
    // strip (e.g. CC buttons). 0 (default) keeps the scale-based width.
    PresetStripControl& setActionButtonWidth (float px) { _actionBtnW = px; return *this; }

    PresetStripControl& setShowSaveLoad    (bool v)          { _showSaveLoad = v;  return *this; }
    PresetStripControl& setShowUndo        (bool v)          { _showUndo     = v;  return *this; }
    PresetStripControl& setShowRedo        (bool v)          { _showRedo     = v;  return *this; }
    PresetStripControl& setShowDir         (bool v)          { _showDir      = v;  return *this; }
    PresetStripControl& setShowScan        (bool v)          { _showScan     = v;  return *this; }

    // Called whenever the strip is toggled. `collapsed` reflects the NEW state.
    // Use to show/hide companion controls (e.g. CC save/load buttons).
    PresetStripControl& setOnToggle (std::function<void(bool collapsed)> fn) {
        _onToggle = std::move(fn); return *this;
    }

    // Enables/disables the collapse/expand toggle button. When disabled the button is
    // drawn dimmed and clicks are ignored — the strip stays in its current state. (The
    // host can hold the strip collapsed during a mode where the row is repurposed.)
    PresetStripControl& setToggleEnabled (bool v) { _toggleEnabled = v; SetDirty(false); return *this; }

    // ── Drawing ───────────────────────────────────────────────────────────────

    void Draw(IGraphics& g) override {
        const auto zones = computeZones();

        g.FillRect(GetColor(kBG), mRECT, &mBlend);

        auto hov = [&](Zone z) { return _hoverZone   == z; };
        auto prs = [&](Zone z) { return _pressedZone == z; };

        // Toggle button — always visible (dimmed + inert when disabled)
        drawBtn(g, zones.toggle, _collapsed ? _collapsedLabel : _expandedLabel,
                hov(Zone::Toggle), !_toggleEnabled, prs(Zone::Toggle));

        if (_collapsed) return;

        const bool navOff  = !_manager.isNavEnabled();
        const bool loadOff = !_manager.isLoadEnabled();
        drawBtn(g, zones.prev, _prevLabel, hov(Zone::Prev), navOff,  prs(Zone::Prev));
        drawBtn(g, zones.next, _nextLabel, hov(Zone::Next), navOff,  prs(Zone::Next));
        if (_showUndo)    drawBtn(g, zones.undo,   _undoLabel, hov(Zone::Undo),   !_manager.canUndo(), prs(Zone::Undo));
        if (_showRedo)    drawBtn(g, zones.redo,   _redoLabel, hov(Zone::Redo),   !_manager.canRedo(), prs(Zone::Redo));
        if (_showSaveLoad){ drawBtn(g, zones.save, _saveLabel, hov(Zone::Save),   false,   prs(Zone::Save));
                            drawBtn(g, zones.load, _loadLabel, hov(Zone::Load),   loadOff, prs(Zone::Load)); }
        if (_showDir)     drawBtn(g, zones.folder, _dirLabel,  hov(Zone::Folder), false, prs(Zone::Folder));
        if (_showScan)    drawBtn(g, zones.scan,   _scanLabel, hov(Zone::Scan),   false, prs(Zone::Scan));

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
        if (!navOff && _hoverZone == Zone::Name)
            g.FillRect(GetColor(kHL), zones.name, &mBlend);
        const float nameTxtOpacity = navOff ? 0.35f : 1.f;
        g.DrawText(mStyle.valueText
                       .WithAlign(EAlign::Center)
                       .WithVAlign(EVAlign::Middle)
                       .WithFGColor(mStyle.valueText.mFGColor.WithOpacity(nameTxtOpacity)),
                   label.c_str(), zones.name, &mBlend);
    }

    // ── Mouse ─────────────────────────────────────────────────────────────────

    void OnMouseDown(float x, float y, const IMouseMod& mod) override {
        const Zone z = hitZone(x, y);
        if (_pressedZone != z) { _pressedZone = z; SetDirty(false); }
        switch (z) {
            case Zone::Toggle:
                if (!_toggleEnabled) break;   // inert while disabled
                _collapsed = !_collapsed;
                if (_onToggle) _onToggle(_collapsed);
                SetDirty(false);
                break;
            case Zone::Prev:   _manager.prev();          SetDirty(false); break;
            case Zone::Next:   _manager.next();          SetDirty(false); break;
            case Zone::Undo:   _manager.undo();          SetDirty(false); break;
            case Zone::Redo:   _manager.redo();          SetDirty(false); break;
            case Zone::Save:   promptSave();                              break;
            case Zone::Load:   promptLoad();                              break;
            case Zone::Folder: _manager.openFolder();                     break;
            case Zone::Scan:   promptScanFolder();                        break;
            default: break;
        }
    }
	
	void OnMouseDblClick(float x, float y, const IMouseMod& mod) override {
		OnMouseDown (x, y, mod);
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

    enum class Zone { None, Toggle, Prev, Name, Next, Undo, Redo, Save, Load, Folder, Scan };

    struct Zones {
        IRECT toggle, prev, name, next, undo, redo, save, load, folder, scan;
        // Unset zones are default-constructed (zero-size IRECT), safe to ignore.
    };

    Zones computeZones() const {
        const float btnW            = mRECT.H() * 1.8f;                  // nav buttons (prev/next)
        const float wideW           = _actionBtnW > 0.f ? _actionBtnW    // absolute width (match a companion row)
                                                        : mRECT.H() * 2.8f * _actionWScale;  // all action buttons — uniform width
        const float toggleCollapsed = _collapsedToggleW > 0 ? _collapsedToggleW : mRECT.H() * 1.2f;
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
        if (_showDir)    z.folder = rslice(wideW);
        if (_showSaveLoad) { z.load = rslice(wideW); z.save = rslice(wideW); }
        if (_showRedo)   z.redo   = rslice(wideW);
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
        if (z.redo  .Contains(x, y)) return Zone::Redo;
        if (z.save  .Contains(x, y)) return Zone::Save;
        if (z.load  .Contains(x, y)) return Zone::Load;
        if (z.folder.Contains(x, y)) return Zone::Folder;
        if (z.scan  .Contains(x, y)) return Zone::Scan;
        if (z.name  .Contains(x, y)) return Zone::Name;
        return Zone::None;
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    void drawBtn(IGraphics& g, const IRECT& r, const GlyphLabel& label,
                 bool hover, bool disabled = false, bool pressed = false) const {
        if (!disabled) {
            if (pressed)    g.FillRect(GetColor(kPR), r, &mBlend);
            else if (hover) g.FillRect(GetColor(kHL), r, &mBlend);
        }
        const IColor frame = GetColor(kFR);
        if (frame.A > 0)
            g.DrawRect(disabled ? frame.WithOpacity(0.35f) : frame, r, &mBlend, 1.f);
        const float txtOpacity = disabled ? 0.35f : 1.f;
        const IText base = mStyle.valueText
                               .WithFGColor(mStyle.valueText.mFGColor.WithOpacity(txtOpacity));
        drawGlyphLabel(g, label, r, base, &mBlend, _runGap);
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
    bool           _toggleEnabled  = true;
    GlyphLabel     _collapsedLabel = ">>";
    GlyphLabel     _expandedLabel  = "<<";
    GlyphLabel     _prevLabel      = "<";
    GlyphLabel     _nextLabel      = ">";
    GlyphLabel     _undoLabel      = "undo";
    GlyphLabel     _redoLabel      = "redo";
    GlyphLabel     _saveLabel      = "save";
    GlyphLabel     _loadLabel      = "load";
    GlyphLabel     _dirLabel       = "dir";
    GlyphLabel     _scanLabel      = "scan";
    float          _runGap         = 0.f;
    float          _actionWScale   = 1.f;
    float          _actionBtnW     = 0.f;   // >0 → absolute action-button width (overrides the scale)
    bool           _showSaveLoad    = true;
    bool           _showUndo        = true;
    bool           _showRedo        = false;   // opt-in (off keeps existing strips unchanged)
    bool           _showDir         = true;
    bool           _showScan        = true;
    float          _collapsedToggleW = 0;   // 0 = auto
    float          _expandedToggleW  = 0;   // 0 = auto
};

} // namespace hvoya::ui
