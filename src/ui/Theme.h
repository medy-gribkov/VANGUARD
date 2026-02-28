#ifndef VANGUARD_THEME_H
#define VANGUARD_THEME_H

/**
 * @file Theme.h
 * @brief Visual constants for Vanguard UI
 *
 * PINKISH-ORANGE THEME
 * All colors, fonts, spacing, and sizing live here.
 *
 * Color format: RGB565 (16-bit) for LGFX compatibility
 */

#include <cstdint>

namespace Vanguard {

namespace Theme {

// =============================================================================
// DISPLAY DIMENSIONS (M5Stack Cardputer in landscape)
// =============================================================================

constexpr int16_t SCREEN_WIDTH  = 240;
constexpr int16_t SCREEN_HEIGHT = 135;

// =============================================================================
// COLOR PALETTE - Vanguard PINKISH-ORANGE
// =============================================================================

// Background colors
constexpr uint16_t COLOR_BACKGROUND      = 0x0000;  // Pure black
constexpr uint16_t COLOR_SURFACE         = 0x18E3;  // Dark gray #1C1C1C
constexpr uint16_t COLOR_SURFACE_RAISED  = 0x31A6;  // Lighter gray #303030

// Text colors
constexpr uint16_t COLOR_TEXT_PRIMARY    = 0xFFFF;  // White
constexpr uint16_t COLOR_TEXT_SECONDARY  = 0xB5B6;  // Light gray
constexpr uint16_t COLOR_TEXT_MUTED      = 0x7BEF;  // Mid gray
constexpr uint16_t COLOR_TEXT_DISABLED   = 0x4208;  // Dark gray

// Accent colors - PINKISH-ORANGE (Coral/Salmon)
constexpr uint16_t COLOR_ACCENT          = 0xFB6D;  // Coral #FF6D55
constexpr uint16_t COLOR_ACCENT_BRIGHT   = 0xFE10;  // Brighter #FF8270
constexpr uint16_t COLOR_ACCENT_DIM      = 0x8206;  // Muted #844136
constexpr uint16_t COLOR_ACCENT_PINK     = 0xF8B2;  // Pinkish #F81690

constexpr uint16_t COLOR_SUCCESS         = 0x07E0;  // Green
constexpr uint16_t COLOR_WARNING         = 0xFFE0;  // Yellow
constexpr uint16_t COLOR_DANGER          = 0xF800;  // Red

// Signal strength colors
constexpr uint16_t COLOR_SIGNAL_EXCELLENT = 0x07E0;  // Green
constexpr uint16_t COLOR_SIGNAL_GOOD      = 0x87E0;  // Yellow-green
constexpr uint16_t COLOR_SIGNAL_FAIR      = 0xFB6D;  // Coral (accent)
constexpr uint16_t COLOR_SIGNAL_WEAK      = 0xFA00;  // Red-orange
constexpr uint16_t COLOR_SIGNAL_POOR      = 0xF800;  // Red

// Target type colors
constexpr uint16_t COLOR_TYPE_AP          = 0x07FF;  // Cyan
constexpr uint16_t COLOR_TYPE_STATION     = 0xFFE0;  // Yellow
constexpr uint16_t COLOR_TYPE_BLE         = 0xF81F;  // Magenta/Pink

// Security colors
constexpr uint16_t COLOR_SECURITY_OPEN    = 0x07E0;  // Green (easy target)
constexpr uint16_t COLOR_SECURITY_WEP     = 0x87E0;  // Yellow-green
constexpr uint16_t COLOR_SECURITY_WPA     = 0xFB6D;  // Coral
constexpr uint16_t COLOR_SECURITY_WPA2    = 0xFA00;  // Red-orange
constexpr uint16_t COLOR_SECURITY_WPA3    = 0xF800;  // Red (hardest)

// =============================================================================
// SPACING (in pixels)
// =============================================================================

constexpr int16_t PADDING_XS     = 2;
constexpr int16_t PADDING_SM     = 4;
constexpr int16_t PADDING_MD     = 8;
constexpr int16_t PADDING_LG     = 12;
constexpr int16_t PADDING_XL     = 16;

constexpr int16_t MARGIN_XS      = 2;
constexpr int16_t MARGIN_SM      = 4;
// Branding
static constexpr const char* APP_NAME = "VANGUARD";
static constexpr const char* VERSION_STRING = "v2.0";

// =============================================================================
// TYPOGRAPHY
// =============================================================================

constexpr int8_t FONT_SIZE_XS    = 1;
constexpr int8_t FONT_SIZE_SM    = 1;
constexpr int8_t FONT_SIZE_MD    = 2;
constexpr int8_t FONT_SIZE_LG    = 2;
constexpr int8_t FONT_SIZE_XL    = 3;

constexpr int16_t LINE_HEIGHT_SM = 10;
constexpr int16_t LINE_HEIGHT_MD = 14;
constexpr int16_t LINE_HEIGHT_LG = 18;

// =============================================================================
// COMPONENTS
// =============================================================================

constexpr int16_t HEADER_HEIGHT  = 16;
constexpr int16_t LIST_ITEM_HEIGHT     = 24;
constexpr int16_t LIST_ITEM_PADDING    = PADDING_SM;
constexpr int16_t LIST_VISIBLE_ITEMS   = 4;
constexpr int16_t BUTTON_HEIGHT        = 20;
constexpr int16_t BUTTON_MIN_WIDTH     = 40;
constexpr int16_t BUTTON_RADIUS        = 4;
constexpr int16_t PROGRESS_HEIGHT      = 6;
constexpr int16_t PROGRESS_RADIUS      = 3;
constexpr int16_t ICON_SIZE_SM         = 8;
constexpr int16_t ICON_SIZE_MD         = 12;
constexpr int16_t ICON_SIZE_LG         = 16;

// =============================================================================
// ANIMATION TIMING
// =============================================================================

constexpr uint32_t ANIM_FADE_DURATION  = 300;
constexpr uint32_t ANIM_SLIDE_DURATION = 200;
constexpr uint32_t ANIM_PULSE_PERIOD   = 1000;
constexpr uint32_t ANIM_BLINK_PERIOD   = 500;

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

inline uint16_t getSignalColor(int8_t rssi) {
    if (rssi > -50) return COLOR_SIGNAL_EXCELLENT;
    if (rssi > -60) return COLOR_SIGNAL_GOOD;
    if (rssi > -70) return COLOR_SIGNAL_FAIR;
    if (rssi > -80) return COLOR_SIGNAL_WEAK;
    return COLOR_SIGNAL_POOR;
}

inline uint16_t getSecurityColor(uint8_t securityType) {
    switch (securityType) {
        case 0: return COLOR_SECURITY_OPEN;
        case 1: return COLOR_SECURITY_WEP;
        case 2: return COLOR_SECURITY_WPA;
        case 3: return COLOR_SECURITY_WPA2;
        case 4: return COLOR_SECURITY_WPA2;
        case 5: return COLOR_SECURITY_WPA3;
        default: return COLOR_TEXT_MUTED;
    }
}

} // namespace Theme

} // namespace Vanguard

#endif // VANGUARD_THEME_H
