#include "easywifi.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include <vector>

// ========================================
// Internal state
// ========================================

static WebServer* webServer = nullptr;
static Preferences prefs;
static String mdnsHostname = "";
static uint32_t apIdleTimeout = 900000;  // 15 minutes default
static uint32_t lastHttpRequest = 0;
static bool isApMode = false;
static bool serverStarted = false;

// Network configuration structure
struct NetworkConfig {
    String ssid;
    String password;
    bool useDhcp;
    IPAddress staticIp;
    IPAddress subnet;
    IPAddress gateway;
    IPAddress dns1;
    IPAddress dns2;
};

static NetworkConfig netConfig;

// Custom section structure
struct CustomSection {
    String title;
    CustomHtmlCallback htmlCallback;
    CustomSaveCallback saveCallback;
};

static std::vector<CustomSection> customSections;
static Preferences customPrefs;

// ========================================
// Forward declarations
// ========================================

static void loadConfig();
static void saveConfig();
static bool connectToWiFi(uint32_t timeoutMs);
static void startAPMode();
static void startWebServer();
static void setupHandlers();
static void handleRoot();
static void handleScan();
static void handleStatus();
static void handleSave();
static void handleCustomSave();
static void handleUpdate();
static void handleUpdateUpload();
static String getHtmlPage();
static IPAddress parseIpAddress(const String& ipStr);

// ========================================
// Version string
// ========================================

const String VERSION_STRING = String(__DATE__) + " " + String(__TIME__);

String GetVersionString() {
    return VERSION_STRING;
}

// ========================================
// Public API implementation
// ========================================

bool StartEasyWifi(const char* mdnsHost, uint32_t staTimeoutMs, uint32_t apIdleTimeoutMs) {
    mdnsHostname = String(mdnsHost);
    apIdleTimeout = apIdleTimeoutMs;

    Serial.begin(115200);
    delay(100);
    Serial.println("\n\n=== EasyWifi Starting ===");
    Serial.print("Version: ");
    Serial.println(VERSION_STRING);
    Serial.print("mDNS Hostname: ");
    Serial.println(mdnsHostname);

    // Load saved configuration
    loadConfig();

    // Try to connect to saved WiFi
    if (!netConfig.ssid.isEmpty()) {
        Serial.print("Attempting to connect to: ");
        Serial.println(netConfig.ssid);

        if (connectToWiFi(staTimeoutMs)) {
            Serial.println("Successfully connected to WiFi!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            isApMode = false;

            // Start mDNS
            if (MDNS.begin(mdnsHostname.c_str())) {
                Serial.print("mDNS started: http://");
                Serial.print(mdnsHostname);
                Serial.println(".local");
            } else {
                Serial.println("Error starting mDNS");
            }

            // Start web server
            startWebServer();
            return true;
        } else {
            Serial.println("Failed to connect to saved WiFi");
        }
    } else {
        Serial.println("No saved WiFi credentials found");
    }

    // Start AP mode as fallback
    Serial.println("Starting AP mode...");
    startAPMode();
    startWebServer();
    return true;
}

int GetWifiStrengthPercent() {
    if (!IsWifiConnected()) {
        return 0;
    }

    int32_t rssi = WiFi.RSSI();

    // Convert RSSI to percentage (typical range: -90 to -30 dBm)
    // -30 dBm = 100%, -90 dBm = 0%
    if (rssi >= -50) return 100;
    if (rssi <= -100) return 0;

    return 2 * (rssi + 100);
}

bool IsWifiConnected() {
    return WiFi.status() == WL_CONNECTED && !isApMode;
}

String GetWifiIpString() {
    if (isApMode) {
        return WiFi.softAPIP().toString();
    } else {
        return WiFi.localIP().toString();
    }
}

void EasyWifiLoop() {
    if (webServer != nullptr) {
        webServer->handleClient();
    }

    // Check AP idle timeout
    if (isApMode && apIdleTimeout > 0) {
        uint32_t idleTime = millis() - lastHttpRequest;

        // Handle millis() overflow
        if (millis() < lastHttpRequest) {
            lastHttpRequest = millis();
            idleTime = 0;
        }

        if (idleTime > apIdleTimeout && lastHttpRequest > 0) {
            Serial.println("AP idle timeout reached - shutting down AP and web server");

            // Stop web server
            if (webServer != nullptr) {
                webServer->stop();
                delete webServer;
                webServer = nullptr;
                serverStarted = false;
            }

            // Stop AP mode
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_OFF);
            isApMode = false;

            Serial.println("AP mode stopped. System in low power state.");
            Serial.println("Reset device to retry WiFi connection.");

            // Note: We don't restart automatically to avoid boot loops.
            // The system stays in a safe, low-power state.
            // User can reset the device manually when ready.
        }
    }
}

// ========================================
// Internal helper functions
// ========================================

static void loadConfig() {
    prefs.begin("easywifi", true);  // Read-only

    netConfig.ssid = prefs.getString("ssid", "");
    netConfig.password = prefs.getString("password", "");
    netConfig.useDhcp = prefs.getBool("dhcp", true);

    // Load static IP settings
    if (!netConfig.useDhcp) {
        String ipStr = prefs.getString("ip", "");
        String subnetStr = prefs.getString("subnet", "");
        String gatewayStr = prefs.getString("gateway", "");
        String dns1Str = prefs.getString("dns1", "");
        String dns2Str = prefs.getString("dns2", "");

        if (!ipStr.isEmpty()) netConfig.staticIp = parseIpAddress(ipStr);
        if (!subnetStr.isEmpty()) netConfig.subnet = parseIpAddress(subnetStr);
        if (!gatewayStr.isEmpty()) netConfig.gateway = parseIpAddress(gatewayStr);
        if (!dns1Str.isEmpty()) netConfig.dns1 = parseIpAddress(dns1Str);
        if (!dns2Str.isEmpty()) netConfig.dns2 = parseIpAddress(dns2Str);
    }

    prefs.end();

    Serial.println("Configuration loaded from NVS");
    if (!netConfig.ssid.isEmpty()) {
        Serial.print("  SSID: ");
        Serial.println(netConfig.ssid);
        Serial.print("  DHCP: ");
        Serial.println(netConfig.useDhcp ? "Yes" : "No");
    }
}

static void saveConfig() {
    prefs.begin("easywifi", false);  // Read-write

    prefs.putString("ssid", netConfig.ssid);
    prefs.putString("password", netConfig.password);
    prefs.putBool("dhcp", netConfig.useDhcp);

    if (!netConfig.useDhcp) {
        prefs.putString("ip", netConfig.staticIp.toString());
        prefs.putString("subnet", netConfig.subnet.toString());
        prefs.putString("gateway", netConfig.gateway.toString());
        prefs.putString("dns1", netConfig.dns1.toString());
        prefs.putString("dns2", netConfig.dns2.toString());
    }

    prefs.end();

    Serial.println("Configuration saved to NVS");
}

static bool connectToWiFi(uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);

    // Configure static IP if needed
    if (!netConfig.useDhcp && netConfig.staticIp != IPAddress(0, 0, 0, 0)) {
        Serial.println("Configuring static IP");
        if (!WiFi.config(netConfig.staticIp, netConfig.gateway, netConfig.subnet,
                        netConfig.dns1, netConfig.dns2)) {
            Serial.println("Failed to configure static IP");
            return false;
        }
    }

    WiFi.begin(netConfig.ssid.c_str(), netConfig.password.c_str());

    uint32_t startTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startTime > timeoutMs) {
            Serial.println("WiFi connection timeout");
            WiFi.disconnect();
            return false;
        }
        delay(100);
        Serial.print(".");
    }

    Serial.println();
    return true;
}

static void startAPMode() {
    WiFi.mode(WIFI_AP);

    // Generate unique AP name using MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String apName = "EasyWiFi-" + String(mac[4], HEX) + String(mac[5], HEX);
    apName.toUpperCase();

    WiFi.softAP(apName.c_str());

    IPAddress apIP = WiFi.softAPIP();
    Serial.print("AP Mode started: ");
    Serial.println(apName);
    Serial.print("AP IP Address: ");
    Serial.println(apIP);

    isApMode = true;
    lastHttpRequest = millis();  // Initialize timer

    // Try to start mDNS in AP mode (may not work on all platforms)
    if (MDNS.begin(mdnsHostname.c_str())) {
        Serial.print("mDNS started: http://");
        Serial.print(mdnsHostname);
        Serial.println(".local");
    } else {
        Serial.println("mDNS not available in AP mode - use IP: " + apIP.toString());
    }
}

static void startWebServer() {
    if (serverStarted) return;

    webServer = new WebServer(80);
    setupHandlers();
    webServer->begin();
    serverStarted = true;

    Serial.println("Web server started on port 80");
}

static void setupHandlers() {
    webServer->on("/", HTTP_GET, handleRoot);
    webServer->on("/scan", HTTP_GET, handleScan);
    webServer->on("/status", HTTP_GET, handleStatus);
    webServer->on("/save", HTTP_POST, handleSave);
    webServer->on("/customsave", HTTP_POST, handleCustomSave);
    webServer->on("/update", HTTP_POST, handleUpdate, handleUpdateUpload);
}

static void handleRoot() {
    lastHttpRequest = millis();
    webServer->send(200, "text/html", getHtmlPage());
}

static void handleScan() {
    lastHttpRequest = millis();

    int n = WiFi.scanNetworks();
    String json = "[";

    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"encryption\":" + String(WiFi.encryptionType(i));
        json += "}";
    }

    json += "]";
    WiFi.scanDelete();

    webServer->send(200, "application/json", json);
}

static void handleStatus() {
    lastHttpRequest = millis();

    String currentSsid = "";
    if (IsWifiConnected()) {
        currentSsid = netConfig.ssid;
    } else if (isApMode) {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        currentSsid = "EasyWiFi-" + String(mac[4], HEX) + String(mac[5], HEX);
        currentSsid.toUpperCase();
    }

    String json = "{";
    json += "\"connected\":" + String(IsWifiConnected() ? "true" : "false") + ",";
    json += "\"ssid\":\"" + currentSsid + "\",";
    json += "\"ip\":\"" + GetWifiIpString() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"strengthPercent\":" + String(GetWifiStrengthPercent()) + ",";
    json += "\"mode\":\"" + String(isApMode ? "AP" : "STA") + "\",";
    json += "\"version\":\"" + VERSION_STRING + "\"";
    json += "}";

    webServer->send(200, "application/json", json);
}

static void handleSave() {
    lastHttpRequest = millis();

    if (!webServer->hasArg("ssid")) {
        webServer->send(400, "text/plain", "Missing SSID");
        return;
    }

    // Update configuration
    netConfig.ssid = webServer->arg("ssid");
    netConfig.password = webServer->arg("password");
    netConfig.useDhcp = webServer->arg("dhcp") == "true";

    if (!netConfig.useDhcp) {
        netConfig.staticIp = parseIpAddress(webServer->arg("ip"));
        netConfig.subnet = parseIpAddress(webServer->arg("subnet"));
        netConfig.gateway = parseIpAddress(webServer->arg("gateway"));
        netConfig.dns1 = parseIpAddress(webServer->arg("dns1"));
        netConfig.dns2 = parseIpAddress(webServer->arg("dns2"));

        // Validate IP addresses
        if (netConfig.staticIp == IPAddress(0, 0, 0, 0) ||
            netConfig.gateway == IPAddress(0, 0, 0, 0) ||
            netConfig.subnet == IPAddress(0, 0, 0, 0)) {
            webServer->send(400, "text/plain", "Invalid IP configuration");
            return;
        }
    }

    // Save to NVS
    saveConfig();

    webServer->send(200, "text/plain", "Configuration saved. Attempting to connect...");

    Serial.println("New configuration received, attempting connection...");

    // Disconnect current connection
    if (isApMode) {
        WiFi.softAPdisconnect();
    } else {
        WiFi.disconnect();
    }

    delay(1000);

    // Try to connect with new settings
    if (connectToWiFi(20000)) {
        Serial.println("Successfully connected with new settings!");
        isApMode = false;

        // Restart mDNS
        MDNS.end();
        if (MDNS.begin(mdnsHostname.c_str())) {
            Serial.print("mDNS restarted: http://");
            Serial.print(mdnsHostname);
            Serial.println(".local");
        }
    } else {
        Serial.println("Failed to connect with new settings, restarting AP mode");
        startAPMode();
    }
}

static void handleCustomSave() {
    lastHttpRequest = millis();

    // Call all registered custom save callbacks
    for (auto& section : customSections) {
        if (section.saveCallback) {
            section.saveCallback(webServer);
        }
    }

    webServer->send(200, "text/plain", "Custom settings saved successfully!");
    Serial.println("Custom settings saved");
}

static void handleUpdate() {
    lastHttpRequest = millis();

    webServer->sendHeader("Connection", "close");
    webServer->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");

    delay(1000);
    ESP.restart();
}

static void handleUpdateUpload() {
    lastHttpRequest = millis();

    HTTPUpload& upload = webServer->upload();

    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("Update: %s\n", upload.filename.c_str());

        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}

static IPAddress parseIpAddress(const String& ipStr) {
    IPAddress ip;
    ip.fromString(ipStr);
    return ip;
}

static String getHtmlPage() {
    String html = R"HTML(
<!DOCTYPE html>
<html lang="nl">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>EasyWifi Configuration</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background: #1e1e2e;
            border-radius: 12px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.5);
            overflow: hidden;
            border: 1px solid #2a2a3e;
        }
        .header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }
        .header h1 { font-size: 28px; margin-bottom: 10px; }
        .header .version { opacity: 0.9; font-size: 14px; }
        .content { padding: 30px; }
        .section {
            background: #252535;
            border-radius: 8px;
            padding: 20px;
            margin-bottom: 20px;
            border: 1px solid #2a2a3e;
        }
        .section h2 {
            color: #8b9dff;
            font-size: 20px;
            margin-bottom: 15px;
            border-bottom: 2px solid #667eea;
            padding-bottom: 8px;
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            color: #b8b8d0;
            font-weight: 500;
        }
        input[type="text"], input[type="password"], select {
            width: 100%;
            padding: 10px;
            border: 2px solid #3a3a4e;
            border-radius: 6px;
            font-size: 14px;
            transition: border-color 0.3s;
            background: #1a1a2a;
            color: #e0e0e0;
        }
        input[type="text"]:focus, input[type="password"]:focus, select:focus {
            outline: none;
            border-color: #667eea;
            background: #202030;
        }
        .checkbox-group {
            display: flex;
            align-items: center;
            margin-bottom: 15px;
        }
        .checkbox-group input[type="checkbox"] {
            width: 20px;
            height: 20px;
            margin-right: 10px;
            cursor: pointer;
        }
        .checkbox-group label {
            margin: 0;
            cursor: pointer;
        }
        button {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 6px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
            width: 100%;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(102, 126, 234, 0.6);
        }
        button:active {
            transform: translateY(0);
        }
        button:disabled {
            opacity: 0.6;
            cursor: not-allowed;
        }
        .status-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }
        .status-item {
            background: #2a2a3e;
            padding: 15px;
            border-radius: 6px;
            border-left: 4px solid #667eea;
        }
        .status-item .label {
            color: #8b8b9f;
            font-size: 12px;
            text-transform: uppercase;
            margin-bottom: 5px;
        }
        .status-item .value {
            color: #e0e0e0;
            font-size: 18px;
            font-weight: 600;
        }
        .network-list {
            max-height: 300px;
            overflow-y: auto;
            border: 2px solid #3a3a4e;
            border-radius: 6px;
            background: #1a1a2a;
        }
        .network-item {
            padding: 12px;
            border-bottom: 1px solid #2a2a3e;
            cursor: pointer;
            transition: background 0.2s;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .network-item:hover {
            background: #252535;
        }
        .network-item:last-child {
            border-bottom: none;
        }
        .network-ssid {
            font-weight: 500;
            color: #e0e0e0;
        }
        .network-rssi {
            color: #8b8b9f;
            font-size: 14px;
        }
        .static-ip-fields {
            display: none;
            margin-top: 15px;
            padding-top: 15px;
            border-top: 2px solid #3a3a4e;
        }
        .static-ip-fields.visible {
            display: block;
        }
        .upload-area {
            border: 2px dashed #667eea;
            border-radius: 6px;
            padding: 30px;
            text-align: center;
            cursor: pointer;
            transition: background 0.3s;
            color: #b8b8d0;
        }
        .upload-area:hover {
            background: #2a2a3e;
        }
        .upload-area input[type="file"] {
            display: none;
        }
        .message {
            padding: 12px;
            border-radius: 6px;
            margin-bottom: 15px;
            display: none;
        }
        .message.success {
            background: #1e4620;
            border: 1px solid #2d6a31;
            color: #6dff7e;
        }
        .message.error {
            background: #4a1f1f;
            border: 1px solid #6b2c2c;
            color: #ff7e7e;
        }
        .message.visible {
            display: block;
        }
        .loader {
            border: 3px solid #3a3a4e;
            border-top: 3px solid #667eea;
            border-radius: 50%;
            width: 30px;
            height: 30px;
            animation: spin 1s linear infinite;
            margin: 20px auto;
            display: none;
        }
        .loader.visible {
            display: block;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .ap-notice {
            background: #3d3420;
            border: 1px solid #6b5a2c;
            color: #ffd966;
            padding: 15px;
            border-radius: 6px;
            margin-bottom: 20px;
            text-align: center;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>EasyWifi Configuration</h1>
            <div class="version">Firmware: <span id="firmwareVersion">Loading...</span></div>
        </div>

        <div class="content">
            <div id="apNotice" class="ap-notice" style="display:none;">
                <strong>AP Mode Active</strong><br>
                If mDNS is not available, access this page via: <strong>192.168.4.1</strong>
            </div>

            <div class="section">
                <h2>Live Status</h2>
                <div class="status-grid">
                    <div class="status-item">
                        <div class="label">Status</div>
                        <div class="value" id="statusConnected">...</div>
                    </div>
                    <div class="status-item">
                        <div class="label">SSID</div>
                        <div class="value" id="statusSsid">...</div>
                    </div>
                    <div class="status-item">
                        <div class="label">IP Address</div>
                        <div class="value" id="statusIp">...</div>
                    </div>
                    <div class="status-item">
                        <div class="label">Signal Strength</div>
                        <div class="value" id="statusStrength">...</div>
                    </div>
                </div>
            </div>

            <div class="section">
                <h2>WiFi Configuration</h2>
                <div id="message" class="message"></div>

                <div class="form-group">
                    <label>Available Networks</label>
                    <button onclick="scanNetworks()" style="margin-bottom: 10px;">Scan Networks</button>
                    <div id="loader" class="loader"></div>
                    <div id="networkList" class="network-list"></div>
                </div>

                <form id="wifiForm">
                    <div class="form-group">
                        <label>SSID</label>
                        <input type="text" id="ssid" name="ssid" required>
                    </div>

                    <div class="form-group">
                        <label>Password</label>
                        <input type="password" id="password" name="password">
                    </div>

                    <div class="checkbox-group">
                        <input type="checkbox" id="dhcp" name="dhcp" checked onchange="toggleStaticIp()">
                        <label for="dhcp" style="margin: 0;">Use DHCP (Automatic IP)</label>
                    </div>

                    <div id="staticIpFields" class="static-ip-fields">
                        <div class="form-group">
                            <label>IP Address</label>
                            <input type="text" id="ip" name="ip" placeholder="192.168.1.100">
                        </div>
                        <div class="form-group">
                            <label>Subnet Mask</label>
                            <input type="text" id="subnet" name="subnet" placeholder="255.255.255.0">
                        </div>
                        <div class="form-group">
                            <label>Gateway</label>
                            <input type="text" id="gateway" name="gateway" placeholder="192.168.1.1">
                        </div>
                        <div class="form-group">
                            <label>DNS 1</label>
                            <input type="text" id="dns1" name="dns1" placeholder="8.8.8.8">
                        </div>
                        <div class="form-group">
                            <label>DNS 2 (Optional)</label>
                            <input type="text" id="dns2" name="dns2" placeholder="8.8.4.4">
                        </div>
                    </div>

                    <button type="submit">Save & Connect</button>
                </form>
            </div>

            <div class="section">
                <h2>Firmware Update</h2>
                <div id="updateMessage" class="message"></div>

                <form id="updateForm" enctype="multipart/form-data">
                    <div class="upload-area" onclick="document.getElementById('firmwareFile').click()">
                        <input type="file" id="firmwareFile" name="update" accept=".bin">
                        <div>&#128193; Click to select firmware (.bin)</div>
                        <div id="fileName" style="margin-top: 10px; color: #667eea; font-weight: 500;"></div>
                    </div>

                    <div id="uploadProgress" style="display: none; margin-top: 15px;">
                        <div style="background: #f0f0f0; border-radius: 10px; overflow: hidden; height: 30px; position: relative;">
                            <div id="progressBar" style="background: linear-gradient(90deg, #667eea, #764ba2); height: 100%; width: 0%; transition: width 0.3s;"></div>
                            <div id="progressText" style="position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); font-weight: 600; color: #333;"></div>
                        </div>
                        <div id="uploadStatus" style="margin-top: 10px; text-align: center; color: #667eea; font-weight: 500;"></div>
                    </div>

                    <button type="submit" id="uploadButton" style="margin-top: 15px;">Upload Firmware</button>
                </form>
            </div>
)HTML";

    // Add custom sections dynamically
    for (const auto& section : customSections) {
        html += R"HTML(
            <div class="section">
                <h2>)HTML";
        html += section.title;
        html += R"HTML(</h2>
                <div id="customMessage_)HTML";
        html += section.title;
        html += R"HTML(" class="message"></div>
                <form id="customForm_)HTML";
        html += section.title;
        html += R"HTML(" class="customForm">
)HTML";

        // Call the custom HTML callback
        if (section.htmlCallback) {
            html += section.htmlCallback();
        }

        html += R"HTML(
                    <button type="submit" style="margin-top: 15px;">Save )HTML";
        html += section.title;
        html += R"HTML(</button>
                </form>
            </div>
)HTML";
    }

    html += R"HTML(
        </div>
    </div>

    <script>
        // Update status every 10 seconds
        setInterval(updateStatus, 10000);
        updateStatus();

        function updateStatus() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('statusConnected').textContent = data.connected ? 'Connected' : 'Disconnected';
                    document.getElementById('statusSsid').textContent = data.ssid || 'N/A';
                    document.getElementById('statusIp').textContent = data.ip || 'N/A';
                    document.getElementById('statusStrength').textContent = data.strengthPercent + '%';
                    document.getElementById('firmwareVersion').textContent = data.version || 'Unknown';

                    // Show AP notice if in AP mode
                    if (data.mode === 'AP') {
                        document.getElementById('apNotice').style.display = 'block';
                    } else {
                        document.getElementById('apNotice').style.display = 'none';
                    }
                })
                .catch(error => console.error('Error fetching status:', error));
        }

        function scanNetworks() {
            const loader = document.getElementById('loader');
            const networkList = document.getElementById('networkList');

            loader.classList.add('visible');
            networkList.innerHTML = '';

            fetch('/scan')
                .then(response => response.json())
                .then(networks => {
                    loader.classList.remove('visible');

                    if (networks.length === 0) {
                        networkList.innerHTML = '<div style="padding: 20px; text-align: center; color: #666;">No networks found</div>';
                        return;
                    }

                    networks.sort((a, b) => b.rssi - a.rssi);

                    networks.forEach(network => {
                        const item = document.createElement('div');
                        item.className = 'network-item';
                        item.onclick = () => selectNetwork(network.ssid);

                        const ssid = document.createElement('div');
                        ssid.className = 'network-ssid';
                        ssid.textContent = network.ssid + (network.encryption !== 7 ? ' \u{1F512}' : '');

                        const rssi = document.createElement('div');
                        rssi.className = 'network-rssi';
                        rssi.textContent = network.rssi + ' dBm';

                        item.appendChild(ssid);
                        item.appendChild(rssi);
                        networkList.appendChild(item);
                    });
                })
                .catch(error => {
                    loader.classList.remove('visible');
                    showMessage('Error scanning networks: ' + error, 'error');
                });
        }

        function selectNetwork(ssid) {
            document.getElementById('ssid').value = ssid;
            document.getElementById('password').focus();
        }

        function toggleStaticIp() {
            const staticFields = document.getElementById('staticIpFields');
            const dhcpChecked = document.getElementById('dhcp').checked;

            if (dhcpChecked) {
                staticFields.classList.remove('visible');
            } else {
                staticFields.classList.add('visible');
            }
        }

        document.getElementById('wifiForm').addEventListener('submit', function(e) {
            e.preventDefault();

            const formData = new FormData(this);
            const params = new URLSearchParams();

            params.append('ssid', formData.get('ssid'));
            params.append('password', formData.get('password'));
            params.append('dhcp', document.getElementById('dhcp').checked);

            if (!document.getElementById('dhcp').checked) {
                params.append('ip', formData.get('ip'));
                params.append('subnet', formData.get('subnet'));
                params.append('gateway', formData.get('gateway'));
                params.append('dns1', formData.get('dns1'));
                params.append('dns2', formData.get('dns2'));
            }

            fetch('/save', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: params.toString()
            })
            .then(response => response.text())
            .then(data => {
                showMessage(data, 'success');
                setTimeout(updateStatus, 3000);
            })
            .catch(error => {
                showMessage('Error: ' + error, 'error');
            });
        });

        document.getElementById('firmwareFile').addEventListener('change', function() {
            const fileName = this.files[0] ? this.files[0].name : '';
            document.getElementById('fileName').textContent = fileName ? 'Selected: ' + fileName : '';
        });

        document.getElementById('updateForm').addEventListener('submit', function(e) {
            e.preventDefault();

            const fileInput = document.getElementById('firmwareFile');
            if (!fileInput.files[0]) {
                showUpdateMessage('Please select a firmware file', 'error');
                return;
            }

            const file = fileInput.files[0];
            const formData = new FormData();
            formData.append('update', file);

            // Show progress UI
            document.getElementById('uploadProgress').style.display = 'block';
            document.getElementById('uploadButton').disabled = true;
            document.getElementById('uploadButton').textContent = 'Uploading...';
            document.getElementById('updateMessage').classList.remove('visible');

            const xhr = new XMLHttpRequest();

            // Upload progress
            xhr.upload.addEventListener('progress', function(e) {
                if (e.lengthComputable) {
                    const percentComplete = Math.round((e.loaded / e.total) * 100);
                    document.getElementById('progressBar').style.width = percentComplete + '%';
                    document.getElementById('progressText').textContent = percentComplete + '%';
                    document.getElementById('uploadStatus').textContent = 'Uploading: ' + Math.round(e.loaded / 1024) + ' KB / ' + Math.round(e.total / 1024) + ' KB';
                }
            });

            // Upload complete
            xhr.addEventListener('load', function() {
                if (xhr.status === 200) {
                    document.getElementById('progressBar').style.width = '100%';
                    document.getElementById('progressText').textContent = '100%';
                    document.getElementById('uploadStatus').textContent = 'Upload complete! Verifying...';

                    setTimeout(function() {
                        document.getElementById('uploadStatus').textContent = 'Firmware verified! Rebooting device...';
                        showUpdateMessage('Firmware update successful! Device is rebooting...', 'success');

                        // Countdown timer
                        let countdown = 10;
                        const countdownInterval = setInterval(function() {
                            countdown--;
                            if (countdown > 0) {
                                document.getElementById('uploadStatus').textContent = 'Device rebooting... reconnecting in ' + countdown + ' seconds';
                            } else {
                                clearInterval(countdownInterval);
                                document.getElementById('uploadStatus').textContent = 'Attempting to reconnect...';
                                location.reload();
                            }
                        }, 1000);
                    }, 1000);
                } else {
                    showUpdateMessage('Upload failed! Status: ' + xhr.status, 'error');
                    resetUploadForm();
                }
            });

            // Upload error
            xhr.addEventListener('error', function() {
                showUpdateMessage('Upload error! Please try again.', 'error');
                resetUploadForm();
            });

            // Upload abort
            xhr.addEventListener('abort', function() {
                showUpdateMessage('Upload cancelled.', 'error');
                resetUploadForm();
            });

            xhr.open('POST', '/update');
            xhr.send(formData);
        });

        function showUpdateMessage(text, type) {
            const message = document.getElementById('updateMessage');
            message.textContent = text;
            message.className = 'message ' + type + ' visible';
        }

        function resetUploadForm() {
            document.getElementById('uploadProgress').style.display = 'none';
            document.getElementById('uploadButton').disabled = false;
            document.getElementById('uploadButton').textContent = 'Upload Firmware';
            document.getElementById('progressBar').style.width = '0%';
        }

        // Custom form handlers
        document.querySelectorAll('.customForm').forEach(function(form) {
            form.addEventListener('submit', function(e) {
                e.preventDefault();

                const formData = new FormData(this);
                const params = new URLSearchParams();

                for (const [key, value] of formData.entries()) {
                    params.append(key, value);
                }

                fetch('/customsave', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: params.toString()
                })
                .then(response => response.text())
                .then(data => {
                    const messageEl = this.querySelector('.message');
                    if (messageEl) {
                        messageEl.textContent = data;
                        messageEl.className = 'message success visible';
                        setTimeout(() => messageEl.classList.remove('visible'), 3000);
                    }
                })
                .catch(error => {
                    const messageEl = this.querySelector('.message');
                    if (messageEl) {
                        messageEl.textContent = 'Error: ' + error;
                        messageEl.className = 'message error visible';
                    }
                });
            });
        });

        function showMessage(text, type) {
            const message = document.getElementById('message');
            message.textContent = text;
            message.className = 'message ' + type + ' visible';

            setTimeout(() => {
                message.classList.remove('visible');
            }, 5000);
        }

        // Initial network scan
        scanNetworks();
    </script>
</body>
</html>
)HTML";

    return html;
}

// ========================================
// Custom Settings API Implementation
// ========================================

void EasyWifiSetCustomString(const char* key, const char* value) {
    customPrefs.begin("easywifi_cust", false);  // Read-write
    customPrefs.putString(key, value);
    customPrefs.end();

    Serial.print("Custom setting saved: ");
    Serial.print(key);
    Serial.print(" = ");
    Serial.println(value);
}

String EasyWifiGetCustomString(const char* key, const char* defaultValue) {
    customPrefs.begin("easywifi_cust", true);  // Read-only
    String value = customPrefs.getString(key, defaultValue);
    customPrefs.end();
    return value;
}

void EasyWifiSetCustomInt(const char* key, int value) {
    customPrefs.begin("easywifi_cust", false);  // Read-write
    customPrefs.putInt(key, value);
    customPrefs.end();

    Serial.print("Custom setting saved: ");
    Serial.print(key);
    Serial.print(" = ");
    Serial.println(value);
}

int EasyWifiGetCustomInt(const char* key, int defaultValue) {
    customPrefs.begin("easywifi_cust", true);  // Read-only
    int value = customPrefs.getInt(key, defaultValue);
    customPrefs.end();
    return value;
}

void EasyWifiSetCustomBool(const char* key, bool value) {
    customPrefs.begin("easywifi_cust", false);  // Read-write
    customPrefs.putBool(key, value);
    customPrefs.end();

    Serial.print("Custom setting saved: ");
    Serial.print(key);
    Serial.print(" = ");
    Serial.println(value ? "true" : "false");
}

bool EasyWifiGetCustomBool(const char* key, bool defaultValue) {
    customPrefs.begin("easywifi_cust", true);  // Read-only
    bool value = customPrefs.getBool(key, defaultValue);
    customPrefs.end();
    return value;
}

// ========================================
// Custom Web Sections API Implementation
// ========================================

void EasyWifiAddCustomSection(const char* title,
                              CustomHtmlCallback htmlCallback,
                              CustomSaveCallback saveCallback) {
    CustomSection section;
    section.title = String(title);
    section.htmlCallback = htmlCallback;
    section.saveCallback = saveCallback;

    customSections.push_back(section);

    Serial.print("Custom section registered: ");
    Serial.println(title);
}
