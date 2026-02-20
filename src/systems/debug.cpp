#include "debug.hpp"
#include "../debug_panel.hpp"
#include <raylib.h>
#include <string>

static constexpr int   PAD      = 8;
static constexpr int   PANEL_W  = 230;
static constexpr int   ROW_H    = 15;
static constexpr int   FONT_SM  = 10;
static constexpr int   FONT_MD  = 11;
static constexpr int   LABEL_W  = 112;  // pixels from content-left to value column
static constexpr Color BG       = {20,  20,  20,  210};
static constexpr Color DIVIDER  = {80,  80,  80,  200};
static constexpr Color C_TITLE  = {160, 160, 160, 255};
static constexpr Color C_HEADER = {210, 190, 80,  255};
static constexpr Color C_LABEL  = {180, 180, 180, 255};
static constexpr Color C_VALUE  = {255, 255, 255, 255};

void DebugSystem::Update(ecs::World& world, float /*dt*/) {
    auto* panel = world.try_resource<DebugPanel>();
    if (!panel) return;

    if (IsKeyPressed(KEY_F3)) panel->visible = !panel->visible;
    if (!panel->visible) return;

    const auto& sections = panel->sections();

    // --- Compute panel height ---
    int rows_total = 0;
    for (const auto& s : sections)
        rows_total += 1 + static_cast<int>(s.rows.size()); // header + data rows

    const int title_area = ROW_H + PAD;      // "DEBUG" line + gap
    const int content_h  = rows_total * ROW_H + static_cast<int>(sections.size()) * 4; // +4 separator per section
    const int panel_h    = PAD + title_area + content_h + PAD;

    // --- Background ---
    const int ox = 10, oy = 10; // panel origin
    DrawRectangle(ox, oy, PANEL_W, panel_h, BG);
    DrawRectangleLines(ox, oy, PANEL_W, panel_h, DIVIDER);

    // --- Title ---
    int cy = oy + PAD;
    DrawText("DEBUG", ox + PAD, cy, FONT_MD, C_TITLE);
    DrawText("[F3]", ox + PANEL_W - PAD - MeasureText("[F3]", FONT_SM) - 2, cy + 1, FONT_SM, DIVIDER);
    cy += ROW_H + PAD;

    // --- Sections ---
    for (const auto& sec : sections) {
        // Separator line + section title
        DrawLine(ox + PAD, cy, ox + PANEL_W - PAD, cy, DIVIDER);
        cy += 4;
        DrawText(sec.title.c_str(), ox + PAD, cy, FONT_MD, C_HEADER);
        cy += ROW_H;

        for (const auto& row : sec.rows) {
            std::string val = row.fn();
            DrawText(row.label.c_str(), ox + PAD + 4, cy, FONT_SM, C_LABEL);
            DrawText(val.c_str(),       ox + PAD + 4 + LABEL_W, cy, FONT_SM, C_VALUE);
            cy += ROW_H;
        }
    }
}
