/**
 * @file TargetDetail.cpp
 * @brief Single target view with context-aware actions
 *
 * Uses sprite-based double buffering to eliminate flickering.
 */

#include "TargetDetail.h"
#include <M5Cardputer.h>

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
    , m_canvas(nullptr)
    , m_lastRenderMs(0)
{
    // Get available actions for this target
    m_actions = m_engine.getActionsFor(target);

    // Create sprite for double buffering
    m_canvas = new M5Canvas(&M5Cardputer.Display);
    m_canvas->createSprite(Theme::SCREEN_WIDTH, Theme::SCREEN_HEIGHT);
}

TargetDetail::~TargetDetail() {
    if (m_canvas) {
        m_canvas->deleteSprite();
        delete m_canvas;
        m_canvas = nullptr;
    }
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
                : m_progress.statusText ? m_progress.statusText : "Attack failed";
            transitionTo(DetailViewState::RESULT);
        }
    }
}

void TargetDetail::render() {
    // Frame rate limiting
    uint32_t now = millis();
    if ((now - m_lastRenderMs) < RENDER_INTERVAL_MS) {
        return;
    }
    m_lastRenderMs = now;

    // Draw everything to sprite first
    m_canvas->fillScreen(Theme::COLOR_BACKGROUND);
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

    // Push sprite to display in one operation
    m_canvas->pushSprite(0, 0);
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
// RENDERING - PRIVATE (all to m_canvas)
// =============================================================================

void TargetDetail::renderHeader() {
    m_canvas->fillRect(0, 0, Theme::SCREEN_WIDTH, Theme::HEADER_HEIGHT,
                       Theme::COLOR_SURFACE);

    // Back indicator
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->drawString("<Q", 4, 3);

    // Target SSID (truncated if needed)
    char ssidDisplay[20];
    strncpy(ssidDisplay, m_target.ssid, 16);
    ssidDisplay[16] = '\0';
    if (strlen(m_target.ssid) > 16) {
        strcat(ssidDisplay, "..");
    }
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->drawString(ssidDisplay, 28, 3);

    // Signal strength on right
    char rssiStr[8];
    snprintf(rssiStr, sizeof(rssiStr), "%ddB", m_target.rssi);
    m_canvas->setTextDatum(TR_DATUM);
    m_canvas->setTextColor(Theme::getSignalColor(m_target.rssi));
    m_canvas->drawString(rssiStr, Theme::SCREEN_WIDTH - 4, 3);
}

void TargetDetail::renderInfo() {
    int16_t y = Theme::HEADER_HEIGHT + 6;

    // BSSID
    char bssidStr[18];
    m_target.formatBssid(bssidStr);
    renderInfoField(y, "BSSID:", bssidStr);
    y += 12;

    // Channel with 5GHz warning
    char chanStr[24];
    if (m_target.channel > 14) {
        snprintf(chanStr, sizeof(chanStr), "%d (5GHz)", m_target.channel);
    } else {
        snprintf(chanStr, sizeof(chanStr), "%d (2.4GHz)", m_target.channel);
    }
    renderInfoField(y, "Channel:", chanStr);
    y += 12;

    // Security
    const char* secStr;
    switch (m_target.security) {
        case SecurityType::OPEN: secStr = "Open (No password)"; break;
        case SecurityType::WEP: secStr = "WEP (Weak)"; break;
        case SecurityType::WPA_PSK: secStr = "WPA-PSK"; break;
        case SecurityType::WPA2_PSK: secStr = "WPA2-PSK"; break;
        case SecurityType::WPA2_ENTERPRISE: secStr = "WPA2-Enterprise"; break;
        case SecurityType::WPA3_SAE: secStr = "WPA3 (Strong)"; break;
        default: secStr = "Unknown";
    }
    renderInfoField(y, "Security:", secStr);
    y += 12;

    // Type
    const char* typeStr = (m_target.type == TargetType::ACCESS_POINT) ? "Access Point" :
                          (m_target.type == TargetType::STATION) ? "Client Station" :
                          (m_target.type == TargetType::BLE_DEVICE) ? "BLE Device" : "Unknown";
    renderInfoField(y, "Type:", typeStr);
    y += 16;

    // 5GHz warning if applicable
    if (m_target.channel > 14) {
        m_canvas->setTextSize(1);
        m_canvas->setTextColor(Theme::COLOR_WARNING);
        m_canvas->setTextDatum(MC_DATUM);
        m_canvas->drawString("! 5GHz - Some attacks limited", Theme::SCREEN_WIDTH / 2, y);
        y += 14;
    }

    // Action prompt
    m_canvas->setTextDatum(MC_DATUM);
    if (!m_actions.empty()) {
        m_canvas->setTextColor(Theme::COLOR_ACCENT);
        m_canvas->drawString("[ENTER] View Actions", Theme::SCREEN_WIDTH / 2, y + 8);
    } else {
        m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
        m_canvas->drawString("No actions available", Theme::SCREEN_WIDTH / 2, y + 8);
    }

    // Footer hint
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->setTextDatum(BC_DATUM);
    m_canvas->drawString("[Q] Back  [M] Menu", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT - 2);
}

void TargetDetail::renderInfoField(int y, const char* label, const char* value) {
    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(TL_DATUM);

    m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);
    m_canvas->drawString(label, 8, y);

    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->drawString(value, 70, y);
}

void TargetDetail::renderActions() {
    int16_t y = Theme::HEADER_HEIGHT + 4;

    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->drawString("AVAILABLE ACTIONS:", 8, y);
    y += 12;

    // Show up to 4 actions
    int startIdx = 0;
    if (m_actionIndex >= 4) {
        startIdx = m_actionIndex - 3;
    }

    for (int i = startIdx; i < (int)m_actions.size() && (i - startIdx) < 4; i++) {
        bool selected = (i == m_actionIndex);
        renderActionItem(m_actions[i], y, selected);
        y += 22;
    }

    // Scroll indicator if needed
    if (m_actions.size() > 4) {
        m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
        m_canvas->setTextDatum(BR_DATUM);
        char scrollStr[16];
        snprintf(scrollStr, sizeof(scrollStr), "%d/%d", m_actionIndex + 1, (int)m_actions.size());
        m_canvas->drawString(scrollStr, Theme::SCREEN_WIDTH - 8, Theme::SCREEN_HEIGHT - 12);
    }

    // Footer hint
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->setTextDatum(BC_DATUM);
    m_canvas->drawString("[;,.] Nav  [ENTER] Select  [Q] Back", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT - 2);
}

void TargetDetail::renderActionItem(const AvailableAction& action, int y, bool selected) {
    int16_t x = 8;
    int16_t w = Theme::SCREEN_WIDTH - 16;
    int16_t h = 20;

    // Background
    uint16_t bgColor = selected ? Theme::COLOR_SURFACE_RAISED : Theme::COLOR_SURFACE;
    m_canvas->fillRoundRect(x, y, w, h, 3, bgColor);

    if (selected) {
        m_canvas->fillRect(x, y, 3, h, Theme::COLOR_ACCENT);
    }

    // Destructive indicator
    if (action.isDestructive) {
        m_canvas->fillCircle(x + 10, y + h/2, 3, Theme::COLOR_DANGER);
    }

    // Label
    m_canvas->setTextSize(1);
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->setTextDatum(TL_DATUM);
    m_canvas->drawString(action.label, x + 18, y + 2);

    // Description (truncated)
    char descDisplay[32];
    strncpy(descDisplay, action.description, 28);
    descDisplay[28] = '\0';
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString(descDisplay, x + 18, y + 11);
}

void TargetDetail::renderConfirmation() {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    // Warning box
    int16_t boxW = 200;
    int16_t boxH = 70;
    int16_t boxX = (Theme::SCREEN_WIDTH - boxW) / 2;
    int16_t boxY = (Theme::SCREEN_HEIGHT - boxH) / 2;

    m_canvas->fillRoundRect(boxX, boxY, boxW, boxH, 4, Theme::COLOR_SURFACE);
    m_canvas->drawRoundRect(boxX, boxY, boxW, boxH, 4, Theme::COLOR_DANGER);

    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(MC_DATUM);

    m_canvas->setTextColor(Theme::COLOR_DANGER);
    m_canvas->drawString("CONFIRM ATTACK?", centerX, centerY - 20);

    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->drawString(m_actions[m_actionIndex].label, centerX, centerY - 5);

    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString("This may disrupt network traffic", centerX, centerY + 10);

    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString("[ENTER] Yes  [Q] No", centerX, centerY + 25);
}

void TargetDetail::renderExecuting() {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(MC_DATUM);

    // Status
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString(m_progress.statusText ? m_progress.statusText : "Running...",
                         centerX, centerY - 25);

    // Animated indicator
    static uint8_t phase = 0;
    phase = (phase + 1) % 4;
    char indicator[8];
    snprintf(indicator, sizeof(indicator), "%.*s", phase + 1, "....");
    m_canvas->drawString(indicator, centerX, centerY - 10);

    // Packets sent
    char pktStr[32];
    snprintf(pktStr, sizeof(pktStr), "Packets: %u", (unsigned int)m_progress.packetsSent);
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->drawString(pktStr, centerX, centerY + 5);

    // Elapsed time
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "Time: %us", (unsigned int)(m_progress.elapsedMs / 1000));
    m_canvas->drawString(timeStr, centerX, centerY + 20);

    // Stop hint
    m_canvas->setTextColor(Theme::COLOR_WARNING);
    m_canvas->drawString("[Q] Stop Attack", centerX, centerY + 40);
}

void TargetDetail::renderResult() {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    uint16_t color = (m_result == ActionResult::SUCCESS)
        ? Theme::COLOR_SUCCESS
        : Theme::COLOR_DANGER;

    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(MC_DATUM);

    // Result status
    const char* statusLabel = (m_result == ActionResult::SUCCESS) ? "SUCCESS" :
                              (m_result == ActionResult::CANCELLED) ? "CANCELLED" :
                              (m_result == ActionResult::FAILED_NOT_SUPPORTED) ? "NOT SUPPORTED" :
                              "FAILED";
    m_canvas->setTextColor(color);
    m_canvas->drawString(statusLabel, centerX, centerY - 25);

    // Message
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->drawString(m_resultMessage ? m_resultMessage : "Done", centerX, centerY - 10);

    // Stats
    if (m_progress.packetsSent > 0) {
        char statsStr[48];
        snprintf(statsStr, sizeof(statsStr), "%u packets in %us",
                 (unsigned int)m_progress.packetsSent, (unsigned int)(m_progress.elapsedMs / 1000));
        m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);
        m_canvas->drawString(statsStr, centerX, centerY + 10);
    }

    // Continue hint
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString("[ENTER] Continue", centerX, centerY + 30);
}

// =============================================================================
// INPUT HANDLING
// =============================================================================

void TargetDetail::handleInput() {
    // Keyboard input is now handled in main.cpp handleKeyboardInput()
}

// =============================================================================
// STATE TRANSITIONS
// =============================================================================

void TargetDetail::transitionTo(DetailViewState newState) {
    m_state = newState;
}

} // namespace Assessor
