#include "csight.h"

#define TAG         "CSIght"
#define CONFIG_PATH EXT_PATH("apps_data/csight/config.bin")

// ─── Board presets — defined here, extern'd in header ─────────────────────────
const BoardPreset BOARD_PRESETS[] = {
    // ── Official Flipper expansion boards ─────────────────────────────────────
    { "FZ WiFi Dev Board",        1,  3,  1 },
    { "FZ WiFi Dev Board Pro",    1,  3,  1 },
    { "FlipMods Mini WiFi",       1,  3,  1 },
    { "FlipMods Combo",          15, 16,  1 },
    { "FEBERIS (BPM Circuits)",   1,  3,  1 },
    { "Marauder DblBarrel 5G",   15, 16,  1 },

    // ── Espressif DevKits ─────────────────────────────────────────────────────
    { "ESP32 DevKit V1",          1,  3,  1 },
    { "ESP32 DevKit V4",          1,  3,  1 },
    { "ESP32-S3 DevKitC-1",      43, 44,  2 },
    { "ESP32-S3 DevKitM-1",      43, 44,  2 },
    { "ESP32-C3 DevKitC-02",     21, 20,  2 },
    { "ESP32-C3 DevKitM-1",      21, 20,  2 },
    { "ESP32-C6 DevKitC-1",      16, 17,  2 },
    { "ESP32-C6 DevKitM-1",      16, 17,  2 },
    { "ESP32-C61 DevKitC-1",     16, 17,  2 },

    // ── Seeed XIAO ────────────────────────────────────────────────────────────
    { "XIAO ESP32-C3",            21, 20,  2 },
    { "XIAO ESP32-S3",            43, 44,  2 },
    { "XIAO ESP32-C6",            16, 17,  2 },

    // ── Popular mini/cheap boards ─────────────────────────────────────────────
    { "C3 Super Mini",            21, 20,  2 },
    { "C3 Mini (generic)",        21, 20,  2 },
    { "ESP32-S3 Zero",            43, 44,  2 },
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
    uint8_t magic;        // 0xC5 = config is valid, first-run detection
    uint8_t preset_idx;
    uint8_t tx_pin;
    uint8_t rx_pin;
    uint8_t sensitivity;
    uint8_t display_mode;
    uint8_t _pad[2];
} CSIghtConfig;

#define CONFIG_MAGIC 0xC5

// ─── Config I/O ──────────────────────────────────────────────────────────────
void csight_config_save(CSIghtApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data/csight"));

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        CSIghtConfig cfg = {
            .magic        = CONFIG_MAGIC,
            .preset_idx   = app->preset_idx,
            .tx_pin       = app->tx_pin,
            .rx_pin       = app->rx_pin,
            .sensitivity  = app->sensitivity,
            .display_mode = (uint8_t)app->display_mode,
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
            app->preset_idx   = cfg.preset_idx < (uint8_t)BOARD_PRESET_COUNT ? cfg.preset_idx : 0;
            app->tx_pin       = cfg.tx_pin  ? cfg.tx_pin  : 13;
            app->rx_pin       = cfg.rx_pin  ? cfg.rx_pin  : 14;
            app->sensitivity  = cfg.sensitivity <= 10 ? cfg.sensitivity : 5;
            app->display_mode = cfg.display_mode <= (uint8_t)DisplayModeProximity ?
                                (DisplayMode)cfg.display_mode : DisplayModeRadar;
            loaded = true;
        }
    }

    if(!loaded) {
        // First run defaults
        app->preset_idx   = 0;
        app->tx_pin       = 13;
        app->rx_pin       = 14;
        app->sensitivity  = 5;
        app->display_mode = DisplayModeRadar;
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

// ─── Tick ─────────────────────────────────────────────────────────────────────
void csight_tick(CSIghtApp* app) {
    app->sweep_angle = (app->sweep_angle + 1) & 63;

    for(int i = 0; i < MAX_BLIPS; i++) {
        if(app->blips[i].age > 0) app->blips[i].age -= 2;
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
            // Timeout — go to main menu, user can retry
            app->state      = AppStateMainMenu;
            app->boot_frame = 0;
        }
    }

    if(app->state == AppStateWebUI) {
        app->boot_frame++;
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
                        app->state = AppStateScanning;
                        break;
                    case MenuItemWebUI:
                        csight_send_webui_toggle(app);
                        if(app->web_ui_active) {
                            app->state = AppStateWebUI;
                        }
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
            if(key == InputKeyOk) {
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
                app->display_mode = (DisplayMode)((app->display_mode + 2) % 3);
                csight_send_mode(app);
            } else if(key == InputKeyRight) {
                app->display_mode = (DisplayMode)((app->display_mode + 1) % 3);
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
            if(key == InputKeyBack) {
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
                if(app->settings_idx == 0) {
                    // Sensitivity — adjust with left/right
                } else if(app->settings_idx == 1) {
                    // Change board
                    app->state = AppStatePresetSelect;
                }
            }
            // Adjust sensitivity with left/right when on that item
            if(app->settings_idx == 0) {
                if(key == InputKeyLeft && app->sensitivity > 0) {
                    app->sensitivity--;
                    csight_send_sensitivity(app);
                } else if(key == InputKeyRight && app->sensitivity < 10) {
                    app->sensitivity++;
                    csight_send_sensitivity(app);
                }
            }
            if(key == InputKeyBack) {
                app->state = AppStateMainMenu;
            }
            break;

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
        case AppStatePinConfig:
            csight_draw_preset(c, app);
            break;
        case AppStateScanning:
            switch(app->display_mode) {
                case DisplayModeRadar:     csight_draw_radar(c, app);     break;
                case DisplayModeWaterfall: csight_draw_waterfall(c, app); break;
                case DisplayModeProximity: csight_draw_proximity(c, app); break;
                default:                   csight_draw_radar(c, app);     break;
            }
            break;
        case AppStateSettings:
            csight_draw_settings(c, app);
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

    app->gui       = furi_record_open(RECORD_GUI);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_cb, app);
    view_port_input_callback_set(app->view_port, input_cb, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // UART init is deferred until after power warning is confirmed
    // to prevent crashing if ESP32 is not yet powered

    return app;
}

void csight_app_free(CSIghtApp* app) {
    // Only deinit UART if it was actually initialised
    if(app->serial != NULL) {
        csight_uart_deinit(app);
    }
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
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
            if(event.key == InputKeyBack && event.type == InputTypeShort) break;
            handle_input(app, event.key, event.type);
        }
    }

    furi_timer_stop(timer);
    furi_timer_free(timer);
    csight_app_free(app);
    return 0;
}
