#include <gtest/gtest.h>
#include "Arduino.h"
#include "SystemTask.h"
#include "IPC.h"

using namespace Vanguard;

// SystemTask mock (test/mocks/core/SystemTask.h) always returns true for
// sendRequest and false for receiveEvent. These tests verify the IPC
// structures are correctly sized and the mock interface works as expected.

class SystemTaskTest : public ::testing::Test {
protected:
    SystemTask& system = SystemTask::getInstance();
};

TEST_F(SystemTaskTest, SingletonInstance) {
    SystemTask& s1 = SystemTask::getInstance();
    SystemTask& s2 = SystemTask::getInstance();
    EXPECT_EQ(&s1, &s2);
}

TEST_F(SystemTaskTest, SendWiFiScanRequest) {
    SystemRequest req;
    memset(&req, 0, sizeof(req));
    req.cmd = SysCommand::WIFI_SCAN_START;
    req.payload = nullptr;
    req.freeCb = nullptr;
    EXPECT_TRUE(system.sendRequest(req));
}

TEST_F(SystemTaskTest, SendBLEScanRequest) {
    SystemRequest req;
    memset(&req, 0, sizeof(req));
    req.cmd = SysCommand::BLE_SCAN_START;
    req.payload = (void*)(uintptr_t)5000; // 5 sec duration
    req.freeCb = nullptr;
    EXPECT_TRUE(system.sendRequest(req));
}

TEST_F(SystemTaskTest, SendActionStartRequest) {
    SystemRequest req;
    memset(&req, 0, sizeof(req));
    req.cmd = SysCommand::ACTION_START;

    ActionRequest* action = new ActionRequest();
    action->type = ActionType::DEAUTH_ALL;
    memset(&action->target, 0, sizeof(Target));
    memset(action->stationMac, 0, 6);

    req.payload = action;
    req.freeCb = [](void* p) { delete (ActionRequest*)p; };

    EXPECT_TRUE(system.sendRequest(req));

    // Clean up (in real code, SystemTask calls freeCb after processing)
    if (req.freeCb && req.payload) {
        req.freeCb(req.payload);
    }
}

TEST_F(SystemTaskTest, SendActionStopRequest) {
    SystemRequest req;
    memset(&req, 0, sizeof(req));
    req.cmd = SysCommand::ACTION_STOP;
    req.payload = nullptr;
    req.freeCb = nullptr;
    EXPECT_TRUE(system.sendRequest(req));
}

TEST_F(SystemTaskTest, NoEventsInitially) {
    SystemEvent evt;
    memset(&evt, 0, sizeof(evt));
    // Mock always returns false (no events in queue)
    EXPECT_FALSE(system.receiveEvent(evt));
}

TEST_F(SystemTaskTest, SystemRequestStructSize) {
    // Verify struct sizes are reasonable for FreeRTOS queue
    EXPECT_LE(sizeof(SystemRequest), 32u);
    EXPECT_LE(sizeof(SystemEvent), 32u);
}

TEST_F(SystemTaskTest, AllCommandEnumsValid) {
    // Verify all command types can be constructed
    SystemRequest req;
    memset(&req, 0, sizeof(req));

    req.cmd = SysCommand::WIFI_SCAN_START;
    EXPECT_TRUE(system.sendRequest(req));

    req.cmd = SysCommand::WIFI_SCAN_STOP;
    EXPECT_TRUE(system.sendRequest(req));

    req.cmd = SysCommand::BLE_SCAN_START;
    EXPECT_TRUE(system.sendRequest(req));

    req.cmd = SysCommand::BLE_SCAN_STOP;
    EXPECT_TRUE(system.sendRequest(req));

    req.cmd = SysCommand::ACTION_START;
    EXPECT_TRUE(system.sendRequest(req));

    req.cmd = SysCommand::ACTION_STOP;
    EXPECT_TRUE(system.sendRequest(req));

    req.cmd = SysCommand::SYSTEM_SHUTDOWN;
    EXPECT_TRUE(system.sendRequest(req));
}
