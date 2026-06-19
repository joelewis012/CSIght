#include "csight.h"

// ─── Screen constants ─────────────────────────────────────────────────────────
#define SCREEN_W         128
#define SCREEN_H          64
#define TARGET_FLASH_MS  800
#define PRESET_VISIBLE     5

// ─── Trig lookup — defined here, extern'd in header ──────────────────────────
const int8_t SIN64[64] = {
     0,  9, 18, 27, 35, 42, 48, 53,
    56, 58, 59, 58, 56, 53, 48, 42,
    35, 27, 18,  9,  0, -9,-18,-27,
   -35,-42,-48,-53,-56,-58,-59,-58,
   -56,-53,-48,-42,-35,-27,-18, -9,
     0,  9, 18, 27, 35, 42, 48, 53,
    56, 58, 59, 58, 56, 53, 48, 42,
    35, 27, 18,  9,  0, -9,-18,-27,
};
const int8_t COS64[64] = {
    59, 58, 56, 53, 48, 42, 35, 27,
    18,  9,  0, -9,-18,-27,-35,-42,
   -48,-53,-56,-58,-59,-58,-56,-53,
   -48,-42,-35,-27,-18, -9,  0,  9,
    18, 27, 35, 42, 48, 53, 56, 58,
    59, 58, 56, 53, 48, 42, 35, 27,
    18,  9,  0, -9,-18,-27,-35,-42,
   -48,-53,-56,-58,-59,-58,-56,-53,
};

// ─── Helpers ──────────────────────────────────────────────────────────────────
static void draw_centered_str(Canvas* c, int y, const char* str) {
    int w = canvas_string_width(c, str);
    canvas_draw_str(c, (SCREEN_W - w) / 2, y, str);
}

static void draw_circle(Canvas* c, int cx, int cy, int r) {
    int x = 0, y = r, d = 3 - 2 * r;
    while(x <= y) {
        canvas_draw_dot(c, cx + x, cy + y);
        canvas_draw_dot(c, cx - x, cy + y);
        canvas_draw_dot(c, cx + x, cy - y);
        canvas_draw_dot(c, cx - x, cy - y);
        canvas_draw_dot(c, cx + y, cy + x);
        canvas_draw_dot(c, cx - y, cy + x);
        canvas_draw_dot(c, cx + y, cy - x);
        canvas_draw_dot(c, cx - y, cy - x);
        if(d < 0) d += 4 * x + 6;
        else { d += 4 * (x - y) + 10; y--; }
        x++;
    }
}

// ─── Boot animation ───────────────────────────────────────────────────────────
void csight_draw_boot(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    // Radar on left side
    int rcx = 32, rcy = 36, rr = 26;
    int angle = (app->boot_frame * 4) % 64;
    int lx = rcx + (COS64[angle] * rr) / 59;
    int ly = rcy + (SIN64[angle] * rr) / 59;
    draw_circle(c, rcx, rcy, rr);
    draw_circle(c, rcx, rcy, rr / 2);
    canvas_draw_dot(c, rcx, rcy);
    canvas_draw_line(c, rcx, rcy, lx, ly);

    // Text on right side — no overlap
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 68, 18, "CSIght");

    canvas_set_font(c, FontSecondary);

    char dots[5] = "    ";
    uint8_t d = app->boot_frame % 4;
    for(uint8_t i = 0; i < d; i++) dots[i] = '.';
    canvas_draw_str(c, 68, 32, dots);
    canvas_draw_str(c, 62, 46, "WiFi CSI");
    canvas_draw_str(c, 68, 57, "Radar");
}

// ─── Power warning screen ─────────────────────────────────────────────────────
void csight_draw_power_warning(Canvas* c, CSIghtApp* app) {
    UNUSED(app);
    canvas_clear(c);

    canvas_set_font(c, FontPrimary);
    draw_centered_str(c, 12, "! BEFORE YOU START !");

    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 26, "Make sure your ESP32");
    canvas_draw_str(c, 2, 36, "board is powered BEFORE");
    canvas_draw_str(c, 2, 46, "launching this app.");
    canvas_draw_str(c, 2, 56, "USB or 5V pin required.");

    canvas_draw_str(c, 2, 64, "[OK] Continue  [Back] Exit");
}

// ─── Compatibility check ──────────────────────────────────────────────────────
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

    const char* support_str;
    if(app->csi_support == 2)      support_str = "CSI: FULL [OK]";
    else if(app->csi_support == 1) support_str = "CSI: LIMITED [!]";
    else                            support_str = "CSI: NONE  [X]";
    canvas_draw_str(c, 2, 46, support_str);

    if(app->csi_support == 0) {
        canvas_draw_str(c, 2, 58, "Not compatible");
    } else {
        canvas_draw_str(c, 2, 58, "[OK] Continue");
    }
}

// ─── Preset select ────────────────────────────────────────────────────────────
void csight_draw_preset(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "SELECT BOARD");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);

    canvas_set_font(c, FontSecondary);

    int start = (int)app->preset_idx - 2;
    if(start < 0) start = 0;
    if(start > BOARD_PRESET_COUNT - PRESET_VISIBLE)
        start = BOARD_PRESET_COUNT - PRESET_VISIBLE;

    for(int i = 0; i < PRESET_VISIBLE; i++) {
        int idx = start + i;
        if(idx >= BOARD_PRESET_COUNT) break;

        int y = 22 + i * 10;
        bool selected = (idx == (int)app->preset_idx);

        if(selected) {
            canvas_draw_box(c, 0, y - 8, SCREEN_W - 12, 10);
            canvas_set_color(c, ColorWhite);
        }

        canvas_draw_str(c, 3, y, BOARD_PRESETS[idx].name);

        if(!selected) {
            const char* tier_str;
            if     (BOARD_PRESETS[idx].csi_tier == 2) tier_str = "F";
            else if(BOARD_PRESETS[idx].csi_tier == 1) tier_str = "L";
            else                                       tier_str = "X";
            canvas_draw_str(c, SCREEN_W - 10, y, tier_str);
        }

        if(selected) canvas_set_color(c, ColorBlack);
    }

    // Scrollbar
    int bar_h = (PRESET_VISIBLE * (SCREEN_H - 14)) / BOARD_PRESET_COUNT;
    if(bar_h < 4) bar_h = 4;
    int bar_y = 14 + ((int)app->preset_idx * (SCREEN_H - 14 - bar_h)) / (BOARD_PRESET_COUNT - 1);
    canvas_draw_box(c, SCREEN_W - 3, bar_y, 3, bar_h);

    // Legend
    uint8_t tier = BOARD_PRESETS[app->preset_idx].csi_tier;
    const char* legend = (tier == 2) ? "CSI: Full" :
                         (tier == 1) ? "CSI: Limited" : "CSI: None";
    canvas_draw_str(c, 2, SCREEN_H - 1, legend);
}

// ─── Radar display ────────────────────────────────────────────────────────────
void csight_draw_radar(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    bool flashing = app->target_acquired &&
        (furi_get_tick() - app->target_ts) <
        (TARGET_FLASH_MS * furi_kernel_get_tick_frequency() / 1000);

    if(!flashing && app->target_acquired) {
        app->target_acquired = false;
    }

    if(flashing) {
        canvas_draw_box(c, 0, 0, SCREEN_W, SCREEN_H);
        canvas_set_color(c, ColorWhite);
    }

    // Rings
    draw_circle(c, RADAR_CX, RADAR_CY, RADAR_R);
    draw_circle(c, RADAR_CX, RADAR_CY, RADAR_R / 2);
    canvas_draw_dot(c, RADAR_CX, RADAR_CY);

    // Axis dots
    canvas_draw_dot(c, RADAR_CX,          RADAR_CY - RADAR_R);
    canvas_draw_dot(c, RADAR_CX,          RADAR_CY + RADAR_R);
    canvas_draw_dot(c, RADAR_CX - RADAR_R, RADAR_CY);
    canvas_draw_dot(c, RADAR_CX + RADAR_R, RADAR_CY);

    // Sweep line
    uint8_t a = app->sweep_angle & 63;
    int sx = RADAR_CX + (COS64[a] * RADAR_R) / 59;
    int sy = RADAR_CY + (SIN64[a] * RADAR_R) / 59;
    canvas_draw_line(c, RADAR_CX, RADAR_CY, sx, sy);

    // Sweep trail
    for(int t = 1; t <= 3; t++) {
        uint8_t ta = (a + 64 - (uint8_t)(t * 2)) & 63;
        int tx2 = RADAR_CX + (COS64[ta] * (RADAR_R - t * 3)) / 59;
        int ty2 = RADAR_CY + (SIN64[ta] * (RADAR_R - t * 3)) / 59;
        canvas_draw_dot(c, tx2, ty2);
    }

    // Blips
    for(int i = 0; i < MAX_BLIPS; i++) {
        if(app->blips[i].age == 0) continue;
        int bx = RADAR_CX + app->blips[i].x;
        int by = RADAR_CY + app->blips[i].y;
        if(app->blips[i].age > 180) {
            canvas_draw_box(c, bx - 1, by - 1, 3, 3);
        } else if(app->blips[i].age > 80) {
            canvas_draw_box(c, bx, by, 2, 2);
        } else {
            canvas_draw_dot(c, bx, by);
        }
    }

    // Right panel
    int px = RADAR_CX + RADAR_R + 6;

    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, px, 10, "CSIght");

    canvas_set_font(c, FontSecondary);

    canvas_draw_str(c, px, 22, "MOTION");
    int bar_w = (app->motion_intensity * 32) / 255;
    canvas_draw_frame(c, px, 24, 32, 5);
    if(bar_w > 0) canvas_draw_box(c, px, 24, bar_w, 5);

    canvas_draw_str(c, px, 36, "RANGE");
    int prox_w = ((100 - app->proximity) * 32) / 100;
    canvas_draw_frame(c, px, 38, 32, 5);
    if(prox_w > 0) canvas_draw_box(c, px, 38, prox_w, 5);

    char sens_str[12];
    snprintf(sens_str, sizeof(sens_str), "SENS: %d", app->sensitivity);
    canvas_draw_str(c, px, 52, sens_str);

    if(flashing) {
        canvas_set_color(c, ColorBlack);
        canvas_set_font(c, FontPrimary);
        draw_centered_str(c, 56, "TARGET ACQUIRED");
        canvas_set_color(c, ColorBlack);
    }
}

// ─── Waterfall display ────────────────────────────────────────────────────────
void csight_draw_waterfall(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 8, "CSIght  WATERFALL");
    canvas_draw_line(c, 0, 10, SCREEN_W, 10);

    int draw_y_start = 12;
    int bytes = WATERFALL_HEIGHT / 4;

    for(int col = 0; col < WATERFALL_COLS; col++) {
        int src_col = (app->wf_write_col + col) % WATERFALL_COLS;
        uint8_t* data = app->waterfall.cols[src_col];

        for(int b = 0; b < bytes; b++) {
            uint8_t val = data[b];
            int y = draw_y_start + b * 4;
            if(val > 200)       canvas_draw_box(c, col, y, 1, 4);
            else if(val > 120) { canvas_draw_dot(c, col, y); canvas_draw_dot(c, col, y + 2); }
            else if(val > 50)   canvas_draw_dot(c, col, y);
        }
    }

    char sens_str[10];
    snprintf(sens_str, sizeof(sens_str), "S:%d", app->sensitivity);
    canvas_draw_str(c, SCREEN_W - 18, SCREEN_H - 1, sens_str);
}

// ─── Main menu ────────────────────────────────────────────────────────────────
static const char* MAIN_MENU_LABELS[MAIN_MENU_COUNT] = {
    "Start Scanning",
    "Web UI",
    "Settings",
    "About",
};

void csight_draw_main_menu(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    // Header
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "CSIght");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);

    canvas_set_font(c, FontSecondary);

    for(int i = 0; i < MAIN_MENU_COUNT; i++) {
        int y = 24 + i * 12;
        bool selected = (i == (int)app->menu_idx);

        if(selected) {
            canvas_draw_box(c, 0, y - 9, SCREEN_W, 11);
            canvas_set_color(c, ColorWhite);
        }

        // Arrow indicator
        if(selected) canvas_draw_str(c, 2, y, ">");
        canvas_draw_str(c, 12, y, MAIN_MENU_LABELS[i]);

        // Show web UI status on that item
        if(i == (int)MenuItemWebUI) {
            const char* status = app->web_ui_active ? "[ON]" : "[OFF]";
            int sw = canvas_string_width(c, status);
            canvas_draw_str(c, SCREEN_W - sw - 2, y, status);
        }

        if(selected) canvas_set_color(c, ColorBlack);
    }
}

// ─── About screen ─────────────────────────────────────────────────────────────
void csight_draw_about(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    canvas_set_font(c, FontPrimary);
    draw_centered_str(c, 10, "CSIght v1.4");
    canvas_draw_line(c, 0, 13, SCREEN_W, 13);

    canvas_set_font(c, FontSecondary);
    draw_centered_str(c, 25, "WiFi CSI Radar");
    draw_centered_str(c, 35, "for Flipper Zero");

    if(app->chip_name[0] != '\0') {
        char chip_line[32];
        snprintf(chip_line, sizeof(chip_line), "Board: %s", app->chip_name);
        draw_centered_str(c, 48, chip_line);
        const char* tier = app->csi_support == 2 ? "CSI: Full" :
                           app->csi_support == 1 ? "CSI: Limited" : "CSI: None";
        draw_centered_str(c, 58, tier);
    } else {
        draw_centered_str(c, 48, "github.com/");
        draw_centered_str(c, 58, "joelewis012/CSIght");
    }
}

// ─── Settings menu ────────────────────────────────────────────────────────────
static const char* SETTINGS_LABELS[SETTINGS_COUNT] = {
    "Sensitivity",
    "Change Board",
};

void csight_draw_settings(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "SETTINGS");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);

    canvas_set_font(c, FontSecondary);

    for(int i = 0; i < SETTINGS_COUNT; i++) {
        int y = 24 + i * 14;
        bool selected = (i == (int)app->settings_idx);

        if(selected) {
            canvas_draw_box(c, 0, y - 9, SCREEN_W, 12);
            canvas_set_color(c, ColorWhite);
        }

        canvas_draw_str(c, 4, y, SETTINGS_LABELS[i]);

        // Show current value on right side
        char val[16] = "";
        if(i == 0) snprintf(val, sizeof(val), "%d", app->sensitivity);
        if(i == 1) snprintf(val, sizeof(val), ">");

        int vw = canvas_string_width(c, val);
        canvas_draw_str(c, SCREEN_W - vw - 4, y, val);

        if(selected) canvas_set_color(c, ColorBlack);
    }

    canvas_draw_str(c, 2, SCREEN_H - 1, "[OK] Select  [Back] Return");
}

// ─── Web UI screen — shown when browser mode is active ───────────────────────
void csight_draw_webui(Canvas* c, CSIghtApp* app) {
    UNUSED(app);
    canvas_clear(c);

    canvas_set_font(c, FontPrimary);
    draw_centered_str(c, 10, "WEB UI ACTIVE");
    canvas_draw_line(c, 0, 13, SCREEN_W, 13);

    canvas_set_font(c, FontSecondary);
    draw_centered_str(c, 24, "Connect your phone to:");
    draw_centered_str(c, 34, "WiFi: CSIght");
    draw_centered_str(c, 44, "Pass: csight123");
    canvas_draw_line(c, 0, 48, SCREEN_W, 48);
    draw_centered_str(c, 57, "http://192.168.4.1");

    // Animated dots to show it's alive
    char dots[4] = "   ";
    uint8_t d = (app->boot_frame / 10) % 4;
    for(uint8_t i = 0; i < d; i++) dots[i] = '.';
    canvas_draw_str(c, 2, 64, dots);
    canvas_draw_str(c, SCREEN_W - 40, 64, "[OK] Stop");
}

// ─── Proximity display ────────────────────────────────────────────────────────
void csight_draw_proximity(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    // Header
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 8, "CSIght");
    canvas_draw_str(c, 80, 8, "PROXIMITY");
    canvas_draw_line(c, 0, 10, SCREEN_W, 10);

    // Concentric arcs — centered, clear of header and bottom bar
    int cx = SCREEN_W / 2;
    int cy = 34;
    int max_r = 22;
    int r = max_r - (app->proximity * max_r) / 100;
    if(r < 3) r = 3;

    for(int i = 0; i < 3; i++) {
        int ar = r + i * 7;
        if(ar <= max_r + 7) draw_circle(c, cx, cy, ar);
    }

    // Center dot
    canvas_draw_box(c, cx - 1, cy - 1, 3, 3);

    // Percentage — clear space below arcs
    char pct[8];
    snprintf(pct, sizeof(pct), "%d%%", app->proximity);
    canvas_set_font(c, FontPrimary);
    draw_centered_str(c, 52, pct);

    // Motion bar at bottom
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, 62, "SIG");
    canvas_draw_frame(c, 20, 56, SCREEN_W - 22, 7);
    int bar_w = (app->motion_intensity * (SCREEN_W - 24)) / 255;
    if(bar_w > 0) canvas_draw_box(c, 21, 57, bar_w, 5);
}
