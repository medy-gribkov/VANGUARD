/**
 * @file TargetDetail.cpp
 * @brief Single target view with context-aware actions
 */

#include "TargetDetail.h"

namespace Assessor {

TargetDetail::TargetDetail(AssessorEngine& engine, const Target& target)
    : m_engine(engine)
    , m_target(target)
    , m_state(DetailViewState::INFO)
    , m_actionIndex(0)
    , m_wantsBack(false)
    , m_actionConfirmed(false)
    , m_confirmedAction(ActionType::NONE)
    , m_resultMessage(nullptr)
{
    // Get available actions for this target
    m_actions = m_engine.getActionsFor(target);
}

// =============================================================================
// LIFECYCLE
// =============================================================================

void TargetDetail::tick() {
    handleInput();

    // Update action progress if executing
    if (m_state == DetailViewState::EXECUTING) {
        m_progress = m_engine.getActionProgress();

        if (m_progress.result != ActionResult::IN_PROGRESS) {
            m_result = m_progress.result;
            m_resultMessage = (m_result == ActionResult::SUCCESS)
                ? "Attack complete"
                : "Attack failed";
            transitionTo(DetailViewState::RESULT);
        }
    }
}

void TargetDetail::render() {
    M5.Display.fillScreen(Theme::COLOR_BACKGROUND);
    renderHeader();

    switch (m_state) {
        case DetailViewState::INFO:
            renderInfo();
            break;
        case DetailViewState::ACTIONS:
            renderActions();
            break;
        case DetailViewState::CONFIRM:
            renderConfirmation();
            break;
        case DetailViewState::EXECUTING:
            renderExecuting();
            break;
        case DetailViewState::RESULT:
            renderResult();
            break;
    }
}

// =============================================================================
// NAVIGATION
// =============================================================================

bool TargetDetail::wantsBack() const {
    return m_wantsBack;
}

void TargetDetail::clearBackRequest() {
    m_wantsBack = false;
}

void TargetDetail::navigateUp() {
    if (m_state == DetailViewState::ACTIONS) {
        if (m_actionIndex > 0) {
            m_actionIndex--;
        }
    }
}

void TargetDetail::navigateDown() {
    if (m_state == DetailViewState::ACTIONS) {
        if (m_actionIndex < (int)m_actions.size() - 1) {
            m_actionIndex++;
        }
    }
}

void TargetDetail::select() {
    switch (m_state) {
        case DetailViewState::INFO:
            if (!m_actions.empty()) {
                transitionTo(DetailViewState::ACTIONS);
            }
            break;

        case DetailViewState::ACTIONS:
            if (m_actionIndex >= 0 && m_actionIndex < (int)m_actions.size()) {
                if (m_actions[m_actionIndex].isDestructive) {
                    transitionTo(DetailViewState::CONFIRM);
                } else {
                    m_confirmedAction = m_actions[m_actionIndex].type;
                    m_actionConfirmed = true;
                }
            }
            break;

        case DetailViewState::CONFIRM:
            m_confirmedAction = m_actions[m_actionIndex].type;
            m_actionConfirmed = true;
            break;

        case DetailViewState::RESULT:
            m_wantsBack = true;
            break;

        default:
            break;
    }
}

void TargetDetail::back() {
    switch (m_state) {
        case DetailViewState::INFO:
            m_wantsBack = true;
            break;

        case DetailViewState::ACTIONS:
            transitionTo(DetailViewState::INFO);
            break;

        case DetailViewState::CONFIRM:
            transitionTo(DetailViewState::ACTIONS);
            break;

        case DetailViewState::EXECUTING:
            m_engine.stopAction();
            transitionTo(DetailViewState::ACTIONS);
            break;

        case DetailViewState::RESULT:
            m_wantsBack = true;
            break;
    }
}

// =============================================================================
// ACTION EXECUTION
// =============================================================================

bool TargetDetail::actionConfirmed() const {
    return m_actionConfirmed;
}

ActionType TargetDetail::getConfirmedAction() const {
    return m_confirmedAction;
}

void TargetDetail::clearActionConfirmation() {
    m_actionConfirmed = false;
    transitionTo(DetailViewState::EXECUTING);
}

void TargetDetail::updateActionProgress(const ActionProgress& progress) {
    m_progress = progress;
}

void TargetDetail::showResult(ActionResult result, const char* message) {
    m_result = result;
    m_resultMessage = message;
    transitionTo(DetailViewState::RESULT);
}

// =============================================================================
// STATE
// =============================================================================

DetailViewState TargetDetail::getState() const {
    return m_state;
}

const Target& TargetDetail::getTarget() const {
    return m_target;
}

// =============================================================================
// RENDERING - PRIVATE
// =============================================================================

void TargetDetail::renderHeader() {
    M5.Display.fillRect(0, 0, Theme::SCREEN_WIDTH, Theme::HEADER_HEIGHT,
                        Theme::COLOR_SURFACE);

    // Back indicator
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(Theme::COLOR_TEXT_SECONDARY);
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.drawString("<", 4, 3);

    // Target SSID
    M5.Display.setTextColor(Theme::COLOR_TEXT_PRIMARY);
    M5.Display.drawString(m_target.ssid, 16, 3);
}

void TargetDetail::renderInfo() {
    int16_t y = Theme::HEADER_HEIGHT + 8;

    // BSSID
    char bssidStr[18];
    m_target.formatBssid(bssidStr);
    renderInfoField(y, "BSSID:", bssidStr);
    y += 14;

    // Channel
    char chanStr[8];
    snprintf(chanStr, sizeof(chanStr), "%d", m_target.channel);
    renderInfoField(y, "Channel:", chanStr);
    y += 14;

    // Security
    const char* secStr;
    switch (m_target.security) {
        case SecurityType::OPEN: secStr = "Open"; break;
        case SecurityType::WEP: secStr = "WEP"; break;
        case SecurityType::WPA_PSK: secStr = "WPA-PSK"; break;
        case SecurityType::WPA2_PSK: secStr = "WPA2-PSK"; break;
        case SecurityType::WPA2_ENTERPRISE: secStr = "WPA2-Enterprise"; break;
        case SecurityType::WPA3_SAE: secStr = "WPA3-SAE"; break;
        default: secStr = "Unknown";
    }
    renderInfoField(y, "Security:", secStr);
    y += 14;

    // Signal
    char rssiStr[16];
    snprintf(rssiStr, sizeof(rssiStr), "%d dBm", m_target.rssi);
    renderInfoField(y, "Signal:", rssiStr);
    y += 14;

    // Clients
    char clientStr[8];
    snprintf(clientStr, sizeof(clientStr), "%d", m_target.clientCount);
    renderInfoField(y, "Clients:", clientStr);
    y += 20;

    // Action prompt
    if (!m_actions.empty()) {
        M5.Display.setTextColor(Theme::COLOR_ACCENT);
        M5.Display.setTextDatum(MC_DATUM);
        M5.Display.drawString("[ENTER] View Actions", Theme::SCREEN_WIDTH / 2, y);
    } else {
        M5.Display.setTextColor(Theme::COLOR_TEXT_MUTED);
        M5.Display.setTextDatum(MC_DATUM);
        M5.Display.drawString("No actions available", Theme::SCREEN_WIDTH / 2, y);
    }
}

void TargetDetail::renderInfoField(int y, const char* label, const char* value) {
    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(TL_DATUM);

    M5.Display.setTextColor(Theme::COLOR_TEXT_SECONDARY);
    M5.Display.drawString(label, 8, y);

    M5.Display.setTextColor(Theme::COLOR_TEXT_PRIMARY);
    M5.Display.drawString(value, 80, y);
}

void TargetDetail::renderActions() {
    int16_t y = Theme::HEADER_HEIGHT + 4;

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(Theme::COLOR_TEXT_SECONDARY);
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.drawString("AVAILABLE ACTIONS:", 8, y);
    y += 14;

    for (int i = 0; i < (int)m_actions.size() && i < 4; i++) {
        bool selected = (i == m_actionIndex);
        renderActionItem(m_actions[i], y, selected);
        y += 24;
    }
}

void TargetDetail::renderActionItem(const AvailableAction& action, int y, bool selected) {
    int16_t x = 8;
    int16_t w = Theme::SCREEN_WIDTH - 16;
    int16_t h = 22;

    // Background
    uint16_t bgColor = selected ? Theme::COLOR_SURFACE_RAISED : Theme::COLOR_SURFACE;
    M5.Display.fillRoundRect(x, y, w, h, 3, bgColor);

    if (selected) {
        M5.Display.drawRoundRect(x, y, w, h, 3, Theme::COLOR_ACCENT);
    }

    // Destructive indicator
    if (action.isDestructive) {
        M5.Display.fillCircle(x + 8, y + h/2, 3, Theme::COLOR_DANGER);
    }

    // Label
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(Theme::COLOR_TEXT_PRIMARY, bgColor);
    M5.Display.setTextDatum(TL_DATUM);
    M5.Display.drawString(action.label, x + 16, y + 3);

    // Description
    M5.Display.setTextColor(Theme::COLOR_TEXT_MUTED, bgColor);
    M5.Display.drawString(action.description, x + 16, y + 12);
}

void TargetDetail::renderConfirmation() {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    // Warning box
    int16_t boxW = 180;
    int16_t boxH = 60;
    int16_t boxX = (Theme::SCREEN_WIDTH - boxW) / 2;
    int16_t boxY = (Theme::SCREEN_HEIGHT - boxH) / 2;

    M5.Display.fillRoundRect(boxX, boxY, boxW, boxH, 4, Theme::COLOR_SURFACE);
    M5.Display.drawRoundRect(boxX, boxY, boxW, boxH, 4, Theme::COLOR_DANGER);

    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(MC_DATUM);

    M5.Display.setTextColor(Theme::COLOR_DANGER);
    M5.Display.drawString("CONFIRM ATTACK?", centerX, centerY - 15);

    M5.Display.setTextColor(Theme::COLOR_TEXT_PRIMARY);
    M5.Display.drawString(m_actions[m_actionIndex].label, centerX, centerY);

    M5.Display.setTextColor(Theme::COLOR_TEXT_SECONDARY);
    M5.Display.drawString("[ENTER] Yes  [ESC] No", centerX, centerY + 18);
}

void TargetDetail::renderExecuting() {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(MC_DATUM);

    M5.Display.setTextColor(Theme::COLOR_ACCENT);
    M5.Display.drawString(m_progress.statusText ? m_progress.statusText : "Running...",
                          centerX, centerY - 20);

    // Packets sent
    char pktStr[32];
    snprintf(pktStr, sizeof(pktStr), "Packets: %u", (unsigned int)m_progress.packetsSent);
    M5.Display.setTextColor(Theme::COLOR_TEXT_PRIMARY);
    M5.Display.drawString(pktStr, centerX, centerY);

    // Elapsed time
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "Time: %us", (unsigned int)(m_progress.elapsedMs / 1000));
    M5.Display.drawString(timeStr, centerX, centerY + 15);

    // Stop hint
    M5.Display.setTextColor(Theme::COLOR_TEXT_MUTED);
    M5.Display.drawString("[ESC] Stop", centerX, centerY + 35);
}

void TargetDetail::renderResult() {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    uint16_t color = (m_result == ActionResult::SUCCESS)
        ? Theme::COLOR_SUCCESS
        : Theme::COLOR_DANGER;

    M5.Display.setTextSize(1);
    M5.Display.setTextDatum(MC_DATUM);

    M5.Display.setTextColor(color);
    M5.Display.drawString(m_resultMessage ? m_resultMessage : "Done", centerX, centerY - 10);

    // Stats
    char statsStr[48];
    snprintf(statsStr, sizeof(statsStr), "%u packets in %us",
             (unsigned int)m_progress.packetsSent, (unsigned int)(m_progress.elapsedMs / 1000));
    M5.Display.setTextColor(Theme::COLOR_TEXT_SECONDARY);
    M5.Display.drawString(statsStr, centerX, centerY + 10);

    // Continue hint
    M5.Display.setTextColor(Theme::COLOR_TEXT_MUTED);
    M5.Display.drawString("[ENTER] Continue", centerX, centerY + 30);
}

// =============================================================================
// INPUT HANDLING
// =============================================================================

void TargetDetail::handleInput() {
    // Button-based navigation
    if (M5.BtnA.wasPressed()) {
        navigateUp();
    }
    if (M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) {
        select();
    }
    if (M5.BtnC.wasPressed()) {
        navigateDown();
    }
    // Long press BtnA for back
    if (M5.BtnA.wasHold()) {
        back();
    }
}

// =============================================================================
// STATE TRANSITIONS
// =============================================================================

void TargetDetail::transitionTo(DetailViewState newState) {
    m_state = newState;
}

} // namespace Assessor
