#ifndef MOCK_BRUCE_BLE_H
#define MOCK_BRUCE_BLE_H

namespace Vanguard {

class BruceBLE {
public:
    static BruceBLE& getInstance() { static BruceBLE i; return i; }
    void init() {}
    void tick() {}
};

}

#endif
