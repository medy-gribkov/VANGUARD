/**
 * @file LegalPanel.cpp
 * @brief Legal disclaimer with first-boot acknowledgment via NVS
 */

#include "LegalPanel.h"
#include <Preferences.h>
#include "CanvasManager.h"

namespace Vanguard {

static const char* LEGAL_LINES[] = {
    "VANGUARD is provided for",
    "authorized security testing",
    "and educational purposes only.",
    "",
    "Unauthorized access to computer",
    "networks is illegal under the",
    "Computer Fraud and Abuse Act",
    "(CFAA) and equivalent laws.",
    "",
    "Users are solely responsible",
    "for ensuring compliance with",
    "all applicable laws.",
    "",
    "The developers assume no",
    "liability for misuse.",
    "",
    "Licensed under AGPL-3.0.",
};
static constexpr int LEGAL_LINE_COUNT = sizeof(LEGAL_LINES) / sizeof(LEGAL_LINES[0]);

LegalPanel::LegalPanel()
    : m_visible(false)
    , m_wantsBack(false)
    , m_acknowledged(false)
    , m_scrollY(0)
    , m_canvas(&CanvasManager::getInstance().getCanvas())
    , m_lastRenderMs(0)
{
}

LegalPanel::~LegalPanel() {
    // m_canvas is shared, do not delete
}

void LegalPanel::show() {
    m_visible = true;
    m_wantsBack = false;
    m_scrollY = 0;
    m_acknowledged = hasAcknowledged();
}

void LegalPanel::hide() {
    m_visible = false;
}

bool LegalPanel::isVisible() const {
    return m_visible;
}

void LegalPanel::tick() {
    // Input handled externally
}

void LegalPanel::render() {
    if (!m_visible || !m_canvas) return;

    uint32_t now = millis();
    if ((now - m_lastRenderMs) < RENDER_INTERVAL_MS) return;
    m_lastRenderMs = now;

    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);

    // Header
    m_canvas->fillRect(0, 0, Theme::SCREEN_WIDTH, 14, Theme::COLOR_SURFACE);
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->drawString("LEGAL DISCLAIMER", 4, 3);

    // Scrollable text area
    int16_t y = 18 - m_scrollY;
    int16_t lineH = 10;
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->setTextDatum(TL_DATUM);

    for (int i = 0; i < LEGAL_LINE_COUNT; i++) {
        if (y > -lineH && y < Theme::SCREEN_HEIGHT - 16) {
            m_canvas->drawString(LEGAL_LINES[i], 8, y);
        }
        y += lineH;
    }

    // Footer
    m_canvas->fillRect(0, Theme::SCREEN_HEIGHT - 14, Theme::SCREEN_WIDTH, 14, Theme::COLOR_SURFACE);
    m_canvas->drawFastHLine(0, Theme::SCREEN_HEIGHT - 14, Theme::SCREEN_WIDTH, Theme::COLOR_ACCENT_DIM);
    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(MC_DATUM);

    if (!m_acknowledged) {
        m_canvas->setTextColor(Theme::COLOR_ACCENT);
        m_canvas->drawString("[ENTER] I Understand", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT - 7);
    } else {
        m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
        m_canvas->drawString("[Q] Back  [;,.] Scroll", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT - 7);
    }

    m_canvas->pushSprite(0, 0);
}

bool LegalPanel::wantsBack() const {
    return m_wantsBack;
}

void LegalPanel::clearBack() {
    m_wantsBack = false;
}

bool LegalPanel::hasAcknowledged() {
    Preferences prefs;
    if (!prefs.begin("vanguard", true)) return false;
    bool ack = prefs.getBool("legal_ack", false);
    prefs.end();
    return ack;
}

void LegalPanel::acknowledge() {
    m_acknowledged = true;
    Preferences prefs;
    if (!prefs.begin("vanguard", false)) return;
    prefs.putBool("legal_ack", true);
    prefs.end();
    if (Serial) Serial.println("[Legal] Disclaimer acknowledged");
}

} // namespace Vanguard
