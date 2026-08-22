#pragma once

/* preset_browser_control.hpp — dropdown preset browser for iPlug2 UIs
 *
 * A modal overlay list that drops out of a preset strip's NAME label. Presets are
 * grouped into collapsible sections:
 *
 *   ▾ FACTORY            ← section header (click the row to collapse / expand)
 *       CLEAN MOOG
 *       SLAG        ●    ← current preset (highlighted)
 *   ▸ BASS               ← collapsed section (click to expand)
 *   ▾ my folder          ← a user (.fxp) folder
 *       thud
 *
 * Sections come from PresetManager::browseEntries(): factory first, then user.
 * A factory preset with no "group/name" slash lands in a single FACTORY section;
 * a user preset with no parent folder lands in USER. Slash-grouped factory
 * presets and named user folders each get their own section, in first-encounter
 * order. Sections default to EXPANDED; the current preset's section is force-open
 * and scrolled into view when the browser opens.
 *
 * GEOMETRY
 * --------
 *   The control's bounds cover the whole editor (so a click anywhere OUTSIDE the
 *   list dismisses it, and the list draws on top). The list rect drops from just
 *   under the anchor (the strip) and its BOTTOM margin equals the anchor's TOP
 *   margin, so the list is vertically balanced within the live editor bounds —
 *   which grow when a morph panel extends the window. Width defaults to the
 *   anchor's width and grows rightward to fit the widest row, capped at the
 *   editor's right edge.
 *
 * USAGE
 * -----
 *   auto* browser = new hvoya::ui::PresetBrowserControl(*_presetManager, style);
 *   browser->setAnchorRect(stripRect);               // where the list drops from
 *   pG->AttachControl(browser, kTagPresetBrowser);   // attach LAST → top of z-order
 *   browser->Hide(true);                             // starts closed
 *   strip->setOnNameClick([browser]{ browser->open(); });
 *
 * The control does NOT own the PresetManager — it holds a reference. Nothing here
 * is serialized (the open state, scroll, and per-section collapse are ephemeral UI).
 */

#include <IControl.h>
#include <IControls.h>
#include <IPlugConstants.h>   // kVK_ESCAPE
#include <algorithm>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include <hvoya/utils/glyph_label.hpp>
#include <hvoya/utils/preset_manager.hpp>

namespace hvoya::ui {

class PresetBrowserControl : public IControl, public IVectorBase {
public:
    PresetBrowserControl(PresetManager& manager, const IVStyle& style = DEFAULT_STYLE)
        : IControl   (IRECT())
        , IVectorBase(style)
        , _manager   (manager)
    {
        AttachIControl(this, "");
        _headerText = mStyle.valueText.WithAlign(EAlign::Near).WithVAlign(EVAlign::Middle);
        _itemText   = mStyle.valueText.WithAlign(EAlign::Near).WithVAlign(EVAlign::Middle);
    }

    // ── Declarative appearance (one named setter per visual property) ────────────

    PresetBrowserControl& setAnchorRect      (const IRECT& r)  { _anchor = r; return *this; }

    PresetBrowserControl& setPanelColor      (const IColor& c) { _panelBg     = c; return *this; }
    PresetBrowserControl& setPanelFrameColor (const IColor& c) { _panelFrame  = c; return *this; }
    PresetBrowserControl& setScrimColor      (const IColor& c) { _scrim       = c; return *this; }  // dims the area outside the list (default none)
    PresetBrowserControl& setHeaderBgColor   (const IColor& c) { _headerBg    = c; return *this; }
    PresetBrowserControl& setHeaderTextColor (const IColor& c) { _headerText.mFGColor = c; return *this; }
    PresetBrowserControl& setItemTextColor   (const IColor& c) { _itemText.mFGColor   = c; return *this; }
    PresetBrowserControl& setItemHoverColor  (const IColor& c) { _itemHoverBg = c; return *this; }
    PresetBrowserControl& setCurrentBgColor  (const IColor& c) { _currentBg   = c; return *this; }
    PresetBrowserControl& setCurrentTextColor(const IColor& c) { _currentText = c; return *this; }
    PresetBrowserControl& setScrollTrackColor(const IColor& c) { _scrollTrack = c; return *this; }
    PresetBrowserControl& setScrollThumbColor(const IColor& c) { _scrollThumb = c; return *this; }

    PresetBrowserControl& setHeaderTextStyle (const IText& t)  { _headerText = t.WithAlign(EAlign::Near).WithVAlign(EVAlign::Middle); return *this; }
    PresetBrowserControl& setItemTextStyle   (const IText& t)  { _itemText   = t.WithAlign(EAlign::Near).WithVAlign(EVAlign::Middle); return *this; }

    // Chevron glyphs for expanded / collapsed section headers (pass icon-font GlyphLabels).
    PresetBrowserControl& setChevronGlyphs   (GlyphLabel open, GlyphLabel closed) {
        _chevronOpen = std::move(open); _chevronClosed = std::move(closed); return *this;
    }
    // Marker drawn on the current preset's row (right-aligned).
    PresetBrowserControl& setCurrentGlyph    (GlyphLabel g) { _currentGlyph = std::move(g); return *this; }
    // Size of the header collapse/expand triangle relative to the header text (1.0 = same size).
    PresetBrowserControl& setChevronScale    (float s)      { _chevronScale = s; return *this; }
    // Text of the bottom overflow indicator row (default "...").
    PresetBrowserControl& setEllipsisText    (std::string s){ _ellipsisText = std::move(s); return *this; }

    PresetBrowserControl& setRowHeight       (float px) { _rowH       = px; return *this; }
    PresetBrowserControl& setHeaderHeight    (float px) { _headerH    = px; return *this; }
    PresetBrowserControl& setItemIndent      (float px) { _itemIndent = px; return *this; }
    PresetBrowserControl& setChevronRunGap   (float px) { _chevronGap = px; return *this; }

    // ── Open / close ────────────────────────────────────────────────────────────

    bool isOpen() const { return _open; }

    void open() {
        syncBounds();             // cover the whole editor (click-catcher + top-of-z draw area)
        _manager.refreshUserPresets();   // reconcile with disk (added / renamed / deleted files)
        buildTree();
        revealCurrent();          // force-open the current preset's section, scroll it into view
        _contentWValid = false;   // remeasure against the (possibly new) entry set
        _open = true;
        Hide(false);
        SetDirty(false);
    }

    void close() {
        if (!_open) return;
        _open = false;
        _hoverRow = -1;
        _draggingThumb = false;
        Hide(true);
        SetDirty(false);
    }

    // ── IControl overrides ──────────────────────────────────────────────────────

    // Full-editor bounds: the mask catches outside-clicks and the draw area spans the window.
    // Assign the rects directly — SetTargetAndDrawRECTs() calls OnResize() and would recurse.
    void syncBounds() {
        if (auto* g = GetUI()) { mRECT = mTargetRECT = g->GetBounds(); }
    }
    void OnResize() override { syncBounds(); }

    void Draw(IGraphics& g) override {
        if (!_open) return;
        if (!_contentWValid) measureContent(g);

        const IRECT list  = listRect();
        if (_scrim.A > 0) g.FillRect(_scrim, mRECT, &mBlend);

        g.FillRect(_panelBg, list, &mBlend);

        const IRECT inner = list.GetPadded(-_pad);
        const float viewH = inner.H();
        clampScroll(viewH);

        g.PathClipRegion(inner);                       // rows never spill past the panel edges
        const float contentTop = inner.T - _scroll;
        for (int i = 0; i < static_cast<int>(_rows.size()); ++i) {
            const Row&  row = _rows[static_cast<size_t>(i)];
            const float top = contentTop + row.y;
            if (top + row.h < inner.T || top > inner.B) continue;   // off-screen
            const IRECT rr(inner.L, top, inner.R, top + row.h);
            row.header ? drawHeader(g, rr, row.node, i, row.depth)
                       : drawItem  (g, rr, row.entryIdx, i, row.depth);
        }
        g.PathClipRegion();                            // reset clip

        drawOverflowHint(g, inner, viewH);             // "…" bottom row when more is hidden below
        drawScrollbar(g, inner, viewH);
        if (_panelFrame.A > 0) g.DrawRect(_panelFrame, list, &mBlend, 1.f);
    }

    void OnMouseDown(float x, float y, const IMouseMod& mod) override {
        if (!_open) return;
        const IRECT list = listRect();
        if (!list.Contains(x, y)) { close(); return; }   // click outside the list → dismiss

        const IRECT inner = list.GetPadded(-_pad);
        if (thumbRect(inner).Contains(x, y)) {           // grab the scroll thumb
            _draggingThumb = true;
            _dragThumbGrabDY = y - thumbRect(inner).T;
            return;
        }

        const int i = rowAt(x, y);
        if (i < 0) return;
        const Row& row = _rows[static_cast<size_t>(i)];
        if (row.header) {
            _nodes[static_cast<size_t>(row.node)].collapsed ^= true;
            rebuildRows();
            clampScroll(listRect().GetPadded(-_pad).H());
            SetDirty(false);
        } else {
            _manager.goTo(_entries[static_cast<size_t>(row.entryIdx)].navIdx);
            close();
        }
    }

    void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override {
        if (!_draggingThumb) return;
        const IRECT inner = listRect().GetPadded(-_pad);
        const float viewH = inner.H();
        if (_contentH <= viewH) return;
        const float trackH   = inner.H();
        const float thumbH   = std::max(_minThumbH, viewH * viewH / _contentH);
        const float thumbTop = std::clamp(y - _dragThumbGrabDY, inner.T, inner.B - thumbH);
        const float frac     = (thumbTop - inner.T) / std::max(1.f, trackH - thumbH);
        _scroll = frac * (_contentH - viewH);
        SetDirty(false);
    }

    void OnMouseUp(float, float, const IMouseMod&) override {
        if (_draggingThumb) { _draggingThumb = false; SetDirty(false); }
    }

    void OnMouseWheel(float x, float y, const IMouseMod&, float d) override {
        if (!_open) return;
        _scroll -= d * _rowH;   // one row per wheel notch → no fractional row drift while scrolling
        clampScroll(listRect().GetPadded(-_pad).H());
        SetDirty(false);
    }

    void OnMouseOver(float x, float y, const IMouseMod&) override {
        const int i = listRect().Contains(x, y) ? rowAt(x, y) : -1;
        if (i != _hoverRow) { _hoverRow = i; SetDirty(false); }
    }

    void OnMouseOut() override {
        if (_hoverRow != -1) { _hoverRow = -1; SetDirty(false); }
    }

    bool OnKeyDown(float, float, const IKeyPress& key) override {
        if (_open && key.VK == kVK_ESCAPE) { close(); return true; }
        return false;
    }

    // Note: no IsDirty() override — the default mDirty flag is right here. Every state change
    // (hover / scroll / collapse / open / close) calls SetDirty(false), so redraws happen only
    // when something moved; a full-editor overlay must NOT report perpetually dirty (that would
    // repaint the whole UI every frame). Hide()+SetDirty on close repaints the revealed region.

private:
    // ── Model ───────────────────────────────────────────────────────────────────

    // A group tree: two roots (FACTORY / USER), each holding presets directly plus nested child
    // folders (factory slash-groups, user preset subfolders). Depth drives the row's indent.
    struct Node {
        std::string      label;
        bool             collapsed = false;
        std::vector<int> entryIdx;   // presets directly in this node (indices into _entries)
        std::vector<int> children;   // child node indices (into _nodes)
    };
    struct Row {
        bool  header;
        int   node;         // node index (valid always; the parent for an item row)
        int   entryIdx;     // valid when !header (index into _entries)
        int   depth;        // indent level: 0 = a top-level (FACTORY/USER) header
        float y;            // top, in content space
        float h;
    };

    void buildTree() {
        _entries = _manager.browseEntries();
        _nodes.clear();
        _roots.clear();

        // Find-or-create a child node by label under `parent` (-1 = the root list). Indices only —
        // a push_back may reallocate _nodes, so the sibling vector is re-fetched after it.
        auto childByLabel = [&](int parent, const std::string& label) -> int {
            auto& sibs = (parent < 0) ? _roots : _nodes[static_cast<size_t>(parent)].children;
            for (int idx : sibs)
                if (_nodes[static_cast<size_t>(idx)].label == label) return idx;
            const int ni = static_cast<int>(_nodes.size());
            _nodes.push_back(Node{ label, false, {}, {} });
            auto& sibs2 = (parent < 0) ? _roots : _nodes[static_cast<size_t>(parent)].children;
            sibs2.push_back(ni);
            return ni;
        };
        auto splitGroup = [](const std::string& g) {
            std::vector<std::string> segs;
            for (size_t start = 0; start <= g.size(); ) {
                const size_t slash = g.find('/', start);
                const size_t end   = (slash == std::string::npos) ? g.size() : slash;
                if (end > start) segs.push_back(g.substr(start, end - start));
                if (slash == std::string::npos) break;
                start = slash + 1;
            }
            return segs;
        };

        for (int i = 0; i < static_cast<int>(_entries.size()); ++i) {
            const auto& e = _entries[static_cast<size_t>(i)];
            int cur = childByLabel(-1, e.factory ? "FACTORY" : "USER");
            for (const auto& seg : splitGroup(e.group)) cur = childByLabel(cur, seg);
            _nodes[static_cast<size_t>(cur)].entryIdx.push_back(i);
        }
        rebuildRows();
    }

    void emitNode(int nodeIdx, int depth, float& y) {
        const Node& n = _nodes[static_cast<size_t>(nodeIdx)];
        _rows.push_back({ true, nodeIdx, -1, depth, y, _headerH });
        y += _headerH;
        if (n.collapsed) return;
        for (int e : n.entryIdx) {
            _rows.push_back({ false, nodeIdx, e, depth + 1, y, _rowH });
            y += _rowH;
        }
        for (int c : n.children) emitNode(c, depth + 1, y);
    }

    void rebuildRows() {
        _rows.clear();
        float y = 0.f;
        for (int root : _roots) emitNode(root, 0, y);
        _contentH = y;
    }

    // Force-open every ancestor of the current preset's node and scroll its row into view.
    void revealCurrent() {
        _pendingScrollToY = -1.f;
        const int cur = _manager.currentIdx();
        if (cur < 0) { _scroll = 0.f; return; }
        bool found = false;
        for (int root : _roots)
            if (expandToEntry(root, cur)) { found = true; break; }
        if (!found) { _scroll = 0.f; return; }
        rebuildRows();
        for (const Row& r : _rows)
            if (!r.header && _entries[static_cast<size_t>(r.entryIdx)].navIdx == cur) {
                _pendingScrollToY = r.y;   // resolved against the view height on first Draw
                break;
            }
    }

    // Un-collapse nodeIdx if it (transitively) holds the entry with navIdx==cur; returns true if found.
    bool expandToEntry(int nodeIdx, int cur) {
        Node& n = _nodes[static_cast<size_t>(nodeIdx)];
        bool here = false;
        for (int e : n.entryIdx)
            if (_entries[static_cast<size_t>(e)].navIdx == cur) { here = true; break; }
        if (!here)
            for (int c : n.children)
                if (expandToEntry(c, cur)) { here = true; break; }
        if (here) n.collapsed = false;
        return here;
    }

    // ── Geometry ─────────────────────────────────────────────────────────────────

    IRECT listRect() const {
        const IRECT ed = GetUI() ? GetUI()->GetBounds() : mRECT;
        // The list drops from just under the anchor; at its MAX it is balanced within the live
        // editor bounds — bottom margin equals the top margin (editor-top → list-top).
        const float topMargin = _anchor.B - ed.T;
        const float left      = _anchor.L;
        const float right     = std::min(ed.R, left + std::max(_anchor.W(), _contentW));
        const float maxBottom = ed.B - topMargin;

        // Shrink from the bottom when the presets need less than the full height; and if everything
        // fits within one row of the max, grow up to that one row to show it ALL rather than hiding
        // just the last preset behind an ellipsis (see drawOverflowHint). Otherwise cap at the max
        // and scroll (with the ellipsis hint).
        const float maxInnerH     = (maxBottom - _pad) - (_anchor.B + _pad);
        const float desiredInnerH = (_contentH <= maxInnerH + _rowH) ? _contentH : maxInnerH;
        const float bottom        = _anchor.B + _pad + desiredInnerH + _pad;
        return IRECT(left, _anchor.B, right, bottom);
    }

    IRECT thumbRect(const IRECT& inner) const {
        const float viewH = inner.H();
        if (_contentH <= viewH) return IRECT();
        const float thumbH = std::max(_minThumbH, viewH * viewH / _contentH);
        const float frac   = _contentH > viewH ? _scroll / (_contentH - viewH) : 0.f;
        const float top    = inner.T + frac * (inner.H() - thumbH);
        return IRECT(inner.R - _scrollbarW, top, inner.R, top + thumbH);
    }

    void clampScroll(float viewH) {
        // Resolve a pending "scroll the current row into view" now that we know viewH.
        if (_pendingScrollToY >= 0.f) {
            _scroll = _pendingScrollToY - viewH * 0.5f + _rowH * 0.5f;
            _pendingScrollToY = -1.f;
        }
        _scroll = std::clamp(_scroll, 0.f, std::max(0.f, _contentH - viewH));
    }

    int rowAt(float x, float y) const {
        const IRECT inner = listRect().GetPadded(-_pad);
        if (!inner.Contains(x, y)) return -1;
        const float cy = (y - inner.T) + _scroll;
        for (int i = 0; i < static_cast<int>(_rows.size()); ++i) {
            const Row& r = _rows[static_cast<size_t>(i)];
            if (cy >= r.y && cy < r.y + r.h) return i;
        }
        return -1;
    }

    // ── Drawing helpers ───────────────────────────────────────────────────────────

    // Each nesting level shifts content right by one row height (row.depth * _rowH).
    void drawHeader(IGraphics& g, const IRECT& r, int nodeIdx, int rowIdx, int depth) {
        const Node& n = _nodes[static_cast<size_t>(nodeIdx)];
        if (_headerBg.A > 0) g.FillRect(_headerBg, r, &mBlend);
        if (rowIdx == _hoverRow && _itemHoverBg.A > 0) g.FillRect(_itemHoverBg, r, &mBlend);

        const IRECT ind   = r.GetReducedFromLeft(depth * _rowH);
        const IRECT chevR = ind.GetFromLeft(_itemIndent);
        IText chevTxt = _headerText.WithAlign(EAlign::Center);
        chevTxt.mSize *= _chevronScale;   // collapse/expand triangle a touch larger than the label
        drawGlyphLabel(g, n.collapsed ? _chevronClosed : _chevronOpen, chevR, chevTxt, &mBlend, _chevronGap);
        const IRECT txtR = ind.GetReducedFromLeft(_itemIndent).GetReducedFromLeft(_textPad);
        g.DrawText(_headerText, n.label.c_str(), txtR, &mBlend);
    }

    void drawItem(IGraphics& g, const IRECT& r, int entryIdx, int rowIdx, int depth) {
        const auto& e       = _entries[static_cast<size_t>(entryIdx)];
        // By index alone: editing the preset must not un-highlight its row — the strip's "*" says that.
        const bool  current = e.navIdx == _manager.currentIdx();

        if (current && _currentBg.A > 0)             g.FillRect(_currentBg,   r, &mBlend);
        else if (rowIdx == _hoverRow && _itemHoverBg.A > 0) g.FillRect(_itemHoverBg, r, &mBlend);

        IText txt = _itemText;
        if (current) txt = txt.WithFGColor(_currentText);
        const IRECT txtR = r.GetReducedFromLeft(depth * _rowH + _textPad);
        g.DrawText(txt, e.name.c_str(), txtR, &mBlend);

        if (current) {
            const IRECT markR = r.GetFromRight(_rowH).GetReducedFromRight(_scrollbarW);
            drawGlyphLabel(g, _currentGlyph, markR, txt.WithAlign(EAlign::Center), &mBlend);
        }
    }

    // A "…" overflow row painted over the bottom band whenever content is hidden below the fold
    // (and we're not already scrolled to the very bottom). It only appears when the list is capped
    // at its max height — a shorter, fully-visible list never triggers it.
    void drawOverflowHint(IGraphics& g, const IRECT& inner, float viewH) {
        if (_contentH <= viewH) return;
        if (_scroll >= (_contentH - viewH) - 0.5f) return;   // at the bottom → last rows fully shown
        const IRECT band(inner.L, inner.B - _rowH, inner.R, inner.B);
        g.FillRect(_panelBg, band, &mBlend);                 // cover the partial row peeking under it
        g.DrawText(_itemText.WithAlign(EAlign::Center), _ellipsisText.c_str(), band, &mBlend);
    }

    void drawScrollbar(IGraphics& g, const IRECT& inner, float viewH) {
        if (_contentH <= viewH) return;
        const IRECT track(inner.R - _scrollbarW, inner.T, inner.R, inner.B);
        if (_scrollTrack.A > 0) g.FillRect(_scrollTrack, track, &mBlend);
        g.FillRect(_scrollThumb, thumbRect(inner), &mBlend);
    }

    void measureContent(IGraphics& g) {
        float maxW = _anchor.W();
        IRECT b;
        for (const Row& r : _rows) {
            const float indent = r.depth * _rowH;
            const float w = r.header
                ? indent + _itemIndent + g.MeasureText(_headerText, _nodes[static_cast<size_t>(r.node)].label.c_str(), b) + _textPad
                : indent + _textPad + g.MeasureText(_itemText, _entries[static_cast<size_t>(r.entryIdx)].name.c_str(), b) + _rowH;  // + marker gutter
            maxW = std::max(maxW, w);
        }
        _contentW      = maxW + _scrollbarW + _textPad;
        _contentWValid = true;
    }

    // ── State ─────────────────────────────────────────────────────────────────────

    PresetManager& _manager;
    IRECT          _anchor;              // the strip the list drops from

    std::vector<PresetManager::BrowseEntry> _entries;
    std::vector<Node>                       _nodes;
    std::vector<int>                        _roots;
    std::vector<Row>                        _rows;

    bool  _open           = false;
    float _scroll         = 0.f;
    float _contentH       = 0.f;
    float _contentW       = 0.f;
    bool  _contentWValid  = false;
    int   _hoverRow       = -1;
    bool  _draggingThumb  = false;
    float _dragThumbGrabDY = 0.f;
    float _pendingScrollToY = -1.f;

    // Tunables (declarative setters above).
    float _rowH        = 20.f;
    float _headerH     = 22.f;
    float _itemIndent  = 18.f;
    float _textPad     = 4.f;
    float _pad         = 3.f;    // inset of the scrolling content from the panel frame
    float _scrollbarW  = 6.f;
    float _minThumbH   = 24.f;
    float _chevronGap  = 0.f;
    float _chevronScale = 1.25f; // collapse/expand triangle size relative to the header label

    IColor _panelBg     { 255, 30, 30, 30 };
    IColor _panelFrame  { 255, 90, 90, 90 };
    IColor _scrim       = COLOR_TRANSPARENT;
    IColor _headerBg    { 255, 50, 50, 50 };
    IColor _itemHoverBg { 255, 70, 70, 70 };
    IColor _currentBg   { 255, 95, 55, 200 };
    IColor _currentText = COLOR_WHITE;
    IColor _scrollTrack = COLOR_TRANSPARENT;
    IColor _scrollThumb { 255, 120, 120, 120 };

    IText       _headerText;
    IText       _itemText;
    GlyphLabel  _chevronOpen   = "v";
    GlyphLabel  _chevronClosed = ">";
    GlyphLabel  _currentGlyph  = "*";
    std::string _ellipsisText  = "...";
};

} // namespace hvoya::ui
