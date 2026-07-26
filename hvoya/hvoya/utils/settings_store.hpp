#pragma once

/* settings_store.hpp — a tiny typed, INI-backed store for machine-local preferences.
 *
 * PURPOSE
 * -------
 * A THIRD axis of state, distinct from a plugin's patch (the sound) and its workspace
 * (the per-project chunk): a small set of **workstation preferences** that belong to the
 * user's machine, not to any project or preset — e.g. "hover help on/off". They live in one
 * flat key/value file, shared by every instance and every host, and survive independently of
 * whatever project happens to be open.
 *
 * The file is INI-flavoured `key=value`, one entry per line, so it's human-readable and
 * hand-editable for debugging:
 *
 *   # Gneiss preferences
 *   hover_help=1
 *
 * The store is path-agnostic (the caller supplies the full file path, typically from iPlug's
 * `INIPath`), so this header carries no iPlug/SWELL dependency — pure C++ over `hvoya::fs`.
 *
 * FORWARD-COMPAT: unknown keys are preserved on round-trip, so a newer build writing a key an
 * older build doesn't know won't lose it when the older build re-saves. A missing/unreadable
 * file loads as an empty store — every getter then returns its supplied default, so a plugin
 * with no preferences file yet simply runs on defaults.
 *
 * USAGE
 * -----
 *   hvoya::SettingsStore prefs (iniPath);          // loads on construction (missing file → empty)
 *   bool help = prefs.getBool ("hover_help", true);
 *   prefs.setBool ("hover_help", false);
 *   prefs.save();                                   // creates parent dirs, rewrites the file
 *
 * Not thread-safe on its own: guard shared access if more than one thread can touch a single
 * instance. (A process-global instance toggled from the UI thread needs no guard.)
 */

#include <charconv>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <hvoya/utils/filesystem_compat.hpp>

namespace hvoya {

class SettingsStore {
public:
    // Loads the file at `path` immediately; a missing/unreadable file yields an empty store.
    explicit SettingsStore (std::string path)
        : _path (std::move (path)) {
        load();
    }

    // ── Reading ───────────────────────────────────────────────────────────

    std::string getString (std::string_view key, std::string_view dflt) const {
        if (const std::string* v = find (key)) return *v;
        return std::string (dflt);
    }

    bool getBool (std::string_view key, bool dflt) const {
        const std::string* v = find (key);
        if (!v) return dflt;
        return *v == "1" || *v == "true" || *v == "yes" || *v == "on";
    }

    long getInt (std::string_view key, long dflt) const {
        const std::string* v = find (key);
        if (!v) return dflt;
        long out {};
        const char* first = v->data();
        const char* last  = first + v->size();
        auto [ptr, ec] = std::from_chars (first, last, out);
        return ec == std::errc{} ? out : dflt;
    }

    double getReal (std::string_view key, double dflt) const {
        const std::string* v = find (key);
        if (!v) return dflt;
        try { return std::stod (*v); } catch (...) { return dflt; }
    }

    // ── Writing (in-memory; call save() to persist) ───────────────────────

    void setString (std::string_view key, std::string_view value) { put (key, std::string (value)); }
    void setBool   (std::string_view key, bool value)   { put (key, value ? "1" : "0"); }
    void setInt    (std::string_view key, long value)   { put (key, std::to_string (value)); }
    void setReal   (std::string_view key, double value) { put (key, std::to_string (value)); }

    // Write the file (creating parent directories). Returns false on I/O error or empty path.
    bool save() const {
        if (_path.empty()) return false;
        std::error_code ec;
        fs::create_directories (fs::path (_path).parent_path(), ec);
        std::ofstream out (_path, std::ios::trunc);
        if (!out) return false;
        out << "# preferences (auto-generated; key=value, editable)\n";
        for (const auto& [k, v] : _entries) out << k << '=' << v << '\n';
        return out.good();
    }

    const std::string& path() const { return _path; }

private:
    void load() {
        std::ifstream in (_path);
        if (!in) return;
        std::string line;
        while (std::getline (in, line)) {
            std::string_view sv = trim (line);
            if (sv.empty() || sv.front() == '#' || sv.front() == ';') continue;
            const auto eq = sv.find ('=');
            if (eq == std::string_view::npos) continue;
            put (trim (sv.substr (0, eq)), std::string (trim (sv.substr (eq + 1))));
        }
    }

    const std::string* find (std::string_view key) const {
        for (const auto& [k, v] : _entries)
            if (k == key) return &v;
        return nullptr;
    }

    void put (std::string_view key, std::string value) {
        for (auto& [k, v] : _entries) {
            if (k == key) { v = std::move (value); return; }
        }
        _entries.emplace_back (std::string (key), std::move (value));
    }

    static std::string_view trim (std::string_view s) {
        const auto b = s.find_first_not_of (" \t\r\n");
        if (b == std::string_view::npos) return {};
        const auto e = s.find_last_not_of (" \t\r\n");
        return s.substr (b, e - b + 1);
    }

    std::string _path;
    std::vector<std::pair<std::string, std::string>> _entries;   // ordered; preserves unknown keys
};

} // namespace hvoya
