#pragma once
#include <stdint.h>
#include <stdbool.h>

// ─── Web UI modes ─────────────────────────────────────────────────────────────
typedef enum {
    WEB_UI_OFF = 0,   // Flipper mode — web server not running
    WEB_UI_ON  = 1,   // Standalone mode — web server active
} WebUIMode;

// Start web UI — creates AP, starts HTTP + WebSocket server
void web_ui_start(void);

// Stop web UI — tears down server and AP
void web_ui_stop(void);

// Push motion event to all connected WebSocket clients
void web_ui_send_motion(uint8_t intensity, uint8_t proximity);

// Push waterfall data to all connected WebSocket clients
void web_ui_send_waterfall(uint8_t *bins, uint8_t count);
void web_ui_send_vitals(uint8_t breathing_bpm, uint8_t heart_bpm);
void web_ui_send_mesh(const int16_t* positions_x, const int16_t* positions_y,
                       uint8_t has_estimate, int16_t est_x, int16_t est_y,
                       const uint8_t* node_ids, const uint8_t* intensities,
                       const uint8_t* proximities, uint8_t node_count);
void web_ui_send_node_found(uint8_t node_id);

// Get current mode
WebUIMode web_ui_get_mode(void);

// AP credentials — connect to this to reach the web UI
#define CSIGHT_AP_SSID     "CSIght"
#define CSIGHT_AP_PASS     "csight123"
#define CSIGHT_AP_CHANNEL  6
#define CSIGHT_AP_MAX_CONN 4
#define CSIGHT_HTTP_PORT   80
