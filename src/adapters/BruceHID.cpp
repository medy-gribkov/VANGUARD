/**
 * @file BruceHID.cpp
 * @brief USB HID (BadUSB) adapter - DuckyScript executor
 *
 * Uses ESP32-S3 native USB-OTG for keyboard emulation.
 * Parses DuckyScript payloads from SD card.
 */

#include "BruceHID.h"
#include "../core/SDManager.h"
#include <USB.h>
#include <USBHIDKeyboard.h>

namespace Vanguard {

static USBHIDKeyboard s_keyboard;

// =============================================================================
// SINGLETON
// =============================================================================

BruceHID& BruceHID::getInstance() {
    static BruceHID instance;
    return instance;
}

BruceHID::BruceHID()
    : m_initialized(false)
    , m_state(HIDState::IDLE)
    , m_scriptLine(0)
    , m_scriptTotalLines(0)
    , m_nextActionMs(0)
    , m_defaultDelay(0)
    , m_repeatCount(0)
    , m_onProgress(nullptr)
{
    memset(m_scriptPath, 0, sizeof(m_scriptPath));
    memset(m_lastCommand, 0, sizeof(m_lastCommand));
}

BruceHID::~BruceHID() {
    shutdown();
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool BruceHID::onEnable() {
    if (m_enabled) return true;
    if (!m_initialized) init();
    m_enabled = m_initialized;
    return m_enabled;
}

void BruceHID::onDisable() {
    if (!m_enabled) return;
    stopScript();
    s_keyboard.end();
    m_enabled = false;
    m_state = HIDState::IDLE;
}

bool BruceHID::init() {
    if (m_initialized) return true;

    USB.begin();
    s_keyboard.begin();

    if (Serial) Serial.println("[HID] Initialized USB keyboard");
    m_initialized = true;
    return true;
}

void BruceHID::shutdown() {
    onDisable();
    m_initialized = false;
}

void BruceHID::onTick() {
    if (m_state != HIDState::RUNNING_SCRIPT) return;

    uint32_t now = millis();
    if (now < m_nextActionMs) return;

    // Read next line from script file
    File f = SD.open(m_scriptPath);
    if (!f) {
        m_state = HIDState::IDLE;
        return;
    }

    // Seek to current line
    uint32_t currentLine = 0;
    char lineBuf[128];
    bool found = false;
    while (f.available()) {
        int len = 0;
        while (f.available() && len < 127) {
            char c = f.read();
            if (c == '\n') break;
            if (c != '\r') lineBuf[len++] = c;
        }
        lineBuf[len] = '\0';

        if (currentLine == m_scriptLine) {
            found = true;
            break;
        }
        currentLine++;
    }
    f.close();

    if (!found) {
        // Script complete
        if (Serial) Serial.printf("[HID] Script complete (%lu lines)\n", m_scriptLine);
        m_state = HIDState::IDLE;
        return;
    }

    // Handle REPEAT
    if (m_repeatCount > 0) {
        executeLine(m_lastCommand);
        m_repeatCount--;
        if (m_defaultDelay > 0) m_nextActionMs = millis() + m_defaultDelay;
        return;
    }

    // Check for REPEAT command
    if (strncmp(lineBuf, "REPEAT ", 7) == 0) {
        m_repeatCount = atoi(lineBuf + 7);
        if (m_repeatCount > 0) {
            m_repeatCount--; // First execution already happened
            executeLine(m_lastCommand);
        }
    } else if (strlen(lineBuf) > 0 && lineBuf[0] != '/') {
        // Store and execute (skip comments starting with //)
        strncpy(m_lastCommand, lineBuf, sizeof(m_lastCommand) - 1);
        m_lastCommand[sizeof(m_lastCommand) - 1] = '\0';
        executeLine(lineBuf);
    }

    m_scriptLine++;
    if (m_defaultDelay > 0) {
        m_nextActionMs = millis() + m_defaultDelay;
    }

    if (m_onProgress) {
        m_onProgress(m_scriptLine, m_scriptTotalLines);
    }
}

// =============================================================================
// BASIC HID OPERATIONS
// =============================================================================

void BruceHID::sendKeystroke(uint8_t keycode, uint8_t modifier) {
    if (!m_initialized) return;
    if (modifier) s_keyboard.press(modifier);
    s_keyboard.press(keycode);
    delay(10);
    s_keyboard.releaseAll();
}

void BruceHID::typeString(const char* str, uint16_t delayMs) {
    if (!m_initialized || !str) return;
    m_state = HIDState::TYPING;
    for (int i = 0; str[i]; i++) {
        s_keyboard.write(str[i]);
        if (delayMs > 0) delay(delayMs);
    }
    m_state = HIDState::IDLE;
}

void BruceHID::pressKey(uint8_t keycode) {
    if (!m_initialized) return;
    s_keyboard.press(keycode);
}

void BruceHID::releaseAll() {
    if (!m_initialized) return;
    s_keyboard.releaseAll();
}

// =============================================================================
// DUCKYSCRIPT EXECUTION
// =============================================================================

bool BruceHID::executeDuckyScript(const char* path) {
    if (!m_initialized || !path) return false;

    // Count lines for progress
    File f = SD.open(path);
    if (!f) {
        if (Serial) Serial.printf("[HID] Script not found: %s\n", path);
        return false;
    }
    m_scriptTotalLines = 0;
    while (f.available()) {
        if (f.read() == '\n') m_scriptTotalLines++;
    }
    m_scriptTotalLines++; // Last line may not end with newline
    f.close();

    strncpy(m_scriptPath, path, sizeof(m_scriptPath) - 1);
    m_scriptPath[sizeof(m_scriptPath) - 1] = '\0';
    m_scriptLine = 0;
    m_defaultDelay = 0;
    m_repeatCount = 0;
    m_nextActionMs = millis();
    m_state = HIDState::RUNNING_SCRIPT;

    if (Serial) Serial.printf("[HID] Running script: %s (%lu lines)\n", path, m_scriptTotalLines);
    return true;
}

void BruceHID::stopScript() {
    if (m_state == HIDState::RUNNING_SCRIPT) {
        s_keyboard.releaseAll();
        m_state = HIDState::IDLE;
        if (Serial) Serial.println("[HID] Script stopped");
    }
}

void BruceHID::executeLine(const char* line) {
    if (!line || strlen(line) == 0) return;

    // REM - Comment
    if (strncmp(line, "REM ", 4) == 0 || strncmp(line, "REM\0", 4) == 0) {
        return;
    }

    // DEFAULT_DELAY
    if (strncmp(line, "DEFAULT_DELAY ", 14) == 0 || strncmp(line, "DEFAULTDELAY ", 13) == 0) {
        const char* val = strchr(line, ' ') + 1;
        m_defaultDelay = atoi(val);
        return;
    }

    // DELAY
    if (strncmp(line, "DELAY ", 6) == 0) {
        uint16_t ms = atoi(line + 6);
        m_nextActionMs = millis() + ms;
        return;
    }

    // STRING - Type text
    if (strncmp(line, "STRING ", 7) == 0) {
        typeString(line + 7, 0);
        return;
    }

    // ENTER
    if (strcmp(line, "ENTER") == 0) {
        sendKeystroke(KEY_RETURN);
        return;
    }

    // TAB
    if (strcmp(line, "TAB") == 0) {
        sendKeystroke(KEY_TAB);
        return;
    }

    // ESC / ESCAPE
    if (strcmp(line, "ESC") == 0 || strcmp(line, "ESCAPE") == 0) {
        sendKeystroke(KEY_ESC);
        return;
    }

    // GUI / WINDOWS - GUI key combos
    if (strncmp(line, "GUI ", 4) == 0 || strncmp(line, "WINDOWS ", 8) == 0) {
        const char* key = strchr(line, ' ') + 1;
        if (strlen(key) == 1) {
            sendKeystroke(key[0], KEY_LEFT_GUI);
        } else {
            uint8_t k = parseSpecialKey(key);
            if (k) sendKeystroke(k, KEY_LEFT_GUI);
        }
        return;
    }
    if (strcmp(line, "GUI") == 0 || strcmp(line, "WINDOWS") == 0) {
        sendKeystroke(KEY_LEFT_GUI);
        return;
    }

    // CTRL combos
    if (strncmp(line, "CTRL ", 5) == 0 || strncmp(line, "CONTROL ", 8) == 0) {
        const char* key = strrchr(line, ' ') + 1;
        if (strlen(key) == 1) {
            sendKeystroke(key[0], KEY_LEFT_CTRL);
        } else {
            uint8_t k = parseSpecialKey(key);
            if (k) sendKeystroke(k, KEY_LEFT_CTRL);
        }
        return;
    }

    // ALT combos
    if (strncmp(line, "ALT ", 4) == 0) {
        const char* key = line + 4;
        if (strlen(key) == 1) {
            sendKeystroke(key[0], KEY_LEFT_ALT);
        } else {
            uint8_t k = parseSpecialKey(key);
            if (k) sendKeystroke(k, KEY_LEFT_ALT);
        }
        return;
    }
    if (strcmp(line, "ALT") == 0) {
        sendKeystroke(KEY_LEFT_ALT);
        return;
    }

    // SHIFT combos
    if (strncmp(line, "SHIFT ", 6) == 0) {
        const char* key = line + 6;
        uint8_t k = parseSpecialKey(key);
        if (k) sendKeystroke(k, KEY_LEFT_SHIFT);
        return;
    }

    // Arrow keys
    if (strcmp(line, "UP") == 0 || strcmp(line, "UPARROW") == 0) {
        sendKeystroke(KEY_UP_ARROW); return;
    }
    if (strcmp(line, "DOWN") == 0 || strcmp(line, "DOWNARROW") == 0) {
        sendKeystroke(KEY_DOWN_ARROW); return;
    }
    if (strcmp(line, "LEFT") == 0 || strcmp(line, "LEFTARROW") == 0) {
        sendKeystroke(KEY_LEFT_ARROW); return;
    }
    if (strcmp(line, "RIGHT") == 0 || strcmp(line, "RIGHTARROW") == 0) {
        sendKeystroke(KEY_RIGHT_ARROW); return;
    }

    // SPACE
    if (strcmp(line, "SPACE") == 0) {
        sendKeystroke(' '); return;
    }

    // BACKSPACE / DELETE
    if (strcmp(line, "BACKSPACE") == 0) {
        sendKeystroke(KEY_BACKSPACE); return;
    }
    if (strcmp(line, "DELETE") == 0) {
        sendKeystroke(KEY_DELETE); return;
    }

    // CAPSLOCK
    if (strcmp(line, "CAPSLOCK") == 0) {
        sendKeystroke(KEY_CAPS_LOCK); return;
    }

    // Function keys
    if (line[0] == 'F' && line[1] >= '1' && line[1] <= '9') {
        int fnum = atoi(line + 1);
        if (fnum >= 1 && fnum <= 12) {
            sendKeystroke(KEY_F1 + fnum - 1);
        }
        return;
    }

    if (Serial) Serial.printf("[HID] Unknown command: %s\n", line);
}

void BruceHID::executeDelay(uint16_t ms) {
    m_nextActionMs = millis() + ms;
}

uint8_t BruceHID::parseSpecialKey(const char* keyName) {
    if (!keyName) return 0;
    if (strcmp(keyName, "ENTER") == 0) return KEY_RETURN;
    if (strcmp(keyName, "TAB") == 0) return KEY_TAB;
    if (strcmp(keyName, "ESC") == 0 || strcmp(keyName, "ESCAPE") == 0) return KEY_ESC;
    if (strcmp(keyName, "BACKSPACE") == 0) return KEY_BACKSPACE;
    if (strcmp(keyName, "DELETE") == 0) return KEY_DELETE;
    if (strcmp(keyName, "SPACE") == 0) return ' ';
    if (strcmp(keyName, "UP") == 0) return KEY_UP_ARROW;
    if (strcmp(keyName, "DOWN") == 0) return KEY_DOWN_ARROW;
    if (strcmp(keyName, "LEFT") == 0) return KEY_LEFT_ARROW;
    if (strcmp(keyName, "RIGHT") == 0) return KEY_RIGHT_ARROW;
    if (keyName[0] == 'F' && keyName[1] >= '1') {
        int fnum = atoi(keyName + 1);
        if (fnum >= 1 && fnum <= 12) return KEY_F1 + fnum - 1;
    }
    return 0;
}

} // namespace Vanguard
