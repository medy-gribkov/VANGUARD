#ifndef VANGUARD_TARGET_TABLE_H
#define VANGUARD_TARGET_TABLE_H

/**
 * @file TargetTable.h
 * @brief Manages the collection of discovered targets
 *
 * The TargetTable is the "State of the World" - it knows everything
 * about every target we've seen. The UI queries it for display,
 * the Engine updates it during scans.
 *
 * @example
 * TargetTable table;
 * table.addOrUpdate(scannedTarget);
 * for (const auto& t : table.getTargets()) {
 *     Serial.println(t.ssid);
 * }
 */

#include "VanguardTypes.h"
#include <vector>
#include <functional>

namespace Vanguard {

/**
 * @brief Sort criteria for target list
 */
enum class SortOrder : uint8_t {
    SIGNAL_STRENGTH,  // Strongest first (default)
    ALPHABETICAL,     // A-Z by SSID
    LAST_SEEN,        // Most recent first
    CLIENT_COUNT,     // Most clients first
    TYPE              // APs, then Stations, then BLE
};

/**
 * @brief Filter criteria for target list
 */
struct TargetFilter {
    bool showAccessPoints  = true;
    bool showStations      = true;
    bool showBLE           = true;
    bool showHidden        = true;
    bool showOpen          = true;
    bool showSecured       = true;
    int8_t minRssi         = -100;  // Show all by default
};

/**
 * @brief Callback when a new target is discovered
 */
using TargetAddedCallback = std::function<void(const Target&)>;

/**
 * @brief Callback when a target is updated
 */
using TargetUpdatedCallback = std::function<void(const Target&)>;

/**
 * @brief Callback when a target is removed (stale)
 */
using TargetRemovedCallback = std::function<void(const Target&)>;

// =============================================================================
// TargetTable Class
// =============================================================================

class TargetTable {
public:
    TargetTable();
    ~TargetTable() = default;

    // Prevent copying (single source of truth)
    TargetTable(const TargetTable&) = delete;
    TargetTable& operator=(const TargetTable&) = delete;

    // -------------------------------------------------------------------------
    // Target Management
    // -------------------------------------------------------------------------

    /**
     * @brief Add a new target or update if BSSID exists
     * @param target The target to add/update
     * @return true if new target added, false if updated existing
     */
    bool addOrUpdate(const Target& target);

    /**
     * @brief Find a target by BSSID
     * @param bssid 6-byte MAC address
     * @return Pointer to target, or nullptr if not found
     */
    const Target* findByBssid(const uint8_t* bssid) const;

    /**
     * @brief Remove targets not seen within timeout
     * @param now Current millis()
     * @return Number of targets removed
     */
    size_t pruneStale(uint32_t now);

    /**
     * @brief Add a virtual/static target (e.g. Universal Remote)
     */
    bool addVirtualTarget(const char* name, TargetType type);

    /**
     * @brief Associate a client MAC with an Access Point MAC
     * @param clientMac Client's MAC address
     * @param apMac AP's BSSID
     * @return true if association was new, false if already tracked
     */
    bool addAssociation(const uint8_t* clientMac, const uint8_t* apMac);

    /**
     * @brief Clear all targets
     */
    void clear();

    // -------------------------------------------------------------------------
    // Queries
    // -------------------------------------------------------------------------

    /**
     * @brief Get all targets (unfiltered, unsorted)
     */
    const std::vector<Target>& getAll() const;

    /**
     * @brief Get filtered and sorted targets
     * @param filter Which targets to include
     * @param order How to sort results
     * @return Filtered/sorted copy of targets
     */
    std::vector<Target> getFiltered(const TargetFilter& filter,
                                     SortOrder order = SortOrder::SIGNAL_STRENGTH) const;

    /**
     * @brief Get target count
     */
    size_t count() const;

    /**
     * @brief Count targets by type
     */
    size_t countByType(TargetType type) const;

    /**
     * @brief Get strongest signal target
     */
    const Target* getStrongest() const;

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    void onTargetAdded(TargetAddedCallback cb);
    void onTargetUpdated(TargetUpdatedCallback cb);
    void onTargetRemoved(TargetRemovedCallback cb);

private:
    std::vector<Target> m_targets;

    TargetAddedCallback   m_onAdded;
    TargetUpdatedCallback m_onUpdated;
    TargetRemovedCallback m_onRemoved;

    /**
     * @brief Find target index by BSSID
     * @return Index or -1 if not found
     */
    int findIndex(const uint8_t* bssid) const;
};

} // namespace Vanguard

#endif // VANGUARD_TARGET_TABLE_H
