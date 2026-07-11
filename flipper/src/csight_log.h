#pragma once
#include "csight.h"

// Ensures the logs directory and CSV file exist, writing a header row if new.
// Call once at app startup — cheap no-op if the file already exists.
void csight_log_init(CSIghtApp* app);

// Appends one row: <ISO timestamp>,<event>,<detail>
// Opens, writes, and closes the file each call rather than holding a handle
// open for the app's lifetime — these events are infrequent (not per-frame),
// so the overhead is negligible, and it means a crash or pulled SD card can't
// leave a corrupted/unflushed log behind.
void csight_log_event(CSIghtApp* app, const char* event, const char* detail);
