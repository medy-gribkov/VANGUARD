/**
 * @file TargetRadar.cpp
 * @brief Main target list view - the heart of the UI
 */

#include "TargetRadar.h"

namespace Assessor {

TargetRadar::TargetRadar(AssessorEngine& engine)
    : m_engine(engine)
    , m_highlightIndex(0)
    , m_scrollOffset(0)
    , m_hasSelection(false)
    , m_selectedTarget(nullptr)
    , m_sortOrder(SortOrder::SIGNAL_STRENGTH)
    , m_autoRefresh(true)
    , m_lastRefreshMs(0)
{
    // Default filter: show everything
    m_filter.showAccessPoints = true;
    m_filter.showStations = true;
    m_filter.showBLE = true;
    m_filter.showHidden = true;
    m_filter.showOpen = true;
    m_filter.showSecured = true;
    m_filter.minRssi = -100;
}

// =============================================================================
// LIFECYCLE
// =============================================================================

void TargetRadar::tick() {
    handleInput();

    // Auto-refresh target list
    if (m_autoRefresh) {
        uint32_t now = millis();
        if (now - m_lastRefreshMs > REFRESH_INTERVAL_MS) {
            updateTargetList();
            m_lastRefreshMs = now;
        }
    }
}

void TargetRadar::render() {
    M5.Display.fillScreen(Theme::COLOR_BACKGROUND);
    renderHeader();

    if (m_targets.empty()) {
        renderEmptyState();
    } else {
        renderTargetList();
        renderScrollIndicator();
    }
}

void TargetRadar::renderScanning() {
    M5.Display.fillScreen(Theme::COLOR_BACKGROUND);
    renderHeader();

    // Progress indicator
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(Theme::COLOR_TEXT_PRIMARY);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString("SCANNING...", centerX, centerY - 10);

    // Animated dots
    static uint8_t dotPhase = 0;
    dotPhase = (dotPhase + 1) % 4;

    String dots = "";
    for (int i = 0; i < dotPhase; i++) {
        dots += ".";
    }
    M5.Display.drawString(dots, centerX, centerY + 10);

    // Target count so far
    size_t count = m_engine.getTargetCount();
    if (count > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Found: %d", (int)count);
        M5.Display.setTextColor(Theme::COLOR_ACCENT);
        M5.Display.drawString(buf, centerX, centerY + 30);
    }
}

// =============================================================================
// SELECTION
// =============================================================================

bool TargetRadar::hasSelection() const {
    return m_hasSelection;
}

const Target* TargetRadar::getSelectedTarget() const {
    return m_selectedTarget;
}

void TargetRadar::clearSelection() {
    m_hasSelection = false;
    m_selectedTarget = nullptr;
}

int TargetRadar::getHighlightedIndex() const {
    return m_highlightIndex;
}

// =============================================================================
// NAVIGATION
// =============================================================================

void TargetRadar::navigateUp() {
    if (m_highlightIndex > 0) {
        m_highlightIndex--;
        ensureHighlightVisible();
    }
}

void TargetRadar::navigateDown() {
    if (m_highlightIndex < (int)m_targets.size() - 1) {
        m_highlightIndex++;
        ensureHighlightVisible();
    }
}

void TargetRadar::select() {
    if (m_highlightIndex >= 0 && m_highlightIndex < (int)m_targets.size()) {
        m_selectedTarget = &m_targets[m_highlightIndex];
        m_hasSelection = true;
    }
}

void TargetRadar::scrollToTop() {
    m_highlightIndex = 0;
    m_scrollOffset = 0;
}

// =============================================================================
// FILTERING
// =============================================================================

void TargetRadar::setFilter(const TargetFilter& filter) {
    m_filter = filter;
    updateTargetList();
}

const TargetFilter& TargetRadar::getFilter() const {
    return m_filter;
}

void TargetRadar::setSortOrder(SortOrder order) {
    m_sortOrder = order;
    updateTargetList();
}

// =============================================================================
// REFRESH
// =============================================================================

void TargetRadar::refresh() {
    updateTargetList();
}

void TargetRadar::setAutoRefresh(bool enabled) {
    m_autoRefresh = enabled;
}

// =============================================================================
// RENDERING - PRIVATE
// =============================================================================

void TargetRadar::renderHeader() {
    // Header bar
    M5.Display.fillRect(0, 0, Theme::SCREEN_WIDTH, HEADER_HEIGHT,
                        Theme::COLOR_SURFACE);

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(Theme::COLOR_TEXT_PRIMARY);
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.drawString("TARGET RADAR", 4, 3);

    // Target count on right
    char countStr[16];
    snprintf(countStr, sizeof(countStr), "%d", (int)m_targets.size());
    M5.Display.setTextDatum(TR_DATUM);
    M5.Display.setTextColor(Theme::COLOR_ACCENT);
    M5.Display.drawString(countStr, Theme::SCREEN_WIDTH - 4, 3);
}

void TargetRadar::renderTargetList() {
    int16_t y = HEADER_HEIGHT + 2;

    for (int i = 0; i < VISIBLE_ITEMS && (m_scrollOffset + i) < (int)m_targets.size(); i++) {
        int idx = m_scrollOffset + i;
        bool highlighted = (idx == m_highlightIndex);
        renderTargetItem(m_targets[idx], y, highlighted);
        y += ITEM_HEIGHT;
    }
}

void TargetRadar::renderTargetItem(const Target& target, int y, bool highlighted) {
    int16_t x = 0;
    int16_t w = Theme::SCREEN_WIDTH;
    int16_t h = ITEM_HEIGHT;

    // Background
    uint16_t bgColor = highlighted ? Theme::COLOR_SURFACE_RAISED : Theme::COLOR_BACKGROUND;
    M5.Display.fillRect(x, y, w, h, bgColor);

    if (highlighted) {
        // Selection indicator
        M5.Display.fillRect(x, y, 3, h, Theme::COLOR_ACCENT);
    }

    // Signal indicator
    drawSignalIndicator(x + 8, y + h/2, target.rssi);

    // SSID
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(Theme::COLOR_TEXT_PRIMARY, bgColor);
    M5.Display.setTextDatum(TL_DATUM);

    char ssidDisplay[24];
    strncpy(ssidDisplay, target.ssid, 20);
    ssidDisplay[20] = '\0';
    if (strlen(target.ssid) > 20) {
        strcat(ssidDisplay, "...");
    }
    M5.Display.drawString(ssidDisplay, x + 22, y + 3);

    // RSSI value
    char rssiStr[16];
    snprintf(rssiStr, sizeof(rssiStr), "%ddB", target.rssi);
    M5.Display.setTextColor(Theme::getSignalColor(target.rssi), bgColor);
    M5.Display.setTextDatum(TR_DATUM);
    M5.Display.drawString(rssiStr, w - 4, y + 3);

    // Security badge (second line)
    drawSecurityBadge(x + 22, y + 14, target.security);

    // Client count if AP with clients
    if (target.type == TargetType::ACCESS_POINT && target.clientCount > 0) {
        drawClientCount(w - 30, y + 14, target.clientCount);
    }
}

void TargetRadar::renderEmptyState() {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(Theme::COLOR_TEXT_MUTED);
    M5.Display.setTextDatum(MC_DATUM);
    M5.Display.drawString("No targets found", centerX, centerY);
    M5.Display.drawString("Press [R] to rescan", centerX, centerY + 16);
}

void TargetRadar::renderScrollIndicator() {
    if (m_targets.size() <= VISIBLE_ITEMS) return;

    // Calculate scroll bar
    int16_t barX = Theme::SCREEN_WIDTH - 3;
    int16_t barY = HEADER_HEIGHT + 2;
    int16_t barH = Theme::SCREEN_HEIGHT - HEADER_HEIGHT - 4;

    float viewRatio = (float)VISIBLE_ITEMS / m_targets.size();
    float posRatio = (float)m_scrollOffset / (m_targets.size() - VISIBLE_ITEMS);

    int16_t thumbH = max(10, (int)(barH * viewRatio));
    int16_t thumbY = barY + (int)((barH - thumbH) * posRatio);

    // Track
    M5.Display.fillRect(barX, barY, 2, barH, Theme::COLOR_SURFACE);
    // Thumb
    M5.Display.fillRect(barX, thumbY, 2, thumbH, Theme::COLOR_ACCENT);
}

// =============================================================================
// DRAWING HELPERS
// =============================================================================

void TargetRadar::drawSignalIndicator(int x, int y, int8_t rssi) {
    // Signal strength dot
    uint16_t color = Theme::getSignalColor(rssi);
    int radius;

    if (rssi > RSSI_EXCELLENT) {
        radius = 5;  // Full dot
    } else if (rssi > RSSI_GOOD) {
        radius = 4;
    } else if (rssi > RSSI_FAIR) {
        radius = 3;
    } else {
        radius = 2;  // Tiny dot
    }

    M5.Display.fillCircle(x, y, radius, color);
}

void TargetRadar::drawSecurityBadge(int x, int y, SecurityType security) {
    const char* label;
    uint16_t color;

    switch (security) {
        case SecurityType::OPEN:
            label = "OPEN";
            color = Theme::COLOR_SECURITY_OPEN;
            break;
        case SecurityType::WEP:
            label = "WEP";
            color = Theme::COLOR_SECURITY_WEP;
            break;
        case SecurityType::WPA_PSK:
            label = "WPA";
            color = Theme::COLOR_SECURITY_WPA;
            break;
        case SecurityType::WPA2_PSK:
        case SecurityType::WPA2_ENTERPRISE:
            label = "WPA2";
            color = Theme::COLOR_SECURITY_WPA2;
            break;
        case SecurityType::WPA3_SAE:
            label = "WPA3";
            color = Theme::COLOR_SECURITY_WPA3;
            break;
        default:
            label = "???";
            color = Theme::COLOR_TEXT_MUTED;
    }

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(color, Theme::COLOR_BACKGROUND);
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.drawString(label, x, y);
}

void TargetRadar::drawClientCount(int x, int y, uint8_t count) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%dc", count);

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(Theme::COLOR_TYPE_STATION, Theme::COLOR_BACKGROUND);
    M5.Display.setTextDatum(TR_DATUM);
    M5.Display.drawString(buf, x, y);
}

// =============================================================================
// INPUT HANDLING
// =============================================================================

void TargetRadar::handleInput() {
    // Button-based navigation (works on all M5Stack devices)
    // Cardputer: Use G key (BtnPWR) for select, or physical buttons
    if (M5.BtnA.wasPressed()) {
        navigateUp();
    }
    if (M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) {
        select();
    }
    if (M5.BtnC.wasPressed()) {
        navigateDown();
    }
}

// =============================================================================
// LIST MANAGEMENT
// =============================================================================

void TargetRadar::updateTargetList() {
    m_targets = m_engine.getFilteredTargets(m_filter, m_sortOrder);

    // Clamp highlight index
    if (m_highlightIndex >= (int)m_targets.size()) {
        m_highlightIndex = max(0, (int)m_targets.size() - 1);
    }

    ensureHighlightVisible();
}

void TargetRadar::ensureHighlightVisible() {
    if (m_highlightIndex < m_scrollOffset) {
        m_scrollOffset = m_highlightIndex;
    } else if (m_highlightIndex >= m_scrollOffset + VISIBLE_ITEMS) {
        m_scrollOffset = m_highlightIndex - VISIBLE_ITEMS + 1;
    }
}

} // namespace Assessor
