// Test helper: provides mock definitions and main() for native test builds
#include "mocks/Arduino.h"
#include "mocks/M5Cardputer.h"
#include "mocks/WiFi.h"
#include <gtest/gtest.h>

MockSerial Serial;
MockM5 M5Cardputer;
MockWiFi WiFi;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
