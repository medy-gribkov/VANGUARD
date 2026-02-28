/**
 * @file MainMenu.cpp
 * @brief Global menu accessible from any screen
 */

#include "MainMenu.h"
#include <M5Cardputer.h>
#include "CanvasManager.h"

namespace Vanguard {

MainMenu::MainMenu()
    : m_visible(false)
    , m_highlightIndex(0)
    , m_hasAction(false)
    , m_selectedAction(MenuAction::NONE)
    , m_needsRedraw(true)
    , m_canvas(&CanvasManager::getInstance().getCanvas())
    , m_lastRenderMs(0)
{
    // Setup menu items
    m_items.push_back({"New Scan", MenuAction::NEW_SCAN});
    m_items.push_back({"Spectrum Analyzer", MenuAction::SPECTRUM});
    m_items.push_back({"Settings", MenuAction::SETTINGS});
    m_items.push_back({"About & Legal", MenuAction::ABOUT});
    m_items.push_back({"Close", MenuAction::BACK});
}

MainMenu::~MainMenu() {
    // m_canvas is shared, do not delete
}

void MainMenu::show() {
    m_visible = true;
    m_highlightIndex = 0;
    m_hasAction = false;
    m_selectedAction = MenuAction::NONE;
    m_needsRedraw = true;  // Force first draw
}

void MainMenu::hide() {
    m_visible = false;
}

bool MainMenu::isVisible() const {
    return m_visible;
}

void MainMenu::tick() {
    // Input is handled externally in main.cpp
}

void MainMenu::render() {
    if (!m_visible) return;

    // Frame rate limiting - only redraw when needed or at minimum interval
    uint32_t now = millis();
    if (!m_needsRedraw && (now - m_lastRenderMs) < RENDER_INTERVAL_MS) {
        return;
    }
    m_lastRenderMs = now;
    m_needsRedraw = false;

    // Near-fullscreen menu
    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);

    // Accent bar at top
    m_canvas->fillRect(0, 0, Theme::SCREEN_WIDTH, 4, Theme::COLOR_ACCENT);

    // Header
    m_canvas->fillRect(0, 4, Theme::SCREEN_WIDTH, 16, Theme::COLOR_SURFACE);
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->drawString("VANGUARD MENU", 8, 8);

    // Menu items
    int16_t itemY = 24;
    int16_t itemH = 20;
    for (size_t i = 0; i < m_items.size(); i++) {
        renderMenuItem((int)i, itemY);
        itemY += itemH;
    }

    // Footer
    m_canvas->fillRect(0, Theme::SCREEN_HEIGHT - 14, Theme::SCREEN_WIDTH, 14, Theme::COLOR_SURFACE);
    m_canvas->drawFastHLine(0, Theme::SCREEN_HEIGHT - 14, Theme::SCREEN_WIDTH, Theme::COLOR_ACCENT_DIM);
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->setTextDatum(MC_DATUM);
    m_canvas->drawString("[;,.] Nav  [ENTER] Select  [M] Close", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT - 7);

    m_canvas->pushSprite(0, 0);
}

void MainMenu::navigateUp() {
    if (m_highlightIndex > 0) {
        m_highlightIndex--;
        m_needsRedraw = true;
    }
}

void MainMenu::navigateDown() {
    if (m_highlightIndex < (int)m_items.size() - 1) {
        m_highlightIndex++;
        m_needsRedraw = true;
    }
}

void MainMenu::select() {
    if (m_highlightIndex >= 0 && m_highlightIndex < (int)m_items.size()) {
        m_selectedAction = m_items[m_highlightIndex].action;
        m_hasAction = true;
        m_visible = false;
    }
}

bool MainMenu::hasAction() const {
    return m_hasAction;
}

MenuAction MainMenu::getAction() {
    MenuAction action = m_selectedAction;
    m_hasAction = false;
    m_selectedAction = MenuAction::NONE;
    return action;
}

void MainMenu::renderMenuItem(int index, int y) {
    bool highlighted = (index == m_highlightIndex);
    int16_t w = Theme::SCREEN_WIDTH;
    int16_t h = 20;

    uint16_t bgColor = highlighted ? Theme::COLOR_SURFACE_RAISED : Theme::COLOR_BACKGROUND;
    m_canvas->fillRect(0, y, w, h, bgColor);

    if (highlighted) {
        m_canvas->fillRect(0, y, 3, h, Theme::COLOR_ACCENT);
    }

    m_canvas->setTextSize(1);
    m_canvas->setTextColor(highlighted ? Theme::COLOR_TEXT_PRIMARY : Theme::COLOR_TEXT_SECONDARY);
    m_canvas->setTextDatum(ML_DATUM);
    m_canvas->drawString(m_items[index].label, 12, y + h / 2);
}

} // namespace Vanguard
