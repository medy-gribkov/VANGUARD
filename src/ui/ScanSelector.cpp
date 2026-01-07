/**
 * @file ScanSelector.cpp
 * @brief Velora - Post-boot scan type selection screen
 * PINKISH-ORANGE THEME
 */

#include "ScanSelector.h"

namespace Assessor {

ScanSelector::ScanSelector()
    : m_visible(false)
    , m_needsRedraw(true)
    , m_selection(ScanChoice::NONE)
    , m_canvas(nullptr)
    , m_lastRenderMs(0)
    , m_animFrame(0)
{
    m_canvas = new M5Canvas(&M5Cardputer.Display);
    m_canvas->createSprite(Theme::SCREEN_WIDTH, Theme::SCREEN_HEIGHT);
}

ScanSelector::~ScanSelector() {
    if (m_canvas) {
        m_canvas->deleteSprite();
        delete m_canvas;
        m_canvas = nullptr;
    }
}

void ScanSelector::show() {
    m_visible = true;
    m_needsRedraw = true;
    m_selection = ScanChoice::NONE;
    m_animFrame = 0;
}

void ScanSelector::hide() {
    m_visible = false;
}

bool ScanSelector::isVisible() const {
    return m_visible;
}

void ScanSelector::tick() {
    uint32_t now = millis();
    if (now - m_lastRenderMs >= ANIM_INTERVAL_MS) {
        m_animFrame++;
        m_needsRedraw = true;
    }
}

void ScanSelector::render() {
    if (!m_visible || !m_canvas) return;

    uint32_t now = millis();
    if (!m_needsRedraw && (now - m_lastRenderMs) < RENDER_INTERVAL_MS) {
        return;
    }
    m_lastRenderMs = now;
    m_needsRedraw = false;

    // Dark background
    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);

    // Coral/pink accent bar at top
    m_canvas->fillRect(0, 0, Theme::SCREEN_WIDTH, 4, Theme::COLOR_ACCENT);

    // Draw a simple pixel "V" logo
    int16_t logoX = Theme::SCREEN_WIDTH / 2 - 12;
    int16_t logoY = 10;
    // V shape in coral
    for (int i = 0; i < 8; i++) {
        m_canvas->drawPixel(logoX + i, logoY + i, Theme::COLOR_ACCENT);
        m_canvas->drawPixel(logoX + i + 1, logoY + i, Theme::COLOR_ACCENT);
        m_canvas->drawPixel(logoX + 24 - i, logoY + i, Theme::COLOR_ACCENT);
        m_canvas->drawPixel(logoX + 23 - i, logoY + i, Theme::COLOR_ACCENT);
    }

    // Title - VELORA in coral
    m_canvas->setTextSize(2);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->setTextDatum(TC_DATUM);
    m_canvas->drawString("VELORA", Theme::SCREEN_WIDTH / 2, 22);

    // Subtitle with blinking cursor
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);
    const char* subtitle = (m_animFrame % 2 == 0) ? "Target First. Always." : "Target First. Always._";
    m_canvas->drawString(subtitle, Theme::SCREEN_WIDTH / 2, 42);

    // Coral separator
    m_canvas->drawFastHLine(40, 52, Theme::SCREEN_WIDTH - 80, Theme::COLOR_ACCENT);

    // Scan options
    int16_t optionY = 62;
    int16_t optionSpacing = 22;

    drawOption(optionY, "R", "WiFi Scan", Theme::COLOR_ACCENT);
    optionY += optionSpacing;

    drawOption(optionY, "B", "Bluetooth Scan", Theme::COLOR_ACCENT);
    optionY += optionSpacing;

    drawOption(optionY, "OK", "Full Scan", Theme::COLOR_ACCENT_BRIGHT);

    // Footer
    m_canvas->fillRect(0, Theme::SCREEN_HEIGHT - 14, Theme::SCREEN_WIDTH, 14, Theme::COLOR_SURFACE);
    m_canvas->drawFastHLine(0, Theme::SCREEN_HEIGHT - 14, Theme::SCREEN_WIDTH, Theme::COLOR_ACCENT_DIM);
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->setTextDatum(MC_DATUM);
    m_canvas->drawString("[M] Menu", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT - 7);

    m_canvas->pushSprite(0, 0);
}

void ScanSelector::drawOption(int16_t y, const char* key, const char* label, uint16_t keyColor) {
    int16_t keyBoxX = 30;
    int16_t keyBoxW = (strlen(key) > 1) ? 24 : 18;
    int16_t keyBoxH = 16;

    // Coral bordered box
    m_canvas->fillRoundRect(keyBoxX, y, keyBoxW, keyBoxH, 3, Theme::COLOR_SURFACE);
    m_canvas->drawRoundRect(keyBoxX, y, keyBoxW, keyBoxH, 3, keyColor);

    // Key text in coral
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(keyColor);
    m_canvas->setTextDatum(MC_DATUM);
    m_canvas->drawString(key, keyBoxX + keyBoxW / 2, y + keyBoxH / 2);

    // Label in white
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->setTextDatum(ML_DATUM);
    m_canvas->drawString(label, keyBoxX + keyBoxW + 12, y + keyBoxH / 2);
}

bool ScanSelector::hasSelection() const {
    return m_selection != ScanChoice::NONE;
}

ScanChoice ScanSelector::getSelection() const {
    return m_selection;
}

void ScanSelector::clearSelection() {
    m_selection = ScanChoice::NONE;
}

void ScanSelector::onKeyR() {
    m_selection = ScanChoice::WIFI_ONLY;
}

void ScanSelector::onKeyB() {
    m_selection = ScanChoice::BLE_ONLY;
}

void ScanSelector::onKeyEnter() {
    m_selection = ScanChoice::COMBINED;
}

} // namespace Assessor
