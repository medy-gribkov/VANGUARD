#include <gtest/gtest.h>
#include <cstring>
#include <cstdint>

// Standalone BLEDeviceInfo struct (mirrors src/adapters/BruceBLE.h)
// Defined locally to avoid NimBLE dependency in native tests.
struct BLEDeviceInfo {
    uint8_t      address[6];
    char         name[32];
    int8_t       rssi;
    uint16_t     appearance;
    bool         isConnectable;
    bool         hasServices;
    uint32_t     lastSeenMs;
    uint16_t     manufacturerId;
    uint8_t      manufacturerData[32];
    uint8_t      manufacturerDataLen;
    bool         isSuspicious;
};

// Replicated scoring logic from BruceBLE::isLikelySkimmer().
// Kept in sync manually. If the source changes, update here.
static bool isLikelySkimmer(const BLEDeviceInfo& device) {
    int score = 0;

    // HC-05/HC-06 Bluetooth modules (very common in skimmers)
    if (strncmp(device.name, "HC-05", 5) == 0 ||
        strncmp(device.name, "HC-06", 5) == 0) {
        score += 5;
    }

    // JDY, BT04 modules (cheap serial Bluetooth, used in skimmers)
    if (strncmp(device.name, "JDY-", 4) == 0 ||
        strncmp(device.name, "BT04", 4) == 0 ||
        strncmp(device.name, "BT05", 4) == 0) {
        score += 4;
    }

    // No name, connectable, no services - suspicious generic module
    if (strlen(device.name) == 0 && device.isConnectable && !device.hasServices) {
        score += 3;
    }

    // Generic/unknown manufacturer ID (0x0000 or 0xFFFF)
    if (device.manufacturerDataLen >= 2) {
        if (device.manufacturerId == 0x0000 || device.manufacturerId == 0xFFFF) {
            score += 2;
        }
    }

    // Very strong signal from unknown device (likely physically close/embedded)
    if (device.rssi > -40 && strlen(device.name) == 0) {
        score += 2;
    }

    // Connectable with no advertised name is suspicious for embedded hardware
    if (device.isConnectable && strlen(device.name) == 0 && device.manufacturerDataLen == 0) {
        score += 2;
    }

    return score >= 4;
}

// Helper to create a zeroed BLEDeviceInfo
static BLEDeviceInfo makeDevice() {
    BLEDeviceInfo d;
    memset(&d, 0, sizeof(d));
    d.rssi = -70;  // Normal signal strength
    return d;
}

// =============================================================================
// TESTS
// =============================================================================

class BLEDetectionTest : public ::testing::Test {};

TEST_F(BLEDetectionTest, HC05DetectedAsSkimmer) {
    auto d = makeDevice();
    strncpy(d.name, "HC-05", sizeof(d.name));
    d.isConnectable = true;
    EXPECT_TRUE(isLikelySkimmer(d)) << "HC-05 modules score >= 5, should be flagged";
}

TEST_F(BLEDetectionTest, HC06DetectedAsSkimmer) {
    auto d = makeDevice();
    strncpy(d.name, "HC-06", sizeof(d.name));
    EXPECT_TRUE(isLikelySkimmer(d)) << "HC-06 modules score >= 5, should be flagged";
}

TEST_F(BLEDetectionTest, JDYModuleDetectedAsSkimmer) {
    auto d = makeDevice();
    strncpy(d.name, "JDY-31", sizeof(d.name));
    EXPECT_TRUE(isLikelySkimmer(d)) << "JDY-xx modules score >= 4, should be flagged";
}

TEST_F(BLEDetectionTest, BT04ModuleDetectedAsSkimmer) {
    auto d = makeDevice();
    strncpy(d.name, "BT04-A", sizeof(d.name));
    EXPECT_TRUE(isLikelySkimmer(d)) << "BT04 modules score >= 4, should be flagged";
}

TEST_F(BLEDetectionTest, BT05ModuleDetectedAsSkimmer) {
    auto d = makeDevice();
    strncpy(d.name, "BT05", sizeof(d.name));
    EXPECT_TRUE(isLikelySkimmer(d)) << "BT05 modules score >= 4, should be flagged";
}

TEST_F(BLEDetectionTest, NamedAppleDeviceNotFlagged) {
    auto d = makeDevice();
    strncpy(d.name, "John's AirPods Pro", sizeof(d.name));
    d.isConnectable = true;
    d.hasServices = true;
    d.manufacturerId = 0x004C;  // Apple
    d.manufacturerDataLen = 4;
    EXPECT_FALSE(isLikelySkimmer(d)) << "Named Apple device should not be flagged";
}

TEST_F(BLEDetectionTest, NamedConnectableDeviceNotFlagged) {
    auto d = makeDevice();
    strncpy(d.name, "Bose QC45", sizeof(d.name));
    d.isConnectable = true;
    d.hasServices = true;
    EXPECT_FALSE(isLikelySkimmer(d)) << "Normal named connectable device should not be flagged";
}

TEST_F(BLEDetectionTest, NoNameConnectableNoServicesIsSuspicious) {
    // Score: 3 (no name + connectable + no services) + 2 (connectable + no name + no mfg) = 5
    auto d = makeDevice();
    d.name[0] = '\0';
    d.isConnectable = true;
    d.hasServices = false;
    d.manufacturerDataLen = 0;
    EXPECT_TRUE(isLikelySkimmer(d)) << "Nameless connectable device with no services scores 5";
}

TEST_F(BLEDetectionTest, StrongSignalNoNameFlagged) {
    // Score: 3 (no name + connectable + no services) + 2 (strong signal + no name) + 2 (connectable + no name + no mfg) = 7
    auto d = makeDevice();
    d.name[0] = '\0';
    d.rssi = -30;
    d.isConnectable = true;
    d.hasServices = false;
    d.manufacturerDataLen = 0;
    EXPECT_TRUE(isLikelySkimmer(d)) << "Strong-signal nameless connectable device scores 7";
}

TEST_F(BLEDetectionTest, GenericManufacturerIdAddsSuspicion) {
    // Score: 3 (no name + connectable + no services) + 2 (mfg ID 0xFFFF) = 5
    auto d = makeDevice();
    d.name[0] = '\0';
    d.isConnectable = true;
    d.hasServices = false;
    d.manufacturerId = 0xFFFF;
    d.manufacturerDataLen = 2;
    EXPECT_TRUE(isLikelySkimmer(d)) << "Generic manufacturer ID 0xFFFF adds to score";
}

TEST_F(BLEDetectionTest, NonConnectableBeaconNotFlagged) {
    // Non-connectable beacon with no name, no services
    // Score: 0 (not connectable, so no name+connectable+no-services check doesn't trigger)
    auto d = makeDevice();
    d.name[0] = '\0';
    d.isConnectable = false;
    d.hasServices = false;
    d.manufacturerDataLen = 0;
    EXPECT_FALSE(isLikelySkimmer(d)) << "Non-connectable beacons should not be flagged";
}

TEST_F(BLEDetectionTest, NamedDeviceWithGenericMfgNotFlagged) {
    // Named device with generic manufacturer - only 2 from mfg ID, not enough
    auto d = makeDevice();
    strncpy(d.name, "MyDevice", sizeof(d.name));
    d.isConnectable = true;
    d.hasServices = true;
    d.manufacturerId = 0x0000;
    d.manufacturerDataLen = 2;
    EXPECT_FALSE(isLikelySkimmer(d)) << "Named device with generic mfg ID scores only 2";
}

TEST_F(BLEDetectionTest, ScoreThresholdExactlyFour) {
    // JDY module scores exactly 4 (threshold)
    auto d = makeDevice();
    strncpy(d.name, "JDY-10", sizeof(d.name));
    d.isConnectable = false;
    d.hasServices = true;
    EXPECT_TRUE(isLikelySkimmer(d)) << "Score of exactly 4 should be flagged (threshold is >= 4)";
}

TEST_F(BLEDetectionTest, ScoreThresholdBelowFour) {
    // Just below threshold: no name + connectable + no services = 3
    // but has services, so that rule doesn't trigger
    // Let's get score 3: no name + connectable + no services only
    // Actually 3 + 2 (no mfg) = 5. We need exactly 3.
    // Use: no name + connectable + no services (3) + has mfg data (blocks the +2 from no-mfg)
    auto d = makeDevice();
    d.name[0] = '\0';
    d.isConnectable = true;
    d.hasServices = false;
    d.manufacturerId = 0x004C;  // Apple, known good
    d.manufacturerDataLen = 4;
    // Score: 3 (no name + connectable + no services) only
    EXPECT_FALSE(isLikelySkimmer(d)) << "Score of 3 is below threshold, should not be flagged";
}
