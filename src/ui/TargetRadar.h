#ifndef ASSESSOR_TARGET_RADAR_H
#define ASSESSOR_TARGET_RADAR_H

/**
 * @file TargetRadar.h
 * @brief Main target list view - the heart of the UI
 *
 * The TargetRadar is where users spend most of their time.
 * It shows all discovered targets in a scrollable list,
 * sorted by signal strength (strongest first).
 *
 * Visual elements per target:
 * - Signal strength indicator (●/◐/○/◦)
 * - SSID or device name
 * - RSSI in dBm
 * - Security badge (OPEN, WPA2, etc.)
 * - Client count (for APs)
 *
 * @example
 * TargetRadar radar(engine);
 * radar.tick();
 * radar.render();
 * if (radar.hasSelection()) {
 *     const Target* selected = radar.getSelectedTarget();
 *     // transition to detail view
 * }
 */

#include <M5Cardputer.h>
#include "../core/Types.h"
#include "../core/AssessorEngine.h"
#include "Theme.h"
#include <vector>

namespace Assessor {

class TargetRadar {
public:
    /**
     * @brief Construct radar view
     * @param engine Reference to the assessor engine for data
     */
    explicit TargetRadar(AssessorEngine& engine);
    ~TargetRadar();

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Process input and update state
     */
    void tick();

    /**
     * @brief Render the radar view
     */
    void render();

    /**
     * @brief Render scanning progress (before targets are available)
     */
    void renderScanning();

    // -------------------------------------------------------------------------
    // Selection
    // -------------------------------------------------------------------------

    /**
     * @brief Check if a target was selected (user pressed enter)
     */
    bool hasSelection() const;

    /**
     * @brief Get the selected target
     * @return Pointer to selected target, or nullptr
     */
    const Target* getSelectedTarget() const;

    /**
     * @brief Clear selection state (after handling)
     */
    void clearSelection();

    /**
     * @brief Get currently highlighted index
     */
    int getHighlightedIndex() const;

    // -------------------------------------------------------------------------
    // Navigation
    // -------------------------------------------------------------------------

    /**
     * @brief Move highlight up
     */
    void navigateUp();

    /**
     * @brief Move highlight down
     */
    void navigateDown();

    /**
     * @brief Select currently highlighted target
     */
    void select();

    /**
     * @brief Scroll to top of list
     */
    void scrollToTop();

    // -------------------------------------------------------------------------
    // Filtering
    // -------------------------------------------------------------------------

    /**
     * @brief Set filter criteria
     */
    void setFilter(const TargetFilter& filter);

    /**
     * @brief Get current filter
     */
    const TargetFilter& getFilter() const;

    /**
     * @brief Set sort order
     */
    void setSortOrder(SortOrder order);

    // -------------------------------------------------------------------------
    // Refresh
    // -------------------------------------------------------------------------

    /**
     * @brief Force refresh of target list from engine
     */
    void refresh();

    /**
     * @brief Enable/disable auto-refresh
     */
    void setAutoRefresh(bool enabled);

private:
    AssessorEngine&      m_engine;

    // State
    std::vector<Target>  m_targets;         // Cached, filtered list
    int                  m_highlightIndex;  // Currently highlighted
    int                  m_scrollOffset;    // For scrolling long lists
    bool                 m_hasSelection;    // User pressed select
    const Target*        m_selectedTarget;

    // Settings
    TargetFilter         m_filter;
    SortOrder            m_sortOrder;
    bool                 m_autoRefresh;
    uint32_t             m_lastRefreshMs;
    uint32_t             m_lastRenderMs;    // For frame limiting
    bool                 m_needsRedraw;     // Dirty flag

    // Double buffer sprite
    M5Canvas*            m_canvas;

    // Rendering constants
    static constexpr int HEADER_HEIGHT      = Theme::HEADER_HEIGHT;
    static constexpr int ITEM_HEIGHT        = Theme::LIST_ITEM_HEIGHT;
    static constexpr int VISIBLE_ITEMS      = Theme::LIST_VISIBLE_ITEMS;
    static constexpr uint32_t REFRESH_INTERVAL_MS = 1000;
    static constexpr uint32_t RENDER_INTERVAL_MS = 50;  // 20 FPS max

    // Rendering helpers
    void renderHeader();
    void renderTargetList();
    void renderTargetItem(const Target& target, int y, bool highlighted);
    void renderTargetItemToCanvas(const Target& target, int y, bool highlighted);
    void renderEmptyState();
    void renderScrollIndicator();

    // Item rendering details
    void drawSignalIndicator(int x, int y, int8_t rssi);
    void drawSecurityBadge(int x, int y, SecurityType security);
    void drawClientCount(int x, int y, uint8_t count);

    // Input handling
    void handleInput();

    // List management
    void updateTargetList();
    void ensureHighlightVisible();
};

} // namespace Assessor

#endif // ASSESSOR_TARGET_RADAR_H
