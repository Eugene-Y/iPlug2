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
 *   Factory presets always come first (in MakePreset() call order). User presets follow,
 *   sorted by (group, name). Appends (rescanUserPresets / addFolder) add at the end;
 *   refreshUserPresets() rebuilds and re-sorts, so user positions can shift there — safe,
 *   since a user preset's combined index is session-relative and never persisted: a project
 *   chunk stores a PatchIdentity instead (factory preset by authored name, user preset by
 *   file path). See patchIdentity() / restorePatchIdentity().
 *
 * DELETION
 * --------
 *   refreshUserPresets() reconciles the list with disk on demand (adds / renames / deletes).
 *   Between refreshes, navigation that lands on a missing file removes it from the in-memory
 *   list and stops at the nearest valid position.
 *
 * UNDO
 * ----
 *   Every navigation or load call pushes the current serialized state AND
 *   preset index onto a ring buffer (depth = constructor's undoDepth, default
 *   kDefaultUndoDepth). undo() restores both.
 *   A single on-screen edit gesture is committed as ONE undo step via
 *   commitDirtyEdit() (call it at the end of a knob gesture); host automation of a
 *   single param is still the host's job. An uncommitted dirty edit (e.g. one made
 *   outside a gesture bracket) is reverted in one shot by undo()'s dirty branch.
 *
 *   Sub-session floor: a host can bracket a sub-session (e.g. a morph session) with
 *   setUndoFloor(undoMark()) on enter / clearUndoFloor() on exit, so undo()/canUndo()
 *   stay WITHIN the session and never walk into the history beneath it; the session is
 *   then committed as one outer step via commitFromState() on exit. NB undo()'s dirty
 *   branch reverts to _baselineChunk REGARDLESS of the floor, so a sub-session whose
 *   state differs from the surrounding history (e.g. morph on vs off) should rebaseline()
 *   on enter, else a dirty-revert escapes the session. See squashUndoTo/commitFromState.
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
#include <hvoya/utils/filesystem_compat.hpp>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
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

    // Last-used-directory persistence (so file dialogs reopen where the user left off, across
    // sessions). The manager tracks the dir in-memory (save / load / addFolder); a host that wants
    // it to survive restarts seeds it once at startup via setLastUsedDir and persists changes via
    // the callback. Seeding does NOT fire the callback (it's a restore, not a user action).
    using DirChangedFn = std::function<void(const std::string&)>;
    void setLastUsedDir(std::string dir)          { _lastUsedDir = nativeDir(dir); }
    void setLastUsedDirCallback(DirChangedFn fn)  { _dirChanged  = std::move(fn); }

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
        , _presetDir (nativeDir (presetDir))
        , _undoDepth (std::max(1, undoDepth))
    {
        // Build index map for factory presets, skipping uninitialized slots
        // (iPlug2 names uninitialized slots "empty" — UNUSED_PRESET_NAME).
        for (int i = 0; i < _plugin->NPresets(); ++i) {
            const char* n = _plugin->GetPresetName(i);
            if (n && std::string_view(n) != "empty")
                _factoryIdxMap.push_back(i);
        }

        // Initial scan — the home dir is the first (always-present) source, scanned recursively.
        // Sorted alphabetically so the starting order is clean.
        _sources.push_back({ _presetDir, "", /*verify*/ false });
        scanSource(_sources.front());
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

        // Floored at _undoFloor while a morph session is open → in-morph undo stays within the
        // session and never walks into the pre-morph normal history (0 = no floor / normal mode).
        if (_undoStack.size() <= _undoFloor) return;
        const bool ccStep = _undoStack.front().ccInChunk;   // the redo of a CC step is also a CC step
        pushBounded(_redoStack, captureEntry(ccStep));       // current state → redo
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
        const bool ccStep = _redoStack.front().ccInChunk;
        pushUndoBounded(captureEntry(ccStep));
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

    // ── In-session undo floor (for a morph session) ─────────────────────────────
    //
    // While a sub-session is open, "floor" the undo line at the engage point so undo/redo stay
    // WITHIN the session: undo() / canUndo() won't pop below `floor`, so the pre-session normal
    // history isn't reachable until the session is committed as one step on exit. (Redo is
    // naturally bounded — the first in-session commit clears it.) Set on engage, clear on EVERY
    // session-exit path. The floor auto-tracks ring eviction (see pushUndoBounded).
    void setUndoFloor(UndoMark floor) { _undoFloor = std::min(floor, _undoStack.size()); }
    void clearUndoFloor()             { _undoFloor = 0; }

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
        if (preState.Size() > 0)
            pushUndoBounded({ preState, iplug::IByteChunk{}, _currentIdx, false, false });
        _redoStack.clear();      // a committed forward result invalidates redo
        _currentIdx = -1;        // a baked / programmatic result is a custom patch
        _divergedFromIndex = false;
        captureBaseline();
    }

    // Commit the current uncommitted dirty edit (the param tweaks made since the last
    // clean baseline) as ONE undo step, then rebaseline. The pre-edit state is the clean
    // baseline; the preset INDEX is KEPT (the live patch now diverges from that preset —
    // next/prev still cycle from it, the strip shows "user preset"). No-op if not dirty.
    //
    // Call at the end of a normal (non-morph) param-knob gesture so several separate edits
    // after a preset load each undo individually, instead of collapsing into a single
    // revert-to-baseline. (Differs from commitFromState, which forgets the index because a
    // morph bake / set-all yields a brand-new custom patch. Differs from undo()'s dirty
    // branch, which travels BACKWARD to the baseline; this commits the edit going forward.)
    void commitDirtyEdit() {
        if (!_modified.load(std::memory_order_relaxed)) return;
        // Push the pre-edit clean state, carrying the index + diverged flag as they were
        // BEFORE this edit, so undo lands exactly on the prior (pristine-or-diverged) state.
        pushUndoBounded({ _baselineChunk, iplug::IByteChunk{}, _currentIdx, false, _divergedFromIndex });
        _redoStack.clear();              // a fresh forward edit invalidates redo
        if (_currentIdx >= 0)
            _divergedFromIndex = true;   // the live patch no longer matches the stored preset
        captureBaseline();               // post-edit state becomes the new clean baseline
    }

    // Record a CC-map edit (learn / clear / paste / file) as ONE undo step. The CC map is workspace,
    // normally preserved across patch undos; but here the map IS the change, so the entry is flagged
    // ccInChunk and undo/redo restore it from the entry's chunk. Call BEFORE applying the edit.
    void recordCCUndoStep() {
        pushUndoBounded(captureEntry(/*ccInChunk*/ true));
        _redoStack.clear();   // a fresh forward action invalidates the redo history
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
        _undoFloor = 0;          // fresh history; any morph session re-floors via reconcileSession
        captureBaseline();
    }

    // True while the manager itself is driving a state restore (undo/redo/navigation),
    // as opposed to an external host load. Lets the plugin's OnRestoreState() honor the
    // restored chunk's transient UI selection (e.g. a morph point) during undo/redo,
    // while still clearing a stray selection on a genuine host load. See onHostStateRestored().
    bool isRestoringInternally() const { return _restoringInternally; }

    // ── File I/O ──────────────────────────────────────────────────────────────

    void saveToFile(const std::string& path) {
        if (!_plugin->SavePresetAsFXP(path.c_str())) {
            LOGE << "[PresetManager] save FAILED: " << path;
            return;
        }
        LOGD << "[PresetManager] saved: " << path;
        noteLastUsedDir(hvoya::fs::path(path).parent_path().string());
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
        noteLastUsedDir(hvoya::fs::path(path).parent_path().string());
        _currentIdx  = factoryCount() + ensureInList(path);
        _divergedFromIndex = false;
        captureBaseline();
        return true;
    }

    // Register folderPath as a preset source and scan it RECURSIVELY (any depth), appending .fxp
    // files belonging to this plugin that aren't already known. Returns the number added.
    //
    // Group assignment: the folder's own name is the base group, extended by each file's path
    // relative to it — so `dir/a/b/x.fxp` gets group "dir/a/b" and the browser nests it accordingly.
    // (A root-level file gets the folder name, staying distinct from native presets whose group = "".)
    // The source is remembered so refreshUserPresets() re-walks it on later opens.
    int addFolder(const std::string& folderPath) {
        namespace fs = hvoya::fs;
        fs::path dir(folderPath);
        if (dir.filename().empty()) dir = dir.parent_path();   // drop a trailing separator
        if (!fs::exists(dir) || !fs::is_directory(dir)) return 0;

        noteLastUsedDir(folderPath);
        const std::string root      = dir.string();
        const std::string baseGroup = dir.filename().string();

        if (std::none_of(_sources.begin(), _sources.end(),
                         [&](const SourceFolder& s) { return s.root == root; }))
            _sources.push_back({ root, baseGroup, /*verify*/ true });

        const int added = scanSource({ root, baseGroup, /*verify*/ true });
        LOGD << "[PresetManager] addFolder done: " << added << " added from " << folderPath;
        return added;
    }

    // Opens _lastUsedDir in Finder / Explorer (falls back to _presetDir).
    void openFolder() const {
        namespace fs = hvoya::fs;
        const std::string& target = _lastUsedDir.empty() ? _presetDir : _lastUsedDir;
        fs::create_directories(target);
#if defined(OS_MAC)
        system(("open \"" + target + "\"").c_str());
#elif defined(OS_WIN)
        ShellExecuteA(nullptr, "explore", target.c_str(), nullptr, nullptr, SW_SHOW);
#endif
    }

    // Append any new .fxp files found under the known sources (home + scanned folders) that aren't
    // already known. Existing entries are left untouched (use refreshUserPresets to also drop stale).
    void rescanUserPresets() { for (const auto& s : _sources) scanSource(s); }

    // Reconcile the whole user-preset list with disk: pick up added files, drop deleted ones, and
    // reflect renames (a rename reads as delete-old + add-new). Re-walks every known source (the home
    // dir + each externally scanned folder), recursively, so nested subfolders and scanned collections
    // are all refreshed. Preserves the current selection by PATH (indices shift on rebuild); if the
    // selected file vanished, the strip falls back to "user preset" with the live sound untouched — no
    // DSP side effects. Meant to be called on a user action (e.g. opening the preset browser), not
    // per-block. Factory selection is left untouched.
    void refreshUserPresets() {
        std::string curPath;
        if (_currentIdx >= factoryCount() && _currentIdx < totalCount())
            curPath = _userPresets[static_cast<size_t>(_currentIdx - factoryCount())].path;

        _userPresets.clear();
        for (const auto& src : _sources) scanSource(src);

        std::sort(_userPresets.begin(), _userPresets.end(),
            [](const UserPreset& a, const UserPreset& b) {
                if (a.group != b.group) {
                    if (a.group.empty()) return true;
                    if (b.group.empty()) return false;
                    return a.group < b.group;
                }
                return a.name < b.name;
            });

        if (!curPath.empty()) {
            const auto it = std::find_if(_userPresets.begin(), _userPresets.end(),
                [&](const UserPreset& u) { return u.path == curPath; });
            _currentIdx = (it != _userPresets.end())
                        ? factoryCount() + static_cast<int>(it - _userPresets.begin())
                        : -1;   // the selected file is gone (renamed/deleted) → custom "user preset"
        }
    }

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

    // Two facts, deliberately NOT merged into one "is custom" predicate: the strip's NAME comes from
    // the identity, its "*" from the dirt. Editing keeps the identity — only a sound that stops being
    // that preset's descendant (randomize / a morph bake) drops it.
    bool hasPresetIdentity() const { return _currentIdx >= 0; }
    bool isPatchDirty()      const { return isModified() || _divergedFromIndex; }

    // Mark the live patch as edited away from the preset at the remembered index, WITHOUT
    // discarding that index (next/prev keep cycling from the last preset position). Use when the
    // host changes the sound in a way the manager can't see as a param edit. Cleared automatically
    // the next time a preset is actually applied.
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

    bool canUndo()      const { return _modified.load(std::memory_order_relaxed) || _undoStack.size() > _undoFloor; }
    bool canRedo()      const { return !_redoStack.empty(); }
    int  currentIdx()   const { return _currentIdx; }
    int  factoryCount() const { return static_cast<int>(_factoryIdxMap.size()); }
    int  userCount()    const { return static_cast<int>(_userPresets.size()); }
    int  totalCount()   const { return factoryCount() + userCount(); }

    // Legacy identity: the FACTORY index, -1 for anything a plain index can't express. Superseded by
    // patchIdentity(), but still written for builds that only know this field — and still the
    // fallback when reading a chunk that predates the identity block.
    int  serializablePresetIdx() const {
        return (!isPatchDirty() && isFactory(_currentIdx)) ? _currentIdx : -1;
    }

    // Restore the legacy factory-index identity (see serializablePresetIdx). No DSP side effects —
    // UnserializeState already restored the params; this only fixes the strip label. Call ONLY on a
    // genuine external restore, never during the manager's own preset nav / undo (those set
    // _currentIdx themselves; a stored -1 would wrongly blank a just-navigated factory preset's name).
    void setRestoredPresetIdx(int idx) {
        _currentIdx        = (idx >= 0 && idx < factoryCount()) ? idx : -1;
        _divergedFromIndex = false;
    }

    // ── Portable patch identity (host-project serialization) ──────────────────
    //
    // Which stored preset the live patch came from, in a form that survives a session. Keyed by name
    // / path rather than index: the factory bank can be re-ordered, and a user preset's combined
    // index is session-relative (see PRESET ORDER above), so a stored index would name the wrong
    // preset after a rescan.
    struct PatchIdentity {
        enum class Kind { None, Factory, User };
        Kind        kind    = Kind::None;
        std::string key;              // Factory: authored name ("group/name"); User: full file path
        int         idxHint = -1;     // Factory: the index it had when saved (used only if the name is gone)
        bool        dirty   = false;
    };

    PatchIdentity patchIdentity() const {
        PatchIdentity id;
        id.dirty = isPatchDirty();
        if (_currentIdx < 0) return id;
        if (isFactory(_currentIdx)) {
            const char* n = _plugin->GetPresetName(_factoryIdxMap[static_cast<size_t>(_currentIdx)]);
            id.kind    = PatchIdentity::Kind::Factory;
            id.key     = n ? n : "";
            id.idxHint = _currentIdx;
            return id;
        }
        const int ui = _currentIdx - factoryCount();
        if (ui >= 0 && ui < userCount()) {
            id.kind = PatchIdentity::Kind::User;
            id.key  = _userPresets[static_cast<size_t>(ui)].path;
        }
        return id;
    }

    // Read straight off the identity, so it also names a preset the current session never scanned.
    std::string displayName(const PatchIdentity& id) const {
        if (id.kind == PatchIdentity::Kind::Factory) return id.key;
        if (id.kind != PatchIdentity::Kind::User || id.key.empty()) return "";
        const hvoya::fs::path p(id.key);
        const std::string name  = p.stem().string();
        const std::string group = p.parent_path() == hvoya::fs::path(_presetDir)
                                ? std::string{} : p.parent_path().filename().string();
        return group.empty() ? name : group + "/" + name;
    }

    // Identity of a browse-list entry / a file WITHOUT selecting it — for a host that loads a preset
    // somewhere other than the live patch (Gneiss: into a morph point).
    PatchIdentity identityForListIndex(int idx) const {
        PatchIdentity id;
        if (isFactory(idx)) {
            const char* n = _plugin->GetPresetName(_factoryIdxMap[static_cast<size_t>(idx)]);
            id.kind = PatchIdentity::Kind::Factory; id.key = n ? n : ""; id.idxHint = idx;
        } else if (const int ui = idx - factoryCount(); ui >= 0 && ui < userCount()) {
            id.kind = PatchIdentity::Kind::User;    id.key = _userPresets[static_cast<size_t>(ui)].path;
        }
        return id;
    }

    static PatchIdentity identityForFile(const std::string& path) {
        PatchIdentity id;
        if (!path.empty()) { id.kind = PatchIdentity::Kind::User; id.key = path; }
        return id;
    }

    // Adopt an identity read back from a project chunk (unresolvable → no identity). Same call rule
    // as setRestoredPresetIdx: genuine external restores only.
    void restorePatchIdentity(const PatchIdentity& id) {
        _currentIdx        = resolveIdentity(id);
        _divergedFromIndex = _currentIdx >= 0 && id.dirty;
    }

    const std::vector<UserPreset>& userPresets() const { return _userPresets; }

    // One flat, ordered enumeration of every selectable preset — factory first (in call order),
    // then user (in list order) — each carrying the nav index goTo() expects plus its parsed
    // (group, name). A browser UI groups these into sections; factory group comes from the
    // "group/name" slash convention (empty when the name has no slash), user group from the
    // preset's parent-folder name. Read-only; safe on the UI thread.
    struct BrowseEntry {
        int         navIdx;   // index for goTo() / currentIdx()
        std::string group;    // "" when ungrouped (a browser folds these into a FACTORY / USER section)
        std::string name;
        bool        factory;
    };

    std::vector<BrowseEntry> browseEntries() const {
        std::vector<BrowseEntry> out;
        out.reserve(static_cast<size_t>(totalCount()));
        for (int i = 0; i < factoryCount(); ++i) {
            const char* n = _plugin->GetPresetName(_factoryIdxMap[static_cast<size_t>(i)]);
            // Split on the LAST slash so a multi-level authored name ("A/B/name") yields the full
            // folder path as the group ("A/B") for a nested browser tree; the leaf is the name.
            const std::string_view full = n ? n : "";
            const auto slash = full.rfind('/');
            std::string group = slash == std::string_view::npos ? std::string{} : std::string(full.substr(0, slash));
            std::string name  = slash == std::string_view::npos ? std::string(full) : std::string(full.substr(slash + 1));
            out.push_back({ i, std::move(group), std::move(name), true });
        }
        for (int u = 0; u < userCount(); ++u) {
            const auto& p = _userPresets[static_cast<size_t>(u)];
            out.push_back({ factoryCount() + u, p.group, p.name, false });
        }
        return out;
    }

private:
    // Native separators, because the Windows shell needs them where std::filesystem does not: the
    // file dialog silently ignores an initial dir containing '/' (it opens the host's last-visited
    // folder instead), and ShellExecute "explore" fails the same way.
    static std::string nativeDir(std::string_view dir) {
        return dir.empty() ? std::string{} : hvoya::fs::path(dir).make_preferred().string();
    }

    // Record the directory a user file action landed in, firing the persistence callback on a real
    // change (save / load / addFolder go through here; the startup seed does not).
    void noteLastUsedDir(std::string dir) {
        dir = nativeDir(dir);
        if (dir == _lastUsedDir) return;
        _lastUsedDir = std::move(dir);
        if (_dirChanged) _dirChanged(_lastUsedDir);
    }

    // ── Undo entry ────────────────────────────────────────────────────────────

    struct UndoEntry {
        iplug::IByteChunk chunk;          // state at push time (may be dirty)
        iplug::IByteChunk baselineChunk;  // clean baseline at push time
        int               presetIdx;
        bool              modified;
        bool              diverged;       // patch diverged from presetIdx (see _divergedFromIndex)
        bool              ccInChunk = false; // CC-map step: restore the map FROM chunk, don't overlay live
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

    // A recursively-scanned preset root. The home dir is the first, always-present source (base
    // group ""); each addFolder() registers another (base group = folder name). Session-only —
    // scanned folders are not persisted, so a reopened instance starts with just the home source.
    struct SourceFolder {
        std::string root;        // absolute path of the scan root
        std::string baseGroup;   // "" for the home dir; the folder's own name for a scanned folder
        bool        verify;      // gate .fxp files through isOurFXP (external scans yes, home no)
    };

    iplug::IPluginBase*       _plugin;
    std::string               _pluginName;
    std::string               _presetDir;
    std::string               _lastUsedDir;     // last dir from save / load / addFolder
    int                       _currentIdx = -1;  // -1 = no preset selected yet
    std::vector<int>          _factoryIdxMap;   // our factory idx → iPlug2 preset idx
    std::vector<UserPreset>   _userPresets;
    std::vector<SourceFolder> _sources;         // home + scanned folders, re-walked on refresh

    std::deque<UndoEntry>       _undoStack;
    std::deque<UndoEntry>       _redoStack;       // states undone away from; cleared on any new forward edit
    int                         _undoDepth;       // max entries kept on the undo ring buffer
    UndoMark                    _undoFloor = 0;    // in-session undo floor (0 = none); see setUndoFloor
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
    DirChangedFn           _dirChanged;   // fired when _lastUsedDir changes (host persists it)

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

    // Snapshot the live state as an undo/redo entry (baselineChunk only when dirty). ccInChunk marks
    // a CC-map step, so restoreEntry restores the map from the chunk instead of overlaying the live one.
    UndoEntry captureEntry(bool ccInChunk = false) const {
        const bool dirty = _modified.load(std::memory_order_relaxed);
        return { serializeCurrent(),
                 dirty ? _baselineChunk : iplug::IByteChunk{},
                 _currentIdx, dirty, _divergedFromIndex, ccInChunk };
    }

    void pushBounded(std::deque<UndoEntry>& stack, UndoEntry e) {
        if (static_cast<int>(stack.size()) >= _undoDepth)
            stack.pop_back();
        stack.push_front(std::move(e));
    }

    // Push onto the UNDO stack with ring bounding, keeping the in-morph floor correct: if the ring
    // is full we drop the OLDEST (bottom) entry — which sits at/below the floor — so the floor index
    // (counted from the bottom) must shift down by one to keep pointing at the same engage boundary.
    void pushUndoBounded(UndoEntry e) {
        if (static_cast<int>(_undoStack.size()) >= _undoDepth) {
            _undoStack.pop_back();
            if (_undoFloor > 0) --_undoFloor;
        }
        _undoStack.push_front(std::move(e));
    }

    // Apply a previously captured entry (used by both undo and redo). The caller
    // has already snapshotted/needs to restore CC + workspace around it.
    void restoreEntry(const UndoEntry& e, const iplug::IByteChunk& cc, const iplug::IByteChunk& ws) {
        int pos = 0;
        _plugin->UnserializeState(e.chunk, pos);
        _plugin->OnRestoreState();
        // A CC-map step's chunk already carries the map to restore (UnserializeState applied it);
        // overlaying the live snapshot would un-revert it. Patch steps preserve the live workspace map.
        if (!e.ccInChunk) restoreCCMap(cc);
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
        pushUndoBounded(captureEntry());
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

    // Recursively add .fxp files under src.root (any depth) not already known, tagging each with a
    // group = base group joined by '/' to the file's directory RELATIVE to the root. So the home dir
    // (base "") yields "", "sub", "sub/deep"; a scanned folder (base = its name) yields "Folder",
    // "Folder/sub", … — which the browser tree splits into nested nodes. `verify` gates isOurFXP
    // (on for external scans, off for the home dir). Returns the number added. Directory symlinks are
    // not followed (no cycles) and unreadable subtrees are skipped.
    int scanSource(const SourceFolder& src) {
        namespace fs = hvoya::fs;
        const fs::path root(src.root);
        if (!fs::exists(root) || !fs::is_directory(root)) return 0;

        int added = 0;
        std::error_code ec;
        for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
             it != end; it.increment(ec)) {
            if (ec) break;
            const fs::path p = it->path();
            if (!it->is_regular_file(ec) || p.extension() != ".fxp") continue;
            if (src.verify && !isOurFXP(p.string())) {
                LOGD << "[PresetManager] scan skip (wrong plugin): " << p.string();
                continue;
            }
            const std::string s = p.string();
            const bool known = std::any_of(_userPresets.begin(), _userPresets.end(),
                [&](const UserPreset& u) { return u.path == s; });
            if (known) continue;

            const std::string rel = fs::relative(p.parent_path(), root, ec).generic_string();
            std::string group = src.baseGroup;
            if (!rel.empty() && rel != ".")
                group = group.empty() ? rel : group + "/" + rel;
            _userPresets.push_back({ s, p.stem().string(), group });
            ++added;
        }
        return added;
    }

    // Returns index in _userPresets for path, appending at the end if not found.
    // Identity → a current combined index, or -1 when it can't be resolved in this session.
    int resolveIdentity(const PatchIdentity& id) {
        using Kind = PatchIdentity::Kind;
        if (id.kind == Kind::Factory) {
            for (int i = 0; i < factoryCount(); ++i) {
                const char* n = _plugin->GetPresetName(_factoryIdxMap[static_cast<size_t>(i)]);
                if (n && id.key == n) return i;
            }
            return (id.idxHint >= 0 && id.idxHint < factoryCount()) ? id.idxHint : -1;
        }
        if (id.kind == Kind::User) {
            if (id.key.empty() || !hvoya::fs::exists(hvoya::fs::path(id.key))) return -1;
            return factoryCount() + ensureInList(id.key);   // re-enters a file the session hasn't scanned
        }
        return -1;
    }


    int ensureInList(const std::string& path) {
        auto it = std::find_if(_userPresets.begin(), _userPresets.end(),
            [&](const UserPreset& p) { return p.path == path; });
        if (it != _userPresets.end())
            return static_cast<int>(it - _userPresets.begin());

        namespace fs = hvoya::fs;
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

// PatchIdentity ⇄ byte chunk: [nFields][kind][idxHint][dirty][key]. ONE record format, shared by
// every block that persists an identity. The int fields are count-prefixed, so a later one appends
// here and both directions still read; the trailing string is the part that isn't self-describing —
// a SECOND string would need the owning block's version to gate it.
inline bool putPatchIdentity(iplug::IByteChunk& chunk, const PresetManager::PatchIdentity& id) {
    const int fields[] = { static_cast<int>(id.kind), id.idxHint, id.dirty ? 1 : 0 };
    int nFields = static_cast<int>(std::size(fields));
    bool ok = chunk.Put(&nFields) > 0;
    for (int f : fields) ok &= chunk.Put(&f) > 0;
    ok &= chunk.PutStr(id.key.c_str()) > 0;
    return ok;
}

// Returns the new position, or the input `pos` unchanged if the record can't be read.
inline int getPatchIdentity(const iplug::IByteChunk& chunk, int pos, PresetManager::PatchIdentity& out) {
    using Kind = PresetManager::PatchIdentity::Kind;
    int nFields = 0;
    int p = chunk.Get(&nFields, pos);
    if (p < 0 || nFields < 0 || nFields > 256) return pos;

    int fields[] = { static_cast<int>(Kind::None), -1, 0 };
    const int knownFields = static_cast<int>(std::size(fields));
    for (int i = 0; i < nFields; ++i) {
        int v = 0;
        p = chunk.Get(&v, p);
        if (p < 0) return pos;
        if (i < knownFields) fields[i] = v;      // unknown trailing fields are skipped
    }
    // GetStr reports an end position even when the stored length overruns the chunk, so check it:
    // a corrupt record must leave `pos` where it was, not walk off the end.
    WDL_String key;
    const int keyEnd = chunk.GetStr(key, p);
    if (keyEnd <= p || keyEnd > chunk.Size()) return pos;
    p = keyEnd;
    if (fields[0] < static_cast<int>(Kind::None) || fields[0] > static_cast<int>(Kind::User)) return pos;

    out.kind    = static_cast<Kind>(fields[0]);
    out.idxHint = fields[1];
    out.dirty   = fields[2] != 0;
    out.key     = key.Get() ? key.Get() : "";
    return p;
}

} // namespace hvoya
