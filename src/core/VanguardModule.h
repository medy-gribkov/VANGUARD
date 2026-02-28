#ifndef VANGUARD_MODULE_H
#define VANGUARD_MODULE_H

#include <Arduino.h>

namespace Vanguard {

/**
 * @brief Base class for all hardware-abstracting modules (WiFi, BLE, IR, LoRa, etc.)
 */
class VanguardModule {
public:
    virtual ~VanguardModule() = default;

    /**
     * @brief Called when the module should initialize its hardware
     */
    virtual bool onEnable() = 0;

    /**
     * @brief Called when the module should release hardware resources
     */
    virtual void onDisable() = 0;

    /**
     * @brief Periodic update call
     */
    virtual void onTick() {}

    /**
     * @brief Get human-readable module name
     */
    virtual const char* getName() const = 0;

    /**
     * @brief Check if module is currently enabled
     */
    bool isEnabled() const { return m_enabled; }

protected:
    volatile bool m_enabled = false;
};

} // namespace Vanguard

#endif // VANGUARD_MODULE_H
