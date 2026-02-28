/**
 * @file AboutPanel.cpp
 * @brief About dialog showing version info and credits
 */

#include "AboutPanel.h"
#include <M5Cardputer.h>
#include "CanvasManager.h"

namespace Vanguard {

AboutPanel::AboutPanel()
    : m_visible(false)
    , m_wantsBack(false)
    , m_wantsLegal(false)
    , m_canvas(&CanvasManager::getInstance().getCanvas())
    , m_lastRenderMs(0)
{
}

AboutPanel::~AboutPanel() {
    // m_canvas is shared, do not delete
}

void AboutPanel::show() {
    m_visible = true;
    m_wantsBack = false;
    m_wantsLegal = false;
}

void AboutPanel::hide() {
    m_visible = false;
}

bool AboutPanel::isVisible() const {
    return m_visible;
}

void AboutPanel::tick() {
    // Input handled externally in main.cpp
}

void AboutPanel::render() {
    if (!m_visible || !m_canvas) return;

    // Frame rate limiting
    uint32_t now = millis();
    if ((now - m_lastRenderMs) < RENDER_INTERVAL_MS) {
        return;
    }
    m_lastRenderMs = now;

    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);

    // Accent bar
    m_canvas->fillRect(0, 0, Theme::SCREEN_WIDTH, 4, Theme::COLOR_ACCENT);

    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t y = 10;

    // App name
    m_canvas->setTextSize(2);
    m_canvas->setTextDatum(TC_DATUM);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString("VANGUARD", centerX, y);
    y += 20;

    // Version
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->drawString(Theme::VERSION_STRING, centerX, y);
    y += 12;

    // Tagline
    m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);
    m_canvas->drawString("Target First. Always.", centerX, y);
    y += 14;

    // GitHub
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString("github.com/Mahdy-gribkov/VANGUARD", centerX, y);
    y += 12;

    // License
    m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);
    m_canvas->drawString("License: AGPL-3.0", centerX, y);
    y += 14;

    // Credits
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString("Based on Bruce firmware concepts", centerX, y);
    y += 10;
    m_canvas->drawString("ESP32-S3 / M5Stack Cardputer ADV", centerX, y);
    y += 10;
    m_canvas->drawString("By Medy Gribkov", centerX, y);

    // Footer
    m_canvas->fillRect(0, Theme::SCREEN_HEIGHT - 14, Theme::SCREEN_WIDTH, 14, Theme::COLOR_SURFACE);
    m_canvas->drawFastHLine(0, Theme::SCREEN_HEIGHT - 14, Theme::SCREEN_WIDTH, Theme::COLOR_ACCENT_DIM);
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->setTextDatum(MC_DATUM);
    m_canvas->drawString("[ENTER] Legal  [Q] Back", centerX, Theme::SCREEN_HEIGHT - 7);

    m_canvas->pushSprite(0, 0);
}

bool AboutPanel::wantsBack() const {
    return m_wantsBack;
}

void AboutPanel::clearBack() {
    m_wantsBack = false;
}

bool AboutPanel::wantsLegal() const {
    return m_wantsLegal;
}

void AboutPanel::clearLegal() {
    m_wantsLegal = false;
}

} // namespace Vanguard
