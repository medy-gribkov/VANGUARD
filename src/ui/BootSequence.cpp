/**
 * @file BootSequence.cpp
 * @brief Boot animation - simplified and stable
 */

#include "BootSequence.h"
#include "Theme.h"
#include <M5Cardputer.h>

namespace Assessor {

BootSequence::BootSequence()
    : m_phase(BootPhase::LOGO_FADE_IN)
    , m_phaseStartMs(0)
    , m_lastFrameMs(0)
    , m_firstBoot(true)
    , m_fadeLevel(0)
{
}

void BootSequence::begin() {
    m_phase = BootPhase::LOGO_FADE_IN;
    m_phaseStartMs = millis();
    m_fadeLevel = 0;
    
    // Clear screen once at start
    M5Cardputer.Display.fillScreen(Theme::COLOR_BACKGROUND);
}

void BootSequence::tick() {
    if (m_phase == BootPhase::COMPLETE) return;

    uint32_t now = millis();
    uint32_t elapsed = now - m_phaseStartMs;

    // Check for skip input
    if (checkSkipInput()) {
        skip();
        return;
    }

    // Only redraw at ~30fps to avoid flicker
    if (now - m_lastFrameMs < 33) return;
    m_lastFrameMs = now;

    // Render current phase
    switch (m_phase) {
        case BootPhase::LOGO_FADE_IN:
            m_fadeLevel = min(255, (int)(elapsed * 255 / LOGO_FADE_DURATION));
            drawLogo(m_fadeLevel);
            if (elapsed >= LOGO_FADE_DURATION) {
                advancePhase();
            }
            break;

        case BootPhase::TAGLINE_FADE_IN:
            drawLogo(255);
            m_fadeLevel = min(255, (int)(elapsed * 255 / TAGLINE_FADE_DURATION));
            drawTagline(m_fadeLevel);
            if (elapsed >= TAGLINE_FADE_DURATION) {
                advancePhase();
            }
            break;

        case BootPhase::HOLD:
            // Static display - no redraw needed
            if (elapsed >= HOLD_DURATION) {
                advancePhase();
            }
            break;

        case BootPhase::FADE_OUT:
            m_fadeLevel = max(0, 255 - (int)(elapsed * 255 / FADE_OUT_DURATION));
            M5Cardputer.Display.fillScreen(Theme::COLOR_BACKGROUND);
            drawLogo(m_fadeLevel);
            drawTagline(m_fadeLevel);
            if (elapsed >= FADE_OUT_DURATION) {
                advancePhase();
            }
            break;

        default:
            break;
    }
}

bool BootSequence::isComplete() const {
    return m_phase == BootPhase::COMPLETE;
}

void BootSequence::skip() {
    m_phase = BootPhase::COMPLETE;
    M5Cardputer.Display.fillScreen(Theme::COLOR_BACKGROUND);
}

BootPhase BootSequence::getPhase() const {
    return m_phase;
}

bool BootSequence::isFirstBoot() const {
    return m_firstBoot;
}

void BootSequence::markOnboardingComplete() {
    m_firstBoot = false;
}

// =============================================================================
// RENDERING
// =============================================================================

void BootSequence::renderLogoFadeIn() {
    drawLogo(m_fadeLevel);
}

void BootSequence::renderTaglineFadeIn() {
    drawLogo(255);
    drawTagline(m_fadeLevel);
}

void BootSequence::renderHold() {
    drawLogo(255);
    drawTagline(255);
}

void BootSequence::renderOnboarding() {
    drawLogo(255);
    drawTagline(255);
}

void BootSequence::renderFadeOut() {
    drawLogo(m_fadeLevel);
    drawTagline(m_fadeLevel);
}

void BootSequence::drawLogo(uint8_t alpha) {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t y = 30;

    // Draw pixel "V" logo in coral
    uint8_t r = (alpha * 255) / 255;
    uint8_t g = (alpha * 109) / 255;
    uint8_t b = (alpha * 85) / 255;
    uint16_t logoColor = Theme::rgb(r, g, b);

    int16_t logoX = centerX - 16;
    int16_t logoY = y - 12;

    // Draw a stylized V with thick lines
    for (int i = 0; i < 12; i++) {
        // Left side of V
        M5Cardputer.Display.drawPixel(logoX + i, logoY + i, logoColor);
        M5Cardputer.Display.drawPixel(logoX + i + 1, logoY + i, logoColor);
        M5Cardputer.Display.drawPixel(logoX + i, logoY + i + 1, logoColor);
        M5Cardputer.Display.drawPixel(logoX + i + 1, logoY + i + 1, logoColor);
        // Right side of V
        M5Cardputer.Display.drawPixel(logoX + 32 - i, logoY + i, logoColor);
        M5Cardputer.Display.drawPixel(logoX + 31 - i, logoY + i, logoColor);
        M5Cardputer.Display.drawPixel(logoX + 32 - i, logoY + i + 1, logoColor);
        M5Cardputer.Display.drawPixel(logoX + 31 - i, logoY + i + 1, logoColor);
    }

    // "VELORA" text in coral
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(logoColor, Theme::COLOR_BACKGROUND);
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.drawString("VELORA", centerX, y + 22);
}

void BootSequence::drawTagline(uint8_t alpha) {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t y = 80;

    // Coral/pink tagline
    uint8_t r = (alpha * 200) / 255;
    uint8_t g = (alpha * 150) / 255;
    uint8_t b = (alpha * 130) / 255;
    uint16_t color = Theme::rgb(r, g, b);

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(color, Theme::COLOR_BACKGROUND);
    M5Cardputer.Display.setTextDatum(MC_DATUM);
    M5Cardputer.Display.drawString("Target First. Always.", centerX, y);

    // Version hint (dim)
    uint8_t dimAlpha = alpha / 3;
    uint16_t dimColor = Theme::rgb(dimAlpha, dimAlpha, dimAlpha);
    M5Cardputer.Display.setTextColor(dimColor, Theme::COLOR_BACKGROUND);
    M5Cardputer.Display.drawString("v0.2", centerX, y + 20);
}

void BootSequence::drawOnboardingCard() {
    // Simplified - skip for now
}

// =============================================================================
// HELPERS
// =============================================================================

void BootSequence::advancePhase() {
    switch (m_phase) {
        case BootPhase::LOGO_FADE_IN:
            m_phase = BootPhase::TAGLINE_FADE_IN;
            break;
        case BootPhase::TAGLINE_FADE_IN:
            m_phase = BootPhase::HOLD;
            break;
        case BootPhase::HOLD:
            m_phase = BootPhase::FADE_OUT;
            break;
        case BootPhase::ONBOARDING:
            m_phase = BootPhase::FADE_OUT;
            break;
        case BootPhase::FADE_OUT:
            m_phase = BootPhase::COMPLETE;
            M5Cardputer.Display.fillScreen(Theme::COLOR_BACKGROUND);
            break;
        default:
            break;
    }

    m_phaseStartMs = millis();
    m_fadeLevel = 0;
}

bool BootSequence::checkSkipInput() {
    // Keyboard skip is now handled in main.cpp handleKeyboardInput()
    // This function returns false - skip is done via main.cpp calling skip()
    return false;
}

} // namespace Assessor
