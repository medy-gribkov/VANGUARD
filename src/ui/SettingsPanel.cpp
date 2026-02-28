/**
 * @file SettingsPanel.cpp
 * @brief Settings screen implementation
 */

#include "SettingsPanel.h"
#include <Preferences.h>
#include <M5Cardputer.h>
#include "CanvasManager.h"

namespace Vanguard {

SettingsPanel::SettingsPanel()
    : m_visible(false)
    , m_highlightIndex(0)
    , m_wantsBack(false)
    , m_needsRedraw(true)
    , m_canvas(&CanvasManager::getInstance().getCanvas())
    , m_lastRenderMs(0)
{
    initSettings();
    loadSettings();
}

SettingsPanel::~SettingsPanel() {
    // m_canvas is shared, do not delete
}

void SettingsPanel::initSettings() {
    // Discovery scan duration (seconds)
    m_settings.push_back({
        "WiFi Scan Duration",
        "Scan duration in seconds",
        SettingType::NUMBER,
        5,      // default 5 sec
        2,      // min
        15,     // max
        1       // step
    });

    // Targeting scan duration (seconds)
    m_settings.push_back({
        "BLE Scan Duration",
        "Scan duration in seconds",
        SettingType::NUMBER,
        3,      // default 3 sec
        1,      // min
        10,     // max
        1       // step
    });

    // Deauth packet count
    m_settings.push_back({
        "Deauth Packets",
        "Packets per burst",
        SettingType::NUMBER,
        10,     // default
        5,      // min
        50,     // max
        5       // step
    });

    // Auto rescan toggle
    m_settings.push_back({
        "Auto Rescan",
        "Periodically refresh targets",
        SettingType::TOGGLE,
        1,      // default ON
        0, 1, 1
    });

    // Sound effects
    m_settings.push_back({
        "Sound Effects",
        "Beeps and feedback",
        SettingType::TOGGLE,
        1,      // default ON
        0, 1, 1
    });
}

void SettingsPanel::show() {
    m_visible = true;
    m_highlightIndex = 0;
    m_wantsBack = false;
    m_needsRedraw = true;
}

void SettingsPanel::hide() {
    m_visible = false;
}

bool SettingsPanel::isVisible() const {
    return m_visible;
}

void SettingsPanel::tick() {
    // Input handled externally
}

void SettingsPanel::render() {
    if (!m_visible) return;

    uint32_t now = millis();
    if (!m_needsRedraw && (now - m_lastRenderMs) < RENDER_INTERVAL_MS) {
        return;
    }
    m_lastRenderMs = now;
    m_needsRedraw = false;

    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);

    // Header
    m_canvas->fillRect(0, 0, Theme::SCREEN_WIDTH, HEADER_HEIGHT, Theme::COLOR_SURFACE);
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->drawString("VANGUARD SETTINGS", 4, 3);

    // Settings list
    int16_t y = HEADER_HEIGHT + 2;
    for (size_t i = 0; i < m_settings.size() && i < VISIBLE_ITEMS; i++) {
        bool highlighted = ((int)i == m_highlightIndex);
        renderSetting(m_settings[i], y, highlighted);
        y += ITEM_HEIGHT;
    }

    // Footer hint
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->setTextDatum(BC_DATUM);
    m_canvas->drawString("[k/l] Adjust  [Enter] Toggle  [Q] Back",
                         Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT - 2);

    m_canvas->pushSprite(0, 0);
}

void SettingsPanel::renderSetting(const Setting& setting, int y, bool highlighted) {
    int16_t w = Theme::SCREEN_WIDTH;
    uint16_t bgColor = highlighted ? Theme::COLOR_SURFACE_RAISED : Theme::COLOR_BACKGROUND;

    m_canvas->fillRect(0, y, w, ITEM_HEIGHT, bgColor);

    if (highlighted) {
        m_canvas->fillRect(0, y, 3, ITEM_HEIGHT, Theme::COLOR_ACCENT);
    }

    // Label
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY, bgColor);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->drawString(setting.label, 8, y + 3);

    // Value on right
    char valueStr[16];
    switch (setting.type) {
        case SettingType::TOGGLE:
            strncpy(valueStr, setting.value ? "ON" : "OFF", sizeof(valueStr) - 1);
            valueStr[sizeof(valueStr) - 1] = '\0';
            m_canvas->setTextColor(setting.value ? Theme::COLOR_SUCCESS : Theme::COLOR_TEXT_MUTED, bgColor);
            break;
        case SettingType::NUMBER:
            snprintf(valueStr, sizeof(valueStr), "%d", setting.value);
            m_canvas->setTextColor(Theme::COLOR_ACCENT, bgColor);
            break;
        case SettingType::CHOICE:
            snprintf(valueStr, sizeof(valueStr), "%d", setting.value);
            m_canvas->setTextColor(Theme::COLOR_ACCENT, bgColor);
            break;
    }

    m_canvas->setTextDatum(TR_DATUM);
    m_canvas->drawString(valueStr, w - 8, y + 3);

    // Description (second line)
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED, bgColor);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->drawString(setting.description, 8, y + 12);
}

void SettingsPanel::navigateUp() {
    if (m_highlightIndex > 0) {
        m_highlightIndex--;
        m_needsRedraw = true;
    }
}

void SettingsPanel::navigateDown() {
    if (m_highlightIndex < (int)m_settings.size() - 1) {
        m_highlightIndex++;
        m_needsRedraw = true;
    }
}

void SettingsPanel::adjustUp() {
    if (m_highlightIndex >= 0 && m_highlightIndex < (int)m_settings.size()) {
        Setting& s = m_settings[m_highlightIndex];
        if (s.value < s.maxVal) {
            s.value += s.step;
            if (s.value > s.maxVal) s.value = s.maxVal;
            m_needsRedraw = true;
            saveSetting(m_highlightIndex);
        }
    }
}

void SettingsPanel::adjustDown() {
    if (m_highlightIndex >= 0 && m_highlightIndex < (int)m_settings.size()) {
        Setting& s = m_settings[m_highlightIndex];
        if (s.value > s.minVal) {
            s.value -= s.step;
            if (s.value < s.minVal) s.value = s.minVal;
            m_needsRedraw = true;
            saveSetting(m_highlightIndex);
        }
    }
}

void SettingsPanel::select() {
    if (m_highlightIndex >= 0 && m_highlightIndex < (int)m_settings.size()) {
        Setting& s = m_settings[m_highlightIndex];
        if (s.type == SettingType::TOGGLE) {
            s.value = !s.value;
            m_needsRedraw = true;
            saveSetting(m_highlightIndex);
        }
    }
}

bool SettingsPanel::wantsBack() const {
    return m_wantsBack;
}

void SettingsPanel::clearBack() {
    m_wantsBack = false;
}

int SettingsPanel::getScanDurationMs() const {
    return m_settings[0].value * 1000;  // Convert to ms
}

int SettingsPanel::getDeauthPacketCount() const {
    return m_settings[2].value;
}

bool SettingsPanel::getAutoRescan() const {
    return m_settings[3].value != 0;
}

bool SettingsPanel::getSoundEnabled() const {
    return m_settings[4].value != 0;
}

void SettingsPanel::loadSettings() {
    Preferences prefs;
    if (!prefs.begin("vanguard", true)) return; // read-only

    // Settings keys match their index: s0, s1, s2, s3, s4
    for (size_t i = 0; i < m_settings.size(); i++) {
        char key[4];
        snprintf(key, sizeof(key), "s%d", (int)i);
        if (prefs.isKey(key)) {
            m_settings[i].value = prefs.getInt(key, m_settings[i].value);
            // Clamp to valid range
            if (m_settings[i].value < m_settings[i].minVal) m_settings[i].value = m_settings[i].minVal;
            if (m_settings[i].value > m_settings[i].maxVal) m_settings[i].value = m_settings[i].maxVal;
        }
    }

    prefs.end();
    if (Serial) Serial.println("[Settings] Loaded from NVS");
}

void SettingsPanel::saveSetting(int index) {
    if (index < 0 || index >= (int)m_settings.size()) return;

    Preferences prefs;
    if (!prefs.begin("vanguard", false)) return; // read-write

    char key[4];
    snprintf(key, sizeof(key), "s%d", index);
    prefs.putInt(key, m_settings[index].value);

    prefs.end();
}

} // namespace Vanguard
