<div align="center">

<img src="csight_logo.svg" alt="CSIght" width="700"/>

<br/>

**See through walls. No cameras. No IR. Just WiFi.**

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![Releases](https://img.shields.io/badge/Releases-Latest-green?style=flat-square)](https://github.com/joelewis012/CSIght/releases/latest)
[![Stars](https://img.shields.io/badge/Stars-⭐_GitHub-yellow?style=flat-square)](https://github.com/joelewis012/CSIght/stargazers)
[![Firmware: Official](https://img.shields.io/badge/firmware-Official-blue?style=flat-square)]()
[![Firmware: Momentum](https://img.shields.io/badge/firmware-Momentum-purple?style=flat-square)]()
[![Firmware: Unleashed](https://img.shields.io/badge/firmware-Unleashed-red?style=flat-square)]()
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2%2B-orange?style=flat-square)]()
[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-support-yellow?style=flat-square&logo=buy-me-a-coffee)](https://buymeacoffee.com/Joelewis012)

### [⬇ Download v3.3](https://github.com/joelewis012/CSIght/releases/latest) &nbsp;·&nbsp; [☕ Buy Me a Coffee](https://buymeacoffee.com/Joelewis012)

</div>

---

## What is CSIght?

Every WiFi packet that moves through a room carries hidden data about that room — the amplitude, phase, and propagation delay across dozens of frequency subcarriers. When something moves, those values shift. CSIght reads those shifts in real time using your ESP32's WiFi hardware and renders them on your Flipper Zero as a live radar display.

No special hardware. No cameras. No IR sensors. Just the WiFi radio already sitting on your ESP32 expansion board.

---

## Display Modes

Switch modes with `←` / `→` during scanning. Four modes available:

**Radar** — rotating sweep with motion blips and TARGET ACQUIRED flash
```
┌──────────────────────────┐
│ CSIght                   │
│  · ╭────╮ ·   MOTION     │
│  · │ ╱  │ ·   ████       │
│  · ╰────╯ ·   RANGE ██   │
│  · · ● · ·    SENS: 7    │
│               ← → mode   │
└──────────────────────────┘
```

**Waterfall** — scrolling CSI amplitude history across all subcarriers
```
┌──────────────────────────┐
│ CSIght  WATERFALL        │
│ █░░░░░░░░░░░░█░░░░░░░░░░ │
│ █░░░░█░░░░░░░█░░░░░░░░░░ │
│ █░░░░█░░█░░░░█░░░░░░░░░░ │
│ █░░░░█░░█░░█░█░░░░░░░░░░ │
│ ↑↓←→ [OK]cal        S:7 │
└──────────────────────────┘
```

**Proximity** — concentric arcs showing estimated distance
```
┌──────────────────────────┐
│         CSIght           │
│        PROXIMITY         │
│           ○              │
│          ○○○             │
│         ○○○○○            │
│          72%             │
└──────────────────────────┘
```

**Vitals** *(experimental)* — breathing rate and heart rate via CSI signal analysis
```
┌──────────────────────────┐
│ CSIght  VITALS           │
│ BREATHING                │
│   14 BPM  ████████░░     │
│ HEART*                   │
│   72 BPM  █████░░░░░     │
│ *experimental [OK]cal    │
└──────────────────────────┘
Needs ~30s to measure. Stay still.
Subject should be between devices.
```

---

## Features

- **Auto chip detection** — ESP32 reports its model and CSI capability tier on handshake
- **40+ board presets** — official Flipper boards, DevKits, XIAO, Adafruit, SparkFun, M5Stack, LOLIN and more
- **Custom pin override** — set any TX/RX pair, saved to SD card permanently
- **Four display modes** — radar, waterfall, proximity, vitals — switchable live with `←`/`→`
- **Sensitivity control** — `↑`/`↓` during scanning, no menu needed
- **Recalibrate on the fly** — press `OK` during scanning to force a fresh baseline
- **Vibration alert** — Flipper vibrates on motion detection
- **Vitals mode** — breathing rate (reliable) and heart rate (experimental) via EMA bandpass + zero-crossing
- **Flash firmware** — flash CSIght firmware to your ESP32 directly from the Flipper, no computer needed
- **Web UI** — live radar in any browser over WiFi, no app required
- **Power insomnia** — Flipper won't sleep during a scan session
- **ESP32-C5 support** — the highest-performing CSI chip, now fully supported

---

## Compatibility

### Which firmware folder is mine?

| Folder | Chip | Typical boards |
|--------|------|----------------|
| `esp32/esp32/` | ESP32 (classic) | WROOM-32, NodeMCU, WROVER, M5Stack Core, HUZZAH32, FlipMods Combo, Marauder boards |
| `esp32/esp32s2/` | ESP32-S2 | **Flipper Zero WiFi Dev Board**, Feather S2, QT Py S2, LOLIN S2 Mini, FeatherS2, TinyS2 |
| `esp32/esp32s3/` | ESP32-S3 | S3 DevKitC/M, XIAO-S3, TinyS3, FeatherS3, ProS3, CoreS3, TTGO T-Display S3 |
| `esp32/esp32c3/` | ESP32-C3 | C3 DevKit, XIAO-C3, C3 Super Mini, LOLIN C3 Mini, M5Stamp C3 |
| `esp32/esp32c6/` | ESP32-C6 / C61 | C6 DevKitC-1, C6 DevKitM-1, C61 DevKitC-1, XIAO-C6 |
| `esp32/esp32c5/` | ESP32-C5 | C5 DevKitC-1 *(requires ESP-IDF v5.5.2)* |

> ⚠️ **Marauder firmware users** — boards flashed with Marauder need reflashing before CSIght will work. Marauder does not expose the WiFi CSI API. You can restore Marauder afterwards.

### CSI quality by chip

| Chip | Tier | Notes |
|------|------|-------|
| ESP32-C5 | ✅ Full (best) | Highest-ranked by Espressif. 2.4 + 5 GHz |
| ESP32-C6 | ✅ Full | WiFi 6, excellent sensitivity |
| ESP32-C61 | ✅ Full | C6 without 802.15.4 — same firmware |
| ESP32-C3 | ✅ Full | Great budget option |
| ESP32-S3 | ✅ Full | Recommended all-rounder |
| ESP32-S2 | ⚠️ Limited | Motion works well, less precision than C-series |
| ESP32 (original) | ⚠️ Limited | Amplitude only — motion detection still works |
| ESP32-H2 | ❌ None | No WiFi — Zigbee/Thread only |

> **Full** = amplitude + phase across all subcarriers → best accuracy  
> **Limited** = amplitude only → motion detection still works fine

---

## Controls

| Button | Scanning | Flash setup | Everywhere else |
|--------|----------|-------------|-----------------|
| `↑` / `↓` | Sensitivity up/down | — | Navigate |
| `←` / `→` | Cycle display mode | RX pin adjust | Adjust value |
| `OK` | Force recalibrate | Confirm flash | Select |
| `Back` (short) | Save + return to menu | Cancel | Back |
| `Back` (hold) | — | — | Exit app (main menu only) |

**Main menu:**
- **Start Scanning** — launch radar
- **Web UI** — toggle browser radar `[ON]`/`[OFF]`
- **Flash Firmware** — flash CSIght firmware to your ESP32 from the Flipper
- **Settings** — sensitivity, alert tuning, mesh setup, and more (see below)
- **About** — chip info, firmware version

**Display modes** (cycle with `←`/`→` while scanning): Radar, Waterfall, Proximity, Vitals, Multi-Node Map, Heatmap.

**Settings screen** (v3.3): Sensitivity, WiFi Channel, Alert Level, Re-scan Channels, Configure Nodes, Forget Nodes, SD Logging, Path-loss Exp, **Test Alert** (fires the tripwire sequence on demand so you can verify it works), **Quiet/Busy Preset** (`←` = Quiet Room, `→` = Busy Room — one-tap sensitivity+threshold combos), **Schedule Start/End** (auto-arm tripwire during set hours — leave both at the same value to disable), Change Board.

---

## Installation

### 1. Download

Grab the latest release from the [Releases page](https://github.com/joelewis012/CSIght/releases/latest):

```
CSIght-v3.3/
├── flipper/
│   ├── official/      csight.fap
│   ├── momentum/      csight.fap
│   └── unleashed/     csight.fap
├── esp32/
│   ├── esp32/         bootloader.bin  partitions.bin  firmware.bin
│   ├── esp32s2/       "
│   ├── esp32s3/       "
│   ├── esp32c3/       "
│   ├── esp32c6/       "
│   └── esp32c5/       "  ← best CSI performance
└── apps_data/
    └── csight/
        └── firmware/  ← pre-placed for in-app flashing
```

---

### 2. Flash the ESP32

#### Option A — From your Flipper (no computer needed) ✅ Easiest

1. Copy the release ZIP contents to your Flipper SD card, keeping the folder structure
2. Open **CSIght → Flash Firmware** on the Flipper
3. On your ESP32: hold **BOOT**, press **RESET**, release **BOOT**
4. Press **OK** on the Flipper — it flashes all three files automatically

#### Option B — ESP Web Flasher (browser, no install)

1. Open **Chrome or Edge** → [espressif.github.io/esptool-js](https://espressif.github.io/esptool-js/)
2. Connect your ESP32 and click **Program**
3. Add the three `.bin` files with these addresses:

| File | ESP32 / S2 | S3 / C3 / C6 / C5 |
|------|-----------|-------------------|
| `bootloader.bin` | `0x1000` | `0x0000` |
| `partitions.bin` | `0x8000` | `0x8000` |
| `firmware.bin` | `0x10000` | `0x10000` |

#### Option C — esptool CLI

```bash
pip install esptool

esptool.py --chip auto --port /dev/ttyUSB0 --baud 460800 \
  write_flash \
  0x1000  bootloader.bin \
  0x8000  partitions.bin \
  0x10000 firmware.bin
```

See [FLASH_INSTRUCTIONS.md](FLASH_INSTRUCTIONS.md) for port detection and troubleshooting.

---

### 3. Install the FAP

Copy the `.fap` for your Flipper firmware to:
```
SD Card/apps/GPIO/csight.fap
```
Open from **Apps → GPIO → CSIght**.

---

### 4. Wire it up

| ESP32 pin | Flipper GPIO pin |
|-----------|-----------------|
| TX | 14 |
| RX | 13 |
| GND | GND (pin 18) |
| 3.3V | 3.3V (pin 9) |

Default TX/RX per chip — auto-detected by firmware, configurable in app:

| Chip | ESP TX | ESP RX |
|------|--------|--------|
| ESP32 classic | GPIO1 | GPIO3 |
| ESP32-S2 / S3 | GPIO43 | GPIO44 |
| ESP32-C3 | GPIO21 | GPIO20 |
| ESP32-C6 / C61 | GPIO16 | GPIO17 |
| ESP32-C5 | GPIO6 | GPIO7 |

> Power your ESP32 **before** launching CSIght. The app warns you if it can't detect power.

---

### 5. Using Vitals mode

Vitals mode uses the CSI signal to estimate breathing rate and heart rate:

- Switch to Vitals with `←`/`→` during scanning
- Allow ~30 seconds for the first reading to appear
- For best results: sit still, 1–3 metres from the ESP32
- Heart rate accuracy improves significantly if the subject is **between two ESP32 nodes**
- Breathing rate (6–40 BPM) is reliable in quiet conditions
- Heart rate (40–180 BPM) is marked experimental — single-node accuracy varies

---

## Building from source

```bash
# ESP32 firmware (standard chips — ESP-IDF v5.2+)
cd esp32
idf.py build

# ESP32-C5 specifically (requires ESP-IDF v5.5.2+)
idf.py set-target esp32c5
idf.py build

# Flipper FAP
cd flipper
ufbt
```

CI builds all targets automatically on push to `main`. Tag `v1.x` to publish a release.

---

## Multi-Node Mode (v2.0)

CSIght can combine readings from up to 4 ESP32s (1 primary + 3 secondaries) to estimate roughly where motion is happening in a room, not just that it's happening.

**Be aware of what this actually is:** as of v2.3, each node's proximity reading is converted to an estimated distance using an inverse power-law curve, then the position is iteratively refined to be consistent with all reported distances — a real step up from the earlier pure intensity-weighted centroid. It's still not true phase-based direction finding. Independent ESP32 oscillators can't be phase-locked over a shared beacon alone, so precise angle-of-arrival isn't achievable with off-the-shelf hardware, and the distance curve uses a fixed path-loss exponent rather than one calibrated to your specific room. What you get is a genuinely useful room-level position estimate — not laser-precision radar.

### Setting up a secondary node

1. Open the ESP32 firmware project, run `idf.py menuconfig`
2. Go to **CSIght Mesh Configuration → Node role**
3. Pick **Secondary (standalone, auto-discovered)**
4. Build and flash — every secondary unit uses this exact same image, no per-unit settings
5. Power it up near your primary. It broadcasts a HELLO every 2 seconds until the primary responds and assigns it an ID (1, 2, or 3, based on whichever slots are free)
6. Your Flipper shows a **"Node X discovered!"** banner the moment pairing completes

Repeat steps 3–5 for up to 3 secondary units — same firmware image every time, since identity comes from each ESP32's own factory-programmed MAC address rather than anything you configure by hand.

Your primary ESP32 (the one wired to the Flipper) stays on the default **Primary** role — no changes needed there. The primary remembers paired nodes across reboots (stored in NVS), so you only pair each physical unit once. To start fresh — say, after swapping hardware — go to **Settings → Forget Nodes** on the Flipper.

> **Note:** release ZIPs only ship primary-role firmware binaries. Secondary nodes need to be built from source with the role changed in menuconfig — this keeps releases from bloating with an extra firmware variant for a feature most users won't need.

### Configuring node positions

On the Flipper: **Settings → Configure Nodes**. Measure the real-world distance (in cm) from your primary to each secondary and enter it — `←`/`→` picks the node, `↑`/`↓` adjusts the value, `OK` toggles between X and Y axis. The primary is always treated as the origin (0,0).

Then select **Multi-Node Map** from the display modes (`←`/`→` while scanning) to see node status and the estimated position live.

**Tuning the estimate (v3.2):** if positions look consistently off in one direction, try **Settings → Path-loss Exp**. Default is 2.0 (free-space approximation) — denser or multipath-heavy rooms (lots of walls/furniture between nodes) often estimate better around 2.5–3.5. This is empirical; there's no way to auto-detect the right value, so it's trial and error based on what looks right for your space.

---

## Event Logging (v2.2)

CSIght keeps a running CSV log on the SD card at `/ext/apps_data/csight/logs/events.csv`. Toggle it on/off in **Settings → SD Logging** (on by default).

Logged events:

| Event | When | Detail column |
|-------|------|----------------|
| `SESSION_START` | You start scanning | — |
| `SESSION_END` | You stop scanning | duration + total motion events |
| `ALERT` | Tripwire mode fires | intensity level that triggered it |
| `VITALS` | A valid breathing/heart reading completes | `breath=X heart=Y` |
| `NODE_FOUND` | A new secondary mesh node pairs | node ID assigned |

The file uses append mode and opens/closes per write rather than holding a handle open — a pulled SD card or crash won't corrupt it, and repeated app launches keep adding to the same log rather than overwriting it.

---

## Wireless Firmware Updates (v3.2)

The primary ESP32 can now update its own firmware over WiFi from the browser dashboard — no USB cable needed after the first flash.

> ⚠️ **One-time requirement:** v3.2 changes the flash partition layout to add OTA support. If your board is already running an older CSIght build, you **must** re-flash it once via USB (or the Flipper's Flash Firmware feature) before OTA will work. You cannot OTA your way onto the new partition table — the bootloader itself needs it. After that one-time re-flash, OTA works normally going forward.

**To update:**
1. Open the web dashboard (**CSIght → Web UI** on the Flipper, then visit the shown address)
2. Scroll down, tap **FIRMWARE UPDATE** to expand it
3. Choose the new `firmware.bin` for your chip and tap **Upload & Reboot**
4. Wait ~10 seconds — the board reboots automatically into the new version

**Scope note — secondary nodes are NOT covered by this.** Secondaries deliberately run no WiFi AP or HTTP server (keeps them lightweight, since they have no Flipper attached anyway). OTA-ing them would need a firmware relay over ESP-NOW, which is a genuinely separate, bigger project — for now, secondaries still need a physical USB re-flash to update. This is honest scope, not an oversight.

---

## Roadmap

| Version | Feature | Status |
|---------|---------|--------|
| v1.1 | C3 Super Mini + expanded board presets | ✅ Done |
| v1.4 | Standalone Web UI + Flipper Web UI toggle + ESP32 screen support | ✅ Done |
| v1.6 | Vitals mode (breathing detection + experimental heart rate) | ✅ Done |
| v1.7 | ESP32-C5 support + in-app firmware flashing + bug fixes | ✅ Done |
| v2.0 | Multi-ESP32 node support (amplitude-weighted position estimate) | ✅ Done |
| v2.1 | Node auto-discovery — no manual per-unit config needed | ✅ Done |
| v2.2 | SD card event logging (CSV: sessions, alerts, vitals, node pairing) | ✅ Done |
| v2.3 | Distance-based trilateration position estimate (replaces weighted centroid) | ✅ Done |
| v3.0 | Web UI: Vitals + Multi-Node Map modes, node-discovery toast | ✅ Done |
| v3.1 | Web UI: session history chart (motion intensity, rolling 2min window) | ✅ Done |
| v3.2 | Multi-client web UI, adjustable path-loss exponent, browser tripwire + session stats, primary OTA updates | ✅ Done |
| v3.3 | Test Alert button, Quiet/Busy presets, node last-seen timestamps, scheduled auto-arm, motion heatmap | ✅ Done |

---

## Legal

CSIght reads signal metadata from your own WiFi radio hardware. It does not intercept, decode, or store anyone's network traffic or communications.

**You are responsible for ensuring your use of CSIght complies with local laws.** Do not use CSIght to monitor people without their knowledge or consent.

This project is provided for hobbyist, research, and educational purposes only. The authors accept no liability for misuse.

---

## Support

If CSIght is useful to you, a coffee goes a long way 👇

[![Buy Me A Coffee](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://buymeacoffee.com/Joelewis012)

---

## License

MIT © CSIght contributors — see [LICENSE](LICENSE) for full terms.
