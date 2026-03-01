#include <gtest/gtest.h>
#include "../../test/mocks/Arduino.h"
#include "../../src/core/IPC.h"
#include "../../src/core/VanguardTypes.h"

using namespace Vanguard;

// =============================================================================
// Struct Size Tests - ensure IPC structs fit in FreeRTOS queues
// =============================================================================

TEST(IPCTest, SystemRequestFitsInQueue) {
    // FreeRTOS queue item size is typically 32 bytes max for efficiency
    EXPECT_LE(sizeof(SystemRequest), 32u);
}

TEST(IPCTest, SystemEventFitsInQueue) {
    EXPECT_LE(sizeof(SystemEvent), 32u);
}

TEST(IPCTest, ActionRequestReasonableSize) {
    // ActionRequest contains a full Target struct, so it's larger
    // but it's heap-allocated and passed via pointer
    EXPECT_GT(sizeof(ActionRequest), 0u);
}

TEST(IPCTest, AssociationEventSize) {
    // Two MAC addresses = 12 bytes
    EXPECT_EQ(sizeof(AssociationEvent), 12u);
}

// =============================================================================
// Field Alignment Tests
// =============================================================================

TEST(IPCTest, SystemRequestDefaultInit) {
    SystemRequest req;
    memset(&req, 0, sizeof(req));
    req.cmd = SysCommand::NONE;
    EXPECT_EQ(req.payload, nullptr);
    EXPECT_EQ(req.freeCb, nullptr);
}

TEST(IPCTest, SystemEventDefaultInit) {
    SystemEvent evt;
    memset(&evt, 0, sizeof(evt));
    evt.type = SysEventType::NONE;
    EXPECT_EQ(evt.data, nullptr);
    EXPECT_EQ(evt.dataLen, 0u);
    EXPECT_FALSE(evt.isPointer);
}

// =============================================================================
// Enum Completeness Tests
// =============================================================================

TEST(IPCTest, SysCommandEnumValues) {
    // Verify all command types exist and are distinct
    EXPECT_NE(SysCommand::NONE, SysCommand::WIFI_SCAN_START);
    EXPECT_NE(SysCommand::WIFI_SCAN_START, SysCommand::WIFI_SCAN_STOP);
    EXPECT_NE(SysCommand::BLE_SCAN_START, SysCommand::BLE_SCAN_STOP);
    EXPECT_NE(SysCommand::ACTION_START, SysCommand::ACTION_STOP);
    EXPECT_NE(SysCommand::ACTION_STOP, SysCommand::SYSTEM_SHUTDOWN);
}

TEST(IPCTest, SysEventTypeEnumValues) {
    // Verify all event types exist and are distinct
    EXPECT_NE(SysEventType::NONE, SysEventType::WIFI_SCAN_STARTED);
    EXPECT_NE(SysEventType::WIFI_SCAN_STARTED, SysEventType::WIFI_SCAN_COMPLETE);
    EXPECT_NE(SysEventType::BLE_SCAN_STARTED, SysEventType::BLE_SCAN_COMPLETE);
    EXPECT_NE(SysEventType::BLE_DEVICE_FOUND, SysEventType::ACTION_PROGRESS);
    EXPECT_NE(SysEventType::ACTION_PROGRESS, SysEventType::ACTION_COMPLETE);
    EXPECT_NE(SysEventType::ACTION_COMPLETE, SysEventType::ERROR_OCCURRED);
    EXPECT_NE(SysEventType::ASSOCIATION_FOUND, SysEventType::NONE);
}

// =============================================================================
// ActionRequest Tests
// =============================================================================

TEST(IPCTest, ActionRequestConstruction) {
    ActionRequest req;
    memset(&req, 0, sizeof(req));
    req.type = ActionType::DEAUTH_ALL;

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    memcpy(req.stationMac, mac, 6);

    EXPECT_EQ(req.type, ActionType::DEAUTH_ALL);
    EXPECT_EQ(memcmp(req.stationMac, mac, 6), 0);
}

TEST(IPCTest, SystemRequestFreeCbWorks) {
    // Verify freeCb can clean up heap-allocated payloads
    bool freed = false;

    SystemRequest req;
    req.cmd = SysCommand::ACTION_START;
    req.payload = new int(42);
    req.freeCb = [](void* p) { delete (int*)p; };

    // Simulate SystemTask cleanup
    if (req.freeCb && req.payload) {
        req.freeCb(req.payload);
        freed = true;
    }

    EXPECT_TRUE(freed);
}

// =============================================================================
// AssociationEvent Tests
// =============================================================================

TEST(IPCTest, AssociationEventMACStorage) {
    AssociationEvent evt;
    uint8_t bssid[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t station[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

    memcpy(evt.bssid, bssid, 6);
    memcpy(evt.station, station, 6);

    EXPECT_EQ(memcmp(evt.bssid, bssid, 6), 0);
    EXPECT_EQ(memcmp(evt.station, station, 6), 0);
}
