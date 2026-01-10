#ifndef MOCK_FEEDBACK_MANAGER_H
#define MOCK_FEEDBACK_MANAGER_H

#include <cstdint>

namespace Vanguard {

class FeedbackManager {
public:
    static FeedbackManager& getInstance() { static FeedbackManager i; return i; }
    void init() {}
    void beep(uint32_t freq, uint32_t dur) {}
    void pulse(uint8_t force) {}
};

}

#endif
