#include "csight.h"
#include <storage/storage.h>
#include <furi_hal_serial.h>
#include <furi_hal_gpio.h>

#define FLASH_TAG "CSIght_Flash"

// ─── Flash addresses per chip ─────────────────────────────────────────────────
// ESP32 classic bootloader lives at 0x1000; all newer chips use 0x0000
typedef struct {
    const char* chip_name;   // matches chip_name from handshake
    const char* folder;      // subfolder under /ext/apps_data/csight/firmware/
    uint32_t    boot_addr;
    uint32_t    part_addr;
    uint32_t    fw_addr;
} ChipFlashInfo;

static const ChipFlashInfo CHIP_FLASH_INFO[] = {
    { "ESP32",     "esp32",   0x1000, 0x8000, 0x10000 },
    { "ESP32-S2",  "esp32s2", 0x1000, 0x8000, 0x10000 },
    { "ESP32-S3",  "esp32s3", 0x0000, 0x8000, 0x10000 },
    { "ESP32-C3",  "esp32c3", 0x0000, 0x8000, 0x10000 },
    { "ESP32-C6",  "esp32c6", 0x0000, 0x8000, 0x10000 },
    { "ESP32-C61", "esp32c6", 0x0000, 0x8000, 0x10000 }, // shares C6 firmware
    { "ESP32-C5",  "esp32c5", 0x0000, 0x8000, 0x10000 },
};
#define CHIP_FLASH_COUNT (int)(sizeof(CHIP_FLASH_INFO) / sizeof(CHIP_FLASH_INFO[0]))

// ─── SLIP framing constants ───────────────────────────────────────────────────
#define SLIP_END     0xC0
#define SLIP_ESC     0xDB
#define SLIP_ESC_END 0xDC
#define SLIP_ESC_ESC 0xDD

// ─── ESP ROM bootloader commands ─────────────────────────────────────────────
#define CMD_SYNC        0x08
#define CMD_FLASH_BEGIN 0x02
#define CMD_FLASH_DATA  0x03
#define CMD_FLASH_END   0x04

#define FLASH_BLOCK_SZ  0x1000  // 4 KB — ROM bootloader block size
#define FLASH_BAUD      115200
#define SYNC_TIMEOUT_MS 500
#define CMD_TIMEOUT_MS  3000

#define FIRMWARE_BASE_PATH EXT_PATH("apps_data/csight/firmware")

// ─── UART primitives (uses Flipper serial HAL directly) ──────────────────────

static FuriHalSerialHandle* s_serial = NULL;

// Receive up to `len` bytes with `timeout_ms`. Returns bytes received.
static size_t slip_uart_read(uint8_t* buf, size_t len, uint32_t timeout_ms) {
    size_t got = 0;
    uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(timeout_ms);
    while(got < len && furi_get_tick() < deadline) {
        if(furi_hal_serial_async_rx_available(s_serial)) {
            buf[got++] = furi_hal_serial_async_rx(s_serial);
        } else {
            furi_delay_ms(1);
        }
    }
    return got;
}

static void slip_uart_write(const uint8_t* data, size_t len) {
    furi_hal_serial_tx(s_serial, data, len);
}

// ─── SLIP encode and send a command frame ────────────────────────────────────
// Frame: [0xC0][dir=0x00][cmd][len_lo][len_hi][chk_b0..b3][data...][0xC0]
// FLASH_DATA checksum = XOR of data bytes, seeded with 0xEF
// All other commands: checksum = 0
static void slip_send_cmd(uint8_t cmd, const uint8_t* data, uint16_t data_len, uint32_t chk) {
    // Build raw frame (before SLIP encoding)
    uint8_t header[8];
    header[0] = 0x00;        // direction: request
    header[1] = cmd;
    header[2] = (uint8_t)(data_len & 0xFF);
    header[3] = (uint8_t)(data_len >> 8);
    header[4] = (uint8_t)(chk & 0xFF);
    header[5] = (uint8_t)((chk >> 8) & 0xFF);
    header[6] = (uint8_t)((chk >> 16) & 0xFF);
    header[7] = (uint8_t)((chk >> 24) & 0xFF);

    // SLIP-encode and send: start with END
    uint8_t slip_end = SLIP_END;
    slip_uart_write(&slip_end, 1);

    // SLIP-encode header
    for(int i = 0; i < 8; i++) {
        if(header[i] == SLIP_END) {
            uint8_t esc[2] = { SLIP_ESC, SLIP_ESC_END };
            slip_uart_write(esc, 2);
        } else if(header[i] == SLIP_ESC) {
            uint8_t esc[2] = { SLIP_ESC, SLIP_ESC_ESC };
            slip_uart_write(esc, 2);
        } else {
            slip_uart_write(&header[i], 1);
        }
    }

    // SLIP-encode data payload
    for(uint16_t i = 0; i < data_len; i++) {
        if(data[i] == SLIP_END) {
            uint8_t esc[2] = { SLIP_ESC, SLIP_ESC_END };
            slip_uart_write(esc, 2);
        } else if(data[i] == SLIP_ESC) {
            uint8_t esc[2] = { SLIP_ESC, SLIP_ESC_ESC };
            slip_uart_write(esc, 2);
        } else {
            slip_uart_write(&data[i], 1);
        }
    }

    slip_uart_write(&slip_end, 1);  // end
}

// ─── Receive and decode one SLIP response frame ───────────────────────────────
// Returns true on success, false on timeout or framing error.
// Fills resp_buf (caller provides, size resp_buf_sz), sets *resp_len.
static bool slip_recv_resp(uint8_t cmd, uint8_t* resp_buf, size_t resp_buf_sz,
                           size_t* resp_len, uint32_t timeout_ms) {
    UNUSED(cmd);
    uint32_t deadline = furi_get_tick() + furi_ms_to_ticks(timeout_ms);
    *resp_len = 0;

    // Wait for first SLIP_END (start of frame)
    while(furi_get_tick() < deadline) {
        uint8_t b;
        if(slip_uart_read(&b, 1, 10) == 0) continue;
        if(b == SLIP_END) break;
    }
    if(furi_get_tick() >= deadline) return false;

    // Decode frame until next SLIP_END
    bool esc = false;
    while(furi_get_tick() < deadline) {
        uint8_t b;
        if(slip_uart_read(&b, 1, 10) == 0) continue;

        if(b == SLIP_END) break;  // end of frame

        if(esc) {
            esc = false;
            b = (b == SLIP_ESC_END) ? SLIP_END : SLIP_ESC;
        } else if(b == SLIP_ESC) {
            esc = true;
            continue;
        }

        if(*resp_len < resp_buf_sz) resp_buf[(*resp_len)++] = b;
    }

    // Minimum valid response: 8 bytes header
    if(*resp_len < 8) return false;

    // Check direction byte (must be 0x01 = response) and status
    if(resp_buf[0] != 0x01) return false;
    uint8_t status = (*resp_len >= 9) ? resp_buf[8] : 0xFF;
    return (status == 0x00);
}

// ─── High-level commands ─────────────────────────────────────────────────────

// SYNC: 36-byte payload, retry up to `retries` times
static bool flash_sync(int retries) {
    uint8_t sync_payload[36];
    sync_payload[0] = 0x07;
    sync_payload[1] = 0x07;
    sync_payload[2] = 0x12;
    sync_payload[3] = 0x20;
    memset(&sync_payload[4], 0x55, 32);

    uint8_t resp[64];
    size_t  resp_len;

    for(int i = 0; i < retries; i++) {
        slip_send_cmd(CMD_SYNC, sync_payload, sizeof(sync_payload), 0);
        if(slip_recv_resp(CMD_SYNC, resp, sizeof(resp), &resp_len, SYNC_TIMEOUT_MS)) {
            return true;
        }
    }
    return false;
}

// FLASH_BEGIN: erase and set up flash write operation
static bool flash_begin(uint32_t total_size, uint32_t offset) {
    uint32_t num_blocks = (total_size + FLASH_BLOCK_SZ - 1) / FLASH_BLOCK_SZ;
    // Erase size must be sector-aligned (sector = 0x1000 = 4 KB)
    uint32_t erase_size = num_blocks * FLASH_BLOCK_SZ;

    uint8_t payload[16];
    memcpy(&payload[0],  &erase_size,   4);
    memcpy(&payload[4],  &num_blocks,   4);
    uint32_t block_sz = FLASH_BLOCK_SZ;
    memcpy(&payload[8],  &block_sz,     4);
    memcpy(&payload[12], &offset,       4);

    uint8_t resp[64]; size_t resp_len;
    slip_send_cmd(CMD_FLASH_BEGIN, payload, sizeof(payload), 0);
    return slip_recv_resp(CMD_FLASH_BEGIN, resp, sizeof(resp), &resp_len, CMD_TIMEOUT_MS);
}

// FLASH_DATA: send one data block
static bool flash_data_block(const uint8_t* data, uint32_t data_len, uint32_t seq) {
    // Pad block to FLASH_BLOCK_SZ
    static uint8_t block[FLASH_BLOCK_SZ + 16];
    memset(block, 0xFF, sizeof(block));  // 0xFF = unprogrammed flash

    uint32_t pad_len = FLASH_BLOCK_SZ;
    memcpy(&block[0],  &pad_len,  4);
    memcpy(&block[4],  &seq,      4);
    uint32_t zero = 0;
    memcpy(&block[8],  &zero,     4);
    memcpy(&block[12], &zero,     4);
    if(data_len > FLASH_BLOCK_SZ) data_len = FLASH_BLOCK_SZ;
    memcpy(&block[16], data,      data_len);

    // Checksum = XOR of data bytes seeded with 0xEF
    uint32_t chk = 0xEF;
    for(uint32_t i = 0; i < FLASH_BLOCK_SZ; i++) chk ^= block[16 + i];

    uint8_t resp[64]; size_t resp_len;
    slip_send_cmd(CMD_FLASH_DATA, block, 16 + FLASH_BLOCK_SZ, chk);
    return slip_recv_resp(CMD_FLASH_DATA, resp, sizeof(resp), &resp_len, CMD_TIMEOUT_MS);
}

// FLASH_END: reboot into firmware
static bool flash_end(void) {
    uint32_t reboot = 0;  // 0 = run firmware after flash
    uint8_t resp[64]; size_t resp_len;
    slip_send_cmd(CMD_FLASH_END, (uint8_t*)&reboot, 4, 0);
    // Response may not arrive if chip immediately reboots — that's fine
    slip_recv_resp(CMD_FLASH_END, resp, sizeof(resp), &resp_len, 500);
    return true;
}

// ─── Flash one binary file to a given address ────────────────────────────────
// Updates *progress_pct incrementally.
static bool flash_binary(const char* path, uint32_t addr,
                         volatile uint8_t* progress_pct, uint8_t base_pct, uint8_t range_pct,
                         char* err_buf, size_t err_sz) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        snprintf(err_buf, err_sz, "File not found:\n%.30s", path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    uint64_t file_size = storage_file_size(file);
    if(file_size == 0) {
        snprintf(err_buf, err_sz, "Empty file");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    if(!flash_begin((uint32_t)file_size, addr)) {
        snprintf(err_buf, err_sz, "FLASH_BEGIN failed\naddr=0x%08lX", addr);
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    static uint8_t read_buf[FLASH_BLOCK_SZ];
    uint32_t seq        = 0;
    uint64_t bytes_done = 0;

    while(bytes_done < file_size) {
        uint16_t chunk = (uint16_t)storage_file_read(file, read_buf, FLASH_BLOCK_SZ);
        if(chunk == 0) break;

        if(!flash_data_block(read_buf, chunk, seq)) {
            snprintf(err_buf, err_sz, "FLASH_DATA failed\nseq=%lu", seq);
            storage_file_close(file);
            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);
            return false;
        }

        bytes_done += chunk;
        seq++;

        uint8_t pct = base_pct + (uint8_t)(((bytes_done * 100) / file_size) * range_pct / 100);
        *progress_pct = pct;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return true;
}

// ─── Flash thread ─────────────────────────────────────────────────────────────
static int32_t flash_thread_fn(void* ctx) {
    CSIghtApp* app = (CSIghtApp*)ctx;

    // Find chip info
    const ChipFlashInfo* info = NULL;
    for(int i = 0; i < CHIP_FLASH_COUNT; i++) {
        if(strcmp(CHIP_FLASH_INFO[i].chip_name, app->chip_name) == 0) {
            info = &CHIP_FLASH_INFO[i];
            break;
        }
    }

    if(!info) {
        snprintf(app->flash_error, sizeof(app->flash_error),
                 "Unknown chip:\n%s", app->chip_name);
        app->flash_state = FlashStateError;
        return 1;
    }

    // Build paths
    char boot_path[96], part_path[96], fw_path[96];
    snprintf(boot_path, sizeof(boot_path), "%s/%s/bootloader.bin", FIRMWARE_BASE_PATH, info->folder);
    snprintf(part_path, sizeof(part_path), "%s/%s/partitions.bin",  FIRMWARE_BASE_PATH, info->folder);
    snprintf(fw_path,   sizeof(fw_path),   "%s/%s/firmware.bin",    FIRMWARE_BASE_PATH, info->folder);

    // Acquire serial for direct byte access
    s_serial = furi_hal_serial_control_acquire(CSIGHT_UART_CH);
    if(!s_serial) {
        snprintf(app->flash_error, sizeof(app->flash_error), "UART busy");
        app->flash_state = FlashStateError;
        return 1;
    }
    furi_hal_serial_init(s_serial, FLASH_BAUD);
    furi_hal_serial_async_rx_start(s_serial, NULL, NULL, false);  // polling mode

    // SYNC with ROM bootloader (send multiple times — standard practice)
    snprintf(app->flash_status, sizeof(app->flash_status), "Syncing...");
    if(!flash_sync(10)) {
        snprintf(app->flash_error, sizeof(app->flash_error),
                 "Sync failed.\nIs BOOT held + RESET pressed?");
        furi_hal_serial_deinit(s_serial);
        furi_hal_serial_control_release(s_serial);
        s_serial = NULL;
        app->flash_state = FlashStateError;
        return 1;
    }

    bool ok = true;

    // Flash bootloader (0–30%)
    snprintf(app->flash_status, sizeof(app->flash_status), "Bootloader...");
    ok = flash_binary(boot_path, info->boot_addr,
                      &app->flash_progress, 0, 30,
                      app->flash_error, sizeof(app->flash_error));

    // Flash partition table (30–45%)
    if(ok) {
        snprintf(app->flash_status, sizeof(app->flash_status), "Partitions...");
        ok = flash_binary(part_path, info->part_addr,
                          &app->flash_progress, 30, 15,
                          app->flash_error, sizeof(app->flash_error));
    }

    // Flash main firmware (45–100%)
    if(ok) {
        snprintf(app->flash_status, sizeof(app->flash_status), "Firmware...");
        ok = flash_binary(fw_path, info->fw_addr,
                          &app->flash_progress, 45, 55,
                          app->flash_error, sizeof(app->flash_error));
    }

    if(ok) {
        flash_end();
        app->flash_progress = 100;
        app->flash_state    = FlashStateDone;
    } else {
        app->flash_state = FlashStateError;
    }

    furi_hal_serial_async_rx_stop(s_serial);
    furi_hal_serial_deinit(s_serial);
    furi_hal_serial_control_release(s_serial);
    s_serial = NULL;
    return ok ? 0 : 1;
}

// ─── Public entry point ───────────────────────────────────────────────────────
// Called from handle_input when user confirms bootloader mode is active.
static void flash_thread_cleanup(FuriThread* thread, FuriThreadState state, void* context) {
    UNUSED(context);
    if(state == FuriThreadStateStopped) {
        furi_thread_free(thread);
    }
}

void csight_flash_start(CSIghtApp* app) {
    if(app->serial != NULL) csight_uart_deinit(app);

    app->flash_progress = 0;
    app->flash_state    = FlashStateFlashing;
    memset(app->flash_status, 0, sizeof(app->flash_status));
    memset(app->flash_error,  0, sizeof(app->flash_error));

    FuriThread* t = furi_thread_alloc_ex("csight_flash", 4096, flash_thread_fn, app);
    furi_thread_set_state_callback(t, flash_thread_cleanup);
    furi_thread_start(t);
    // Cleaned up in flash_thread_cleanup when thread stops
}

// Check if firmware files exist for the connected chip
bool csight_flash_files_exist(CSIghtApp* app) {
    for(int i = 0; i < CHIP_FLASH_COUNT; i++) {
        if(strcmp(CHIP_FLASH_INFO[i].chip_name, app->chip_name) == 0) {
            char fw_path[96];
            snprintf(fw_path, sizeof(fw_path), "%s/%s/firmware.bin",
                     FIRMWARE_BASE_PATH, CHIP_FLASH_INFO[i].folder);
            Storage* storage = furi_record_open(RECORD_STORAGE);
            bool exists = storage_file_exists(storage, fw_path);
            furi_record_close(RECORD_STORAGE);
            return exists;
        }
    }
    return false;
}
