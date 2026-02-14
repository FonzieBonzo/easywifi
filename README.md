# EasyWifi - ESP32 WiFi Configuration Library

[![PlatformIO](https://img.shields.io/badge/PlatformIO-Library-orange.svg)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg)](https://www.espressif.com/en/products/socs/esp32)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A powerful and easy-to-use ESP32 Arduino library for WiFi configuration with a beautiful dark mode web interface, custom settings API, and OTA firmware updates.

## ✨ Features

- 🔌 **Auto-Connect** - Automatically connects to saved WiFi credentials
- 🌐 **AP Fallback** - Starts access point mode if connection fails
- 🎨 **Dark Mode Web GUI** - Modern, responsive web interface
- 📡 **Network Scanner** - Scan and select WiFi networks with signal strength
- ⚙️ **DHCP & Static IP** - Full network configuration support
- 🔄 **OTA Updates** - Upload firmware via web interface with progress bar
- 💾 **Custom Settings API** - Store your own configuration in NVS
- 🎯 **Custom Web Sections** - Add your own configuration forms to the web interface
- 🏷️ **mDNS Support** - Access device via `http://hostname.local`
- ⏱️ **Smart AP Timeout** - Automatically disables AP mode after inactivity
- 📝 **Serial Logging** - Detailed debug information

## 🚀 Quick Start

### Installation

#### PlatformIO

Add to your `platformio.ini`:

```ini
[env:esp32]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino

lib_deps =
    https://github.com/FonzieBonzo/easywifi.git
```

### Basic Usage

```cpp
#include <easywifi.h>

void setup() {
  StartEasyWifi("mydevice");  // Access via http://mydevice.local
}

void loop() {
  EasyWifiLoop();  // Required!
}
```

That's it! Your ESP32 now has:
- WiFi configuration portal
- Web interface at `http://mydevice.local`
- OTA firmware updates
- Persistent settings storage

## 📚 Examples

### Example 1: Basic WiFi Configuration

See [BasicUsage.ino](examples/BasicUsage/BasicUsage.ino)

### Example 2: Custom Settings (MQTT Configuration)

See [AdvancedUsage.ino](examples/AdvancedUsage/AdvancedUsage.ino)

```cpp
#include <easywifi.h>

String getMqttHtml() {
  String server = EasyWifiGetCustomString("mqtt_server", "192.168.1.100");
  int port = EasyWifiGetCustomInt("mqtt_port", 1883);

  String html = "<div class='form-group'>";
  html += "<label>MQTT Server</label>";
  html += "<input type='text' name='mqtt_server' value='" + server + "'>";
  html += "</div>";
  // ... more fields
  return html;
}

void saveMqtt(WebServer* server) {
  EasyWifiSetCustomString("mqtt_server", server->arg("mqtt_server").c_str());
  EasyWifiSetCustomInt("mqtt_port", server->arg("mqtt_port").toInt());
}

void setup() {
  StartEasyWifi("mydevice");

  // Add custom MQTT configuration section to web interface
  EasyWifiAddCustomSection("MQTT Settings", getMqttHtml, saveMqtt);

  // Use the settings
  String server = EasyWifiGetCustomString("mqtt_server");
  connectToMQTT(server);
}

void loop() {
  EasyWifiLoop();
}
```

## 🎯 API Reference

### Core Functions

```cpp
// Initialize WiFi with mDNS hostname
bool StartEasyWifi(const char* mdnsHost,
                   uint32_t staTimeoutMs = 20000,
                   uint32_t apIdleTimeoutMs = 900000);

// Must be called in loop()
void EasyWifiLoop();

// WiFi status
bool   IsWifiConnected();
int    GetWifiStrengthPercent();  // 0-100%
String GetWifiIpString();
String GetVersionString();
```

### Custom Settings API

```cpp
// String settings
void   EasyWifiSetCustomString(const char* key, const char* value);
String EasyWifiGetCustomString(const char* key, const char* defaultValue = "");

// Integer settings
void EasyWifiSetCustomInt(const char* key, int value);
int  EasyWifiGetCustomInt(const char* key, int defaultValue = 0);

// Boolean settings
void EasyWifiSetCustomBool(const char* key, bool value);
bool EasyWifiGetCustomBool(const char* key, bool defaultValue = false);
```

### Custom Web Sections

```cpp
// Add custom configuration section to web interface
void EasyWifiAddCustomSection(const char* title,
                              CustomHtmlCallback htmlCallback,
                              CustomSaveCallback saveCallback);
```

## 🌐 Web Interface

The web interface provides:

1. **Live WiFi Status** - Connection state, SSID, IP, signal strength
2. **Network Scanner** - Scan and select available networks
3. **WiFi Configuration** - SSID, password, DHCP/Static IP
4. **Custom Sections** - Your own configuration forms
5. **Firmware Update** - OTA upload with real-time progress

### Screenshots

```
┌─────────────────────────────┐
│  WiFi Configuration         │
│  • Network Scanner          │
│  • SSID & Password          │
│  • DHCP/Static IP           │
└─────────────────────────────┘

┌─────────────────────────────┐
│  Your Custom Sections       │
│  • MQTT Settings            │
│  • Device Info              │
│  • API Configuration        │
└─────────────────────────────┘

┌─────────────────────────────┐
│  Firmware Update            │
│  • Upload .bin file         │
│  • Progress bar             │
│  • Auto-reboot              │
└─────────────────────────────┘
```

## ⚙️ Configuration

### Default Behavior

- **STA Timeout**: 20 seconds
- **AP Idle Timeout**: 15 minutes (configurable)
- **AP SSID**: `EasyWiFi-XXXX` (XXXX = last 4 MAC digits)
- **Default IP**: `192.168.4.1` (in AP mode)
- **Web Port**: 80
- **Serial Baud**: 115200

### Customization

```cpp
// Custom timeouts
StartEasyWifi("mydevice",
              30000,   // 30s STA connection timeout
              600000); // 10min AP idle timeout

// Disable AP timeout (always keep AP on)
StartEasyWifi("mydevice", 20000, 0);
```

## 💾 Storage

Settings are stored in ESP32 NVS (Non-Volatile Storage):

- **WiFi credentials**: `easywifi` namespace
- **Custom settings**: `easywifi_cust` namespace

Data persists across reboots and firmware updates.

## 🔧 Technical Details

- **Framework**: Arduino (ESP32)
- **WebServer**: Synchronous (not AsyncWebServer)
- **mDNS**: ESPmDNS library
- **OTA**: Update.h (ESP32 Arduino core)
- **Storage**: Preferences (NVS)
- **RAM Usage**: ~47KB (14.4%)
- **Flash Usage**: ~848KB (64.7%)

## 📖 Documentation

For detailed documentation, see [CLAUDE.md](CLAUDE.md)

## 🤝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 🙏 Acknowledgments

Built with ❤️ using:
- ESP32 Arduino Core
- PlatformIO
- Claude Code

## 📞 Support

- Create an issue for bug reports or feature requests
- Pull requests are welcome!

---

**Happy Coding!** 🚀
