#ifndef VANGUARD_TARGET_DETAIL_H
#define VANGUARD_TARGET_DETAIL_H

/**
 * @file TargetDetail.h
 * @brief Single-target detail view with available actions
 *
 * When a user selects a target from the radar, this view shows:
 * - Full target information (BSSID, SSID, channel, security, etc.)
 * - List of available actions (context-aware!)
 * - Action selection and confirmation
 *
 * This is where VANGUARD's philosophy shines - we ONLY show
 * actions that make sense for this specific target's state.
 *
 * @example
 * TargetDetail detail(engine, selectedTarget);
 * detail.tick();
 * detail.render();
 * if (detail.actionSelected()) {
 *     engine.executeAction(detail.getSelectedAction(), target);
 * }
 */

#include <M5Cardputer.h>
#include "../core/VanguardTypes.h"
#include "../core/VanguardEngine.h"
#include "Theme.h"
#include <vector>

namespace Vanguard {

/**
 * @brief View state within target detail
 */
enum class DetailViewState : uint8_t {
    INFO,           // Showing target info
    ACTIONS,        // Showing action list
    CLIENT_SELECT,  // Selecting a specific station/client
    CONFIRM,        // Confirming destructive action
    EXECUTING,      // Action in progress
    RESULT          // Showing action result
};

class TargetDetail {
public:
    /**
     * @brief Construct detail view for a target
     * @param engine The VANGUARD engine
     * @param target The target to display
     */
    TargetDetail(VanguardEngine& engine, const Target& target);
    ~TargetDetail();

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Process input and update state
     */
    void tick();

    /**
     * @brief Render the detail view
     */
    void render();

    // -------------------------------------------------------------------------
    // Navigation
    // -------------------------------------------------------------------------

    /**
     * @brief Check if user wants to go back
     */
    bool wantsBack() const;

    /**
     * @brief Clear back request (after handling)
     */
    void clearBackRequest();

    /**
     * @brief Move selection up
     */
    void navigateUp();

    /**
     * @brief Move selection down
     */
    void navigateDown();

    /**
     * @brief Select current item (action)
     */
    void select();

    /**
     * @brief Go back (cancel/close)
     */
    void back();

    // -------------------------------------------------------------------------
    // Action Execution
    // -------------------------------------------------------------------------

    /**
     * @brief Check if an action was selected and confirmed
     */
    bool actionConfirmed() const;

    /**
     * @brief Get the confirmed action type
     */
    ActionType getConfirmedAction() const;

    /**
     * @brief Clear action confirmation (after starting execution)
     */
    void clearActionConfirmation();

    /**
     * @brief Get station MAC for precision attacks
     */
    void getConfirmedStationMac(uint8_t* mac) const;

    // -------------------------------------------------------------------------
    // State
    // -------------------------------------------------------------------------

    /**
     * @brief Get current view state
     */
    DetailViewState getState() const;

    /**
     * @brief Get the target being displayed
     */
    const Target& getTarget() const;

private:
    VanguardEngine&             m_engine;
    Target                      m_target;

    // State
    DetailViewState             m_state;
    std::vector<AvailableAction> m_actions;
    int                         m_actionIndex;
    int                         m_selectedClientIndex;
    bool                        m_hasSelectedClient;
    bool                        m_wantsBack;
    bool                        m_actionConfirmed;
    ActionType                  m_confirmedAction;
    ActionProgress              m_progress;
    ActionResult                m_result;
    const char*                 m_resultMessage;
    char                        m_resultBuf[48];

    // Double buffer sprite
    M5Canvas*                   m_canvas;
    uint32_t                    m_lastRenderMs;
    static constexpr uint32_t   RENDER_INTERVAL_MS = 50;

    // Rendering helpers
    void renderInfo();
    void renderActions();
    void renderConfirmation();
    void renderExecuting();
    void renderResult();

    void renderHeader();
    void renderInfoField(int y, const char* label, const char* value);
    void renderActionItem(const AvailableAction& action, int y, bool selected);

    // State transitions
    void transitionTo(DetailViewState newState);
};

} // namespace Vanguard

#endif // VANGUARD_TARGET_DETAIL_H
