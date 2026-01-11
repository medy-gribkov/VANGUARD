<div align="center">
  
  <h1>🛡️ VANGUARD</h1>
  <h3>Target-First Wireless Auditing Suite for M5Cardputer</h3>

  <p>
    <a href="https://github.com/Mahdy-gribkov/Task-Oriented-Assesor/actions"><img src="https://img.shields.io/github/actions/workflow/status/Mahdy-gribkov/Task-Oriented-Assesor/build.yml?style=for-the-badge&label=BUILD" alt="Build Status"></a>
    <img src="https://img.shields.io/badge/Platform-ESP32--S3-blue?style=for-the-badge&logo=espressif" alt="Platform ESP32">
    <img src="https://img.shields.io/badge/Version-v1.1--ALPHA-orange?style=for-the-badge" alt="Version">
    <img src="https://img.shields.io/badge/License-MIT-green?style=for-the-badge" alt="License">
  </p>

  <p><b>Turn your M5Cardputer into a pocket-sized cyber-swiss-army-knife.</b></p>
  <p><i>Standard wireless tools focus on lists. Vanguard focuses on <br><b>TARGETS</b>.</i></p>

</div>

---

## ⚡ Features

### 📡 Target-Oriented Warfare
Why scroll through 100 APs when you only care about one?
- **Lock On**: Select a target via WiFi or BLE MAC.
- **Focus Fire**: All tools (Deauth, Probe Sniff, Beacon Spam) auto-target your selection.
- **Persistence**: Remembers targets across reboots.

### 🔍 Bruce-Powered Core
Built on the legendary **Bruce** firmware logic, instantiated for specific tactical needs.
- **Hybrid Scanning**: Simultaneous WiFi + BLE reconnaissance.
- **Lazy Loading**: Zero-latency boot; heavy radios only initialize when you pull the trigger.
- **Visual Feedback**: Real-time RSSI radar and signal strength graphing.

### 🎨 Premium "Dark Mode" UI
- **Zero-Flicker**: Double-buffered display rendering.
- **Glitch Aesthetics**: Cyberpunk-inspired transitions and boot sequences.
- **Haptic Feedback**: Feel the packets.

---

## 🚀 Getting Started

### Prerequisites
- **M5Stack Cardputer** (ESP32-S3)
- SD Card (Optional, for PCAP storage)

### Installation (The Easy Way)
1.  Go to **[Releases](https://github.com/Mahdy-gribkov/Task-Oriented-Assesor/releases)**.
2.  Download the latest `firmware.bin`.
3.  Flash using [M5Burner](https://m5stack.com/pages/download) or [Esptool](https://github.com/espressif/esptool).

### Installation (The "Genius" Way)
Clone the repo and build it yourself.
```bash
git clone https://github.com/Mahdy-gribkov/Task-Oriented-Assesor.git
cd Task-Oriented-Assesor
pio run -t upload
```

---

## 🎮 Controls

| Key | Action | Context |
| :--- | :--- | :--- |
| **Starts** | **Boot** | Power On / Reset |
| `R` | **WiFi Scan** | Scan Selector |
| `B` | **BLE Scan** | Scan Selector |
| `Enter` | **Select / Attack** | Menus & Radar |
| `M` | **Main Menu** | Global |
| `Q` / `Bksp` | **Back / Cancel** | Menus |
| `Arrows` | **Navigate** | Radar / Target List |

---

## 🛠️ Roadmap (Alpha 1.x)
- [x] **Core Engine**: Lazy Loading & Stability
- [x] **Input System**: Native driver handling
- [x] **PCAP Buffering**: Save handshakes to SD (Verified)
- [x] **IR Blaster**: TV-B-Gone & Receiver (Verified)
- [ ] **BadUSB**: HID Rubber Ducky scripts
- [ ] **PMKID Capture**: Offline cracking support

---

<div align="center">
  <i>Crafted with 💀 by the Vanguard Team</i>
</div>
