#ifndef MOCK_SD_MANAGER_H
#define MOCK_SD_MANAGER_H

namespace Vanguard {

class SDManager {
public:
    static SDManager& getInstance() { static SDManager i; return i; }
    void init() {}
};

}

#endif
