// Test helper: provides mock definitions and main() for native test builds
#include "mocks/Arduino.h"
#include <gtest/gtest.h>

MockSerial Serial;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
