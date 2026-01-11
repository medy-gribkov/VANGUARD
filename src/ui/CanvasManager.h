#ifndef VANGUARD_CANVAS_MANAGER_H
#define VANGUARD_CANVAS_MANAGER_H

#include <M5Cardputer.h>
#include "Theme.h"

namespace Vanguard {

/**
 * @class CanvasManager
 * @brief Singleton manager for a shared M5Canvas sprite.
 *
 * This reduces SRAM usage by sharing a single 240x135 16-bit sprite (~65KB)
 * across all UI screens instead of each screen owning its own.
 */
class CanvasManager {
public:
    static CanvasManager& getInstance() {
        static CanvasManager instance;
        return instance;
    }

    /**
     * @brief Get the shared canvas instance
     * @return Reference to the shared M5Canvas
     */
    M5Canvas& getCanvas() {
        if (!m_spriteCreated) {
            m_canvas.createSprite(Theme::SCREEN_WIDTH, Theme::SCREEN_HEIGHT);
            m_spriteCreated = true;
        }
        return m_canvas;
    }

    /**
     * @brief Delete the sprite to free memory (if needed)
     */
    void freeSprite() {
        if (m_spriteCreated) {
            m_canvas.deleteSprite();
            m_spriteCreated = false;
        }
    }

private:
    CanvasManager() : m_canvas(&M5Cardputer.Display), m_spriteCreated(false) {}
    ~CanvasManager() {
        freeSprite();
    }

    // Prevent copying
    CanvasManager(const CanvasManager&) = delete;
    CanvasManager& operator=(const CanvasManager&) = delete;

    M5Canvas m_canvas;
    bool     m_spriteCreated;
};

} // namespace Vanguard

#endif // VANGUARD_CANVAS_MANAGER_H
