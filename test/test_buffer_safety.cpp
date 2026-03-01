#include <gtest/gtest.h>
#include "../../test/mocks/Arduino.h"
#include "../../src/core/VanguardTypes.h"
#include "../../src/core/IPC.h"

using namespace Vanguard;

// =============================================================================
// Target.ssid Buffer Tests
// =============================================================================

TEST(BufferSafetyTest, TargetSsidMaxLength) {
    Target t;
    memset(&t, 0, sizeof(t));

    // SSID_MAX_LEN is 32, buffer is 33 (32 + null)
    EXPECT_EQ(sizeof(t.ssid), SSID_MAX_LEN + 1);
}

TEST(BufferSafetyTest, TargetSsidFullFill) {
    Target t;
    memset(&t, 0, sizeof(t));

    // Fill to max with strncpy (safe)
    char longSsid[64];
    memset(longSsid, 'A', 63);
    longSsid[63] = '\0';

    strncpy(t.ssid, longSsid, SSID_MAX_LEN);
    t.ssid[SSID_MAX_LEN] = '\0';

    EXPECT_EQ(strlen(t.ssid), SSID_MAX_LEN);
    EXPECT_EQ(t.ssid[SSID_MAX_LEN], '\0');
}

TEST(BufferSafetyTest, TargetSsidEmpty) {
    Target t;
    memset(&t, 0, sizeof(t));

    EXPECT_EQ(strlen(t.ssid), 0u);
    EXPECT_EQ(t.ssid[0], '\0');
}

// =============================================================================
// Target.bssid Buffer Tests
// =============================================================================

TEST(BufferSafetyTest, TargetBssidSize) {
    Target t;
    EXPECT_EQ(sizeof(t.bssid), 6u);
}

TEST(BufferSafetyTest, TargetBssidAllZeros) {
    Target t;
    memset(&t, 0, sizeof(t));

    uint8_t zeros[6] = {0};
    EXPECT_EQ(memcmp(t.bssid, zeros, 6), 0);
}

TEST(BufferSafetyTest, TargetBssidAllOnes) {
    Target t;
    memset(&t, 0, sizeof(t));

    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(t.bssid, broadcast, 6);
    EXPECT_EQ(memcmp(t.bssid, broadcast, 6), 0);
}

// =============================================================================
// Target.clientMacs Buffer Tests
// =============================================================================

TEST(BufferSafetyTest, ClientMacsArraySize) {
    Target t;
    // MAX_CLIENTS_PER_AP entries, each 6 bytes
    EXPECT_EQ(sizeof(t.clientMacs), MAX_CLIENTS_PER_AP * 6u);
}

TEST(BufferSafetyTest, ClientMacsOverflowProtection) {
    Target t;
    memset(&t, 0, sizeof(t));

    // Fill all client slots
    for (uint8_t i = 0; i < MAX_CLIENTS_PER_AP; i++) {
        uint8_t mac[6] = {0xCC, 0x00, 0x00, 0x00, 0x00, i};
        EXPECT_TRUE(t.addClientMac(mac));
    }

    // 17th client should be rejected
    uint8_t overflow[6] = {0xDD, 0x00, 0x00, 0x00, 0x00, 0xFF};
    EXPECT_FALSE(t.addClientMac(overflow));
    EXPECT_EQ(t.clientCount, MAX_CLIENTS_PER_AP);
}

// =============================================================================
// ActionProgress.statusText Buffer Tests
// =============================================================================

TEST(BufferSafetyTest, ActionProgressStatusTextSize) {
    ActionProgress p;
    EXPECT_EQ(sizeof(p.statusText), 32u);
}

TEST(BufferSafetyTest, ActionProgressStatusTextFullFill) {
    ActionProgress p;
    memset(&p, 0, sizeof(p));

    // Fill to max with safe strncpy
    char longMsg[64];
    memset(longMsg, 'X', 63);
    longMsg[63] = '\0';

    strncpy(p.statusText, longMsg, sizeof(p.statusText) - 1);
    p.statusText[sizeof(p.statusText) - 1] = '\0';

    EXPECT_EQ(strlen(p.statusText), 31u);
    EXPECT_EQ(p.statusText[31], '\0');
}

// =============================================================================
// formatBssid Buffer Tests
// =============================================================================

TEST(BufferSafetyTest, FormatBssidMinBuffer) {
    Target t;
    memset(&t, 0, sizeof(t));
    t.bssid[0] = 0xFF;
    t.bssid[1] = 0xFF;
    t.bssid[2] = 0xFF;
    t.bssid[3] = 0xFF;
    t.bssid[4] = 0xFF;
    t.bssid[5] = 0xFF;

    char buf[18]; // Exactly 17 chars + null
    t.formatBssid(buf);

    EXPECT_STREQ(buf, "FF:FF:FF:FF:FF:FF");
    EXPECT_EQ(strlen(buf), 17u);
}

// =============================================================================
// RSSI Edge Cases
// =============================================================================

TEST(BufferSafetyTest, RssiExtremeValues) {
    Target t;
    memset(&t, 0, sizeof(t));

    // int8_t range: -128 to 127
    t.rssi = -128;
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::POOR);

    t.rssi = 0;
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::EXCELLENT);

    t.rssi = 127;
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::EXCELLENT);

    t.rssi = -1;
    EXPECT_EQ(t.getSignalStrength(), SignalStrength::EXCELLENT);
}

// =============================================================================
// Struct Zero-Init Safety
// =============================================================================

TEST(BufferSafetyTest, TargetZeroInitValid) {
    Target t;
    memset(&t, 0, sizeof(t));

    // Zero-init should be safe for all operations
    EXPECT_FALSE(t.hasClients());
    EXPECT_TRUE(t.isOpen()); // SecurityType::OPEN = 0, memset(0) sets OPEN
    EXPECT_EQ(t.beaconCount, 0u);
    EXPECT_EQ(t.channel, 0u);
    EXPECT_EQ(strlen(t.ssid), 0u);
}

TEST(BufferSafetyTest, ActionProgressZeroInitValid) {
    ActionProgress p;
    memset(&p, 0, sizeof(p));

    EXPECT_EQ(p.packetsSent, 0u);
    EXPECT_EQ(p.elapsedMs, 0u);
    EXPECT_EQ(p.startTimeMs, 0u);
    EXPECT_EQ(strlen(p.statusText), 0u);
}
