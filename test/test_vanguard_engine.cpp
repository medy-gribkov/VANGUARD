// VanguardEngine tests are excluded from native builds because VanguardEngine.cpp
// depends on ESP32 WiFi/BLE hardware APIs (WiFi.BSSID, NimBLE, esp_wifi) that
// cannot be mocked without significant refactoring. The engine's core logic flows
// through IPC events from SystemTask on Core 0, making it inherently hardware-coupled.
//
// To test on device: pio test -e m5stack-cardputer
// For native: test_action_resolver.cpp and test_target_table.cpp cover the
// engine's data layer (ActionResolver, TargetTable) which contain the testable logic.
//
// TODO: Refactor VanguardEngine to accept adapter interfaces via dependency injection
// to enable native unit testing of the state machine and IPC event handling.
