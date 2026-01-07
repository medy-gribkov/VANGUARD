/**
 * @file MainMenu.cpp
 * @brief Global menu accessible from any screen
 */

#include "MainMenu.h"
#include <M5Cardputer.h>

namespace Assessor {

MainMenu::MainMenu()
    : m_visible(false)
    , m_highlightIndex(0)
    , m_hasAction(false)
    , m_selectedAction(MenuAction::NONE)
    , m_needsRedraw(true)
    , m_canvas(nullptr)
    , m_lastRenderMs(0)
{
    // Create sprite for overlay
    m_canvas = new M5Canvas(&M5Cardputer.Display);
    m_canvas->createSprite(160, 100);

    // Setup menu items
    m_items.push_back({"Rescan WiFi", MenuAction::RESCAN});
    m_items.push_back({"Scan BLE", MenuAction::RESCAN_BLE});
    m_items.push_back({"Settings", MenuAction::SETTINGS});
    m_items.push_back({"About", MenuAction::ABOUT});
    m_items.push_back({"Close Menu", MenuAction::BACK});
}

MainMenu::~MainMenu() {
    if (m_canvas) {
        m_canvas->deleteSprite();
        delete m_canvas;
        m_canvas = nullptr;
    }
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

    // Draw menu background with border
    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);
    m_canvas->drawRect(0, 0, 160, 100, Theme::COLOR_ACCENT);

    // Header with VELORA branding
    m_canvas->fillRect(1, 1, 158, 16, Theme::COLOR_SURFACE);
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->setTextDatum(MC_DATUM);
    m_canvas->drawString("VELORA MENU", 80, 9);

    // Menu items
    int16_t y = 20;
    for (size_t i = 0; i < m_items.size(); i++) {
        bool highlighted = ((int)i == m_highlightIndex);

        if (highlighted) {
            m_canvas->fillRect(2, y, 156, 18, Theme::COLOR_SURFACE_RAISED);
            m_canvas->fillRect(2, y, 3, 18, Theme::COLOR_ACCENT);
        }

        m_canvas->setTextColor(highlighted ? Theme::COLOR_TEXT_PRIMARY : Theme::COLOR_TEXT_SECONDARY);
        m_canvas->setTextDatum(TL_DATUM);
        m_canvas->drawString(m_items[i].label, 10, y + 4);

        y += 19;
    }

    // Push sprite to center of screen
    int16_t x = (Theme::SCREEN_WIDTH - 160) / 2;
    int16_t yPos = (Theme::SCREEN_HEIGHT - 100) / 2;
    m_canvas->pushSprite(x, yPos);
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

} // namespace Assessor
