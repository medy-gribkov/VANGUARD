#ifndef MOCK_BRUCE_WIFI_H
#define MOCK_BRUCE_WIFI_H

#include <cstdint>
#include <functional>

namespace Vanguard {

class BruceWiFi {
public:
    static BruceWiFi& getInstance() { static BruceWiFi i; return i; }
    bool init() { return true; }
    void onAssociation(std::function<void(const uint8_t*, const uint8_t*)> cb) {}
};

}

#endif
