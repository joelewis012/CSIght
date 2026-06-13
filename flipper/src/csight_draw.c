#include "csight.h"
#include <math.h>

// ─── Screen constants ─────────────────────────────────────────────────────────
#define SCREEN_W   128
#define SCREEN_H    64

// Target acquired flash duration (ms)
#define TARGET_FLASH_MS  800

// ─── Helpers ──────────────────────────────────────────────────────────────────
static void draw_centered_str(Canvas* c, int y, const char* str) {
    int w = canvas_string_width(c, str);
    canvas_draw_str(c, (SCREEN_W - w) / 2, y, str);
}

// Draw a circle (bresenham)
static void draw_circle(Canvas* c, int cx, int cy, int r) {
    int x = 0, y = r, d = 3 - 2 * r;
    while (x <= y) {
        canvas_draw_dot(c, cx + x, cy + y);
        canvas_draw_dot(c, cx - x, cy + y);
        canvas_draw_dot(c, cx + x, cy - y);
        canvas_draw_dot(c, cx - x, cy - y);
        canvas_draw_dot(c, cx + y, cy + x);
        canvas_draw_dot(c, cx - y, cy + x);
        canvas_draw_dot(c, cx + y, cy - x);
        canvas_draw_dot(c, cx - y, cy - x);
        if (d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

// ─── Boot animation ───────────────────────────────────────────────────────────
void csight_draw_boot(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);

    // Animated title
    draw_centered_str(c, 14, "CSIght");

    canvas_set_font(c, FontSecondary);

    // Progress dots
    char dots[4] = "   ";
    uint8_t d = app->boot_frame % 4;
    for (uint8_t i = 0; i < d; i++) dots[i] = '.';
    char line[24];
    snprintf(line, sizeof(line), "INITIALIZING%s", dots);
    draw_centered_str(c, 28, line);

    // Sweeping line animation during boot
    int angle = (app->boot_frame * 4) % 64;
    int lx = RADAR_CX + (COS64[angle] * RADAR_R) / 59;
    int ly = RADAR_CY + (SIN64[angle] * RADAR_R) / 59;
    canvas_draw_line(c, RADAR_CX, RADAR_CY, lx, ly);
    draw_circle(c, RADAR_CX, RADAR_CY, RADAR_R);

    canvas_set_font(c, FontSecondary);
    draw_centered_str(c, 56, "WiFi CSI Radar");
}

// ─── Compatibility check screen ───────────────────────────────────────────────
void csight_draw_compat(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 12, "ESP32 DETECTED");

    canvas_set_font(c, FontSecondary);

    char line[32];
    snprintf(line, sizeof(line), "Chip: %s", app->chip_name);
    canvas_draw_str(c, 2, 26, line);

    snprintf(line, sizeof(line), "FW:   v%d.%d", app->fw_major, app->fw_minor);
    canvas_draw_str(c, 2, 36, line);

    // CSI support indicator
    const char* support_str;
    if (app->csi_support == 2)      support_str = "CSI: FULL [OK]";
    else if (app->csi_support == 1) support_str = "CSI: LIMITED [!]";
    else                             support_str = "CSI: NONE  [X]";
    canvas_draw_str(c, 2, 46, support_str);

    if (app->csi_support == 0) {
        canvas_draw_str(c, 2, 58, "Not compatible");
    } else {
        canvas_draw_str(c, 2, 58, "[OK] Continue");
    }
}

// ─── Preset select screen ─────────────────────────────────────────────────────
// Shows 5 entries at a time, scrolls with selection
#define PRESET_VISIBLE 5
void csight_draw_preset(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    // Header
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "SELECT BOARD");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);

    canvas_set_font(c, FontSecondary);

    // Scroll window
    int start = (int)app->preset_idx - 2;
    if (start < 0) start = 0;
    if (start > BOARD_PRESET_COUNT - PRESET_VISIBLE)
        start = BOARD_PRESET_COUNT - PRESET_VISIBLE;

    for (int i = 0; i < PRESET_VISIBLE; i++) {
        int idx = start + i;
        if (idx >= BOARD_PRESET_COUNT) break;

        int y = 22 + i * 10;
        bool selected = (idx == (int)app->preset_idx);

        if (selected) {
            canvas_draw_box(c, 0, y - 8, SCREEN_W - 12, 10);
            canvas_set_color(c, ColorWhite);
        }

        canvas_draw_str(c, 3, y, BOARD_PRESETS[idx].name);

        // CSI tier indicator on right side
        const char* tier_str;
        if      (BOARD_PRESETS[idx].csi_tier == 2) tier_str = "F"; // Full
        else if (BOARD_PRESETS[idx].csi_tier == 1) tier_str = "L"; // Limited
        else                                        tier_str = "X"; // None

        if (!selected) {
            // only show tier on non-selected rows
            canvas_draw_str(c, SCREEN_W - 10, y, tier_str);
        }

        if (selected) canvas_set_color(c, ColorBlack);
    }

    // Scrollbar
    int bar_h = (PRESET_VISIBLE * (SCREEN_H - 14)) / BOARD_PRESET_COUNT;
    if (bar_h < 4) bar_h = 4;
    int bar_y = 14 + (app->preset_idx * (SCREEN_H - 14 - bar_h)) / (BOARD_PRESET_COUNT - 1);
    canvas_draw_box(c, SCREEN_W - 3, bar_y, 3, bar_h);

    // CSI tier legend at bottom for selected board
    if (app->preset_idx < BOARD_PRESET_COUNT) {
        uint8_t tier = BOARD_PRESETS[app->preset_idx].csi_tier;
        const char* legend = (tier == 2) ? "CSI: Full" :
                             (tier == 1) ? "CSI: Limited" : "CSI: None";
        canvas_draw_str(c, 2, SCREEN_H - 1, legend);
    }
}

// ─── Radar display ────────────────────────────────────────────────────────────
void csight_draw_radar(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    // TARGET ACQUIRED flash (invert whole screen briefly)
    bool flashing = app->target_acquired &&
        (furi_get_tick() - app->target_ts) < (TARGET_FLASH_MS * furi_kernel_get_tick_frequency() / 1000);
    if (!flashing && app->target_acquired) {
        app->target_acquired = false;
    }

    if (flashing) {
        canvas_draw_box(c, 0, 0, SCREEN_W, SCREEN_H);
        canvas_set_color(c, ColorWhite);
    }

    // ── Radar rings ───────────────────────────────────────────────────────────
    draw_circle(c, RADAR_CX, RADAR_CY, RADAR_R);
    draw_circle(c, RADAR_CX, RADAR_CY, RADAR_R / 2);
    // center dot
    canvas_draw_dot(c, RADAR_CX, RADAR_CY);

    // Crosshairs (faint — just 4 dots at axis intersections)
    canvas_draw_dot(c, RADAR_CX, RADAR_CY - RADAR_R);
    canvas_draw_dot(c, RADAR_CX, RADAR_CY + RADAR_R);
    canvas_draw_dot(c, RADAR_CX - RADAR_R, RADAR_CY);
    canvas_draw_dot(c, RADAR_CX + RADAR_R, RADAR_CY);

    // ── Sweep line ────────────────────────────────────────────────────────────
    uint8_t a = app->sweep_angle & 63;
    int sx = RADAR_CX + (COS64[a] * RADAR_R) / 59;
    int sy = RADAR_CY + (SIN64[a] * RADAR_R) / 59;
    canvas_draw_line(c, RADAR_CX, RADAR_CY, sx, sy);

    // Faint trail (3 steps behind)
    for (int t = 1; t <= 3; t++) {
        uint8_t ta = (a + 64 - t * 2) & 63;
        int tx2 = RADAR_CX + (COS64[ta] * (RADAR_R - t * 3)) / 59;
        int ty2 = RADAR_CY + (SIN64[ta] * (RADAR_R - t * 3)) / 59;
        canvas_draw_dot(c, tx2, ty2);
    }

    // ── Blips ─────────────────────────────────────────────────────────────────
    for (int i = 0; i < MAX_BLIPS; i++) {
        if (app->blips[i].age == 0) continue;
        int bx = RADAR_CX + app->blips[i].x;
        int by = RADAR_CY + app->blips[i].y;
        // Strong blip = 3x3, fading = 1x1
        if (app->blips[i].age > 180) {
            canvas_draw_box(c, bx - 1, by - 1, 3, 3);
        } else if (app->blips[i].age > 80) {
            canvas_draw_box(c, bx, by, 2, 2);
        } else {
            canvas_draw_dot(c, bx, by);
        }
    }

    // ── Right panel ───────────────────────────────────────────────────────────
    canvas_set_font(c, FontSecondary);
    int px = RADAR_CX + RADAR_R + 6;

    // Title
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, px, 10, "CSIght");
    canvas_set_font(c, FontSecondary);

    // Motion level bar
    canvas_draw_str(c, px, 22, "MOTION");
    int bar_w = (app->motion_intensity * 32) / 255;
    canvas_draw_frame(c, px, 24, 32, 5);
    if (bar_w > 0) canvas_draw_box(c, px, 24, bar_w, 5);

    // Proximity bar
    canvas_draw_str(c, px, 36, "RANGE");
    int prox_w = ((100 - app->proximity) * 32) / 100;
    canvas_draw_frame(c, px, 38, 32, 5);
    if (prox_w > 0) canvas_draw_box(c, px, 38, prox_w, 5);

    // Sensitivity
    char sens_str[12];
    snprintf(sens_str, sizeof(sens_str), "SENS: %d", app->sensitivity);
    canvas_draw_str(c, px, 52, sens_str);

    // TARGET ACQUIRED label
    if (flashing) {
        canvas_set_color(c, ColorBlack);
        canvas_set_font(c, FontPrimary);
        draw_centered_str(c, 56, "TARGET ACQUIRED");
    }

    if (flashing) canvas_set_color(c, ColorBlack);
}

// ─── Waterfall display ────────────────────────────────────────────────────────
void csight_draw_waterfall(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    // Title bar
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 8, "CSIght  WATERFALL");
    canvas_draw_line(c, 0, 10, SCREEN_W, 10);

    // Draw waterfall: newest on right, oldest on left
    // Each column = 1px wide, fills from draw_y_start to bottom
    int draw_y_start = 12;

    for (int col = 0; col < WATERFALL_COLS; col++) {
        // Map write col so newest = rightmost
        int src_col = (app->wf_write_col + col) % WATERFALL_COLS;
        int draw_x = col; // left to right

        // Each entry is a compressed amplitude 0-255
        // Map to pixel height within draw_h
        uint8_t *data = app->waterfall.cols[src_col];
        int bytes = WATERFALL_HEIGHT / 4;
        for (int b = 0; b < bytes; b++) {
            uint8_t val = data[b];
            int y = draw_y_start + b * 4;
            // Threshold: high signal = draw dot
            if (val > 200)      canvas_draw_box(c, draw_x, y, 1, 4);
            else if (val > 120) { canvas_draw_dot(c, draw_x, y); canvas_draw_dot(c, draw_x, y+2); }
            else if (val > 50)  canvas_draw_dot(c, draw_x, y);
        }
    }

    // Sensitivity label bottom right
    char sens_str[10];
    snprintf(sens_str, sizeof(sens_str), "S:%d", app->sensitivity);
    canvas_draw_str(c, SCREEN_W - 18, SCREEN_H - 1, sens_str);
}

// ─── Proximity display ────────────────────────────────────────────────────────
void csight_draw_proximity(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    canvas_set_font(c, FontPrimary);
    draw_centered_str(c, 10, "CSIght");

    canvas_set_font(c, FontSecondary);
    draw_centered_str(c, 20, "PROXIMITY");

    // Big arc representing proximity — radius shrinks as target gets closer
    int max_r = 26;
    int r = max_r - (app->proximity * max_r) / 100;
    if (r < 3) r = 3;

    // Draw concentric arcs
    for (int i = 0; i < 3; i++) {
        int ar = r + i * 7;
        if (ar <= max_r) draw_circle(c, SCREEN_W / 2, 42, ar);
    }

    // Center dot = "you"
    canvas_draw_box(c, SCREEN_W / 2 - 1, 41, 3, 3);

    // Proximity percentage
    char pct[12];
    snprintf(pct, sizeof(pct), "%d%%", app->proximity);
    draw_centered_str(c, 62, pct);

    // Motion bar across bottom
    canvas_draw_frame(c, 2, SCREEN_H - 8, SCREEN_W - 4, 6);
    int bar_w = (app->motion_intensity * (SCREEN_W - 6)) / 255;
    if (bar_w > 0) canvas_draw_box(c, 3, SCREEN_H - 7, bar_w, 4);
}
