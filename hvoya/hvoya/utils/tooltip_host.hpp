#pragma once

#include <IGraphicsStructs.h>

// ITooltipHost — a control that packs several buttons/sliders into ONE IControl (drawn as zones,
// not child controls) implements this so a hover-help layer can show a DIFFERENT tooltip per zone,
// anchored to that zone rather than to the whole control.
//
// The control maps its own current hover state (which it already tracks for its highlights) to a
// tooltip string + the sub-rect to anchor the bubble to. A hover-help consumer dynamic_casts the
// hovered IControl to ITooltipHost*; if it matches it uses these, else it falls back to the plain
// IControl::GetTooltip()/GetRECT(). Returning an empty string means "no tooltip for this spot".
//
// Only the tooltip TEXT is the host's concern — how/whether it is displayed is the consumer's.

namespace hvoya::ui {

struct ITooltipHost {
    virtual ~ITooltipHost() = default;

    // Tooltip for the currently hovered zone; empty string ("") when the hover isn't over a
    // tooltip-bearing zone (or nothing is hovered).
    virtual const char* hoveredTooltip() const = 0;

    // The zone to anchor the bubble to (only read when hoveredTooltip() is non-empty).
    virtual iplug::igraphics::IRECT hoveredTooltipRect() const = 0;
};

} // namespace hvoya::ui
