#pragma once
#include <stdint.h>

// Callback types
typedef void (*csi_motion_cb_t)(uint8_t intensity, uint8_t proximity);
typedef void (*csi_waterfall_cb_t)(uint8_t *bins, uint8_t count);
typedef void (*csi_vitals_cb_t)(uint8_t breathing_bpm, uint8_t heart_bpm);

// Init — pass in your motion and waterfall callbacks
void csi_handler_init(csi_motion_cb_t motion_cb, csi_waterfall_cb_t waterfall_cb);

// Register vitals callback (called every 30s when in vitals mode)
void csi_set_vitals_cb(csi_vitals_cb_t cb);

// Called from UART command handler
void csi_set_sensitivity(uint8_t level);   // 0-10
void csi_set_mode(uint8_t mode);           // 0=motion 1=waterfall 2=proximity 3=vitals
void csi_reset_baseline(void);             // force recalibration without changing mode
