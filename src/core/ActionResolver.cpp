/**
 * @file ActionResolver.cpp
 * @brief Context-aware action filtering - the heart of target-first philosophy
 */

#include "ActionResolver.h"

namespace Assessor {

// =============================================================================
// ACTION DEFINITIONS
// =============================================================================

const ActionResolver::ActionDefinition ActionResolver::s_actionDefs[] = {
    // WiFi AP Actions
    {
        ActionType::MONITOR,
        "Monitor",
        "Passively capture packets",
        false,
        TargetType::ACCESS_POINT,
        false, false,
        SecurityType::UNKNOWN
    },
    {
        ActionType::DEAUTH_ALL,
        "Deauth All",
        "Disconnect all clients",
        true,
        TargetType::ACCESS_POINT,
        true,   // Requires clients!
        false,
        SecurityType::UNKNOWN
    },
    {
        ActionType::DEAUTH_SINGLE,
        "Deauth One",
        "Disconnect specific client",
        true,
        TargetType::ACCESS_POINT,
        true,   // Requires clients!
        false,
        SecurityType::UNKNOWN
    },
    {
        ActionType::BEACON_FLOOD,
        "Clone Beacon",
        "Spam copies of this network",
        true,
        TargetType::ACCESS_POINT,
        false, false,
        SecurityType::UNKNOWN
    },
    {
        ActionType::EVIL_TWIN,
        "Evil Twin",
        "Fake AP with captive portal",
        true,
        TargetType::ACCESS_POINT,
        false, false,
        SecurityType::UNKNOWN
    },
    {
        ActionType::CAPTURE_PMKID,
        "Capture PMKID",
        "Grab hash for offline crack",
        false,
        TargetType::ACCESS_POINT,
        false, false,
        SecurityType::WPA3_SAE  // Incompatible with WPA3
    },
    {
        ActionType::CAPTURE_HANDSHAKE,
        "Capture Handshake",
        "4-way WPA handshake",
        false,
        TargetType::ACCESS_POINT,
        true,   // Need client to reconnect
        false,
        SecurityType::OPEN  // Incompatible with Open
    },

    // BLE Actions
    {
        ActionType::BLE_SPAM,
        "BLE Spam",
        "Flood with pairing requests",
        true,
        TargetType::BLE_DEVICE,
        false, false,
        SecurityType::UNKNOWN
    },
    {
        ActionType::BLE_SOUR_APPLE,
        "Sour Apple",
        "Apple device disruption",
        true,
        TargetType::BLE_DEVICE,
        false, false,
        SecurityType::UNKNOWN
    },
    {
        ActionType::BLE_SKIMMER_DETECT,
        "Skimmer Check",
        "Analyze for suspicious traits",
        false,
        TargetType::BLE_DEVICE,
        false, false,
        SecurityType::UNKNOWN
    }
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

        // CRITICAL: Only show actions that are actually implemented
        if (!isImplemented(def.type)) {
            continue;
        }

        // Check target type
        if (!checkTargetType(target, def.requiredTargetType)) {
            continue;
        }

        // Check 5GHz - can't do WiFi attacks on 5GHz networks
        if (!check5GHzCompatibility(target, def.type)) {
            continue;
        }

        // Check client requirement
        if (!checkClientRequirement(target, def.requiresClients)) {
            continue;
        }

        // Check security compatibility
        if (!checkSecurityCompatibility(target, def.incompatibleSecurity)) {
            continue;
        }

        // This action is valid!
        AvailableAction action;
        action.type = def.type;
        action.label = def.label;
        action.description = def.description;
        action.isDestructive = def.isDestructive;
        action.requiresClients = def.requiresClients;

        result.push_back(action);
    }

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

bool ActionResolver::check5GHzCompatibility(const Target& target, ActionType action) const {
    // BLE actions don't care about WiFi channel
    if (action == ActionType::BLE_SPAM ||
        action == ActionType::BLE_SOUR_APPLE ||
        action == ActionType::BLE_SKIMMER_DETECT) {
        return true;
    }

    // WiFi attacks can only work on 2.4GHz (channels 1-14)
    // ESP32 cannot transmit on 5GHz bands
    if (target.channel > 14) {
        return false;  // 5GHz target, can't attack
    }

    return true;
}

bool ActionResolver::isImplemented(ActionType action) const {
    // Only return true for actions that are FULLY implemented
    // and wired up in AssessorEngine::executeAction()
    switch (action) {
        // WORKING - fully implemented
        case ActionType::DEAUTH_ALL:
        case ActionType::DEAUTH_SINGLE:
        case ActionType::BEACON_FLOOD:
        case ActionType::EVIL_TWIN:          // Basic soft AP implemented
        case ActionType::BLE_SPAM:
        case ActionType::BLE_SOUR_APPLE:
            return true;

        // NOT IMPLEMENTED - don't show to user
        case ActionType::MONITOR:            // Not routed
        case ActionType::CAPTURE_PMKID:      // Not routed
        case ActionType::CAPTURE_HANDSHAKE:  // Partial, detection missing
        case ActionType::PROBE_FLOOD:        // Not implemented
        case ActionType::BLE_SKIMMER_DETECT: // Not implemented
        default:
            return false;
    }
}

} // namespace Assessor
