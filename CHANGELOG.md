# Changelog

All notable changes to this project will be documented in this file.

## [v1.0.3] - 2026-03-01

### Security
- Excluded `.claude/` from git tracking (was leaking local paths)
- Added Dependabot for GitHub Actions and pip dependency monitoring
- Added CodeQL C/C++ scanning workflow on push and PRs
- Added SECURITY.md with vulnerability reporting process
- Fixed 4 strncpy calls missing null termination in EvilPortal.cpp
- Added null checks for all heap allocations in SystemTask
- Pinned PlatformIO version in CI (build.yml and release.yml)
- Added `-Werror` to both firmware and native builds
- Fixed hardcoded path in vloop.ps1

### Added
- Attack chains: Full Capture, Recon, and Disruption multi-step sequences
- 44 new unit tests (141 total): IPC validation, buffer safety, stress tests, chain validation, edge cases
- AddressSanitizer enabled in CI for native tests
- MAC vendor prefix lookup in target detail view (Apple, Samsung, Google, Intel, TP-Link, etc.)
- Scan elapsed time displayed during scanning
- Architecture section in CONTRIBUTING.md with singleton ownership table

### Fixed
- Buffer truncation warnings: SettingsPanel key buffer, TargetDetail RSSI/MAC buffers, TargetRadar battery buffer
- Removed dead code from BootSequence (onboarding references)
- Removed unused variable in BruceWiFi probe handler
- Cleaned up 14 unused mock files and empty directories

### Changed
- Refactored SystemTask::handleActionStart() into per-protocol helpers (startWiFiAction, startBLEAction, startIRAction, startPortalAction)
- Standardized all serial log tags to uppercase format ([SYSTEM], [ENGINE], [SCAN], etc.)
- Added `-Wshadow` to native test build flags
- PlatformIO cache added to release.yml workflow

## [v1.0.2] - 2026-02-28

### Added
- 14 attacks operational: Deauth All, Deauth Station, Beacon Flood, Evil Portal, Handshake Capture, PMKID Capture, Probe Flood, Probe Monitor, BLE Spam, BLE Skimmer Detect, IR TV-B-Gone, IR Receive
- BLE skimmer detection with native unit tests
- Legal disclaimer shown at boot and in About panel
- ScanSelector with per-scan-type descriptions
- Full-screen main menu with consistent navigation
- About panel with version, build info, and attribution
- Settings menu renamed for clarity

### Fixed
- BLE scan crash caused by NimBLE callback race condition
- UI flickering during rapid scan updates

### Changed
- Complete UI overhaul: consistent theme, full-screen layouts, improved readability
- Refactored adapter layer for cleaner separation from Bruce firmware

## [v1.0.0-alpha] - 2026-01-12

### Added
- Initial public release
- WiFi AP and station scanning
- Basic deauth attack
- BLE device enumeration
- IR TV-B-Gone support
- Target-first workflow with context-aware action filtering
- Boot sequence animation
- Native test framework (googletest on PlatformIO)
