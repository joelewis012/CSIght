#include "web_ui.h"
#include "csi_handler.h"
#include "web_ui_html.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "cJSON.h"

static const char *TAG = "web_ui";

// ─── State ────────────────────────────────────────────────────────────────────
static httpd_handle_t  s_server      = NULL;
static WebUIMode       s_mode        = WEB_UI_OFF;
#define MAX_WS_CLIENTS 4
static int             s_ws_fds[MAX_WS_CLIENTS];   // -1 = empty slot
static SemaphoreHandle_t s_ws_mutex  = NULL;        // guards s_ws_fds

// ─── WebSocket send helper — broadcasts to every connected client ────────────
static void ws_send_binary(const uint8_t *data, size_t len) {
    if(s_server == NULL) return;
    if(xSemaphoreTake(s_ws_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;

    int fds[MAX_WS_CLIENTS];
    memcpy(fds, s_ws_fds, sizeof(fds));
    xSemaphoreGive(s_ws_mutex);

    httpd_ws_frame_t frame = {
        .type    = HTTPD_WS_TYPE_BINARY,
        .payload = (uint8_t*)data,
        .len     = len,
        .final   = true,
    };

    for(int i = 0; i < MAX_WS_CLIENTS; i++) {
        if(fds[i] < 0) continue;
        esp_err_t err = httpd_ws_send_frame_async(s_server, fds[i], &frame);
        if(err != ESP_OK) {
            ESP_LOGW(TAG, "WS send failed for fd=%d, dropping client", fds[i]);
            if(xSemaphoreTake(s_ws_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if(s_ws_fds[i] == fds[i]) s_ws_fds[i] = -1; // only clear if unchanged
                xSemaphoreGive(s_ws_mutex);
            }
        }
    }
}

// ─── HTTP handlers ────────────────────────────────────────────────────────────
// Serve root — returns the embedded HTML page
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_send(req, CSIGHT_WEB_UI, strlen(CSIGHT_WEB_UI));
    return ESP_OK;
}

// WebSocket handler — handles upgrade + incoming control messages
static esp_err_t ws_handler(httpd_req_t *req) {
    if(req->method == HTTP_GET) {
        // WebSocket upgrade handshake — find a free slot, or evict the oldest
        // if all are full (better to drop a stale client than refuse a new one)
        int fd = httpd_req_to_sockfd(req);
        if(xSemaphoreTake(s_ws_mutex, portMAX_DELAY) == pdTRUE) {
            int slot = -1;
            for(int i = 0; i < MAX_WS_CLIENTS; i++) {
                if(s_ws_fds[i] < 0) { slot = i; break; }
            }
            if(slot < 0) slot = 0; // all full — evict slot 0 as a simple fallback
            s_ws_fds[slot] = fd;
            xSemaphoreGive(s_ws_mutex);
        }
        ESP_LOGI(TAG, "WS client connected, fd=%d", fd);
        return ESP_OK;
    }

    // Receive frame
    httpd_ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &frame, 0);
    if(ret != ESP_OK) return ret;

    if(frame.len == 0) return ESP_OK;

    uint8_t *buf = calloc(1, frame.len + 1);
    if(!buf) return ESP_ERR_NO_MEM;

    frame.payload = buf;
    ret = httpd_ws_recv_frame(req, &frame, frame.len);
    if(ret != ESP_OK) { free(buf); return ret; }

    // Parse JSON command: {"cmd":"mode","val":1} or {"cmd":"sens","val":5}
    cJSON *json = cJSON_ParseWithLength((char*)buf, frame.len);
    if(json) {
        cJSON *cmd = cJSON_GetObjectItem(json, "cmd");
        cJSON *val = cJSON_GetObjectItem(json, "val");
        if(cJSON_IsString(cmd) && cJSON_IsNumber(val)) {
            if(strcmp(cmd->valuestring, "mode") == 0) {
                csi_set_mode((uint8_t)val->valueint);
                ESP_LOGI(TAG, "WS: mode -> %d", val->valueint);
            } else if(strcmp(cmd->valuestring, "sens") == 0) {
                csi_set_sensitivity((uint8_t)val->valueint);
                ESP_LOGI(TAG, "WS: sensitivity -> %d", val->valueint);
            }
        }
        cJSON_Delete(json);
    }

    free(buf);
    return ESP_OK;
}

// ─── HTTP server start ────────────────────────────────────────────────────────
// ─── OTA firmware update ──────────────────────────────────────────────────────
// Accepts a raw firmware.bin as the POST body (no multipart parsing needed —
// the browser just sends the file's bytes directly). Streams straight into
// the inactive OTA partition so memory use stays flat regardless of image size.
static esp_err_t ota_handler(httpd_req_t *req) {
    if(req->content_len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty upload");
        return ESP_FAIL;
    }

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
    if(!update_partition) {
        ESP_LOGE(TAG, "No OTA partition available — was this board flashed with an old partition table?");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "No OTA slot. Re-flash via USB once to update the partition table.");
        return ESP_FAIL;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
        return ESP_FAIL;
    }

    static uint8_t buf[1024];
    int remaining = req->content_len;
    int received_total = 0;

    while(remaining > 0) {
        int chunk = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        int received = httpd_req_recv(req, (char*)buf, chunk);
        if(received <= 0) {
            if(received == HTTPD_SOCK_ERR_TIMEOUT) continue; // retry on timeout
            ESP_LOGE(TAG, "OTA recv failed at %d/%d bytes", received_total, (int)req->content_len);
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Upload interrupted");
            return ESP_FAIL;
        }

        err = esp_ota_write(ota_handle, buf, received);
        if(err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Flash write failed");
            return ESP_FAIL;
        }

        received_total += received;
        remaining      -= received;
    }

    err = esp_ota_end(ota_handle);
    if(err != ESP_OK) {
        // Most common cause: uploaded file wasn't a valid signed CSIght image
        ESP_LOGE(TAG, "esp_ota_end failed: %s (was this a valid firmware.bin?)", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid firmware image");
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not set boot partition");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA update successful (%d bytes), rebooting...", received_total);
    httpd_resp_sendstr(req, "OK — rebooting");

    // Give the response time to actually reach the browser before we reboot
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK; // unreachable, but keeps the compiler happy
}

static httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port      = CSIGHT_HTTP_PORT;
    config.max_open_sockets  = 6;   // 4 WS clients + root/OTA headroom
    config.stack_size        = 8192; // OTA writes need more stack than the default

    httpd_handle_t server = NULL;
    if(httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return NULL;
    }

    // Root page
    static const httpd_uri_t root = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = root_handler,
    };
    httpd_register_uri_handler(server, &root);

    // WebSocket endpoint
    static const httpd_uri_t ws = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(server, &ws);

    // Firmware update endpoint — raw binary POST body
    static const httpd_uri_t ota = {
        .uri     = "/update",
        .method  = HTTP_POST,
        .handler = ota_handler,
    };
    httpd_register_uri_handler(server, &ota);

    ESP_LOGI(TAG, "HTTP server started on port %d", CSIGHT_HTTP_PORT);
    return server;
}

// ─── WiFi AP init ─────────────────────────────────────────────────────────────
static void wifi_ap_init(void) {
    // WiFi is already started in main.c — just switch to AP+STA mode
    // so CSI sniffing continues alongside the AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid            = CSIGHT_AP_SSID,
            .ssid_len        = strlen(CSIGHT_AP_SSID),
            .password        = CSIGHT_AP_PASS,
            .channel         = CSIGHT_AP_CHANNEL,
            .max_connection  = CSIGHT_AP_MAX_CONN,
            .authmode        = WIFI_AUTH_WPA2_PSK,
        },
    };
    // Open network if no password set
    if(strlen(CSIGHT_AP_PASS) == 0) {
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_LOGI(TAG, "AP started: SSID=%s PASS=%s", CSIGHT_AP_SSID, CSIGHT_AP_PASS);
}

// ─── Public API ───────────────────────────────────────────────────────────────
void web_ui_start(void) {
    if(s_mode == WEB_UI_ON) return;
    if(!s_ws_mutex) s_ws_mutex = xSemaphoreCreateMutex();
    for(int i = 0; i < MAX_WS_CLIENTS; i++) s_ws_fds[i] = -1;
    wifi_ap_init();
    s_server = start_webserver();
    s_mode   = WEB_UI_ON;
    ESP_LOGI(TAG, "Web UI started — connect to WiFi '%s' then open http://192.168.4.1",
             CSIGHT_AP_SSID);
}

void web_ui_send_vitals(uint8_t breathing_bpm, uint8_t heart_bpm) {
    if(s_mode == WEB_UI_OFF) return;
    uint8_t pkt[4];
    pkt[0] = 0x04;  // PROTO_VITALS_EVT
    pkt[1] = breathing_bpm;
    pkt[2] = heart_bpm;
    pkt[3] = pkt[0] ^ pkt[1] ^ pkt[2];
    ws_send_binary(pkt, 4);
}

void web_ui_stop(void) {
    if(s_mode == WEB_UI_OFF) return;
    if(s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    if(s_ws_mutex && xSemaphoreTake(s_ws_mutex, portMAX_DELAY) == pdTRUE) {
        for(int i = 0; i < MAX_WS_CLIENTS; i++) s_ws_fds[i] = -1;
        xSemaphoreGive(s_ws_mutex);
    }
    esp_wifi_set_mode(WIFI_MODE_STA);
    s_mode  = WEB_UI_OFF;
    ESP_LOGI(TAG, "Web UI stopped");
}

void web_ui_send_motion(uint8_t intensity, uint8_t proximity) {
    if(s_mode == WEB_UI_OFF) return;
    uint8_t pkt[4];
    pkt[0] = 0x01;
    pkt[1] = intensity;
    pkt[2] = proximity;
    pkt[3] = pkt[0] ^ pkt[1] ^ pkt[2];
    ws_send_binary(pkt, 4);
}

void web_ui_send_waterfall(uint8_t *bins, uint8_t count) {
    if(s_mode == WEB_UI_OFF) return;
    uint8_t pkt[64];
    if(count > 60) count = 60;
    pkt[0] = 0x03;
    pkt[1] = count;
    memcpy(&pkt[2], bins, count);
    uint8_t chk = 0;
    for(int i = 0; i < count + 2; i++) chk ^= pkt[i];
    pkt[count + 2] = chk;
    ws_send_binary(pkt, count + 3);
}

// Mesh packet for the browser — a different (simpler) layout than the
// Flipper's UART protocol, since the browser has no local config and needs
// node positions included every time (the Flipper already knows them).
// [0x05][node_count]
//   [x0][y0][x1][y1][x2][y2][x3][y3]  -- all 4 positions, int16 cm, always sent
//   [has_est][est_x][est_y]           -- int16 cm
//   [node_id][intensity][proximity] * node_count
void web_ui_send_mesh(const int16_t* positions_x, const int16_t* positions_y,
                       uint8_t has_estimate, int16_t est_x, int16_t est_y,
                       const uint8_t* node_ids, const uint8_t* intensities,
                       const uint8_t* proximities, uint8_t node_count) {
    if(s_mode == WEB_UI_OFF) return;
    if(node_count > 4) node_count = 4;

    uint8_t pkt[2 + 16 + 5 + 4 * 3];
    int i = 0;
    pkt[i++] = 0x05;
    pkt[i++] = node_count;

    for(int n = 0; n < 4; n++) {
        pkt[i++] = (uint8_t)(positions_x[n] & 0xFF);
        pkt[i++] = (uint8_t)((positions_x[n] >> 8) & 0xFF);
        pkt[i++] = (uint8_t)(positions_y[n] & 0xFF);
        pkt[i++] = (uint8_t)((positions_y[n] >> 8) & 0xFF);
    }

    pkt[i++] = has_estimate;
    pkt[i++] = (uint8_t)(est_x & 0xFF);
    pkt[i++] = (uint8_t)((est_x >> 8) & 0xFF);
    pkt[i++] = (uint8_t)(est_y & 0xFF);
    pkt[i++] = (uint8_t)((est_y >> 8) & 0xFF);

    for(uint8_t n = 0; n < node_count; n++) {
        pkt[i++] = node_ids[n];
        pkt[i++] = intensities[n];
        pkt[i++] = proximities[n];
    }

    ws_send_binary(pkt, i);
}

void web_ui_send_node_found(uint8_t node_id) {
    if(s_mode == WEB_UI_OFF) return;
    uint8_t pkt[2] = { 0x06, node_id };
    ws_send_binary(pkt, 2);
}

WebUIMode web_ui_get_mode(void) {
    return s_mode;
}
