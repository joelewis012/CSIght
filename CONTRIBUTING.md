# Contributing to CSIght

Thanks for your interest in contributing! CSIght is an open source project and contributions are welcome.

---

## Before you open a PR

- Check existing issues and PRs first to avoid duplicates
- For big changes, open an issue to discuss before writing code
- **Automated security PRs** (OrbisAI, Snyk, Dependabot etc.) — please open an issue first so changes can be reviewed in context. Automated scanners often misidentify patterns in embedded C code.

---

## How to contribute

### Reporting bugs
Open an issue with:
- Your ESP32 board and chip (e.g. ESP32-C3 Super Mini)
- Your Flipper firmware (Official / Momentum / Unleashed)
- What happened vs what you expected
- Any error messages or screenshots

### Board presets
If your board isn't in the preset list or has wrong pins, open an issue or PR with:
- Board name and chip model
- Correct UART TX/RX pin numbers
- Source (datasheet, pinout diagram link)

### Code contributions
1. Fork the repo
2. Create a branch: `git checkout -b fix/your-fix-name`
3. Make your changes
4. Test on real hardware if possible
5. Open a PR with a clear description of what changed and why

### ESP32 firmware changes
- Test compiles for all four targets: `esp32`, `esp32s3`, `esp32c3`, `esp32c6`
- The GitHub Actions workflow will build all targets automatically on push

### Flipper FAP changes
- Test against Official firmware at minimum
- Avoid floats, `strnlen`, `pdMS_TO_TICKS`, and other symbols not in the Flipper SDK
- All warnings are treated as errors — `-Werror` is enforced

---

## Project structure

```
CSIght/
├── flipper/          Flipper Zero FAP source
│   └── src/
│       ├── csight.h          Shared types, structs, declarations
│       ├── csight_app.c      App lifecycle, input handling, state machine
│       ├── csight_draw.c     All canvas rendering
│       └── csight_uart.c     UART comms with ESP32
├── esp32/            ESP32 firmware source (ESP-IDF v5.2)
│   ├── main/         Entry point, UART handler, WiFi init
│   └── components/
│       ├── csi_handler/    CSI capture and processing
│       └── web_ui/         Web server and WebSocket radar UI
└── .github/
    └── workflows/
        └── compile.yml     Matrix build for all targets
```

---

## Code style

- C99/gnu17 for ESP32, Flipper SDK conventions for FAP
- Keep names clear and consistent with existing code
- Comment anything non-obvious, especially hardware-specific behaviour
- No `malloc` in draw callbacks
- Integer arithmetic only — no floats in Flipper FAP code

---

## Questions

Open an issue or start a discussion. We'll respond as soon as we can.
