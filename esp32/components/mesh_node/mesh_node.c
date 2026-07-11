#include "mesh_node.h"
#include <string.h>
#include <math.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "mesh_node";

// ─── Wire format ──────────────────────────────────────────────────────────────
// Two packet types share one struct — `type` disambiguates. Kept tiny since
// this all travels over the shared CSI channel and airtime matters.
typedef enum {
    MeshPktHello    = 0,  // secondary -> primary: "I exist, who am I?"
    MeshPktHelloAck = 1,  // primary -> secondary: "you're node X"
    MeshPktReading  = 2,  // secondary -> primary: motion/proximity update
} MeshPktType;

typedef struct __attribute__((packed)) {
    uint8_t magic;      // 0xC5 — sanity check, ignore anything else on air
    uint8_t type;        // MeshPktType
    uint8_t node_id;     // meaningful for HelloAck/Reading, ignored on Hello
    uint8_t intensity;   // only meaningful for Reading
    uint8_t proximity;   // only meaningful for Reading
} MeshPacket;

#define MESH_MAGIC 0xC5
static const uint8_t BROADCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// ─── NVS-backed MAC -> node_id table (primary only) ──────────────────────────
#define NVS_NAMESPACE "csight_mesh"

static MeshRole         s_role = MeshRolePrimary;
static MeshNodeReading  s_readings[MESH_MAX_NODES];   // index = node_id, 0 = primary
static uint8_t          s_known_macs[MESH_MAX_NODES][6]; // index = node_id, 0 unused
static mesh_discovery_cb_t s_discovery_cb = NULL;

// Secondary-side pairing state
static volatile uint8_t s_my_id  = 0;     // 0 = not yet paired
static volatile bool    s_paired = false;

static void nvs_load_mac_table(void) {
    memset(s_known_macs, 0, sizeof(s_known_macs));
    nvs_handle_t h;
    if(nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return;

    size_t len = sizeof(s_known_macs);
    nvs_get_blob(h, "mac_table", s_known_macs, &len);
    nvs_close(h);
}

static void nvs_save_mac_table(void) {
    nvs_handle_t h;
    if(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "mac_table", s_known_macs, sizeof(s_known_macs));
    nvs_commit(h);
    nvs_close(h);
}

static bool mac_is_zero(const uint8_t* mac) {
    for(int i = 0; i < 6; i++) if(mac[i] != 0) return false;
    return true;
}

static bool mac_eq(const uint8_t* a, const uint8_t* b) {
    return memcmp(a, b, 6) == 0;
}

// Look up an existing node_id for this MAC, or assign the next free slot.
// Returns 0 if the mesh is full (no free slots among 1-3).
static uint8_t assign_or_lookup_node_id(const uint8_t* mac, bool* is_new) {
    *is_new = false;
    for(int id = 1; id < MESH_MAX_NODES; id++) {
        if(mac_eq(s_known_macs[id], mac)) return id;
    }
    for(int id = 1; id < MESH_MAX_NODES; id++) {
        if(mac_is_zero(s_known_macs[id])) {
            memcpy(s_known_macs[id], mac, 6);
            nvs_save_mac_table();
            *is_new = true;
            return id;
        }
    }
    return 0; // full
}

// ─── Primary: ESP-NOW receive callback ───────────────────────────────────────
static void mesh_recv_cb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if(len != (int)sizeof(MeshPacket)) return;
    const MeshPacket* pkt = (const MeshPacket*)data;
    if(pkt->magic != MESH_MAGIC) return;

    if(pkt->type == MeshPktHello) {
        bool is_new = false;
        uint8_t id = assign_or_lookup_node_id(info->src_addr, &is_new);
        if(id == 0) {
            ESP_LOGW(TAG, "Mesh full — ignoring HELLO from new node");
            return; // no free slots, ignore silently
        }

        // Reply with an ACK telling this specific MAC its assigned ID
        MeshPacket ack = { .magic = MESH_MAGIC, .type = MeshPktHelloAck, .node_id = id };
        esp_now_peer_info_t peer = { 0 };
        memcpy(peer.peer_addr, info->src_addr, 6);
        peer.channel = 0;
        peer.ifidx   = WIFI_IF_STA;
        if(!esp_now_is_peer_exist(info->src_addr)) esp_now_add_peer(&peer);
        esp_now_send(info->src_addr, (const uint8_t*)&ack, sizeof(ack));

        if(is_new) {
            ESP_LOGI(TAG, "New node paired: id=%d mac=%02x:%02x:%02x:%02x:%02x:%02x",
                     id, info->src_addr[0], info->src_addr[1], info->src_addr[2],
                     info->src_addr[3], info->src_addr[4], info->src_addr[5]);
            if(s_discovery_cb) s_discovery_cb(id);
        }
        return;
    }

    if(pkt->type == MeshPktReading) {
        if(pkt->node_id == 0 || pkt->node_id >= MESH_MAX_NODES) return;
        // Only accept readings from a MAC that matches our recorded pairing —
        // prevents a stale/rogue node_id claim from a different device.
        if(!mac_eq(s_known_macs[pkt->node_id], info->src_addr)) return;

        s_readings[pkt->node_id].active       = true;
        s_readings[pkt->node_id].node_id      = pkt->node_id;
        s_readings[pkt->node_id].intensity    = pkt->intensity;
        s_readings[pkt->node_id].proximity    = pkt->proximity;
        s_readings[pkt->node_id].last_seen_ms = (uint32_t)(esp_timer_get_time() / 1000);
    }
}

// ─── Secondary: ESP-NOW receive callback (only cares about HelloAck) ────────
static void mesh_secondary_recv_cb(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    (void)info;
    if(len != (int)sizeof(MeshPacket)) return;
    const MeshPacket* pkt = (const MeshPacket*)data;
    if(pkt->magic != MESH_MAGIC || pkt->type != MeshPktHelloAck) return;

    if(!s_paired) {
        s_my_id  = pkt->node_id;
        s_paired = true;
        ESP_LOGI(TAG, "Paired! Assigned node_id=%d", s_my_id);
    }
}

// ─── Staleness sweep — marks nodes inactive if not heard from in 3s ──────────
static void mesh_staleness_task(void* arg) {
    (void)arg;
    while(1) {
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        for(int i = 1; i < MESH_MAX_NODES; i++) {
            if(s_readings[i].active && (now_ms - s_readings[i].last_seen_ms) > 3000) {
                s_readings[i].active = false;
                ESP_LOGW(TAG, "Node %d timed out", i);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ─── Secondary: repeatedly broadcast HELLO until paired ─────────────────────
static void mesh_hello_task(void* arg) {
    (void)arg;
    while(!s_paired) {
        MeshPacket hello = { .magic = MESH_MAGIC, .type = MeshPktHello };
        esp_now_send(BROADCAST_MAC, (const uint8_t*)&hello, sizeof(hello));
        vTaskDelay(pdMS_TO_TICKS(2000)); // retry every 2s until an ACK arrives
    }
    ESP_LOGI(TAG, "Hello task done — paired as node %d", s_my_id);
    vTaskDelete(NULL);
}

// ─── Shared ESP-NOW bring-up ──────────────────────────────────────────────────
static void esp_now_bringup(void) {
    ESP_ERROR_CHECK(esp_now_init());

    esp_now_peer_info_t peer = { 0 };
    memcpy(peer.peer_addr, BROADCAST_MAC, 6);
    peer.channel = 0;      // 0 = use current WiFi channel (already set by CSI survey)
    peer.ifidx   = WIFI_IF_STA;
    peer.encrypt = false;

    if(!esp_now_is_peer_exist(BROADCAST_MAC)) {
        esp_err_t err = esp_now_add_peer(&peer);
        if(err != ESP_OK) ESP_LOGE(TAG, "esp_now_add_peer failed: %d", err);
    }
}

// ─── Public API — Primary ────────────────────────────────────────────────────
void mesh_init_primary(void) {
    s_role = MeshRolePrimary;
    memset(s_readings, 0, sizeof(s_readings));
    nvs_load_mac_table();

    esp_now_bringup();
    ESP_ERROR_CHECK(esp_now_register_recv_cb(mesh_recv_cb));

    xTaskCreate(mesh_staleness_task, "mesh_stale", 2048, NULL, 3, NULL);
    ESP_LOGI(TAG, "Mesh primary ready — listening for secondary nodes");
}

void mesh_set_discovery_cb(mesh_discovery_cb_t cb) {
    s_discovery_cb = cb;
}

void mesh_forget_all_nodes(void) {
    memset(s_known_macs, 0, sizeof(s_known_macs));
    memset(s_readings, 0, sizeof(s_readings));
    nvs_save_mac_table();
    ESP_LOGI(TAG, "Forgot all paired nodes");
}

void mesh_set_local_reading(uint8_t intensity, uint8_t proximity) {
    s_readings[0].active       = true;
    s_readings[0].node_id      = 0;
    s_readings[0].intensity    = intensity;
    s_readings[0].proximity    = proximity;
    s_readings[0].last_seen_ms = (uint32_t)(esp_timer_get_time() / 1000);
}

uint8_t mesh_get_readings(MeshNodeReading* out, uint8_t max_count) {
    uint8_t n = 0;
    for(int i = 0; i < MESH_MAX_NODES && n < max_count; i++) {
        if(s_readings[i].active) out[n++] = s_readings[i];
    }
    return n;
}

// ─── Position estimation (v2.3) ──────────────────────────────────────────────
// Converts each node's proximity score (0-100, higher = closer — see
// csi_handler.c's delta_to_proximity) into an estimated distance using an
// inverse power-law curve, matching how RF signal strength falls off with
// distance far better than a linear mapping would. GAMMA=2.0 approximates
// free-space path loss; real indoor environments vary, so treat distances
// as directional, not calibrated measurements.
#define PROX_MAX_RANGE_CM    500.0f  // proximity=0 maps to this distance

// Runtime-adjustable — was a hardcoded #define, now a Flipper Settings item.
// 2.0 approximates free-space path loss; denser/multipath-heavy rooms often
// estimate better with something in the 2.5-4.0 range. See mesh_set_pathloss_gamma().
static float s_pathloss_gamma = 2.0f;

void mesh_set_pathloss_gamma(float gamma) {
    if(gamma < 1.0f) gamma = 1.0f;   // guard against nonsensical/negative values
    if(gamma > 5.0f) gamma = 5.0f;
    s_pathloss_gamma = gamma;
}

float mesh_get_pathloss_gamma(void) {
    return s_pathloss_gamma;
}
#define TRILAT_ITERATIONS    6
#define TRILAT_STEP          0.35f   // fraction of error corrected per iteration

static float proximity_to_distance_cm(uint8_t proximity) {
    float p = proximity / 100.0f;
    if(p > 1.0f) p = 1.0f;
    if(p < 0.0f) p = 0.0f;
    float frac = 1.0f - p; // 0 = right on top of the node, 1 = at max range
    return PROX_MAX_RANGE_CM * powf(frac, s_pathloss_gamma);
}

bool mesh_estimate_position(const MeshNodePos* positions, int16_t* out_x_cm, int16_t* out_y_cm) {
    MeshNodeReading readings[MESH_MAX_NODES];
    uint8_t n = mesh_get_readings(readings, MESH_MAX_NODES);
    if(n < 2) return false;

    float dist_cm[MESH_MAX_NODES]  = { 0 };
    bool  have_dist[MESH_MAX_NODES] = { false };

    // Step 1: initial guess via intensity-weighted centroid — gives the
    // iterative refinement below a reasonable starting point, especially
    // valuable with only 2 active nodes where a true fix is under-determined.
    float wx = 0, wy = 0, wsum = 0;
    for(uint8_t i = 0; i < n; i++) {
        uint8_t id = readings[i].node_id;
        float w = (float)readings[i].intensity + 1.0f;
        wx   += (float)positions[id].x_cm * w;
        wy   += (float)positions[id].y_cm * w;
        wsum += w;

        dist_cm[id]   = proximity_to_distance_cm(readings[i].proximity);
        have_dist[id] = true;
    }
    if(wsum <= 0.0f) return false;

    float ex = wx / wsum;
    float ey = wy / wsum;

    // Step 2: iteratively nudge the estimate so its distance to each node
    // matches that node's proximity-derived distance. This is a simplified,
    // matrix-free form of multilateration (similar in spirit to one step of
    // Gauss-Newton per node, repeated) — appropriate for an embedded target
    // with only 2-4 anchors and no linear algebra library.
    for(int iter = 0; iter < TRILAT_ITERATIONS; iter++) {
        for(uint8_t i = 0; i < n; i++) {
            uint8_t id = readings[i].node_id;
            if(!have_dist[id]) continue;

            float dx = ex - (float)positions[id].x_cm;
            float dy = ey - (float)positions[id].y_cm;
            float cur_d = sqrtf(dx * dx + dy * dy);
            if(cur_d < 1.0f) cur_d = 1.0f; // guard divide-by-zero if estimate lands on a node

            float error = dist_cm[id] - cur_d; // +ve = estimate needs to move further from this node
            float ux = dx / cur_d;
            float uy = dy / cur_d;

            ex += ux * error * TRILAT_STEP;
            ey += uy * error * TRILAT_STEP;
        }
    }

    *out_x_cm = (int16_t)ex;
    *out_y_cm = (int16_t)ey;
    return true;
}

// ─── Public API — Secondary ──────────────────────────────────────────────────
void mesh_init_secondary(void) {
    s_role   = MeshRoleSecondary;
    s_paired = false;
    s_my_id  = 0;

    esp_now_bringup();
    ESP_ERROR_CHECK(esp_now_register_recv_cb(mesh_secondary_recv_cb));

    xTaskCreate(mesh_hello_task, "mesh_hello", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Mesh secondary ready — broadcasting HELLO until paired");
}

bool mesh_is_paired(void) {
    return s_paired;
}

void mesh_broadcast_reading(uint8_t intensity, uint8_t proximity) {
    if(s_role != MeshRoleSecondary || !s_paired) return;

    MeshPacket pkt = {
        .magic     = MESH_MAGIC,
        .type      = MeshPktReading,
        .node_id   = s_my_id,
        .intensity = intensity,
        .proximity = proximity,
    };
    esp_err_t err = esp_now_send(BROADCAST_MAC, (const uint8_t*)&pkt, sizeof(pkt));
    if(err != ESP_OK) ESP_LOGW(TAG, "Broadcast failed: %d", err);
}
