#pragma once

/* preset_manager.hpp — user preset management for iPlug2 plugins
 *
 * Maintains a combined list of factory presets (from IPluginBase::MakePreset)
 * and user presets (*.fxp files in a directory, optionally grouped into
 * subdirectories).
 *
 * USAGE
 * -----
 *   // Construct once, e.g. at end of plugin constructor
 *   _presetManager = std::make_unique<hvoya::PresetManager>(
 *       this, "Melter", "/path/to/presets/dir");
 *
 *   // Navigation (UI thread)
 *   _presetManager->next();
 *   _presetManager->prev();
 *   _presetManager->goTo(idx);
 *   _presetManager->undo();
 *
 *   // File I/O (paths come from dialogs in PresetStripControl)
 *   _presetManager->saveToFile(path);
 *   _presetManager->loadFromFile(path);
 *   _presetManager->addFolder(folderPath); // scan a directory, append valid presets
 *   _presetManager->openFolder();          // shell-opens the last-used directory
 *
 * PRESET ORDER
 * ------------
 *   Factory presets always come first (in MakePreset() call order).
 *   User presets follow; the initial list is sorted alphabetically.
 *   Any new preset added during the session is appended at the end —
 *   existing positions never shift.
 *
 * DELETION
 * --------
 *   Deleted files are not tracked proactively. If navigation lands on a
 *   missing file, it is removed from the in-memory list and navigation stops
 *   at the nearest valid position.
 *
 * UNDO
 * ----
 *   Every navigation or load call pushes the current serialized state AND
 *   preset index onto a ring buffer (depth = constructor's undoDepth, default
 *   kDefaultUndoDepth). undo() restores both.
 *   Individual parameter tweaks are NOT tracked — that is the host's job.
 *
 *   Only deliberate on-screen edits (EParamSource::kUI) mark a preset dirty.
 *   Host automation (kHost) and MIDI CC (kDelegate) are live, host/performer-
 *   owned control, not unsaved edits: tracking them as dirty would trap undo in
 *   a perpetual baseline-revert whenever a continuous LFO or recorded CC lane is
 *   active. See onParamChanged().
 */

#include <IPlugPluginBase.h>
#include <IPlugPaths.h>
#include <hvoya/utils/log/logger.hpp>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace hvoya {

class PresetManager {
public:
    struct UserPreset {
        std::string path;   // full filesystem path
        std::string name;   // filename without extension
        std::string group;  // parent subdirectory name, "" if root-level
    };

    static constexpr int kDefaultUndoDepth = 100;

    // CC-map callbacks — optional. When set, the manager snapshots the map
    // before every state load and restores it afterwards, so MIDI CC
    // assignments are never clobbered by preset navigation.
    using CCSerializeFn   = std::function<void(iplug::IByteChunk&)>;
    using CCUnserializeFn = std::function<void(const iplug::IByteChunk&)>;

    void setCCMapCallbacks(CCSerializeFn serializeFn, CCUnserializeFn unserializeFn) {
        _ccSerialize   = std::move(serializeFn);
        _ccUnserialize = std::move(unserializeFn);
    }

    // Workspace callbacks — optional. "Workspace" is state that should SURVIVE a
    // preset swap (not be overwritten by the loaded preset): e.g. a morph rig, custom
    // UI layout. Like the CC map, the manager snapshots it before every state load
    // and restores it afterwards. A host PROJECT load goes through the plugin
    // directly (not the manager), so it restores the workspace from the chunk as
    // normal — only preset-strip swaps / undo preserve it.
    using WorkspaceSerializeFn   = std::function<void(iplug::IByteChunk&)>;
    using WorkspaceUnserializeFn = std::function<void(const iplug::IByteChunk&)>;

    void setWorkspaceCallbacks(WorkspaceSerializeFn serializeFn, WorkspaceUnserializeFn unserializeFn) {
        _wsSerialize   = std::move(serializeFn);
        _wsUnserialize = std::move(unserializeFn);
    }

    using NavGateFn = std::function<bool()>;
    void setNavigationGate(NavGateFn fn) { _navGate = std::move(fn); }

    // Optional predicates for the strip UI: when set and returns true, the corresponding
    // button is shown disabled. Separate from the redirect (which has side effects on
    // Handled) — these are pure queries called every frame by PresetStripControl.
    using BlockedFn = std::function<bool()>;
    void setNavBlockedWhen  (BlockedFn fn) { _navBlocked  = std::move(fn); }
    void setLoadBlockedWhen (BlockedFn fn) { _loadBlocked = std::move(fn); }

    bool isNavEnabled()  const { return !_navBlocked  || !_navBlocked();  }
    bool isLoadEnabled() const { return !_loadBlocked || !_loadBlocked(); }

    // ── Context-sensitive preset application (redirect) ─────────────────────────
    //
    // Normally choosing a preset (strip nav OR an explicit file load) swaps the whole patch.
    // A host may want it to mean something else IN A GIVEN CONTEXT — e.g. while editing a morph
    // point, load only its SOUND into that point; or, while morphing, forbid a whole-patch swap
    // altogether. The manager doesn't know the host's context, so it offers each preset to an
    // optional redirect and obeys its verdict.

    // Where a preset comes from (an explicit Kind, not an int sentinel → clear + extensible).
    struct PresetSource {
        enum class Kind { BrowseList, File };
        Kind        kind      = Kind::BrowseList;
        int         listIndex = -1;     // valid when kind == BrowseList (factory or user)
        std::string filePath;           // valid when kind == File (an .fxp opened outside the list)
        bool fromList() const { return kind == Kind::BrowseList; }
        static PresetSource list(int idx)        { return { Kind::BrowseList, idx, {} }; }
        static PresetSource file(std::string p)  { return { Kind::File, -1, std::move(p) }; }
    };

    // The redirect's verdict for a source (consulted BEFORE normal application / the nav gate):
    //   PassThrough — not claimed; apply normally (swap the whole patch).
    //   Handled     — the host applied it to its own destination (e.g. the selected morph point);
    //                 the manager does nothing else (list nav still moves the displayed index so
    //                 the strip name follows; a claimed file does NOT enter the list).
    //   Block       — do nothing at all (e.g. forbid a whole-patch swap while morphing) — a no-op.
    enum class PresetApply { PassThrough, Handled, Block };
    using PresetRedirectFn = std::function<PresetApply(const PresetSource&)>;
    void setPresetRedirect(PresetRedirectFn fn) { _redirect = std::move(fn); }

    // Resolve a browse-list index to a peekable origin (so a redirect can READ a preset without
    // applying it): factoryPluginIdx() >= 0 for a factory preset (index into the plugin's own
    // preset list), else userPresetPath() gives the .fxp path.
    int factoryPluginIdx(int navIdx) const {
        return isFactory(navIdx) ? _factoryIdxMap[static_cast<size_t>(navIdx)] : -1;
    }
    std::string userPresetPath(int navIdx) const {
        const int ui = navIdx - factoryCount();
        return (ui >= 0 && ui < userCount()) ? _userPresets[static_cast<size_t>(ui)].path
                                             : std::string{};
    }

    // ── Construction ──────────────────────────────────────────────────────────

    PresetManager(iplug::IPluginBase* plugin,
                  std::string_view    pluginName,
                  std::string_view    presetDir,
                  int                 undoDepth = kDefaultUndoDepth)
        : _plugin    (plugin)
        , _pluginName(pluginName)
        , _presetDir (presetDir)
        , _undoDepth (std::max(1, undoDepth))
    {
        // Build index map for factory presets, skipping uninitialized slots
        // (iPlug2 names uninitialized slots "empty" — UNUSED_PRESET_NAME).
        for (int i = 0; i < _plugin->NPresets(); ++i) {
            const char* n = _plugin->GetPresetName(i);
            if (n && std::string_view(n) != "empty")
                _factoryIdxMap.push_back(i);
        }

        // Initial scan — sorted alphabetically so the starting order is clean.
        // Subsequent calls to rescanUserPresets() only append new entries.
        scanFromDisk(_presetDir);
        std::sort(_userPresets.begin(), _userPresets.end(),
            [](const UserPreset& a, const UserPreset& b) {
                if (a.group != b.group) {
                    if (a.group.empty()) return true;
                    if (b.group.empty()) return false;
                    return a.group < b.group;
                }
                return a.name < b.name;
            });

        captureBaseline();
    }

    // ── Navigation ────────────────────────────────────────────────────────────

    void next() {
        if (totalCount() == 0) return;
        navigateTo(_currentIdx < 0 ? 0 : (_currentIdx + 1) % totalCount());
    }

    void prev() {
        if (totalCount() == 0) return;
        navigateTo(_currentIdx < 0 ? totalCount() - 1 : (_currentIdx - 1 + totalCount()) % totalCount());
    }

    void goTo(int idx) {
        if (idx < 0 || idx >= totalCount()) return;
        navigateTo(idx);
    }

    void undo() {
        InternalRestoreScope guard(_restoringInternally);
        auto cc = snapshotCCMap();
        auto ws = snapshotWorkspace();

        if (_modified.load(std::memory_order_relaxed)) {
            // First undo while dirty: revert to the baseline of the current
            // preset without consuming the navigation stack. The current (dirty)
            // state goes onto the redo stack so redo can bring it back.
            pushBounded(_redoStack, captureEntry());
            int pos = 0;
            _plugin->UnserializeState(_baselineChunk, pos);
            _plugin->OnRestoreState();
            restoreCCMap(cc);
            restoreWorkspace(ws);
            captureBaseline();   // _modified → false, _baselineChunk stays same
            return;
        }

        if (_undoStack.empty()) return;
        pushBounded(_redoStack, captureEntry());   // current state → redo
        UndoEntry entry = _undoStack.front();
        _undoStack.pop_front();
        restoreEntry(entry, cc, ws);
    }

    // Step forward again through states undone away from. The current state is pushed
    // back onto the undo stack (so undo returns here) WITHOUT clearing redo.
    void redo() {
        if (_redoStack.empty()) return;
        InternalRestoreScope guard(_restoringInternally);
        auto cc = snapshotCCMap();
        auto ws = snapshotWorkspace();
        pushBounded(_undoStack, captureEntry());
        UndoEntry entry = _redoStack.front();
        _redoStack.pop_front();
        restoreEntry(entry, cc, ws);
    }

    // Wraps a wholesale programmatic patch change (randomize / mutate) as one
    // undo step: the pre-change state is pushed onto the stack (so undo walks
    // back through every such change, like preset navigation), then the new
    // state becomes the clean baseline. applyChange performs the parameter writes.
    template <class ApplyFn>
    void changePatch(ApplyFn&& applyChange) {
        pushUndo();
        applyChange();
        _currentIdx = -1;        // result is a custom patch, not a stored preset
        captureBaseline();
    }

    // Make the current live state the clean baseline WITHOUT recording an undo step (and without
    // clearing redo). For a host action that changes state but must not be undoable on its own —
    // e.g. selecting a morph point to edit (navigation, not a patch edit). Subsequent dirty edits
    // revert to here; the previous undo entry (the last real change) stays poppable.
    void rebaseline() { captureBaseline(); }

    // ── Sub-session squash / commit ─────────────────────────────────────────────
    //
    // A "sub-session" (e.g. a morph session) is driven directly on the live params
    // rather than through changePatch, and may push several intermediate undo steps
    // (point selects). Mark the position when it begins, then on exit squash those
    // steps and optionally record ONE outer step — so the host-level history sees
    // the whole session as a single transaction.

    using UndoMark = std::size_t;

    // Current undo-stack depth — pass to commitFromState / squashUndoTo on exit.
    UndoMark undoMark() const { return _undoStack.size(); }

    // Drop undo entries pushed since `mark`. Baseline / dirty flag are untouched —
    // for an exit that must not rebaseline (a no-op session). If the depth ring
    // dropped pre-session entries during the session, this conservatively keeps
    // whatever remains (never over-truncates older history).
    void squashUndoTo(UndoMark mark) {
        while (_undoStack.size() > mark) _undoStack.pop_front();
    }

    // Close a sub-session as one atomic step: squash its intermediate entries, push
    // `preState` (the state from before the session) as the single undo step if
    // non-empty, and make the current live state the new clean baseline. Empty
    // preState → just squash + rebaseline (no back-step).
    void commitFromState(const iplug::IByteChunk& preState, UndoMark mark) {
        squashUndoTo(mark);
        if (preState.Size() > 0) {
            if (static_cast<int>(_undoStack.size()) >= _undoDepth)
                _undoStack.pop_back();
            _undoStack.push_front({ preState, iplug::IByteChunk{}, _currentIdx, false, false });
        }
        _redoStack.clear();      // a committed forward result invalidates redo
        _currentIdx = -1;        // a baked / programmatic result is a custom patch
        _divergedFromIndex = false;
        captureBaseline();
    }

    // Call from the plugin's OnRestoreState(). On a genuine EXTERNAL restore (host
    // project load, host-level undo) the manager's baseline must follow the new
    // state and stale undo history is dropped. The manager's OWN restores
    // (undo()/navigation, which also reach OnRestoreState) must NOT do this — the
    // reentrancy guard suppresses them.
    void onHostStateRestored() {
        if (_restoringInternally) return;
        _undoStack.clear();
        _redoStack.clear();
        captureBaseline();
    }

    // ── File I/O ──────────────────────────────────────────────────────────────

    void saveToFile(const std::string& path) {
        if (!_plugin->SavePresetAsFXP(path.c_str())) {
            LOGE << "[PresetManager] save FAILED: " << path;
            return;
        }
        LOGD << "[PresetManager] saved: " << path;
        _lastUsedDir = std::filesystem::path(path).parent_path().string();
        _currentIdx  = factoryCount() + ensureInList(path);
        _divergedFromIndex = false;
        captureBaseline();
    }

    bool loadFromFile(const std::string& path) {
        // The redirect may claim the file (e.g. load only its sound into the selected morph
        // point → Handled; a claimed file is NOT entered into the list) or forbid the whole-patch
        // load in this context (e.g. while morphing → Block, a no-op).
        const PresetApply v = _redirect ? _redirect(PresetSource::file(path)) : PresetApply::PassThrough;
        if (v == PresetApply::Handled) return true;
        if (v == PresetApply::Block)   return false;
        InternalRestoreScope guard(_restoringInternally);
        pushUndo();
        auto cc = snapshotCCMap();
        // NB: workspace is NOT snapshotted here — an explicit file load opens the
        // COMPLETE patch (sound + workspace), unlike strip navigation (next/prev/goTo),
        // which preserves the current workspace. (CC stays snapshotted for now.)
        if (!_plugin->LoadStateFromFXP(path.c_str())) {
            LOGE << "[PresetManager] load FAILED: " << path;
            _undoStack.pop_front();
            return false;
        }
        restoreCCMap(cc);
        LOGD << "[PresetManager] loaded: " << path;
        _lastUsedDir = std::filesystem::path(path).parent_path().string();
        _currentIdx  = factoryCount() + ensureInList(path);
        _divergedFromIndex = false;
        captureBaseline();
        return true;
    }

    // Scan folderPath for .fxp files belonging to this plugin and append any
    // that aren't already in the list. Returns the number of presets added.
    //
    // Group assignment (mirrors _presetDir convention, but unambiguous):
    //   root-level files → group = folder name  (not "", to stay distinct from
    //                                             native presets whose group = "")
    //   one-level subdir → group = subdir name
    //   deeper levels are not scanned
    int addFolder(const std::string& folderPath) {
        namespace fs = std::filesystem;
        const fs::path dir(folderPath);
        if (!fs::exists(dir) || !fs::is_directory(dir)) return 0;

        _lastUsedDir = folderPath;
        const std::string rootGroup = dir.filename().string();
        int added = 0;

        auto tryAdd = [&](const fs::path& p, const std::string& group) {
            if (p.extension() != ".fxp") return;
            if (!isOurFXP(p.string())) {
                LOGD << "[PresetManager] scan skip (wrong plugin): " << p.string();
                return;
            }
            const std::string s = p.string();
            const bool known = std::any_of(_userPresets.begin(), _userPresets.end(),
                [&](const UserPreset& u) { return u.path == s; });
            if (!known) {
                _userPresets.push_back({ s, p.stem().string(), group });
                LOGD << "[PresetManager] scan added [" << group << "] " << p.stem().string();
                ++added;
            }
        };

        for (const auto& e : fs::directory_iterator(dir)) {
            if (e.is_regular_file())
                tryAdd(e.path(), rootGroup);
            else if (e.is_directory()) {
                const std::string subGroup = e.path().filename().string();
                for (const auto& sub : fs::directory_iterator(e))
                    if (sub.is_regular_file())
                        tryAdd(sub.path(), subGroup);
            }
        }
        LOGD << "[PresetManager] addFolder done: " << added << " added from " << folderPath;
        return added;
    }

    // Opens _lastUsedDir in Finder / Explorer (falls back to _presetDir).
    void openFolder() const {
        namespace fs = std::filesystem;
        const std::string& target = _lastUsedDir.empty() ? _presetDir : _lastUsedDir;
        fs::create_directories(target);
#if defined(OS_MAC)
        system(("open \"" + target + "\"").c_str());
#elif defined(OS_WIN)
        ShellExecuteA(nullptr, "explore", target.c_str(), nullptr, nullptr, SW_SHOW);
#endif
    }

    // Append any new .fxp files found in _presetDir that aren't already known.
    void rescanUserPresets() { scanFromDisk(_presetDir); }

    // ── Queries (UI thread) ───────────────────────────────────────────────────

    // Parses "group/name" → {group, name}. No slash → {"", fullName}.
    static std::pair<std::string, std::string> parseName(std::string_view full) {
        const auto slash = full.find('/');
        if (slash == std::string_view::npos)
            return { "", std::string(full) };
        return { std::string(full.substr(0, slash)),
                 std::string(full.substr(slash + 1)) };
    }

    std::string currentName() const {
        if (_currentIdx < 0) return "";
        if (isFactory(_currentIdx)) {
            const int pi = _factoryIdxMap[_currentIdx];
            const char* n = _plugin->GetPresetName(pi);
            return parseName(n ? n : "").second;
        }
        const int ui = _currentIdx - factoryCount();
        return (ui >= 0 && ui < userCount()) ? _userPresets[ui].name : "";
    }

    std::string currentGroup() const {
        if (_currentIdx < 0) return "";
        if (isFactory(_currentIdx)) {
            const int pi = _factoryIdxMap[_currentIdx];
            const char* n = _plugin->GetPresetName(pi);
            return parseName(n ? n : "").first;
        }
        const int ui = _currentIdx - factoryCount();
        return (ui >= 0 && ui < userCount()) ? _userPresets[ui].group : "";
    }

    // True when the live patch is not a pristine stored preset — edited,
    // randomized, mutated, never loaded from one, or explicitly marked diverged
    // (see markPatchCustom). The strip shows it as "user preset" instead of a name.
    bool isCustomPatch() const { return _currentIdx < 0 || isModified() || _divergedFromIndex; }

    // Mark the live patch as no longer matching the preset at the remembered index, WITHOUT
    // discarding that index. The strip then shows "user preset", but next/prev keep cycling
    // from the last preset position. Use when the host changes the sound to something that
    // isn't the indexed preset yet the navigation position should be preserved (e.g. switching
    // between morph points). Cleared automatically the next time a preset is actually applied.
    void markPatchCustom() { _divergedFromIndex = true; }

    // Directory to open file dialogs in: last-used dir, or _presetDir if none.
    const std::string& browseDir() const {
        return _lastUsedDir.empty() ? _presetDir : _lastUsedDir;
    }

    // O(1), safe to call from UI thread every frame
    bool isModified() const { return _modified.load(std::memory_order_relaxed); }

    // Call this from OnParamChange. Only a deliberate on-screen edit (kUI) marks
    // the preset dirty. Host automation (kHost) and MIDI CC (kDelegate) are live
    // control owned by the host/performer, not unsaved edits — tracking them as
    // dirty would (a) light the "*" spuriously and (b) trap undo in an infinite
    // baseline-revert while a continuous LFO / recorded CC lane keeps re-dirtying.
    void onParamChanged(iplug::EParamSource source) {
        if (source == iplug::kUI) {
            _modified.store(true, std::memory_order_relaxed);
            // A genuine on-screen edit branches history → redo no longer applies.
            // (The manager's own restores broadcast as kPresetRecall/kDelegate, not
            // kUI, so they don't reach here.)
            if (!_restoringInternally && !_redoStack.empty())
                _redoStack.clear();
        }
    }

    const std::string& presetDir()   const { return _presetDir; }

    bool canUndo()      const { return _modified.load(std::memory_order_relaxed) || !_undoStack.empty(); }
    bool canRedo()      const { return !_redoStack.empty(); }
    int  currentIdx()   const { return _currentIdx; }
    int  factoryCount() const { return static_cast<int>(_factoryIdxMap.size()); }
    int  userCount()    const { return static_cast<int>(_userPresets.size()); }
    int  totalCount()   const { return factoryCount() + userCount(); }

    const std::vector<UserPreset>& userPresets() const { return _userPresets; }

private:
    // ── Undo entry ────────────────────────────────────────────────────────────

    struct UndoEntry {
        iplug::IByteChunk chunk;          // state at push time (may be dirty)
        iplug::IByteChunk baselineChunk;  // clean baseline at push time
        int               presetIdx;
        bool              modified;
        bool              diverged;       // patch diverged from presetIdx (see _divergedFromIndex)
    };

    // RAII flag set while the manager itself drives a state restore, so the plugin's
    // OnRestoreState → onHostStateRestored() can tell our restores from a host load.
    bool _restoringInternally = false;
    struct InternalRestoreScope {
        bool& flag; bool prev;
        explicit InternalRestoreScope(bool& f) : flag(f), prev(f) { flag = true; }
        ~InternalRestoreScope() { flag = prev; }
    };

    // ── Members ───────────────────────────────────────────────────────────────

    iplug::IPluginBase*     _plugin;
    std::string             _pluginName;
    std::string             _presetDir;
    std::string             _lastUsedDir;     // last dir from save / load / addFolder
    int                     _currentIdx = -1;  // -1 = no preset selected yet
    std::vector<int>        _factoryIdxMap;   // our factory idx → iPlug2 preset idx
    std::vector<UserPreset> _userPresets;

    std::deque<UndoEntry>       _undoStack;
    std::deque<UndoEntry>       _redoStack;       // states undone away from; cleared on any new forward edit
    int                         _undoDepth;       // max entries kept on the undo ring buffer
    mutable std::atomic<bool>   _modified { false };
    bool                        _divergedFromIndex = false;  // patch no longer matches preset @ _currentIdx (index kept)
    iplug::IByteChunk           _baselineChunk;  // clean state of the current preset

    CCSerializeFn   _ccSerialize;
    CCUnserializeFn _ccUnserialize;

    WorkspaceSerializeFn   _wsSerialize;
    WorkspaceUnserializeFn _wsUnserialize;
    NavGateFn              _navGate;
    PresetRedirectFn       _redirect;
    BlockedFn              _navBlocked;
    BlockedFn              _loadBlocked;

    bool isFactory(int idx) const { return idx >= 0 && idx < factoryCount(); }

    iplug::IByteChunk snapshotCCMap() const {
        iplug::IByteChunk c;
        if (_ccSerialize) _ccSerialize(c);
        return c;
    }

    void restoreCCMap(const iplug::IByteChunk& c) const {
        if (_ccUnserialize && c.Size() > 0) _ccUnserialize(c);
    }

    iplug::IByteChunk snapshotWorkspace() const {
        iplug::IByteChunk c;
        if (_wsSerialize) _wsSerialize(c);
        return c;
    }

    void restoreWorkspace(const iplug::IByteChunk& c) const {
        if (_wsUnserialize && c.Size() > 0) _wsUnserialize(c);
    }

    bool navAllowed() const { return !_navGate || _navGate(); }

    // Shared core of next/prev/goTo. The redirect (if any) runs first and may claim the target
    // without touching the patch; we then just move the displayed index so the strip name
    // follows. Otherwise normal, gated navigation.
    void navigateTo(int target) {
        const PresetApply v = _redirect ? _redirect(PresetSource::list(target)) : PresetApply::PassThrough;
        if (v == PresetApply::Handled) { _currentIdx = target; _divergedFromIndex = false; return; }   // host took it; name follows
        if (v == PresetApply::Block)   return;                             // forbidden in this context
        if (!navAllowed() || target == _currentIdx) return;               // legacy gate / already there
        pushUndo();
        applyIdx(target);
    }

    // Snapshot the live state as an undo/redo entry (baselineChunk only when dirty).
    UndoEntry captureEntry() const {
        const bool dirty = _modified.load(std::memory_order_relaxed);
        return { serializeCurrent(),
                 dirty ? _baselineChunk : iplug::IByteChunk{},
                 _currentIdx, dirty, _divergedFromIndex };
    }

    void pushBounded(std::deque<UndoEntry>& stack, UndoEntry e) {
        if (static_cast<int>(stack.size()) >= _undoDepth)
            stack.pop_back();
        stack.push_front(std::move(e));
    }

    // Apply a previously captured entry (used by both undo and redo). The caller
    // has already snapshotted/needs to restore CC + workspace around it.
    void restoreEntry(const UndoEntry& e, const iplug::IByteChunk& cc, const iplug::IByteChunk& ws) {
        int pos = 0;
        _plugin->UnserializeState(e.chunk, pos);
        _plugin->OnRestoreState();
        restoreCCMap(cc);
        restoreWorkspace(ws);
        _currentIdx = std::clamp(e.presetIdx, -1, std::max(-1, totalCount() - 1));
        _divergedFromIndex = e.diverged;
        if (e.modified) {
            _modified.store(true, std::memory_order_relaxed);
            _baselineChunk = e.baselineChunk;   // original clean state before tweaks
        } else {
            captureBaseline();
        }
    }

    void pushUndo() {
        pushBounded(_undoStack, captureEntry());
        _redoStack.clear();   // a fresh forward action invalidates the redo history
    }

    // Navigate to idx. If the target user preset file is missing, removes it
    // from the list and clamps _currentIdx without changing DSP state.
    void applyIdx(int idx) {
        InternalRestoreScope guard(_restoringInternally);
        _currentIdx = idx;
        _divergedFromIndex = false;   // the live patch now IS this preset → strip shows its name
        auto cc = snapshotCCMap();
        auto ws = snapshotWorkspace();
        if (isFactory(idx)) {
            const int pi = _factoryIdxMap[idx];
            LOGD << "[PresetManager] factory preset: " << _plugin->GetPresetName(pi);
            _plugin->RestorePreset(pi);
            restoreCCMap(cc);
            restoreWorkspace(ws);
        } else {
            const int ui = idx - factoryCount();
            if (ui >= 0 && ui < userCount()) {
                const auto& p = _userPresets[ui];
                if (!_plugin->LoadStateFromFXP(p.path.c_str())) {
                    LOGE << "[PresetManager] navigate FAILED (file missing?): " << p.path;
                    _userPresets.erase(_userPresets.begin() + ui);
                    _currentIdx = std::clamp(_currentIdx, 0,
                                             std::max(0, totalCount() - 1));
                    captureBaseline();
                    return;
                }
                restoreCCMap(cc);
                restoreWorkspace(ws);
                LOGD << "[PresetManager] navigated to: " << p.name
                     << (p.group.empty() ? "" : " [" + p.group + "]");
            }
        }
        captureBaseline();
    }

    void captureBaseline() {
        _modified.store(false, std::memory_order_relaxed);
        _baselineChunk = serializeCurrent();
    }

    // Append .fxp files in dir (and immediate subdirs) not already in the list.
    void scanFromDisk(const std::string& dirPath) {
        namespace fs = std::filesystem;
        const fs::path dir(dirPath);
        if (!fs::exists(dir) || !fs::is_directory(dir)) return;

        auto addIfNew = [&](const fs::path& p, const std::string& group) {
            const std::string s = p.string();
            const bool known = std::any_of(_userPresets.begin(), _userPresets.end(),
                [&](const UserPreset& u) { return u.path == s; });
            if (!known)
                _userPresets.push_back({ s, p.stem().string(), group });
        };

        for (const auto& e : fs::directory_iterator(dir))
            if (e.is_regular_file() && e.path().extension() == ".fxp")
                addIfNew(e.path(), "");

        for (const auto& gd : fs::directory_iterator(dir)) {
            if (!gd.is_directory()) continue;
            const std::string group = gd.path().filename().string();
            for (const auto& e : fs::directory_iterator(gd))
                if (e.is_regular_file() && e.path().extension() == ".fxp")
                    addIfNew(e.path(), group);
        }
    }

    // Returns index in _userPresets for path, appending at the end if not found.
    int ensureInList(const std::string& path) {
        auto it = std::find_if(_userPresets.begin(), _userPresets.end(),
            [&](const UserPreset& p) { return p.path == path; });
        if (it != _userPresets.end())
            return static_cast<int>(it - _userPresets.begin());

        namespace fs = std::filesystem;
        std::string group = "";
        const fs::path p(path);
        if (p.parent_path() != fs::path(_presetDir))
            group = p.parent_path().filename().string();

        _userPresets.push_back({ path, p.stem().string(), group });
        return static_cast<int>(_userPresets.size()) - 1;
    }

    // Returns true if the file is a valid FXP preset for this plugin.
    // Only reads the 20-byte header — does not load state.
    bool isOurFXP(const std::string& path) const {
        FILE* fp = fopen(path.c_str(), "rb");
        if (!fp) return false;

        // FXP header is big-endian
        auto read32 = [&]() -> uint32_t {
            uint8_t b[4] = {};
            fread(b, 1, 4, fp);
            return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) |
                   (uint32_t(b[2]) <<  8) |  uint32_t(b[3]);
        };

        const uint32_t chunkMagic = read32();   // 'CcnK'
        read32();                                // byteSize (skip)
        read32();                                // fxpMagic (skip)
        read32();                                // fxpVersion (skip)
        const uint32_t pluginID   = read32();
        fclose(fp);

        return chunkMagic == 0x43636E4Bu &&     // 'CcnK'
               pluginID   == static_cast<uint32_t>(_plugin->GetUniqueID());
    }

    iplug::IByteChunk serializeCurrent() const {
        iplug::IByteChunk chunk;
        _plugin->SerializeState(chunk);
        return chunk;
    }
};

} // namespace hvoya
