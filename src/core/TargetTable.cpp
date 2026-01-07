/**
 * @file TargetTable.cpp
 * @brief Target state management implementation
 */

#include "TargetTable.h"
#include <algorithm>

namespace Assessor {

TargetTable::TargetTable()
    : m_onAdded(nullptr)
    , m_onUpdated(nullptr)
    , m_onRemoved(nullptr)
{
    m_targets.reserve(MAX_TARGETS);
}

// =============================================================================
// TARGET MANAGEMENT
// =============================================================================

bool TargetTable::addOrUpdate(const Target& target) {
    int idx = findIndex(target.bssid);

    if (idx >= 0) {
        // Update existing
        Target& existing = m_targets[idx];
        existing.rssi = target.rssi;
        existing.lastSeenMs = target.lastSeenMs;
        existing.beaconCount++;

        // Update client count if provided
        if (target.clientCount > 0) {
            existing.clientCount = target.clientCount;
        }

        if (m_onUpdated) {
            m_onUpdated(existing);
        }
        return false;  // Not new
    }

    // Add new target
    if (m_targets.size() >= MAX_TARGETS) {
        // Remove weakest signal to make room
        auto weakest = std::min_element(m_targets.begin(), m_targets.end(),
            [](const Target& a, const Target& b) {
                return a.rssi < b.rssi;
            });
        if (weakest != m_targets.end() && weakest->rssi < target.rssi) {
            if (m_onRemoved) {
                m_onRemoved(*weakest);
            }
            m_targets.erase(weakest);
        } else {
            return false;  // Can't add, weaker than all existing
        }
    }

    m_targets.push_back(target);

    if (m_onAdded) {
        m_onAdded(target);
    }
    return true;  // New target added
}

const Target* TargetTable::findByBssid(const uint8_t* bssid) const {
    int idx = findIndex(bssid);
    return (idx >= 0) ? &m_targets[idx] : nullptr;
}

size_t TargetTable::pruneStale(uint32_t now) {
    size_t removed = 0;

    auto it = m_targets.begin();
    while (it != m_targets.end()) {
        if (it->isStale(now)) {
            if (m_onRemoved) {
                m_onRemoved(*it);
            }
            it = m_targets.erase(it);
            removed++;
        } else {
            ++it;
        }
    }

    return removed;
}

void TargetTable::clear() {
    m_targets.clear();
}

// =============================================================================
// QUERIES
// =============================================================================

const std::vector<Target>& TargetTable::getAll() const {
    return m_targets;
}

std::vector<Target> TargetTable::getFiltered(const TargetFilter& filter,
                                              SortOrder order) const {
    std::vector<Target> result;
    result.reserve(m_targets.size());

    // Apply filters
    for (const auto& t : m_targets) {
        // Type filter
        if (t.type == TargetType::ACCESS_POINT && !filter.showAccessPoints) continue;
        if (t.type == TargetType::STATION && !filter.showStations) continue;
        if (t.type == TargetType::BLE_DEVICE && !filter.showBLE) continue;

        // Hidden filter
        if (t.isHidden && !filter.showHidden) continue;

        // Security filter
        if (t.isOpen() && !filter.showOpen) continue;
        if (!t.isOpen() && !filter.showSecured) continue;

        // Signal filter
        if (t.rssi < filter.minRssi) continue;

        result.push_back(t);
    }

    // Sort
    switch (order) {
        case SortOrder::SIGNAL_STRENGTH:
            std::sort(result.begin(), result.end(),
                [](const Target& a, const Target& b) {
                    return a.rssi > b.rssi;  // Strongest first
                });
            break;

        case SortOrder::ALPHABETICAL:
            std::sort(result.begin(), result.end(),
                [](const Target& a, const Target& b) {
                    return strcmp(a.ssid, b.ssid) < 0;
                });
            break;

        case SortOrder::LAST_SEEN:
            std::sort(result.begin(), result.end(),
                [](const Target& a, const Target& b) {
                    return a.lastSeenMs > b.lastSeenMs;  // Most recent first
                });
            break;

        case SortOrder::CLIENT_COUNT:
            std::sort(result.begin(), result.end(),
                [](const Target& a, const Target& b) {
                    return a.clientCount > b.clientCount;  // Most clients first
                });
            break;

        case SortOrder::TYPE:
            std::sort(result.begin(), result.end(),
                [](const Target& a, const Target& b) {
                    return static_cast<int>(a.type) < static_cast<int>(b.type);
                });
            break;
    }

    return result;
}

size_t TargetTable::count() const {
    return m_targets.size();
}

size_t TargetTable::countByType(TargetType type) const {
    return std::count_if(m_targets.begin(), m_targets.end(),
        [type](const Target& t) { return t.type == type; });
}

const Target* TargetTable::getStrongest() const {
    if (m_targets.empty()) return nullptr;

    auto strongest = std::max_element(m_targets.begin(), m_targets.end(),
        [](const Target& a, const Target& b) {
            return a.rssi < b.rssi;
        });

    return &(*strongest);
}

// =============================================================================
// CALLBACKS
// =============================================================================

void TargetTable::onTargetAdded(TargetAddedCallback cb) {
    m_onAdded = cb;
}

void TargetTable::onTargetUpdated(TargetUpdatedCallback cb) {
    m_onUpdated = cb;
}

void TargetTable::onTargetRemoved(TargetRemovedCallback cb) {
    m_onRemoved = cb;
}

// =============================================================================
// PRIVATE
// =============================================================================

int TargetTable::findIndex(const uint8_t* bssid) const {
    for (size_t i = 0; i < m_targets.size(); i++) {
        if (memcmp(m_targets[i].bssid, bssid, 6) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace Assessor
