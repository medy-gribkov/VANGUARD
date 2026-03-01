/**
 * @file ActionResolver.cpp
 * @brief Context-aware action filtering - the heart of target-first philosophy
 */

#include "ActionResolver.h"
#include <algorithm>

namespace Vanguard {

// =============================================================================
// ACTION DEFINITIONS
// =============================================================================

const ActionResolver::ActionDefinition ActionResolver::s_actionDefs[] = {
    // WiFi AP Actions - sorted by priority in definition, but getActionsFor() also sorts
    {ActionType::DEAUTH_ALL,        "Deauth All",        "Disconnect all clients",       true,  TargetType::ACCESS_POINT, true,  SecurityType::UNKNOWN,  10},
    {ActionType::DEAUTH_SINGLE,     "Deauth One",        "Disconnect specific client",   true,  TargetType::ACCESS_POINT, true,  SecurityType::UNKNOWN,  10},
    {ActionType::EVIL_TWIN,         "Evil Twin",         "Fake AP with captive portal",  true,  TargetType::ACCESS_POINT, false, SecurityType::UNKNOWN,  20},
    {ActionType::BEACON_FLOOD,      "Clone Beacon",      "Spam copies of this network",  true,  TargetType::ACCESS_POINT, false, SecurityType::UNKNOWN,  20},
    {ActionType::CAPTURE_HANDSHAKE, "Capture Handshake", "4-way WPA handshake",          false, TargetType::ACCESS_POINT, true,  SecurityType::OPEN,     30},
    {ActionType::CAPTURE_PMKID,     "Capture PMKID",     "Grab hash for offline crack",  false, TargetType::ACCESS_POINT, false, SecurityType::WPA3_SAE, 30},
    {ActionType::MONITOR,           "Monitor",           "Passively capture packets",    false, TargetType::ACCESS_POINT, false, SecurityType::UNKNOWN,  40},
    {ActionType::PROBE_FLOOD,       "Probe Flood",       "Flood channel with fake probes", true, TargetType::ACCESS_POINT, false, SecurityType::UNKNOWN, 40},

    // BLE Actions
    {ActionType::BLE_SPAM,          "BLE Spam",          "Flood with pairing requests",  true,  TargetType::BLE_DEVICE, false, SecurityType::UNKNOWN, 35},
    {ActionType::BLE_SOUR_APPLE,    "Sour Apple",        "Apple device disruption",      true,  TargetType::BLE_DEVICE, false, SecurityType::UNKNOWN, 35},
    {ActionType::BLE_SKIMMER_DETECT,"Skimmer Detect",    "Scan for suspicious devices",  false, TargetType::BLE_DEVICE, false, SecurityType::UNKNOWN, 50},

    // IR Actions
    {ActionType::IR_REPLAY,         "IR Replay",         "Record and replay IR signal",  false, TargetType::IR_DEVICE, false, SecurityType::UNKNOWN, 50},
    {ActionType::IR_TVBGONE,        "TV-B-Gone",         "Power cycle nearby TVs",       true,  TargetType::IR_DEVICE, false, SecurityType::UNKNOWN, 50},
};

const size_t ActionResolver::s_actionDefCount =
    sizeof(s_actionDefs) / sizeof(s_actionDefs[0]);

// =============================================================================
// IMPLEMENTATION
// =============================================================================

ActionResolver::ActionResolver() {}

std::vector<AvailableAction> ActionResolver::getActionsFor(const Target& target) const {
    std::vector<AvailableAction> result;

    for (size_t i = 0; i < s_actionDefCount; i++) {
        const ActionDefinition& def = s_actionDefs[i];

        // Only show implemented actions
        if (!isImplemented(def.type)) continue;

        // Must match target type (skip entirely if wrong type)
        if (!checkTargetType(target, def.requiredTargetType)) continue;

        AvailableAction action;
        action.type = def.type;
        action.label = def.label;
        action.description = def.description;
        action.isDestructive = def.isDestructive;
        action.requiresClients = def.requiresClients;
        action.priority = def.priority;
        action.enabled = true;
        action.disabledReason = nullptr;

        // Check each requirement. If failed, disable instead of hiding.
        if (!check5GHzCompatibility(target, def.type)) {
            action.enabled = false;
            action.disabledReason = "5GHz not supported";
            action.priority = 99;
        } else if (!checkClientRequirement(target, def.requiresClients)) {
            action.enabled = false;
            action.disabledReason = "No clients found";
            action.priority = 99;
        } else if (!checkSecurityCompatibility(target, def.incompatibleSecurity)) {
            action.enabled = false;
            action.disabledReason = "Wrong security type";
            action.priority = 99;
        }

        result.push_back(action);
    }

    // Sort: enabled actions first by priority, disabled at end
    std::sort(result.begin(), result.end(), [](const AvailableAction& a, const AvailableAction& b) {
        if (a.enabled != b.enabled) return a.enabled;
        return a.priority < b.priority;
    });

    return result;
}

bool ActionResolver::isActionValid(const Target& target, ActionType action) const {
    for (size_t i = 0; i < s_actionDefCount; i++) {
        const ActionDefinition& def = s_actionDefs[i];

        if (def.type != action) continue;

        return checkTargetType(target, def.requiredTargetType)
            && checkClientRequirement(target, def.requiresClients)
            && checkSecurityCompatibility(target, def.incompatibleSecurity);
    }

    return false;
}

const char* ActionResolver::getInvalidReason(const Target& target, ActionType action) const {
    for (size_t i = 0; i < s_actionDefCount; i++) {
        const ActionDefinition& def = s_actionDefs[i];

        if (def.type != action) continue;

        if (!checkTargetType(target, def.requiredTargetType)) {
            return "Wrong target type";
        }

        if (!checkClientRequirement(target, def.requiresClients)) {
            return "No clients connected";
        }

        if (!checkSecurityCompatibility(target, def.incompatibleSecurity)) {
            return "Incompatible security type";
        }

        return nullptr;  // Action is valid
    }

    return "Unknown action";
}

// =============================================================================
// VALIDATION HELPERS
// =============================================================================

bool ActionResolver::checkTargetType(const Target& target, TargetType required) const {
    if (required == TargetType::UNKNOWN) {
        return true;  // Any type accepted
    }
    return target.type == required;
}

bool ActionResolver::checkClientRequirement(const Target& target, bool requiresClients) const {
    if (!requiresClients) {
        return true;  // No requirement
    }
    return target.hasClients();
}

bool ActionResolver::checkSecurityCompatibility(const Target& target, SecurityType incompatible) const {
    if (incompatible == SecurityType::UNKNOWN) {
        return true;  // Compatible with all
    }
    return target.security != incompatible;
}

// [DORMANT] 5GHz requires external add-on hardware (ESP32-S3 is 2.4GHz only)
bool ActionResolver::check5GHzCompatibility(const Target& target, ActionType action) const {
    // BLE and IR actions don't care about WiFi channel
    if (action == ActionType::BLE_SPAM ||
        action == ActionType::BLE_SOUR_APPLE ||
        action == ActionType::IR_REPLAY ||
        action == ActionType::IR_TVBGONE) {
        return true;
    }

    // [DORMANT] 5GHz requires external add-on hardware (ESP32-S3 is 2.4GHz only)
    // WiFi attacks can only work on 2.4GHz (channels 1-14)
    if (target.channel > 14) {
        return false;  // 5GHz target, can't attack
    }

    return true;
}

bool ActionResolver::isImplemented(ActionType action) const {
    // Only return true for actions that are FULLY implemented
    // and wired up in VanguardEngine::executeAction()
    switch (action) {
        // WORKING - fully implemented
        case ActionType::DEAUTH_ALL:
        case ActionType::DEAUTH_SINGLE:
        case ActionType::BEACON_FLOOD:
        case ActionType::EVIL_TWIN:          // Basic soft AP implemented
        case ActionType::BLE_SPAM:
        case ActionType::BLE_SOUR_APPLE:
        case ActionType::CAPTURE_HANDSHAKE:  // Now fully implemented
        case ActionType::IR_REPLAY:
        case ActionType::IR_TVBGONE:
            return true;

        case ActionType::MONITOR:
        case ActionType::CAPTURE_PMKID:
        case ActionType::PROBE_FLOOD:
        case ActionType::BLE_SKIMMER_DETECT:
            return true;

        // NOT IMPLEMENTED - don't show to user
        default:
            return false;
    }
}

// =============================================================================
// ATTACK CHAINS
// =============================================================================

std::vector<ActionChain> ActionResolver::getChainsFor(const Target& target) const {
    std::vector<ActionChain> chains;

    if (target.type == TargetType::ACCESS_POINT && target.channel <= 14) {
        // Full Capture: deauth to force reconnect, then capture handshake
        if (target.hasClients() && target.security != SecurityType::OPEN) {
            ActionChain capture = {"Full Capture", "Deauth + Handshake",
                {ActionType::DEAUTH_ALL, ActionType::CAPTURE_HANDSHAKE}, 2};
            chains.push_back(capture);
        }

        // Recon: passive monitoring
        ActionChain recon = {"Recon", "Monitor + Probe scan",
            {ActionType::MONITOR, ActionType::PROBE_FLOOD}, 2};
        chains.push_back(recon);

        // Disruption: knock off + confuse
        if (target.hasClients()) {
            ActionChain disrupt = {"Disruption", "Deauth + Beacon flood",
                {ActionType::DEAUTH_ALL, ActionType::BEACON_FLOOD}, 2};
            chains.push_back(disrupt);
        }
    }

    return chains;
}

} // namespace Vanguard
