#include "csight.h"
#include "csight_log.h"
#include <notification/notification_messages.h>

#define TAG              "CSIght_UART"
#define THREAD_STOP_FLAG  0x1

// ─── RX thread ────────────────────────────────────────────────────────────────
static int32_t uart_rx_thread(void* ctx) {
    CSIghtApp* app = (CSIghtApp*)ctx;
    uint8_t    buf[CSIGHT_RX_BUF];

    while(true) {
        // Check stop flag before blocking
        uint32_t flags = furi_thread_flags_get();
        if(flags & THREAD_STOP_FLAG) {
            furi_thread_flags_clear(THREAD_STOP_FLAG);
            break;
        }

        // Block with short timeout so stop flag is checked regularly
        size_t len = furi_stream_buffer_receive(
            app->rx_stream, buf, sizeof(buf), furi_ms_to_ticks(50));
        if(len == 0) continue;

        size_t i = 0;
        while(i < len) {
            uint8_t hdr = buf[i++];

            switch(hdr) {

                case PROTO_HANDSHAKE_ACK: {
                    // [0xBB][payload_len][chip_name\0][csi_support][fw_major][fw_minor][webui][chk]
                    if(i >= len) break;
                    uint8_t payload_len = buf[i++];
                    if(i + payload_len + 1 > len) break; // +1 for checksum byte

                    // Verify checksum: XOR of payload_len byte through last payload byte
                    uint8_t chk = payload_len;
                    for(uint8_t j = 0; j < payload_len; j++) chk ^= buf[i + j];
                    if(chk != buf[i + payload_len]) {
                        FURI_LOG_W(TAG, "Handshake checksum mismatch, discarding");
                        i += payload_len + 1;
                        break;
                    }

                    // Bounded strlen — strnlen not available in Flipper SDK
                    size_t name_len = 0;
                    while(name_len < payload_len && buf[i + name_len] != '\0') name_len++;
                    if(name_len >= sizeof(app->chip_name)) name_len = sizeof(app->chip_name) - 1;

                    memcpy(app->chip_name, &buf[i], name_len);
                    app->chip_name[name_len] = '\0';
                    i += name_len + 1;

                    if(i < len) app->csi_support   = buf[i++];
                    if(i < len) app->fw_major       = buf[i++];
                    if(i < len) app->fw_minor        = buf[i++];
                    if(i < len) app->web_ui_active  = (buf[i++] == 1);
                    i++; // skip verified checksum byte

                    FURI_LOG_I(TAG, "Handshake: %s CSI=%d FW=%d.%d",
                               app->chip_name, app->csi_support,
                               app->fw_major, app->fw_minor);

                    // First run → preset select, returning user → main menu
                    app->state = app->config_exists ?
                                 AppStateMainMenu : AppStateCompatCheck;
                    break;
                }

                case PROTO_MOTION_EVT: {
                    // [0x01][intensity][proximity][chk]  chk = 0x01^intensity^proximity
                    if(i + 3 > len) break;
                    uint8_t intensity = buf[i++];
                    uint8_t proximity = buf[i++];
                    uint8_t chk       = buf[i++];
                    if((PROTO_MOTION_EVT ^ intensity ^ proximity) != chk) {
                        FURI_LOG_W(TAG, "Motion checksum mismatch, discarding");
                        break;
                    }

                    app->motion_intensity = intensity;
                    app->proximity        = proximity;
                    csight_add_blip(app, intensity, proximity);
                    app->target_acquired = true;
                    app->target_ts       = furi_get_tick();
                    app->motion_count++;

                    // Alert / tripwire
                    if(app->alert_armed && intensity >= app->alert_threshold
                       && !app->alert_triggered) {
                        app->alert_triggered = true;
                        app->alert_ts        = furi_get_tick();
                        notification_message(app->notifications, &sequence_blink_red_100);
                        char detail[16];
                        snprintf(detail, sizeof(detail), "intensity=%d", intensity);
                        csight_log_event(app, "ALERT", detail);
                    } else {
                        notification_message(app->notifications, &sequence_single_vibro);
                    }
                    break;
                }

                case PROTO_WATERFALL_EVT: {
                    // [0x03][count][bin0..binN][chk]
                    if(i >= len) break;
                    uint8_t count = buf[i++];
                    if(i + count + 1 > len) break; // +1 for checksum

                    // Verify checksum over header + count byte + all bins
                    uint8_t chk = PROTO_WATERFALL_EVT ^ count;
                    for(uint8_t j = 0; j < count; j++) chk ^= buf[i + j];
                    if(chk != buf[i + count]) {
                        FURI_LOG_W(TAG, "Waterfall checksum mismatch, discarding");
                        i += count + 1;
                        break;
                    }

                    // Compress count bins → WATERFALL_HEIGHT/4 pixels
                    uint8_t col[WATERFALL_HEIGHT / 4];
                    memset(col, 0, sizeof(col));
                    int bins_per_pixel = count / (WATERFALL_HEIGHT / 4);
                    if(bins_per_pixel < 1) bins_per_pixel = 1;

                    for(int p = 0; p < WATERFALL_HEIGHT / 4; p++) {
                        uint32_t sum = 0;
                        for(int b = 0; b < bins_per_pixel; b++) {
                            int idx = p * bins_per_pixel + b;
                            if(idx < (int)count) sum += buf[i + idx];
                        }
                        col[p] = (uint8_t)(sum / (uint32_t)bins_per_pixel);
                    }

                    memcpy(app->waterfall.cols[app->wf_write_col], col, sizeof(col));
                    app->wf_write_col = (app->wf_write_col + 1) % WATERFALL_COLS;

                    i += count + 1; // bins + checksum
                    break;
                }

                case PROTO_VITALS_EVT: {
                    // [0x04][breathing_bpm][heart_bpm][chk]
                    if(i + 3 > len) break;
                    uint8_t breathing = buf[i++];
                    uint8_t heart     = buf[i++];
                    uint8_t chk       = buf[i++];
                    if((PROTO_VITALS_EVT ^ breathing ^ heart) != chk) {
                        FURI_LOG_W(TAG, "Vitals checksum mismatch, discarding");
                        break;
                    }
                    app->breathing_bpm = breathing;
                    app->heart_bpm     = heart;
                    app->vitals_valid  = true;

                    // Only log genuinely valid readings (0 = no valid reading that window)
                    if(breathing > 0) {
                        char detail[32];
                        snprintf(detail, sizeof(detail), "breath=%d heart=%d", breathing, heart);
                        csight_log_event(app, "VITALS", detail);
                    }
                    break;
                }

                // ESP32 → Flipper: confirms which channel was auto-selected
                case PROTO_CHANNEL_SET: {
                    if(i + 1 > len) break;
                    uint8_t ch = buf[i++];
                    if(ch >= 1 && ch <= 13) {
                        app->wifi_channel = ch;
                        FURI_LOG_I(TAG, "ESP32 auto-selected CH %d", ch);
                    }
                    break;
                }

                // Primary → Flipper: aggregated multi-node readings + position estimate
                // [0x05][node_count][has_est][est_x_lo][est_x_hi][est_y_lo][est_y_hi]
                //       [node_id][intensity][proximity]*node_count [chk]
                // Primary → Flipper: a brand-new secondary node just paired
                case PROTO_NODE_FOUND: {
                    if(i + 2 > len) break;
                    uint8_t node_id = buf[i];
                    uint8_t chk     = buf[i + 1];
                    if((PROTO_NODE_FOUND ^ node_id) != chk) {
                        FURI_LOG_W(TAG, "Node-found checksum mismatch, discarding");
                        i += 2;
                        break;
                    }
                    i += 2;

                    app->node_found_pending = true;
                    app->node_found_id      = node_id;
                    app->node_found_ts      = furi_get_tick();
                    notification_message(app->notifications, &sequence_success);
                    FURI_LOG_I(TAG, "New node discovered: %d", node_id);
                    char detail[16];
                    snprintf(detail, sizeof(detail), "node_id=%d", node_id);
                    csight_log_event(app, "NODE_FOUND", detail);
                    break;
                }

                case PROTO_MESH_EVT: {
                    if(i >= len) break;
                    uint8_t node_count = buf[i];
                    size_t needed = 6 + (size_t)node_count * 3 + 1; // header + nodes + chk
                    if(i + needed > len) break;

                    size_t start = i;  // position of node_count byte (cmd already consumed)

                    uint8_t has_est = buf[i + 1];
                    int16_t est_x   = (int16_t)(buf[i + 2] | (buf[i + 3] << 8));
                    int16_t est_y   = (int16_t)(buf[i + 4] | (buf[i + 5] << 8));
                    i += 6; // past the 6-byte header

                    // Node data spans [i, i + node_count*3)
                    size_t node_data_end = i + (size_t)node_count * 3;
                    size_t chk_pos       = node_data_end; // checksum byte position

                    // Verify checksum: cmd byte (seed) XOR all bytes from start to node_data_end-1
                    uint8_t chk = PROTO_MESH_EVT;
                    for(size_t j = start; j < node_data_end; j++) chk ^= buf[j];
                    if(chk != buf[chk_pos]) {
                        FURI_LOG_W(TAG, "Mesh checksum mismatch, discarding");
                        i = chk_pos + 1;
                        break;
                    }

                    // Clear all node states, then repopulate from packet
                    for(int n = 0; n < MESH_MAX_NODES; n++) app->mesh_node_active[n] = false;

                    for(uint8_t j = 0; j < node_count; j++) {
                        uint8_t node_id   = buf[i++];
                        uint8_t intensity = buf[i++];
                        uint8_t proximity = buf[i++];
                        if(node_id < MESH_MAX_NODES) {
                            app->mesh_node_active[node_id]    = true;
                            app->mesh_node_intensity[node_id] = intensity;
                            app->mesh_node_proximity[node_id] = proximity;
                            app->mesh_node_last_seen_tick[node_id] = furi_get_tick();
                        }
                    }

                    i++; // skip verified checksum byte
                    app->mesh_has_estimate = (has_est == 1);
                    app->mesh_est_x_cm     = est_x;
                    app->mesh_est_y_cm     = est_y;
                    if(app->mesh_has_estimate) {
                        csight_heatmap_add(app, est_x, est_y);
                    }
                    break;
                }

                default:
                    // Unknown byte — skip and keep parsing
                    break;
            }
        }
    }
    return 0;
}

// ─── Serial RX callback → push to stream buffer ──────────────────────────────
static void serial_rx_cb(FuriHalSerialHandle* handle,
                          FuriHalSerialRxEvent event,
                          void* ctx) {
    UNUSED(handle);
    CSIghtApp* app = (CSIghtApp*)ctx;
    if(event == FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(app->rx_stream, &byte, 1, 0);
    }
}

// ─── Init / Deinit ────────────────────────────────────────────────────────────
void csight_uart_init(CSIghtApp* app) {
    app->rx_stream = furi_stream_buffer_alloc(CSIGHT_RX_BUF, 1);

    app->serial = furi_hal_serial_control_acquire(CSIGHT_UART_CH);
    furi_assert(app->serial);
    furi_hal_serial_init(app->serial, CSIGHT_UART_BAUD);
    furi_hal_serial_async_rx_start(app->serial, serial_rx_cb, app, false);

    app->uart_thread = furi_thread_alloc_ex("csight_rx", 1024, uart_rx_thread, app);
    furi_thread_start(app->uart_thread);
}

void csight_uart_deinit(CSIghtApp* app) {
    // Signal thread to stop cleanly
    furi_thread_flags_set(furi_thread_get_id(app->uart_thread), THREAD_STOP_FLAG);
    furi_thread_join(app->uart_thread);
    furi_thread_free(app->uart_thread);
    app->uart_thread = NULL;

    furi_hal_serial_async_rx_stop(app->serial);
    furi_hal_serial_deinit(app->serial);
    furi_hal_serial_control_release(app->serial);
    app->serial = NULL;

    furi_stream_buffer_free(app->rx_stream);
    app->rx_stream = NULL;
}

// ─── Send helpers ─────────────────────────────────────────────────────────────
void csight_uart_send(CSIghtApp* app, const uint8_t* data, size_t len) {
    furi_hal_serial_tx(app->serial, data, len);
}

void csight_send_handshake(CSIghtApp* app) {
    uint8_t pkt = PROTO_HANDSHAKE_REQ;
    csight_uart_send(app, &pkt, 1);
}

void csight_send_sensitivity(CSIghtApp* app) {
    uint8_t pkt[2] = { PROTO_SENSITIVITY, app->sensitivity };
    csight_uart_send(app, pkt, 2);
}

void csight_send_webui_toggle(CSIghtApp* app) {
    uint8_t pkt = app->web_ui_active ? PROTO_WEBUI_OFF : PROTO_WEBUI_ON;
    csight_uart_send(app, &pkt, 1);
    app->web_ui_active = !app->web_ui_active;
}

void csight_send_mode(CSIghtApp* app) {
    uint8_t mode = (app->display_mode == DisplayModeWaterfall) ? 1 :
                   (app->display_mode == DisplayModeProximity) ? 2 :
                   (app->display_mode == DisplayModeVitals)    ? 3 : 0;
    uint8_t pkt[2] = { PROTO_MODE_SET, mode };
    csight_uart_send(app, pkt, 2);
}

void csight_send_calibrate(CSIghtApp* app) {
    uint8_t pkt = PROTO_CALIBRATE;
    csight_uart_send(app, &pkt, 1);
}

void csight_send_channel(CSIghtApp* app) {
    uint8_t pkt[2] = { PROTO_CHANNEL_SET, app->wifi_channel };
    csight_uart_send(app, pkt, 2);
}

void csight_send_channel_auto(CSIghtApp* app) {
    uint8_t pkt = PROTO_CHANNEL_AUTO;
    csight_uart_send(app, &pkt, 1);
    // wifi_channel will be updated when ESP32 replies with PROTO_CHANNEL_SET
}

void csight_send_forget_nodes(CSIghtApp* app) {
    uint8_t pkt = PROTO_FORGET_NODES;
    csight_uart_send(app, &pkt, 1);
    // Clear locally too so the map reflects it immediately rather than
    // waiting for the next mesh report (nodes will just show inactive)
    for(int n = 1; n < MESH_MAX_NODES; n++) app->mesh_node_active[n] = false;
}

void csight_send_node_positions(CSIghtApp* app) {
    // [PROTO_NODE_POSITIONS][x1][y1][x2][y2][x3][y3] — int16 cm each, no checksum
    uint8_t pkt[13];
    pkt[0] = PROTO_NODE_POSITIONS;
    int p = 1;
    for(int n = 1; n <= 3; n++) {
        int16_t x = app->mesh_node_x_cm[n];
        int16_t y = app->mesh_node_y_cm[n];
        pkt[p++] = (uint8_t)(x & 0xFF);
        pkt[p++] = (uint8_t)((x >> 8) & 0xFF);
        pkt[p++] = (uint8_t)(y & 0xFF);
        pkt[p++] = (uint8_t)((y >> 8) & 0xFF);
    }
    csight_uart_send(app, pkt, sizeof(pkt));
}

void csight_send_pathloss_gamma(CSIghtApp* app) {
    uint8_t pkt[2] = { PROTO_PATHLOSS_GAMMA, app->pathloss_gamma_x10 };
    csight_uart_send(app, pkt, 2);
}
