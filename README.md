<div align="center">

  <h1>VANGUARD</h1>
  <h3>Target-First Wireless Auditing Suite for M5Cardputer ADV</h3>

  <p>
    <a href="https://github.com/Mahdy-gribkov/VANGUARD/actions"><img src="https://img.shields.io/github/actions/workflow/status/Mahdy-gribkov/VANGUARD/build.yml?style=for-the-badge&label=BUILD" alt="Build Status"></a>
    <img src="https://img.shields.io/badge/Platform-ESP32--S3-blue?style=for-the-badge&logo=espressif" alt="Platform ESP32">
    <img src="https://img.shields.io/badge/Version-v2.0.0-orange?style=for-the-badge" alt="Version">
    <img src="https://img.shields.io/badge/License-AGPL--3.0-green?style=for-the-badge" alt="License">
  </p>

  <p><b>Turn your M5Cardputer into a pocket-sized wireless auditing tool.</b></p>
  <p>Standard wireless tools focus on attack menus. VANGUARD focuses on <b>TARGETS</b>.</p>

</div>

---

## Legal Disclaimer

VANGUARD is intended **exclusively for authorized security testing and educational purposes**. You must have explicit written permission from the network or device owner before using any feature of this tool.

Unauthorized use of wireless auditing tools is illegal under the Computer Fraud and Abuse Act (CFAA), the UK Computer Misuse Act, and equivalent laws in most jurisdictions. The authors and contributors accept no liability for misuse. By using this software you agree that you are solely responsible for compliance with all applicable laws.

**Use responsibly. Get permission first.**

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
- **WiFi Discovery** - 2.4GHz AP and station detection with RSSI, channel, and security info
- **BLE Scanning** - Device enumeration via NimBLE with manufacturer data
- **Combined Scan** - WiFi then BLE in sequence
- **IR Targets** - Virtual IR target for TV-B-Gone and IR signal replay

### WiFi Attacks
| Attack | Description |
|--------|-------------|
| **Deauth All** | Disconnect all clients from a target AP |
| **Deauth Station** | Disconnect a specific client from its AP |
| **Deauth Selective** | Choose which clients to disconnect from a multi-client AP |
| **Beacon Flood** | Clone and broadcast copies of a target network to confuse scanners |
| **Evil Portal** | Fake AP with captive portal for credential capture |
| **Handshake Capture** | Sniff WPA 4-way handshakes and save PCAP to SD card |
| **PMKID Capture** | Extract PMKID from the first EAPOL message (no client needed) |
| **Probe Flood** | Broadcast fake probe requests to pollute nearby AP logs |
| **Probe Monitor** | Passively observe probe requests to fingerprint nearby devices |

### BLE Attacks
| Attack | Description |
|--------|-------------|
| **BLE Spam** | Flood target with crafted advertisement packets |
| **BLE Skimmer Detect** | Identify characteristics of Bluetooth credit card skimmers |

### IR
| Attack | Description |
|--------|-------------|
| **TV-B-Gone** | Power cycle nearby TVs using a database of known IR codes |
| **IR Receive** | Capture and decode incoming infrared signals |

### Context-Aware Actions
- WPA3 target? PMKID capture hidden (not vulnerable).
- No clients detected? Deauth hidden (nothing to deauth).
- 5GHz network? WiFi attacks hidden (ESP32-S3 is 2.4GHz only).

---

## Hardware

- **Device**: M5Stack Cardputer ADV (ESP32-S3FN8, 8MB flash, no PSRAM)
- **Keyboard**: TCA8418 I2C matrix controller at 0x34
- **Display**: ST7789 135x240 SPI
- **SD Card**: Optional, required for PCAP storage

---

## Building

Requires [PlatformIO](https://platformio.org/).

```bash
git clone https://github.com/Mahdy-gribkov/VANGUARD.git
cd VANGUARD
pio run -e m5stack-cardputer
pio run -t upload
```

### Running Tests

```bash
pio test -e native --verbose
```

All native tests must pass before merging. See [CONTRIBUTING.md](CONTRIBUTING.md) for details.

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

## Acknowledgments

- [Bruce firmware](https://github.com/pr3y/Bruce) - ESP32 offensive framework that VANGUARD's adapters build upon
- [M5Stack](https://m5stack.com/) - Hardware platform and Cardputer ecosystem
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) - Lightweight BLE stack for ESP32

---

## License

AGPL-3.0. See [LICENSE](LICENSE).

Built by [SporeSec](https://github.com/spore-sec).
