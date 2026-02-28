#ifndef VANGUARD_ABOUT_PANEL_H
#define VANGUARD_ABOUT_PANEL_H

/**
 * @file AboutPanel.h
 * @brief About dialog showing version info and credits
 */

#include <M5Cardputer.h>
#include "Theme.h"

namespace Vanguard {

class AboutPanel {
public:
    AboutPanel();
    ~AboutPanel();

    /**
     * @brief Show/hide panel
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
     * @brief Check if user wants to close (any key pressed)
     */
    bool wantsBack() const;
    void clearBack();

private:
    bool         m_visible;
    bool         m_wantsBack;
    M5Canvas*    m_canvas;
    uint32_t     m_lastRenderMs;

    static constexpr uint32_t RENDER_INTERVAL_MS = 50;
};

} // namespace Vanguard

#endif // VANGUARD_ABOUT_PANEL_H
