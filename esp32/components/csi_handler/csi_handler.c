#include "csi_handler.h"
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_log.h"

static const char *TAG = "csi_handler";

// Forward declaration — defined after wifi_csi_cb
static void vitals_update(float *amp, int n_sub);

// ─── State ────────────────────────────────────────────────────────────────────
static csi_motion_cb_t    s_motion_cb    = NULL;
static csi_waterfall_cb_t s_waterfall_cb = NULL;
static csi_vitals_cb_t    s_vitals_cb    = NULL;
static uint8_t            s_sensitivity  = 5;
static uint8_t            s_mode         = 0;

// ─── Baseline state ───────────────────────────────────────────────────────────
#define CSI_BUF_LEN      64
#define BASELINE_FRAMES  30
#define WF_COLS          32  // waterfall output bins

static float s_baseline[CSI_BUF_LEN];
static int   s_frame_count   = 0;
static bool  s_baseline_done = false;

// ─── Vitals DSP state ─────────────────────────────────────────────────────────
// Uses EMA-diff bandpass: breathing = med_ema - long_ema (0.1–0.5 Hz)
//                         heart     = short_ema - med_ema  (0.5–3 Hz)
// Zero-crossing count over 30-second windows → BPM
#define VITALS_WINDOW_FRAMES  300   // ~30s at approx 10 fps (beacon rate)

static float   s_amp_long_ema        = 0.0f; // alpha=0.01 → ~10s TC (DC removal)
static float   s_amp_med_ema         = 0.0f; // alpha=0.05 → ~2s TC  (breathing upper)
static float   s_amp_short_ema       = 0.0f; // alpha=0.30 → ~0.3s TC (heart upper)
static bool    s_vitals_ema_init     = false;
static bool    s_breath_prev_pos     = false;
static bool    s_heart_prev_pos      = false;
static int     s_breath_cross_count  = 0;
static int     s_heart_cross_count   = 0;
static int     s_vitals_frame_count  = 0;

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
            // Compress n_sub bins → WF_COLS output bins
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

        case 3: // vitals — breathing & heart rate via EMA-diff bandpass
            vitals_update(amp, n_sub);
            break;

        default:
            break;
    }
}

// ─── Vitals DSP — EMA-diff bandpass + zero-crossing counter ──────────────────
static void vitals_update(float *amp, int n_sub) {
    float amp_avg = 0.0f;
    for(int i = 0; i < n_sub; i++) amp_avg += amp[i];
    amp_avg /= (float)n_sub;

    if(!s_vitals_ema_init) {
        s_amp_long_ema    = amp_avg;
        s_amp_med_ema     = amp_avg;
        s_amp_short_ema   = amp_avg;
        s_vitals_ema_init = true;
    }

    // One-pole IIR lowpass at three timescales
    s_amp_long_ema  = 0.99f * s_amp_long_ema  + 0.01f * amp_avg; // ~10s TC
    s_amp_med_ema   = 0.95f * s_amp_med_ema   + 0.05f * amp_avg; // ~2s TC
    s_amp_short_ema = 0.70f * s_amp_short_ema + 0.30f * amp_avg; // ~0.3s TC

    // Bandpass via subtraction of adjacent lowpass outputs
    float breath_signal = s_amp_med_ema   - s_amp_long_ema;  // 0.1–0.5 Hz band
    float heart_signal  = s_amp_short_ema - s_amp_med_ema;   // 0.5–3.0 Hz band

    // Count upward zero-crossings — each = one complete oscillation cycle
    bool breath_pos = (breath_signal >= 0.0f);
    if(breath_pos && !s_breath_prev_pos) s_breath_cross_count++;
    s_breath_prev_pos = breath_pos;

    bool heart_pos = (heart_signal >= 0.0f);
    if(heart_pos && !s_heart_prev_pos) s_heart_cross_count++;
    s_heart_prev_pos = heart_pos;

    if(++s_vitals_frame_count < VITALS_WINDOW_FRAMES) return;

    // 30-second window complete — compute BPM and report
    uint8_t breathing_bpm = (uint8_t)((s_breath_cross_count * 60) / 30);
    uint8_t heart_bpm     = (uint8_t)((s_heart_cross_count  * 60) / 30);

    // Clamp to physiological ranges; 0 = no valid reading
    if(breathing_bpm < 6  || breathing_bpm > 40)  breathing_bpm = 0;
    if(heart_bpm     < 40 || heart_bpm     > 180) heart_bpm     = 0;

    ESP_LOGI(TAG, "Vitals: breath=%d BPM  heart=%d BPM (exp)",
             breathing_bpm, heart_bpm);

    if(s_vitals_cb) s_vitals_cb(breathing_bpm, heart_bpm);

    s_breath_cross_count = 0;
    s_heart_cross_count  = 0;
    s_vitals_frame_count = 0;
}

// ─── Public API ───────────────────────────────────────────────────────────────
void csi_handler_init(csi_motion_cb_t motion_cb, csi_waterfall_cb_t waterfall_cb) {
    s_motion_cb    = motion_cb;
    s_waterfall_cb = waterfall_cb;

    memset(s_baseline, 0, sizeof(s_baseline));
    s_frame_count   = 0;
    s_baseline_done = false;

    // ── CSI config varies significantly between chip families ─────────────────
    // ESP32-C6 and C61 use wifi_csi_acquire_config_t (renamed + different fields)
    // All other targets use wifi_csi_config_t with lltf_en/htltf_en etc
#if CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32C61 || CONFIG_IDF_TARGET_ESP32C5
    wifi_csi_acquire_config_t csi_cfg = {
        .enable              = 1,
        .acquire_csi_legacy  = 1,   // L-LTF (legacy preamble)
        .acquire_csi_ht20    = 1,   // HT20 packets
        .acquire_csi_ht40    = 0,   // HT40 — disable to keep buf size predictable
        .acquire_csi_su      = 0,
        .acquire_csi_mu      = 0,
        .acquire_csi_dcm     = 0,
        .acquire_csi_beamformed = 0,
        .acquire_csi_he_stbc = 0,
        .val_scale_cfg       = 0,
        .dump_ack_en         = 0,
    };
#else
    // ESP32, ESP32-S2, ESP32-S3, ESP32-C3 all use this struct
    wifi_csi_config_t csi_cfg = {
        .lltf_en           = true,
        .htltf_en          = true,
        .stbc_htltf2_en    = true,
        .ltf_merge_en      = true,
        .channel_filter_en = false,
        .manu_scale        = false,
    };
#endif

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
    if(mode > 3) mode = 0;
    s_mode = mode;
    csi_reset_baseline();
    // Reset vitals DSP state when entering/leaving vitals mode
    s_vitals_ema_init    = false;
    s_breath_cross_count = 0;
    s_heart_cross_count  = 0;
    s_vitals_frame_count = 0;
    s_breath_prev_pos    = false;
    s_heart_prev_pos     = false;
    ESP_LOGI(TAG, "Mode -> %d", mode);
}

void csi_reset_baseline(void) {
    s_baseline_done = false;
    s_frame_count   = 0;
    memset(s_baseline, 0, sizeof(s_baseline));
    ESP_LOGI(TAG, "Baseline reset");
}

void csi_set_vitals_cb(csi_vitals_cb_t cb) {
    s_vitals_cb = cb;
}
