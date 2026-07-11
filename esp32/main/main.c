#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "csi_handler.h"
#include "web_ui.h"
#include "mesh_node.h"

// ESP-IDF has no built-in equivalent to Furi's UNUSED() macro used throughout
// the Flipper side of this project — define our own so the same pattern works here.
#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

static const char *TAG = "CSIght";

// ─── UART config ──────────────────────────────────────────────────────────────
#define UART_PORT      UART_NUM_1
#define UART_BAUD      115200
#define UART_BUF_SIZE  1024

#if CONFIG_CSIGHT_ROLE_PRIMARY
// Resolved at runtime from chip model — see uart_init(). Secondary builds
// never touch UART (no Flipper attached), so these are primary-only.
static int s_uart_tx_pin = 16;  // default: C6
static int s_uart_rx_pin = 17;
#endif

// ─── Protocol ────────────────────────────────────────────────────────────────
#define PROTO_HANDSHAKE_REQ  0xAA
#define PROTO_HANDSHAKE_ACK  0xBB
#define PROTO_MOTION_EVT     0x01
#define PROTO_WATERFALL_EVT  0x03
#define PROTO_VITALS_EVT     0x04
#define PROTO_SENSITIVITY    0x10
#define PROTO_MODE_SET       0x11
#define PROTO_CALIBRATE      0x12
#define PROTO_CHANNEL_SET    0x13  // [0x13][ch 1-13] — manual override
#define PROTO_CHANNEL_AUTO   0x14  // [0x14] — trigger auto survey and pick best
#define PROTO_MESH_EVT       0x05  // Primary→Flipper: aggregated node readings
#define PROTO_NODE_FOUND     0x06  // Primary→Flipper: [node_id] new secondary paired
#define PROTO_FORGET_NODES   0x15  // Flipper→ESP32: clear all paired secondaries
#define PROTO_NODE_POSITIONS 0x16  // Flipper→ESP32: [x1][y1][x2][y2][x3][y3] int16 cm each
#define PROTO_PATHLOSS_GAMMA 0x17  // Flipper→ESP32: [gamma_x10] e.g. 25 = gamma 2.5
#define PROTO_WEBUI_ON       0x20
#define PROTO_WEBUI_OFF      0x21

// ─── NVS key for mode persistence ─────────────────────────────────────────────
#define NVS_NAMESPACE    "csight"
#define NVS_KEY_WEBUI    "webui_mode"

// ─── Chip helpers ─────────────────────────────────────────────────────────────
// These, and everything through uart_init() below, are only used to build the
// UART handshake and manage the physical UART link — none of which a secondary
// node needs (no Flipper attached). Gating them avoids "defined but not used"
// warnings on secondary builds.
#if CONFIG_CSIGHT_ROLE_PRIMARY
static const char* get_chip_name(esp_chip_model_t model) {
    switch(model) {
        case CHIP_ESP32:   return "ESP32";
        case CHIP_ESP32S2: return "ESP32-S2";
        case CHIP_ESP32S3: return "ESP32-S3";
        case CHIP_ESP32C3: return "ESP32-C3";
        case CHIP_ESP32C6: return "ESP32-C6";
        case CHIP_ESP32H2: return "ESP32-H2";
#ifdef CHIP_ESP32C5
        case CHIP_ESP32C5:  return "ESP32-C5";
#endif
#ifdef CHIP_ESP32C61
        case CHIP_ESP32C61: return "ESP32-C61";
#endif
        default:           return "ESP32-??";
    }
}

static uint8_t get_csi_support(esp_chip_model_t model) {
    switch(model) {
        case CHIP_ESP32:   return 1;  // limited CSI
        case CHIP_ESP32S2: return 1;  // limited CSI (same generation as ESP32)
        case CHIP_ESP32S3: return 2;  // full CSI
        case CHIP_ESP32C3: return 2;  // full CSI
        case CHIP_ESP32C6: return 2;  // full CSI
        case CHIP_ESP32H2: return 0;  // no WiFi — Zigbee/Thread only
#ifdef CHIP_ESP32C5
        case CHIP_ESP32C5:  return 2; // best CSI performer per Espressif ranking (IDF >= 5.3)
#endif
#ifdef CHIP_ESP32C61
        case CHIP_ESP32C61: return 2; // C6 variant, full CSI (IDF >= 5.4)
#endif
        default:           return 1;
    }
}

// ─── Handshake packet ─────────────────────────────────────────────────────────
// [0xBB][len][chip_name\0][csi_support][fw_major][fw_minor][webui_mode][chk]
static int build_handshake(uint8_t *buf, size_t buf_size) {
    esp_chip_info_t chip;
    esp_chip_info(&chip);

    const char *name    = get_chip_name(chip.model);
    uint8_t     support = get_csi_support(chip.model);
    uint8_t     fw_maj  = 3;
    uint8_t     fw_min  = 3;  // v3.3
    uint8_t     webui   = (web_ui_get_mode() == WEB_UI_ON) ? 1 : 0;

    size_t  name_len    = strlen(name);
    uint8_t payload_len = (uint8_t)(name_len + 1 + 4); // +webui byte
    if((size_t)(payload_len + 3) > buf_size) return -1;

    int i = 0;
    buf[i++] = PROTO_HANDSHAKE_ACK;
    buf[i++] = payload_len;
    memcpy(&buf[i], name, name_len + 1);
    i += (int)name_len + 1;
    buf[i++] = support;
    buf[i++] = fw_maj;
    buf[i++] = fw_min;
    buf[i++] = webui;

    uint8_t chk = 0;
    for(int j = 1; j < i; j++) chk ^= buf[j];
    buf[i++] = chk;
    return i;
}

// ─── UART init ────────────────────────────────────────────────────────────────
static void uart_init(void) {
    // Auto-select UART0 default pins for each chip family
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    switch(chip.model) {
        case CHIP_ESP32S3:
        case CHIP_ESP32S2:
            s_uart_tx_pin = 43; s_uart_rx_pin = 44; break;
        case CHIP_ESP32C3:
            s_uart_tx_pin = 21; s_uart_rx_pin = 20; break;
        case CHIP_ESP32C6:
#ifdef CHIP_ESP32C61
        case CHIP_ESP32C61:
#endif
            s_uart_tx_pin = 16; s_uart_rx_pin = 17; break;
#ifdef CHIP_ESP32C5
        case CHIP_ESP32C5:
            s_uart_tx_pin = 6;  s_uart_rx_pin = 7;  break;  // C5 UART0 default
#endif
        default: // ESP32 classic
            s_uart_tx_pin = 1;  s_uart_rx_pin = 3;  break;
    }

    const uart_config_t cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, s_uart_tx_pin, s_uart_rx_pin,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2,
                                        UART_BUF_SIZE * 2, 0, NULL, 0));
    ESP_LOGI(TAG, "UART ready TX=%d RX=%d @ %d (chip=%d)",
             s_uart_tx_pin, s_uart_rx_pin, UART_BAUD, chip.model);
}
#endif // CONFIG_CSIGHT_ROLE_PRIMARY

// ─── WiFi init (STA mode, configurable channel for CSI sniffing) ─────────────
#define DEFAULT_WIFI_CHANNEL 6
static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
#if CONFIG_CSIGHT_ROLE_PRIMARY
    esp_netif_create_default_wifi_ap(); // only primary ever runs the web UI's AP
#endif

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(DEFAULT_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_LOGI(TAG, "WiFi ready, channel %d", DEFAULT_WIFI_CHANNEL);
}

// ─── NVS — persist web UI mode across reboots ─────────────────────────────────
// Web UI only exists on the primary; secondaries have no HTTP server to persist.
#if CONFIG_CSIGHT_ROLE_PRIMARY
static void save_webui_mode(uint8_t on) {
    nvs_handle_t h;
    if(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, NVS_KEY_WEBUI, on);
        nvs_commit(h);
        nvs_close(h);
    }
}

static uint8_t load_webui_mode(void) {
    nvs_handle_t h;
    uint8_t val = 0;
    if(nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        nvs_get_u8(h, NVS_KEY_WEBUI, &val);
        nvs_close(h);
    }
    return val;
}
#endif // CONFIG_CSIGHT_ROLE_PRIMARY

// ─── Auto channel survey ─────────────────────────────────────────────────────
// Spends 1s per channel counting promiscuous packets, picks the busiest one
// (most activity = most CSI frames), then reports back via PROTO_CHANNEL_SET
// so the Flipper UI stays in sync. Only ever triggered by the primary (from
// the handshake handler or a Flipper channel-rescan request).
#if CONFIG_CSIGHT_ROLE_PRIMARY
static volatile uint16_t s_survey_count = 0;
static volatile bool     s_survey_running = false;

static void IRAM_ATTR survey_promisc_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    UNUSED(buf); UNUSED(type);
    s_survey_count++;
}

static void channel_survey_task(void* arg) {
    UNUSED(arg);

    if(s_survey_running) {
        ESP_LOGW(TAG, "Survey already running, ignoring request");
        vTaskDelete(NULL);
        return;
    }
    s_survey_running = true;

    esp_wifi_set_csi(false);
    esp_wifi_set_promiscuous_rx_cb(survey_promisc_cb);
    esp_wifi_set_promiscuous(true);

    uint8_t  best_ch  = DEFAULT_WIFI_CHANNEL;
    uint16_t best_cnt = 0;

    for(uint8_t ch = 1; ch <= 13; ch++) {
        s_survey_count = 0;
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint16_t cnt = s_survey_count;
        ESP_LOGI(TAG, "Survey CH %d: %d pkts", ch, cnt);
        if(cnt > best_cnt) { best_cnt = cnt; best_ch = ch; }
    }

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_channel(best_ch, WIFI_SECOND_CHAN_NONE);
    csi_reset_baseline();
    esp_wifi_set_csi(true);

    // Inform Flipper which channel was selected
    uint8_t reply[2] = { PROTO_CHANNEL_SET, best_ch };
    uart_write_bytes(UART_PORT, (const char*)reply, 2);
    ESP_LOGI(TAG, "Auto-selected CH %d (%d pkts)", best_ch, best_cnt);

    s_survey_running = false;
    vTaskDelete(NULL);
}
#endif // CONFIG_CSIGHT_ROLE_PRIMARY

// ─── CSI callbacks — route to UART/Web UI (primary) or mesh (secondary) ──────
void csight_on_motion(uint8_t intensity, uint8_t proximity) {
#if CONFIG_CSIGHT_ROLE_PRIMARY
    // Primary: full behaviour — Flipper, web UI, and feed our own reading
    // into the mesh aggregator so it's included in position estimates too.
    uint8_t pkt[4];
    pkt[0] = PROTO_MOTION_EVT;
    pkt[1] = intensity;
    pkt[2] = proximity;
    pkt[3] = pkt[0] ^ pkt[1] ^ pkt[2];
    uart_write_bytes(UART_PORT, (const char*)pkt, sizeof(pkt));

    web_ui_send_motion(intensity, proximity);
    mesh_set_local_reading(intensity, proximity);
#else
    // Secondary: no Flipper attached — just broadcast to the primary
    mesh_broadcast_reading(intensity, proximity);
#endif
}

void csight_on_waterfall(uint8_t *bins, uint8_t count) {
    uint8_t pkt[64];
    if(count > 60) count = 60;
    pkt[0] = PROTO_WATERFALL_EVT;
    pkt[1] = count;
    memcpy(&pkt[2], bins, count);
    uint8_t chk = 0;
    for(int i = 0; i < count + 2; i++) chk ^= pkt[i];
    pkt[count + 2] = chk;
    uart_write_bytes(UART_PORT, (const char*)pkt, count + 3);

    web_ui_send_waterfall(bins, count);
}

void csight_on_vitals(uint8_t breathing_bpm, uint8_t heart_bpm) {
    uint8_t pkt[4];
    pkt[0] = PROTO_VITALS_EVT;
    pkt[1] = breathing_bpm;
    pkt[2] = heart_bpm;
    pkt[3] = pkt[0] ^ pkt[1] ^ pkt[2];
    uart_write_bytes(UART_PORT, (const char*)pkt, sizeof(pkt));
    web_ui_send_vitals(breathing_bpm, heart_bpm);
}

#if CONFIG_CSIGHT_ROLE_PRIMARY
// ─── Node discovery (primary only) ───────────────────────────────────────────
// Fired by mesh_node.c the moment a brand-new secondary completes pairing.
// Forwarded straight to the Flipper so the user gets immediate feedback
// rather than silently seeing a new dot appear on the map minutes later.
static void on_node_discovered(uint8_t node_id) {
    uint8_t pkt[3];
    pkt[0] = PROTO_NODE_FOUND;
    pkt[1] = node_id;
    pkt[2] = pkt[0] ^ pkt[1];
    uart_write_bytes(UART_PORT, (const char*)pkt, sizeof(pkt));
    web_ui_send_node_found(node_id);
    ESP_LOGI(TAG, "Notified Flipper: node %d discovered", node_id);
}

// ─── Mesh reporting (primary only) ────────────────────────────────────────────
// Default node layout: a 2m x 2m square, node 0 (primary) at origin corner.
// Overridden at runtime by whatever the Flipper has configured — this is just
// a sane fallback so the estimate isn't garbage before the user sets real
// positions. Units are centimetres.
static MeshNodePos s_node_positions[MESH_MAX_NODES] = {
    { 0,   0   },  // node 0 — primary
    { 200, 0   },  // node 1
    { 200, 200 },  // node 2
    { 0,   200 },  // node 3
};

static void mesh_report_task(void* arg) {
    (void)arg;
    while(1) {
        MeshNodeReading readings[MESH_MAX_NODES];
        uint8_t n = mesh_get_readings(readings, MESH_MAX_NODES);

        int16_t est_x = 0, est_y = 0;
        bool has_estimate = mesh_estimate_position(s_node_positions, &est_x, &est_y);

        // Packet: [0x05][node_count][has_est][est_x_lo][est_x_hi][est_y_lo][est_y_hi]
        //         [node_id][intensity][proximity] * node_count [chk]
        uint8_t pkt[8 + MESH_MAX_NODES * 3 + 1];
        int i = 0;
        pkt[i++] = PROTO_MESH_EVT;
        pkt[i++] = n;
        pkt[i++] = has_estimate ? 1 : 0;
        pkt[i++] = (uint8_t)(est_x & 0xFF);
        pkt[i++] = (uint8_t)((est_x >> 8) & 0xFF);
        pkt[i++] = (uint8_t)(est_y & 0xFF);
        pkt[i++] = (uint8_t)((est_y >> 8) & 0xFF);

        for(uint8_t j = 0; j < n; j++) {
            pkt[i++] = readings[j].node_id;
            pkt[i++] = readings[j].intensity;
            pkt[i++] = readings[j].proximity;
        }

        uint8_t chk = 0;
        for(int k = 0; k < i; k++) chk ^= pkt[k];
        pkt[i++] = chk;

        uart_write_bytes(UART_PORT, (const char*)pkt, i);

        // Also broadcast to any connected web UI clients — separate simpler
        // format since the browser has no local config for node positions
        int16_t pos_x[4], pos_y[4];
        uint8_t node_ids[MESH_MAX_NODES], intensities[MESH_MAX_NODES], proximities[MESH_MAX_NODES];
        for(int p = 0; p < 4; p++) {
            pos_x[p] = s_node_positions[p].x_cm;
            pos_y[p] = s_node_positions[p].y_cm;
        }
        for(uint8_t j = 0; j < n; j++) {
            node_ids[j]    = readings[j].node_id;
            intensities[j] = readings[j].intensity;
            proximities[j] = readings[j].proximity;
        }
        web_ui_send_mesh(pos_x, pos_y, has_estimate ? 1 : 0, est_x, est_y,
                          node_ids, intensities, proximities, n);

        vTaskDelay(pdMS_TO_TICKS(500));  // 2 Hz — plenty for a position estimate
    }
}
#endif

// ─── UART command task ────────────────────────────────────────────────────────
// Secondaries have no UART/Flipper attached, so this whole task is primary-only.
#if CONFIG_CSIGHT_ROLE_PRIMARY
static void uart_cmd_task(void *arg) {
    (void)arg;
    uint8_t rxbuf[64];
    uint8_t txbuf[64];

    while(1) {
        int len = uart_read_bytes(UART_PORT, rxbuf, sizeof(rxbuf),
                                  pdMS_TO_TICKS(20));
        if(len <= 0) continue;

        for(int i = 0; i < len; i++) {
            switch(rxbuf[i]) {

                case PROTO_HANDSHAKE_REQ: {
                    int pkt_len = build_handshake(txbuf, sizeof(txbuf));
                    if(pkt_len > 0) {
                        uart_write_bytes(UART_PORT, (const char*)txbuf, pkt_len);
                        ESP_LOGI(TAG, "Handshake sent — starting channel survey");
                    }
                    // Auto-select best channel right after handshake
                    xTaskCreate(channel_survey_task, "ch_survey", 4096, NULL, 5, NULL);
                    break;
                }

                case PROTO_SENSITIVITY:
                    if(i + 1 < len) csi_set_sensitivity(rxbuf[++i]);
                    break;

                case PROTO_MODE_SET:
                    if(i + 1 < len) csi_set_mode(rxbuf[++i]);
                    break;

                case PROTO_CALIBRATE:
                    csi_reset_baseline();
                    ESP_LOGI(TAG, "Forced recalibration");
                    break;

                case PROTO_CHANNEL_SET:
                    if(i + 1 < len) {
                        uint8_t ch = rxbuf[++i];
                        if(ch >= 1 && ch <= 13) {
                            esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
                            csi_reset_baseline(); // recalibrate after channel change
                            ESP_LOGI(TAG, "Channel -> %d", ch);
                        }
                    }
                    break;

                case PROTO_CHANNEL_AUTO:
                    // Kick off background survey — takes ~13s, result sent back via PROTO_CHANNEL_SET
                    xTaskCreate(channel_survey_task, "ch_survey", 4096, NULL, 5, NULL);
                    ESP_LOGI(TAG, "Channel auto-survey started");
                    break;

                case PROTO_FORGET_NODES:
                    mesh_forget_all_nodes();
                    ESP_LOGI(TAG, "All paired mesh nodes forgotten");
                    break;

                case PROTO_NODE_POSITIONS: {
                    // [x1_lo][x1_hi][y1_lo][y1_hi] x3 nodes = 12 payload bytes
                    // after the command byte. No checksum — small, low-stakes
                    // data where a corrupted read just means a temporarily
                    // wrong estimate until the next update arrives.
                    if(i + 12 >= len) break; // need indices i+1..i+12 all valid
                    int p = i + 1;
                    for(int n = 1; n <= 3; n++) {
                        int16_t x = (int16_t)(rxbuf[p]     | (rxbuf[p + 1] << 8));
                        int16_t y = (int16_t)(rxbuf[p + 2] | (rxbuf[p + 3] << 8));
                        s_node_positions[n].x_cm = x;
                        s_node_positions[n].y_cm = y;
                        p += 4;
                    }
                    i += 12; // advance past all consumed payload bytes
                    ESP_LOGI(TAG, "Node positions updated from Flipper");
                    break;
                }

                case PROTO_PATHLOSS_GAMMA: {
                    // [gamma_x10] — single byte, gamma value * 10 (e.g. 25 = 2.5)
                    if(i + 1 >= len) break;
                    uint8_t gamma_x10 = rxbuf[++i];
                    mesh_set_pathloss_gamma((float)gamma_x10 / 10.0f);
                    ESP_LOGI(TAG, "Path-loss gamma -> %.1f", (float)gamma_x10 / 10.0f);
                    break;
                }

                case PROTO_WEBUI_ON:
                    ESP_LOGI(TAG, "Web UI ON command from Flipper");
                    web_ui_start();
                    save_webui_mode(1);
                    break;

                case PROTO_WEBUI_OFF:
                    ESP_LOGI(TAG, "Web UI OFF command from Flipper");
                    web_ui_stop();
                    save_webui_mode(0);
                    break;

                default:
                    break;
            }
        }
    }
}
#endif // CONFIG_CSIGHT_ROLE_PRIMARY

// ─── Entry point ─────────────────────────────────────────────────────────────
void app_main(void) {
    ESP_LOGI(TAG, "CSIght v3.3 booting (role=%s)...",
#if CONFIG_CSIGHT_ROLE_PRIMARY
             "primary"
#else
             "secondary"
#endif
    );

    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
    csi_handler_init(csight_on_motion, csight_on_waterfall);

#if CONFIG_CSIGHT_ROLE_PRIMARY
    // Full primary behaviour — unchanged from v1.7, plus mesh aggregation
    uart_init();
    csi_set_vitals_cb(csight_on_vitals);
    mesh_init_primary();
    mesh_set_discovery_cb(on_node_discovered);

    if(load_webui_mode()) {
        ESP_LOGI(TAG, "Restoring web UI mode from NVS");
        web_ui_start();
    }

    xTaskCreate(uart_cmd_task,   "uart_cmd",   4096, NULL, 5, NULL);
    xTaskCreate(mesh_report_task, "mesh_report", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "CSIght primary ready. Web UI: %s",
             web_ui_get_mode() == WEB_UI_ON ? "ON" : "OFF (Flipper mode)");
#else
    // Secondary — standalone node, no Flipper, no UART, no web UI.
    // v2.1: no manual ID needed — identity comes from this chip's factory MAC,
    // and the primary assigns an ID automatically on first contact.
    mesh_init_secondary();
    ESP_LOGI(TAG, "CSIght secondary ready — broadcasting HELLO until paired");
#endif
}
