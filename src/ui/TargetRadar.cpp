/**
 * @file TargetRadar.cpp
 * @brief Main target list view - the heart of the UI
 *
 * Uses sprite-based double buffering to eliminate flickering.
 */

#include "TargetRadar.h"
#include <M5Cardputer.h>
#include "FeedbackManager.h"

namespace Vanguard {

TargetRadar::TargetRadar(VanguardEngine& engine)
    : m_engine(engine)
    , m_highlightIndex(0)
    , m_scrollOffset(0)
    , m_hasSelection(false)
    , m_selectedTarget()
    , m_sortOrder(SortOrder::SIGNAL_STRENGTH)
    , m_autoRefresh(true)
    , m_lastRefreshMs(0)
    , m_lastRenderMs(0)
    , m_needsRedraw(true)
    , m_show5GHzWarning(false)
    , m_pending5GHzTarget()
    , m_canvas(&CanvasManager::getInstance().getCanvas())
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

TargetRadar::~TargetRadar() {
    // m_canvas is shared, do not delete
}

// =============================================================================
// LIFECYCLE
// =============================================================================

void TargetRadar::tick() {
    // Auto-refresh target list
    if (m_autoRefresh) {
        uint32_t now = millis();
        if (now - m_lastRefreshMs > REFRESH_INTERVAL_MS) {
            updateTargetList();
            m_lastRefreshMs = now;
            m_needsRedraw = true;
        }
    }

    // Update Geiger Counter based on currently highlighted target
    if (m_highlightIndex >= 0 && m_highlightIndex < (int)m_targets.size()) {
        FeedbackManager::getInstance().updateGeiger(m_targets[m_highlightIndex].rssi);
    }
    
    // Check WIDS Alert
    const auto& alert = m_engine.getWidsAlert();
    if (alert.active) {
         m_needsRedraw = true;
    }
}

void TargetRadar::render() {
    // Frame rate limiting to reduce flickering
    uint32_t now = millis();
    if (!m_needsRedraw && (now - m_lastRenderMs) < RENDER_INTERVAL_MS) {
        return;  // Skip this frame
    }
    m_lastRenderMs = now;
    m_needsRedraw = false;

    // Draw everything to sprite first (off-screen)
    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);

    // Accent lines
    m_canvas->drawFastHLine(0, 0, Theme::SCREEN_WIDTH, Theme::COLOR_ACCENT);
    m_canvas->drawFastHLine(0, 1, Theme::SCREEN_WIDTH, Theme::COLOR_ACCENT_DIM);

    // Header
    m_canvas->fillRect(0, 2, Theme::SCREEN_WIDTH, HEADER_HEIGHT, Theme::COLOR_SURFACE);
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->drawString("VANGUARD RADAR", 4, 5);

    // Count WiFi and BLE targets
    int wifiCount = 0, bleCount = 0;
    for (const auto& t : m_targets) {
        if (t.type == TargetType::BLE_DEVICE) bleCount++;
        else wifiCount++;
    }

    // Target count on right (WiFi + BLE breakdown)
    char countStr[24];
    if (bleCount > 0) {
        snprintf(countStr, sizeof(countStr), "%dW %dB", wifiCount, bleCount);
    } else {
        snprintf(countStr, sizeof(countStr), "%d", wifiCount);
    }
    m_canvas->setTextDatum(TR_DATUM);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString(countStr, Theme::SCREEN_WIDTH - 4, 5);

    // Battery indicator
    int batLevel = M5Cardputer.Power.getBatteryLevel();
    if (batLevel >= 0) {
        char batStr[12];
        snprintf(batStr, sizeof(batStr), "%d%%", batLevel);
        m_canvas->setTextDatum(TC_DATUM);
        m_canvas->setTextColor(batLevel < 20 ? Theme::COLOR_DANGER : Theme::COLOR_TEXT_MUTED);
        m_canvas->drawString(batStr, Theme::SCREEN_WIDTH / 2, 5);
    }

    if (m_targets.empty()) {
        // Empty state
        int16_t centerX = Theme::SCREEN_WIDTH / 2;
        int16_t centerY = Theme::SCREEN_HEIGHT / 2;

        m_canvas->setTextSize(1);
        m_canvas->setTextColor(Theme::COLOR_WARNING);
        m_canvas->setTextDatum(MC_DATUM);
        m_canvas->drawString("No targets found", centerX, centerY - 20);

        m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
        m_canvas->drawString("Start a scan to discover targets", centerX, centerY);

        m_canvas->setTextColor(Theme::COLOR_ACCENT);
        m_canvas->drawString("[R] Rescan", centerX, centerY + 20);
        m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
        m_canvas->drawString("[Q] Quit", centerX, centerY + 35);
        m_canvas->drawString("[M] Menu", centerX, centerY + 50);
    } else {
        // Target list
        int16_t y = HEADER_HEIGHT + 2;
        for (int i = 0; i < VISIBLE_ITEMS && (m_scrollOffset + i) < (int)m_targets.size(); i++) {
            int idx = m_scrollOffset + i;
            bool highlighted = (idx == m_highlightIndex);
            renderTargetItemToCanvas(m_targets[idx], y, highlighted);
            y += ITEM_HEIGHT;
        }

        // Scroll indicator
        if (m_targets.size() > VISIBLE_ITEMS) {
            int16_t barX = Theme::SCREEN_WIDTH - 3;
            int16_t barY = HEADER_HEIGHT + 2;
            int16_t barH = Theme::SCREEN_HEIGHT - HEADER_HEIGHT - 4;

            float viewRatio = (float)VISIBLE_ITEMS / m_targets.size();
            float posRatio = (float)m_scrollOffset / (m_targets.size() - VISIBLE_ITEMS);

            int16_t thumbH = max(10, (int)(barH * viewRatio));
            int16_t thumbY = barY + (int)((barH - thumbH) * posRatio);

            m_canvas->fillRect(barX, barY, 2, barH, Theme::COLOR_SURFACE);
            m_canvas->fillRect(barX, thumbY, 2, thumbH, Theme::COLOR_ACCENT);
        }
    }

    // Footer hint
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->setTextDatum(BC_DATUM);
    m_canvas->drawString("[;,.] Nav  [Enter] Sel  [Q] Back  [M] Menu", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT - 2);

    // Render 5GHz warning popup if active (on top of everything)
    if (m_show5GHzWarning) {
        render5GHzWarning();
    }

    // WIDS Overlay
    const auto& alert = m_engine.getWidsAlert();
    if (alert.active) {
        if (millis() - alert.timestamp > 5000) {
             m_engine.clearWidsAlert();
        } else {
             renderWidsAlert(alert);
        }
    }

    // Push sprite to display in one operation (no flicker!)
    m_canvas->pushSprite(0, 0);
}

void TargetRadar::renderScanning() {
    // Frame rate limiting
    uint32_t now = millis();
    if ((now - m_lastRenderMs) < RENDER_INTERVAL_MS) {
        return;
    }
    m_lastRenderMs = now;

    // Draw to sprite
    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);

    // Accent lines
    m_canvas->drawFastHLine(0, 0, Theme::SCREEN_WIDTH, Theme::COLOR_ACCENT);
    m_canvas->drawFastHLine(0, 1, Theme::SCREEN_WIDTH, Theme::COLOR_ACCENT_DIM);

    // Header
    m_canvas->fillRect(0, 2, Theme::SCREEN_WIDTH, HEADER_HEIGHT, Theme::COLOR_SURFACE);
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->drawString("SCANNING", 4, 5);

    // Show progress and elapsed time
    uint8_t progress = m_engine.getScanProgress();
    uint32_t elapsedSec = m_engine.getScanElapsedMs() / 1000;
    char progStr[16];
    snprintf(progStr, sizeof(progStr), "%ds %d%%", elapsedSec, progress);
    m_canvas->setTextDatum(TR_DATUM);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString(progStr, Theme::SCREEN_WIDTH - 4, 5);

    // Progress indicator
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    m_canvas->setTextSize(2);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->setTextDatum(MC_DATUM);

    // Animated scanning text with fixed-width dots
    static uint8_t dotPhase = 0;
    dotPhase = (dotPhase + 1) % 4;

    char scanText[16] = "SCANNING";
    for (int i = 0; i < dotPhase; i++) {
        strcat(scanText, ".");
    }
    // Pad with spaces to prevent text shifting
    for (int i = dotPhase; i < 3; i++) {
        strcat(scanText, " ");
    }
    m_canvas->drawString(scanText, centerX, centerY - 16);

    // Progress bar
    int16_t barW = 160;
    int16_t barH = 8;
    int16_t barX = (Theme::SCREEN_WIDTH - barW) / 2;
    int16_t barY = centerY + 8;
    m_canvas->drawRect(barX, barY, barW, barH, Theme::COLOR_SURFACE_RAISED);
    int16_t fillW = (barW - 2) * progress / 100;
    m_canvas->fillRect(barX + 1, barY + 1, fillW, barH - 2, Theme::COLOR_ACCENT);

    // Hint - show what's being scanned
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    ScanState state = m_engine.getScanState();
    if (state == ScanState::WIFI_SCANNING) {
        m_canvas->drawString("Scanning WiFi channels...", centerX, centerY + 30);
    } else if (state == ScanState::BLE_SCANNING) {
        m_canvas->drawString("Scanning BLE devices...", centerX, centerY + 30);
    } else if (state == ScanState::RECON) {
        char reconStr[40];
        size_t targetCount = m_engine.getTargetCount();
        snprintf(reconStr, sizeof(reconStr), "Recon... discovering clients (%d APs)", (int)targetCount);
        m_canvas->drawString(reconStr, centerX, centerY + 30);
    } else {
        m_canvas->drawString("Scanning...", centerX, centerY + 30);
    }

    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->setTextDatum(BC_DATUM);
    m_canvas->drawString("[Q] Cancel", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT - 2);

    // Push to display
    m_canvas->pushSprite(0, 0);
}

// =============================================================================
// PRIVATE RENDERING TO CANVAS
// =============================================================================

void TargetRadar::renderTargetItemToCanvas(const Target& target, int y, bool highlighted) {
    int16_t x = 0;
    int16_t w = Theme::SCREEN_WIDTH - 6;  // Leave room for scroll bar
    int16_t h = ITEM_HEIGHT;

    // Background
    uint16_t bgColor = highlighted ? Theme::COLOR_SURFACE_RAISED : Theme::COLOR_BACKGROUND;
    m_canvas->fillRect(x, y, w, h, bgColor);

    if (highlighted) {
        // Selection indicator
        m_canvas->fillRect(x, y, 3, h, Theme::COLOR_ACCENT);
    }

    // Type icon (WiFi or BLE)
    bool isBLE = (target.type == TargetType::BLE_DEVICE || target.type == TargetType::BLE_SKIMMER);
    if (isBLE) {
        // BLE icon - simplified "B" shape
        uint16_t iconColor = (target.type == TargetType::BLE_SKIMMER) ? Theme::COLOR_DANGER : Theme::COLOR_TYPE_BLE;
        m_canvas->fillRect(x + 6, y + 4, 2, 17, iconColor);
        m_canvas->drawLine(x + 8, y + 4, x + 12, y + 8, iconColor);
        m_canvas->drawLine(x + 12, y + 8, x + 8, y + 12, iconColor);
        m_canvas->drawLine(x + 8, y + 12, x + 12, y + 16, iconColor);
        m_canvas->drawLine(x + 12, y + 16, x + 8, y + 20, iconColor);
    } else {
        // WiFi signal bars (4 bars)
        uint16_t signalColor = Theme::getSignalColor(target.rssi);
        int bars = (target.rssi > -50) ? 4 : (target.rssi > -60) ? 3 : (target.rssi > -70) ? 2 : 1;
        for (int i = 0; i < 4; i++) {
            uint16_t barColor = (i < bars) ? signalColor : Theme::COLOR_SURFACE;
            int barH = 5 + i * 3;  // Heights: 5, 8, 11, 14
            int barY = y + h - 3 - barH;
            m_canvas->fillRect(x + 6 + i * 4, barY, 3, barH, barColor);
        }
    }

    // SSID (truncate if too long)
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY, bgColor);
    m_canvas->setTextDatum(TL_DATUM);

    char ssidDisplay[24];
    int maxLen = isBLE ? 16 : 18;  // BLE names can be longer
    strncpy(ssidDisplay, target.ssid, maxLen);
    ssidDisplay[maxLen] = '\0';
    if (strlen(target.ssid) > (size_t)maxLen) {
        ssidDisplay[maxLen - 2] = '.';
        ssidDisplay[maxLen - 1] = '.';
    }
    m_canvas->drawString(ssidDisplay, x + 24, y + 3);

    // RSSI value (top right)
    char rssiStr[16];
    snprintf(rssiStr, sizeof(rssiStr), "%d dBm", target.rssi);
    uint16_t signalColor = Theme::getSignalColor(target.rssi);
    m_canvas->setTextColor(signalColor, bgColor);
    m_canvas->setTextDatum(TR_DATUM);
    m_canvas->drawString(rssiStr, w - 4, y + 3);

    // Second line: Security/Type + Channel/Status
    if (isBLE) {
        // BLE device - show type indicator
        if (target.type == TargetType::BLE_SKIMMER) {
            m_canvas->setTextColor(Theme::COLOR_DANGER, bgColor);
            m_canvas->setTextDatum(TL_DATUM);
            m_canvas->drawString("SKIMMER", x + 24, y + 14);
        } else {
            m_canvas->setTextColor(Theme::COLOR_TYPE_BLE, bgColor);
            m_canvas->setTextDatum(TL_DATUM);
            m_canvas->drawString("BLE", x + 24, y + 14);
        }

        // Show connectable status
        m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED, bgColor);
        m_canvas->drawString("Device", x + 50, y + 14);
    } else if (target.type == TargetType::IR_DEVICE) {
        // IR device - show type indicator
        m_canvas->setTextColor(Theme::COLOR_WARNING, bgColor); // Use warning color for IR
        m_canvas->setTextDatum(TL_DATUM);
        m_canvas->drawString("IR", x + 24, y + 14);

        m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED, bgColor);
        m_canvas->drawString("Remote", x + 44, y + 14);
    } else {
        // WiFi AP - show security
        const char* secLabel;
        uint16_t secColor;
        switch (target.security) {
            case SecurityType::OPEN:
                secLabel = "OPEN";
                secColor = Theme::COLOR_SECURITY_OPEN;
                break;
            case SecurityType::WEP:
                secLabel = "WEP";
                secColor = Theme::COLOR_SECURITY_WEP;
                break;
            case SecurityType::WPA_PSK:
                secLabel = "WPA";
                secColor = Theme::COLOR_SECURITY_WPA;
                break;
            case SecurityType::WPA2_PSK:
            case SecurityType::WPA2_ENTERPRISE:
                secLabel = "WPA2";
                secColor = Theme::COLOR_SECURITY_WPA2;
                break;
            case SecurityType::WPA3_SAE:
                secLabel = "WPA3";
                secColor = Theme::COLOR_SECURITY_WPA3;
                break;
            default:
                secLabel = "???";
                secColor = Theme::COLOR_TEXT_MUTED;
        }
        m_canvas->setTextColor(secColor, bgColor);
        m_canvas->setTextDatum(TL_DATUM);
        m_canvas->drawString(secLabel, x + 24, y + 14);

        // Channel - mark 5GHz (channel > 14) in warning color
        char chanStr[12];
        bool is5GHz = target.channel > 14;
        if (is5GHz) {
            snprintf(chanStr, sizeof(chanStr), "5G CH%d", target.channel);
            m_canvas->setTextColor(Theme::COLOR_WARNING, bgColor);
        } else {
            snprintf(chanStr, sizeof(chanStr), "CH%d", target.channel);
            m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED, bgColor);
        }
        m_canvas->drawString(chanStr, x + 64, y + 14);

        // Client count
        if (target.clientCount > 0) {
            char cliStr[8];
            snprintf(cliStr, sizeof(cliStr), "C:%d", target.clientCount);
            m_canvas->setTextColor(Theme::COLOR_ACCENT, bgColor);
            m_canvas->drawString(cliStr, x + 110, y + 14);
        }
    }
}

// =============================================================================
// SELECTION
// =============================================================================

bool TargetRadar::hasSelection() const {
    return m_hasSelection;
}

const Target* TargetRadar::getSelectedTarget() const {
    return m_hasSelection ? &m_selectedTarget : nullptr;
}

void TargetRadar::clearSelection() {
    m_hasSelection = false;
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
        m_needsRedraw = true;
    }
}

void TargetRadar::navigateDown() {
    if (m_highlightIndex < (int)m_targets.size() - 1) {
        m_highlightIndex++;
        ensureHighlightVisible();
        m_needsRedraw = true;
    }
}

void TargetRadar::select() {
    if (m_highlightIndex >= 0 && m_highlightIndex < (int)m_targets.size()) {
        const Target& target = m_targets[m_highlightIndex];

        // Check if this is a 5GHz WiFi network (channel > 14)
        // BLE devices are fine, only WiFi APs on 5GHz have limitations
        bool is5GHzWiFi = (target.type == TargetType::ACCESS_POINT && target.channel > 14);

        if (is5GHzWiFi) {
            // Show warning popup instead of direct selection
            m_pending5GHzTarget = target;
            m_show5GHzWarning = true;
            m_needsRedraw = true;
        } else {
            // Normal selection
            m_selectedTarget = target;
            m_hasSelection = true;
        }
    }
}

bool TargetRadar::isShowingWarning() const {
    return m_show5GHzWarning;
}

void TargetRadar::confirmWarning() {
    if (m_show5GHzWarning) {
        // User confirmed they want to view 5GHz network info
        m_selectedTarget = m_pending5GHzTarget;
        m_hasSelection = true;
        m_show5GHzWarning = false;
        m_needsRedraw = true;
    }
}

void TargetRadar::cancelWarning() {
    if (m_show5GHzWarning) {
        // User cancelled - go back to radar
        m_show5GHzWarning = false;
        m_needsRedraw = true;
    }
}

void TargetRadar::scrollToTop() {
    m_highlightIndex = 0;
    m_scrollOffset = 0;
    m_needsRedraw = true;
}

// =============================================================================
// FILTERING
// =============================================================================

void TargetRadar::setFilter(const TargetFilter& filter) {
    m_filter = filter;
    updateTargetList();
    m_needsRedraw = true;
}

const TargetFilter& TargetRadar::getFilter() const {
    return m_filter;
}

void TargetRadar::setSortOrder(SortOrder order) {
    m_sortOrder = order;
    updateTargetList();
    m_needsRedraw = true;
}

// =============================================================================
// REFRESH
// =============================================================================

void TargetRadar::refresh() {
    updateTargetList();
    m_needsRedraw = true;
}

void TargetRadar::setAutoRefresh(bool enabled) {
    m_autoRefresh = enabled;
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

void TargetRadar::render5GHzWarning() {
    // Semi-transparent overlay effect (darken background)
    // Draw a filled rectangle with partial alpha effect
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    // Warning box dimensions
    int16_t boxW = 220;
    int16_t boxH = 90;
    int16_t boxX = (Theme::SCREEN_WIDTH - boxW) / 2;
    int16_t boxY = (Theme::SCREEN_HEIGHT - boxH) / 2;

    // Draw box background and border
    m_canvas->fillRoundRect(boxX, boxY, boxW, boxH, 4, Theme::COLOR_SURFACE);
    m_canvas->drawRoundRect(boxX, boxY, boxW, boxH, 4, Theme::COLOR_WARNING);

    // Title
    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(MC_DATUM);
    m_canvas->setTextColor(Theme::COLOR_WARNING);
    m_canvas->drawString("5GHz LIMITATION", centerX, centerY - 32);

    // Network name
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    char ssidStr[24];
    strncpy(ssidStr, m_pending5GHzTarget.ssid, 20);
    ssidStr[20] = '\0';
    m_canvas->drawString(ssidStr, centerX, centerY - 18);

    // Explanation (two lines)
    m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);
    m_canvas->drawString("ESP32-S3 operates on 2.4GHz only.", centerX, centerY - 2);
    m_canvas->drawString("View target info? No attacks available.", centerX, centerY + 10);

    // Action buttons
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString("[ENTER] View Info", centerX - 50, centerY + 30);
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString("[Q] Cancel", centerX + 60, centerY + 30);
}

void TargetRadar::renderWidsAlert(const WidsAlertState& alert) {
    int16_t w = 220;
    int16_t h = 80;
    int16_t x = (Theme::SCREEN_WIDTH - w) / 2;
    int16_t y = (Theme::SCREEN_HEIGHT - h) / 2;

    // Flashing Alert Box
    bool flash = (millis() / 250) % 2 == 0;
    uint16_t bgColor = flash ? Theme::COLOR_DANGER : Theme::COLOR_SURFACE;
    uint16_t fgColor = flash ? Theme::COLOR_TEXT_PRIMARY : Theme::COLOR_DANGER;

    m_canvas->fillRoundRect(x, y, w, h, 6, bgColor);
    m_canvas->drawRoundRect(x, y, w, h, 6, Theme::COLOR_TEXT_PRIMARY);

    m_canvas->setTextSize(2);
    m_canvas->setTextDatum(MC_DATUM);
    m_canvas->setTextColor(fgColor);

    const char* title = (alert.type == WidsEventType::DEAUTH_FLOOD) ? "DEAUTH ATTACK!" : "EAPOL FLOOD!";
    m_canvas->drawString(title, Theme::SCREEN_WIDTH/2, y + 25);

    m_canvas->setTextSize(1);
    char buf[32];
    snprintf(buf, sizeof(buf), "Intensity: %d pps", alert.count);
    m_canvas->drawString(buf, Theme::SCREEN_WIDTH/2, y + 50);

    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString("Auto-dismiss in 5s", Theme::SCREEN_WIDTH/2, y + 66);
}

} // namespace Vanguard
