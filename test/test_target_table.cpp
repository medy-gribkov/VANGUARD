#include <gtest/gtest.h>
#include "../../test/mocks/Arduino.h"
#include "../../src/core/TargetTable.h"
#include "../../src/core/VanguardTypes.h"

using namespace Vanguard;

class TargetTableTest : public ::testing::Test {
protected:
    TargetTable table;

    void SetUp() override {
        table.clear();
    }

    Target makeTarget(uint8_t lastByte, const char* ssid, int8_t rssi = -50,
                      TargetType type = TargetType::ACCESS_POINT,
                      SecurityType sec = SecurityType::WPA2_PSK) {
        Target t;
        memset(&t, 0, sizeof(Target));
        uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, lastByte};
        memcpy(t.bssid, mac, 6);
        strncpy(t.ssid, ssid, SSID_MAX_LEN);
        t.type = type;
        t.rssi = rssi;
        t.security = sec;
        t.channel = 6;
        t.lastSeenMs = 5000;
        return t;
    }
};

// =============================================================================
// Basic CRUD
// =============================================================================

TEST_F(TargetTableTest, AddNewTarget) {
    Target t;
    memset(&t, 0, sizeof(Target));
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    memcpy(t.bssid, mac, 6);
    strcpy(t.ssid, "TestNet");
    t.type = TargetType::ACCESS_POINT;
    t.rssi = -50;

    EXPECT_TRUE(table.addOrUpdate(t));
    EXPECT_EQ(table.count(), 1);

    const Target* found = table.findByBssid(mac);
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found->ssid, "TestNet");
}

TEST_F(TargetTableTest, UpdateExistingTarget) {
    Target t1;
    memset(&t1, 0, sizeof(Target));
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    memcpy(t1.bssid, mac, 6);
    strcpy(t1.ssid, "TestNet");

    table.addOrUpdate(t1);

    Target t2 = t1;
    t2.rssi = -30;

    EXPECT_FALSE(table.addOrUpdate(t2)); // Should return false for update
    EXPECT_EQ(table.count(), 1);

    const Target* found = table.findByBssid(mac);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->rssi, -30);
}

TEST_F(TargetTableTest, ClearRemovesAll) {
    table.addOrUpdate(makeTarget(0x01, "Net1"));
    table.addOrUpdate(makeTarget(0x02, "Net2"));
    table.addOrUpdate(makeTarget(0x03, "Net3"));
    EXPECT_EQ(table.count(), 3);

    table.clear();
    EXPECT_EQ(table.count(), 0);
}

// =============================================================================
// MAX_TARGETS Eviction
// =============================================================================

TEST_F(TargetTableTest, EvictWeakestWhenFull) {
    // Fill table to MAX_TARGETS with rssi = -80
    for (size_t i = 0; i < MAX_TARGETS; i++) {
        char ssid[8];
        snprintf(ssid, sizeof(ssid), "Net%03u", (unsigned)i);
        Target t = makeTarget((uint8_t)i, ssid, -80);
        table.addOrUpdate(t);
    }
    EXPECT_EQ(table.count(), MAX_TARGETS);

    // Add one more with stronger signal, should evict weakest
    Target strong = makeTarget(0xFE, "StrongNet", -30);
    EXPECT_TRUE(table.addOrUpdate(strong));
    EXPECT_EQ(table.count(), MAX_TARGETS);

    // The strong one should be findable
    uint8_t strongMac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFE};
    EXPECT_NE(table.findByBssid(strongMac), nullptr);
}

TEST_F(TargetTableTest, RejectWeakerWhenFull) {
    // Fill table to MAX_TARGETS with rssi = -30 (strong)
    for (size_t i = 0; i < MAX_TARGETS; i++) {
        char ssid[8];
        snprintf(ssid, sizeof(ssid), "Net%03u", (unsigned)i);
        Target t = makeTarget((uint8_t)i, ssid, -30);
        table.addOrUpdate(t);
    }
    EXPECT_EQ(table.count(), MAX_TARGETS);

    // Try to add a weaker one, should be rejected
    Target weak = makeTarget(0xFE, "WeakNet", -90);
    EXPECT_FALSE(table.addOrUpdate(weak));
    EXPECT_EQ(table.count(), MAX_TARGETS);

    uint8_t weakMac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFE};
    EXPECT_EQ(table.findByBssid(weakMac), nullptr);
}

// =============================================================================
// Pruning
// =============================================================================

TEST_F(TargetTableTest, PruneStale) {
    Target t;
    memset(&t, 0, sizeof(Target));
    uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    memcpy(t.bssid, mac, 6);
    t.lastSeenMs = 1000;

    table.addOrUpdate(t);

    // Default timeout is 60000ms
    EXPECT_EQ(table.pruneStale(1000), 0);
    EXPECT_EQ(table.pruneStale(62000), 1);
    EXPECT_EQ(table.count(), 0);
}

TEST_F(TargetTableTest, PruneStaleInterleaved) {
    // Mix of stale and fresh targets
    Target stale1 = makeTarget(0x01, "Stale1");
    stale1.lastSeenMs = 1000;

    Target fresh1 = makeTarget(0x02, "Fresh1");
    fresh1.lastSeenMs = 50000;

    Target stale2 = makeTarget(0x03, "Stale2");
    stale2.lastSeenMs = 2000;

    Target fresh2 = makeTarget(0x04, "Fresh2");
    fresh2.lastSeenMs = 55000;

    table.addOrUpdate(stale1);
    table.addOrUpdate(fresh1);
    table.addOrUpdate(stale2);
    table.addOrUpdate(fresh2);

    EXPECT_EQ(table.count(), 4);

    // At time 63000: stale1 (1000) and stale2 (2000) are stale, fresh ones are not
    size_t removed = table.pruneStale(63000);
    EXPECT_EQ(removed, 2);
    EXPECT_EQ(table.count(), 2);

    // Verify the right ones remain
    uint8_t freshMac1[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x02};
    uint8_t freshMac2[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x04};
    EXPECT_NE(table.findByBssid(freshMac1), nullptr);
    EXPECT_NE(table.findByBssid(freshMac2), nullptr);
}

// =============================================================================
// Filtering & Sorting
// =============================================================================

TEST_F(TargetTableTest, GetFilteredBySignal) {
    table.addOrUpdate(makeTarget(0x01, "Weak", -80));
    table.addOrUpdate(makeTarget(0x02, "Strong", -30));
    table.addOrUpdate(makeTarget(0x03, "Medium", -55));

    TargetFilter filter;
    auto result = table.getFiltered(filter, SortOrder::SIGNAL_STRENGTH);

    ASSERT_EQ(result.size(), 3);
    EXPECT_STREQ(result[0].ssid, "Strong");    // -30 first
    EXPECT_STREQ(result[1].ssid, "Medium");    // -55 second
    EXPECT_STREQ(result[2].ssid, "Weak");      // -80 last
}

TEST_F(TargetTableTest, GetFilteredAlphabetical) {
    table.addOrUpdate(makeTarget(0x01, "Zebra"));
    table.addOrUpdate(makeTarget(0x02, "Alpha"));
    table.addOrUpdate(makeTarget(0x03, "Middle"));

    TargetFilter filter;
    auto result = table.getFiltered(filter, SortOrder::ALPHABETICAL);

    ASSERT_EQ(result.size(), 3);
    EXPECT_STREQ(result[0].ssid, "Alpha");
    EXPECT_STREQ(result[1].ssid, "Middle");
    EXPECT_STREQ(result[2].ssid, "Zebra");
}

TEST_F(TargetTableTest, GetFilteredLastSeen) {
    Target t1 = makeTarget(0x01, "Old");
    t1.lastSeenMs = 1000;
    Target t2 = makeTarget(0x02, "New");
    t2.lastSeenMs = 5000;
    Target t3 = makeTarget(0x03, "Mid");
    t3.lastSeenMs = 3000;

    table.addOrUpdate(t1);
    table.addOrUpdate(t2);
    table.addOrUpdate(t3);

    TargetFilter filter;
    auto result = table.getFiltered(filter, SortOrder::LAST_SEEN);

    ASSERT_EQ(result.size(), 3);
    EXPECT_STREQ(result[0].ssid, "New");   // 5000 first
    EXPECT_STREQ(result[1].ssid, "Mid");   // 3000 second
    EXPECT_STREQ(result[2].ssid, "Old");   // 1000 last
}

TEST_F(TargetTableTest, GetFilteredByClientCount) {
    Target t1 = makeTarget(0x01, "NoClients");
    t1.clientCount = 0;
    Target t2 = makeTarget(0x02, "ManyClients");
    t2.clientCount = 10;
    Target t3 = makeTarget(0x03, "SomeClients");
    t3.clientCount = 3;

    table.addOrUpdate(t1);
    table.addOrUpdate(t2);
    table.addOrUpdate(t3);

    TargetFilter filter;
    auto result = table.getFiltered(filter, SortOrder::CLIENT_COUNT);

    ASSERT_EQ(result.size(), 3);
    EXPECT_STREQ(result[0].ssid, "ManyClients");  // 10 first
    EXPECT_STREQ(result[1].ssid, "SomeClients");  // 3 second
    EXPECT_STREQ(result[2].ssid, "NoClients");     // 0 last
}

TEST_F(TargetTableTest, GetFilteredHideBLE) {
    table.addOrUpdate(makeTarget(0x01, "WiFiAP", -50, TargetType::ACCESS_POINT));
    table.addOrUpdate(makeTarget(0x02, "BLEDev", -60, TargetType::BLE_DEVICE));

    TargetFilter filter;
    filter.showBLE = false;
    auto result = table.getFiltered(filter);

    ASSERT_EQ(result.size(), 1);
    EXPECT_STREQ(result[0].ssid, "WiFiAP");
}

TEST_F(TargetTableTest, GetFilteredHideOpen) {
    table.addOrUpdate(makeTarget(0x01, "Secured", -50, TargetType::ACCESS_POINT, SecurityType::WPA2_PSK));
    table.addOrUpdate(makeTarget(0x02, "OpenNet", -50, TargetType::ACCESS_POINT, SecurityType::OPEN));

    TargetFilter filter;
    filter.showOpen = false;
    auto result = table.getFiltered(filter);

    ASSERT_EQ(result.size(), 1);
    EXPECT_STREQ(result[0].ssid, "Secured");
}

TEST_F(TargetTableTest, GetFilteredMinRssi) {
    table.addOrUpdate(makeTarget(0x01, "Strong", -30));
    table.addOrUpdate(makeTarget(0x02, "Weak", -85));

    TargetFilter filter;
    filter.minRssi = -70;
    auto result = table.getFiltered(filter);

    ASSERT_EQ(result.size(), 1);
    EXPECT_STREQ(result[0].ssid, "Strong");
}

// =============================================================================
// countByType
// =============================================================================

TEST_F(TargetTableTest, CountByTypeMixed) {
    table.addOrUpdate(makeTarget(0x01, "AP1", -50, TargetType::ACCESS_POINT));
    table.addOrUpdate(makeTarget(0x02, "AP2", -50, TargetType::ACCESS_POINT));
    table.addOrUpdate(makeTarget(0x03, "BLE1", -60, TargetType::BLE_DEVICE));
    table.addOrUpdate(makeTarget(0x04, "IR1", -30, TargetType::IR_DEVICE));

    EXPECT_EQ(table.countByType(TargetType::ACCESS_POINT), 2);
    EXPECT_EQ(table.countByType(TargetType::BLE_DEVICE), 1);
    EXPECT_EQ(table.countByType(TargetType::IR_DEVICE), 1);
    EXPECT_EQ(table.countByType(TargetType::STATION), 0);
}

// =============================================================================
// Association
// =============================================================================

TEST_F(TargetTableTest, Association) {
    uint8_t apMac[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    uint8_t cliMac[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};

    Target ap;
    memset(&ap, 0, sizeof(Target));
    memcpy(ap.bssid, apMac, 6);
    ap.type = TargetType::ACCESS_POINT;
    table.addOrUpdate(ap);

    EXPECT_TRUE(table.addAssociation(cliMac, apMac));

    const Target* found = table.findByBssid(apMac);
    EXPECT_EQ(found->clientCount, 1);
    EXPECT_TRUE(found->hasClient(cliMac));
}

TEST_F(TargetTableTest, AssociationDuplicate) {
    uint8_t apMac[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    uint8_t cliMac[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};

    Target ap;
    memset(&ap, 0, sizeof(Target));
    memcpy(ap.bssid, apMac, 6);
    ap.type = TargetType::ACCESS_POINT;
    table.addOrUpdate(ap);

    EXPECT_TRUE(table.addAssociation(cliMac, apMac));
    EXPECT_FALSE(table.addAssociation(cliMac, apMac));  // Duplicate

    const Target* found = table.findByBssid(apMac);
    EXPECT_EQ(found->clientCount, 1);
}

TEST_F(TargetTableTest, AssociationToNonExistentAP) {
    uint8_t apMac[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    uint8_t cliMac[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};

    // Don't add AP first
    EXPECT_FALSE(table.addAssociation(cliMac, apMac));
}

TEST_F(TargetTableTest, AssociationToNonAPTarget) {
    uint8_t bleMac[6] = {0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB};
    uint8_t cliMac[6] = {0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};

    Target ble;
    memset(&ble, 0, sizeof(Target));
    memcpy(ble.bssid, bleMac, 6);
    ble.type = TargetType::BLE_DEVICE;
    table.addOrUpdate(ble);

    // Association to non-AP should fail
    EXPECT_FALSE(table.addAssociation(cliMac, bleMac));
}

// =============================================================================
// findByBssid
// =============================================================================

TEST_F(TargetTableTest, FindByBssidNotFound) {
    uint8_t mac[6] = {0x99, 0x99, 0x99, 0x99, 0x99, 0x99};
    EXPECT_EQ(table.findByBssid(mac), nullptr);
}

TEST_F(TargetTableTest, FindByBssidAfterPrune) {
    Target t = makeTarget(0x01, "PruneMe");
    t.lastSeenMs = 0;
    table.addOrUpdate(t);

    table.pruneStale(TARGET_AGE_TIMEOUT + 100);

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    EXPECT_EQ(table.findByBssid(mac), nullptr);
}

// =============================================================================
// getStrongest
// =============================================================================

TEST_F(TargetTableTest, GetStrongestEmpty) {
    EXPECT_EQ(table.getStrongest(), nullptr);
}

TEST_F(TargetTableTest, GetStrongestMultiple) {
    table.addOrUpdate(makeTarget(0x01, "Weak", -80));
    table.addOrUpdate(makeTarget(0x02, "Strong", -20));
    table.addOrUpdate(makeTarget(0x03, "Medium", -55));

    const Target* strongest = table.getStrongest();
    ASSERT_NE(strongest, nullptr);
    EXPECT_STREQ(strongest->ssid, "Strong");
    EXPECT_EQ(strongest->rssi, -20);
}

// =============================================================================
// Callbacks
// =============================================================================

TEST_F(TargetTableTest, OnAddedCallback) {
    int callCount = 0;
    table.onTargetAdded([&](const Target& t) {
        callCount++;
    });

    table.addOrUpdate(makeTarget(0x01, "New1"));
    EXPECT_EQ(callCount, 1);

    table.addOrUpdate(makeTarget(0x02, "New2"));
    EXPECT_EQ(callCount, 2);
}

TEST_F(TargetTableTest, OnUpdatedCallback) {
    int callCount = 0;
    table.onTargetUpdated([&](const Target& t) {
        callCount++;
    });

    Target t = makeTarget(0x01, "Net");
    table.addOrUpdate(t);
    EXPECT_EQ(callCount, 0);  // Add, not update

    t.rssi = -40;
    table.addOrUpdate(t);
    EXPECT_EQ(callCount, 1);  // Now it's an update
}

TEST_F(TargetTableTest, OnRemovedCallbackPrune) {
    int callCount = 0;
    table.onTargetRemoved([&](const Target& t) {
        callCount++;
    });

    Target t = makeTarget(0x01, "Stale");
    t.lastSeenMs = 0;
    table.addOrUpdate(t);

    table.pruneStale(TARGET_AGE_TIMEOUT + 100);
    EXPECT_EQ(callCount, 1);
}

// =============================================================================
// MAX_TARGETS Boundary Stress Tests (T7)
// =============================================================================

TEST_F(TargetTableTest, ExactlyMaxTargets) {
    for (size_t i = 0; i < MAX_TARGETS; i++) {
        char ssid[8];
        snprintf(ssid, sizeof(ssid), "Net%03u", (unsigned)i);
        table.addOrUpdate(makeTarget((uint8_t)i, ssid, -50));
    }
    EXPECT_EQ(table.count(), MAX_TARGETS);
}

TEST_F(TargetTableTest, SixtyFifthTargetEvictsWeakest) {
    // Fill with varying signal strengths
    for (size_t i = 0; i < MAX_TARGETS; i++) {
        char ssid[8];
        snprintf(ssid, sizeof(ssid), "Net%03u", (unsigned)i);
        int8_t rssi = (int8_t)(-90 + (int)(i % 20)); // -90 to -71
        table.addOrUpdate(makeTarget((uint8_t)i, ssid, rssi));
    }
    EXPECT_EQ(table.count(), MAX_TARGETS);

    // 65th target with strong signal should succeed
    Target strong = makeTarget(0xFE, "Strong65", -20);
    EXPECT_TRUE(table.addOrUpdate(strong));
    EXPECT_EQ(table.count(), MAX_TARGETS);

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFE};
    EXPECT_NE(table.findByBssid(mac), nullptr);
}

TEST_F(TargetTableTest, SortStabilityAfterEviction) {
    for (size_t i = 0; i < MAX_TARGETS; i++) {
        char ssid[8];
        snprintf(ssid, sizeof(ssid), "Net%03u", (unsigned)i);
        table.addOrUpdate(makeTarget((uint8_t)i, ssid, -80));
    }

    // Evict by adding stronger
    table.addOrUpdate(makeTarget(0xFE, "New", -30));

    // Sort by signal should still work correctly
    TargetFilter filter;
    auto sorted = table.getFiltered(filter, SortOrder::SIGNAL_STRENGTH);
    ASSERT_GT(sorted.size(), 1u);
    EXPECT_STREQ(sorted[0].ssid, "New");
    EXPECT_EQ(sorted[0].rssi, -30);
}

TEST_F(TargetTableTest, PruneAllStale) {
    for (size_t i = 0; i < 10; i++) {
        char ssid[8];
        snprintf(ssid, sizeof(ssid), "Net%03u", (unsigned)i);
        Target t = makeTarget((uint8_t)i, ssid, -50);
        t.lastSeenMs = 1000;
        table.addOrUpdate(t);
    }
    EXPECT_EQ(table.count(), 10u);

    size_t removed = table.pruneStale(1000 + TARGET_AGE_TIMEOUT + 1);
    EXPECT_EQ(removed, 10u);
    EXPECT_EQ(table.count(), 0u);
}

TEST_F(TargetTableTest, FilterZeroMatches) {
    table.addOrUpdate(makeTarget(0x01, "WiFiAP", -50, TargetType::ACCESS_POINT));
    table.addOrUpdate(makeTarget(0x02, "BLEDev", -60, TargetType::BLE_DEVICE));

    TargetFilter filter;
    filter.showAccessPoints = false;
    filter.showBLE = false;
    filter.showStations = false;
    auto result = table.getFiltered(filter);

    EXPECT_EQ(result.size(), 0u);
}

TEST_F(TargetTableTest, AddVirtualTarget) {
    EXPECT_TRUE(table.addVirtualTarget("Universal Remote", TargetType::IR_DEVICE));
    EXPECT_EQ(table.count(), 1u);
    EXPECT_EQ(table.countByType(TargetType::IR_DEVICE), 1u);
}
