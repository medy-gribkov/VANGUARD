#ifndef VANGUARD_SPECTRUM_VIEW_H
#define VANGUARD_SPECTRUM_VIEW_H

/**
 * @file SpectrumView.h
 * @brief Real-time 2.4GHz WiFi Spectrum Analyzer
 * 
 * Visualizes traffic density across all 14 channels.
 * Uses rapid channel hopping in monitor mode.
 */

#include <Arduino.h>
#include <M5Cardputer.h>
#include "../core/VanguardTypes.h"
#include "../ui/CanvasManager.h"
#include <vector>

namespace Vanguard {

class SpectrumView {
public:
    SpectrumView();
    ~SpectrumView();

    void show();
    void hide();
    bool isVisible() const { return m_visible; }

    void tick();
    void render();

    // Navigation
    void handleInput(); // Consumes global input

private:
    bool m_visible;
    M5Canvas* m_canvas;
    
    // State
    uint32_t m_lastRenderMs;
    uint32_t m_lastHopMs;
    uint8_t  m_currentChannel; // 1-14
    bool     m_paused;
    
    // Data Storage
    static constexpr int CHANNELS = 14;
    struct ChannelStats {
        int packetCount;    // Traffic density
        int peakRssi;       // Max signal seen
        int samples;        // Number of samples this sweep
        
        // History for waterfall/smoothing
        int history[10];
    };
    ChannelStats m_channels[CHANNELS + 1]; // 1-based indexing for convenience
    
    // Current sample (accumulating during dwell time)
    int m_currentPackets;
    int m_currentMaxRssi;

    // Rendering
    void drawGrid();
    void drawBars();
    void drawInfo();
};

} // namespace Vanguard

#endif // VANGUARD_SPECTRUM_VIEW_H
