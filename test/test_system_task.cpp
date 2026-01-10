#include <gtest/gtest.h>
#include "Arduino.h"
#include "SystemTask.h"
#include "IPC.h"

using namespace Vanguard;

class SystemTaskTest : public ::testing::Test {
protected:
    SystemTask& system = SystemTask::getInstance();

    void SetUp() override {
        // SystemTask doesn't have a reset method, but we can check initial state via sendRequest
    }
};

TEST_F(SystemTaskTest, SingletonInstance) {
    SystemTask& s1 = SystemTask::getInstance();
    SystemTask& s2 = SystemTask::getInstance();
    EXPECT_EQ(&s1, &s2);
}

TEST_F(SystemTaskTest, SendRequest) {
    SystemRequest req;
    req.cmd = SysCommand::WIFI_SCAN_START;
    req.payload = nullptr;
    
    EXPECT_TRUE(system.sendRequest(req));
}

TEST_F(SystemTaskTest, EventFlow) {
    SystemEvent evt;
    // Initially no events
    EXPECT_FALSE(system.receiveEvent(evt));
    
    // In a real test we'd need to mock the internal run() loop to process requests and generate events
    // For now, this verifies the IPC structures can be instantiated and basic methods called
}
