#ifndef VANGUARD_BRUCE_HID_H
#define VANGUARD_BRUCE_HID_H

/**
 * @file BruceHID.h
 * @brief USB HID (BadUSB) adapter for ESP32-S3 native USB-OTG
 *
 * Provides keyboard emulation and DuckyScript execution.
 * Payloads stored on SD: /vanguard/hid_payloads/*.txt
 */

#include <Arduino.h>
#include "../core/VanguardModule.h"
#include <functional>

namespace Vanguard {

enum class HIDState : uint8_t {
    IDLE,
    TYPING,
    RUNNING_SCRIPT,
    ERROR
};

using HIDProgressCallback = std::function<void(uint32_t linesExecuted, uint32_t totalLines)>;

class BruceHID : public VanguardModule {
public:
    static BruceHID& getInstance();

    // VanguardModule interface
    bool onEnable() override;
    void onDisable() override;
    void onTick() override;
    const char* getName() const override { return "HID"; }

    // Prevent copying
    BruceHID(const BruceHID&) = delete;
    BruceHID& operator=(const BruceHID&) = delete;

    // Lifecycle
    bool init();
    void shutdown();

    // Basic HID operations
    void sendKeystroke(uint8_t keycode, uint8_t modifier = 0);
    void typeString(const char* str, uint16_t delayMs = 50);
    void pressKey(uint8_t keycode);
    void releaseAll();

    // DuckyScript execution
    bool executeDuckyScript(const char* path);
    void stopScript();
    bool isRunning() const { return m_state == HIDState::RUNNING_SCRIPT; }
    HIDState getState() const { return m_state; }

    // Progress callback
    void onProgress(HIDProgressCallback cb) { m_onProgress = cb; }

private:
    BruceHID();
    ~BruceHID();

    bool m_initialized;
    HIDState m_state;

    // Script execution state
    char m_scriptPath[64];
    uint32_t m_scriptLine;
    uint32_t m_scriptTotalLines;
    uint32_t m_nextActionMs;      // When to execute next line
    uint16_t m_defaultDelay;      // DEFAULT_DELAY in ms
    uint16_t m_repeatCount;       // REPEAT counter
    char m_lastCommand[128];      // Last command for REPEAT

    HIDProgressCallback m_onProgress;

    // DuckyScript parsing
    void executeLine(const char* line);
    void executeDelay(uint16_t ms);
    uint8_t parseSpecialKey(const char* keyName);
};

} // namespace Vanguard

#endif // VANGUARD_BRUCE_HID_H
