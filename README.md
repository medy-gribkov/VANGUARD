# VANGUARD

<p align="center">
  <img src="docs/images/logo.png" alt="VANGUARD Logo" width="200"/>
  <br/>
  <strong>Target First. Always.</strong>
</p>

<p align="center">
  <a href="#features">Features</a> |
  <a href="#installation">Install</a> |
  <a href="#usage">Usage</a> |
  <a href="#attacks">Attacks</a> |
  <a href="#architecture">Architecture</a>
</p>

---

## Overview

**VANGUARD** is a target-first wireless auditing tool for the M5Stack Cardputer (ESP32-S3). Instead of navigating endless attack menus, you see what's around you first, tap a target, and instantly know what actions are available.

```
Traditional Approach:
[Menu] -> [Category] -> [Attack] -> [Scan] -> [Target] -> [Execute]

The Assessor:
[Boot] -> [See Targets] -> [Pick One] -> [See Actions] -> [Execute]
```

**The target is the noun. The attack is the verb. Pick the noun first.**

---

## Features

### Target-First Discovery
Boot up and choose your scan type: WiFi, Bluetooth, or both. Immediately see every wireless target around you sorted by signal strength.

### Context-Aware Actions
Select a target and see **only** the attacks that will work:
- **WiFi AP with clients?** Deauth available
- **Open network?** Evil Twin enabled
- **5GHz network?** Attacks disabled (ESP32 limitation shown)
- **BLE device?** Spam attacks available

### Unified Target View
WiFi networks, client stations, and BLE devices all appear in the same list. Color-coded icons distinguish target types at a glance.

### Professional UI
- Sprite-based double buffering (no flickering)
- Orange accent theme
- Signal strength visualization
- Security type color coding
- Smooth scrolling with scroll indicators

---

## Screenshots

```
┌──────────────────────────────────────┐
│  THE ASSESSOR                        │
│  "Target First. Always."             │
│  ─────────────────────────────────   │
│   [R] Scan WiFi                      │
│   [B] Scan Bluetooth                 │
│   [ENT] Scan Both                    │
│                                      │
│            [M] Menu                  │
└──────────────────────────────────────┘

┌──────────────────────────────────────┐
│  TARGET RADAR                  12W 3B│
│  ─────────────────────────────────   │
│  ▌████ NETGEAR-5G       WPA2  -42dB │
│   ███  HomeNetwork      WPA2  -58dB │
│   ██   FBI_Van          OPEN  -71dB │
│   █    [Hidden]         WPA3  -85dB │
│                                    ▌ │
│  [;,] Up  [./] Down  [Enter] Select │
└──────────────────────────────────────┘
```

---

## Installation

### Requirements
- M5Stack Cardputer (ESP32-S3)
- PlatformIO (VS Code extension or CLI)
- USB-C cable

### Build & Flash

```bash
# Clone
git clone https://github.com/Mahdy-gribkov/Task-Oriented-Assesor.git
cd Task-Oriented-Assesor

# Build
pio run

# Flash
pio run -t upload

# Monitor (optional)
pio device monitor -b 115200
```

---

## Usage

### Boot Sequence
1. Device shows boot animation
2. Press any key to skip (optional)
3. Scan selector appears

### Scan Selection
| Key | Action |
|-----|--------|
| `R` | WiFi scan only |
| `B` | Bluetooth scan only |
| `Enter` / `E` | Combined WiFi + BLE scan |
| `M` | Open menu |

### Target Radar
| Key | Action |
|-----|--------|
| `;` / `,` | Navigate up |
| `.` / `/` | Navigate down |
| `Enter` / `E` | Select target |
| `R` | Rescan |
| `M` | Open menu |

### Target Detail
| Key | Action |
|-----|--------|
| `;` / `,` | Navigate actions up |
| `.` / `/` | Navigate actions down |
| `Enter` / `E` | Execute action |
| `Q` / Backspace | Go back |

### During Attack
| Key | Action |
|-----|--------|
| `Q` / Backspace | Cancel attack |

---

## Attacks

### WiFi Attacks

| Attack | Description | Target Type |
|--------|-------------|-------------|
| **Deauth All** | Disconnect all clients from AP | Access Point |
| **Deauth Single** | Target specific client | Station |
| **Beacon Flood** | Spam fake network names | Any (uses AP's channel) |
| **Evil Twin** | Clone AP for credential capture | Access Point |
| **Capture PMKID** | Extract PMKID for offline crack | WPA2 Access Point |
| **Monitor** | Passive packet capture | Any |

### BLE Attacks

| Attack | Description | Target Type |
|--------|-------------|-------------|
| **BLE Spam** | Flood fake pairing popups | BLE Device |
| **Sour Apple** | iOS/macOS disruption attack | Apple BLE Device |
| **iOS Popup** | Fake AirPods pairing popup | Any (broadcasts) |
| **Android Fast Pair** | Fake Google pairing popup | Any (broadcasts) |
| **Windows Swift Pair** | Fake Windows pairing popup | Any (broadcasts) |

### Limitations

- **5GHz Networks**: ESP32 can scan but not transmit on 5GHz. These targets are marked with yellow "5G" indicator and attacks are disabled.
- **WPA3**: PMKID capture not available on WPA3 networks.
- **BLE Range**: Effective range ~10m depending on target device.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      THE ASSESSOR                           │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────┐ │
│  │     UI      │  │   ENGINE    │  │     ADAPTERS        │ │
│  │             │  │             │  │                     │ │
│  │BootSequence │  │ AssessorEng │  │ BruceWiFi (attacks) │ │
│  │ ScanSelector│◄─┤ TargetTable │◄─┤ BruceBLE  (BLE)     │ │
│  │ TargetRadar │  │ ActionRes.  │  │                     │ │
│  │ TargetDetail│  │             │  │                     │ │
│  │ MainMenu    │  │             │  │                     │ │
│  │ SettingsPane│  │             │  │                     │ │
│  └─────────────┘  └─────────────┘  └─────────────────────┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Key Components

- **AssessorEngine**: Orchestrates scanning, maintains target state, executes actions
- **TargetTable**: Stores and filters discovered targets
- **ActionResolver**: Determines valid actions for each target type
- **BruceWiFi**: Raw WiFi packet injection using ESP32 promiscuous mode
- **BruceBLE**: NimBLE-based BLE scanning and advertising

---

## Configuration

Settings available via menu (`M` key):

| Setting | Range | Default |
|---------|-------|---------|
| WiFi Scan Time | 2-15 sec | 5 sec |
| BLE Scan Time | 1-10 sec | 3 sec |
| Deauth Packets | 5-50 | 10 |
| Auto Rescan | On/Off | On |
| Sound Effects | On/Off | Off |

---

## Legal Disclaimer

**This tool is for authorized security testing and educational purposes only.**

- You must have explicit written permission before testing any network
- Unauthorized network intrusion is illegal in most jurisdictions
- The developers assume no liability for misuse
- Always follow your local laws and regulations

**If you can't hack responsibly, don't hack at all.**

---

## Credits

- **[Bruce](https://github.com/pr3y/Bruce)** - Attack implementations and inspiration
- **M5Stack** - Hardware and M5Unified library
- **NimBLE-Arduino** - BLE stack
- **ESP-IDF** - ESP32 framework

---

## License

GPL-3.0 - See [LICENSE](LICENSE)

This project is a derivative work and maintains license compatibility with Bruce.

---

<p align="center">
  <strong>The Assessor</strong><br/>
  <em>Target First. Always.</em>
</p>
