#ifndef ASSESSOR_MAIN_MENU_H
#define ASSESSOR_MAIN_MENU_H

/**
 * @file MainMenu.h
 * @brief Global menu accessible from any screen
 *
 * Press 'M' from anywhere to bring up the menu.
 * Menu options:
 * - Rescan WiFi
 * - Settings (TODO)
 * - About
 * - Back to Radar
 */

#include <M5Cardputer.h>
#include "Theme.h"
#include <vector>

namespace Assessor {

enum class MenuAction {
    NONE,
    RESCAN,
    RESCAN_BLE,
    SETTINGS,
    ABOUT,
    BACK
};

struct MenuItem {
    const char* label;
    MenuAction  action;
};

class MainMenu {
public:
    MainMenu();
    ~MainMenu();

    /**
     * @brief Show/hide menu
     */
    void show();
    void hide();
    bool isVisible() const;

    /**
     * @brief Process input and update state
     */
    void tick();

    /**
     * @brief Render menu overlay
     */
    void render();

    /**
     * @brief Navigation
     */
    void navigateUp();
    void navigateDown();
    void select();

    /**
     * @brief Check if an action was selected
     */
    bool hasAction() const;
    MenuAction getAction();

private:
    bool         m_visible;
    int          m_highlightIndex;
    bool         m_hasAction;
    MenuAction   m_selectedAction;
    bool         m_needsRedraw;       // Only redraw when needed

    M5Canvas*    m_canvas;
    uint32_t     m_lastRenderMs;
    static constexpr uint32_t RENDER_INTERVAL_MS = 50;  // 20 FPS max

    std::vector<MenuItem> m_items;

    void renderMenuItem(int index, int y);
};

} // namespace Assessor

#endif // ASSESSOR_MAIN_MENU_H
