#include "csight_log.h"
#include <storage/storage.h>
#include <furi_hal_rtc.h>

#define LOG_DIR  EXT_PATH("apps_data/csight/logs")
#define LOG_PATH EXT_PATH("apps_data/csight/logs/events.csv")

static void format_timestamp(char* out, size_t out_size) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(out, out_size, "%04d-%02d-%02d %02d:%02d:%02d",
             dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
}

void csight_log_init(CSIghtApp* app) {
    if(!app->log_enabled) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure the logs directory exists — storage_common_mkdir is a no-op
    // (returns false harmlessly) if it's already there.
    storage_common_mkdir(storage, LOG_DIR);

    // Only write the header if the file doesn't already exist, so repeated
    // app launches append to the same running log rather than truncating it.
    if(!storage_file_exists(storage, LOG_PATH)) {
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, LOG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* header = "timestamp,event,detail\n";
            storage_file_write(file, header, strlen(header));
        }
        storage_file_close(file);
        storage_file_free(file);
    }

    furi_record_close(RECORD_STORAGE);
}

void csight_log_event(CSIghtApp* app, const char* event, const char* detail) {
    if(!app->log_enabled) return;

    char ts[24];
    format_timestamp(ts, sizeof(ts));

    char line[128];
    int len = snprintf(line, sizeof(line), "%s,%s,%s\n", ts, event, detail ? detail : "");
    if(len <= 0) return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    // Append mode — FSOM_OPEN_APPEND creates the file if missing too, so this
    // is safe even if csight_log_init() somehow didn't run first.
    if(storage_file_open(file, LOG_PATH, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        storage_file_write(file, line, (size_t)len);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
