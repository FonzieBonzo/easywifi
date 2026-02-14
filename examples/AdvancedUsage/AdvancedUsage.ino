/*
 * EasyWifi - Advanced Usage with Custom Settings
 *
 * This example demonstrates:
 * 1. Using the Custom Settings API to store your own configuration
 * 2. Adding custom web sections to the configuration portal
 * 3. Creating a complete MQTT configuration interface
 *
 * The web interface will show custom sections for:
 * - MQTT settings (server, port, username, password, topic)
 * - Device information (name, location)
 */

#include <easywifi.h>

// ========================================
// Custom MQTT Settings Section
// ========================================

String getMqttSettingsHtml() {
  // Load current values from NVS
  String server = EasyWifiGetCustomString("mqtt_server", "192.168.1.100");
  int port = EasyWifiGetCustomInt("mqtt_port", 1883);
  String user = EasyWifiGetCustomString("mqtt_user", "");
  String topic = EasyWifiGetCustomString("mqtt_topic", "home/esp32");
  bool enabled = EasyWifiGetCustomBool("mqtt_enabled", false);

  String html = "";

  // Enable checkbox
  html += "<div class='checkbox-group'>";
  html += "<input type='checkbox' id='mqtt_enabled' name='mqtt_enabled' value='true'";
  if (enabled) html += " checked";
  html += ">";
  html += "<label for='mqtt_enabled' style='margin: 0;'>Enable MQTT</label>";
  html += "</div>";

  // Server input
  html += "<div class='form-group'>";
  html += "<label>MQTT Server</label>";
  html += "<input type='text' name='mqtt_server' value='" + server + "' placeholder='192.168.1.100'>";
  html += "</div>";

  // Port input
  html += "<div class='form-group'>";
  html += "<label>MQTT Port</label>";
  html += "<input type='text' name='mqtt_port' value='" + String(port) + "' placeholder='1883'>";
  html += "</div>";

  // Username input
  html += "<div class='form-group'>";
  html += "<label>Username (optional)</label>";
  html += "<input type='text' name='mqtt_user' value='" + user + "'>";
  html += "</div>";

  // Password input
  html += "<div class='form-group'>";
  html += "<label>Password (optional)</label>";
  html += "<input type='password' name='mqtt_pass' placeholder='Leave empty to keep current'>";
  html += "</div>";

  // Topic input
  html += "<div class='form-group'>";
  html += "<label>Topic Prefix</label>";
  html += "<input type='text' name='mqtt_topic' value='" + topic + "' placeholder='home/esp32'>";
  html += "</div>";

  return html;
}

void saveMqttSettings(WebServer* server) {
  // Save checkbox state
  EasyWifiSetCustomBool("mqtt_enabled", server->hasArg("mqtt_enabled"));

  // Save other fields
  if (server->hasArg("mqtt_server")) {
    EasyWifiSetCustomString("mqtt_server", server->arg("mqtt_server").c_str());
  }

  if (server->hasArg("mqtt_port")) {
    EasyWifiSetCustomInt("mqtt_port", server->arg("mqtt_port").toInt());
  }

  if (server->hasArg("mqtt_user")) {
    EasyWifiSetCustomString("mqtt_user", server->arg("mqtt_user").c_str());
  }

  // Only save password if provided
  if (server->hasArg("mqtt_pass") && server->arg("mqtt_pass").length() > 0) {
    EasyWifiSetCustomString("mqtt_pass", server->arg("mqtt_pass").c_str());
  }

  if (server->hasArg("mqtt_topic")) {
    EasyWifiSetCustomString("mqtt_topic", server->arg("mqtt_topic").c_str());
  }

  Serial.println("MQTT settings saved!");
}

// ========================================
// Custom Device Info Section
// ========================================

String getDeviceInfoHtml() {
  String deviceName = EasyWifiGetCustomString("device_name", "ESP32-Device");
  String location = EasyWifiGetCustomString("location", "Living Room");

  String html = "";

  html += "<div class='form-group'>";
  html += "<label>Device Name</label>";
  html += "<input type='text' name='device_name' value='" + deviceName + "'>";
  html += "</div>";

  html += "<div class='form-group'>";
  html += "<label>Location</label>";
  html += "<input type='text' name='location' value='" + location + "'>";
  html += "</div>";

  return html;
}

void saveDeviceInfo(WebServer* server) {
  if (server->hasArg("device_name")) {
    EasyWifiSetCustomString("device_name", server->arg("device_name").c_str());
  }

  if (server->hasArg("location")) {
    EasyWifiSetCustomString("location", server->arg("location").c_str());
  }

  Serial.println("Device info saved!");
}

// ========================================
// Setup & Loop
// ========================================

void setup() {
  // Start EasyWifi
  StartEasyWifi("easywifi");

  // Register custom configuration sections
  // These will appear in the web interface
  EasyWifiAddCustomSection("MQTT Settings", getMqttSettingsHtml, saveMqttSettings);
  EasyWifiAddCustomSection("Device Info", getDeviceInfoHtml, saveDeviceInfo);

  // Print current settings
  Serial.println("\n=== Current Settings ===");
  Serial.print("Device Name: ");
  Serial.println(EasyWifiGetCustomString("device_name", "ESP32-Device"));
  Serial.print("Location: ");
  Serial.println(EasyWifiGetCustomString("location", "Unknown"));

  // Check if MQTT is enabled
  if (EasyWifiGetCustomBool("mqtt_enabled", false)) {
    Serial.println("\n=== MQTT Configuration ===");
    Serial.print("Server: ");
    Serial.print(EasyWifiGetCustomString("mqtt_server"));
    Serial.print(":");
    Serial.println(EasyWifiGetCustomInt("mqtt_port"));
    Serial.print("Topic: ");
    Serial.println(EasyWifiGetCustomString("mqtt_topic"));

    // Here you would initialize your MQTT client
    // initMQTT(server, port, user, pass, topic);
  } else {
    Serial.println("MQTT is disabled");
  }
}

void loop() {
  EasyWifiLoop();

  // Your application code here
  // You can access custom settings anytime:
  // String deviceName = EasyWifiGetCustomString("device_name");
  // bool mqttEnabled = EasyWifiGetCustomBool("mqtt_enabled");
}
