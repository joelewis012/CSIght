<div align="center">

```
 ██████╗███████╗██╗ ██████╗ ██╗  ██╗████████╗
██╔════╝██╔════╝██║██╔════╝ ██║  ██║╚══██╔══╝
██║     ███████╗██║██║  ███╗███████║   ██║   
██║     ╚════██║██║██║   ██║██╔══██║   ██║   
╚██████╗███████║██║╚██████╔╝██║  ██║   ██║   
 ╚═════╝╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═╝   ╚═╝  
```

### WiFi CSI Radar for Flipper Zero + ESP32

**See through walls. No cameras. No IR. Just WiFi.**

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![Firmware: Official](https://img.shields.io/badge/firmware-Official-blue?style=flat-square)]()
[![Firmware: Momentum](https://img.shields.io/badge/firmware-Momentum-purple?style=flat-square)]()
[![Firmware: Unleashed](https://img.shields.io/badge/firmware-Unleashed-red?style=flat-square)]()
[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2-orange?style=flat-square)]()

</div>

---

## What is CSIght?

Every WiFi packet that moves through a room carries hidden data about that room — the amplitude, phase, and propagation delay of the signal across dozens of frequency subcarriers. When something moves, those values shift. CSIght reads those shifts in real time using your ESP32's WiFi hardware and renders them on your Flipper Zero as a live radar display.

No special hardware. No cameras. No IR sensors. Just the WiFi radio already sitting on your ESP32 expansion board.

---

## Displays

```
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│    CSIght       │  │CSIght WATERFALL │  │    CSIght       │
│   ·  ·  ·       │  │█░░░░░░░░░░░░█░░│  │   PROXIMITY     │
│  · ╭───╮ ·      │  │█░░░░█░░░░░░░█░░│  │                 │
│  · │ ╱ │ · MOTION│  │█░░░░█░░█░░░░█░░│  │   ○             │
│  · ╰───╯ · ████│  │█░░░░█░░█░░█░█░░│  │  ○○○            │
│  · · ● · ·  RANGE│  │█░██░█░░█░░█░█░█│  │ ○○○○○          │
│  · · · · ·  ████│  │                 │  │    ■            │
│         SENS: 7 │  │            S:7  │  │   72%           │
└─────────────────┘  └─────────────────┘  └─────────────────┘
    Radar Mode           Waterfall Mode      Proximity Mode
```

**Radar** — rotating sweep with fading blips. Motion triggers a `TARGET ACQUIRED` flash.  
**Waterfall** — scrolling history of CSI amplitude across all subcarriers. Looks incredible.  
**Proximity** — concentric arc display showing estimated distance to the detected target.

Switch modes with `←` / `→` during scanning.

---

## Features

- **Auto chip detection** — ESP32 reports its own model and CSI support tier on connect
- **40+ board presets** — covers official Flipper boards, DevKits, XIAO, Adafruit, SparkFun, M5Stack, LOLIN, and more
- **Custom pin override** — set any TX/RX pair, saved to SD card so you only do it once
- **Three display modes** — radar sweep, waterfall, proximity — switchable live
- **Adjustable sensitivity** — tune the detection threshold on the fly
- **Proximity estimation** — rough distance to detected motion using signal delta strength
- **Zero configuration on known boards** — preset auto-populates pins from firmware handshake

---

## Compatibility

### ESP32 chips

| Chip | CSI Support | Notes |
|------|-------------|-------|
| ESP32-S3 | ✅ Full | Recommended |
| ESP32-C3 | ✅ Full | Great budget option |
| ESP32-C6 | ✅ Full | WiFi 6, best sensitivity |
| ESP32-C61 | ✅ Full | C6 without 802.15.4 |
| ESP32 (original) | ⚠️ Limited | Older CSI API, amplitude only |
| ESP32-S2 | ❌ None | No WiFi CSI support |
| ESP32-H2 | ❌ None | 802.15.4 only, no WiFi |

> **Full** = amplitude + phase across all subcarriers → better proximity accuracy  
> **Limited** = amplitude only → motion detection works, proximity less precise

### Flipper firmware

| Firmware | Status |
|----------|--------|
| Official | ✅ |
| Momentum | ✅ |
| Unleashed | ✅ |

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

### 1. Flash the ESP32 firmware

See **[FLASH_INSTRUCTIONS.md](FLASH_INSTRUCTIONS.md)** for the full flashing guide, wiring diagram, and recovery steps.

Quick start (replace port as needed):

```bash
pip install esptool

esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  write_flash \
  0x1000  esp32/bootloader.bin \
  0x8000  esp32/partition-table.bin \
  0x10000 esp32/csight_esp32.bin
```

### 2. Install the Flipper FAP

Copy the `.fap` matching your firmware to your Flipper SD card:

```
SD Card/apps/GPIO/csight.fap
```

Launch from **Apps → GPIO → CSIght**.

### 3. Wire it up

| ESP32 pin | Flipper GPIO |
|-----------|-------------|
| TX (default 17) | Pin 14 |
| RX (default 16) | Pin 13 |
| GND | GND |
| 3.3V | 3.3V |

Pins are configurable inside the app — you only need to set them once.

---

## Building from source

Push to `main` to trigger the GitHub Actions build. It compiles for all three Flipper firmware targets and packages everything into a release ZIP automatically.

Manual build:

```bash
# ESP32 firmware (requires ESP-IDF v5.2+)
cd esp32
idf.py build

# Flipper FAP (official firmware)
cd flipper
ufbt
```

---

## Roadmap

| Version | Feature |
|---------|---------|
| v1.x | Breathing detection (experimental) |
| v2.0 | Multi-ESP32 node support for true directional radar |

---

## License

MIT © CSIght contributors

See [LICENSE](LICENSE) for full terms.
