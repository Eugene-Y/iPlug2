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
 *   preset index onto a ring buffer (depth = kUndoDepth). undo() restores both.
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

    static constexpr int kUndoDepth = 20;

    // CC-map callbacks — optional. When set, the manager snapshots the map
    // before every state load and restores it afterwards, so MIDI CC
    // assignments are never clobbered by preset navigation.
    using CCSerializeFn   = std::function<void(iplug::IByteChunk&)>;
    using CCUnserializeFn = std::function<void(const iplug::IByteChunk&)>;

    void setCCMapCallbacks(CCSerializeFn serializeFn, CCUnserializeFn unserializeFn) {
        _ccSerialize   = std::move(serializeFn);
        _ccUnserialize = std::move(unserializeFn);
    }

    // ── Construction ──────────────────────────────────────────────────────────

    PresetManager(iplug::IPluginBase* plugin,
                  std::string_view    pluginName,
                  std::string_view    presetDir)
        : _plugin    (plugin)
        , _pluginName(pluginName)
        , _presetDir (presetDir)
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
        pushUndo();
        applyIdx(_currentIdx < 0 ? 0 : (_currentIdx + 1) % totalCount());
    }

    void prev() {
        if (totalCount() == 0) return;
        pushUndo();
        applyIdx(_currentIdx < 0 ? totalCount() - 1 : (_currentIdx - 1 + totalCount()) % totalCount());
    }

    void goTo(int idx) {
        if (idx == _currentIdx || idx < 0 || idx >= totalCount()) return;
        pushUndo();
        applyIdx(idx);
    }

    void undo() {
        auto cc = snapshotCCMap();

        if (_modified.load(std::memory_order_relaxed)) {
            // First undo while dirty: revert to the baseline of the current
            // preset without consuming the navigation stack.
            int pos = 0;
            _plugin->UnserializeState(_baselineChunk, pos);
            _plugin->OnRestoreState();
            restoreCCMap(cc);
            captureBaseline();   // _modified → false, _baselineChunk stays same
            return;
        }

        if (_undoStack.empty()) return;
        const auto& entry = _undoStack.front();
        int pos = 0;
        _plugin->UnserializeState(entry.chunk, pos);
        _plugin->OnRestoreState();
        restoreCCMap(cc);
        _currentIdx = std::clamp(entry.presetIdx, -1, std::max(-1, totalCount() - 1));
        // Restore the dirty flag that was in effect when this entry was pushed.
        // If it was modified, update baseline to match (so a further undo works).
        if (entry.modified) {
            _modified.store(true, std::memory_order_relaxed);
            _baselineChunk = entry.baselineChunk;  // original clean state before tweaks
        } else {
            captureBaseline();
        }
        _undoStack.pop_front();
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
        captureBaseline();
    }

    bool loadFromFile(const std::string& path) {
        pushUndo();
        auto cc = snapshotCCMap();
        if (!_plugin->LoadStateFromFXP(path.c_str())) {
            LOGE << "[PresetManager] load FAILED: " << path;
            _undoStack.pop_front();
            return false;
        }
        restoreCCMap(cc);
        LOGD << "[PresetManager] loaded: " << path;
        _lastUsedDir = std::filesystem::path(path).parent_path().string();
        _currentIdx  = factoryCount() + ensureInList(path);
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
        if (source == iplug::kUI)
            _modified.store(true, std::memory_order_relaxed);
    }

    const std::string& presetDir()   const { return _presetDir; }

    bool canUndo()      const { return _modified.load(std::memory_order_relaxed) || !_undoStack.empty(); }
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
    mutable std::atomic<bool>   _modified { false };
    iplug::IByteChunk           _baselineChunk;  // clean state of the current preset

    CCSerializeFn   _ccSerialize;
    CCUnserializeFn _ccUnserialize;

    bool isFactory(int idx) const { return idx >= 0 && idx < factoryCount(); }

    iplug::IByteChunk snapshotCCMap() const {
        iplug::IByteChunk c;
        if (_ccSerialize) _ccSerialize(c);
        return c;
    }

    void restoreCCMap(const iplug::IByteChunk& c) const {
        if (_ccUnserialize && c.Size() > 0) _ccUnserialize(c);
    }

    void pushUndo() {
        if (static_cast<int>(_undoStack.size()) >= kUndoDepth)
            _undoStack.pop_back();
        const bool dirty = _modified.load(std::memory_order_relaxed);
        // baselineChunk is only needed when dirty — skip the copy otherwise.
        _undoStack.push_front({ serializeCurrent(),
                                dirty ? _baselineChunk : iplug::IByteChunk{},
                                _currentIdx, dirty });
    }

    // Navigate to idx. If the target user preset file is missing, removes it
    // from the list and clamps _currentIdx without changing DSP state.
    void applyIdx(int idx) {
        _currentIdx = idx;
        auto cc = snapshotCCMap();
        if (isFactory(idx)) {
            const int pi = _factoryIdxMap[idx];
            LOGD << "[PresetManager] factory preset: " << _plugin->GetPresetName(pi);
            _plugin->RestorePreset(pi);
            restoreCCMap(cc);
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
