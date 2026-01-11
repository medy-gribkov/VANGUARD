#ifndef VANGUARD_SCAN_SELECTOR_H
#define VANGUARD_SCAN_SELECTOR_H

/**
 * @file ScanSelector.h
 * @brief Post-boot screen for scan type selection
 *
 * Shows options:
 * - [R] Scan WiFi
 * - [B] Scan Bluetooth
 * - [Enter] Scan Both
 */

#include <M5Cardputer.h>
#include "Theme.h"
#include "CanvasManager.h"

namespace Vanguard {

enum class ScanChoice {
    NONE,
    WIFI_ONLY,
    BLE_ONLY,
    COMBINED
};

class ScanSelector {
public:
    ScanSelector();
    ~ScanSelector();

    /**
     * @brief Show/hide the selector screen
     */
    void show();
    void hide();
    bool isVisible() const;

    /**
     * @brief Update and render
     */
    void tick();
    void render();

    /**
     * @brief Check if user made a selection
     */
    bool hasSelection() const;
    ScanChoice getSelection() const;
    void clearSelection();

    /**
     * @brief Handle key input
     */
    void onKeyR();      // WiFi scan
    void onKeyB();      // BLE scan
    void onKeyEnter();  // Combined scan

private:
    bool m_visible;
    bool m_needsRedraw;
    ScanChoice m_selection;

    M5Canvas* m_canvas;
    uint32_t m_lastRenderMs;
    uint32_t m_animFrame;

    static constexpr uint32_t RENDER_INTERVAL_MS = 100;
    static constexpr uint32_t ANIM_INTERVAL_MS = 500;

    void drawOption(int16_t y, const char* key, const char* label, uint16_t keyColor);
};

} // namespace Vanguard

#endif // ASSESSOR_SCAN_SELECTOR_H
