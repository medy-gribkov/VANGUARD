#include <gtest/gtest.h>
#include "../../test/mocks/Arduino.h"
#include "../../src/core/ActionResolver.h"
#include "../../src/core/VanguardTypes.h"

using namespace Vanguard;

class ActionResolverTest : public ::testing::Test {
protected:
    ActionResolver resolver;

    Target makeWiFiAP(const char* ssid, SecurityType sec, uint8_t clients = 0, uint8_t channel = 6) {
        Target t;
        memset(&t, 0, sizeof(Target));
        uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
        memcpy(t.bssid, mac, 6);
        strncpy(t.ssid, ssid, SSID_MAX_LEN);
        t.type = TargetType::ACCESS_POINT;
        t.security = sec;
        t.channel = channel;
        t.rssi = -50;
        t.clientCount = clients;
        // Add fake client MACs
        for (uint8_t i = 0; i < clients && i < MAX_CLIENTS_PER_AP; i++) {
            t.clientMacs[i][0] = 0xCC;
            t.clientMacs[i][5] = i + 1;
        }
        return t;
    }

    Target makeBLEDevice(const char* name) {
        Target t;
        memset(&t, 0, sizeof(Target));
        uint8_t mac[6] = {0xBB, 0xBB, 0xCC, 0xDD, 0xEE, 0x02};
        memcpy(t.bssid, mac, 6);
        strncpy(t.ssid, name, SSID_MAX_LEN);
        t.type = TargetType::BLE_DEVICE;
        t.rssi = -60;
        return t;
    }

    Target makeIRDevice(const char* name) {
        Target t;
        memset(&t, 0, sizeof(Target));
        uint8_t mac[6] = {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x01};
        memcpy(t.bssid, mac, 6);
        strncpy(t.ssid, name, SSID_MAX_LEN);
        t.type = TargetType::IR_DEVICE;
        t.rssi = -30;
        return t;
    }

    bool hasAction(const std::vector<AvailableAction>& actions, ActionType type) {
        for (const auto& a : actions) {
            if (a.type == type) return true;
        }
        return false;
    }
};

// =============================================================================
// WiFi AP Action Tests
// =============================================================================

TEST_F(ActionResolverTest, WiFiAPWithClientsGetsDeauthActions) {
    Target ap = makeWiFiAP("TestNet", SecurityType::WPA2_PSK, 3);
    auto actions = resolver.getActionsFor(ap);

    EXPECT_TRUE(hasAction(actions, ActionType::DEAUTH_ALL));
    EXPECT_TRUE(hasAction(actions, ActionType::DEAUTH_SINGLE));
    EXPECT_TRUE(hasAction(actions, ActionType::BEACON_FLOOD));
    EXPECT_TRUE(hasAction(actions, ActionType::EVIL_TWIN));
}

TEST_F(ActionResolverTest, WiFiAPWithoutClientsNoDeauth) {
    Target ap = makeWiFiAP("NoClients", SecurityType::WPA2_PSK, 0);
    auto actions = resolver.getActionsFor(ap);

    EXPECT_FALSE(hasAction(actions, ActionType::DEAUTH_ALL));
    EXPECT_FALSE(hasAction(actions, ActionType::DEAUTH_SINGLE));
    // Beacon flood and evil twin don't require clients
    EXPECT_TRUE(hasAction(actions, ActionType::BEACON_FLOOD));
    EXPECT_TRUE(hasAction(actions, ActionType::EVIL_TWIN));
}

TEST_F(ActionResolverTest, OpenNetworkNoHandshakeCapture) {
    Target ap = makeWiFiAP("OpenNet", SecurityType::OPEN, 2);
    auto actions = resolver.getActionsFor(ap);

    // CAPTURE_HANDSHAKE is incompatible with OPEN security
    EXPECT_FALSE(hasAction(actions, ActionType::CAPTURE_HANDSHAKE));
    // Deauth should still work
    EXPECT_TRUE(hasAction(actions, ActionType::DEAUTH_ALL));
}

TEST_F(ActionResolverTest, WPA3NetworkNoPMKID) {
    // CAPTURE_PMKID is marked incompatible with WPA3_SAE
    // Also CAPTURE_PMKID is not in isImplemented(), so it won't show regardless
    Target ap = makeWiFiAP("WPA3Net", SecurityType::WPA3_SAE, 2);
    auto actions = resolver.getActionsFor(ap);

    EXPECT_FALSE(hasAction(actions, ActionType::CAPTURE_PMKID));
}

TEST_F(ActionResolverTest, WiFiAPNoClientNoHandshake) {
    // CAPTURE_HANDSHAKE requires clients (needs client to reconnect)
    Target ap = makeWiFiAP("Secured", SecurityType::WPA2_PSK, 0);
    auto actions = resolver.getActionsFor(ap);

    EXPECT_FALSE(hasAction(actions, ActionType::CAPTURE_HANDSHAKE));
}

TEST_F(ActionResolverTest, WiFiAPWithClientGetsHandshake) {
    Target ap = makeWiFiAP("Secured", SecurityType::WPA2_PSK, 1);
    auto actions = resolver.getActionsFor(ap);

    EXPECT_TRUE(hasAction(actions, ActionType::CAPTURE_HANDSHAKE));
}

// =============================================================================
// BLE Action Tests
// =============================================================================

TEST_F(ActionResolverTest, BLEDeviceGetsBLEActions) {
    Target ble = makeBLEDevice("BLE-Speaker");
    auto actions = resolver.getActionsFor(ble);

    EXPECT_TRUE(hasAction(actions, ActionType::BLE_SPAM));
    EXPECT_TRUE(hasAction(actions, ActionType::BLE_SOUR_APPLE));
    // Should NOT get WiFi actions
    EXPECT_FALSE(hasAction(actions, ActionType::DEAUTH_ALL));
    EXPECT_FALSE(hasAction(actions, ActionType::BEACON_FLOOD));
}

TEST_F(ActionResolverTest, BLEDeviceNoSkimmerDetect) {
    // BLE_SKIMMER_DETECT is not implemented, should not appear
    Target ble = makeBLEDevice("BLE-Device");
    auto actions = resolver.getActionsFor(ble);

    EXPECT_FALSE(hasAction(actions, ActionType::BLE_SKIMMER_DETECT));
}

// =============================================================================
// IR Action Tests
// =============================================================================

TEST_F(ActionResolverTest, IRDeviceGetsIRActions) {
    Target ir = makeIRDevice("Universal Remote");
    auto actions = resolver.getActionsFor(ir);

    EXPECT_TRUE(hasAction(actions, ActionType::IR_REPLAY));
    EXPECT_TRUE(hasAction(actions, ActionType::IR_TVBGONE));
    // Should NOT get WiFi or BLE actions
    EXPECT_FALSE(hasAction(actions, ActionType::DEAUTH_ALL));
    EXPECT_FALSE(hasAction(actions, ActionType::BLE_SPAM));
}

// =============================================================================
// isActionValid Tests
// =============================================================================

TEST_F(ActionResolverTest, IsActionValidTargetTypeMismatch) {
    Target ble = makeBLEDevice("BLE-Thing");

    // WiFi action on BLE target = invalid
    EXPECT_FALSE(resolver.isActionValid(ble, ActionType::DEAUTH_ALL));
    EXPECT_FALSE(resolver.isActionValid(ble, ActionType::BEACON_FLOOD));
}

TEST_F(ActionResolverTest, IsActionValidClientRequired) {
    Target ap = makeWiFiAP("NoClients", SecurityType::WPA2_PSK, 0);

    // Deauth requires clients
    EXPECT_FALSE(resolver.isActionValid(ap, ActionType::DEAUTH_ALL));
    EXPECT_FALSE(resolver.isActionValid(ap, ActionType::DEAUTH_SINGLE));

    // Beacon flood does not require clients
    EXPECT_TRUE(resolver.isActionValid(ap, ActionType::BEACON_FLOOD));
}

TEST_F(ActionResolverTest, IsActionValidSecurityIncompatible) {
    Target open = makeWiFiAP("Open", SecurityType::OPEN, 2);

    // CAPTURE_HANDSHAKE is incompatible with OPEN
    EXPECT_FALSE(resolver.isActionValid(open, ActionType::CAPTURE_HANDSHAKE));
}

// =============================================================================
// getInvalidReason Tests
// =============================================================================

TEST_F(ActionResolverTest, GetInvalidReasonMessages) {
    Target ble = makeBLEDevice("BLE");
    Target apNoClients = makeWiFiAP("AP", SecurityType::WPA2_PSK, 0);
    Target openAP = makeWiFiAP("Open", SecurityType::OPEN, 2);

    EXPECT_STREQ(resolver.getInvalidReason(ble, ActionType::DEAUTH_ALL), "Wrong target type");
    EXPECT_STREQ(resolver.getInvalidReason(apNoClients, ActionType::DEAUTH_ALL), "No clients connected");
    EXPECT_STREQ(resolver.getInvalidReason(openAP, ActionType::CAPTURE_HANDSHAKE), "Incompatible security type");

    // Valid action should return nullptr
    Target apWithClients = makeWiFiAP("AP", SecurityType::WPA2_PSK, 3);
    EXPECT_EQ(resolver.getInvalidReason(apWithClients, ActionType::DEAUTH_ALL), nullptr);
}

// =============================================================================
// 5GHz Compatibility Tests
// =============================================================================

TEST_F(ActionResolverTest, FiveGHzTargetNoWiFiActions) {
    // Channel > 14 = 5GHz, ESP32-S3 is 2.4GHz only
    Target ap5g = makeWiFiAP("5GHz-Net", SecurityType::WPA2_PSK, 3, 36);
    auto actions = resolver.getActionsFor(ap5g);

    // All WiFi actions should be filtered out
    EXPECT_FALSE(hasAction(actions, ActionType::DEAUTH_ALL));
    EXPECT_FALSE(hasAction(actions, ActionType::BEACON_FLOOD));
    EXPECT_FALSE(hasAction(actions, ActionType::EVIL_TWIN));
    EXPECT_EQ(actions.size(), 0u);
}

TEST_F(ActionResolverTest, TwoFourGHzTargetGetsActions) {
    Target ap = makeWiFiAP("2.4GHz-Net", SecurityType::WPA2_PSK, 2, 6);
    auto actions = resolver.getActionsFor(ap);

    EXPECT_TRUE(actions.size() > 0);
    EXPECT_TRUE(hasAction(actions, ActionType::DEAUTH_ALL));
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(ActionResolverTest, UnknownActionTypeReturnsInvalid) {
    Target ap = makeWiFiAP("AP", SecurityType::WPA2_PSK, 1);
    EXPECT_FALSE(resolver.isActionValid(ap, ActionType::NONE));
    EXPECT_STREQ(resolver.getInvalidReason(ap, ActionType::NONE), "Unknown action");
}

TEST_F(ActionResolverTest, UnimplementedActionsNotReturned) {
    Target ap = makeWiFiAP("AP", SecurityType::WPA2_PSK, 3);
    auto actions = resolver.getActionsFor(ap);

    // MONITOR and CAPTURE_PMKID are not implemented
    EXPECT_FALSE(hasAction(actions, ActionType::MONITOR));
    EXPECT_FALSE(hasAction(actions, ActionType::CAPTURE_PMKID));
    EXPECT_FALSE(hasAction(actions, ActionType::PROBE_FLOOD));
}
