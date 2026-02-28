/**
 * @file TargetDetail.cpp
 * @brief Single target view with context-aware actions
 *
 * Uses sprite-based double buffering to eliminate flickering.
 */

#include "TargetDetail.h"
#include <M5Cardputer.h>
#include "CanvasManager.h"

namespace Vanguard {

TargetDetail::TargetDetail(VanguardEngine& engine, const Target& target)
    : m_engine(engine)
    , m_target(target)
    , m_state(DetailViewState::INFO)
    , m_actionIndex(0)
    , m_selectedClientIndex(-1)
    , m_hasSelectedClient(false)
    , m_wantsBack(false)
    , m_actionConfirmed(false)
    , m_confirmedAction(ActionType::NONE)
    , m_resultMessage(nullptr)
    , m_confirmingStop(false)
    , m_canvas(&CanvasManager::getInstance().getCanvas())
    , m_lastRenderMs(0)
{
    yield();  // Feed watchdog before potentially slow operations

    // Get available actions for this target
    m_actions = m_engine.getActionsFor(target);

    // Get available chains for this target
    ActionResolver resolver;
    m_chains = resolver.getChainsFor(target);
    m_chainIndex = 0;
    m_chainActive = false;
    m_chainStep = 0;
    m_chainTotalSteps = 0;

    yield();  // Feed watchdog

    if (Serial) {
        Serial.printf("[Detail] Created for target '%s' with %d actions, %d chains\n",
                      target.ssid, (int)m_actions.size(), (int)m_chains.size());
    }
}

TargetDetail::~TargetDetail() {
    // m_canvas is shared, do not delete
}

// =============================================================================
// LIFECYCLE
// =============================================================================

void TargetDetail::tick() {
    // Update action progress if executing
    if (m_state == DetailViewState::EXECUTING) {
        m_progress = m_engine.getActionProgress();

        if (m_progress.result != ActionResult::IN_PROGRESS) {
            // Chain: advance to next step if current succeeded
            if (m_chainActive && m_chainStep + 1 < m_chainTotalSteps &&
                m_progress.result == ActionResult::SUCCESS) {
                m_chainStep++;
                m_confirmedAction = m_chains[m_chainIndex].steps[m_chainStep];
                m_actionConfirmed = true;
                // Stay in EXECUTING state, main.cpp will pick up actionConfirmed
                return;
            }
            // Chain complete or single action complete
            m_chainActive = false;

            m_result = m_progress.result;
            if (m_result == ActionResult::SUCCESS) {
                // Build a contextual success message with the action name
                if (m_actionIndex >= 0 && m_actionIndex < (int)m_actions.size()) {
                    snprintf(m_resultBuf, sizeof(m_resultBuf), "%s completed", m_actions[m_actionIndex].label);
                    m_resultMessage = m_resultBuf;
                } else {
                    m_resultMessage = "Action completed";
                }
            } else if (m_result == ActionResult::STOPPED || m_result == ActionResult::CANCELLED) {
                // Use statusText from progress if available (has packet count info)
                if (m_progress.statusText[0] != '\0') {
                    m_resultMessage = m_progress.statusText;
                } else {
                    m_resultMessage = "Stopped by user";
                }
            } else {
                m_resultMessage = (m_progress.statusText[0] != '\0') ? m_progress.statusText : "Action failed";
            }
            transitionTo(DetailViewState::RESULT);
        }
    }
}

void TargetDetail::render() {
    if (!m_canvas) return;

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
        case DetailViewState::CLIENT_SELECT:
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
        if (m_actionIndex > 0) m_actionIndex--;
    } else if (m_state == DetailViewState::CLIENT_SELECT) {
        if (m_selectedClientIndex > 0) {
            m_selectedClientIndex--;
        } else {
            transitionTo(DetailViewState::INFO);
        }
    }
}

void TargetDetail::navigateDown() {
    if (m_state == DetailViewState::ACTIONS) {
        int totalItems = (int)m_actions.size() + (int)m_chains.size();
        if (m_actionIndex < totalItems - 1) m_actionIndex++;
    } else if (m_state == DetailViewState::INFO) {
        if (m_target.clientCount > 0) {
            m_selectedClientIndex = 0;
            transitionTo(DetailViewState::CLIENT_SELECT);
        }
    } else if (m_state == DetailViewState::CLIENT_SELECT) {
        if (m_selectedClientIndex < m_target.clientCount - 1 && m_selectedClientIndex < 2) { // 3 clients max for now
            m_selectedClientIndex++;
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
                // Regular action
                if (m_actions[m_actionIndex].isDestructive) {
                    transitionTo(DetailViewState::CONFIRM);
                } else {
                    m_confirmedAction = m_actions[m_actionIndex].type;
                    m_actionConfirmed = true;
                }
            } else if (m_actionIndex >= (int)m_actions.size()) {
                // Chain selected
                int chainIdx = m_actionIndex - (int)m_actions.size();
                if (chainIdx < (int)m_chains.size()) {
                    m_chainActive = true;
                    m_chainIndex = chainIdx;
                    m_chainStep = 0;
                    m_chainTotalSteps = m_chains[chainIdx].stepCount;
                    m_confirmedAction = m_chains[chainIdx].steps[0];
                    m_actionConfirmed = true;
                }
            }
            break;

        case DetailViewState::CLIENT_SELECT:
            m_hasSelectedClient = true;
            transitionTo(DetailViewState::ACTIONS);
            break;

        case DetailViewState::CONFIRM:
            if (m_confirmingStop) {
                m_confirmingStop = false;
                m_wantsBack = true;  // Signal to main.cpp to stop attack
            } else {
                m_confirmedAction = m_actions[m_actionIndex].type;
                m_actionConfirmed = true;
            }
            break;

        case DetailViewState::RESULT:
            transitionTo(DetailViewState::ACTIONS);
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

        case DetailViewState::CLIENT_SELECT:
            transitionTo(DetailViewState::INFO);
            break;

        case DetailViewState::CONFIRM:
            if (m_confirmingStop) {
                m_confirmingStop = false;
                transitionTo(DetailViewState::EXECUTING);  // Go back to executing
            } else {
                transitionTo(DetailViewState::ACTIONS);
            }
            break;

        case DetailViewState::EXECUTING:
            m_confirmingStop = true;
            transitionTo(DetailViewState::CONFIRM);
            break;

        case DetailViewState::RESULT:
            transitionTo(DetailViewState::ACTIONS);
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

void TargetDetail::getConfirmedStationMac(uint8_t* mac) const {
    if (m_hasSelectedClient && m_selectedClientIndex >= 0 && m_selectedClientIndex < m_target.clientCount) {
        memcpy(mac, m_target.clientMacs[m_selectedClientIndex], 6);
    } else {
        memset(mac, 0, 6);
    }
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
    snprintf(rssiStr, sizeof(rssiStr), "%d dBm", m_target.rssi);
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
                          (m_target.type == TargetType::BLE_DEVICE) ? "BLE Device" : 
                          (m_target.type == TargetType::IR_DEVICE) ? "Infrared Remote" : "Unknown";

    // If IR, show different info
    if (m_target.type == TargetType::IR_DEVICE) {
        y = Theme::HEADER_HEIGHT + 6;
        renderInfoField(y, "Type:", typeStr);
        y += 12;
        renderInfoField(y, "Frequency:", "38 kHz");
        y += 12;
        renderInfoField(y, "Protocol:", "Generic/Raw");
        y += 12;
    } else {
        // ... (existing WiFi info)
        renderInfoField(y, "Type:", typeStr);
        y += 12;
    }

    // Clients
    if (m_target.type == TargetType::ACCESS_POINT) {
        char cliCountStr[16];
        snprintf(cliCountStr, sizeof(cliCountStr), "%d detected", m_target.clientCount);
        renderInfoField(y, "Clients:", cliCountStr);
        y += 12;

        if (m_target.clientCount > 0) {
            m_canvas->setTextSize(1);
            m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
            m_canvas->setTextDatum(TL_DATUM);
            
            // Show up to 3 client MACs
            for (int i = 0; i < m_target.clientCount && i < 3; i++) {
                char macStr[20];
                snprintf(macStr, sizeof(macStr), "#%d %02X:%02X:%02X:%02X:%02X:%02X",
                         i + 1,
                         m_target.clientMacs[i][0], m_target.clientMacs[i][1], m_target.clientMacs[i][2],
                         m_target.clientMacs[i][3], m_target.clientMacs[i][4], m_target.clientMacs[i][5]);
                
                if (m_state == DetailViewState::CLIENT_SELECT && i == m_selectedClientIndex) {
                    m_canvas->setTextColor(Theme::COLOR_ACCENT);
                    m_canvas->fillRect(68, y-1, Theme::SCREEN_WIDTH - 76, 11, Theme::COLOR_SURFACE_RAISED);
                } else {
                    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
                }
                
                m_canvas->drawString(macStr, 70, y);
                y += 10;
            }
            if (m_target.clientCount > 3) {
                m_canvas->drawString(" ...", 70, y);
                y += 10;
            }
        }
    }
    y += 4;

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
        // Check if this is a 5GHz network (explains why no actions)
        if (m_target.channel > 14 && m_target.type == TargetType::ACCESS_POINT) {
            m_canvas->setTextColor(Theme::COLOR_WARNING);
            m_canvas->drawString("5GHz - Info Only", Theme::SCREEN_WIDTH / 2, y + 8);
            m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
            m_canvas->drawString("ESP32 cannot attack 5GHz", Theme::SCREEN_WIDTH / 2, y + 20);
        } else {
            m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
            m_canvas->drawString("No actions available", Theme::SCREEN_WIDTH / 2, y + 8);
        }
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

    // Legend for destructive indicator
    m_canvas->fillCircle(12, y + 3, 2, Theme::COLOR_DANGER);
    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString("= destructive", 18, y);
    y += 10;

    // Show up to 4 actions
    int startIdx = 0;
    if (m_actionIndex >= 4) {
        startIdx = m_actionIndex - 3;
    }

    int totalItems = (int)m_actions.size() + (int)m_chains.size();

    for (int i = startIdx; i < (int)m_actions.size() && (i - startIdx) < 4; i++) {
        bool selected = (i == m_actionIndex);
        renderActionItem(m_actions[i], y, selected);
        y += 22;
    }

    // Chain section (if chains available and visible in scroll window)
    if (!m_chains.empty()) {
        int actionsDrawn = min((int)m_actions.size() - startIdx, 4);
        int slotsLeft = 4 - actionsDrawn;

        // Show separator if we have room and chains are in view
        int chainStartInList = (int)m_actions.size();
        if (startIdx + actionsDrawn >= chainStartInList && slotsLeft > 0) {
            m_canvas->drawLine(8, y + 2, Theme::SCREEN_WIDTH - 8, y + 2, Theme::COLOR_SURFACE_RAISED);
            y += 6;
            m_canvas->setTextColor(Theme::COLOR_TEXT_SECONDARY);
            m_canvas->setTextDatum(TL_DATUM);
            m_canvas->drawString("ATTACK CHAINS:", 8, y);
            y += 12;
        }

        // Draw chain items that fall within scroll window
        for (int i = 0; i < (int)m_chains.size() && y < Theme::SCREEN_HEIGHT - 14; i++) {
            int totalIdx = (int)m_actions.size() + i;
            if (totalIdx < startIdx) continue;
            bool selected = (totalIdx == m_actionIndex);

            int16_t x = 8;
            int16_t w = Theme::SCREEN_WIDTH - 16;
            int16_t h = 20;
            uint16_t bgColor = selected ? Theme::COLOR_SURFACE_RAISED : Theme::COLOR_SURFACE;
            m_canvas->fillRoundRect(x, y, w, h, 3, bgColor);
            if (selected) {
                m_canvas->fillRect(x, y, 3, h, Theme::COLOR_WARNING);
            }
            m_canvas->setTextSize(1);
            m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
            m_canvas->setTextDatum(TL_DATUM);
            m_canvas->drawString(m_chains[i].name, x + 18, y + 2);
            m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
            m_canvas->drawString(m_chains[i].description, x + 18, y + 11);
            y += 22;
        }
    }

    // Scroll indicator if needed
    if (totalItems > 4) {
        m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
        m_canvas->setTextDatum(BR_DATUM);
        char scrollStr[32];
        snprintf(scrollStr, sizeof(scrollStr), "%d/%d", m_actionIndex + 1, totalItems);
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

    int16_t boxW = 200;
    int16_t boxH = 70;
    int16_t boxX = (Theme::SCREEN_WIDTH - boxW) / 2;
    int16_t boxY = (Theme::SCREEN_HEIGHT - boxH) / 2;

    m_canvas->fillRoundRect(boxX, boxY, boxW, boxH, 4, Theme::COLOR_SURFACE);
    m_canvas->drawRoundRect(boxX, boxY, boxW, boxH, 4, m_confirmingStop ? Theme::COLOR_WARNING : Theme::COLOR_DANGER);

    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(MC_DATUM);

    if (m_confirmingStop) {
        m_canvas->setTextColor(Theme::COLOR_WARNING);
        m_canvas->drawString("STOP ATTACK?", centerX, centerY - 20);
        m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
        m_canvas->drawString("Attack is still running", centerX, centerY - 5);
    } else {
        m_canvas->setTextColor(Theme::COLOR_DANGER);
        m_canvas->drawString("CONFIRM ATTACK?", centerX, centerY - 20);
        m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
        m_canvas->drawString(m_actions[m_actionIndex].label, centerX, centerY - 5);
    }

    m_canvas->setTextColor(Theme::COLOR_TEXT_MUTED);
    m_canvas->drawString(m_confirmingStop ? "Press Enter to stop" : "This may disrupt network traffic", centerX, centerY + 10);

    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString("[ENTER] Yes  [Q] No", centerX, centerY + 25);
}

void TargetDetail::renderExecuting() {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(MC_DATUM);

    // Chain progress (if active)
    if (m_chainActive) {
        char chainStr[48];
        snprintf(chainStr, sizeof(chainStr), "Step %d/%d: %s",
                 m_chainStep + 1, m_chainTotalSteps, m_chains[m_chainIndex].name);
        m_canvas->setTextColor(Theme::COLOR_WARNING);
        m_canvas->drawString(chainStr, centerX, centerY - 42);
    }

    // Status text
    m_canvas->setTextColor(Theme::COLOR_ACCENT);
    m_canvas->drawString(m_progress.statusText[0] ? m_progress.statusText : "Running...",
                         centerX, centerY - 30);

    // Progress bar
    int16_t barW = 160;
    int16_t barH = 6;
    int16_t barX = (Theme::SCREEN_WIDTH - barW) / 2;
    int16_t barY = centerY - 16;
    m_canvas->drawRect(barX, barY, barW, barH, Theme::COLOR_SURFACE_RAISED);
    uint32_t estTimeout = 30000;
    if (m_progress.elapsedMs > 0) {
        uint8_t pct = (uint8_t)min((uint32_t)95, m_progress.elapsedMs * 100 / estTimeout);
        int16_t fillW = (barW - 2) * pct / 100;
        m_canvas->fillRect(barX + 1, barY + 1, fillW, barH - 2, Theme::COLOR_ACCENT);
    }

    // Packets sent
    char pktStr[32];
    snprintf(pktStr, sizeof(pktStr), "Packets: %u", (unsigned int)m_progress.packetsSent);
    m_canvas->setTextColor(Theme::COLOR_TEXT_PRIMARY);
    m_canvas->drawString(pktStr, centerX, centerY + 2);

    // Elapsed time
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "Time: %us", (unsigned int)(m_progress.elapsedMs / 1000));
    m_canvas->drawString(timeStr, centerX, centerY + 16);

    // Stop hint
    m_canvas->setTextColor(Theme::COLOR_WARNING);
    m_canvas->setTextDatum(BC_DATUM);
    m_canvas->drawString("[Q] Stop Attack", centerX, Theme::SCREEN_HEIGHT - 2);
}

void TargetDetail::renderResult() {
    int16_t centerX = Theme::SCREEN_WIDTH / 2;
    int16_t centerY = Theme::SCREEN_HEIGHT / 2;

    uint16_t color = (m_result == ActionResult::SUCCESS) ? Theme::COLOR_SUCCESS :
                     (m_result == ActionResult::STOPPED) ? Theme::COLOR_ACCENT :
                     Theme::COLOR_DANGER;

    m_canvas->setTextSize(1);
    m_canvas->setTextDatum(MC_DATUM);

    // Result status
    const char* statusLabel = (m_result == ActionResult::SUCCESS) ? "SUCCESS" :
                              (m_result == ActionResult::STOPPED) ? "STOPPED" :
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
// STATE TRANSITIONS
// =============================================================================

void TargetDetail::transitionTo(DetailViewState newState) {
    if (m_state == newState) return;
    
    // Cleanup/Reset on entry to certain states
    if (newState == DetailViewState::INFO) {
        m_selectedClientIndex = -1;
        m_hasSelectedClient = false;
    }
    
    m_state = newState;
}

} // namespace Vanguard
