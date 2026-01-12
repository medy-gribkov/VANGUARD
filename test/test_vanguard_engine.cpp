#include <gtest/gtest.h>
#include "Arduino.h"
#include "VanguardEngine.h"
#include "VanguardTypes.h"

using namespace Vanguard;

class VanguardEngineTest : public ::testing::Test {
protected:
    VanguardEngine& engine = VanguardEngine::getInstance();

    void SetUp() override {
        engine.shutdown();
    }
};

TEST_F(VanguardEngineTest, SingletonInstance) {
    VanguardEngine& e1 = VanguardEngine::getInstance();
    VanguardEngine& e2 = VanguardEngine::getInstance();
    EXPECT_EQ(&e1, &e2);
}

TEST_F(VanguardEngineTest, Initialization) {
    EXPECT_TRUE(engine.init());
    EXPECT_EQ(engine.getScanState(), ScanState::IDLE);
}

TEST_F(VanguardEngineTest, ScanStateTransitions) {
    engine.init();
    engine.beginScan();
    // In our mock, beginScan should immediately trigger WIFI_SCANNING or IDLE if mocked simply
    // Since we didn't mock SystemTask's logic deeply, we just check if it's not crashing
    EXPECT_TRUE(engine.isCombinedScan());
}
TEST_F(VanguardEngineTest, ExecuteDeauthAction) {
    engine.init();
    Target t;
    memset(&t, 0, sizeof(Target));
    strcpy(t.ssid, "TestTarget");
    t.type = TargetType::ACCESS_POINT;
    t.channel = 6;
    
    // Execute action
    EXPECT_TRUE(engine.executeAction(ActionType::DEAUTH_ALL, t));
    
    // Check state (Engine should handle this via IPC events, but we can check if it's active)
    EXPECT_TRUE(engine.isActionActive());
    EXPECT_EQ(engine.getActionProgress().type, ActionType::DEAUTH_ALL);
}
