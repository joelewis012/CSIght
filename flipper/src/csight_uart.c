#include "csight.h"
#include <furi_hal_serial.h>

#define TAG          "CSIght_UART"
#define THREAD_STOP_FLAG 0x1

// ─── RX thread ────────────────────────────────────────────────────────────────
static int32_t uart_rx_thread(void* ctx) {
    CSIghtApp* app = (CSIghtApp*)ctx;
    uint8_t    buf[CSIGHT_RX_BUF];

    while (true) {
        // Check stop flag (non-blocking)
        uint32_t flags = furi_thread_flags_get();
        furi_thread_flags_clear(flags);
        if (flags & THREAD_STOP_FLAG) break;

        // Block until data arrives — short timeout so stop flag is checked regularly
        size_t len = furi_stream_buffer_receive(
            app->rx_stream, buf, sizeof(buf), furi_ms_to_ticks(50));
        if (len == 0) continue;

        size_t i = 0;
        while (i < len) {
            uint8_t hdr = buf[i++];

            switch (hdr) {

                case PROTO_HANDSHAKE_ACK: {
                    // [0xBB][payload_len][chip_name\0][csi_support][fw_major][fw_minor][chk]
                    if (i >= len) break;
                    uint8_t payload_len = buf[i++];
                    if (i + payload_len > len) break;

                    // null-terminated chip name
                    size_t name_len = strnlen((char*)&buf[i], payload_len);
                    if (name_len >= sizeof(app->chip_name)) name_len = sizeof(app->chip_name) - 1;
                    memcpy(app->chip_name, &buf[i], name_len);
                    app->chip_name[name_len] = '\0';
                    i += name_len + 1;

                    if (i < len) app->csi_support = buf[i++];
                    if (i < len) app->fw_major    = buf[i++];
                    if (i < len) app->fw_minor     = buf[i++];
                    if (i < len) i++; // skip checksum

                    FURI_LOG_I(TAG, "Handshake: %s CSI=%d FW=%d.%d",
                               app->chip_name, app->csi_support,
                               app->fw_major, app->fw_minor);

                    app->state = AppStateCompatCheck;
                    break;
                }

                case PROTO_MOTION_EVT: {
                    // [0x01][intensity][proximity][chk]
                    if (i + 2 >= len) break;
                    uint8_t intensity = buf[i++];
                    uint8_t proximity = buf[i++];
                    if (i < len) i++; // chk

                    app->motion_intensity = intensity;
                    app->proximity        = proximity;

                    csight_add_blip(app, intensity, proximity);

                    app->target_acquired = true;
                    app->target_ts       = furi_get_tick();
                    break;
                }

                case PROTO_WATERFALL_EVT: {
                    // [0x03][count][bin0..binN][chk]
                    if (i >= len) break;
                    uint8_t count = buf[i++];
                    if (i + count >= len) break;

                    uint8_t col[WATERFALL_HEIGHT / 4];
                    memset(col, 0, sizeof(col));
                    uint8_t bins_per_pixel = count / (WATERFALL_HEIGHT / 4);
                    if (bins_per_pixel < 1) bins_per_pixel = 1;
                    for (int p = 0; p < WATERFALL_HEIGHT / 4; p++) {
                        uint32_t sum = 0;
                        for (int b = 0; b < bins_per_pixel; b++) {
                            int idx = p * bins_per_pixel + b;
                            if (idx < count) sum += buf[i + idx];
                        }
                        col[p] = (uint8_t)(sum / bins_per_pixel);
                    }
                    memcpy(app->waterfall.cols[app->wf_write_col], col, sizeof(col));
                    app->wf_write_col = (app->wf_write_col + 1) % WATERFALL_COLS;

                    i += count;
                    if (i < len) i++; // chk
                    break;
                }

                default:
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
    if (event == FuriHalSerialRxEventData) {
        uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(app->rx_stream, &byte, 1, 0);
    }
}

// ─── Public ───────────────────────────────────────────────────────────────────
void csight_uart_init(CSIghtApp* app) {
    app->rx_stream = furi_stream_buffer_alloc(CSIGHT_RX_BUF, 1);

    app->serial = furi_hal_serial_control_acquire(CSIGHT_UART_CH);
    furi_assert(app->serial);
    furi_hal_serial_init(app->serial, CSIGHT_UART_BAUD);
    furi_hal_serial_async_rx_start(app->serial, serial_rx_cb, app, false);

    app->uart_thread = furi_thread_alloc_ex(
        "csight_rx", 1024, uart_rx_thread, app);
    furi_thread_start(app->uart_thread);
}

void csight_uart_deinit(CSIghtApp* app) {
    // Signal thread to stop then wait for clean exit
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

void csight_send_mode(CSIghtApp* app) {
    uint8_t mode = (app->display_mode == DisplayModeWaterfall) ? 1 :
                   (app->display_mode == DisplayModeProximity) ? 2 : 0;
    uint8_t pkt[2] = { PROTO_MODE_SET, mode };
    csight_uart_send(app, pkt, 2);
}
