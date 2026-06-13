#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>

// ─── Protocol ────────────────────────────────────────────────────────────────
#define PROTO_HANDSHAKE_REQ  0xAA
#define PROTO_HANDSHAKE_ACK  0xBB
#define PROTO_MOTION_EVT     0x01
#define PROTO_PROXIMITY_EVT  0x02
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

static const BoardPreset BOARD_PRESETS[] = {
    // ── Official Flipper expansion boards ─────────────────────────────────────
    { "FZ WiFi Dev Board",       1,  3,  1 },  // Flipper official ESP32-WROOM, UART0
    { "FZ WiFi Dev Board Pro",   1,  3,  1 },  // JustCallMeKoko, ESP32-WROOM-32
    { "FlipMods Mini WiFi",      1,  3,  1 },  // Sacred Labs, ESP32-WROOM-32UE
    { "FlipMods Combo",         15, 16,  1 },  // 3-in-1, GPS+CC1101, alt UART pins
    { "FEBERIS (BPM Circuits)",  1,  3,  1 },  // Triple module Marauder board

    // ── Marauder Double Barrel 5G ──────────────────────────────────────────────
    { "Marauder DblBarrel 5G",  15, 16,  1 },  // Momentum: set pins 15/16

    // ── Generic DevKit boards (Espressif official) ────────────────────────────
    { "ESP32 DevKit V1",         1,  3,  1 },  // Classic, UART0 TX=1 RX=3
    { "ESP32 DevKit V4",         1,  3,  1 },  // Same UART0 pinout
    { "ESP32-S3 DevKitC-1",     43, 44,  2 },  // USB-UART on 43/44
    { "ESP32-S3 DevKitM-1",     43, 44,  2 },
    { "ESP32-C3 DevKitC-02",    21, 20,  2 },  // UART0 on C3: TX=21 RX=20
    { "ESP32-C3 DevKitM-1",     21, 20,  2 },
    { "ESP32-C6 DevKitC-1",     16, 17,  2 },  // UART0 on C6
    { "ESP32-C6 DevKitM-1",     16, 17,  2 },
    { "ESP32-C61 DevKitC-1",    16, 17,  2 },  // C61 = C6 without 802.15.4

    // ── Seeed XIAO series ─────────────────────────────────────────────────────
    { "XIAO ESP32-C3",           21, 20,  2 },  // USB-C, tiny form factor
    { "XIAO ESP32-S3",           43, 44,  2 },  // camera variant also works
    { "XIAO ESP32-C6",           16, 17,  2 },

    // ── Adafruit ──────────────────────────────────────────────────────────────
    { "Adafruit ESP32 Feather",   1,  3,  1 },  // HUZZAH32, UART0
    { "Adafruit QT Py ESP32-S3", 43, 44,  2 },
    { "Adafruit QT Py ESP32-C3", 21, 20,  2 },

    // ── SparkFun ──────────────────────────────────────────────────────────────
    { "SparkFun ESP32 Thing",     1,  3,  1 },
    { "SparkFun ESP32-S2 Thing",  0,  0,  0 },  // S2 = no CSI
    { "SparkFun C6 Qwiic Pocket", 16, 17,  2 },

    // ── Lolin / WEMOS ─────────────────────────────────────────────────────────
    { "LOLIN D32",                1,  3,  1 },
    { "LOLIN D32 Pro",            1,  3,  1 },
    { "WEMOS D1 Mini32",          1,  3,  1 },
    { "LOLIN S3",                43, 44,  2 },
    { "LOLIN C3 Mini",           21, 20,  2 },

    // ── AI-Thinker ────────────────────────────────────────────────────────────
    { "AI-Thinker ESP32-CAM",     1,  3,  1 },  // limited GPIOs but works
    { "NodeMCU-32S",              1,  3,  1 },

    // ── M5Stack ───────────────────────────────────────────────────────────────
    { "M5Stack Core",             1,  3,  1 },
    { "M5Stack Core2",            1,  3,  1 },
    { "M5Stamp C3",              21, 20,  2 },
    { "M5Stamp S3",              43, 44,  2 },

    // ── Unexpected Maker ──────────────────────────────────────────────────────
    { "TinyS3",                  43, 44,  2 },
    { "FeatherS3",               43, 44,  2 },
    { "ProS3",                   43, 44,  2 },

    // ── Olimex ────────────────────────────────────────────────────────────────
    { "Olimex ESP32-EVB",         1,  3,  1 },
    { "Olimex ESP32-S3-DevKit",  43, 44,  2 },

    // ── Custom ────────────────────────────────────────────────────────────────
    { "Custom...",                0,  0,  0 },
};
#define BOARD_PRESET_COUNT  41
#define BOARD_PRESET_CUSTOM 40

// ─── Display modes ────────────────────────────────────────────────────────────
typedef enum {
    DisplayModeRadar     = 0,
    DisplayModeWaterfall = 1,
    DisplayModeProximity = 2,
} DisplayMode;

// ─── App state ────────────────────────────────────────────────────────────────
typedef enum {
    AppStateBooting,
    AppStateConnecting,
    AppStateCompatCheck,
    AppStatePresetSelect,
    AppStatePinConfig,
    AppStateScanning,
} AppState;

// ─── Radar geometry (shared between draw and app logic) ──────────────────────
#define RADAR_CX    42
#define RADAR_CY    32
#define RADAR_R     28

// ─── Trig lookup (64 steps = 360 degrees, values * 59 for integer math) ──────
// Defined here so both csight_draw.c and csight_app.c can use without extern
static const int8_t SIN64[64] = {
     0,  9, 18, 27, 35, 42, 48, 53,
    56, 58, 59, 58, 56, 53, 48, 42,
    35, 27, 18,  9,  0, -9,-18,-27,
   -35,-42,-48,-53,-56,-58,-59,-58,
   -56,-53,-48,-42,-35,-27,-18, -9,
     0,  9, 18, 27, 35, 42, 48, 53,
    56, 58, 59, 58, 56, 53, 48, 42,
    35, 27, 18,  9,  0, -9,-18,-27,
};
static const int8_t COS64[64] = {
    59, 58, 56, 53, 48, 42, 35, 27,
    18,  9,  0, -9,-18,-27,-35,-42,
   -48,-53,-56,-58,-59,-58,-56,-53,
   -48,-42,-35,-27,-18, -9,  0,  9,
    18, 27, 35, 42, 48, 53, 56, 58,
    59, 58, 56, 53, 48, 42, 35, 27,
    18,  9,  0, -9,-18,-27,-35,-42,
   -48,-53,-56,-58,-59,-58,-56,-53,
};

// ─── Radar blip ───────────────────────────────────────────────────────────────
#define MAX_BLIPS 8
typedef struct {
    int8_t  x, y;       // canvas coords relative to radar center
    uint8_t age;        // 0 = fresh, 255 = gone
    uint8_t intensity;
} RadarBlip;

// ─── Waterfall column ─────────────────────────────────────────────────────────
#define WATERFALL_WIDTH  80
#define WATERFALL_HEIGHT 40
#define WATERFALL_COLS   WATERFALL_WIDTH
typedef struct {
    uint8_t cols[WATERFALL_COLS][WATERFALL_HEIGHT / 4]; // 2bpp packed
} WaterfallBuf;

// ─── Main app struct ──────────────────────────────────────────────────────────
typedef struct {
    // GUI
    Gui*             gui;
    ViewPort*        view_port;
    FuriMessageQueue* event_queue;

    // UART
    FuriHalSerialHandle* serial;
    FuriStreamBuffer*    rx_stream;
    FuriThread*          uart_thread;

    // State
    AppState    state;
    DisplayMode display_mode;
    uint8_t     sensitivity;      // 0-10
    bool        target_acquired;  // flash flag
    uint32_t    target_ts;        // tick when target was acquired

    // Chip info (from handshake)
    char    chip_name[16];
    uint8_t csi_support;          // 0=none 1=limited 2=full
    uint8_t fw_major;
    uint8_t fw_minor;

    // Board config
    uint8_t preset_idx;
    uint8_t tx_pin;
    uint8_t rx_pin;

    // Radar
    RadarBlip blips[MAX_BLIPS];
    uint8_t   sweep_angle;        // 0-63 (maps to 0-360)
    uint8_t   motion_intensity;
    uint8_t   proximity;          // 0-100

    // Waterfall
    WaterfallBuf waterfall;
    uint8_t      wf_write_col;

    // Animation
    uint8_t  boot_frame;
    uint32_t last_tick;
} CSIghtApp;

// ─── Scene IDs ────────────────────────────────────────────────────────────────
typedef enum {
    SceneMain,
    ScenePresetSelect,
    ScenePinConfig,
    SceneCompatCheck,
    SceneScanning,
    SceneCount,
} CSIghtScene;

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
void csight_draw_compat(Canvas* c, CSIghtApp* app);
void csight_draw_preset(Canvas* c, CSIghtApp* app);
void csight_draw_radar(Canvas* c, CSIghtApp* app);
void csight_draw_waterfall(Canvas* c, CSIghtApp* app);
void csight_draw_proximity(Canvas* c, CSIghtApp* app);

void csight_config_load(CSIghtApp* app);
void csight_config_save(CSIghtApp* app);

void csight_add_blip(CSIghtApp* app, uint8_t intensity, uint8_t proximity);
void csight_tick(CSIghtApp* app);
