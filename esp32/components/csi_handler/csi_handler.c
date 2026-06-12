#include "csi_handler.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"

static const char *TAG = "csi_handler";

// ─── State ───────────────────────────────────────────────────────────────────
static csi_motion_cb_t   s_motion_cb    = NULL;
static csi_waterfall_cb_t s_waterfall_cb = NULL;
static uint8_t            s_sensitivity  = 5;   // 0-10
static uint8_t            s_mode         = 0;   // 0=motion 1=waterfall 2=proximity

// ─── CSI baseline / detection state ──────────────────────────────────────────
#define CSI_BUF_LEN      64    // subcarriers we track
#define BASELINE_FRAMES  30    // frames to build baseline on boot

static float   s_baseline[CSI_BUF_LEN];
static float   s_prev[CSI_BUF_LEN];
static int     s_frame_count   = 0;
static bool    s_baseline_done = false;

// Waterfall history: 32 columns, each = one amplitude snapshot
#define WATERFALL_COLS   32
static uint8_t s_waterfall[WATERFALL_COLS];
static int     s_wf_idx = 0;

// ─── Helpers ─────────────────────────────────────────────────────────────────

// Map sensitivity 0-10 → threshold (higher sens = lower threshold)
static float sensitivity_to_threshold(uint8_t sens) {
    // Range: sens=0 → 8.0, sens=10 → 1.0
    return 8.0f - (sens * 0.7f);
}

// Compute amplitude from CSI int8 (real, imag pairs)
static float csi_amplitude(int8_t real, int8_t imag) {
    return sqrtf((float)(real * real) + (float)(imag * imag));
}

// Map float delta to 0-255 byte for waterfall
static uint8_t delta_to_byte(float delta) {
    float clamped = delta < 0.0f ? 0.0f : (delta > 20.0f ? 20.0f : delta);
    return (uint8_t)((clamped / 20.0f) * 255.0f);
}

// Map float delta 0-N to proximity 0-100 (closer = higher)
static uint8_t delta_to_proximity(float avg_delta) {
    // Very rough: strong delta = close presence
    float clamped = avg_delta < 0.0f ? 0.0f : (avg_delta > 15.0f ? 15.0f : avg_delta);
    return (uint8_t)((clamped / 15.0f) * 100.0f);
}

// ─── CSI callback ─────────────────────────────────────────────────────────────
static void wifi_csi_cb(void *ctx, wifi_csi_info_t *info) {
    if (!info || !info->buf) return;

    int8_t *raw    = info->buf;
    int     len    = info->len;           // bytes = 2 * subcarriers
    int     n_sub  = len / 2;
    if (n_sub > CSI_BUF_LEN) n_sub = CSI_BUF_LEN;

    // Build amplitude array for this frame
    float amp[CSI_BUF_LEN] = {0};
    for (int i = 0; i < n_sub; i++) {
        amp[i] = csi_amplitude(raw[i * 2], raw[i * 2 + 1]);
    }

    // ── Phase 1: build baseline ───────────────────────────────────────────────
    if (!s_baseline_done) {
        for (int i = 0; i < n_sub; i++) {
            s_baseline[i] += amp[i];
        }
        s_frame_count++;
        if (s_frame_count >= BASELINE_FRAMES) {
            for (int i = 0; i < n_sub; i++) {
                s_baseline[i] /= BASELINE_FRAMES;
            }
            s_baseline_done = true;
            ESP_LOGI(TAG, "Baseline established over %d subcarriers", n_sub);
        }
        memcpy(s_prev, amp, sizeof(float) * n_sub);
        return;
    }

    // ── Phase 2: compute delta from baseline ─────────────────────────────────
    float total_delta = 0.0f;
    float wf_bins[CSI_BUF_LEN];

    for (int i = 0; i < n_sub; i++) {
        float d = fabsf(amp[i] - s_baseline[i]);
        wf_bins[i] = d;
        total_delta += d;
    }
    float avg_delta = total_delta / n_sub;

    // Gently update baseline (exponential moving average) to adapt to env
    float alpha = 0.02f;
    for (int i = 0; i < n_sub; i++) {
        s_baseline[i] = s_baseline[i] * (1.0f - alpha) + amp[i] * alpha;
    }

    float threshold = sensitivity_to_threshold(s_sensitivity);

    // ── Dispatch based on mode ────────────────────────────────────────────────
    switch (s_mode) {
        case 0: { // motion detection
            if (avg_delta > threshold) {
                uint8_t intensity  = delta_to_byte(avg_delta);
                uint8_t proximity  = delta_to_proximity(avg_delta);
                if (s_motion_cb) s_motion_cb(intensity, proximity);
            }
            break;
        }

        case 1: { // waterfall
            // Compress n_sub bins down to WATERFALL_COLS
            uint8_t out[WATERFALL_COLS] = {0};
            int bins_per_col = n_sub / WATERFALL_COLS;
            if (bins_per_col < 1) bins_per_col = 1;
            for (int c = 0; c < WATERFALL_COLS; c++) {
                float sum = 0;
                for (int b = 0; b < bins_per_col; b++) {
                    int idx = c * bins_per_col + b;
                    if (idx < n_sub) sum += wf_bins[idx];
                }
                out[c] = delta_to_byte(sum / bins_per_col);
            }
            if (s_waterfall_cb) s_waterfall_cb(out, WATERFALL_COLS);
            break;
        }

        case 2: { // proximity — still fires motion_cb, proximity field is key
            uint8_t intensity = delta_to_byte(avg_delta);
            uint8_t proximity = delta_to_proximity(avg_delta);
            if (s_motion_cb) s_motion_cb(intensity, proximity);
            break;
        }
    }

    memcpy(s_prev, amp, sizeof(float) * n_sub);
}

// ─── Public API ───────────────────────────────────────────────────────────────
void csi_handler_init(csi_motion_cb_t motion_cb, csi_waterfall_cb_t waterfall_cb) {
    s_motion_cb    = motion_cb;
    s_waterfall_cb = waterfall_cb;

    memset(s_baseline, 0, sizeof(s_baseline));
    memset(s_prev,     0, sizeof(s_prev));
    memset(s_waterfall,0, sizeof(s_waterfall));

    wifi_csi_config_t csi_cfg = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = false,
        .manu_scale        = false,
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

    ESP_LOGI(TAG, "CSI handler init complete, sensitivity=%d mode=%d",
             s_sensitivity, s_mode);
}

void csi_set_sensitivity(uint8_t level) {
    if (level > 10) level = 10;
    s_sensitivity = level;
}

void csi_set_mode(uint8_t mode) {
    if (mode > 2) mode = 0;
    s_mode = mode;
    // reset baseline when switching modes so we recalibrate
    s_baseline_done = false;
    s_frame_count   = 0;
    memset(s_baseline, 0, sizeof(s_baseline));
    ESP_LOGI(TAG, "Mode changed to %d, recalibrating baseline...", mode);
}
