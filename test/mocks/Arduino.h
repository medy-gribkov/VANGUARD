#ifndef MOCK_ARDUINO_H
#define MOCK_ARDUINO_H

#include <cstdint>
#include <cstring>
#include <chrono>
#include <thread>
#include <string>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

#define F(x) x
#define PROGMEM
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

typedef std::string String;
typedef bool boolean;

inline uint32_t millis() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

inline uint32_t micros() {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(now - start).count();
}

inline void delay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void yield() {
    std::this_thread::yield();
}

// Mock Serial
class MockSerial {
public:
    operator bool() const { return false; } // Suppress output in tests
    void begin(unsigned long) {}
    void println(const char*) {}
    template<typename... Args>
    void printf(const char*, Args...) {}
    void print(const char*) {}
};
extern MockSerial Serial;

// Pin functions (no-op)
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return 0; }
inline void delayMicroseconds(unsigned int) {}

#define INPUT 0
#define OUTPUT 1
#define HIGH 1
#define LOW 0

#endif // MOCK_ARDUINO_H
