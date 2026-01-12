#ifndef MOCK_EVIL_PORTAL_H
#define MOCK_EVIL_PORTAL_H

namespace Vanguard {

class EvilPortal {
public:
    static EvilPortal& getInstance() { static EvilPortal i; return i; }
    bool isRunning() { return false; }
    void stop() {}
    bool start(const char* ssid, uint8_t ch, int type) { return true; }
    int getClientCount() { return 0; }
};

}

#endif
