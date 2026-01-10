#ifndef VANGUARD_IPC_H
#define VANGUARD_IPC_H

#include <Arduino.h>
#include "VanguardTypes.h"

namespace Vanguard {

// =============================================================================
// COMMANDS (UI -> System)
// =============================================================================

enum class SysCommand : uint8_t {
    NONE,
    
    // WiFi Scanning
    WIFI_SCAN_START,      // Payload: Scan config (optional)
    WIFI_SCAN_STOP,
    
    // BLE Scanning
    BLE_SCAN_START,       // Payload: uint32_t duration_ms
    BLE_SCAN_STOP,
    
    // Attacks / Actions
    ACTION_START,         // Payload: ActionRequest*
    ACTION_STOP,
    
    // System
    SYSTEM_SHUTDOWN
};

struct ActionRequest {
    ActionType type;
    Target target;
    // Additional parameters can be added here
};

struct SystemRequest {
    SysCommand cmd;
    void* payload;       // Command specific data (must be heap allocated or static)
    
    // Helper to free payload if needed
    void (*freeCb)(void*); 
};


// =============================================================================
// EVENTS (System -> UI)
// =============================================================================

enum class SysEventType : uint8_t {
    NONE,
    
    // WiFi Status
    WIFI_SCAN_STARTED,
    WIFI_SCAN_COMPLETE,   // Payload: int count
    ASSOCIATION_FOUND,    // Payload: AssociationEvent*

    
    // BLE Status
    BLE_SCAN_STARTED,
    BLE_SCAN_COMPLETE,    // Payload: int count
    BLE_DEVICE_FOUND,     // Payload: BLEDeviceInfo* (copy)
    
    // Action Status
    ACTION_PROGRESS,      // Payload: ActionProgress*
    ACTION_COMPLETE,      // Payload: ActionResult
    
    // Errors
    ERROR_OCCURRED        // Payload: char* message
};

struct AssociationEvent {
    uint8_t bssid[6];
    uint8_t station[6];
};

struct SystemEvent {
    SysEventType type;
    void* data;          // Event specific data
    size_t dataLen;
    
    // Helper to identify primitive data vs pointers
    bool isPointer; 
};

} // namespace Vanguard

#endif // VANGUARD_IPC_H
