# Contributing to VANGUARD

Thank you for your interest in contributing. This document covers what you need to build, test, and submit changes.

## Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- ESP32-S3 toolchain (installed automatically by PlatformIO)
- Git

## Build

```bash
# Build firmware
pio run -e m5stack-cardputer

# Flash to device
pio run -t upload

# Run native tests
pio test -e native --verbose
```

## Code Style

- **C++17**. Use modern features (auto, structured bindings, constexpr) where they improve clarity.
- **Namespace**: All project code lives under the `Vanguard` namespace.
- **Singletons**: Adapters (BruceWiFi, BruceBLE, BruceIR) use the singleton pattern. Access via `getInstance()`.
- **Naming**: PascalCase for classes and enums, camelCase for functions and variables, UPPER_SNAKE for constants.
- **Headers**: Use `#pragma once`. Keep includes minimal.
- **No raw `new`/`delete`**: Use smart pointers or stack allocation.

## Test Requirements

All native tests must pass before a PR can be merged:

```bash
pio test -e native --verbose
```

Tests run on the host machine (not on hardware) using googletest. If you add a new feature, add corresponding tests in `test/`.

## Pull Request Process

1. Fork the repository and create a feature branch from `main`.
2. Make your changes. Keep commits focused and descriptive.
3. Ensure `pio test -e native` passes with zero failures.
4. Ensure `pio run -e m5stack-cardputer` compiles without errors.
5. Open a PR against `main`. Describe what changed and why.
6. CI must pass (build, tests, cppcheck) before review.

## Reporting Issues

Use the [issue templates](.github/ISSUE_TEMPLATE/) for bug reports and feature requests.

## License

By contributing, you agree that your contributions will be licensed under the AGPL-3.0 license.
