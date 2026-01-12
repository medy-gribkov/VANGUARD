#ifndef MOCK_BRUCE_WIFI_H
#define MOCK_BRUCE_WIFI_H

#include <cstdint>
#include <functional>

namespace Vanguard {

class BruceWiFi {
public:
    static BruceWiFi& getInstance() { static BruceWiFi i; return i; }
    
    bool onEnable() { return true; }
    void onDisable() {}
    bool init() { return true; }
    void shutdown() {}
    void onTick() {}
    
    void beginScan() {}
    void stopScan() {}
    void onScanComplete(std::function<void(int)> cb) {}
    void onAssociation(std::function<void(const uint8_t*, const uint8_t*)> cb) {}
    
    bool deauthStation(const uint8_t* target, const uint8_t* ap, uint8_t ch) { return true; }
    bool deauthAll(const uint8_t* ap, uint8_t ch) { return true; }
    bool captureHandshake(const uint8_t* ap, uint8_t ch, bool deauth) { return true; }
    bool beaconFlood(const char** ssids, size_t count, uint8_t ch) { return true; }
    
    void stopHardwareActivities() {}
    uint32_t getPacketsSent() const { return 0; }
    bool hasHandshake() const { return false; }
    void onHandshakeCaptured(std::function<void(const uint8_t*)> cb) {}
    void onWidsAlert(std::function<void(int, int)> cb) {}
    void setPcapLogging(bool en, const char* f = nullptr) {}
};


}

#endif
