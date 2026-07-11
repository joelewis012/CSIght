# CSIght — Flashing the ESP32

There are three ways to flash. Option A is the easiest and requires nothing installed.

---

## Option A — From your Flipper Zero (no computer needed) ✅

1. Copy the release ZIP to your Flipper SD card, keeping the folder structure intact
2. Open **CSIght → Flash Firmware** on the Flipper
3. On your ESP32: hold **BOOT**, press **RESET**, release **BOOT**
4. Press **OK** on the Flipper — it flashes bootloader, partition table and firmware automatically
5. ESP32 reboots into CSIght when done

---

## Option B — Browser (no install required)

1. Open **Chrome or Edge** (must be Chromium — Safari and Firefox won't work)
2. Go to **https://espressif.github.io/esptool-js/**
3. Set baud rate to **460800** → **Connect** → select your board
4. Under **Flash**, add the three files with the addresses below
5. Click **Program** → press **RESET** on the board when done

### Flash addresses by chip

| Chip | `bootloader.bin` | `partitions.bin` | `firmware.bin` |
|------|-----------------|-----------------|---------------|
| ESP32 classic | `0x1000` | `0x8000` | `0x10000` |
| ESP32-S2 | `0x1000` | `0x8000` | `0x10000` |
| ESP32-S3 | `0x0000` | `0x8000` | `0x10000` |
| ESP32-C3 | `0x0000` | `0x8000` | `0x10000` |
| ESP32-C6 / C61 | `0x0000` | `0x8000` | `0x10000` |
| ESP32-C5 | `0x0000` | `0x8000` | `0x10000` |

### Which firmware folder is mine?

| Board | Chip | Folder |
|-------|------|--------|
| FZ WiFi Dev Board, FlipMods Mini WiFi, Adafruit Feather S2, QT Py S2, LOLIN S2 Mini, FeatherS2, TinyS2 | ESP32-S2 | `esp32/esp32s2/` |
| WROOM-32, NodeMCU, TTGO T-Display, M5Stack Core, HUZZAH32, FlipMods Combo, Marauder boards | ESP32 | `esp32/esp32/` |
| S3 DevKitC/M, XIAO-S3, TinyS3, FeatherS3, ProS3, CoreS3, TTGO T-Display S3 | ESP32-S3 | `esp32/esp32s3/` |
| C3 DevKit, XIAO-C3, C3 Super Mini, LOLIN C3 Mini, M5Stamp C3 | ESP32-C3 | `esp32/esp32c3/` |
| C6 DevKitC-1, C6 DevKitM-1, C61 DevKitC-1, XIAO-C6 | ESP32-C6/C61 | `esp32/esp32c6/` |
| C5 DevKitC-1 | ESP32-C5 | `esp32/esp32c5/` |

> **C5 note:** requires ESP-IDF v5.5.2 to build from source. Pre-built binary included in releases.
> **Marauder note:** boards flashed with Marauder need reflashing before CSIght works. Marauder can be restored afterwards.

---

## Option C — esptool CLI

```bash
pip install esptool

# Classic ESP32
esptool.py --chip auto --port /dev/ttyUSB0 --baud 460800 \
  write_flash 0x1000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin

# S3, C3, C6, C5 (bootloader at 0x0)
esptool.py --chip auto --port /dev/ttyUSB0 --baud 460800 \
  write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

**Finding your port:**
- macOS/Linux: `ls /dev/tty.*` or `ls /dev/ttyUSB*`
- Windows: Device Manager → Ports (COM & LPT)

---

## Recovery

If the ESP32 is bricked, hold **BOOT** during power-on and re-flash using Option B or C. The ROM bootloader is always recoverable this way.
