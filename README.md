<div align="center">

  <h1>VANGUARD</h1>
  <h3>Target-First Wireless Auditing Suite for M5Cardputer ADV</h3>

  <p>
    <a href="https://github.com/Mahdy-gribkov/VANGUARD/actions"><img src="https://img.shields.io/github/actions/workflow/status/Mahdy-gribkov/VANGUARD/build.yml?style=for-the-badge&label=BUILD" alt="Build Status"></a>
    <img src="https://img.shields.io/badge/Platform-ESP32--S3-blue?style=for-the-badge&logo=espressif" alt="Platform ESP32">
    <img src="https://img.shields.io/badge/Version-v2.0-orange?style=for-the-badge" alt="Version">
    <img src="https://img.shields.io/badge/License-AGPL--3.0-green?style=for-the-badge" alt="License">
  </p>

  <p><b>Turn your M5Cardputer into a pocket-sized wireless auditing tool.</b></p>
  <p>Standard wireless tools focus on attack menus. VANGUARD focuses on <b>TARGETS</b>.</p>

</div>

---

## How It Works

Traditional pentest tools: pick an attack category, then find a target.

VANGUARD inverts this:

1. Boot. Scan begins.
2. See every target in range (WiFi, BLE, IR).
3. Select one. Only valid actions appear.
4. Execute. No guessing, no "incompatible target" errors.

The target is the noun. The attack is the verb. Pick the noun first.

---

## Features

### Scanning
- **WiFi discovery**: 2.4GHz AP and station detection with RSSI, channel, security info
- **BLE scanning**: Device enumeration via NimBLE
- **Combined scan**: WiFi then BLE in sequence
- **IR targets**: Virtual IR target for TV-B-Gone and IR replay

### Attacks (WiFi)
- **Deauth All/Single**: Disconnect clients from an AP (requires detected clients)
- **Beacon Flood**: Clone and spam copies of a target network
- **Evil Portal**: Fake AP with captive portal for credential capture
- **Handshake Capture**: 4-way WPA handshake sniffing (PCAP to SD)

### Attacks (BLE)
- **BLE Spam**: Flood target with pairing requests
- **Sour Apple**: Apple device disruption

### IR
- **TV-B-Gone**: Power cycle nearby TVs using known codes
- **IR Replay**: Record and replay infrared signals

### Context-Aware Actions
- WPA3 target? PMKID capture hidden (not vulnerable).
- No clients? Deauth hidden (nothing to deauth).
- 5GHz network? WiFi attacks hidden (ESP32-S3 is 2.4GHz only).

---

## Hardware

- **Device**: M5Stack Cardputer ADV (ESP32-S3FN8, 8MB flash, no PSRAM)
- **Keyboard**: TCA8418 I2C matrix controller at 0x34
- **Display**: ST7789 135x240 SPI
- **SD Card**: Optional, for PCAP storage

---

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
git clone https://github.com/Mahdy-gribkov/VANGUARD.git
cd VANGUARD
pio run -t upload
```

### Running Tests

```bash
pio test -e native --verbose
```

---

## Controls

| Key | Action | Context |
| :--- | :--- | :--- |
| `R` | WiFi Scan | Scan selector / Radar (rescan) |
| `B` | BLE Scan | Scan selector |
| `Enter` | Select / Confirm | All menus |
| `M` | Main Menu | Global (except boot) |
| `Q` / `Backspace` | Back / Cancel | All screens |
| `;` `,` | Navigate Up | Lists and menus |
| `.` `/` | Navigate Down | Lists and menus |

---

## Project Structure

```
src/
  core/        Engine, TargetTable, ActionResolver, SystemTask, IPC
  adapters/    BruceWiFi, BruceBLE, BruceIR, EvilPortal
  ui/          BootSequence, TargetRadar, TargetDetail, MainMenu, Theme
test/          Native unit tests (googletest)
```

---

## License

AGPL-3.0. See LICENSE.

Built by SporeSec.
