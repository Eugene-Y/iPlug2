#include "hvoya_file.hpp"
#include <fstream>
#include <sstream>
#include <iterator>

namespace hvoya {

const std::vector<std::string> HvoyaFile::kEmptyLines;

// ── Helpers ───────────────────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string::npos) return {};
    const auto last  = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

// ── Construction ──────────────────────────────────────────────────────────

HvoyaFile::HvoyaFile(std::string pluginName, std::string pluginVersion)
    : _pluginName(std::move(pluginName))
    , _pluginVersion(std::move(pluginVersion))
{}

// ── Writing ───────────────────────────────────────────────────────────────

void HvoyaFile::setSection(std::string name, std::vector<std::string> lines) {
    if (auto* s = findSection(name))
        s->lines = std::move(lines);
    else
        _sections.push_back({std::move(name), std::move(lines)});
}

std::string HvoyaFile::toString() const {
    std::ostringstream os;
    os << "# hvoya v1\n";
    os << "# plugin " << _pluginName << " " << _pluginVersion << "\n";
    for (const auto& sec : _sections) {
        os << "\n[" << sec.name << "]\n";
        for (const auto& line : sec.lines)
            os << line << "\n";
    }
    return os.str();
}

bool HvoyaFile::toFile(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << toString();
    return f.good();
}

// ── Reading ───────────────────────────────────────────────────────────────

std::optional<HvoyaFile> HvoyaFile::fromText(const std::string& text) {
    std::istringstream is(text);
    std::string line;
    bool foundHeader = false;
    HvoyaFile layout("", "");

    while (std::getline(is, line)) {
        const std::string t = trim(line);
        if (t.empty()) continue;

        if (!foundHeader) {
            if (t == "# hvoya v1") { foundHeader = true; continue; }
            return std::nullopt;  // first non-empty line must be the header
        }

        // Plugin metadata.
        if (t.rfind("# plugin ", 0) == 0) {
            std::istringstream ss(t.substr(9));
            ss >> layout._pluginName >> layout._pluginVersion;
            continue;
        }

        if (t.front() == '#') continue;  // other comments

        // Section header.
        if (t.front() == '[' && t.back() == ']') {
            layout._sections.push_back({t.substr(1, t.size() - 2), {}});
            continue;
        }

        // Content line — append to current section (lines before any section are dropped).
        if (!layout._sections.empty())
            layout._sections.back().lines.push_back(t);
    }

    if (!foundHeader) return std::nullopt;
    return layout;
}

std::optional<HvoyaFile> HvoyaFile::fromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return std::nullopt;
    std::string text((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());
    return fromText(text);
}

// ── Querying ──────────────────────────────────────────────────────────────

HvoyaFile::Section* HvoyaFile::findSection(std::string_view name) {
    for (auto& s : _sections)
        if (s.name == name) return &s;
    return nullptr;
}

const HvoyaFile::Section* HvoyaFile::findSection(std::string_view name) const {
    for (const auto& s : _sections)
        if (s.name == name) return &s;
    return nullptr;
}

bool HvoyaFile::hasSection(std::string_view name) const {
    return findSection(name) != nullptr;
}

const std::vector<std::string>& HvoyaFile::section(std::string_view name) const {
    if (const auto* s = findSection(name)) return s->lines;
    return kEmptyLines;
}

} // namespace hvoya
