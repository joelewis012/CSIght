# CSIght

**WiFi CSI radar for Flipper Zero + ESP32**

Detect motion, presence, and proximity through walls using WiFi Channel State Information — no cameras, no IR, no special sensors.

---

## How it works

CSIght uses your ESP32's WiFi hardware to read Channel State Information (CSI) — detailed data about how WiFi signals are arriving, including amplitude and phase across dozens of subcarriers. When something moves in the environment, it disturbs the signal in measurable ways. CSIght processes those disturbances in real time and renders them on the Flipper screen.

---

## Features

- **Auto chip detection** — identifies your ESP32 model and reports CSI compatibility on first connect
- **Board presets** — one-tap setup for common boards, or full custom pin override saved to SD
- **Radar mode** — classic sweep with fading blips, proximity bars, TARGET ACQUIRED flash
- **Waterfall mode** — scrolling CSI amplitude history across subcarriers
- **Proximity mode** — visual distance estimation with motion intensity bar
- **Adjustable sensitivity** — tune detection threshold on the fly with Up/Down
- **Mode switching** — cycle between all three displays with Left/Right during scan

---

## Compatibility

### ESP32 boards
| Chip       | Support  |
|------------|----------|
| ESP32-S3   | ✅ Full  |
| ESP32-C3   | ✅ Full  |
| ESP32-C6   | ✅ Full  |
| ESP32      | ⚠️ Limited |
| ESP32-S2   | ❌ None  |
| ESP32-H2   | ❌ None  |

### Flipper firmware
| Firmware   | Status |
|------------|--------|
| Official   | ✅     |
| Momentum   | ✅     |
| Unleashed  | ✅     |

---

## Controls (scanning mode)

| Button     | Action                        |
|------------|-------------------------------|
| Left/Right | Switch display mode           |
| Up         | Increase sensitivity          |
| Down       | Decrease sensitivity          |
| Back       | Exit app                      |

---

## Installation

See [FLASH_INSTRUCTIONS.md](FLASH_INSTRUCTIONS.md) for full flashing guide and wiring diagram.

---

## Building from source

Builds are handled by GitHub Actions — push to `main` to trigger a full build for all three Flipper firmware targets plus the ESP32 firmware. Download the release ZIP from the Actions artifacts.

Manual build:
```bash
# ESP32
cd esp32
idf.py build

# Flipper (official)
cd flipper
ufbt
```

---

## Planned

- v1.x — Breathing detection (experimental mode)
- v2.0 — Multi-ESP32 node support for true directional radar

---

## License

MIT
