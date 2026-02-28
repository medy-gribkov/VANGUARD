#ifndef VANGUARD_BOOT_SEQUENCE_H
#define VANGUARD_BOOT_SEQUENCE_H

/**
 * @file BootSequence.h
 * @brief Animated boot splash and onboarding
 *
 * The BootSequence runs once at startup. It shows:
 * 1. Logo fade-in with tagline
 * 2. Brief "What is VANGUARD?" explainer (first run only)
 * 3. Transition to scanning state
 *
 * @example
 * BootSequence boot;
 * boot.begin();
 * // in loop():
 * boot.tick();
 * if (boot.isComplete()) {
 *     // transition to radar
 * }
 */

#include <M5Cardputer.h>
#include <cstdint>

namespace Vanguard {

/**
 * @brief Boot sequence animation states
 */
enum class BootPhase : uint8_t {
    LOGO_FADE_IN,
    TAGLINE_FADE_IN,
    HOLD,
    ONBOARDING,      // First-run only
    FADE_OUT,
    COMPLETE
};

class BootSequence {
public:
    BootSequence();
    ~BootSequence() = default;

    /**
     * @brief Start the boot sequence
     */
    void begin();

    /**
     * @brief Non-blocking tick - call every loop()
     */
    void tick();

    /**
     * @brief Check if boot sequence is finished
     */
    bool isComplete() const;

    /**
     * @brief Skip directly to complete (user pressed button)
     */
    void skip();

    /**
     * @brief Get current phase (for debugging)
     */
    BootPhase getPhase() const;

    /**
     * @brief Check if this is the first boot (show onboarding)
     */
    bool isFirstBoot() const;

    /**
     * @brief Mark onboarding as seen (won't show again)
     */
    void markOnboardingComplete();

private:
    BootPhase m_phase;
    uint32_t  m_phaseStartMs;
    uint32_t  m_lastFrameMs;
    bool      m_firstBoot;
    uint8_t   m_fadeLevel;       // 0-255 for animations

    // Phase durations (ms)
    static constexpr uint32_t LOGO_FADE_DURATION    = 300;
    static constexpr uint32_t TAGLINE_FADE_DURATION = 300;
    static constexpr uint32_t HOLD_DURATION         = 500;
    static constexpr uint32_t ONBOARDING_DURATION   = 1000;
    static constexpr uint32_t FADE_OUT_DURATION     = 200;

    // Rendering helpers
    void drawLogo(uint8_t alpha);
    void drawTagline(uint8_t alpha);

    void advancePhase();
    bool checkSkipInput();
};

} // namespace Vanguard

#endif // VANGUARD_BOOT_SEQUENCE_H
