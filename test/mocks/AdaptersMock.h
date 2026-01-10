#ifndef MOCK_ADAPTERS_H
#define MOCK_ADAPTERS_H

#include <cstdint>
#include <functional>

namespace Vanguard {

class BruceWiFi {
public:
    static BruceWiFi& getInstance() { static BruceWiFi i; return i; }
    bool init() { return true; }
    void onAssociation(std::function<void(const uint8_t*, const uint8_t*)> cb) {}
};

class BruceBLE {
public:
    static BruceBLE& getInstance() { static BruceBLE i; return i; }
    void tick() {}
};

class BruceIR {
public:
    static BruceIR& getInstance() { static BruceIR i; return i; }
    void init() {}
};

class EvilPortal {
public:
    static EvilPortal& getInstance() { static EvilPortal i; return i; }
    bool isRunning() { return false; }
    void stop() {}
};

}

#endif
