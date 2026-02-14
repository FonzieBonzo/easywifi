/*
 * EasyWifi - Basic Usage Example
 *
 * This example shows the simplest way to use the EasyWifi library.
 * The ESP32 will:
 * 1. Try to connect to saved WiFi credentials
 * 2. If no credentials or connection fails, start AP mode
 * 3. Provide a web interface at http://easywifi.local or http://192.168.4.1 (in AP mode)
 *
 * Upload this sketch, then:
 * - Open Serial Monitor (115200 baud) to see connection status
 * - Connect to "EasyWiFi-XXXX" network (in AP mode)
 * - Open browser and go to http://easywifi.local
 * - Configure your WiFi settings
 */

#include <easywifi.h>

void setup() {
  // Start EasyWifi with your desired mDNS hostname
  // Device will be accessible at http://easywifi.local
  StartEasyWifi("easywifi");

  // That's it! EasyWifi handles everything:
  // - WiFi connection
  // - Web server
  // - Configuration portal
  // - OTA updates
}

void loop() {
  // IMPORTANT: Call EasyWifiLoop() in your main loop
  // This handles web server requests and AP timeout monitoring
  EasyWifiLoop();

  // Your application code here
  // You can check WiFi status anytime:
  if (IsWifiConnected()) {
    // Do something when connected
    // Serial.println("WiFi connected!");
  }
}
