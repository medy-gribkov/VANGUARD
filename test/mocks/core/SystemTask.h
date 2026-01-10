#ifndef MOCK_SYSTEM_TASK_H
#define MOCK_SYSTEM_TASK_H

#include "IPC.h"

namespace Vanguard {

class SystemTask {
public:
    static SystemTask& getInstance() {
        static SystemTask instance;
        return instance;
    }
    void start() {}
    bool sendRequest(const SystemRequest& req) { return true; }
    bool receiveEvent(SystemEvent& evt) { return false; }
};

}

#endif
