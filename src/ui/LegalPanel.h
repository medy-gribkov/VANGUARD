#ifndef VANGUARD_LEGAL_PANEL_H
#define VANGUARD_LEGAL_PANEL_H

/**
 * @file LegalPanel.h
 * @brief Legal disclaimer panel with first-boot acknowledgment
 */

#include <M5Cardputer.h>
#include "Theme.h"

namespace Vanguard {

class LegalPanel {
public:
    LegalPanel();
    ~LegalPanel();

    void show();
    void hide();
    bool isVisible() const;

    void tick();
    void render();

    bool wantsBack() const;
    void clearBack();

    /** Check if user has previously acknowledged the disclaimer (NVS) */
    static bool hasAcknowledged();
    /** Mark disclaimer as acknowledged (NVS) */
    void acknowledge();

    /** @brief Input methods called from main.cpp */
    void scrollUp() { m_scrollY = (m_scrollY > 10) ? m_scrollY - 10 : 0; }
    void scrollDown() { m_scrollY += 10; }
    void onKeyBack() { m_wantsBack = true; }
    void onKeyAccept() { if (!m_acknowledged) acknowledge(); m_wantsBack = true; }

private:
    bool         m_visible;
    bool         m_wantsBack;
    bool         m_acknowledged;
    int16_t      m_scrollY;
    M5Canvas*    m_canvas;
    uint32_t     m_lastRenderMs;

    static constexpr uint32_t RENDER_INTERVAL_MS = 50;
};

} // namespace Vanguard

#endif // VANGUARD_LEGAL_PANEL_H
