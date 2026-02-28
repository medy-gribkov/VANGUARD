#ifndef VANGUARD_ACTION_RESOLVER_H
#define VANGUARD_ACTION_RESOLVER_H

/**
 * @file ActionResolver.h
 * @brief Determines which actions are valid for a given target
 *
 * This is the "brain" that makes VANGUARD special. Given a target's
 * current state, it returns ONLY the actions that make sense.
 *
 * Example logic:
 * - Target has 0 clients? Don't offer "Deauth" (nothing to deauth)
 * - Target is WPA3? Don't offer "Capture PMKID" (not vulnerable)
 * - Target is BLE? Only offer BLE actions
 *
 * @example
 * ActionResolver resolver;
 * auto actions = resolver.getActionsFor(wifiTarget);
 * for (const auto& action : actions) {
 *     menu.addItem(action.label, action.description);
 * }
 */

#include "VanguardTypes.h"
#include <vector>

namespace Vanguard {

class ActionResolver {
public:
    ActionResolver();
    ~ActionResolver() = default;

    /**
     * @brief Get all valid actions for a target
     * @param target The target to evaluate
     * @return List of available actions (empty if none apply)
     */
    std::vector<AvailableAction> getActionsFor(const Target& target) const;

    /**
     * @brief Check if a specific action is valid for a target
     * @param target The target to check
     * @param action The action type to validate
     * @return true if action can be performed
     */
    bool isActionValid(const Target& target, ActionType action) const;

    /**
     * @brief Get human-readable reason why an action is invalid
     * @param target The target
     * @param action The action type
     * @return Explanation string (e.g., "No clients connected")
     */
    const char* getInvalidReason(const Target& target, ActionType action) const;

private:
    // -------------------------------------------------------------------------
    // Action Definitions (static data)
    // -------------------------------------------------------------------------

    struct ActionDefinition {
        ActionType   type;
        const char*  label;
        const char*  description;
        bool         isDestructive;
        TargetType   requiredTargetType;  // UNKNOWN = any
        bool         requiresClients;
        SecurityType incompatibleSecurity; // UNKNOWN = compatible with all
    };

    static const ActionDefinition s_actionDefs[];
    static const size_t s_actionDefCount;

    // -------------------------------------------------------------------------
    // Validation Helpers
    // -------------------------------------------------------------------------

    bool checkTargetType(const Target& target, TargetType required) const;
    bool checkClientRequirement(const Target& target, bool requiresClients) const;
    bool checkSecurityCompatibility(const Target& target, SecurityType incompatible) const;
    bool check5GHzCompatibility(const Target& target, ActionType action) const;
    bool isImplemented(ActionType action) const;
};

} // namespace Vanguard

#endif // VANGUARD_ACTION_RESOLVER_H
