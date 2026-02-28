# Changelog

All notable changes to this project will be documented in this file.

## [v2.0.0] - 2026-02-28

### Added
- **14 attacks operational**: Deauth All, Deauth Station, Deauth Selective, Beacon Flood, Evil Portal, Handshake Capture, PMKID Capture, Probe Flood, Probe Monitor, BLE Spam, BLE Skimmer Detect, IR TV-B-Gone, IR Receive
- BLE skimmer detection with native unit tests
- Legal disclaimer shown at boot and in About panel
- ScanSelector with per-scan-type descriptions
- Full-screen main menu with consistent navigation
- About panel upgrade with version, build info, and attribution
- Settings menu renamed for clarity
- Copy audit across all UI strings

### Fixed
- BLE scan crash caused by NimBLE callback race condition
- UI flickering during rapid scan updates

### Changed
- Complete UI overhaul: consistent theme, full-screen layouts, improved readability
- Refactored adapter layer for cleaner separation from Bruce firmware

## [v1.1.0-alpha] - 2026-01-12

### Added
- Initial public release
- WiFi AP and station scanning
- Basic deauth attack
- BLE device enumeration
- IR TV-B-Gone support
- Target-first workflow with context-aware action filtering
- Boot sequence animation
- Native test framework (googletest on PlatformIO)
