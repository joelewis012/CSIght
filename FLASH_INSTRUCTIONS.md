# CSIght — ESP32 Flash Instructions

## ⚠️ Read before flashing

- **Never** interrupt power during flashing
- **Never** modify the `bootloader.bin` partition manually
- If something goes wrong, the ESP32 can almost always be recovered — see Recovery below

---

## What you need

- ESP32 board (any variant — see compatibility list in README)
- USB cable (data capable, not charge-only)
- Python 3.7+ installed
- `esptool.py` — install with: `pip install esptool`

---

## Compatibility

| Chip       | CSI Support | Notes                        |
|------------|-------------|------------------------------|
| ESP32      | Limited     | Older CSI API, still works   |
| ESP32-S3   | Full        | Recommended                  |
| ESP32-C3   | Full        | Good budget option           |
| ESP32-C6   | Full        | Best WiFi 6 support          |
| ESP32-S2   | None        | No WiFi CSI — not compatible |
| ESP32-H2   | None        | 802.15.4 only — not compatible |

---

## Flashing

1. Download the release ZIP and extract it
2. Open a terminal in the `esp32/` folder
3. Put your ESP32 into flash mode:
   - Hold **BOOT** button, press **EN/RST**, release BOOT
   - (some boards do this automatically)
4. Run:

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 460800 \
  write_flash \
  0x1000  bootloader.bin \
  0x8000  partition-table.bin \
  0x10000 csight_esp32.bin
```

> Replace `/dev/ttyUSB0` with your actual port:
> - **macOS**: `/dev/cu.usbserial-XXXX`
> - **Windows**: `COM3` (check Device Manager)
> - **Linux**: `/dev/ttyUSB0` or `/dev/ttyACM0`

5. Press **EN/RST** to boot after flashing

---

## Wiring to Flipper Zero

| ESP32 Pin | Flipper GPIO | Notes               |
|-----------|--------------|---------------------|
| TX (17)   | Pin 14 (RX)  | Default — adjustable in app |
| RX (16)   | Pin 13 (TX)  | Default — adjustable in app |
| GND       | GND          | Required            |
| 3.3V      | 3.3V         | Or use USB power    |

> Custom pins can be configured inside the CSIght app under Board → Custom...

---

## Recovery (if something goes wrong)

The ESP32 bootloader lives at `0x1000`. As long as you haven't overwritten that partition with garbage, the board is recoverable.

**Hard recovery:**
```bash
# Erase flash completely
esptool.py --chip esp32 --port /dev/ttyUSB0 erase_flash

# Then re-flash from scratch using the command above
```

This resets the ESP32 to factory state and you can flash again cleanly.

---

## Flipper FAP Install

1. Copy the `.fap` file matching your Flipper firmware to:
   `SD Card/apps/GPIO/csight.fap`
2. Launch from **Apps → GPIO → CSIght**

---

## First run

On first launch CSIght will:
1. Play boot animation
2. Send handshake to ESP32
3. Show detected chip + CSI compatibility
4. Ask you to confirm board preset (or set custom pins)
5. Save config to SD — you won't need to do this again

Settings are stored at: `SD Card/apps_data/csight/config.bin`
