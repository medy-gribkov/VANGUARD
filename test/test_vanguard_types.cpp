#include <gtest/gtest.h>
#include "../../test/mocks/Arduino.h"
#include "../../src/core/VanguardTypes.h"

using namespace Vanguard;

class TargetTest : public ::testing::Test {
protected:
    Target makeTarget() {
        Target t;
        memset(&t, 0, sizeof(Target));
        uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
        memcpy(t.bssid, mac, 6);
        strcpy(t.ssid, "TestNet");
        t.type = TargetType::ACCESS_POINT;
        t.security = SecurityType::WPA2_PSK;
        t.rssi = -50;
        t.channel = 6;
        t.lastSeenMs = 1000;
        return t;
    }
};

// =============================================================================
// hasClients Tests
// =============================================================================

TEST_F(TargetTest, HasClientsZero) {
    Target t = makeTarget();
    t.clientCount = 0;
    EXPECT_FALSE(t.hasClients());
}

TEST_F(TargetTest, HasClientsOne) {
    Target t = makeTarget();
    t.clientCount = 1;
    EXPECT_TRUE(t.hasClients());
}

TEST_F(TargetTest, HasClientsMax) {
    Target t = makeTarget();
    t.clientCount = MAX_CLIENTS_PER_AP;
    EXPECT_TRUE(t.hasClients());
}

// =============================================================================
// addClientMac Tests
// =============================================================================

TEST_F(TargetTest, AddClientMacBasic) {
    Target t = makeTarget();
    uint8_t client[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x01};

    EXPECT_TRUE(t.addClientMac(client));
    EXPECT_EQ(t.clientCount, 1);
    EXPECT_TRUE(t.hasClient(client));
}

TEST_F(TargetTest, AddClientMacDuplicate) {
    Target t = makeTarget();
    uint8_t client[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x01};

    EXPECT_TRUE(t.addClientMac(client));
    EXPECT_FALSE(t.addClientMac(client));  // Duplicate
    EXPECT_EQ(t.clientCount, 1);
}

TEST_F(TargetTest, AddClientMacAtMaxBoundary) {
    Target t = makeTarget();

    // Fill to MAX
    for (uint8_t i = 0; i < MAX_CLIENTS_PER_AP; i++) {
        uint8_t client[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, i};
        EXPECT_TRUE(t.addClientMac(client));
    }

    EXPECT_EQ(t.clientCount, MAX_CLIENTS_PER_AP);

    // One more should fail
    uint8_t overflow[6] = {0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xFF};
    EXPECT_FALSE(t.addClientMac(overflow));
    EXPECT_EQ(t.clientCount, MAX_CLIENTS_PER_AP);
}

// =============================================================================
// hasClient Tests
// =============================================================================

TEST_F(TargetTest, HasClientNotFound) {
    Target t = makeTarget();
    uint8_t client[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x01};
    EXPECT_FALSE(t.hasClient(client));
}

TEST_F(TargetTest, HasClientFound) {
    Target t = makeTarget();
    uint8_t client[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x01};
    t.addClientMac(client);
    EXPECT_TRUE(t.hasClient(client));
}

// =============================================================================
// isStale Tests
// =============================================================================

TEST_F(TargetTest, IsStaleNotYet) {
    Target t = makeTarget();
    t.lastSeenMs = 1000;

    // Just under the timeout
    EXPECT_FALSE(t.isStale(1000 + TARGET_AGE_TIMEOUT));
}

TEST_F(TargetTest, IsStaleExact) {
    Target t = makeTarget();
    t.lastSeenMs = 1000;

    // Exactly at timeout boundary (> not >=, so equal is NOT stale)
    EXPECT_FALSE(t.isStale(1000 + TARGET_AGE_TIMEOUT));
}

TEST_F(TargetTest, IsStaleOver) {
    Target t = makeTarget();
    t.lastSeenMs = 1000;

    // One ms over the timeout
    EXPECT_TRUE(t.isStale(1000 + TARGET_AGE_TIMEOUT + 1));
}

TEST_F(TargetTest, IsStaleWayOver) {
    Target t = makeTarget();
    t.lastSeenMs = 0;

    EXPECT_TRUE(t.isStale(TARGET_AGE_TIMEOUT + 100));
}

// =============================================================================
// getSignalStrength Tests
// =============================================================================

TEST_F(TargetTest, SignalStrengthExcellent) {
    Target t = makeTarget();
    t.rssi = -40;  // > -50
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::EXCELLENT);
}

TEST_F(TargetTest, SignalStrengthExactExcellentBoundary) {
    Target t = makeTarget();
    t.rssi = RSSI_EXCELLENT;  // -50 exactly (not > -50, so falls to GOOD)
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::GOOD);
}

TEST_F(TargetTest, SignalStrengthGood) {
    Target t = makeTarget();
    t.rssi = -55;  // > -60
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::GOOD);
}

TEST_F(TargetTest, SignalStrengthFair) {
    Target t = makeTarget();
    t.rssi = -65;  // > -70
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::FAIR);
}

TEST_F(TargetTest, SignalStrengthWeak) {
    Target t = makeTarget();
    t.rssi = -75;  // > -80
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::WEAK);
}

TEST_F(TargetTest, SignalStrengthPoor) {
    Target t = makeTarget();
    t.rssi = -90;  // < -80
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::POOR);
}

TEST_F(TargetTest, SignalStrengthExactWeakBoundary) {
    Target t = makeTarget();
    t.rssi = RSSI_WEAK;  // -80 exactly (not > -80, so falls to POOR)
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::POOR);
}

// =============================================================================
// formatBssid Tests
// =============================================================================

TEST_F(TargetTest, FormatBssidCorrect) {
    Target t = makeTarget();
    // bssid is {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
    char buf[18];
    t.formatBssid(buf);
    EXPECT_STREQ(buf, "AA:BB:CC:DD:EE:FF");
}

TEST_F(TargetTest, FormatBssidAllZeros) {
    Target t;
    memset(&t, 0, sizeof(Target));
    char buf[18];
    t.formatBssid(buf);
    EXPECT_STREQ(buf, "00:00:00:00:00:00");
}

// =============================================================================
// isOpen Tests
// =============================================================================

TEST_F(TargetTest, IsOpenTrue) {
    Target t = makeTarget();
    t.security = SecurityType::OPEN;
    EXPECT_TRUE(t.isOpen());
}

TEST_F(TargetTest, IsOpenFalseWPA2) {
    Target t = makeTarget();
    t.security = SecurityType::WPA2_PSK;
    EXPECT_FALSE(t.isOpen());
}

TEST_F(TargetTest, IsOpenFalseUnknown) {
    Target t = makeTarget();
    t.security = SecurityType::UNKNOWN;
    EXPECT_FALSE(t.isOpen());
}

// =============================================================================
// Target Struct Edge Cases (T8)
// =============================================================================

TEST_F(TargetTest, EmptySSID) {
    Target t;
    memset(&t, 0, sizeof(t));
    EXPECT_EQ(strlen(t.ssid), 0u);
    EXPECT_EQ(t.ssid[0], '\0');
}

TEST_F(TargetTest, MaxLengthSSID) {
    Target t;
    memset(&t, 0, sizeof(t));

    // Fill SSID to exactly SSID_MAX_LEN
    for (size_t i = 0; i < SSID_MAX_LEN; i++) {
        t.ssid[i] = 'A' + (char)(i % 26);
    }
    t.ssid[SSID_MAX_LEN] = '\0';

    EXPECT_EQ(strlen(t.ssid), SSID_MAX_LEN);
}

TEST_F(TargetTest, RssiExtremeNegative) {
    Target t = makeTarget();
    t.rssi = -128; // int8_t minimum
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::POOR);
    EXPECT_FALSE(t.rssi > RSSI_WEAK);
}

TEST_F(TargetTest, RssiZero) {
    Target t = makeTarget();
    t.rssi = 0;
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::EXCELLENT);
}

TEST_F(TargetTest, RssiPositive) {
    Target t = makeTarget();
    t.rssi = 127; // int8_t maximum (unusual but valid)
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::EXCELLENT);
}

TEST_F(TargetTest, MaxClientsOverflow) {
    Target t = makeTarget();

    // Fill to MAX_CLIENTS_PER_AP
    for (uint8_t i = 0; i < MAX_CLIENTS_PER_AP; i++) {
        uint8_t mac[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, i};
        t.addClientMac(mac);
    }
    EXPECT_EQ(t.clientCount, MAX_CLIENTS_PER_AP);

    // Attempt overflow
    uint8_t extra[6] = {0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xFF};
    EXPECT_FALSE(t.addClientMac(extra));
    EXPECT_EQ(t.clientCount, MAX_CLIENTS_PER_AP);
}

TEST_F(TargetTest, AllSecurityTypes) {
    Target t = makeTarget();

    t.security = SecurityType::OPEN;
    EXPECT_TRUE(t.isOpen());

    t.security = SecurityType::WEP;
    EXPECT_FALSE(t.isOpen());

    t.security = SecurityType::WPA_PSK;
    EXPECT_FALSE(t.isOpen());

    t.security = SecurityType::WPA2_PSK;
    EXPECT_FALSE(t.isOpen());

    t.security = SecurityType::WPA2_ENTERPRISE;
    EXPECT_FALSE(t.isOpen());

    t.security = SecurityType::WPA3_SAE;
    EXPECT_FALSE(t.isOpen());

    t.security = SecurityType::UNKNOWN;
    EXPECT_FALSE(t.isOpen());
}

TEST_F(TargetTest, AllTargetTypes) {
    Target t = makeTarget();

    // Verify all enum values compile and are distinct
    t.type = TargetType::UNKNOWN;
    t.type = TargetType::ACCESS_POINT;
    t.type = TargetType::STATION;
    t.type = TargetType::BLE_DEVICE;
    t.type = TargetType::BLE_BEACON;
    t.type = TargetType::BLE_SKIMMER;
    t.type = TargetType::RF_DEVICE;
    t.type = TargetType::IR_DEVICE;

    EXPECT_NE(TargetType::ACCESS_POINT, TargetType::BLE_DEVICE);
    EXPECT_NE(TargetType::STATION, TargetType::IR_DEVICE);
}

TEST_F(TargetTest, IsStaleWithOverflow) {
    Target t = makeTarget();
    // Test near uint32_t boundary
    t.lastSeenMs = 0xFFFFFFFF - 100;

    // (0 - (0xFFFFFFFF - 100)) wraps to 101 in uint32_t arithmetic
    // 101 < TARGET_AGE_TIMEOUT (60000), so NOT stale
    EXPECT_FALSE(t.isStale(0));

    // But with large enough gap, it is stale
    t.lastSeenMs = 0;
    EXPECT_TRUE(t.isStale(TARGET_AGE_TIMEOUT + 1));
}
