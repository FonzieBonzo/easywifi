#ifndef EASYWIFI_H
#define EASYWIFI_H

#include <Arduino.h>
#include <WebServer.h>
#include <functional>

/**
 * EasyWifi - ESP32 WiFi library with web configuration portal
 *
 * Features:
 * - Auto-connect to saved WiFi credentials (STA mode)
 * - Fallback AP mode for configuration
 * - Web interface for WiFi setup and network config
 * - OTA firmware updates
 * - mDNS support
 * - Persistent storage via NVS
 * - Custom settings storage and web sections
 */

// ========================================
// Custom Section Callbacks
// ========================================

/**
 * Callback function type for generating custom HTML
 * @return HTML string for the custom section content
 */
typedef std::function<String()> CustomHtmlCallback;

/**
 * Callback function type for saving custom settings
 * @param server Pointer to WebServer for accessing POST arguments
 */
typedef std::function<void(WebServer*)> CustomSaveCallback;

/**
 * Initialize and start EasyWifi
 *
 * @param mdnsHost DNS hostname for mDNS (access via http://mdnsHost.local)
 * @param staTimeoutMs Timeout in ms for STA connection attempt (default: 20000)
 * @param apIdleTimeoutMs Idle timeout in ms for AP mode auto-shutdown (default: 900000 = 15 min)
 * @return true if successfully started in STA or AP mode
 */
bool StartEasyWifi(const char* mdnsHost,
                   uint32_t staTimeoutMs = 20000,
                   uint32_t apIdleTimeoutMs = 900000);

/**
 * Get WiFi signal strength as percentage
 * @return 0-100 (0 = no signal, 100 = excellent)
 */
int GetWifiStrengthPercent();

/**
 * Check if connected to WiFi in STA mode
 * @return true if connected
 */
bool IsWifiConnected();

/**
 * Get current IP address as string
 * @return IP address string (STA IP or AP IP)
 */
String GetWifiIpString();

/**
 * Get firmware version string
 * @return Version string based on compile date/time
 */
String GetVersionString();

/**
 * EasyWifi loop - must be called in main loop()
 * Handles web server requests and AP idle timeout monitoring
 */
void EasyWifiLoop();

// ========================================
// Custom Settings API (Option 1)
// ========================================

/**
 * Save a custom string setting to NVS
 * @param key Setting key (max 15 chars)
 * @param value String value to save
 */
void EasyWifiSetCustomString(const char* key, const char* value);

/**
 * Load a custom string setting from NVS
 * @param key Setting key
 * @param defaultValue Default value if key not found
 * @return Stored value or default
 */
String EasyWifiGetCustomString(const char* key, const char* defaultValue = "");

/**
 * Save a custom integer setting to NVS
 * @param key Setting key (max 15 chars)
 * @param value Integer value to save
 */
void EasyWifiSetCustomInt(const char* key, int value);

/**
 * Load a custom integer setting from NVS
 * @param key Setting key
 * @param defaultValue Default value if key not found
 * @return Stored value or default
 */
int EasyWifiGetCustomInt(const char* key, int defaultValue = 0);

/**
 * Save a custom boolean setting to NVS
 * @param key Setting key (max 15 chars)
 * @param value Boolean value to save
 */
void EasyWifiSetCustomBool(const char* key, bool value);

/**
 * Load a custom boolean setting from NVS
 * @param key Setting key
 * @param defaultValue Default value if key not found
 * @return Stored value or default
 */
bool EasyWifiGetCustomBool(const char* key, bool defaultValue = false);

// ========================================
// Custom Web Sections API (Option 2)
// ========================================

/**
 * Register a custom configuration section in the web interface
 *
 * @param title Section title (displayed as h2 header)
 * @param htmlCallback Function that returns HTML content for the section
 * @param saveCallback Function called when settings are saved (can be nullptr)
 *
 * Example usage:
 *
 *   String getMqttHtml() {
 *       return "<div class='form-group'><label>MQTT Server</label>"
 *              "<input type='text' name='mqtt_server' value='" +
 *              EasyWifiGetCustomString("mqtt_server") + "'></div>";
 *   }
 *
 *   void saveMqtt(WebServer* server) {
 *       EasyWifiSetCustomString("mqtt_server", server->arg("mqtt_server").c_str());
 *   }
 *
 *   void setup() {
 *       StartEasyWifi("mydevice");
 *       EasyWifiAddCustomSection("MQTT Settings", getMqttHtml, saveMqtt);
 *   }
 */
void EasyWifiAddCustomSection(const char* title,
                              CustomHtmlCallback htmlCallback,
                              CustomSaveCallback saveCallback = nullptr);

#endif // EASYWIFI_H
