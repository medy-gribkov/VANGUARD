#ifndef ASSESSOR_ENGINE_H
#define ASSESSOR_ENGINE_H

/**
 * @file AssessorEngine.h
 * @brief The orchestrator - connects scanning, targets, and actions
 *
 * AssessorEngine is the central coordinator. It doesn't do the work itself;
 * it delegates to TargetTable, ActionResolver, and the Bruce adapters.
 *
 * Responsibilities:
 * - Manage scan lifecycle (start, progress, complete)
 * - Route action requests to appropriate adapter
 * - Maintain global state (are we scanning? attacking?)
 * - Provide unified API for the UI layer
 *
 * @example
 * auto& engine = AssessorEngine::getInstance();
 * engine.beginScan();
 * // ... in loop() ...
 * engine.tick();
 * if (engine.getScanState() == ScanState::COMPLETE) {
 *     for (const auto& target : engine.getTargets()) {
 *         // display target
 *     }
 * }
 */

#include "Types.h"
#include "TargetTable.h"
#include "ActionResolver.h"
#include <functional>

namespace Assessor {

// Forward declarations for adapters
class BruceWiFiAdapter;
class BruceBLEAdapter;

/**
 * @brief Callback for scan progress updates
 */
using ScanProgressCallback = std::function<void(ScanState state, uint8_t percentComplete)>;

/**
 * @brief Callback for action progress updates
 */
using ActionProgressCallback = std::function<void(const ActionProgress& progress)>;

// =============================================================================
// AssessorEngine Class
// =============================================================================

class AssessorEngine {
public:
    /**
     * @brief Get singleton instance
     * Singleton because there's only one radio/hardware set.
     */
    static AssessorEngine& getInstance();

    // Prevent copying
    AssessorEngine(const AssessorEngine&) = delete;
    AssessorEngine& operator=(const AssessorEngine&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Initialize the engine and all adapters
     * Call once in setup(). Returns false if hardware init fails.
     */
    bool init();

    /**
     * @brief Shutdown cleanly
     */
    void shutdown();

    /**
     * @brief Non-blocking tick - MUST be called every loop()
     */
    void tick();

    // -------------------------------------------------------------------------
    // Scanning
    // -------------------------------------------------------------------------

    /**
     * @brief Start a full scan (WiFi + BLE)
     * Non-blocking. WiFi scans first, then BLE automatically chains.
     * Check getScanState() for progress.
     */
    void beginScan();

    /**
     * @brief Start WiFi-only scan
     */
    void beginWiFiScan();

    /**
     * @brief Start BLE-only scan
     */
    void beginBLEScan();

    /**
     * @brief Check if this is a combined scan that will include BLE
     */
    bool isCombinedScan() const;

    /**
     * @brief Stop any active scan
     */
    void stopScan();

    /**
     * @brief Get current scan state
     */
    ScanState getScanState() const;

    /**
     * @brief Get scan progress (0-100)
     */
    uint8_t getScanProgress() const;

    /**
     * @brief Register callback for scan updates
     */
    void onScanProgress(ScanProgressCallback cb);

    // -------------------------------------------------------------------------
    // Targets
    // -------------------------------------------------------------------------

    /**
     * @brief Get all discovered targets
     */
    const std::vector<Target>& getTargets() const;

    /**
     * @brief Get target count
     */
    size_t getTargetCount() const;

    /**
     * @brief Get targets filtered and sorted
     */
    std::vector<Target> getFilteredTargets(const TargetFilter& filter,
                                            SortOrder order) const;

    /**
     * @brief Find target by BSSID
     */
    const Target* findTarget(const uint8_t* bssid) const;

    /**
     * @brief Clear all targets
     */
    void clearTargets();

    // -------------------------------------------------------------------------
    // Actions
    // -------------------------------------------------------------------------

    /**
     * @brief Get available actions for a target
     * Delegates to ActionResolver for context-aware filtering.
     */
    std::vector<AvailableAction> getActionsFor(const Target& target) const;

    /**
     * @brief Execute an action on a target
     * Non-blocking. Check getActionProgress() for status.
     *
     * @param action The action type to perform
     * @param target The target to act upon
     * @return true if action started successfully
     */
    bool executeAction(ActionType action, const Target& target);

    /**
     * @brief Stop any active action
     */
    void stopAction();

    /**
     * @brief Check if an action is currently running
     */
    bool isActionActive() const;

    /**
     * @brief Get progress of current action
     */
    ActionProgress getActionProgress() const;

    /**
     * @brief Register callback for action updates
     */
    void onActionProgress(ActionProgressCallback cb);

    // -------------------------------------------------------------------------
    // Hardware Status
    // -------------------------------------------------------------------------

    /**
     * @brief Check if WiFi hardware is available
     */
    bool hasWiFi() const;

    /**
     * @brief Check if BLE hardware is available
     */
    bool hasBLE() const;

    /**
     * @brief Check if RF hardware is available (sub-GHz)
     */
    bool hasRF() const;

    /**
     * @brief Check if IR hardware is available
     */
    bool hasIR() const;

private:
    AssessorEngine();
    ~AssessorEngine();

    // State
    bool           m_initialized;
    ScanState      m_scanState;
    uint8_t        m_scanProgress;
    bool           m_actionActive;
    ActionProgress m_actionProgress;
    bool           m_combinedScan;  // true if BLE should chain after WiFi

    // Components
    TargetTable    m_targetTable;
    ActionResolver m_actionResolver;

    // Adapters (lazy-initialized)
    // BruceWiFiAdapter* m_wifiAdapter;
    // BruceBLEAdapter*  m_bleAdapter;

    // Callbacks
    ScanProgressCallback   m_onScanProgress;
    ActionProgressCallback m_onActionProgress;

    // Timing
    uint32_t m_scanStartMs;
    uint32_t m_actionStartMs;

    // Internal tick handlers
    void tickScan();
    void tickAction();
    void processScanResults(int count);
    void processBLEScanResults();
};

} // namespace Assessor

#endif // ASSESSOR_ENGINE_H
