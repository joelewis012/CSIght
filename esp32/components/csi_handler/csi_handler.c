#include "csi_handler.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"

static const char *TAG = "csi_handler";

// ─── State ────────────────────────────────────────────────────────────────────
static csi_motion_cb_t    s_motion_cb    = NULL;
static csi_waterfall_cb_t s_waterfall_cb = NULL;
static uint8_t            s_sensitivity  = 5;
static uint8_t            s_mode         = 0;

// ─── Baseline state ───────────────────────────────────────────────────────────
#define CSI_BUF_LEN      64
#define BASELINE_FRAMES  30

static float s_baseline[CSI_BUF_LEN];
static int   s_frame_count   = 0;
static bool  s_baseline_done = false;

// ─── Helpers ──────────────────────────────────────────────────────────────────
static float sensitivity_to_threshold(uint8_t sens) {
    // sens 0 → threshold 8.0, sens 10 → threshold 1.0
    return 8.0f - ((float)sens * 0.7f);
}

static float csi_amplitude(int8_t real, int8_t imag) {
    return sqrtf((float)(real * real) + (float)(imag * imag));
}

static uint8_t float_to_byte(float val, float max) {
    if(val < 0.0f) val = 0.0f;
    if(val > max)  val = max;
    return (uint8_t)((val / max) * 255.0f);
}

static uint8_t delta_to_proximity(float avg_delta) {
    // Strong delta = close target = high proximity value
    if(avg_delta < 0.0f)  avg_delta = 0.0f;
    if(avg_delta > 15.0f) avg_delta = 15.0f;
    return (uint8_t)((avg_delta / 15.0f) * 100.0f);
}

// ─── CSI callback ─────────────────────────────────────────────────────────────
static void wifi_csi_cb(void *ctx, wifi_csi_info_t *info) {
    (void)ctx;
    if(!info || !info->buf) return;

    int8_t *raw   = info->buf;
    int     n_sub = info->len / 2;
    if(n_sub > CSI_BUF_LEN) n_sub = CSI_BUF_LEN;
    if(n_sub <= 0) return;

    // Build amplitude array for this frame
    float amp[CSI_BUF_LEN];
    memset(amp, 0, sizeof(amp));
    for(int i = 0; i < n_sub; i++) {
        amp[i] = csi_amplitude(raw[i * 2], raw[i * 2 + 1]);
    }

    // Phase 1: accumulate baseline
    if(!s_baseline_done) {
        for(int i = 0; i < n_sub; i++) s_baseline[i] += amp[i];
        s_frame_count++;
        if(s_frame_count >= BASELINE_FRAMES) {
            for(int i = 0; i < n_sub; i++) s_baseline[i] /= (float)BASELINE_FRAMES;
            s_baseline_done = true;
            ESP_LOGI(TAG, "Baseline ready, %d subcarriers", n_sub);
        }
        return;
    }

    // Phase 2: compute per-subcarrier delta from baseline
    float wf_bins[CSI_BUF_LEN];
    float total_delta = 0.0f;
    for(int i = 0; i < n_sub; i++) {
        float d = fabsf(amp[i] - s_baseline[i]);
        wf_bins[i]  = d;
        total_delta += d;
    }
    float avg_delta = total_delta / (float)n_sub;

    // Slowly adapt baseline to environment changes
    const float alpha = 0.02f;
    for(int i = 0; i < n_sub; i++) {
        s_baseline[i] = s_baseline[i] * (1.0f - alpha) + amp[i] * alpha;
    }

    float threshold = sensitivity_to_threshold(s_sensitivity);

    switch(s_mode) {
        case 0: // motion
        case 2: // proximity (same callback, different use of proximity field)
        {
            if(avg_delta > threshold) {
                uint8_t intensity = float_to_byte(avg_delta, 20.0f);
                uint8_t proximity = delta_to_proximity(avg_delta);
                if(s_motion_cb) s_motion_cb(intensity, proximity);
            }
            break;
        }

        case 1: // waterfall
        {
            // Compress n_sub bins → 32 output bins
            #define WF_COLS 32
            uint8_t out[WF_COLS];
            memset(out, 0, sizeof(out));
            int bins_per_col = n_sub / WF_COLS;
            if(bins_per_col < 1) bins_per_col = 1;
            for(int c = 0; c < WF_COLS; c++) {
                float sum = 0.0f;
                for(int b = 0; b < bins_per_col; b++) {
                    int idx = c * bins_per_col + b;
                    if(idx < n_sub) sum += wf_bins[idx];
                }
                out[c] = float_to_byte(sum / (float)bins_per_col, 20.0f);
            }
            if(s_waterfall_cb) s_waterfall_cb(out, WF_COLS);
            break;
        }

        default:
            break;
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────
void csi_handler_init(csi_motion_cb_t motion_cb, csi_waterfall_cb_t waterfall_cb) {
    s_motion_cb    = motion_cb;
    s_waterfall_cb = waterfall_cb;

    memset(s_baseline, 0, sizeof(s_baseline));
    s_frame_count   = 0;
    s_baseline_done = false;

    wifi_csi_config_t csi_cfg = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = false,
        .manu_scale        = false,
#if CONFIG_IDF_TARGET_ESP32
        // Original ESP32 uses legacy CSI config fields
        .rx_ant_num        = 1,
        .rx_ant_set        = 0,
#endif
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

    ESP_LOGI(TAG, "CSI handler ready, sensitivity=%d mode=%d", s_sensitivity, s_mode);
}

void csi_set_sensitivity(uint8_t level) {
    if(level > 10) level = 10;
    s_sensitivity = level;
    ESP_LOGI(TAG, "Sensitivity -> %d", level);
}

void csi_set_mode(uint8_t mode) {
    if(mode > 2) mode = 0;
    s_mode          = mode;
    s_baseline_done = false;
    s_frame_count   = 0;
    memset(s_baseline, 0, sizeof(s_baseline));
    ESP_LOGI(TAG, "Mode -> %d, recalibrating...", mode);
}
