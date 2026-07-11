#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MESH_MAX_NODES  4   // 1 primary + up to 3 secondaries

typedef enum {
    MeshRolePrimary   = 0,  // wired to Flipper, aggregates all node data
    MeshRoleSecondary = 1,  // standalone, broadcasts readings via ESP-NOW
} MeshRole;

// One node's latest reading, as tracked by the primary
typedef struct {
    bool     active;        // true if we've heard from this node recently
    uint8_t  node_id;       // 0 = primary itself, 1-3 = secondaries
    uint8_t  intensity;     // last motion intensity (0-255)
    uint8_t  proximity;     // last proximity estimate (0-100)
    uint32_t last_seen_ms;  // for staleness detection
} MeshNodeReading;

// Fixed physical position of a node, in centimetres from an arbitrary origin.
// Entered by the user on the Flipper — the firmware doesn't know real coordinates.
typedef struct {
    int16_t x_cm;
    int16_t y_cm;
} MeshNodePos;

// Callback fired on the primary when a brand-new MAC completes the HELLO
// handshake and is assigned a node_id for the first time. Lets main.c forward
// a "new node discovered" event to the Flipper.
typedef void (*mesh_discovery_cb_t)(uint8_t node_id);

// ─── Primary ──────────────────────────────────────────────────────────────────

// Starts ESP-NOW receive, loads the known-MAC table from NVS, and begins
// listening for both HELLO handshakes and normal readings from secondaries.
void mesh_init_primary(void);

// Register a callback fired whenever a new (never-seen) secondary is assigned
// a node_id for the first time. Optional — pass NULL to ignore.
void mesh_set_discovery_cb(mesh_discovery_cb_t cb);

// Record the primary's own local reading into the aggregation table (node_id 0).
void mesh_set_local_reading(uint8_t intensity, uint8_t proximity);

// Fetch current snapshot of all known node readings (including local node 0).
// Returns number of active nodes written into out[].
uint8_t mesh_get_readings(MeshNodeReading* out, uint8_t max_count);

// Estimate target position via iterative distance-based trilateration (v2.3).
// Each node's proximity score is converted to an estimated distance using an
// inverse power-law curve, then the position is refined to be consistent with
// all reported distances — a real improvement over pure intensity-weighting,
// though still an approximation without per-environment calibration.
// positions[] must be indexed by node_id (0-3). Returns false if fewer than
// 2 active nodes (not enough to estimate anything meaningful).
bool mesh_estimate_position(const MeshNodePos* positions, int16_t* out_x_cm, int16_t* out_y_cm);

// Clear the primary's remembered MAC→ID table (forgets all paired secondaries).
// Call this if you want to re-pair nodes from scratch, e.g. after swapping hardware.
void mesh_forget_all_nodes(void);

// Adjusts the path-loss exponent used in proximity-to-distance conversion
// (see mesh_estimate_position). 1.0-5.0 range; default 2.0 approximates
// free-space path loss. Denser/multipath-heavy rooms may estimate better
// with a higher value — this is empirical, not something the firmware can
// determine on its own.
void mesh_set_pathloss_gamma(float gamma);
float mesh_get_pathloss_gamma(void);

// ─── Secondary ────────────────────────────────────────────────────────────────

// Starts ESP-NOW, then repeatedly broadcasts a HELLO until the primary assigns
// this device a node_id. No manual ID configuration needed — identity comes
// from the ESP32's own factory-programmed MAC address.
void mesh_init_secondary(void);

// Returns true once this secondary has been assigned an ID by the primary.
// Until then, mesh_broadcast_reading() is a no-op (nothing to tag readings with).
bool mesh_is_paired(void);

// Broadcasts this secondary's own reading, tagged with its assigned node_id.
// Safe to call before pairing completes — silently does nothing until then.
void mesh_broadcast_reading(uint8_t intensity, uint8_t proximity);
