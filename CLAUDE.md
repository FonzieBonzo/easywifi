# EasyWifi - ESP32 WiFi Configuration Library

## Project Overzicht

**EasyWifi** is een herbruikbare ESP32 Arduino library voor eenvoudige WiFi configuratie met een web interface. Het project biedt een complete oplossing voor WiFi connectiviteit met automatische fallback naar AP mode, webgebaseerde configuratie, en OTA firmware updates.

**Aangemaakt**: 14 februari 2026
**Platform**: ESP32 (DOIT ESP32 DEVKIT V1)
**Framework**: Arduino (PlatformIO)
**Taal**: Nederlands (comments en UI)

---

## Architectuur

### Tech Stack

```
Hardware: ESP32-D0WDQ6 (revision v1.0)
├── WiFi: Dual mode (STA + AP)
├── Flash: 4MB
├── RAM: 320KB
└── USB-Serial: CP2102 (Silicon Labs)

Software Stack:
├── Arduino Framework (ESP32)
├── WebServer (synchronous, not AsyncWebServer)
├── ESPmDNS (voor .local toegang)
├── Preferences (NVS storage)
├── Update.h (OTA firmware)
└── HTTPClient
```

### File Structure

```
easywifi/
├── src/
│   ├── easywifi.h       # Public API (declaraties)
│   ├── easywifi.cpp     # Implementatie (~1000 regels)
│   └── main.cpp         # Gebruiksvoorbeeld
├── platformio.ini       # PlatformIO configuratie
└── CLAUDE.md           # Deze documentatie
```

---

## Public API

### easywifi.h

```cpp
// Initialisatie
bool StartEasyWifi(const char* mdnsHost,
                   uint32_t staTimeoutMs = 20000,
                   uint32_t apIdleTimeoutMs = 900000);

// Status functies
int    GetWifiStrengthPercent();   // 0-100%
bool   IsWifiConnected();          // true als STA connected
String GetWifiIpString();          // IP address
String GetVersionString();         // Compile datum/tijd

// Loop functie (MOET in main loop worden aangeroepen)
void   EasyWifiLoop();
```

### Gebruik in main.cpp

```cpp
#include <Arduino.h>
#include "easywifi.h"

void setup() {
    StartEasyWifi("easywifi");  // mDNS hostname
}

void loop() {
    EasyWifiLoop();  // Verplicht!
}
```

---

## Custom Settings API

### Optie 1: Settings Storage (zonder GUI)

**Simpele opslag van custom instellingen in NVS:**

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

**Voorbeeld gebruik:**

```cpp
void setup() {
    StartEasyWifi("easywifi");

    // Sla settings op
    EasyWifiSetCustomString("api_key", "abc123");
    EasyWifiSetCustomInt("interval", 60);
    EasyWifiSetCustomBool("debug_mode", true);
}

void loop() {
    EasyWifiLoop();

    // Lees settings
    String apiKey = EasyWifiGetCustomString("api_key");
    int interval = EasyWifiGetCustomInt("interval");

    if (EasyWifiGetCustomBool("debug_mode")) {
        Serial.println("Debug info...");
    }
}
```

**Storage**: Aparte NVS namespace `"easywifi_cust"` (niet gemengd met WiFi settings)

### Optie 2: Custom Web Sections (met GUI)

**Voeg je eigen configuratie secties toe aan de web interface:**

```cpp
// Callback types
typedef std::function<String()> CustomHtmlCallback;           // Genereer HTML
typedef std::function<void(WebServer*)> CustomSaveCallback;   // Save handler

// Registreer custom sectie
void EasyWifiAddCustomSection(const char* title,
                              CustomHtmlCallback htmlCallback,
                              CustomSaveCallback saveCallback);
```

**Voorbeeld: MQTT Configuratie**

```cpp
// HTML generator voor MQTT settings
String getMqttSettingsHtml() {
    String server = EasyWifiGetCustomString("mqtt_server", "192.168.1.100");
    int port = EasyWifiGetCustomInt("mqtt_port", 1883);
    String topic = EasyWifiGetCustomString("mqtt_topic", "home/esp32");
    bool enabled = EasyWifiGetCustomBool("mqtt_enabled", false);

    String html = "";

    // Enable checkbox
    html += "<div class='checkbox-group'>";
    html += "<input type='checkbox' name='mqtt_enabled' value='true'";
    if (enabled) html += " checked";
    html += "><label>Enable MQTT</label></div>";

    // Server input
    html += "<div class='form-group'>";
    html += "<label>MQTT Server</label>";
    html += "<input type='text' name='mqtt_server' value='" + server + "'>";
    html += "</div>";

    // Port input
    html += "<div class='form-group'>";
    html += "<label>MQTT Port</label>";
    html += "<input type='text' name='mqtt_port' value='" + String(port) + "'>";
    html += "</div>";

    // Topic input
    html += "<div class='form-group'>";
    html += "<label>Topic Prefix</label>";
    html += "<input type='text' name='mqtt_topic' value='" + topic + "'>";
    html += "</div>";

    return html;
}

// Save handler voor MQTT settings
void saveMqttSettings(WebServer* server) {
    // Checkbox (unchecked = niet aanwezig in POST)
    EasyWifiSetCustomBool("mqtt_enabled", server->hasArg("mqtt_enabled"));

    if (server->hasArg("mqtt_server")) {
        EasyWifiSetCustomString("mqtt_server", server->arg("mqtt_server").c_str());
    }

    if (server->hasArg("mqtt_port")) {
        EasyWifiSetCustomInt("mqtt_port", server->arg("mqtt_port").toInt());
    }

    if (server->hasArg("mqtt_topic")) {
        EasyWifiSetCustomString("mqtt_topic", server->arg("mqtt_topic").c_str());
    }

    Serial.println("MQTT settings saved!");
}

void setup() {
    StartEasyWifi("easywifi");

    // Registreer MQTT settings sectie in web interface
    EasyWifiAddCustomSection("MQTT Settings", getMqttSettingsHtml, saveMqttSettings);

    // Gebruik de settings in je code
    if (EasyWifiGetCustomBool("mqtt_enabled")) {
        String server = EasyWifiGetCustomString("mqtt_server");
        int port = EasyWifiGetCustomInt("mqtt_port");
        String topic = EasyWifiGetCustomString("mqtt_topic");

        // Initialiseer MQTT client
        initMQTT(server, port, topic);
    }
}
```

### Web Interface Resultaat

Custom sections verschijnen automatisch in de web GUI tussen WiFi Config en Firmware Update:

```
┌─────────────────────────────┐
│  WiFi Configuration         │
└─────────────────────────────┘

┌─────────────────────────────┐
│  MQTT Settings             │  ← Custom sectie
│  ☑ Enable MQTT             │
│  [Server] [Port] [Topic]   │
│  [Save MQTT Settings]      │
└─────────────────────────────┘

┌─────────────────────────────┐
│  Device Info               │  ← Custom sectie
│  [Name] [Location]         │
│  [Save Device Info]        │
└─────────────────────────────┘

┌─────────────────────────────┐
│  Firmware Update           │
└─────────────────────────────┘
```

### Automatische Features

- ✅ **Formulier generatie** - Elke custom section krijgt automatisch een `<form>`
- ✅ **Save button** - "Save [Section Title]" per sectie
- ✅ **Success/Error messages** - Groene/rode feedback na opslaan
- ✅ **NVS storage** - Persistent opslag in `"easywifi_cust"` namespace
- ✅ **Serial logging** - Alle saves worden gelogd naar Serial Monitor
- ✅ **Dark mode** - Custom sections gebruiken automatisch het dark theme

### HTTP Endpoint

**Nieuwe endpoint**: `POST /customsave`
- Ontvangt alle custom form data via `application/x-www-form-urlencoded`
- Roept alle geregistreerde `CustomSaveCallback` functies aan
- Retourneert `200 OK` met success message

### Meerdere Custom Sections

Je kunt **onbeperkt** custom sections toevoegen:

```cpp
void setup() {
    StartEasyWifi("easywifi");

    // Registreer meerdere secties
    EasyWifiAddCustomSection("MQTT Settings", getMqttHtml, saveMqtt);
    EasyWifiAddCustomSection("Device Info", getDeviceHtml, saveDevice);
    EasyWifiAddCustomSection("Sensor Config", getSensorHtml, saveSensor);
    EasyWifiAddCustomSection("API Settings", getApiHtml, saveApi);
}
```

Elke sectie:
- Heeft eigen form
- Heeft eigen save button
- Heeft eigen message area
- Roept eigen save callback aan

### Best Practices

1. **Key naming**: Gebruik duidelijke prefixes
   ```cpp
   EasyWifiSetCustomString("mqtt_server", ...);  // ✓ Goed
   EasyWifiSetCustomString("server", ...);       // ✗ Onduidelijk
   ```

2. **Default values**: Geef altijd sensible defaults
   ```cpp
   String server = EasyWifiGetCustomString("mqtt_server", "localhost");  // ✓
   String server = EasyWifiGetCustomString("mqtt_server");                // ✗ Kan leeg zijn
   ```

3. **Input validation**: Valideer in je save callback
   ```cpp
   void saveMqtt(WebServer* server) {
       int port = server->arg("mqtt_port").toInt();
       if (port < 1 || port > 65535) {
           Serial.println("Invalid port!");
           return;
       }
       EasyWifiSetCustomInt("mqtt_port", port);
   }
   ```

4. **Checkboxes**: Unchecked checkboxes worden NIET gepost
   ```cpp
   // Checkbox detectie
   bool enabled = server->hasArg("my_checkbox");  // ✓ Correct
   bool enabled = server->arg("my_checkbox") == "true";  // ✗ Werkt niet als unchecked
   ```

---

## Functionele Werking

### 1. Boot Sequence

```
Power On
    ↓
Laad config uit NVS (Preferences)
    ↓
Credentials aanwezig? ──No──→ Start AP Mode
    ↓ Yes
Probeer te verbinden (20s timeout)
    ↓
Success? ──No──→ Start AP Mode
    ↓ Yes
Start STA Mode + mDNS + WebServer
```

### 2. STA Mode (Station - Client)

**Wanneer**: Bij succesvolle verbinding met opgeslagen WiFi

**Features**:
- Auto-connect bij boot
- DHCP of Static IP configuratie
- mDNS: toegang via `http://easywifi.local`
- WebServer op poort 80
- Live WiFi sterkte monitoring

**Network Config** (opgeslagen in NVS):
```
- SSID
- Password
- DHCP (bool)
- Static IP settings:
  - IP Address
  - Subnet Mask
  - Gateway
  - DNS1, DNS2 (optioneel)
```

### 3. AP Mode (Access Point)

**Wanneer**:
- Geen opgeslagen credentials
- Verbinding mislukt
- Handmatig via web interface

**AP Configuratie**:
- SSID: `EasyWiFi-XXXX` (XXXX = laatste 4 hex van MAC)
- Geen wachtwoord (open netwerk)
- IP: `192.168.4.1`
- mDNS: `http://easywifi.local` (mogelijk niet werkend in AP mode)

**AP Idle Timeout** (BELANGRIJK):
- Default: 15 minuten (900000 ms)
- Na timeout zonder HTTP activiteit: AP + WebServer stoppen
- **Geen bootloop**: Device blijft in safe low-power state
- User moet handmatig resetten om opnieuw te proberen
- Reden: voorkomt eindeloos AP mode bij verkeerde configuratie

---

## Web Interface

### Design

**Theme**: Dark Mode 🌙
- Achtergrond: `#1a1a2e` → `#16213e` gradient
- Container: `#1e1e2e`
- Accenten: Purple/Blue gradient (`#667eea` → `#764ba2`)
- Tekst: `#e0e0e0` (licht grijs)
- Modern, responsive, mobile-friendly

### HTTP Endpoints

```
GET  /           HTML configuratie pagina
GET  /scan       JSON lijst met SSIDs + RSSI
GET  /status     JSON status (connected, ssid, ip, rssi, %, mode, version)
POST /save       WiFi + network config opslaan
POST /update     OTA firmware upload
```

### Pagina Secties

#### 1. Live Status
- Connected/Disconnected
- SSID (huidige netwerk)
- IP Address
- Signal Strength (percentage)
- Auto-refresh: elke 10 seconden via `/status`

#### 2. WiFi Configuratie
- **Network Scanner**
  - Scan button
  - Lijst met SSIDs + RSSI in dBm
  - 🔒 icoon bij encrypted networks
  - Sorteer op sterkte (sterkste bovenaan)
  - Click om SSID te selecteren
- **Credentials**
  - SSID input (auto-fill from scan)
  - Password input
- **Network Settings**
  - DHCP checkbox (default: aan)
  - Static IP velden (toon alleen als DHCP uit):
    - IP Address
    - Subnet Mask
    - Gateway
    - DNS1, DNS2
  - Input validatie

#### 3. Firmware Update
- **File selector** (accept: `.bin`)
- **Upload progress**:
  - Progress bar (0-100%)
  - KB uploaded / Total KB
  - Status messages:
    1. "Uploading: X KB / Y KB"
    2. "Upload complete! Verifying..."
    3. "Firmware verified! Rebooting device..."
    4. "Device rebooting... reconnecting in X seconds"
  - Countdown timer (10s)
  - Auto-reload na reboot
- **Upload method**: XMLHttpRequest (for progress events)

### Status Polling

JavaScript fetches `/status` elke 10 seconden:

```json
{
  "connected": true,
  "ssid": "Area 51",
  "ip": "192.168.1.100",
  "rssi": -45,
  "strengthPercent": 90,
  "mode": "STA",
  "version": "Feb 14 2026 18:56:23"
}
```

---

## Implementatie Details

### NVS Storage (Preferences)

**Namespace**: `"easywifi"`

**Keys**:
```cpp
"ssid"      -> String
"password"  -> String
"dhcp"      -> bool
"ip"        -> String (IPAddress.toString())
"subnet"    -> String
"gateway"   -> String
"dns1"      -> String
"dns2"      -> String
```

**Operaties**:
- `loadConfig()`: Bij boot, read-only mode
- `saveConfig()`: Na web form submit, read-write mode

### WiFi Sterkte Conversie

```cpp
int GetWifiStrengthPercent() {
    int32_t rssi = WiFi.RSSI();

    // -30 dBm = 100%
    // -90 dBm = 0%
    if (rssi >= -50) return 100;
    if (rssi <= -100) return 0;

    return 2 * (rssi + 100);  // Linear mapping
}
```

### Version String

```cpp
const String VERSION_STRING = String(__DATE__) + " " + String(__TIME__);
// Voorbeeld: "Feb 14 2026 18:56:23"
```

### HTML Embedding

**Methode**: Raw string literal met custom delimiter

```cpp
String html = R"HTML(
<!DOCTYPE html>
<html>
...
</html>
)HTML";
```

**Waarom `HTML` delimiter?**
- Voorkomt conflicten met `)` in JavaScript code
- Standaard `R"(...)"` werkte niet door code in HTML

**Unicode in HTML**:
- ❌ Geen directe emoji's (compile errors)
- ✅ HTML entities: `&#128193;` (📁), `&#128274;` (🔒)
- ✅ JavaScript Unicode escapes: `\u{1F512}` (🔒)

### AP Idle Timeout Logic

```cpp
void EasyWifiLoop() {
    if (isApMode && apIdleTimeout > 0) {
        uint32_t idleTime = millis() - lastHttpRequest;

        // Handle millis() overflow
        if (millis() < lastHttpRequest) {
            lastHttpRequest = millis();
            idleTime = 0;
        }

        if (idleTime > apIdleTimeout && lastHttpRequest > 0) {
            // Stop web server netjes
            webServer->stop();
            delete webServer;

            // Stop AP
            WiFi.softAPdisconnect(true);
            WiFi.mode(WIFI_OFF);

            // GEEN restart -> voorkomt bootloop
        }
    }
}
```

### OTA Update Flow

```cpp
handleUpdateUpload() {
    if (UPLOAD_FILE_START) {
        Update.begin(UPDATE_SIZE_UNKNOWN);
    }
    else if (UPLOAD_FILE_WRITE) {
        Update.write(upload.buf, upload.currentSize);
    }
    else if (UPLOAD_FILE_END) {
        if (Update.end(true)) {
            // Success
            ESP.restart();  // Reboot after 1s delay
        }
    }
}
```

---

## Build & Deploy

### Hardware Setup

**ESP32 Board**: DOIT ESP32 DEVKIT V1
- **USB-Serial Chip**: CP2102 (Silicon Labs)
- **Driver**: CP210x VCP Driver (automatisch geïnstalleerd)
- **COM Port**: COM4 (kan variëren)

### PlatformIO Configuratie

**platformio.ini**:
```ini
[env:esp32doit-devkit-v1]
platform = espressif32
board = esp32doit-devkit-v1
framework = arduino
monitor_speed = 115200
```

**Dependencies**: Allemaal built-in ESP32 Arduino framework
- WiFi
- WebServer
- ESPmDNS
- Preferences
- Update
- HTTPClient

### Build Commands

```bash
# PlatformIO CLI (vanuit project root)
pio run                           # Build
pio run --target upload           # Upload (auto-detect port)
pio run --target upload --upload-port COM4  # Upload naar specifieke port
pio device monitor                # Serial monitor
```

**Build Stats** (laatste build met Custom Settings):
```
RAM:   14.4% (47112 / 327680 bytes)
Flash: 64.7% (848257 / 1310720 bytes)
```
*Inclusief: WiFi, WebServer, mDNS, OTA, NVS, Custom Settings API, Custom Web Sections*

### Upload Proces

1. **Driver Check**: CP2102 moet status "OK" hebben
2. **Port**: COM4 (of auto-detect)
3. **Baud Rate**: 460800 (upload), 115200 (monitor)
4. **Timing**: ~30 seconden voor build + upload

### Troubleshooting

**"Could not open COM4, port is busy"**
- Sluit Serial Monitor in VS Code
- Sluit Arduino IDE
- Check andere processen met `Get-PnpDevice`

**"Upload failed"**
- Druk EN HOUD BOOT knop tijdens upload
- Check USB kabel (sommige kabels zijn power-only)
- Verifieer CP2102 driver status

---

## Known Issues & Fixes

### Issue 1: Emoji's in C++ Code ❌

**Probleem**: Directe emoji's (`📁`, `🔒`) geven compile errors

**Oplossing**:
- HTML: `&#128193;` (folder), `&#128274;` (lock)
- JavaScript: `\u{1F512}` voor lock icon

### Issue 2: Verkeerde SSID in Status

**Probleem**: `WiFi.SSID()` zonder parameter geeft scan resultaat, niet verbonden netwerk

**Oplossing**: Gebruik `netConfig.ssid` voor verbonden netwerk in STA mode

```cpp
String currentSsid = "";
if (IsWifiConnected()) {
    currentSsid = netConfig.ssid;  // ✅ Correct
    // NIET: currentSsid = WiFi.SSID();  // ❌ Verkeerd
}
```

### Issue 3: Firmware Versie String

**Probleem**: Raw string literal onderbreking voor `VERSION_STRING` werkte niet

**Oplossing**: Dynamisch invullen via JavaScript + `/status` API

```html
<!-- HTML -->
<span id="firmwareVersion">Loading...</span>

<!-- JavaScript -->
document.getElementById('firmwareVersion').textContent = data.version;
```

---

## Future Improvements

### Mogelijke Uitbreidingen

1. **WiFi Multi-Connect**
   - Opslaan van meerdere netwerken
   - Auto-switch naar sterkste beschikbare netwerk

2. **MQTT Integration**
   - Configureerbare MQTT broker
   - Status publishing
   - Remote commands

3. **Authentication**
   - Login voor web interface
   - AP mode met wachtwoord

4. **Advanced Network**
   - WiFi power mode settings
   - Channel selection
   - WiFi sleep modes

5. **Logging**
   - Event log in NVS
   - Download logs via web

6. **Metrics**
   - WiFi uptime
   - Reconnect history
   - Signal strength graphs

---

## Belangrijke Ontwerpbeslissingen

### 1. WebServer vs AsyncWebServer
**Keuze**: WebServer (sync)
**Reden**: Eenvoudiger, minder dependencies, voldoende voor use case

### 2. Preferences vs SPIFFS
**Keuze**: Preferences (NVS)
**Reden**: Sneller, wear-leveling, geen filesystem overhead

### 3. AP Auto-Off vs Blijven Draaien
**Keuze**: Auto-off na 15 min
**Reden**: Voorkomt eeuwig AP mode, bespaart energie, geen bootloop

### 4. Dark Mode Default
**Keuze**: Dark mode als enige thema
**Reden**: Modern, minder energie (OLED), minder oogvermoeidheid

### 5. mDNS Hostname
**Keuze**: Configureerbaar via parameter
**Reden**: Flexibiliteit, meerdere devices op zelfde netwerk

---

## Testing Checklist

### Eerste Boot (Geen Credentials)
- [ ] Start in AP mode
- [ ] AP SSID = `EasyWiFi-XXXX`
- [ ] Web interface bereikbaar via `192.168.4.1`
- [ ] Network scan werkt
- [ ] Save & Connect opslaat in NVS

### STA Mode (Met Credentials)
- [ ] Auto-connect bij boot
- [ ] mDNS werkt (`http://easywifi.local`)
- [ ] Status toont juiste SSID
- [ ] Signal strength klopt
- [ ] Static IP configuratie werkt

### Web Interface
- [ ] Dark mode laadt correct
- [ ] Network scan toont SSIDs met 🔒
- [ ] Status update elke 10s
- [ ] Firmware versie toont compile datum/tijd
- [ ] DHCP/Static toggle werkt

### OTA Update
- [ ] Progress bar werkt
- [ ] Status messages kloppen
- [ ] Countdown timer werkt
- [ ] Auto-reload na reboot
- [ ] Nieuwe firmware draait

### AP Timeout
- [ ] AP stopt na 15 min inactiviteit
- [ ] Geen bootloop
- [ ] Device blijft in safe state

---

## Serial Monitor Output (Normaal)

```
=== EasyWifi Starting ===
Version: Feb 14 2026 18:56:23
mDNS Hostname: easywifi
Configuration loaded from NVS
  SSID: Area 51
  DHCP: Yes
Attempting to connect to: Area 51
..........
Successfully connected to WiFi!
IP Address: 192.168.1.100
mDNS started: http://easywifi.local
Web server started on port 80
```

---

## Referenties

### ESP32 Arduino Documentation
- https://docs.espressif.com/projects/arduino-esp32/

### Libraries
- WebServer: ESP32 Arduino core
- ESPmDNS: ESP32 Arduino core
- Preferences: ESP32 Arduino core

### Hardware
- ESP32-DOIT-DEVKIT-V1: https://docs.platformio.org/en/latest/boards/espressif32/esp32doit-devkit-v1.html
- CP2102 Driver: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

---

## Contact & Support

**Project Locatie**: `C:\Users\theki\Documents\GitHub\PlatformIO\easywifi\easywifi`

**Build Systeem**: PlatformIO Core

**COM Port**: COM4 (CP2102 USB to UART Bridge)

---

*Laatste update: 14 februari 2026*
*Build versie: Feb 14 2026 19:15:00 (voorbeeld)*
