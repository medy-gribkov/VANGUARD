#ifndef MOCK_BRUCE_BLE_H
#define MOCK_BRUCE_BLE_H

namespace Vanguard {

class BruceBLE {
public:
    static BruceBLE& getInstance() { static BruceBLE i; return i; }
    
    bool init() { return true; }
    void shutdown() {}
    void onTick() {}
    
    void beginScan(uint32_t duration = 0) {}
    void stopScan() {}
    void onDeviceFound(std::function<void(const struct BLEDeviceInfo&)> cb) {}
    void onScanComplete(std::function<void(int)> cb) {}
    
    bool startSpam(int type) { return true; }
    void stopHardwareActivities() {}
    uint32_t getAdvertisementsSent() const { return 0; }
};

}

#endif
