#include "csight.h"

// ─── Screen constants ─────────────────────────────────────────────────────────
#define SCREEN_W         128
#define SCREEN_H          64
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
    canvas_draw_str(c, 68, 30, dots);

    // Show channel survey progress if wifi_channel is 0 (not yet selected)
    if(app->wifi_channel == 0) {
        canvas_draw_str(c, 62, 42, "Scanning ch..");
    } else {
        char ch_str[12];
        snprintf(ch_str, sizeof(ch_str), "CH %d auto", app->wifi_channel);
        canvas_draw_str(c, 62, 42, ch_str);
    }
    canvas_draw_str(c, 68, 57, "WiFi CSI Radar");
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

    canvas_draw_str(c, 2, 62, "[OK] Continue  [Back] Exit");
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
    // Show ALERT! header when armed, normal otherwise
    if(app->alert_armed) {
        canvas_draw_str(c, px, 10, app->alert_triggered ? "!! ALERT !!" : "ARMED");
    } else {
        canvas_draw_str(c, px, 10, "CSIght");
    }

    canvas_set_font(c, FontSecondary);

    // Session stats: elapsed time + event count
    uint32_t elapsed_s = (furi_get_tick() - app->session_start_tick) /
                          furi_kernel_get_tick_frequency();
    char stats[20];
    snprintf(stats, sizeof(stats), "%02lu:%02lu  #%lu",
             elapsed_s / 60, elapsed_s % 60, app->motion_count);
    canvas_draw_str(c, px, 19, stats);

    canvas_draw_str(c, px, 30, "MOTION");
    int bar_w = (app->motion_intensity * 32) / 255;
    canvas_draw_frame(c, px, 32, 32, 5);
    if(bar_w > 0) canvas_draw_box(c, px, 32, bar_w, 5);

    canvas_draw_str(c, px, 43, "RANGE");
    int prox_w = ((100 - app->proximity) * 32) / 100;
    canvas_draw_frame(c, px, 45, 32, 5);
    if(prox_w > 0) canvas_draw_box(c, px, 45, prox_w, 5);

    char sens_str[12];
    snprintf(sens_str, sizeof(sens_str), "SENS: %d", app->sensitivity);
    canvas_draw_str(c, px, 52, sens_str);

    // Control hints
    canvas_draw_str(c, px, 62, "\x11\x10 mode");

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
    canvas_draw_str(c, 2, SCREEN_H - 1, "\x12\x11\x10 [OK]cal");
}

// ─── Main menu ────────────────────────────────────────────────────────────────
static const char* MAIN_MENU_LABELS[MAIN_MENU_COUNT] = {
    "Start Scanning",
    "Web UI",
    "Flash Firmware",
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

    // Hold back to exit hint
    canvas_set_font(c, FontSecondary);
    canvas_draw_str(c, 2, SCREEN_H - 1, "Hold Back to exit");
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
    "WiFi Channel",
    "Alert Level",
    "Re-scan Channels",
    "Configure Nodes",
    "Forget Nodes",
    "SD Logging",
    "Path-loss Exp",
    "Test Alert",
    "Quiet/Busy Preset",
    "Schedule Start",
    "Schedule End",
    "Change Board",
};

void csight_draw_settings(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "SETTINGS");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);
    canvas_set_font(c, FontSecondary);

    // Scrolling window: show 4 items at a time, centred around the selection
    // where possible, so all SETTINGS_COUNT items fit legibly on a 64px screen.
    #define VISIBLE_ROWS 4
    int top = (int)app->settings_idx - 1;
    if(top < 0) top = 0;
    if(top > SETTINGS_COUNT - VISIBLE_ROWS) top = SETTINGS_COUNT - VISIBLE_ROWS;
    if(top < 0) top = 0;

    for(int row = 0; row < VISIBLE_ROWS && (top + row) < SETTINGS_COUNT; row++) {
        int i = top + row;
        int y = 24 + row * 11;
        bool selected = (i == (int)app->settings_idx);

        if(selected) {
            canvas_draw_box(c, 0, y - 8, SCREEN_W, 10);
            canvas_set_color(c, ColorWhite);
        }

        canvas_draw_str(c, 4, y, SETTINGS_LABELS[i]);

        char val[12] = "";
        if(i == (int)SettingSensitivity)  snprintf(val, sizeof(val), "%d",   app->sensitivity);
        if(i == (int)SettingChannel)      snprintf(val, sizeof(val), app->wifi_channel == 0 ? "Auto" : "CH %d", app->wifi_channel);
        if(i == (int)SettingAlertThresh)  snprintf(val, sizeof(val), "%d",   app->alert_threshold);
        if(i == (int)SettingRescanCh)     snprintf(val, sizeof(val), "[OK]");
        if(i == (int)SettingMeshConfig)   snprintf(val, sizeof(val), ">");
        if(i == (int)SettingForgetNodes)  snprintf(val, sizeof(val), "[OK]");
        if(i == (int)SettingSdLogging)    snprintf(val, sizeof(val), app->log_enabled ? "On" : "Off");
        if(i == (int)SettingPathloss)     snprintf(val, sizeof(val), "%d.%d",
                                                     app->pathloss_gamma_x10 / 10,
                                                     app->pathloss_gamma_x10 % 10);
        if(i == (int)SettingTestAlert)    snprintf(val, sizeof(val), "[OK]");
        if(i == (int)SettingPreset)       snprintf(val, sizeof(val), "\x10 \x0f");
        if(i == (int)SettingScheduleStart) {
            if(app->schedule_start_hour == app->schedule_end_hour) {
                snprintf(val, sizeof(val), "Off");
            } else {
                snprintf(val, sizeof(val), "%02d:00", app->schedule_start_hour);
            }
        }
        if(i == (int)SettingScheduleEnd) {
            if(app->schedule_start_hour == app->schedule_end_hour) {
                snprintf(val, sizeof(val), "Off");
            } else {
                snprintf(val, sizeof(val), "%02d:00", app->schedule_end_hour);
            }
        }
        if(i == (int)SettingChangeBoard)  snprintf(val, sizeof(val), ">");

        int vw = canvas_string_width(c, val);
        canvas_draw_str(c, SCREEN_W - vw - 3, y, val);

        if(selected) canvas_set_color(c, ColorBlack);
    }
    #undef VISIBLE_ROWS

    canvas_draw_str(c, 2, 62, "\x10\x0f adj  [OK] select  [Back] save");
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
    canvas_draw_str(c, 2, 62, dots);
    canvas_draw_str(c, SCREEN_W - 40, 62, "[OK] Stop");
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

// ─── Pin config ───────────────────────────────────────────────────────────────
void csight_draw_pin_config(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "Custom Pin Setup");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);

    canvas_set_font(c, FontSecondary);

    // TX pin row
    char tx_str[20];
    snprintf(tx_str, sizeof(tx_str), "ESP TX: GPIO %d", app->tx_pin);
    canvas_draw_str(c, 4, 26, tx_str);
    canvas_draw_str(c, SCREEN_W - 26, 26, "\x12\x11"); // up/down arrows

    // RX pin row
    char rx_str[20];
    snprintf(rx_str, sizeof(rx_str), "ESP RX: GPIO %d", app->rx_pin);
    canvas_draw_str(c, 4, 40, rx_str);
    canvas_draw_str(c, SCREEN_W - 26, 40, "\x10\x0f"); // left/right arrows

    canvas_draw_str(c, 2, 52, "Flipper TX=13 RX=14");
    canvas_draw_str(c, 2, 62, "[OK] Save  [Back] Cancel");
}

// ─── Vitals ───────────────────────────────────────────────────────────────────
void csight_draw_vitals(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);

    // Header
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "VITALS");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);

    canvas_set_font(c, FontSecondary);

    if(!app->vitals_valid) {
        // Still accumulating first 30-second window
        uint8_t dots = (app->boot_frame / 8) % 4;
        char msg[20] = "Measuring";
        for(uint8_t i = 0; i < dots; i++) msg[9 + i] = '.';
        msg[9 + dots] = '\0';
        canvas_draw_str(c, 4, 30, msg);
        canvas_draw_str(c, 4, 42, "Stay still, ~30s");
        canvas_draw_str(c, 4, 54, "Subject between devices");
        canvas_draw_str(c, 2, 62, "[OK]cal \x10\x0f mode");
        return;
    }

    // Breathing row
    canvas_draw_str(c, 2, 24, "BREATHING");
    if(app->breathing_bpm > 0) {
        char bstr[12];
        snprintf(bstr, sizeof(bstr), "%d BPM", app->breathing_bpm);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 70, 24, bstr);
        canvas_set_font(c, FontSecondary);
        // Confidence bar (maps 6-40 BPM to 0-100%)
        int bw = ((app->breathing_bpm - 6) * (SCREEN_W - 4)) / 34;
        if(bw < 0) bw = 0;
        if(bw > SCREEN_W - 4) bw = SCREEN_W - 4;
        canvas_draw_frame(c, 2, 27, SCREEN_W - 4, 5);
        if(bw > 0) canvas_draw_box(c, 2, 27, bw, 5);
    } else {
        canvas_draw_str(c, 70, 24, "---");
    }

    // Heart rate row (experimental)
    canvas_draw_str(c, 2, 42, "HEART*");
    if(app->heart_bpm > 0) {
        char hstr[12];
        snprintf(hstr, sizeof(hstr), "%d BPM", app->heart_bpm);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 70, 42, hstr);
        canvas_set_font(c, FontSecondary);
        int hw = ((app->heart_bpm - 40) * (SCREEN_W - 4)) / 140;
        if(hw < 0) hw = 0;
        if(hw > SCREEN_W - 4) hw = SCREEN_W - 4;
        canvas_draw_frame(c, 2, 45, SCREEN_W - 4, 5);
        if(hw > 0) canvas_draw_box(c, 2, 45, hw, 5);
    } else {
        canvas_draw_str(c, 70, 42, "---");
    }

    canvas_draw_str(c, 2, 57, "*experimental");
    canvas_draw_str(c, 2, 62, "[OK]cal \x10\x0f mode \x12\x11 sens");
}

// ─── Flash screen ─────────────────────────────────────────────────────────────
void csight_draw_flash(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "Flash Firmware");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);
    canvas_set_font(c, FontSecondary);

    switch(app->flash_state) {

        case FlashStateConfirm:
            canvas_draw_str(c, 2, 24, "On your ESP32:");
            canvas_draw_str(c, 2, 34, "1. Hold BOOT button");
            canvas_draw_str(c, 2, 44, "2. Press+release RESET");
            canvas_draw_str(c, 2, 54, "3. Release BOOT");
            canvas_draw_str(c, 2, 62, "[OK] Flash  [Back] Cancel");
            break;

        case FlashStateFlashing: {
            // Status line
            canvas_draw_str(c, 2, 26, app->flash_status[0] ? app->flash_status : "Flashing...");

            // Progress bar
            canvas_draw_frame(c, 2, 32, SCREEN_W - 4, 10);
            int bar_w = ((int)app->flash_progress * (SCREEN_W - 6)) / 100;
            if(bar_w > 0) canvas_draw_box(c, 3, 33, bar_w, 8);

            // Percentage
            char pct_str[8];
            snprintf(pct_str, sizeof(pct_str), "%d%%", app->flash_progress);
            canvas_draw_str(c, SCREEN_W / 2 - 8, 50, pct_str);

            canvas_draw_str(c, 2, 62, "Do not disconnect...");
            break;
        }

        case FlashStateDone:
            canvas_draw_str(c, 2, 30, "Flash complete!");
            canvas_draw_str(c, 2, 42, "ESP32 is rebooting.");
            canvas_draw_str(c, 2, 62, "[OK] Reconnect");
            break;

        case FlashStateError:
            canvas_draw_str(c, 2, 22, "Flash failed:");
            // Word-wrap error across two lines if needed
            canvas_draw_str(c, 2, 34, app->flash_error);
            canvas_draw_str(c, 2, 62, "[OK] Back  [Back] Cancel");
            break;
    }
}

// ─── Multi-node map (v2.0 preview) ────────────────────────────────────────────
// Draws a simple top-down 2D map: primary at origin, secondary nodes at their
// configured positions, and an estimated target dot if 2+ nodes are active.
// This is amplitude-weighted multilateration, not phase-based AoA — accuracy
// improves with more active nodes but stays approximate by design.
void csight_draw_mesh(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "MULTI-NODE MAP");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);
    canvas_set_font(c, FontSecondary);

    // Map area: 90x40 px box, scaled to fit whatever the node spread is
    const int map_x = 2, map_y = 16, map_w = 90, map_h = 40;
    canvas_draw_frame(c, map_x, map_y, map_w, map_h);

    // Find the bounding box of all configured node positions so the map scales
    int16_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;
    for(int n = 0; n < MESH_MAX_NODES; n++) {
        if(app->mesh_node_x_cm[n] < min_x) min_x = app->mesh_node_x_cm[n];
        if(app->mesh_node_x_cm[n] > max_x) max_x = app->mesh_node_x_cm[n];
        if(app->mesh_node_y_cm[n] < min_y) min_y = app->mesh_node_y_cm[n];
        if(app->mesh_node_y_cm[n] > max_y) max_y = app->mesh_node_y_cm[n];
    }
    int16_t span_x = (max_x - min_x) > 0 ? (max_x - min_x) : 1;
    int16_t span_y = (max_y - min_y) > 0 ? (max_y - min_y) : 1;

    // Map a cm coordinate to a pixel inside the box (small margin)
    #define MAP_PX(cm_x) (map_x + 4 + ((cm_x - min_x) * (map_w - 8)) / span_x)
    #define MAP_PY(cm_y) (map_y + map_h - 4 - ((cm_y - min_y) * (map_h - 8)) / span_y)

    // Draw each node as a small circle, filled if active
    for(int n = 0; n < MESH_MAX_NODES; n++) {
        int px = MAP_PX(app->mesh_node_x_cm[n]);
        int py = MAP_PY(app->mesh_node_y_cm[n]);
        if(app->mesh_node_active[n]) {
            canvas_draw_disc(c, px, py, 2);
        } else {
            canvas_draw_circle(c, px, py, 2);
        }
    }

    // Draw estimated target position if we have one
    if(app->mesh_has_estimate) {
        int ex = MAP_PX(app->mesh_est_x_cm);
        int ey = MAP_PY(app->mesh_est_y_cm);
        // Blinking cross so it's visually distinct from node dots
        if((app->boot_frame / 8) % 2 == 0) {
            canvas_draw_line(c, ex - 3, ey, ex + 3, ey);
            canvas_draw_line(c, ex, ey - 3, ex, ey + 3);
        }
    }

    // Side panel: active node count + legend
    int sx = map_x + map_w + 4;
    uint8_t active_count = 0;
    for(int n = 0; n < MESH_MAX_NODES; n++) if(app->mesh_node_active[n]) active_count++;

    char cnt[10];
    snprintf(cnt, sizeof(cnt), "%d/4", active_count);
    canvas_draw_str(c, sx, 24, "NODES");
    canvas_draw_str(c, sx, 34, cnt);

    if(active_count < 2) {
        canvas_draw_str(c, sx, 46, "Need");
        canvas_draw_str(c, sx, 54, "2+");
    }

    // Show how long ago the most-recently-dropped node was last seen —
    // the one usually worth checking first when troubleshooting. Placed in
    // the side panel with clearance above the footer control hints.
    uint32_t best_age_s = 0xFFFFFFFF;
    int      best_node  = -1;
    for(int n = 1; n < MESH_MAX_NODES; n++) {
        if(app->mesh_node_active[n]) continue;
        if(app->mesh_node_last_seen_tick[n] == 0) continue; // never seen at all
        uint32_t age_s = (furi_get_tick() - app->mesh_node_last_seen_tick[n]) /
                          furi_kernel_get_tick_frequency();
        if(age_s < best_age_s) { best_age_s = age_s; best_node = n; }
    }
    if(best_node >= 0) {
        char age_str[16];
        if(best_age_s < 60) {
            snprintf(age_str, sizeof(age_str), "N%d %lus", best_node, best_age_s);
        } else {
            snprintf(age_str, sizeof(age_str), "N%d %lum", best_node, best_age_s / 60);
        }
        canvas_draw_str(c, sx, 58, age_str);
    }

    canvas_draw_str(c, 2, 62, "\x10\x0f mode  [OK] cal");
    #undef MAP_PX
    #undef MAP_PY
}

// ─── Mesh node position config ─────────────────────────────────────────────────
void csight_draw_mesh_config(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "Configure Nodes");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);
    canvas_set_font(c, FontSecondary);

    uint8_t idx = app->mesh_config_node_idx;

    char title[20];
    snprintf(title, sizeof(title), "Node %d position", idx);
    canvas_draw_str(c, 4, 24, title);

    char x_str[20], y_str[20];
    snprintf(x_str, sizeof(x_str), "X: %d cm", app->mesh_node_x_cm[idx]);
    snprintf(y_str, sizeof(y_str), "Y: %d cm", app->mesh_node_y_cm[idx]);

    // Highlight whichever axis is currently being edited
    if(!app->mesh_config_edit_y) {
        canvas_draw_box(c, 2, 30, 70, 11);
        canvas_set_color(c, ColorWhite);
        canvas_draw_str(c, 4, 38, x_str);
        canvas_set_color(c, ColorBlack);
        canvas_draw_str(c, 4, 50, y_str);
    } else {
        canvas_draw_str(c, 4, 38, x_str);
        canvas_draw_box(c, 2, 44, 70, 11);
        canvas_set_color(c, ColorWhite);
        canvas_draw_str(c, 4, 50, y_str);
        canvas_set_color(c, ColorBlack);
    }

    canvas_draw_str(c, 2, 62, "\x10\x0f node \x12\x11 val [OK] X/Y");
}

// ─── Motion heatmap (v3.3) ──────────────────────────────────────────────────
// Same 8x8 grid, coordinate space, and bounding box as the mesh map — cell
// heat is shown via filled-square size since the display has no grayscale.
void csight_draw_heatmap(Canvas* c, CSIghtApp* app) {
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str(c, 2, 10, "MOTION HEATMAP");
    canvas_draw_line(c, 0, 12, SCREEN_W, 12);
    canvas_set_font(c, FontSecondary);

    const int map_x = 2, map_y = 16, map_w = 90, map_h = 40;
    canvas_draw_frame(c, map_x, map_y, map_w, map_h);

    int cell_w = map_w / HEATMAP_GRID;
    int cell_h = map_h / HEATMAP_GRID;

    for(int gy = 0; gy < HEATMAP_GRID; gy++) {
        for(int gx = 0; gx < HEATMAP_GRID; gx++) {
            uint8_t heat = app->heatmap[gy][gx];
            if(heat == 0) continue;

            // Scale box size with heat: small dot at low heat, fills the
            // cell at max heat
            int max_sz = (cell_w < cell_h ? cell_w : cell_h) - 1;
            int sz = 1 + (heat * (max_sz - 1)) / 255;
            if(sz > max_sz) sz = max_sz;

            int cx = map_x + gx * cell_w + cell_w / 2;
            int cy = map_y + gy * cell_h + cell_h / 2;
            canvas_draw_box(c, cx - sz / 2, cy - sz / 2, sz, sz);
        }
    }

    // Node positions overlaid as small circles for spatial reference
    int16_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;
    for(int n = 0; n < MESH_MAX_NODES; n++) {
        if(app->mesh_node_x_cm[n] < min_x) min_x = app->mesh_node_x_cm[n];
        if(app->mesh_node_x_cm[n] > max_x) max_x = app->mesh_node_x_cm[n];
        if(app->mesh_node_y_cm[n] < min_y) min_y = app->mesh_node_y_cm[n];
        if(app->mesh_node_y_cm[n] > max_y) max_y = app->mesh_node_y_cm[n];
    }
    int16_t span_x = (max_x - min_x) > 0 ? (max_x - min_x) : 1;
    int16_t span_y = (max_y - min_y) > 0 ? (max_y - min_y) : 1;
    for(int n = 0; n < MESH_MAX_NODES; n++) {
        if(!app->mesh_node_active[n] && n != 0) continue; // skip unpaired secondaries
        int px = map_x + 2 + ((app->mesh_node_x_cm[n] - min_x) * (map_w - 4)) / span_x;
        int py = map_y + map_h - 2 - ((app->mesh_node_y_cm[n] - min_y) * (map_h - 4)) / span_y;
        canvas_draw_circle(c, px, py, 1);
    }

    int sx = map_x + map_w + 4;
    canvas_draw_str(c, sx, 24, "HEAT");
    canvas_draw_str(c, sx, 34, "MAP");
    canvas_draw_str(c, 2, 62, "\x10\x0f mode  fades ~2s");
}
