/**
 * @file SpectrumView.cpp
 * @brief Spectrum Analyzer visualization
 */

#include "SpectrumView.h"
#include "../adapters/BruceWiFi.h"
#include "../ui/Theme.h"

namespace Vanguard {

SpectrumView::SpectrumView()
    : m_visible(false)
    , m_wantsBack(false)
    , m_canvas(&CanvasManager::getInstance().getCanvas())
    , m_lastRenderMs(0)
    , m_lastHopMs(0)
    , m_currentChannel(1)
    , m_paused(false)
    , m_currentPackets(0)
    , m_currentMaxRssi(-100)
{
    // Initialize stats
    for (int i = 0; i <= CHANNELS; i++) {
        m_channels[i].packetCount = 0;
        m_channels[i].peakRssi = -100;
        m_channels[i].samples = 0;
        for (int h = 0; h < 10; h++) m_channels[i].history[h] = 0;
    }
}

SpectrumView::~SpectrumView() {
    // Canvas is shared
}

void SpectrumView::show() {
    m_visible = true;
    m_wantsBack = false;
    m_paused = false;
    m_currentChannel = 1;
    m_lastHopMs = millis();
    
    m_currentPackets = 0;
    m_currentMaxRssi = -100;
    
    // Start Monitoring on Channel 1
    BruceWiFi::getInstance().startMonitor(1);
    
    // Hook callback
    BruceWiFi::getInstance().onPacketReceived([this](const uint8_t* payload, uint16_t len, int8_t rssi) {
        if (!m_visible || m_paused) return;
        m_currentPackets++;
        if (rssi > m_currentMaxRssi) m_currentMaxRssi = rssi;
    });
}

void SpectrumView::hide() {
    m_visible = false;
    // Release callback
    BruceWiFi::getInstance().onPacketReceived(nullptr);
    // Stop monitoring
    BruceWiFi::getInstance().stopMonitor();
}

void SpectrumView::handleInput() {
    if (M5Cardputer.Keyboard.isPressed()) {
        if (M5Cardputer.Keyboard.isKeyPressed('q') || M5Cardputer.Keyboard.isKeyPressed('Q')) {
            m_wantsBack = true;
        }
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER)) {
            m_paused = !m_paused;
        }
    }
}

void SpectrumView::tick() {
    if (!m_visible || m_paused) return;
    
    uint32_t now = millis();
    if (now - m_lastHopMs >= 150) { // 150ms dwell time
        // Save current channel data
        ChannelStats& stats = m_channels[m_currentChannel];
        stats.packetCount = m_currentPackets;
        stats.peakRssi = m_currentMaxRssi;
        
        // Push histogram
        for (int i = 9; i > 0; i--) {
            stats.history[i] = stats.history[i-1];
        }
        stats.history[0] = m_currentPackets;
        
        // Hop
        m_currentChannel++;
        if (m_currentChannel > CHANNELS) m_currentChannel = 1;
        
        BruceWiFi::getInstance().setChannel(m_currentChannel);
        
        // Reset counters
        m_currentPackets = 0;
        m_currentMaxRssi = -100;
        m_lastHopMs = now;
    }
}

void SpectrumView::render() {
    if (!m_visible || !m_canvas) return;
    
    // FPS Limit
    uint32_t now = millis();
    if (now - m_lastRenderMs < 50) return;
    m_lastRenderMs = now;
    
    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);
    
    drawGrid();
    drawBars();
    drawInfo();
    
    m_canvas->pushSprite(0, 0);
}

void SpectrumView::drawGrid() {
    // Bottom axis (raised to make room for key hints)
    int axisY = Theme::SCREEN_HEIGHT - 26;
    m_canvas->drawFastHLine(0, axisY, Theme::SCREEN_WIDTH, Theme::COLOR_TEXT_MUTED);

    // Channel number labels
    int barW = (Theme::SCREEN_WIDTH - 20) / CHANNELS;
    int startX = 10;

    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(TC_DATUM);
    m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);

    for (int i = 1; i <= CHANNELS; i++) {
        int x = startX + (i - 1) * barW + barW/2;
        m_canvas->drawFastVLine(x, axisY, 3, Theme::COLOR_TEXT_MUTED);
        m_canvas->drawNumber(i, x, axisY + 4);
    }
}

void SpectrumView::drawBars() {
    int barW = (Theme::SCREEN_WIDTH - 20) / CHANNELS;
    int startX = 10;
    int bottomY = Theme::SCREEN_HEIGHT - 26;
    int maxH = Theme::SCREEN_HEIGHT - 50;
    
    for (int i = 1; i <= CHANNELS; i++) {
        int x = startX + (i - 1) * barW + 1;
        int count = m_channels[i].packetCount;
        
        // Scale: 0-50 packets -> 0-maxH
        int h = map(count, 0, 50, 0, maxH);
        if (h > maxH) h = maxH;
        if (h < 0) h = 0;
        
        uint16_t color = Theme::COLOR_ACCENT;
        if (count > 20) color = Theme::COLOR_WARNING;
        if (count > 40) color = Theme::COLOR_DANGER;
        
        // Active channel highlight
        if (i == m_currentChannel) {
            m_canvas->drawRect(x-1, bottomY - maxH - 2, barW, maxH + 4, Theme::COLOR_TEXT_PRIMARY);
        }
        
        if (h > 0) {
            m_canvas->fillRect(x, bottomY - h, barW - 2, h, color);
        }
        
        // Peak RSSI Marker
        int peak = m_channels[i].peakRssi;
        if (peak > -90) {
            // Map RSSI -90 to -30 -> 0 to maxH
            int ry = map(peak, -90, -30, bottomY, bottomY - maxH);
            ry = constrain(ry, bottomY - maxH, bottomY);
            m_canvas->drawFastHLine(x, ry, barW - 2, Theme::COLOR_SUCCESS);
        }
    }
}

void SpectrumView::drawInfo() {
    // Title
    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString("2.4GHz Channel Activity", 4, 4);

    // Current channel (prominent) or PAUSED
    m_canvas->setTextDatum(TR_DATUM);
    if (m_paused) {
        m_canvas->setTextColor(Theme::COLOR_WARNING);
        m_canvas->drawString("PAUSED", Theme::SCREEN_WIDTH - 4, 4);
    } else {
        char buf[20];
        snprintf(buf, sizeof(buf), "CH %d", m_currentChannel);
        m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
        m_canvas->drawString(buf, Theme::SCREEN_WIDTH - 4, 4);
    }

    // Y-axis label
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString("Packets/s", 4, 14);

    // Key hints at bottom
    m_canvas->setTextDatum(BC_DATUM);
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString("[Enter] Pause  [Q] Exit", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT - 1);
}

} // namespace Vanguard
