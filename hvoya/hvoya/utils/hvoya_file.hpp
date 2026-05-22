#pragma once

/* hvoya_file.hpp — reader/writer for the .hvoya preset file format
 *
 * FORMAT
 * ------
 *   # hvoya v1
 *   # plugin Melter 1.2.3
 *
 *   [user-tab]
 *   slot 12 5
 *   slot 7 -2
 *
 *   [midi-cc]
 *   74 5
 *   71 12
 *
 * Rules:
 *   - First non-empty line must be "# hvoya v1" (identifies the format).
 *   - "# plugin <name> <version>" carries authoring metadata.
 *   - "[section-name]" starts a named section.
 *   - All other non-comment, non-empty lines are content lines of the current section.
 *   - Comment lines (starting with #) are ignored everywhere except the two above.
 *   - Unknown sections are silently preserved on round-trip.
 *
 * USAGE — writing
 * ---------------
 *   HvoyaFile layout("Melter", "1.2.3");
 *   layout.setSection("user-tab", { "slot 12 5", "slot 7" });
 *   layout.toFile("/path/to/preset.hvoya");
 *
 * USAGE — reading
 * ---------------
 *   auto layout = HvoyaFile::fromFile("/path/to/preset.hvoya");
 *   if (layout && layout->hasSection("user-tab")) {
 *       for (const auto& line : layout->section("user-tab"))
 *           // parse line...
 *   }
 *
 * Each consumer reads only its own section; all other sections are ignored.
 * This makes it safe to load a file from a different plugin version or with
 * sections added in the future.
 */

#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace hvoya {

class HvoyaFile {
public:
    // ── Writing ───────────────────────────────────────────────────────────

    HvoyaFile(std::string pluginName, std::string pluginVersion);

    // Set (or replace) the lines for a named section.
    void setSection(std::string name, std::vector<std::string> lines);

    // Serialize to .hvoya text.
    std::string toString() const;

    // Write to file. Returns false on I/O error.
    bool toFile(const std::string& path) const;

    // ── Reading ───────────────────────────────────────────────────────────

    // Parse .hvoya text. Returns nullopt if the "# hvoya v1" header is absent.
    static std::optional<HvoyaFile> fromText(const std::string& text);

    // Read and parse a .hvoya file. Returns nullopt on I/O error or missing header.
    static std::optional<HvoyaFile> fromFile(const std::string& path);

    // ── Querying ──────────────────────────────────────────────────────────

    bool hasSection(std::string_view name) const;

    // Lines of a named section, each trimmed of leading/trailing whitespace.
    // Returns a reference to an empty vector if the section doesn't exist.
    const std::vector<std::string>& section(std::string_view name) const;

    const std::string& pluginName()    const { return _pluginName; }
    const std::string& pluginVersion() const { return _pluginVersion; }

private:
    struct Section { std::string name; std::vector<std::string> lines; };

    std::string          _pluginName;
    std::string          _pluginVersion;
    std::vector<Section> _sections;

    static const std::vector<std::string> kEmptyLines;

    Section*       findSection(std::string_view name);
    const Section* findSection(std::string_view name) const;
};

} // namespace hvoya
