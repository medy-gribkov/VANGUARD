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
    m_items.push_back({"Rescan WiFi", MenuAction::RESCAN});
    m_items.push_back({"Scan BLE", MenuAction::RESCAN_BLE});
    m_items.push_back({"Settings", MenuAction::SETTINGS});
    m_items.push_back({"About", MenuAction::ABOUT});
    m_items.push_back({"Close Menu", MenuAction::BACK});
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

    // Draw menu background with border
    // Centered box: 160x100 on 240x135 screen
    int16_t boxW = 160;
    int16_t boxH = 100;
    int16_t boxX = (Theme::SCREEN_WIDTH - boxW) / 2;
    int16_t boxY = (Theme::SCREEN_HEIGHT - boxH) / 2;

    // We don't fillScreen since this is an overlay, but in shared model we must 
    // be careful. However, since we push the WHOLE sprite to the display, 
    // we should probably capture the background first? 
    // NO: The current architecture seems to assume each screen renders FULLY.
    // If MainMenu is an overlay, it needs to know what's underneath.
    // BUT! Looking at the original code, it pushed a 160x100 sprite to a specific (x,y).
    // In our shared model, we MUST fill the background if we push 0,0.
    
    // For now, let's keep it as an overlay by only pushing the changed area if possible,
    // OR we fillScreen with a darkened version of "nothing" for now.
    // Actually, the best way for an overlay is to NOT use the shared canvas if it needs to preserve background.
    // BUT the goal is saving RAM. 
    
    // DECISION: MainMenu will fillScreen with a translucent-looking dark color or just black for now.
    m_canvas->fillScreen(Theme::COLOR_BACKGROUND); 
    
    m_canvas->drawRect(boxX, boxY, boxW, boxH, Theme::COLOR_ACCENT);

    // Header with Vanguard branding
    m_canvas->fillRect(boxX + 1, boxY + 1, boxW - 2, 16, Theme::COLOR_SURFACE);
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->setTextDatum(TC_DATUM);
    m_canvas->drawString("VANGUARD", Theme::SCREEN_WIDTH / 2, boxY + 6);

    // Menu items
    int16_t itemY = boxY + 20;
    for (size_t i = 0; i < m_items.size(); i++) {
        bool highlighted = ((int)i == m_highlightIndex);

        if (highlighted) {
            m_canvas->fillRect(boxX + 2, itemY, boxW - 4, 18, Theme::COLOR_SURFACE_RAISED);
            m_canvas->fillRect(boxX + 2, itemY, 3, 18, Theme::COLOR_ACCENT);
        }

        m_canvas->setTextColor(highlighted ? Theme::COLOR_TEXT_PRIMARY : Theme::COLOR_TEXT_SECONDARY);
        m_canvas->setTextDatum(TL_DATUM);
        m_canvas->drawString(m_items[i].label, boxX + 10, itemY + 4);

        itemY += 19;
    }

    // Push full sprite
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

} // namespace Vanguard
