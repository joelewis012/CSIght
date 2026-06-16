<div align="center">

<img src="csight_logo.svg" alt="CSIght" width="700"/>

<br/>

**See through walls. No cameras. No IR. Just WiFi.**

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![Release](https://img.shields.io/github/v/release/joelewis012/CSIght?style=flat-square&color=green&label=Latest%20Release&cacheSeconds=3600)](https://github.com/joelewis012/CSIght/releases/latest)
[![Stars](https://img.shields.io/github/stars/joelewis012/CSIght?style=flat-square&color=yellow&cacheSeconds=3600)](https://github.com/joelewis012/CSIght/stargazers)
[![Forks](https://img.shields.io/github/forks/joelewis012/CSIght?style=flat-square&color=blue&cacheSeconds=3600)](https://github.com/joelewis012/CSIght/network/members)
[![Firmware: Official](https://img.shields.io/badge/firmware-Official-blue?style=flat-square)]()
[![Firmware: Momentum](https://img.shields.io/badge/firmware-Momentum-purple?style=flat-square)]()
[![Firmware: Unleashed](https://img.shields.io/badge/firmware-Unleashed-red?style=flat-square)]()
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2-orange?style=flat-square)]()
[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-support-yellow?style=flat-square&logo=buy-me-a-coffee)](https://buymeacoffee.com/Joelewis012)

### [⬇ Download Latest Release](https://github.com/joelewis012/CSIght/releases/latest) &nbsp;·&nbsp; [☕ Buy Me a Coffee](https://buymeacoffee.com/Joelewis012)

</div>

---

## What is CSIght?

Every WiFi packet that moves through a room carries hidden data about that room — the amplitude, phase, and propagation delay of the signal across dozens of frequency subcarriers. When something moves, those values shift. CSIght reads those shifts in real time using your ESP32's WiFi hardware and renders them on your Flipper Zero as a live radar display.

No special hardware. No cameras. No IR sensors. Just the WiFi radio already sitting on your ESP32 expansion board.

---

## Displays

**Radar Mode**
```
┌──────────────────────────┐
│ CSIght                   │
│  · ╭────╮ ·   MOTION     │
│  · │ ╱  │ ·   ████       │
│  · ╰────╯ ·   RANGE      │
│  · · ● · ·    ████       │
│              SENS: 7     │
└──────────────────────────┘
Sweep rotates, blips fade.
Motion triggers TARGET ACQUIRED flash.
```

**Waterfall Mode**
```
┌──────────────────────────┐
│ CSIght  WATERFALL        │
│ █░░░░░░░░░░░░█░░░░░░░░░░ │
│ █░░░░█░░░░░░░█░░░░░░░░░░ │
│ █░░░░█░░█░░░░█░░░░░░░░░░ │
│ █░░░░█░░█░░█░█░░░░░░░░░░ │
│ █░██░█░░█░░█░█░░░░░░░░░█ │
│                      S:7 │
└──────────────────────────┘
Scrolling CSI amplitude history.
```

**Proximity Mode**
```
┌──────────────────────────┐
│         CSIght           │
│        PROXIMITY         │
│           ○              │
│          ○○○             │
│         ○○○○○            │
│           ■              │
│          72%             │
└──────────────────────────┘
Concentric arcs show distance.
```

Switch modes with `←` / `→` during scanning.

---

## Features

- **Auto chip detection** — ESP32 reports its own model and CSI support tier on connect
- **40+ board presets** — covers official Flipper boards, DevKits, XIAO, Adafruit, SparkFun, M5Stack, LOLIN, and more
- **Custom pin override** — set any TX/RX pair, saved to SD card so you only do it once
- **Three display modes** — radar sweep, waterfall, proximity — switchable live
- **Adjustable sensitivity** — tune detection threshold on the fly
- **Proximity estimation** — rough distance to detected motion using signal delta strength
- **Zero configuration on known boards** — preset auto-populates pins from firmware handshake

---

## Compatibility

### ESP32 firmware — which folder is mine?

| Firmware folder | Chip | Boards |
|----------------|------|--------|
| `esp32/esp32/` | ESP32 (original) | ESP32-WROOM-32, ESP32-WROOM-32U, ESP32-WROOM-32UE, ESP32-WROVER, ESP32-WROVER-B, NodeMCU-32S, FZ WiFi Dev Board, FZ WiFi Dev Board Pro, FlipMods Mini WiFi, FlipMods Combo, FEBERIS, Marauder Double Barrel, AI-Thinker ESP32-CAM, LOLIN D32, LOLIN D32 Pro, WEMOS D1 Mini32, Adafruit HUZZAH32, SparkFun ESP32 Thing, M5Stack Core, M5Stack Core2, Olimex ESP32-EVB |
| `esp32/esp32s3/` | ESP32-S3 | ESP32-S3 DevKitC-1, ESP32-S3 DevKitM-1, XIAO ESP32-S3, LOLIN S3, TinyS3, FeatherS3, ProS3, Adafruit QT Py ESP32-S3, M5Stamp S3, Olimex ESP32-S3-DevKit |
| `esp32/esp32c3/` | ESP32-C3 | ESP32-C3 DevKitC-02, ESP32-C3 DevKitM-1, XIAO ESP32-C3, LOLIN C3 Mini, Adafruit QT Py ESP32-C3, M5Stamp C3, SparkFun ESP32-C3 |
| `esp32/esp32c6/` | ESP32-C6 | ESP32-C6 DevKitC-1, ESP32-C6 DevKitM-1, ESP32-C61 DevKitC-1, XIAO ESP32-C6, SparkFun C6 Qwiic Pocket |

### CSI quality by chip

| Chip | CSI Support | Notes |
|------|-------------|-------|
| ESP32-C6 | ✅ Full | WiFi 6, excellent sensitivity |
| ESP32-C61 | ✅ Full | C6 without 802.15.4 |
| ESP32-C3 | ✅ Full | Great budget option |
| ESP32-S3 | ✅ Full | Recommended all-rounder |
| ESP32 (original) | ⚠️ Limited | Motion detection works, proximity less precise |
| ESP32-WROOM-32 / 32U / 32UE | ⚠️ Limited | Same original ESP32 chip, amplitude only |
| ESP32-S2 | ❌ None | No WiFi CSI support |
| ESP32-H2 | ❌ None | 802.15.4 only, no WiFi |
| ESP32-C5 | 🔜 Coming soon | Requires ESP-IDF v5.5 — not yet supported |

> **Full** = amplitude + phase across all subcarriers → best accuracy
> **Limited** = amplitude only → motion detection still works fine

### Flipper firmware — which FAP is mine?

| FAP folder | Use if you run |
|-----------|----------------|
| `flipper/official/` | Stock Flipper firmware |
| `flipper/momentum/` | Momentum firmware |
| `flipper/unleashed/` | Unleashed firmware |

---

## Controls

| Button | Scanning mode | Setup screens |
|--------|--------------|---------------|
| `←` / `→` | Switch display mode | — |
| `↑` | Sensitivity up | Next option |
| `↓` | Sensitivity down | Previous option |
| `OK` | — | Confirm selection |
| `Back` | Exit app | Go back |

---

## Installation

### 1. Download

Grab the latest release ZIP from the [Releases page](https://github.com/joelewis012/CSIght/releases/latest). It contains:

```
CSIght/
├── flipper/
│   ├── official/     csight.fap  ← stock firmware
│   ├── momentum/     csight.fap  ← Momentum firmware
│   └── unleashed/    csight.fap  ← Unleashed firmware
└── esp32/
    ├── esp32/        ← WROOM-32, NodeMCU, most 3-in-1 boards
    ├── esp32s3/      ← S3 DevKit, XIAO-S3, TinyS3
    ├── esp32c3/      ← C3 Mini, XIAO-C3, C3 Super Mini
    └── esp32c6/      ← C6 DevKit, XIAO-C6
```

---

### 2. Flash the ESP32

#### Option A — Easy (GUI, no install needed) ✅ Recommended for beginners

1. Open **Chrome or Edge** (must be Chromium-based)
2. Go to **[ESP Web Flasher](https://espressif.github.io/esptool-js/)**
3. Click **Connect** and select your ESP32 from the popup
4. Click **Program**, add each `.bin` file with the correct address:

| File | Address |
|------|---------|
| `bootloader.bin` | `0x1000` |
| `partition-table.bin` | `0x8000` |
| `csight_esp32.bin` | `0x10000` |

5. Click **Program** and wait for it to finish
6. Press **Reset** on the board

> ⚠️ ESP Web Flasher only works in Chrome or Edge — not Safari or Firefox

#### Option B — Command line (esptool)

```bash
pip install esptool

esptool.py --chip auto --port /dev/ttyUSB0 --baud 460800 \
  write_flash \
  0x1000  bootloader.bin \
  0x8000  partition-table.bin \
  0x10000 csight_esp32.bin
```

Replace `/dev/ttyUSB0` with your port — see [FLASH_INSTRUCTIONS.md](FLASH_INSTRUCTIONS.md) for full details and port finding instructions.

---

### 3. Install the FAP

Copy the `.fap` matching your Flipper firmware to:

```
SD Card/apps/GPIO/csight.fap
```

Launch from **Apps → GPIO → CSIght**.

---

### 4. Wire it up

| ESP32 pin | Flipper GPIO |
|-----------|-------------|
| TX (default 17) | Pin 14 |
| RX (default 16) | Pin 13 |
| GND | GND |
| 3.3V | 3.3V |

> ⚠️ Power your ESP32 board **before** launching CSIght on the Flipper. The app will warn you if you forget.

Pins are configurable inside the app and saved to SD card — set once, never again.

---

## Building from source

Push to `main` to trigger the GitHub Actions build. It compiles all three Flipper firmware targets and packages everything into a release ZIP. Tag with `v1.0` to publish a GitHub Release automatically.

```bash
# ESP32 firmware (requires ESP-IDF v5.2+)
cd esp32
idf.py build

# Flipper FAP
cd flipper
ufbt
```

---

## Roadmap

| Version | Feature |
|---------|---------|
| v1.1 | C3 Super Mini + expanded board presets |
| v1.4 | Standalone Web UI (no Flipper needed) + Flipper Web UI toggle + ESP32 screen support (M5Stack, TTGO, Lilygo) |
| v1.x | Breathing detection (experimental) |
| v2.0 | Multi-ESP32 node support for true directional radar |
| v2.1 | ESP32-C5 support (requires ESP-IDF v5.5) |

---

## Legal

CSIght is intended for use on hardware you own, in spaces you have permission to monitor. WiFi CSI reads signal metadata from your own radio hardware — it does not intercept, decode, or store anyone's network traffic or communications.

**You are responsible for ensuring your use of CSIght complies with the laws of your country and region.** In particular:

- Do not use CSIght to monitor people in spaces without their knowledge or consent
- Do not use CSIght in jurisdictions where passive RF sensing is restricted
- The authors accept no liability for misuse of this software

This project is provided in good faith for hobbyist, research, and educational purposes.

---

## Support

If CSIght is useful to you, a coffee goes a long way 👇

[![Buy Me A Coffee](https://www.buymeacoffee.com/assets/img/custom_images/orange_img.png)](https://buymeacoffee.com/Joelewis012)

---

## License

MIT © CSIght contributors — see [LICENSE](LICENSE) for full terms.
