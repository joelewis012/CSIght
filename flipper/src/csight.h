#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <storage/storage.h>

// ─── Protocol ────────────────────────────────────────────────────────────────
#define PROTO_HANDSHAKE_REQ  0xAA
#define PROTO_HANDSHAKE_ACK  0xBB
#define PROTO_MOTION_EVT     0x01
#define PROTO_WATERFALL_EVT  0x03
#define PROTO_SENSITIVITY    0x10
#define PROTO_MODE_SET       0x11
#define PROTO_CALIBRATE      0x12
#define PROTO_CHANNEL_SET    0x13  // ESP32→Flipper: here's the active channel
#define PROTO_CHANNEL_AUTO   0x14  // Flipper→ESP32: run auto survey
#define PROTO_WEBUI_ON       0x20
#define PROTO_WEBUI_OFF      0x21
#define PROTO_VITALS_EVT     0x04
#define PROTO_MESH_EVT       0x05
#define PROTO_NODE_FOUND     0x06
#define PROTO_FORGET_NODES   0x15
#define PROTO_NODE_POSITIONS 0x16
#define PROTO_PATHLOSS_GAMMA 0x17

#define MESH_MAX_NODES 4  // node 0 = primary, 1-3 = secondaries

// ─── UART ────────────────────────────────────────────────────────────────────
#define CSIGHT_UART_CH       FuriHalSerialIdUsart
#define CSIGHT_UART_BAUD     115200
#define CSIGHT_RX_BUF        256

// ─── Board presets ────────────────────────────────────────────────────────────
// tx_pin / rx_pin = ESP32-side UART pins (not Flipper pins)
// Flipper side always uses GPIO13(TX) and GPIO14(RX) by default
typedef struct {
    const char* name;       // shown on preset select screen
    uint8_t     tx_pin;     // ESP32 TX pin
    uint8_t     rx_pin;     // ESP32 RX pin
    uint8_t     csi_tier;   // 0=none 1=limited 2=full (shown as hint)
} BoardPreset;

// Defined in csight_app.c — extern here to avoid duplicate symbol across TUs
extern const BoardPreset BOARD_PRESETS[];
extern const int          BOARD_PRESET_COUNT;
#define BOARD_PRESET_CUSTOM (BOARD_PRESET_COUNT - 1)

// ─── Display modes ────────────────────────────────────────────────────────────
typedef enum {
    DisplayModeRadar     = 0,
    DisplayModeWaterfall = 1,
    DisplayModeProximity = 2,
    DisplayModeVitals    = 3,
    DisplayModeMesh      = 4,
    DisplayModeHeatmap   = 5,
} DisplayMode;
#define DISPLAY_MODE_COUNT 6

// ─── App state ────────────────────────────────────────────────────────────────
typedef enum {
    AppStateBooting,
    AppStatePowerWarning,
    AppStateMainMenu,
    AppStatePresetSelect,
    AppStatePinConfig,
    AppStateConnecting,
    AppStateCompatCheck,
    AppStateScanning,
    AppStateWebUI,
    AppStateSettings,
    AppStateAbout,
    AppStateFlash,
    AppStateMeshConfig,
} AppState;

// ─── Flash sub-states ─────────────────────────────────────────────────────────
typedef enum {
    FlashStateConfirm,     // "Put ESP32 into bootloader mode, then press OK"
    FlashStateFlashing,    // progress bar
    FlashStateDone,        // success
    FlashStateError,       // error message
} FlashSubState;

// ─── Main menu items ──────────────────────────────────────────────────────────
#define MAIN_MENU_COUNT 5
typedef enum {
    MenuItemScan     = 0,
    MenuItemWebUI    = 1,
    MenuItemFlash    = 2,
    MenuItemSettings = 3,
    MenuItemAbout    = 4,
} MainMenuItem;

// ─── Settings menu items ──────────────────────────────────────────────────────
#define SETTINGS_COUNT 13
typedef enum {
    SettingSensitivity  = 0,
    SettingChannel      = 1,
    SettingAlertThresh  = 2,
    SettingRescanCh     = 3,   // triggers auto channel survey
    SettingMeshConfig   = 4,   // node position setup for multi-node mode
    SettingForgetNodes  = 5,   // clears paired secondary nodes on the primary
    SettingSdLogging    = 6,   // toggle SD card event logging
    SettingPathloss     = 7,   // trilateration path-loss exponent (mesh mode)
    SettingTestAlert    = 8,   // fires the alert sequence on demand, no real trigger needed
    SettingPreset       = 9,   // one-tap sensitivity+threshold combo
    SettingScheduleStart = 10, // auto-arm schedule start hour
    SettingScheduleEnd   = 11, // auto-arm schedule end hour
    SettingChangeBoard  = 12,
} SettingItem;

// ─── Radar geometry ───────────────────────────────────────────────────────────
#define RADAR_CX    42
#define RADAR_CY    32
#define RADAR_R     28
#define TARGET_FLASH_MS  800  // how long the "TARGET ACQUIRED" flash lasts

// ─── Trig lookup (64 steps = 360°, scaled by 59) ─────────────────────────────
// extern — defined once in csight_draw.c, used by csight_app.c too
extern const int8_t SIN64[64];
extern const int8_t COS64[64];

// ─── Radar blip ───────────────────────────────────────────────────────────────
#define MAX_BLIPS 8
typedef struct {
    int8_t  x, y;
    uint8_t age;
    uint8_t intensity;
} RadarBlip;

// ─── Waterfall ────────────────────────────────────────────────────────────────
#define WATERFALL_COLS   80
#define WATERFALL_HEIGHT 40
typedef struct {
    uint8_t cols[WATERFALL_COLS][WATERFALL_HEIGHT / 4];
} WaterfallBuf;

// ─── Main app struct ──────────────────────────────────────────────────────────
#define HEATMAP_GRID 8
typedef struct {
    // GUI
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* event_queue;
    NotificationApp*  notifications;

    // UART
    FuriHalSerialHandle* serial;
    FuriStreamBuffer*    rx_stream;
    FuriThread*          uart_thread;

    // State
    AppState    state;
    DisplayMode display_mode;
    uint8_t     sensitivity;
    bool        target_acquired;
    uint32_t    target_ts;

    // Chip info (from handshake)
    char    chip_name[16];
    uint8_t csi_support;
    uint8_t fw_major;
    uint8_t fw_minor;
    bool    web_ui_active;  // ESP32 has web UI running

    // Board config
    uint8_t preset_idx;
    uint8_t tx_pin;
    uint8_t rx_pin;
    bool    config_exists;  // false = first run, show setup flow

    // Radar
    RadarBlip blips[MAX_BLIPS];
    uint8_t   sweep_angle;
    uint8_t   motion_intensity;
    uint8_t   proximity;

    // Waterfall
    WaterfallBuf waterfall;
    uint8_t      wf_write_col;

    // Vitals (mode 3)
    uint8_t breathing_bpm;   // 0 = still measuring
    uint8_t heart_bpm;       // 0 = no valid reading
    bool    vitals_valid;    // true once first 30s window completes

    // Multi-node mesh (v2.0 preview)
    // Node 0 = primary (fixed at origin), nodes 1-3 = secondaries.
    // Positions are entered by the user in centimetres and saved to config.
    int16_t mesh_node_x_cm[MESH_MAX_NODES];
    int16_t mesh_node_y_cm[MESH_MAX_NODES];
    bool    mesh_node_active[MESH_MAX_NODES];
    uint32_t mesh_node_last_seen_tick[MESH_MAX_NODES]; // furi_get_tick() when last reported active

    // Motion heatmap (v3.3) — 8x8 grid over the same bounding box as the
    // mesh map. Each cell decays slowly so the display shows recent activity
    // patterns, not just a single snapshot.
    uint8_t heatmap[HEATMAP_GRID][HEATMAP_GRID]; // 0-255 heat per cell
    uint32_t heatmap_last_decay_tick;

    // Scheduled auto-arm (v3.3) — if start_hour == end_hour, schedule is off.
    // While enabled, this overrides manual long-press arm/disarm entirely so
    // the two controls can't fight each other.
    uint8_t schedule_start_hour; // 0-23
    uint8_t schedule_end_hour;   // 0-23
    uint8_t mesh_node_intensity[MESH_MAX_NODES];
    uint8_t mesh_node_proximity[MESH_MAX_NODES];
    bool    mesh_has_estimate;
    int16_t mesh_est_x_cm;
    int16_t mesh_est_y_cm;
    uint8_t mesh_config_node_idx;  // which node (1-3) is being edited in config screen
    bool    mesh_config_edit_y;    // false = editing X, true = editing Y

    // Node discovery banner — brief on-screen "Node X discovered!" overlay
    bool     node_found_pending;
    uint8_t  node_found_id;
    uint32_t node_found_ts;

    // SD card event logging (v2.2)
    bool log_enabled;   // toggled in Settings, saved to config

    // Trilateration tuning (v3.1)
    uint8_t pathloss_gamma_x10;  // gamma * 10, e.g. 20 = 2.0. Range 10-50.

    // Flash
    FlashSubState    flash_state;
    volatile uint8_t flash_progress;  // 0-100
    char             flash_status[32];
    char             flash_error[64];

    // Channel selection
    uint8_t wifi_channel;       // 1-13, default 6

    // Alert / tripwire
    bool    alert_armed;        // tripwire mode active
    uint8_t alert_threshold;    // intensity that fires the alarm (0-255)
    bool    alert_triggered;    // currently alarming
    uint32_t alert_ts;          // tick when alert fired

    // Session stats
    uint32_t session_start_tick;
    uint32_t motion_count;

    // Menu navigation
    uint8_t menu_idx;
    uint8_t settings_idx;
    uint8_t boot_frame;
} CSIghtApp;

// ─── Function declarations ────────────────────────────────────────────────────
CSIghtApp* csight_app_alloc(void);
void       csight_app_free(CSIghtApp* app);
int32_t    csight_app(void* p);

void csight_uart_init(CSIghtApp* app);
void csight_uart_deinit(CSIghtApp* app);
void csight_uart_send(CSIghtApp* app, const uint8_t* data, size_t len);
void csight_send_handshake(CSIghtApp* app);
void csight_send_sensitivity(CSIghtApp* app);
void csight_send_mode(CSIghtApp* app);
void csight_send_calibrate(CSIghtApp* app);
void csight_send_channel(CSIghtApp* app);
void csight_send_channel_auto(CSIghtApp* app);
void csight_send_forget_nodes(CSIghtApp* app);
void csight_send_node_positions(CSIghtApp* app);
void csight_send_pathloss_gamma(CSIghtApp* app);

void csight_draw_boot(Canvas* c, CSIghtApp* app);
void csight_draw_power_warning(Canvas* c, CSIghtApp* app);
void csight_draw_main_menu(Canvas* c, CSIghtApp* app);
void csight_draw_compat(Canvas* c, CSIghtApp* app);
void csight_draw_preset(Canvas* c, CSIghtApp* app);
void csight_draw_pin_config(Canvas* c, CSIghtApp* app);
void csight_draw_radar(Canvas* c, CSIghtApp* app);
void csight_draw_waterfall(Canvas* c, CSIghtApp* app);
void csight_draw_proximity(Canvas* c, CSIghtApp* app);
void csight_draw_vitals(Canvas* c, CSIghtApp* app);
void csight_draw_mesh(Canvas* c, CSIghtApp* app);
void csight_draw_heatmap(Canvas* c, CSIghtApp* app);

// Adds heat at a real-world (x_cm, y_cm) position — maps it onto the same
// grid used by csight_draw_heatmap, based on the current node bounding box.
void csight_heatmap_add(CSIghtApp* app, int16_t x_cm, int16_t y_cm);
void csight_draw_mesh_config(Canvas* c, CSIghtApp* app);
void csight_draw_flash(Canvas* c, CSIghtApp* app);
void csight_draw_webui(Canvas* c, CSIghtApp* app);
void csight_draw_settings(Canvas* c, CSIghtApp* app);
void csight_draw_about(Canvas* c, CSIghtApp* app);

void csight_flash_start(CSIghtApp* app);
bool csight_flash_files_exist(CSIghtApp* app);

bool csight_config_load(CSIghtApp* app);  // returns true if config existed
void csight_config_save(CSIghtApp* app);

void csight_add_blip(CSIghtApp* app, uint8_t intensity, uint8_t proximity);
void csight_tick(CSIghtApp* app);
void csight_send_webui_toggle(CSIghtApp* app);
