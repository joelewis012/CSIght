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
} DisplayMode;

// ─── App state ────────────────────────────────────────────────────────────────
typedef enum {
    AppStateBooting,
    AppStatePowerWarning,
    AppStateConnecting,
    AppStateCompatCheck,
    AppStatePresetSelect,
    AppStatePinConfig,
    AppStateScanning,
} AppState;

// ─── Radar geometry ───────────────────────────────────────────────────────────
#define RADAR_CX    42
#define RADAR_CY    32
#define RADAR_R     28

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
typedef struct {
    // GUI
    Gui*              gui;
    ViewPort*         view_port;
    FuriMessageQueue* event_queue;

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

    // Board config
    uint8_t preset_idx;
    uint8_t tx_pin;
    uint8_t rx_pin;

    // Radar
    RadarBlip blips[MAX_BLIPS];
    uint8_t   sweep_angle;
    uint8_t   motion_intensity;
    uint8_t   proximity;

    // Waterfall
    WaterfallBuf waterfall;
    uint8_t      wf_write_col;

    // Animation
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

void csight_draw_boot(Canvas* c, CSIghtApp* app);
void csight_draw_power_warning(Canvas* c, CSIghtApp* app);
void csight_draw_compat(Canvas* c, CSIghtApp* app);
void csight_draw_preset(Canvas* c, CSIghtApp* app);
void csight_draw_radar(Canvas* c, CSIghtApp* app);
void csight_draw_waterfall(Canvas* c, CSIghtApp* app);
void csight_draw_proximity(Canvas* c, CSIghtApp* app);

void csight_config_load(CSIghtApp* app);
void csight_config_save(CSIghtApp* app);

void csight_add_blip(CSIghtApp* app, uint8_t intensity, uint8_t proximity);
void csight_tick(CSIghtApp* app);
