#pragma once
#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// DebugPanel — extensible provider registry for the debug overlay.
//
// Stored as a World resource. Call watch(section, label, fn) at startup to
// register a provider; DebugSystem calls every provider each render frame
// and draws the results as a sectioned text overlay.
//
// Zero engine dependencies — safe to include in any target.
// ---------------------------------------------------------------------------

struct DebugPanel {
    using Provider = std::function<std::string()>;

    struct Row {
        std::string label;
        Provider    fn;
    };

    struct Section {
        std::string      title;
        std::vector<Row> rows;
    };

    bool visible = false;

    // Register a named provider under a section heading.
    // Creates the section if it does not already exist.
    void watch(const std::string& section,
               const std::string& label,
               Provider fn) {
        for (auto& s : sections_) {
            if (s.title == section) {
                s.rows.push_back({label, std::move(fn)});
                return;
            }
        }
        sections_.push_back({section, {{label, std::move(fn)}}});
    }

    const std::vector<Section>& sections() const { return sections_; }

private:
    std::vector<Section> sections_;
};
