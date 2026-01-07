/**
 * @file main.cpp
 * @brief The Assessor - Target-First Auditing Tool
 *
 * A philosophical fork of Bruce that inverts the UX paradigm:
 * Instead of "pick attack, then target", we do "see targets, pick one, see options".
 *
 * @author The Assessor Contributors
 * @license GPL-3.0
 */

#include <M5Cardputer.h>
#include "core/AssessorEngine.h"
#include "ui/BootSequence.h"
#include "ui/TargetRadar.h"
#include "ui/TargetDetail.h"
#include "ui/MainMenu.h"
#include "ui/SettingsPanel.h"
#include "ui/Theme.h"

using namespace Assessor;

// Note: KEY_ENTER and KEY_BACKSPACE are defined in M5Cardputer's Keyboard_def.h
// We'll use the library's definitions

// Forward declare keyboard handler
void handleKeyboardInput();

// =============================================================================
// GLOBALS
// =============================================================================

static AssessorEngine* g_engine   = nullptr;
static BootSequence*   g_boot     = nullptr;
static TargetRadar*    g_radar    = nullptr;
static TargetDetail*   g_detail   = nullptr;
static MainMenu*       g_menu     = nullptr;
static SettingsPanel*  g_settings = nullptr;

enum class AppState {
    BOOTING,
    SCANNING,
    RADAR,
    TARGET_DETAIL,
    ATTACKING,
    SETTINGS,
    ERROR
};

static AppState g_state = AppState::BOOTING;

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    // Feed watchdog early
    yield();

    // Initialize M5Cardputer with keyboard enabled
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);  // true = enable keyboard

    // Feed watchdog after M5.begin
    yield();

    // Apply theme
    M5Cardputer.Display.setRotation(1);  // Landscape
    M5Cardputer.Display.fillScreen(Theme::COLOR_BACKGROUND);
    M5Cardputer.Display.setTextColor(Theme::COLOR_TEXT_PRIMARY);
    M5Cardputer.Display.setFont(&fonts::Font0);

    // Show immediate feedback (before heavy init)
    M5Cardputer.Display.setCursor(10, 60);
    M5Cardputer.Display.print("Initializing...");

    yield();  // Feed watchdog

    // Initialize components (heavy operations)
    g_engine = &AssessorEngine::getInstance();

    yield();  // Feed watchdog

    g_boot = new BootSequence();
    g_radar = new TargetRadar(*g_engine);
    g_menu = new MainMenu();
    g_settings = new SettingsPanel();

    // Start boot sequence
    g_boot->begin();
    g_state = AppState::BOOTING;

    // Safe serial print (only if CDC is connected)
    if (Serial) {
        Serial.println(F("[Assessor] Boot sequence started"));
    }
}

// =============================================================================
// LOOP
// =============================================================================

void loop() {
    M5Cardputer.update();  // Read keyboard and buttons

    // Handle keyboard input globally
    handleKeyboardInput();

    // If menu is visible, render it on top and handle its actions
    if (g_menu && g_menu->isVisible()) {
        g_menu->tick();
        g_menu->render();

        // Check for menu action
        if (g_menu->hasAction()) {
            MenuAction action = g_menu->getAction();
            switch (action) {
                case MenuAction::RESCAN:
                    g_engine->beginScan();
                    g_state = AppState::SCANNING;
                    break;
                case MenuAction::RESCAN_BLE:
                    g_engine->beginBLEScan();
                    g_state = AppState::SCANNING;
                    break;
                case MenuAction::SETTINGS:
                    g_settings->show();
                    g_state = AppState::SETTINGS;
                    break;
                case MenuAction::ABOUT:
                    // TODO: Show about
                    break;
                case MenuAction::BACK:
                case MenuAction::NONE:
                    // Just close menu
                    break;
            }
        }
        yield();
        return;  // Skip normal processing while menu is visible
    }

    switch (g_state) {
        case AppState::BOOTING:
            g_boot->tick();
            if (g_boot->isComplete()) {
                // Don't auto-scan - let user trigger it
                // This is less jarring and gives user control
                g_state = AppState::RADAR;
            }
            break;

        case AppState::SCANNING:
            g_engine->tick();
            g_radar->renderScanning();
            if (g_engine->getScanState() == ScanState::COMPLETE) {
                g_state = AppState::RADAR;
            }
            break;

        case AppState::RADAR:
            g_engine->tick();
            g_radar->tick();
            g_radar->render();

            // Handle target selection
            if (g_radar->hasSelection()) {
                const Target* selected = g_radar->getSelectedTarget();
                if (selected) {
                    // Clean up any previous detail view
                    if (g_detail) {
                        delete g_detail;
                    }
                    g_detail = new TargetDetail(*g_engine, *selected);
                    g_radar->clearSelection();
                    g_state = AppState::TARGET_DETAIL;
                }
            }
            break;

        case AppState::TARGET_DETAIL:
            g_engine->tick();
            if (g_detail) {
                g_detail->tick();
                g_detail->render();

                // Check if user wants to go back
                if (g_detail->wantsBack()) {
                    delete g_detail;
                    g_detail = nullptr;
                    g_state = AppState::RADAR;
                }
                // Check if action was confirmed
                else if (g_detail->actionConfirmed()) {
                    ActionType action = g_detail->getConfirmedAction();
                    g_detail->clearActionConfirmation();
                    g_engine->executeAction(action, g_detail->getTarget());
                    g_state = AppState::ATTACKING;
                }
            }
            break;

        case AppState::ATTACKING:
            g_engine->tick();
            if (g_detail) {
                g_detail->tick();
                g_detail->render();

                // Check if attack finished
                if (!g_engine->isActionActive()) {
                    g_state = AppState::TARGET_DETAIL;
                }
                // Check if user cancelled
                if (g_detail->wantsBack()) {
                    g_engine->stopAction();
                    g_state = AppState::TARGET_DETAIL;
                }
            }
            break;

        case AppState::SETTINGS:
            g_settings->tick();
            g_settings->render();
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
                g_state = AppState::SCANNING;
            }
            break;
    }

    // Small yield to prevent watchdog
    yield();
}

// =============================================================================
// KEYBOARD INPUT HANDLER
// =============================================================================

void handleKeyboardInput() {
    if (!M5Cardputer.Keyboard.isChange()) {
        return;  // No keyboard state change
    }

    // Get keyboard state
    Keyboard_Class::KeysState keys = M5Cardputer.Keyboard.keysState();

    // Handle menu input first (if visible)
    if (g_menu && g_menu->isVisible()) {
        if (M5Cardputer.Keyboard.isKeyPressed(';') || M5Cardputer.Keyboard.isKeyPressed(',')) {
            g_menu->navigateUp();
        }
        if (M5Cardputer.Keyboard.isKeyPressed('.') || M5Cardputer.Keyboard.isKeyPressed('/')) {
            g_menu->navigateDown();
        }
        if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER) || M5Cardputer.Keyboard.isKeyPressed('e')) {
            g_menu->select();
        }
        // Close menu with M, Q, or Backspace
        if (M5Cardputer.Keyboard.isKeyPressed('m') || M5Cardputer.Keyboard.isKeyPressed('M') ||
            M5Cardputer.Keyboard.isKeyPressed('q') || M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            g_menu->hide();
        }
        return;  // Don't process other input while menu is open
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

    // Global shortcuts that work in any state
    // 'R' - Rescan (only when in RADAR state)
    if (g_state == AppState::RADAR) {
        if (M5Cardputer.Keyboard.isKeyPressed('r') || M5Cardputer.Keyboard.isKeyPressed('R')) {
            g_engine->beginScan();
            g_state = AppState::SCANNING;
            if (Serial) Serial.println(F("[Assessor] Rescan triggered"));
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
            g_state = AppState::TARGET_DETAIL;
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
            g_state = AppState::RADAR;
        }
    }
}
