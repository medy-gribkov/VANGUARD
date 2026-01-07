#ifndef ASSESSOR_SETTINGS_PANEL_H
#define ASSESSOR_SETTINGS_PANEL_H

/**
 * @file SettingsPanel.h
 * @brief Settings screen for configuring scan and attack parameters
 */

#include <M5Cardputer.h>
#include "Theme.h"
#include <vector>

namespace Assessor {

enum class SettingType {
    TOGGLE,     // On/Off boolean
    NUMBER,     // Numeric value with min/max
    CHOICE      // Select from options
};

struct Setting {
    const char* label;
    const char* description;
    SettingType type;
    int         value;      // Current value
    int         minVal;     // For NUMBER type
    int         maxVal;     // For NUMBER type
    int         step;       // Increment for NUMBER
};

class SettingsPanel {
public:
    SettingsPanel();
    ~SettingsPanel();

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
     * @brief Navigation
     */
    void navigateUp();
    void navigateDown();
    void adjustUp();     // Increase value
    void adjustDown();   // Decrease value
    void select();       // Toggle or enter sub-menu

    /**
     * @brief Check if back was requested
     */
    bool wantsBack() const;
    void clearBack();

    // Settings accessors
    int  getScanDurationMs() const;
    int  getDeauthPacketCount() const;
    bool getAutoRescan() const;
    bool getSoundEnabled() const;

private:
    bool         m_visible;
    int          m_highlightIndex;
    bool         m_wantsBack;
    bool         m_needsRedraw;

    M5Canvas*    m_canvas;
    uint32_t     m_lastRenderMs;

    std::vector<Setting> m_settings;

    static constexpr int HEADER_HEIGHT = 16;
    static constexpr int ITEM_HEIGHT = 22;
    static constexpr int VISIBLE_ITEMS = 5;
    static constexpr uint32_t RENDER_INTERVAL_MS = 50;

    void initSettings();
    void renderSetting(const Setting& setting, int y, bool highlighted);
};

} // namespace Assessor

#endif // ASSESSOR_SETTINGS_PANEL_H
