#ifndef MOCK_WIFI_H
#define MOCK_WIFI_H

#include <cstdint>

enum { WIFI_SCAN_RUNNING = -1, WIFI_SCAN_FAILED = -2 };

struct MockWiFi {
    int16_t scanNetworks(bool async = false, bool show_hidden = false, bool passive = false, uint32_t max_ms_per_chan = 300) { return 0; }
    int16_t scanComplete() { return 0; }
    void scanDelete() {}
};

extern MockWiFi WiFi;

#endif
