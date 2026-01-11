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
}

void AboutPanel::hide() {
    m_visible = false;
}

bool AboutPanel::isVisible() const {
    return m_visible;
}

void AboutPanel::tick() {
    // Any key press closes the dialog
    if (m_visible && M5Cardputer.Keyboard.isPressed()) {
        m_wantsBack = true;
    }
}

void AboutPanel::render() {
    if (!m_visible || !m_canvas) return;

    // Frame rate limiting
    uint32_t now = millis();
    if ((now - m_lastRenderMs) < RENDER_INTERVAL_MS) {
        return;
    }
    m_lastRenderMs = now;

    // Draw to sprite
    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);

    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t y = 20;

    // App name
    m_canvas->setTextSize(2);
    m_canvas->setTextDatum(MC_DATUM);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString("VANGUARD", centerX, y);
    y += 24;

    // Version
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->drawString(Theme::VERSION_STRING, centerX, y);
    y += 16;

    // Tagline
    m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);
    m_canvas->drawString("Target First. Always.", centerX, y);
    y += 20;

    // Description
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString("A target-first auditing tool", centerX, y);
    y += 12;
    m_canvas->drawString("for M5Stack Cardputer", centerX, y);
    y += 16;

    // Credits
    m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);
    m_canvas->drawString("Based on Bruce firmware concepts", centerX, y);
    y += 16;

    // Hardware
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString("ESP32-S3 / M5Stack Cardputer", centerX, y);

    // Footer - close hint
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->setTextDatum(TC_DATUM);
    m_canvas->drawString("VANGUARD", Theme::SCREEN_WIDTH / 2, 8);
    m_canvas->setTextColor(Theme::COLOR_ACCENT); // Restore color for the actual footer
    m_canvas->setTextDatum(BC_DATUM); // Restore datum for the actual footer
    m_canvas->drawString("[Any key] Close", centerX, Theme::SCREEN_HEIGHT - 4);

    // Push to display
    m_canvas->pushSprite(0, 0);
}

bool AboutPanel::wantsBack() const {
    return m_wantsBack;
}

void AboutPanel::clearBack() {
    m_wantsBack = false;
}

} // namespace Vanguard
