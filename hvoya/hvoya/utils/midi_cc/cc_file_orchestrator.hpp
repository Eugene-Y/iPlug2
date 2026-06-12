#pragma once

/* cc_file_orchestrator.hpp
 *
 * Owns the CC map file I/O logic and creates the corresponding UI buttons.
 * Templated on Mediator so it works with any CCMediator<Plugin> without
 * knowing the plugin type directly.
 *
 * USAGE
 * -----
 *   // In plugin constructor (after preset manager is ready):
 *   _ccOrch = std::make_unique<CCFileOrchestrator<CCMediator<MyPlugin>>>(
 *       _midiCCMediator,
 *       PLUG_NAME, PLUG_VERSION_STR,
 *       [this]{ return _presetManager->browseDir(); },
 *       [this]{ for (int s = 0; s < kNumStages; ++s) updateCCCache(s); });
 *
 *   // In layout function:
 *   auto btns = _ccOrch->makeButtons(saveR, loadR, style);
 *   pG->AttachControl(btns.save, CCFileOrch::kTagSave, kGroupALL);
 *   pG->AttachControl(btns.load, CCFileOrch::kTagLoad, kGroupALL);
 *   pStrip->setOnToggle([btns](bool collapsed) {
 *       btns.save->Hide(collapsed);
 *       btns.load->Hide(collapsed);
 *   });
 */

#include <functional>
#include <string>

#include <IControls.h>

#include <hvoya/utils/hvoya_file.hpp>
#include <hvoya/utils/glyph_label.hpp>
#include "mapper_hvoya_adapter.hpp"




namespace hvoya::midi_cc {

using namespace iplug::igraphics;

template <typename Mediator>
class CCFileOrchestrator {
public:

    // ── Tags ─────────────────────────────────────────────────────────────────
    // Unique control tags — use these when calling AttachControl and
    // GetControlWithTag so the caller never hard-codes magic numbers.

    static constexpr int kTagSave = 1050;
    static constexpr int kTagLoad = 1051;


    // ── Button factory result ─────────────────────────────────────────────────

    struct Buttons {
        IControl* save;
        IControl* load;
    };


    // ── Construction ──────────────────────────────────────────────────────────

    CCFileOrchestrator (
        Mediator&                    mediator,
        std::string                  pluginName,
        std::string                  pluginVersion,
        std::function<std::string()> browseDir,
        std::function<void()>        onLoaded)
        : _mediator      (mediator)
        , _pluginName    (std::move (pluginName))
        , _pluginVersion (std::move (pluginVersion))
        , _browseDir     (std::move (browseDir))
        , _onLoaded      (std::move (onLoaded))
        , _style         (DEFAULT_STYLE)
    {}

    // ── Style ─────────────────────────────────────────────────────────────────
    // Call before makeButtons() to configure button appearance.

    CCFileOrchestrator& setStyle           (const IVStyle& s)  { _style = s; return *this; }
    CCFileOrchestrator& setBackgroundColor (const IColor& c)   { _style.colorSpec.mColors[kBG] = c; return *this; }
    CCFileOrchestrator& setMouseOverColor  (const IColor& c)   { _style.colorSpec.mColors[kHL] = c; return *this; }
    CCFileOrchestrator& setPressedColor    (const IColor& c)   { _style.colorSpec.mColors[kPR] = c; return *this; }
    CCFileOrchestrator& setFrameColor      (const IColor& c)   { _style.colorSpec.mColors[kFR] = c; return *this; }
    CCFileOrchestrator& setTextColor       (const IColor& c)   { _style.valueText.mFGColor = c;     return *this; }

    // Default filename pre-filled in the Save dialog (e.g. "ccmap.hvoya").
    CCFileOrchestrator& setDefaultFilename (const char* name)  { _defaultFilename = name;           return *this; }

    // Opt-in icon / mixed-font labels. When set, the button is drawn as a
    // GlyphButtonControl with that label instead of the default "save cc"/"load cc"
    // text button. `runGap` spaces the glyphs within a mixed label.
    CCFileOrchestrator& setSaveLabel   (hvoya::ui::GlyphLabel l) { _saveLabel = std::move (l); return *this; }
    CCFileOrchestrator& setLoadLabel   (hvoya::ui::GlyphLabel l) { _loadLabel = std::move (l); return *this; }
    CCFileOrchestrator& setLabelRunGap (float px)               { _labelRunGap = px;          return *this; }


    // ── Button factory ────────────────────────────────────────────────────────
    // Creates save/load buttons using the style set via setStyle() / color setters.
    // Both start hidden (matches collapsed preset strip default).
    // Caller attaches them with kTagSave / kTagLoad and manages their visibility.

    Buttons makeButtons (const IRECT& saveR, const IRECT& loadR) {
        auto saveClick = [this](IControl* pC) {
            pC->SetValue (0.0); pC->SetDirty (false); // clear pressed highlight
            WDL_String fn, path;
            fn.Set (_defaultFilename.c_str());
            path.Set (_browseDir().c_str());
            pC->GetUI()->PromptForFile (fn, path, EFileAction::Save, "hvoya",
                [this](const WDL_String& chosen, const WDL_String&) {
                    if (chosen.GetLength()) _save (chosen.Get());
                });
        };
        auto loadClick = [this](IControl* pC) {
            pC->SetValue (0.0); pC->SetDirty (false); // clear pressed highlight
            WDL_String fn, path;
            path.Set (_browseDir().c_str());
            pC->GetUI()->PromptForFile (fn, path, EFileAction::Open, "hvoya",
                [this](const WDL_String& chosen, const WDL_String&) {
                    if (chosen.GetLength()) _load (chosen.Get());
                });
        };

        IControl* save = makeButton (saveR, _saveLabel, "save cc", std::move (saveClick));
        IControl* load = makeButton (loadR, _loadLabel, "load cc", std::move (loadClick));

        save->Hide (true);
        load->Hide (true);

        return { save, load };
    }

    IControl* makeButton (const IRECT& r, const hvoya::ui::GlyphLabel& label,
                          const char* fallbackText, std::function<void(IControl*)> onClick) {
        if (label.empty())
            return new IVButtonControl (r, std::move (onClick), fallbackText, _style);
        auto* b = new hvoya::ui::GlyphButtonControl (r, label, std::move (onClick), _style);
        b->setRunGap (_labelRunGap);
        return b;
    }


private:

    // ── File I/O ──────────────────────────────────────────────────────────────

    void _save (const std::string& path) const {
        HvoyaFile f (_pluginName, _pluginVersion);
        MapperHvoyaAdapter::writeSection (_mediator.getCCtoParamMap(), f);
        f.toFile (path);
    }

    void _load (const std::string& path) {
        auto f = HvoyaFile::fromFile (path);
        if (!f) return;
        auto map = MapperHvoyaAdapter::readSection (*f);
        if (!map) return;
        _mediator.setCCtoParamMap (std::move (*map));
        if (_onLoaded) _onLoaded();
    }


    // ── State ─────────────────────────────────────────────────────────────────

    Mediator&                    _mediator;
    std::string                  _pluginName;
    std::string                  _pluginVersion;
    std::function<std::string()> _browseDir;
    std::function<void()>        _onLoaded;
    IVStyle                      _style;
    std::string                  _defaultFilename = "ccmap.hvoya";
    hvoya::ui::GlyphLabel        _saveLabel;   // empty → default "save cc" text button
    hvoya::ui::GlyphLabel        _loadLabel;
    float                        _labelRunGap = 0.f;
};

} // namespace hvoya::midi_cc
