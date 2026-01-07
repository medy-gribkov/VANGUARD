/**
 * @file BootSequence.cpp
 * @brief Boot animation and onboarding
 */

#include "BootSequence.h"
#include "Theme.h"
#include <Preferences.h>

namespace Assessor {

// Preference key for first boot
static const char* PREF_NAMESPACE = "assessor";
static const char* PREF_ONBOARDED = "onboarded";

BootSequence::BootSequence()
    : m_phase(BootPhase::LOGO_FADE_IN)
    , m_phaseStartMs(0)
    , m_lastFrameMs(0)
    , m_firstBoot(true)
    , m_fadeLevel(0)
{
    // Check if onboarding was completed
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    m_firstBoot = !prefs.getBool(PREF_ONBOARDED, false);
    prefs.end();
}

void BootSequence::begin() {
    m_phase = BootPhase::LOGO_FADE_IN;
    m_phaseStartMs = millis();
    m_fadeLevel = 0;

    // Clear screen
    M5.Display.fillScreen(Theme::COLOR_BACKGROUND);
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

    // Render current phase
    switch (m_phase) {
        case BootPhase::LOGO_FADE_IN:
            m_fadeLevel = min(255, (int)(elapsed * 255 / LOGO_FADE_DURATION));
            renderLogoFadeIn();
            if (elapsed >= LOGO_FADE_DURATION) {
                advancePhase();
            }
            break;

        case BootPhase::TAGLINE_FADE_IN:
            m_fadeLevel = min(255, (int)(elapsed * 255 / TAGLINE_FADE_DURATION));
            renderTaglineFadeIn();
            if (elapsed >= TAGLINE_FADE_DURATION) {
                advancePhase();
            }
            break;

        case BootPhase::HOLD:
            renderHold();
            if (elapsed >= HOLD_DURATION) {
                advancePhase();
            }
            break;

        case BootPhase::ONBOARDING:
            renderOnboarding();
            if (elapsed >= ONBOARDING_DURATION || !m_firstBoot) {
                advancePhase();
            }
            break;

        case BootPhase::FADE_OUT:
            m_fadeLevel = max(0, 255 - (int)(elapsed * 255 / FADE_OUT_DURATION));
            renderFadeOut();
            if (elapsed >= FADE_OUT_DURATION) {
                advancePhase();
            }
            break;

        default:
            break;
    }

    m_lastFrameMs = now;
}

bool BootSequence::isComplete() const {
    return m_phase == BootPhase::COMPLETE;
}

void BootSequence::skip() {
    m_phase = BootPhase::COMPLETE;
    M5.Display.fillScreen(Theme::COLOR_BACKGROUND);
}

BootPhase BootSequence::getPhase() const {
    return m_phase;
}

bool BootSequence::isFirstBoot() const {
    return m_firstBoot;
}

void BootSequence::markOnboardingComplete() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.putBool(PREF_ONBOARDED, true);
    prefs.end();
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
    drawOnboardingCard();
}

void BootSequence::renderFadeOut() {
    // Fade to black
    uint8_t darkness = 255 - m_fadeLevel;
    M5.Display.fillScreen(Theme::rgb(darkness, darkness, darkness));
    drawLogo(m_fadeLevel);
    drawTagline(m_fadeLevel);
}

void BootSequence::drawLogo(uint8_t alpha) {
    // Simple text-based logo for now
    // Could be replaced with sprite later

    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t y = 30;

    // Scale alpha to color brightness
    uint8_t bright = (alpha * 255) / 255;
    uint16_t color = Theme::rgb(bright, bright, bright);

    M5.Display.setTextSize(2);
    M5.Display.setTextColor(color, Theme::COLOR_BACKGROUND);
    M5.Display.setTextDatum(MC_DATUM);

    M5.Display.drawString("THE ASSESSOR", centerX, y);
}

void BootSequence::drawTagline(uint8_t alpha) {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t y = 60;

    uint8_t bright = (alpha * 200) / 255;  // Slightly dimmer
    uint16_t color = Theme::rgb(0, bright, bright);  // Cyan tint

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(color, Theme::COLOR_BACKGROUND);
    M5.Display.setTextDatum(MC_DATUM);

    M5.Display.drawString("Know your target first.", centerX, y);
}

void BootSequence::drawOnboardingCard() {
    int16_t x = 20;
    int16_t y = 80;
    int16_t w = Theme::SCREEN_WIDTH - 40;
    int16_t h = 45;

    // Card background
    M5.Display.fillRoundRect(x, y, w, h, 4, Theme::COLOR_SURFACE);

    // Text
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(Theme::COLOR_TEXT_SECONDARY, Theme::COLOR_SURFACE);
    M5.Display.setTextDatum(TL_DATUM);

    M5.Display.drawString("Scan. Select. Strike.", x + 8, y + 8);
    M5.Display.drawString("Press any key to skip.", x + 8, y + 24);
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
            m_phase = m_firstBoot ? BootPhase::ONBOARDING : BootPhase::FADE_OUT;
            break;
        case BootPhase::ONBOARDING:
            markOnboardingComplete();
            m_phase = BootPhase::FADE_OUT;
            break;
        case BootPhase::FADE_OUT:
            m_phase = BootPhase::COMPLETE;
            M5.Display.fillScreen(Theme::COLOR_BACKGROUND);
            break;
        default:
            break;
    }

    m_phaseStartMs = millis();
    m_fadeLevel = 0;
}

bool BootSequence::checkSkipInput() {
    // Check for any button press
    if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) {
        return true;
    }

    // Check Cardputer keyboard via M5.BtnPWR (G key on Cardputer)
    if (M5.BtnPWR.wasPressed()) {
        return true;
    }

    return false;
}

} // namespace Assessor
