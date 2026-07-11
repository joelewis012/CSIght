#include "csight.h"
#include "csight_log.h"
#include <notification/notification_messages.h>

#define TAG         "CSIght"
#define CONFIG_PATH EXT_PATH("apps_data/csight/config.bin")

// ─── Board presets — defined here, extern'd in header ─────────────────────────
const BoardPreset BOARD_PRESETS[] = {
    // ── Official Flipper expansion boards ─────────────────────────────────────
    // FZ WiFi Dev Board uses ESP32-S2 — UART0 pins are GPIO43/44 on S2
    { "FZ WiFi Dev Board",        43, 44,  1 },  // ESP32-S2: limited CSI
    { "FlipMods Mini WiFi",       43, 44,  1 },  // ESP32-S2: limited CSI
    { "FlipMods Combo",          15, 16,  1 },
    { "FEBERIS (BPM Circuits)",   1,  3,  1 },
    { "Marauder DblBarrel 5G",   15, 16,  1 },

    // ── Espressif DevKits ─────────────────────────────────────────────────────
    { "ESP32 DevKit V1",          1,  3,  1 },
    { "ESP32 DevKit V4",          1,  3,  1 },
    { "ESP32-S2 DevKit",          43, 44,  1 },  // S2: limited CSI, UART0=GPIO43/44
    { "ESP32-S3 DevKitC-1",      43, 44,  2 },
    { "ESP32-S3 DevKitM-1",      43, 44,  2 },
    { "ESP32-C3 DevKitC-02",     21, 20,  2 },
    { "ESP32-C3 DevKitM-1",      21, 20,  2 },
    { "ESP32-C6 DevKitC-1",      16, 17,  2 },
    { "ESP32-C6 DevKitM-1",      16, 17,  2 },
    { "ESP32-C61 DevKitC-1",     16, 17,  2 },
    { "ESP32-C5 DevKitC-1",       6,  7,  2 },  // Best CSI; needs IDF v5.5.2+

    // ── Seeed XIAO ────────────────────────────────────────────────────────────
    { "XIAO ESP32-C3",            21, 20,  2 },
    { "XIAO ESP32-S3",            43, 44,  2 },
    { "XIAO ESP32-C6",            16, 17,  2 },

    // ── Popular mini/cheap boards ─────────────────────────────────────────────
    { "C3 Super Mini",            21, 20,  2 },
    { "C3 Mini (generic)",        21, 20,  2 },
    { "ESP32-S3 Zero",            43, 44,  2 },
    { "LOLIN S2 Mini",            43, 44,  1 },  // S2: limited CSI
    { "LOLIN S3 Mini",            43, 44,  2 },
    { "ESP32 Nano (Arduino)",      1,  3,  1 },
    { "TTGO T-Display",            1,  3,  1 },
    { "TTGO T-Display S3",        43, 44,  2 },
    { "Lilygo T-Display S3",      43, 44,  2 },
    { "Lilygo T7 S3",             43, 44,  2 },
    { "Lilygo T-OI Plus (C3)",    21, 20,  2 },
    { "DFRobot Beetle C3",        21, 20,  2 },
    { "FireBeetle 2 ESP32-S3",    43, 44,  2 },
    { "ESP32-CAM (AI-Thinker)",    1,  3,  1 },

    // ── Adafruit ──────────────────────────────────────────────────────────────
    { "Adafruit ESP32 Feather",    1,  3,  1 },
    { "Adafruit Feather ESP32-S2", 43, 44,  1 },  // S2: limited CSI
    { "Adafruit QT Py S2",         43, 44,  1 },  // S2: limited CSI
    { "Adafruit QT Py S3",        43, 44,  2 },
    { "Adafruit QT Py C3",        21, 20,  2 },
    { "Adafruit Feather S3",      43, 44,  2 },

    // ── SparkFun ──────────────────────────────────────────────────────────────
    { "SparkFun ESP32 Thing",      1,  3,  1 },
    { "SparkFun Thing Plus",       1,  3,  1 },
    { "SparkFun C6 Qwiic",        16, 17,  2 },

    // ── Lolin / WEMOS ─────────────────────────────────────────────────────────
    { "LOLIN D32",                 1,  3,  1 },
    { "LOLIN D32 Pro",             1,  3,  1 },
    { "WEMOS D1 Mini32",           1,  3,  1 },
    { "LOLIN S3",                 43, 44,  2 },
    { "LOLIN C3 Mini",            21, 20,  2 },

    // ── M5Stack ───────────────────────────────────────────────────────────────
    { "M5Stack Core",              1,  3,  1 },
    { "M5Stack Core2",             1,  3,  1 },
    { "M5Stack CoreS3",           43, 44,  2 },
    { "M5Stamp C3",               21, 20,  2 },
    { "M5Stamp S3",               43, 44,  2 },
    { "M5StickC Plus",             1,  3,  1 },
    { "M5StickC Plus2",            1,  3,  1 },
    { "M5AtomS3",                 43, 44,  2 },

    // ── Unexpected Maker ──────────────────────────────────────────────────────
    { "TinyS3",                   43, 44,  2 },
    { "FeatherS3",                43, 44,  2 },
    { "FeatherS2",                43, 44,  1 },  // S2: limited CSI
    { "TinyS2",                   43, 44,  1 },  // S2: limited CSI
    { "ProS3",                    43, 44,  2 },
    { "NanoS3",                   43, 44,  2 },

    // ── Olimex ────────────────────────────────────────────────────────────────
    { "Olimex ESP32-EVB",          1,  3,  1 },
    { "Olimex ESP32-S3",          43, 44,  2 },

    // ── NodeMCU ───────────────────────────────────────────────────────────────
    { "NodeMCU-32S",               1,  3,  1 },
    { "NodeMCU ESP-C3-32S",       21, 20,  2 },

    // ── Custom ────────────────────────────────────────────────────────────────
    { "Custom...",                 0,  0,  0 },
};
const int BOARD_PRESET_COUNT = (int)(sizeof(BOARD_PRESETS) / sizeof(BOARD_PRESETS[0]));

// ─── Config struct ────────────────────────────────────────────────────────────
typedef struct {
    uint8_t  magic;           // CONFIG_MAGIC = struct is valid
    uint8_t  preset_idx;
    uint8_t  tx_pin;
    uint8_t  rx_pin;
    uint8_t  sensitivity;
    uint8_t  display_mode;
    uint8_t  wifi_channel;    // 0 = auto, 1-13 = manual
    uint8_t  alert_threshold; // 0-255 motion level that fires alarm
    int16_t  mesh_x[3];       // node 1-3 position, cm (node 0 always at origin)
    int16_t  mesh_y[3];
    uint8_t  log_enabled;     // 0/1 — SD card event logging toggle
    uint8_t  pathloss_gamma_x10; // trilateration tuning, 10-50 (1.0-5.0)
    uint8_t  schedule_start_hour; // 0-23, start==end means disabled
    uint8_t  schedule_end_hour;
} CSIghtConfig;

#define CONFIG_MAGIC 0xCB  // bumped: schedule_start/end_hour added (v3.3)

// ─── Config I/O ──────────────────────────────────────────────────────────────
void csight_config_save(CSIghtApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/csight"));

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        CSIghtConfig cfg = {
            .magic           = CONFIG_MAGIC,
            .preset_idx      = app->preset_idx,
            .tx_pin          = app->tx_pin,
            .rx_pin          = app->rx_pin,
            .sensitivity     = app->sensitivity,
            .display_mode    = (uint8_t)app->display_mode,
            .wifi_channel    = app->wifi_channel,
            .alert_threshold = app->alert_threshold,
            .mesh_x          = { app->mesh_node_x_cm[1], app->mesh_node_x_cm[2], app->mesh_node_x_cm[3] },
            .mesh_y          = { app->mesh_node_y_cm[1], app->mesh_node_y_cm[2], app->mesh_node_y_cm[3] },
            .log_enabled     = app->log_enabled ? 1 : 0,
            .pathloss_gamma_x10 = app->pathloss_gamma_x10,
            .schedule_start_hour = app->schedule_start_hour,
            .schedule_end_hour   = app->schedule_end_hour,
        };
        storage_file_write(file, &cfg, sizeof(cfg));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// Returns true if config existed and was loaded successfully
bool csight_config_load(CSIghtApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool loaded = false;

    if(storage_file_open(file, CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        CSIghtConfig cfg;
        if(storage_file_read(file, &cfg, sizeof(cfg)) == sizeof(cfg)
           && cfg.magic == CONFIG_MAGIC) {
            app->preset_idx      = cfg.preset_idx < (uint8_t)BOARD_PRESET_COUNT ? cfg.preset_idx : 0;
            app->tx_pin          = (cfg.tx_pin  != 0xFF) ? cfg.tx_pin  : 13;
            app->rx_pin          = (cfg.rx_pin  != 0xFF) ? cfg.rx_pin  : 14;
            app->sensitivity     = cfg.sensitivity <= 10 ? cfg.sensitivity : 5;
            app->display_mode    = cfg.display_mode <= (uint8_t)DisplayModeHeatmap ?
                                   (DisplayMode)cfg.display_mode : DisplayModeRadar;
            app->wifi_channel    = cfg.wifi_channel;    // 0 = will auto-survey on connect
            app->alert_threshold = cfg.alert_threshold > 0 ? cfg.alert_threshold : 180;

            // Mesh positions — node 0 always at origin, nodes 1-3 loaded from config.
            // If a loaded position is all-zero, fall back to the default square layout
            // (distinguishes "never configured" from "user genuinely wants node at 0,0").
            app->mesh_node_x_cm[0] = 0;
            app->mesh_node_y_cm[0] = 0;
            static const int16_t default_x[3] = { 200, 200, 0 };
            static const int16_t default_y[3] = { 0,   200, 200 };
            for(int n = 0; n < 3; n++) {
                bool unset = (cfg.mesh_x[n] == 0 && cfg.mesh_y[n] == 0);
                app->mesh_node_x_cm[n + 1] = unset ? default_x[n] : cfg.mesh_x[n];
                app->mesh_node_y_cm[n + 1] = unset ? default_y[n] : cfg.mesh_y[n];
            }
            app->log_enabled = (cfg.log_enabled != 0);
            app->pathloss_gamma_x10 = (cfg.pathloss_gamma_x10 >= 10 && cfg.pathloss_gamma_x10 <= 50)
                                      ? cfg.pathloss_gamma_x10 : 20;
            app->schedule_start_hour = (cfg.schedule_start_hour < 24) ? cfg.schedule_start_hour : 0;
            app->schedule_end_hour   = (cfg.schedule_end_hour < 24)   ? cfg.schedule_end_hour   : 0;
            loaded = true;
        }
    }

    if(!loaded) {
        // First run defaults
        app->preset_idx      = 0;
        app->tx_pin          = 13;
        app->rx_pin          = 14;
        app->sensitivity     = 5;
        app->display_mode    = DisplayModeRadar;
        app->wifi_channel    = 0;   // 0 = not yet auto-selected
        app->alert_threshold = 180;
        app->log_enabled     = true;
        app->pathloss_gamma_x10 = 20; // 2.0 default
        app->schedule_start_hour = 0; // 0==0 means disabled by default
        app->schedule_end_hour   = 0;

        // Default mesh layout — matches ESP32 firmware's built-in fallback
        // (a 2m x 2m square), overridden here once user configures real positions
        app->mesh_node_x_cm[0] = 0;    app->mesh_node_y_cm[0] = 0;
        app->mesh_node_x_cm[1] = 200;  app->mesh_node_y_cm[1] = 0;
        app->mesh_node_x_cm[2] = 200;  app->mesh_node_y_cm[2] = 200;
        app->mesh_node_x_cm[3] = 0;    app->mesh_node_y_cm[3] = 200;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return loaded;
}

// ─── Blip logic ───────────────────────────────────────────────────────────────
void csight_add_blip(CSIghtApp* app, uint8_t intensity, uint8_t proximity) {
    int oldest = 0;
    for(int i = 1; i < MAX_BLIPS; i++) {
        if(app->blips[i].age < app->blips[oldest].age) oldest = i;
    }

    uint8_t a  = app->sweep_angle & 63;
    uint8_t ja = (a + (intensity & 3)) & 63;
    int dist   = 2 + ((100 - proximity) * (RADAR_R - 4)) / 100;

    app->blips[oldest].x         = (int8_t)((COS64[ja] * dist) / 59);
    app->blips[oldest].y         = (int8_t)((SIN64[ja] * dist) / 59);
    app->blips[oldest].age       = 255;
    app->blips[oldest].intensity = intensity;
}

// ─── Heatmap ──────────────────────────────────────────────────────────────────
void csight_heatmap_add(CSIghtApp* app, int16_t x_cm, int16_t y_cm) {
    // Same bounding-box logic as csight_draw_mesh, so the heatmap and map
    // always agree on where things are
    int16_t min_x = 0, max_x = 0, min_y = 0, max_y = 0;
    for(int n = 0; n < MESH_MAX_NODES; n++) {
        if(app->mesh_node_x_cm[n] < min_x) min_x = app->mesh_node_x_cm[n];
        if(app->mesh_node_x_cm[n] > max_x) max_x = app->mesh_node_x_cm[n];
        if(app->mesh_node_y_cm[n] < min_y) min_y = app->mesh_node_y_cm[n];
        if(app->mesh_node_y_cm[n] > max_y) max_y = app->mesh_node_y_cm[n];
    }
    int16_t span_x = (max_x - min_x) > 0 ? (max_x - min_x) : 1;
    int16_t span_y = (max_y - min_y) > 0 ? (max_y - min_y) : 1;

    int gx = ((x_cm - min_x) * HEATMAP_GRID) / span_x;
    int gy = ((y_cm - min_y) * HEATMAP_GRID) / span_y;
    if(gx < 0) gx = 0; if(gx >= HEATMAP_GRID) gx = HEATMAP_GRID - 1;
    if(gy < 0) gy = 0; if(gy >= HEATMAP_GRID) gy = HEATMAP_GRID - 1;

    // Add heat, capped at 255. Neighbouring cells get a smaller bump too —
    // avoids a single hard pixel-sized dot and better reflects that the
    // position estimate itself has some inherent fuzziness (see v2.3 notes).
    for(int dy = -1; dy <= 1; dy++) {
        for(int dx = -1; dx <= 1; dx++) {
            int nx = gx + dx, ny = gy + dy;
            if(nx < 0 || nx >= HEATMAP_GRID || ny < 0 || ny >= HEATMAP_GRID) continue;
            int add = (dx == 0 && dy == 0) ? 60 : 15;
            int v = app->heatmap[ny][nx] + add;
            app->heatmap[ny][nx] = (v > 255) ? 255 : (uint8_t)v;
        }
    }
}

// ─── Tick ─────────────────────────────────────────────────────────────────────
void csight_tick(CSIghtApp* app) {
    app->sweep_angle = (app->sweep_angle + 1) & 63;

    for(int i = 0; i < MAX_BLIPS; i++) {
        if(app->blips[i].age > 0) app->blips[i].age -= 2;
    }

    // Heatmap decay — every ~2s, fade all cells slightly so the display
    // reflects recent activity rather than accumulating forever
    uint32_t now = furi_get_tick();
    if(now - app->heatmap_last_decay_tick > 2 * furi_kernel_get_tick_frequency()) {
        app->heatmap_last_decay_tick = now;
        for(int y = 0; y < HEATMAP_GRID; y++) {
            for(int x = 0; x < HEATMAP_GRID; x++) {
                if(app->heatmap[y][x] > 0) {
                    app->heatmap[y][x] = (app->heatmap[y][x] > 10) ? app->heatmap[y][x] - 10 : 0;
                }
            }
        }
    }

    // Clear target flag once flash window has expired
    if(app->target_acquired) {
        uint32_t elapsed = furi_get_tick() - app->target_ts;
        uint32_t flash_ticks = TARGET_FLASH_MS * furi_kernel_get_tick_frequency() / 1000;
        if(elapsed >= flash_ticks) app->target_acquired = false;
    }

    if(app->state == AppStateBooting) {
        app->boot_frame++;
        if(app->boot_frame > 40) {
            app->state      = AppStatePowerWarning;
            app->boot_frame = 0;
        }
    }

    if(app->state == AppStateConnecting) {
        app->boot_frame++;
        if(app->boot_frame > 90) {
            app->state      = AppStateMainMenu;
            app->boot_frame = 0;
        }
    }

    if(app->state == AppStateWebUI) {
        app->boot_frame++;
    }

    // Vitals "Measuring..." animation needs boot_frame ticking during scan
    if(app->state == AppStateScanning && app->display_mode == DisplayModeVitals) {
        app->boot_frame++;
    }

    // Auto-reset alert after 5 seconds of no new motion
    if(app->alert_triggered) {
        uint32_t alert_age = furi_get_tick() - app->alert_ts;
        if(alert_age > 5 * furi_kernel_get_tick_frequency()) {
            app->alert_triggered = false;
        }
    }

    // Auto-dismiss the "Node discovered" banner after 3 seconds
    if(app->node_found_pending) {
        uint32_t banner_age = furi_get_tick() - app->node_found_ts;
        if(banner_age > 3 * furi_kernel_get_tick_frequency()) {
            app->node_found_pending = false;
        }
    }

    // Scheduled auto-arm — only active while scanning, since that's the only
    // context where alert_armed is meaningful. start==end means "disabled".
    if(app->state == AppStateScanning && app->schedule_start_hour != app->schedule_end_hour) {
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        uint8_t hour = dt.hour;

        bool in_window;
        if(app->schedule_start_hour < app->schedule_end_hour) {
            // Same-day window, e.g. 09-17
            in_window = (hour >= app->schedule_start_hour && hour < app->schedule_end_hour);
        } else {
            // Overnight wraparound, e.g. 22-06
            in_window = (hour >= app->schedule_start_hour || hour < app->schedule_end_hour);
        }

        if(in_window != app->alert_armed) {
            app->alert_armed     = in_window;
            app->alert_triggered = false;
        }
    }
}

// ─── Input ────────────────────────────────────────────────────────────────────
static void handle_input(CSIghtApp* app, InputKey key, InputType type) {
    if(type != InputTypeShort && type != InputTypeLong && type != InputTypeRepeat) return;

    switch(app->state) {

        // ── Power warning — confirm board is powered before UART init ──────────
        case AppStatePowerWarning:
            if(key == InputKeyOk) {
                csight_uart_init(app);
                csight_send_handshake(app);
                app->state      = AppStateConnecting;
                app->boot_frame = 0;
            }
            break;

        // ── Main menu ──────────────────────────────────────────────────────────
        case AppStateMainMenu:
            if(key == InputKeyUp && app->menu_idx > 0) {
                app->menu_idx--;
            } else if(key == InputKeyDown && app->menu_idx < MAIN_MENU_COUNT - 1) {
                app->menu_idx++;
            } else if(key == InputKeyOk) {
                switch(app->menu_idx) {
                    case MenuItemScan:
                        app->state              = AppStateScanning;
                        app->session_start_tick = furi_get_tick();
                        app->motion_count       = 0;
                        app->alert_armed        = false;
                        app->alert_triggered    = false;
                        csight_send_channel(app);
                        csight_send_node_positions(app);
                        csight_send_pathloss_gamma(app);
                        csight_log_event(app, "SESSION_START", "");
                        break;
                    case MenuItemWebUI:
                        csight_send_webui_toggle(app);
                        if(app->web_ui_active) {
                            app->state = AppStateWebUI;
                        }
                        break;
                    case MenuItemFlash:
                        app->flash_state = FlashStateConfirm;
                        app->state = AppStateFlash;
                        break;
                    case MenuItemSettings:
                        app->settings_idx = 0;
                        app->state = AppStateSettings;
                        break;
                    case MenuItemAbout:
                        app->state = AppStateAbout;
                        break;
                    default:
                        break;
                }
            }
            break;

        // ── Compat check — shown after handshake ───────────────────────────────
        case AppStateCompatCheck:
            if(key == InputKeyOk && app->csi_support > 0) {
                app->state = AppStatePresetSelect;
            }
            if(key == InputKeyBack) {
                app->state = AppStateMainMenu;
            }
            break;

        // ── Board preset select ────────────────────────────────────────────────
        case AppStatePresetSelect:
            if(key == InputKeyUp && app->preset_idx > 0) {
                app->preset_idx--;
            } else if(key == InputKeyDown && app->preset_idx < (uint8_t)(BOARD_PRESET_COUNT - 1)) {
                app->preset_idx++;
            } else if(key == InputKeyOk) {
                if(app->preset_idx != (uint8_t)BOARD_PRESET_CUSTOM) {
                    app->tx_pin = BOARD_PRESETS[app->preset_idx].tx_pin;
                    app->rx_pin = BOARD_PRESETS[app->preset_idx].rx_pin;
                    app->config_exists = true;
                    csight_config_save(app);
                    app->state = AppStateMainMenu;
                } else {
                    app->state = AppStatePinConfig;
                }
            } else if(key == InputKeyBack) {
                app->state = AppStateSettings;
            }
            break;

        // ── Custom pin config ──────────────────────────────────────────────────
        case AppStatePinConfig:
            if(key == InputKeyUp   && app->tx_pin < 39) app->tx_pin++;
            if(key == InputKeyDown && app->tx_pin > 0)  app->tx_pin--;
            if(key == InputKeyRight && app->rx_pin < 39) app->rx_pin++;
            if(key == InputKeyLeft  && app->rx_pin > 0)  app->rx_pin--;
            if(key == InputKeyOk) {
                app->config_exists = true;
                csight_config_save(app);
                app->state = AppStateMainMenu;
            }
            if(key == InputKeyBack) {
                app->state = AppStatePresetSelect;
            }
            break;

        // ── Scanning ──────────────────────────────────────────────────────────
        case AppStateScanning:
            if(key == InputKeyLeft) {
                app->display_mode = (DisplayMode)((app->display_mode + DISPLAY_MODE_COUNT - 1) % DISPLAY_MODE_COUNT);
                csight_send_mode(app);
            } else if(key == InputKeyRight) {
                app->display_mode = (DisplayMode)((app->display_mode + 1) % DISPLAY_MODE_COUNT);
                csight_send_mode(app);
            }
            if(key == InputKeyUp && app->sensitivity < 10) {
                app->sensitivity++;
                csight_send_sensitivity(app);
            }
            if(key == InputKeyDown && app->sensitivity > 0) {
                app->sensitivity--;
                csight_send_sensitivity(app);
            }
            if(key == InputKeyOk && type == InputTypeShort) {
                // Force baseline recalibration
                csight_send_calibrate(app);
                notification_message(app->notifications, &sequence_blink_cyan_10);
            }
            if(key == InputKeyOk && type == InputTypeLong) {
                if(app->schedule_start_hour != app->schedule_end_hour) {
                    // Schedule is active — it owns the armed state, ignore manual toggle
                    notification_message(app->notifications, &sequence_blink_cyan_10);
                } else {
                    app->alert_armed     = !app->alert_armed;
                    app->alert_triggered = false;
                    notification_message(app->notifications,
                        app->alert_armed ? &sequence_blink_red_10
                                         : &sequence_blink_green_10);
                }
            }
            if(key == InputKeyBack) {
                csight_config_save(app);
                uint32_t elapsed_s = (furi_get_tick() - app->session_start_tick) /
                                      furi_kernel_get_tick_frequency();
                char detail[32];
                snprintf(detail, sizeof(detail), "%lus, %lu events",
                         elapsed_s, app->motion_count);
                csight_log_event(app, "SESSION_END", detail);
                app->state = AppStateMainMenu;
            }
            break;

        // ── Settings ──────────────────────────────────────────────────────────
        case AppStateSettings:
            if(key == InputKeyUp && app->settings_idx > 0) {
                app->settings_idx--;
            } else if(key == InputKeyDown && app->settings_idx < SETTINGS_COUNT - 1) {
                app->settings_idx++;
            } else if(key == InputKeyOk) {
                if(app->settings_idx == (uint8_t)SettingRescanCh) {
                    csight_send_channel_auto(app);  // ~13s survey, result comes back async
                } else if(app->settings_idx == (uint8_t)SettingMeshConfig) {
                    app->mesh_config_node_idx = 1;
                    app->mesh_config_edit_y   = false;
                    app->state = AppStateMeshConfig;
                } else if(app->settings_idx == (uint8_t)SettingForgetNodes) {
                    csight_send_forget_nodes(app);
                    notification_message(app->notifications, &sequence_blink_red_10);
                } else if(app->settings_idx == (uint8_t)SettingSdLogging) {
                    app->log_enabled = !app->log_enabled;
                    if(app->log_enabled) csight_log_init(app); // (re)create file if needed
                } else if(app->settings_idx == (uint8_t)SettingTestAlert) {
                    notification_message(app->notifications, &sequence_blink_red_100);
                } else if(app->settings_idx == (uint8_t)SettingChangeBoard) {
                    app->state = AppStatePresetSelect;
                }
            }
            if(app->settings_idx == (uint8_t)SettingSensitivity) {
                if(key == InputKeyLeft  && app->sensitivity > 0)  { app->sensitivity--; csight_send_sensitivity(app); }
                if(key == InputKeyRight && app->sensitivity < 10) { app->sensitivity++; csight_send_sensitivity(app); }
            } else if(app->settings_idx == (uint8_t)SettingChannel) {
                if(key == InputKeyLeft  && app->wifi_channel > 1)  { app->wifi_channel--; csight_send_channel(app); }
                if(key == InputKeyRight && app->wifi_channel < 13) { app->wifi_channel++; csight_send_channel(app); }
            } else if(app->settings_idx == (uint8_t)SettingAlertThresh) {
                if(key == InputKeyLeft  && app->alert_threshold > 10)  app->alert_threshold -= 10;
                if(key == InputKeyRight && app->alert_threshold < 250) app->alert_threshold += 10;
            } else if(app->settings_idx == (uint8_t)SettingPathloss) {
                if(key == InputKeyLeft  && app->pathloss_gamma_x10 > 10) {
                    app->pathloss_gamma_x10--;
                    csight_send_pathloss_gamma(app);
                }
                if(key == InputKeyRight && app->pathloss_gamma_x10 < 50) {
                    app->pathloss_gamma_x10++;
                    csight_send_pathloss_gamma(app);
                }
            } else if(app->settings_idx == (uint8_t)SettingPreset) {
                // One-tap combos — Left = Quiet Room (catches subtle motion),
                // Right = Busy Room (filters incidental background movement)
                if(key == InputKeyLeft) {
                    app->sensitivity     = 8;
                    app->alert_threshold = 100;
                    csight_send_sensitivity(app);
                } else if(key == InputKeyRight) {
                    app->sensitivity     = 3;
                    app->alert_threshold = 200;
                    csight_send_sensitivity(app);
                }
            } else if(app->settings_idx == (uint8_t)SettingScheduleStart) {
                if(key == InputKeyLeft)  app->schedule_start_hour = (app->schedule_start_hour + 23) % 24;
                if(key == InputKeyRight) app->schedule_start_hour = (app->schedule_start_hour + 1) % 24;
            } else if(app->settings_idx == (uint8_t)SettingScheduleEnd) {
                if(key == InputKeyLeft)  app->schedule_end_hour = (app->schedule_end_hour + 23) % 24;
                if(key == InputKeyRight) app->schedule_end_hour = (app->schedule_end_hour + 1) % 24;
            }
            if(key == InputKeyBack) {
                csight_config_save(app);
                app->state = AppStateMainMenu;
            }
            break;

        // ── Flash ─────────────────────────────────────────────────────────────
        case AppStateFlash:
            if(app->flash_state == FlashStateConfirm) {
                if(key == InputKeyOk) {
                    csight_flash_start(app);
                } else if(key == InputKeyBack) {
                    app->state = AppStateMainMenu;
                }
            } else if(app->flash_state == FlashStateDone ||
                      app->flash_state == FlashStateError) {
                if(key == InputKeyOk || key == InputKeyBack) {
                    // Reinit UART for normal comms after flash
                    if(app->serial == NULL) {
                        csight_uart_init(app);
                        csight_send_handshake(app);
                        app->state      = AppStateConnecting;
                        app->boot_frame = 0;
                    } else {
                        app->state = AppStateMainMenu;
                    }
                }
            }
            break;

        // ── Mesh node position config ────────────────────────────────────────
        case AppStateMeshConfig: {
            int16_t* target = app->mesh_config_edit_y
                ? &app->mesh_node_y_cm[app->mesh_config_node_idx]
                : &app->mesh_node_x_cm[app->mesh_config_node_idx];

            if(key == InputKeyLeft && app->mesh_config_node_idx > 1) {
                app->mesh_config_node_idx--;
            } else if(key == InputKeyRight && app->mesh_config_node_idx < MESH_MAX_NODES - 1) {
                app->mesh_config_node_idx++;
            } else if(key == InputKeyUp) {
                *target += 10; // 10cm steps
                if(*target > 2000) *target = 2000; // cap at 20m — sane physical limit
                csight_send_node_positions(app);
            } else if(key == InputKeyDown) {
                *target -= 10;
                if(*target < -2000) *target = -2000;
                csight_send_node_positions(app);
            } else if(key == InputKeyOk) {
                app->mesh_config_edit_y = !app->mesh_config_edit_y;
            } else if(key == InputKeyBack) {
                csight_config_save(app);
                csight_send_node_positions(app); // make sure ESP32 has the final values
                app->state = AppStateSettings;
            }
            break;
        }

        // ── Web UI ────────────────────────────────────────────────────────────
        case AppStateWebUI:
            if(key == InputKeyOk || key == InputKeyBack) {
                if(app->web_ui_active) csight_send_webui_toggle(app);
                app->state = AppStateMainMenu;
            }
            break;

        // ── About ─────────────────────────────────────────────────────────────
        case AppStateAbout:
            if(key == InputKeyBack || key == InputKeyOk) {
                app->state = AppStateMainMenu;
            }
            break;

        default:
            break;
    }
}

// ─── Draw callback ────────────────────────────────────────────────────────────
static void draw_cb(Canvas* c, void* ctx) {
    CSIghtApp* app = (CSIghtApp*)ctx;

    switch(app->state) {
        case AppStateBooting:
            csight_draw_boot(c, app);
            break;
        case AppStatePowerWarning:
            csight_draw_power_warning(c, app);
            break;
        case AppStateConnecting:
            csight_draw_boot(c, app);
            break;
        case AppStateMainMenu:
            csight_draw_main_menu(c, app);
            break;
        case AppStateCompatCheck:
            csight_draw_compat(c, app);
            break;
        case AppStatePresetSelect:
            csight_draw_preset(c, app);
            break;
        case AppStatePinConfig:
            csight_draw_pin_config(c, app);
            break;
        case AppStateScanning:
            switch(app->display_mode) {
                case DisplayModeRadar:     csight_draw_radar(c, app);     break;
                case DisplayModeWaterfall: csight_draw_waterfall(c, app); break;
                case DisplayModeProximity: csight_draw_proximity(c, app); break;
                case DisplayModeVitals:    csight_draw_vitals(c, app);    break;
                case DisplayModeMesh:      csight_draw_mesh(c, app);      break;
                case DisplayModeHeatmap:   csight_draw_heatmap(c, app);   break;
                default:                   csight_draw_radar(c, app);     break;
            }
            break;
        case AppStateSettings:
            csight_draw_settings(c, app);
            break;
        case AppStateFlash:
            csight_draw_flash(c, app);
            break;
        case AppStateMeshConfig:
            csight_draw_mesh_config(c, app);
            break;
        case AppStateWebUI:
            csight_draw_webui(c, app);
            break;
        case AppStateAbout:
            csight_draw_about(c, app);
            break;
        default:
            break;
    }

    // "Node discovered" banner — overlays on top of whatever screen is showing
    if(app->node_found_pending) {
        char banner[24];
        snprintf(banner, sizeof(banner), "Node %d discovered!", app->node_found_id);
        int w = canvas_string_width(c, banner);
        int bx = (128 - w) / 2 - 3; // 128 = Flipper screen width
        canvas_draw_box(c, bx, 24, w + 6, 12);
        canvas_set_color(c, ColorWhite);
        canvas_draw_str(c, bx + 3, 33, banner);
        canvas_set_color(c, ColorBlack);
    }
}

// ─── Input callback ───────────────────────────────────────────────────────────
static void input_cb(InputEvent* event, void* ctx) {
    CSIghtApp* app = (CSIghtApp*)ctx;
    furi_message_queue_put(app->event_queue, event, 0);
}

// ─── Timer callback ───────────────────────────────────────────────────────────
static void timer_cb(void* ctx) {
    CSIghtApp* app = (CSIghtApp*)ctx;
    csight_tick(app);
    view_port_update(app->view_port);
}

// ─── Alloc / Free ─────────────────────────────────────────────────────────────
CSIghtApp* csight_app_alloc(void) {
    CSIghtApp* app = malloc(sizeof(CSIghtApp));
    memset(app, 0, sizeof(CSIghtApp));

    app->state        = AppStateBooting;
    app->sensitivity  = 5;
    app->display_mode = DisplayModeRadar;
    app->event_queue  = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->config_exists = csight_config_load(app);
    csight_log_init(app);

    app->gui       = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Prevent Flipper from sleeping while the app is open
    furi_hal_power_insomnia_enter();

    // UART init is deferred until after power warning is confirmed
    // to prevent crashing if ESP32 is not yet powered

    return app;
}

void csight_app_free(CSIghtApp* app) {
    furi_hal_power_insomnia_exit();
    // Only deinit UART if it was actually initialised
    if(app->serial != NULL) {
        csight_uart_deinit(app);
    }
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->event_queue);
    free(app);
}

// ─── Entry point ──────────────────────────────────────────────────────────────
int32_t csight_app(void* p) {
    UNUSED(p);
    CSIghtApp* app = csight_app_alloc();

    FuriTimer* timer = furi_timer_alloc(timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 30);

    InputEvent event;
    while(1) {
        if(furi_message_queue_get(app->event_queue, &event, 10) == FuriStatusOk) {
            // Only exit on long Back press from main menu
            if(event.key == InputKeyBack &&
               event.type == InputTypeLong &&
               app->state == AppStateMainMenu) break;
            handle_input(app, event.key, event.type);
        }
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);
    csight_app_free(app);
    return 0;
}
