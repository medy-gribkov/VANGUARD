/**
 * @file main.cpp
 * @brief VANGUARD - Target-First Auditing Suite
 *
 * A philosophical fork of Bruce that inverts the UX paradigm:
 * Instead of "pick attack, then target", we do "see targets, pick one, see options".
 *
 * @author VANGUARD Contributors
 * @license GPL-3.0
 */

#include <M5Cardputer.h>
#include <M5Unified.hpp>
#include <esp_task_wdt.h>
#include "core/VanguardEngine.h"
#include "core/SystemTask.h"
#include "core/SystemMonitor.h"
#include "ui/SafeMode.h"
#include "ui/BootSequence.h"
#include "ui/ScanSelector.h"
#include "ui/TargetRadar.h"
#include "ui/TargetDetail.h"
#include "ui/MainMenu.h"
#include "ui/SettingsPanel.h"
#include "ui/AboutPanel.h"
#include "ui/SpectrumView.h"
#include "ui/Theme.h"
#include "ui/FeedbackManager.h"

using namespace Vanguard;

// Note: KEY_ENTER and KEY_BACKSPACE are defined in M5Cardputer's Keyboard_def.h
// We'll use the library's definitions

// Forward declare keyboard handler
void handleKeyboardInput();

// =============================================================================
// GLOBALS
// =============================================================================

static VanguardEngine* g_engine       = nullptr;
static BootSequence*   g_boot         = nullptr;
static ScanSelector*   g_scanSelector = nullptr;
static TargetRadar*    g_radar        = nullptr;
static TargetDetail*   g_detail       = nullptr;
static MainMenu*       g_menu         = nullptr;
static SettingsPanel*  g_settings     = nullptr;
static AboutPanel*     g_about        = nullptr;
static SpectrumView*   g_spectrum     = nullptr;

enum class AppState {
    INITIALIZING,    // New state for lazy loading
    BOOTING,
    READY_TO_SCAN,   // Post-boot scan type selection
    SCANNING,
    RADAR,
    TARGET_DETAIL,
    ATTACKING,
    SPECTRUM_ANALYZER,
    SETTINGS,
    ABOUT,           // About dialog
    ERROR
};

static AppState g_state = AppState::INITIALIZING;
static uint32_t g_lastKeyMs = 0;  // Debounce
static constexpr uint32_t KEY_DEBOUNCE_MS = 50;
static bool g_consumeNextInput = false;  // Prevents key "bleed-through" after menu actions

void setAppState(AppState newState) {
    Serial.printf("[STATE] %d -> %d\n", (int)g_state, (int)newState);
    g_state = newState;
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    // Feed watchdog early
    yield();

    // Initialize M5Cardputer ADV: split keyboard init for I2C timing
    // The TCA8418 keyboard controller needs In_I2C to be fully ready.
    // M5.begin() initializes In_I2C on GPIO 8/9, but we give it settling time.
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, false);  // false = skip keyboard init
    delay(50);                       // Let I2C bus and TCA8418 settle

    // Now initialize keyboard after I2C is stable
    M5Cardputer.Keyboard.begin();

    // Verify TCA8418 is reachable at 0x34 via In_I2C
    bool tca_ok = m5::In_I2C.start(0x34, false, 400000);
    m5::In_I2C.stop();

    if (!tca_ok) {
        // Force In_I2C re-init and retry
        m5::In_I2C.release();
        m5::In_I2C.begin(I2C_NUM_1, 8, 9);
        delay(50);
        M5Cardputer.Keyboard.begin();  // Retry keyboard init
    }

    // Feed watchdog after init
    yield();

    // Apply theme
    M5Cardputer.Display.fillScreen(Theme::COLOR_BACKGROUND);
    M5Cardputer.Display.setTextColor(Theme::COLOR_TEXT_PRIMARY);
    M5Cardputer.Display.setFont(&fonts::Font0);

    // KEY CHECK: Safe Mode
    // Check if G0 (BtnA) is held during boot
    M5Cardputer.update();
    if (M5Cardputer.BtnA.isPressed()) {
        SafeMode::run(); // Blocks forever
    }

    // Start Background System Task (Core 0)
    // This handles all WiFi/BLE operations to prevent UI freezing
    Vanguard::SystemTask::getInstance().start();
    Vanguard::SystemMonitor::getInstance().start();

    // Application State Machine (Core 1)
    g_engine = &VanguardEngine::getInstance();
    g_engine->init();
    
    yield();  // Feed watchdog
    
    // UI Components (null checks for allocation failures)
    g_boot = new (std::nothrow) BootSequence();
    g_scanSelector = new (std::nothrow) ScanSelector();
    g_radar = new (std::nothrow) TargetRadar(*g_engine);
    g_menu = new (std::nothrow) MainMenu();
    g_settings = new (std::nothrow) SettingsPanel();
    g_about = new (std::nothrow) AboutPanel();
    g_spectrum = new (std::nothrow) SpectrumView();

    if (!g_boot || !g_scanSelector || !g_radar || !g_menu) {
        if (Serial) Serial.println("[FATAL] UI allocation failed");
        setAppState(AppState::ERROR);
        return;
    }

    FeedbackManager::getInstance().init();
    FeedbackManager::getInstance().beep(2000, 100); // Boot beep

    // Safe serial print (only if CDC is connected)
    if (Serial) {
        Serial.println(F("[VANGUARD] Initialized. Entering loop."));
    }
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
    uint32_t frameStart = millis();

    // [FOUNDATION] Feed Watchdog (Item 11)
    esp_task_wdt_reset();

    M5Cardputer.update();  // Read keyboard and buttons

    // Handle keyboard input globally
    handleKeyboardInput();

    // Check for pending menu action FIRST (even if menu closed)
    if (g_menu && g_menu->hasAction()) {
        g_consumeNextInput = true;  // Prevent key "bleed-through" to next state
        MenuAction action = g_menu->getAction();
        switch (action) {
            case MenuAction::RESCAN:
                g_engine->beginScan();
                setAppState(AppState::SCANNING);
                break;
            case MenuAction::RESCAN_BLE:
                g_engine->beginBLEScan();
                setAppState(AppState::SCANNING);
                break;
            case MenuAction::SPECTRUM:
                if (!g_spectrum) break;
                g_spectrum->show();
                setAppState(AppState::SPECTRUM_ANALYZER);
                break;
            case MenuAction::SETTINGS:
                if (g_settings) {
                    g_settings->show();
                    setAppState(AppState::SETTINGS);
                }
                break;
            case MenuAction::ABOUT:
                if (g_about) {
                    g_about->show();
                    setAppState(AppState::ABOUT);
                }
                break;
            case MenuAction::BACK:
            case MenuAction::NONE:
                // Just close menu
                break;
        }
    }

    // Always tick engine so scans/attacks continue during menu
    if (g_engine && g_state != AppState::INITIALIZING && g_state != AppState::BOOTING) {
        g_engine->tick();
    }

    // If menu is visible, render it on top
    if (g_menu && g_menu->isVisible()) {
        g_menu->tick();
        g_menu->render();
        yield();
        return;  // Skip normal UI processing while menu is visible
    }

    switch (g_state) {
        case AppState::INITIALIZING:
            // Engine already initialized in setup(), just start boot animation
            if (g_boot) {
                g_boot->begin();
            }
            setAppState(AppState::BOOTING);
            break;

        case AppState::BOOTING: {
            if (!g_boot) { setAppState(AppState::ERROR); break; }
            g_boot->tick();

            // Failsafe: if boot sequence seems stuck in logo phase for too long
            static uint32_t bootStart = millis();
            if (millis() - bootStart > 5000 && !g_boot->isComplete()) {
                if (Serial) Serial.println(F("[BOOT] Failsafe triggered: skipping boot animation."));
                g_boot->skip();
            }

            if (g_boot->isComplete()) {
                if (Serial) Serial.println(F("[VANGUARD] Boot complete. Showing Scan Selector."));
                // Transition to scan selection screen
                g_scanSelector->show();
                setAppState(AppState::READY_TO_SCAN);
            }
            break;
        }

        case AppState::READY_TO_SCAN:
            if (!g_scanSelector) { setAppState(AppState::ERROR); break; }
            g_scanSelector->tick();
            g_scanSelector->render();

            // Check if user made a selection
            if (g_scanSelector->hasSelection()) {
                ScanChoice choice = g_scanSelector->getSelection();
                g_scanSelector->clearSelection();
                g_scanSelector->hide();

                switch (choice) {
                    case ScanChoice::WIFI_ONLY:
                        g_engine->beginWiFiScan();
                        break;
                    case ScanChoice::BLE_ONLY:
                        g_engine->beginBLEScan();
                        break;
                    case ScanChoice::COMBINED:
                    default:
                        g_engine->beginScan();
                        break;
                }
                setAppState(AppState::SCANNING);
            }
            break;

        case AppState::SCANNING:
            g_radar->renderScanning();
            if (g_engine->getScanState() == ScanState::COMPLETE) {
                setAppState(AppState::RADAR);
            }
            // Cancel handled via handleKeyboardInput() below
            break;

        case AppState::RADAR:
            g_radar->tick();
            g_radar->render();

            // Handle target selection
            if (g_radar->hasSelection()) {
                const Target* selected = g_radar->getSelectedTarget();
                if (selected) {
                    // Clean up any previous detail view
                    // [MEMORY] Fix Leak: Delete previous instance (Item 34/56)
                    if (g_detail) {
                        delete g_detail;
                        g_detail = nullptr;
                    }
                    g_detail = new (std::nothrow) TargetDetail(*g_engine, *selected);
                    g_radar->clearSelection();
                    setAppState(AppState::TARGET_DETAIL);
                }
            }
            break;

        case AppState::TARGET_DETAIL:
            if (g_detail) {
                g_detail->tick();
                g_detail->render();

                // Check if user wants to go back
                if (g_detail->wantsBack()) {
                    delete g_detail;
                    g_detail = nullptr;
                    setAppState(AppState::RADAR);
                }
                // Check if action was confirmed
                else if (g_detail->actionConfirmed()) {
                    ActionType action = g_detail->getConfirmedAction();
                    uint8_t stationMac[6];
                    g_detail->getConfirmedStationMac(stationMac);
                    g_detail->clearActionConfirmation();
                    
                    g_engine->executeAction(action, g_detail->getTarget(), stationMac);
                    setAppState(AppState::ATTACKING);
                }
            }
            break;

        case AppState::ATTACKING:
            if (g_detail) {
                g_detail->tick();
                g_detail->render();

                // Check if attack finished
                if (!g_engine->isActionActive()) {
                    setAppState(AppState::TARGET_DETAIL);
                }
                if (g_detail->wantsBack()) {
                    g_engine->stopAction();
                    setAppState(AppState::TARGET_DETAIL);
                }
            }
            break;

        case AppState::SPECTRUM_ANALYZER:
             if (g_spectrum) {
                 g_spectrum->handleInput();
                 g_spectrum->tick();
                 g_spectrum->render();
                 // Exit handled via handleKeyboardInput() below
             } else {
                 setAppState(AppState::RADAR);
             }
             break;

        case AppState::SETTINGS:
            if (g_settings) {
                g_settings->tick();
                g_settings->render();

                // Check if settings wants to go back
                if (g_settings->wantsBack()) {
                    g_settings->clearBack();
                    g_settings->hide();
                    setAppState(AppState::RADAR);
                }
            } else {
                // Settings failed to initialize, go back
                setAppState(AppState::RADAR);
            }
            break;

        case AppState::ABOUT:
            if (g_about) {
                g_about->tick();
                g_about->render();

                // Any key closes about dialog
                if (g_about->wantsBack()) {
                    g_about->clearBack();
                    g_about->hide();
                    setAppState(AppState::RADAR);
                }
            } else {
                setAppState(AppState::RADAR);
            }
            break;

        case AppState::ERROR:
            // Show error screen
            M5Cardputer.Display.fillScreen(Theme::COLOR_BACKGROUND);
            M5Cardputer.Display.setTextColor(Theme::COLOR_DANGER);
            M5Cardputer.Display.setTextDatum(MC_DATUM);
            M5Cardputer.Display.drawString("ERROR", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT / 2 - 10);
            M5Cardputer.Display.setTextColor(Theme::COLOR_TEXT_SECONDARY);
            M5Cardputer.Display.drawString("Press any key to restart", Theme::SCREEN_WIDTH / 2, Theme::SCREEN_HEIGHT / 2 + 10);

            // Any key press restarts scanning
            if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
                g_engine->beginScan();
                setAppState(AppState::SCANNING);
            }
            break;
    }

    // Adaptive frame budget: target ~10ms per frame, subtract processing time
    if (g_state != AppState::BOOTING && g_state != AppState::INITIALIZING) {
        int32_t remaining = 10 - (int32_t)(millis() - frameStart);
        if (remaining > 0) delay(remaining);
    }

    yield();
}

// =============================================================================
// KEYBOARD INPUT HANDLER
// =============================================================================

void handleKeyboardInput() {
    // Skip input processing if we just processed a menu action
    if (g_consumeNextInput) {
        g_consumeNextInput = false;
        return;
    }

    // Must have a key state change AND a key press to process
    // isChange() is required before isPressed() on M5Cardputer GPIO matrix keyboard
    if (!M5Cardputer.Keyboard.isChange()) return;
    if (!M5Cardputer.Keyboard.isPressed()) return;

    // Debounce: prevent rapid-fire key events
    uint32_t now = millis();
    if (now - g_lastKeyMs < KEY_DEBOUNCE_MS) return;
    g_lastKeyMs = now;

    // Handle menu input first (if visible)
    if (g_menu && g_menu->isVisible()) {
        if (M5Cardputer.Keyboard.isKeyPressed(';') || M5Cardputer.Keyboard.isKeyPressed(',')) {
            g_menu->navigateUp();
        } else if (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed('/')) {
            g_menu->navigateDown();
        } else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || M5Cardputer.Keyboard.isKeyPressed('e')) {
            g_menu->select();
        }
        // Close menu with M, Q, or Backspace
        else if (M5Cardputer.Keyboard.isKeyPressed('m') || M5Cardputer.Keyboard.isKeyPressed('M') ||
            M5Cardputer.Keyboard.isKeyPressed('q') || M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            g_menu->hide();
        }
        return;
    }

    // Global: 'M' to open menu (except during boot)
    if (g_state != AppState::BOOTING) {
        if (M5Cardputer.Keyboard.isKeyPressed('m') || M5Cardputer.Keyboard.isKeyPressed('M')) {
            if (g_menu) {
                g_menu->show();
            }
            return;
        }
    }

    // Scan selector input (READY_TO_SCAN state)
    if (g_state == AppState::READY_TO_SCAN && g_scanSelector) {
        if (M5Cardputer.Keyboard.isKeyPressed('r') || M5Cardputer.Keyboard.isKeyPressed('R')) {
            g_scanSelector->onKeyR();
            return;
        }
        if (M5Cardputer.Keyboard.isKeyPressed('b') || M5Cardputer.Keyboard.isKeyPressed('B')) {
            g_scanSelector->onKeyB();
            return;
        }
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || M5Cardputer.Keyboard.isKeyPressed('e')) {
            g_scanSelector->onKeyEnter();
            return;
        }
    }

    // Global shortcuts that work in any state
    // 'R' - Rescan (only when in RADAR state)
    if (g_state == AppState::RADAR) {
        if (M5Cardputer.Keyboard.isKeyPressed('r') || M5Cardputer.Keyboard.isKeyPressed('R')) {
            g_engine->beginScan();
            setAppState(AppState::SCANNING);
            if (Serial) Serial.println(F("[VANGUARD] Rescan triggered"));
            return;
        }
        // 'Q' - Back to Scan Selector
        if (M5Cardputer.Keyboard.isKeyPressed('q') || M5Cardputer.Keyboard.isKeyPressed('Q')) {
            g_engine->stopScan();
            g_radar->scrollToTop();
            g_scanSelector->show();
            setAppState(AppState::READY_TO_SCAN);
            return;
        }
    }

    // Cancel scan with Q or Backspace
    if (g_state == AppState::SCANNING) {
        if (M5Cardputer.Keyboard.isKeyPressed('q') || M5Cardputer.Keyboard.isKeyPressed('Q') ||
            M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            g_engine->stopScan();
            setAppState(AppState::RADAR);
            return;
        }
    }

    // Exit spectrum analyzer with Q or Backspace
    if (g_state == AppState::SPECTRUM_ANALYZER && g_spectrum) {
        if (M5Cardputer.Keyboard.isKeyPressed('q') || M5Cardputer.Keyboard.isKeyPressed('Q') ||
            M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            g_spectrum->hide();
            setAppState(AppState::RADAR);
            return;
        }
    }

    // Skip boot animation with any key
    if (g_state == AppState::BOOTING) {
        if (M5Cardputer.Keyboard.isPressed()) {
            g_boot->skip();
            return;
        }
    }

    // Navigation in RADAR state
    if (g_state == AppState::RADAR) {
        // Handle 5GHz warning popup if showing
        if (g_radar->isShowingWarning()) {
            if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || M5Cardputer.Keyboard.isKeyPressed('e')) {
                g_radar->confirmWarning();  // User wants to view info anyway
            }
            if (M5Cardputer.Keyboard.isKeyPressed('q') || M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
                g_radar->cancelWarning();  // User cancelled
            }
            return;  // Don't process other radar input while popup is showing
        }

        // Up navigation: ';' or ',' (above . and / on cardputer keyboard)
        if (M5Cardputer.Keyboard.isKeyPressed(';') || M5Cardputer.Keyboard.isKeyPressed(',')) {
            g_radar->navigateUp();
        }
        // Down navigation: '.' or '/'
        if (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed('/')) {
            g_radar->navigateDown();
        }
        // Select: Enter or 'e'
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || M5Cardputer.Keyboard.isKeyPressed('e')) {
            g_radar->select();
        }
    }

    // Navigation in TARGET_DETAIL state
    if (g_state == AppState::TARGET_DETAIL && g_detail) {
        // Up/Down for action selection
        if (M5Cardputer.Keyboard.isKeyPressed(';') || M5Cardputer.Keyboard.isKeyPressed(',')) {
            g_detail->navigateUp();
        }
        if (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed('/')) {
            g_detail->navigateDown();
        }
        // Select action: Enter or 'e'
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || M5Cardputer.Keyboard.isKeyPressed('e')) {
            g_detail->select();
        }
        // Back: Backspace, 'q', or ESC
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
            M5Cardputer.Keyboard.isKeyPressed('q') ||
            M5Cardputer.Keyboard.isKeyPressed('`')) {
            g_detail->back();
        }
    }

    // Cancel attack in ATTACKING state
    if (g_state == AppState::ATTACKING) {
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
            M5Cardputer.Keyboard.isKeyPressed('q') ||
            M5Cardputer.Keyboard.isKeyPressed('`')) {
            g_engine->stopAction();
            setAppState(AppState::TARGET_DETAIL);

        }
    }

    // Settings navigation
    if (g_state == AppState::SETTINGS && g_settings) {
        // Up/Down
        if (M5Cardputer.Keyboard.isKeyPressed(';') || M5Cardputer.Keyboard.isKeyPressed(',')) {
            g_settings->navigateUp();
        }
        if (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed('/')) {
            g_settings->navigateDown();
        }
        // Left/Right to adjust values
        if (M5Cardputer.Keyboard.isKeyPressed('[') || M5Cardputer.Keyboard.isKeyPressed('k')) {
            g_settings->adjustDown();
        }
        if (M5Cardputer.Keyboard.isKeyPressed(']') || M5Cardputer.Keyboard.isKeyPressed('l')) {
            g_settings->adjustUp();
        }
        // Enter to toggle
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || M5Cardputer.Keyboard.isKeyPressed('e')) {
            g_settings->select();
        }
        // Back
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE) ||
            M5Cardputer.Keyboard.isKeyPressed('q') ||
            M5Cardputer.Keyboard.isKeyPressed('`')) {
            g_settings->hide();
            setAppState(AppState::RADAR);
        }
    }
}
