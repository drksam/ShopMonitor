/**
 * ESP32 RFID Machine Monitor
 * PlatformIO Version with OTA Updates and Device Discovery
 * 
 * Features:
 * - RFID-based machine access control (4 zones)
 * - Activity monitoring and automatic logout
 * - Web configuration interface
 * - WiFi configuration with captive portal
 * - OTA firmware updates
 * - Automatic device discovery with mDNS
 * - Offline access with EEPROM storage for cards
 * - Activity counting with reporting to server
 */

#include <Arduino.h>
#include <WiFi.h>
#include <FastLED.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <ESPmDNS.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <FS.h>
#include <LittleFS.h>

// WiFi credentials
String ssid = "YourWiFi";       // Will be loaded from config
String password = "YourPass";    // Will be loaded from config
String serverIP = "10.0.0.1";    // Server IP address (will be loaded from config)
int serverPort = 5000;           // Server port (will be loaded from config)

// Device configuration
String deviceName = "ESP32-RFID";     // Device name (will be loaded from config)
String locationName = "Main Shop";    // Location name (will be loaded from config)
String macAddress = "";               // Will be populated during setup

// Firmware version
#define VERSION "1.2.0"               // Current firmware version

// RFID Module pin configuration for SPI
#define RST_PIN     22           // Reset pin
#define SS_PIN      21           // Slave Select pin (SDA)

// LED Configuration
#define NUM_LEDS    1            // Number of WS2812B LEDs per zone
CRGB leds[4][NUM_LEDS];          // LED arrays for 4 zones

// Pin definitions for ESP32
#define LED_PIN     5            // WS2812B LED data pin
#define RELAY_PIN_1 13           // Relay control pin for zone 1
#define RELAY_PIN_2 12           // Relay control pin for zone 2
#define RELAY_PIN_3 14           // Relay control pin for zone 3
#define RELAY_PIN_4 27           // Relay control pin for zone 4
#define ACTIVITY_PIN_1 36        // Activity monitoring pin for zone 1
#define ACTIVITY_PIN_2 39        // Activity monitoring pin for zone 2
#define ACTIVITY_PIN_3 34        // Activity monitoring pin for zone 3
#define ACTIVITY_PIN_4 35        // Activity monitoring pin for zone 4
#define BUZZER_PIN 15            // Optional buzzer for audio feedback

// RFID readers - up to 4 zones
MFRC522 rfidReaders[4] = {
  MFRC522(SS_PIN, RST_PIN),      // Zone 1 reader (primary)
  MFRC522(SS_PIN + 1, RST_PIN),  // Zone 2 reader (if equipped)
  MFRC522(SS_PIN + 2, RST_PIN),  // Zone 3 reader (if equipped)
  MFRC522(SS_PIN + 3, RST_PIN)   // Zone 4 reader (if equipped)
};

// Zone configuration
int activeZones = 1;             // Number of active zones (1-4, loaded from config)

// Relay pin array for 4 zones
const int relayPins[4] = {RELAY_PIN_1, RELAY_PIN_2, RELAY_PIN_3, RELAY_PIN_4};

// Activity pin array for 4 zones
const int activityPins[4] = {ACTIVITY_PIN_1, ACTIVITY_PIN_2, ACTIVITY_PIN_3, ACTIVITY_PIN_4};

// LED pin array for 4 zones
const int ledPins[4] = {LED_PIN, LED_PIN + 1, LED_PIN + 2, LED_PIN + 3};

// Machine IDs for 4 zones (loaded from config)
String machineIDs[4] = {"01", "02", "03", "04"};

// Active RFIDs for each zone
String activeRFIDs[4] = {"", "", "", ""};

// States for LED indication
enum Status { IDLE, ACCESS_GRANTED, ACCESS_DENIED, LOGGED_OUT, WARNING };
Status zoneStatus[4] = {IDLE, IDLE, IDLE, IDLE};

// Activity tracking
unsigned long lastActivityTime[4] = {0, 0, 0, 0};
unsigned long relayOffTime[4] = {0, 0, 0, 0};
unsigned long activityCount[4] = {0, 0, 0, 0};
unsigned long lastReportTime[4] = {0, 0, 0, 0};

// Timeouts
const unsigned long activityTimeout = 3600000;  // 1-hour timeout (ms)
const unsigned long warningTimeout = 300000;    // 5 minutes warning before timeout (ms)
const unsigned long relayOffDelay = 10000;      // 10-second delay for relay off (ms)
const unsigned long reportInterval = 600000;    // 10-minute report interval (ms)

// LED flashing state
bool flashingYellow[4] = {false, false, false, false};

// NTP Client for time synchronization
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Web server
AsyncWebServer server(80);

// Offline access
#define EEPROM_SIZE 1024
#define MAX_OFFLINE_CARDS 20
struct OfflineCard {
  char rfid[16];
  uint8_t zoneAccess;  // Bitmask for zone access (bit 0 = zone 1, etc.)
  bool adminOverride;  // Admin override flag
};
OfflineCard offlineCards[MAX_OFFLINE_CARDS];
int offlineCardCount = 0;

// Function declarations
bool loadConfig();
void saveConfig();
void setupRFID();
void setupWiFi();
void setupServer();
void setupMDNS();
void handleRFID(int zone);
void checkUserAccess(String rfid, int zone);
bool checkOfflineAccess(String rfid, int zone);
void grantAccess(String rfid, int zone);
void denyAccess(int zone);
void logOutUser(int zone);
void setLedColor(CRGB color, int zone);
void flashYellowWarning(int zone);
void checkActivityTimeouts();
void reportActivityCounts();
void syncOfflineCards();
void handleRoot(AsyncWebServerRequest *request);
void handleConfig(AsyncWebServerRequest *request);
void handleSaveConfig(AsyncWebServerRequest *request);
String getStatusJSON();
void setupLEDs();
void initOfflineCards();
void addOfflineCard(const char* rfid, uint8_t zoneAccess, bool adminOverride);
void clearOfflineCards();

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nESP32 RFID Machine Monitor starting...");
  
  // Initialize LittleFS
  if(!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed! Using default configuration.");
  } else {
    Serial.println("LittleFS mounted successfully.");
  }
  
  // Load configuration
  if (!loadConfig()) {
    Serial.println("Using default configuration values.");
  }
  
  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  initOfflineCards();
  
  // Configure pins
  for (int i = 0; i < activeZones; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW); // Start with relays off
    pinMode(activityPins[i], INPUT_PULLUP); // Activity pins with internal pullup
  }
  
  // Setup LEDs
  setupLEDs();
  
  // Setup WiFi
  setupWiFi();
  
  // Get MAC address
  macAddress = WiFi.macAddress();
  macAddress.replace(":", "");
  Serial.print("MAC Address: ");
  Serial.println(macAddress);
  
  // Start NTP Client
  timeClient.begin();
  timeClient.setTimeOffset(0);  // Set your timezone offset here
  
  // Setup RFID readers
  setupRFID();
  
  // Setup web server
  setupServer();
  
  // Setup mDNS for discovery
  setupMDNS();
  
  // Initial sync for offline cards
  syncOfflineCards();
  
  Serial.println("Setup complete!");
}

void loop() {
  // Check each RFID reader
  for (int zone = 0; zone < activeZones; zone++) {
    handleRFID(zone);
  }
  
  // Check for activity on each zone
  for (int zone = 0; zone < activeZones; zone++) {
    if (digitalRead(activityPins[zone]) == LOW && activeRFIDs[zone] != "") {
      // Machine is active
      lastActivityTime[zone] = millis();
      activityCount[zone]++;  // Increment activity counter
    }
    
    // Check for inactivity warning
    if (activeRFIDs[zone] != "" && 
        millis() - lastActivityTime[zone] > (activityTimeout - warningTimeout) && 
        zoneStatus[zone] == ACCESS_GRANTED) {
      flashingYellow[zone] = true;
    }
  }
  
  // Check for timeouts
  checkActivityTimeouts();
  
  // Handle LED flashing
  for (int zone = 0; zone < activeZones; zone++) {
    if (flashingYellow[zone]) {
      flashYellowWarning(zone);
    }
  }
  
  // Report activity counts periodically
  reportActivityCounts();
  
  // Update NTP time
  timeClient.update();
  
  // Allow web server to process requests
  delay(50);
}

void setupLEDs() {
  // Setup WS2812B LEDs for each zone
  for (int zone = 0; zone < activeZones; zone++) {
    FastLED.addLeds<NEOPIXEL, LED_PIN>(leds[zone], NUM_LEDS);
    setLedColor(CRGB::Blue, zone);  // Default color (waiting for input)
  }
}

void setupRFID() {
  SPI.begin();
  for (int zone = 0; zone < activeZones; zone++) {
    rfidReaders[zone].PCD_Init();
    rfidReaders[zone].PCD_DumpVersionToSerial();
    Serial.print("Zone ");
    Serial.print(zone + 1);
    Serial.println(" RFID Reader initialized.");
  }
}

void setupWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid.c_str(), password.c_str());
  
  // Try to connect with timeout
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Connected to WiFi. IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi. Operating in offline mode.");
    // In a real application, you might want to start a captive portal here
    // for configuration or use a different fallback mechanism
  }
}

void setupMDNS() {
  if (MDNS.begin(deviceName.c_str())) {
    Serial.println("mDNS responder started");
    MDNS.addService("rfid-monitor", "tcp", 80);
    
    // Set up TXT records for discovery
    MDNS.addServiceTxt("rfid-monitor", "tcp", "mac", macAddress.c_str());
    MDNS.addServiceTxt("rfid-monitor", "tcp", "name", deviceName.c_str());
    MDNS.addServiceTxt("rfid-monitor", "tcp", "location", locationName.c_str());
    MDNS.addServiceTxt("rfid-monitor", "tcp", "zone_count", String(activeZones).c_str());
    
    // Add machine IDs to TXT records
    for (int i = 0; i < activeZones; i++) {
      MDNS.addServiceTxt("rfid-monitor", "tcp", String("machine").c_str() + String(i).c_str(), machineIDs[i].c_str());
    }
  } else {
    Serial.println("Error setting up mDNS responder!");
  }
}

void setupServer() {
  // Setup routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleRoot(request);
  });
  
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    handleConfig(request);
  });
  
  server.on("/save_config", HTTP_POST, [](AsyncWebServerRequest *request) {
    handleSaveConfig(request);
  });
  
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", getStatusJSON());
  });
  
  server.on("/sync", HTTP_GET, [](AsyncWebServerRequest *request) {
    syncOfflineCards();
    request->send(200, "text/plain", "Offline cards synchronized");
  });
  
  server.on("/logout_all", HTTP_GET, [](AsyncWebServerRequest *request) {
    for (int zone = 0; zone < activeZones; zone++) {
      if (activeRFIDs[zone] != "") {
        logOutUser(zone);
      }
    }
    request->send(200, "text/plain", "All users logged out");
  });
  
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Device restarting...");
    delay(1000);
    ESP.restart();
  });
  
  // OTA Update route
  AsyncElegantOTA.begin(&server);
  
  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void handleRoot(AsyncWebServerRequest *request) {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>" + deviceName + " - Machine Monitor</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; color: #333; }";
  html += ".container { max-width: 1200px; margin: 0 auto; padding: 20px; }";
  html += "header { background-color: #333; color: white; padding: 1rem; }";
  html += "h1 { margin: 0; font-size: 1.5rem; }";
  html += ".card { background: white; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin-bottom: 20px; overflow: hidden; }";
  html += ".card-header { background: #f0f0f0; padding: 15px; font-weight: bold; border-bottom: 1px solid #ddd; }";
  html += ".card-body { padding: 15px; }";
  html += ".status { padding: 15px; display: flex; justify-content: space-between; border-bottom: 1px solid #eee; }";
  html += ".status-name { font-weight: bold; }";
  html += ".badge { display: inline-block; padding: 3px 8px; border-radius: 12px; font-size: 0.8rem; }";
  html += ".badge-success { background-color: #d4edda; color: #155724; }";
  html += ".badge-danger { background-color: #f8d7da; color: #721c24; }";
  html += ".badge-warning { background-color: #fff3cd; color: #856404; }";
  html += ".badge-info { background-color: #d1ecf1; color: #0c5460; }";
  html += ".btn { display: inline-block; padding: 8px 16px; background: #007bff; color: white; text-decoration: none; border-radius: 4px; margin-top: 10px; }";
  html += ".btn:hover { background: #0069d9; }";
  html += ".btn-warning { background: #ffc107; color: #333; }";
  html += ".btn-danger { background: #dc3545; }";
  html += ".btn-secondary { background: #6c757d; }";
  html += ".btn-group { display: flex; gap: 10px; margin-top: 20px; }";
  html += "</style>";
  html += "</head><body>";
  html += "<header><h1>" + deviceName + " - " + locationName + "</h1></header>";
  html += "<div class='container'>";
  
  // Status card
  html += "<div class='card'>";
  html += "<div class='card-header'>Status</div>";
  html += "<div class='card-body'>";
  
  // Display status for each zone
  for (int zone = 0; zone < activeZones; zone++) {
    html += "<div class='status'>";
    html += "<div class='status-name'>Zone " + String(zone + 1) + " (Machine " + machineIDs[zone] + ")</div>";
    
    // Display status badge
    html += "<div>";
    if (activeRFIDs[zone] != "") {
      html += "<span class='badge badge-success'>Active - User: " + activeRFIDs[zone] + "</span>";
    } else {
      html += "<span class='badge badge-info'>Idle</span>";
    }
    html += "</div>";
    
    html += "</div>";
  }
  
  html += "</div>"; // card-body
  html += "</div>"; // card
  
  // Action buttons
  html += "<div class='btn-group'>";
  html += "<a href='/config' class='btn'>Settings</a>";
  html += "<a href='/sync' class='btn btn-warning'>Sync Offline Cards</a>";
  html += "<a href='/logout_all' class='btn btn-danger'>Logout All Users</a>";
  html += "<a href='/update' class='btn btn-secondary'>Update Firmware</a>";
  html += "</div>";
  
  html += "</div>"; // container
  html += "</body></html>";
  
  request->send(200, "text/html", html);
}

void handleConfig(AsyncWebServerRequest *request) {
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>" + deviceName + " - Configuration</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; color: #333; }";
  html += ".container { max-width: 800px; margin: 0 auto; padding: 20px; }";
  html += "header { background-color: #333; color: white; padding: 1rem; }";
  html += "h1 { margin: 0; font-size: 1.5rem; }";
  html += ".card { background: white; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin-bottom: 20px; overflow: hidden; }";
  html += ".card-header { background: #f0f0f0; padding: 15px; font-weight: bold; border-bottom: 1px solid #ddd; }";
  html += ".card-body { padding: 15px; }";
  html += "form { display: grid; gap: 15px; }";
  html += ".form-group { display: grid; gap: 5px; }";
  html += "label { font-weight: bold; }";
  html += "input, select { padding: 8px; border: 1px solid #ddd; border-radius: 4px; }";
  html += ".btn { display: inline-block; padding: 10px 16px; background: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; }";
  html += ".btn:hover { background: #0069d9; }";
  html += ".btn-secondary { background: #6c757d; }";
  html += "</style>";
  html += "</head><body>";
  html += "<header><h1>" + deviceName + " - Configuration</h1></header>";
  html += "<div class='container'>";
  
  // Configuration form
  html += "<div class='card'>";
  html += "<div class='card-header'>Device Settings</div>";
  html += "<div class='card-body'>";
  
  html += "<form action='/save_config' method='post'>";
  
  // Device name
  html += "<div class='form-group'>";
  html += "<label for='deviceName'>Device Name:</label>";
  html += "<input type='text' id='deviceName' name='deviceName' value='" + deviceName + "' required>";
  html += "</div>";
  
  // Location name
  html += "<div class='form-group'>";
  html += "<label for='locationName'>Location Name:</label>";
  html += "<input type='text' id='locationName' name='locationName' value='" + locationName + "' required>";
  html += "</div>";
  
  // WiFi settings
  html += "<div class='form-group'>";
  html += "<label for='ssid'>WiFi SSID:</label>";
  html += "<input type='text' id='ssid' name='ssid' value='" + ssid + "' required>";
  html += "</div>";
  
  html += "<div class='form-group'>";
  html += "<label for='password'>WiFi Password:</label>";
  html += "<input type='password' id='password' name='password' value='" + password + "'>";
  html += "</div>";
  
  // Server settings
  html += "<div class='form-group'>";
  html += "<label for='serverIP'>Server IP:</label>";
  html += "<input type='text' id='serverIP' name='serverIP' value='" + serverIP + "' required>";
  html += "</div>";
  
  html += "<div class='form-group'>";
  html += "<label for='serverPort'>Server Port:</label>";
  html += "<input type='number' id='serverPort' name='serverPort' value='" + String(serverPort) + "' required>";
  html += "</div>";
  
  // Active zones
  html += "<div class='form-group'>";
  html += "<label for='activeZones'>Active Zones:</label>";
  html += "<select id='activeZones' name='activeZones'>";
  for (int i = 1; i <= 4; i++) {
    html += "<option value='" + String(i) + "'" + (activeZones == i ? " selected" : "") + ">" + String(i) + "</option>";
  }
  html += "</select>";
  html += "</div>";
  
  // Machine IDs
  for (int i = 0; i < 4; i++) {
    html += "<div class='form-group'>";
    html += "<label for='machineID" + String(i) + "'>Machine ID (Zone " + String(i+1) + "):</label>";
    html += "<input type='text' id='machineID" + String(i) + "' name='machineID" + String(i) + "' value='" + machineIDs[i] + "' maxlength='2'>";
    html += "</div>";
  }
  
  // Submit and back buttons
  html += "<div style='display: flex; gap: 10px;'>";
  html += "<button type='submit' class='btn'>Save Configuration</button>";
  html += "<a href='/' class='btn btn-secondary' style='text-decoration: none;'>Back</a>";
  html += "</div>";
  
  html += "</form>";
  html += "</div>"; // card-body
  html += "</div>"; // card
  
  html += "</div>"; // container
  html += "</body></html>";
  
  request->send(200, "text/html", html);
}

void handleSaveConfig(AsyncWebServerRequest *request) {
  // Get parameters
  if (request->hasParam("deviceName", true)) {
    deviceName = request->getParam("deviceName", true)->value();
  }
  
  if (request->hasParam("locationName", true)) {
    locationName = request->getParam("locationName", true)->value();
  }
  
  if (request->hasParam("ssid", true)) {
    ssid = request->getParam("ssid", true)->value();
  }
  
  if (request->hasParam("password", true)) {
    password = request->getParam("password", true)->value();
  }
  
  if (request->hasParam("serverIP", true)) {
    serverIP = request->getParam("serverIP", true)->value();
  }
  
  if (request->hasParam("serverPort", true)) {
    serverPort = request->getParam("serverPort", true)->value().toInt();
  }
  
  if (request->hasParam("activeZones", true)) {
    activeZones = request->getParam("activeZones", true)->value().toInt();
  }
  
  // Machine IDs
  for (int i = 0; i < 4; i++) {
    String paramName = "machineID" + String(i);
    if (request->hasParam(paramName, true)) {
      machineIDs[i] = request->getParam(paramName, true)->value();
    }
  }
  
  // Save configuration
  saveConfig();
  
  // Send response
  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Configuration Saved</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; color: #333; }";
  html += ".container { max-width: 800px; margin: 0 auto; padding: 20px; text-align: center; }";
  html += ".card { background: white; border-radius: 5px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin-bottom: 20px; padding: 20px; }";
  html += ".alert { padding: 15px; background-color: #d4edda; color: #155724; border-radius: 4px; margin-bottom: 20px; }";
  html += ".btn { display: inline-block; padding: 10px 16px; background: #007bff; color: white; text-decoration: none; border-radius: 4px; }";
  html += "</style>";
  html += "<meta http-equiv='refresh' content='5;url=/' />";
  html += "</head><body>";
  html += "<div class='container'>";
  html += "<div class='card'>";
  html += "<div class='alert'>Configuration saved successfully! Redirecting...</div>";
  html += "<p>The device will use the new settings on the next restart.</p>";
  html += "<a href='/' class='btn'>Back to Home</a>";
  html += "</div>";
  html += "</div>";
  html += "</body></html>";
  
  request->send(200, "text/html", html);
}

bool loadConfig() {
  if (LittleFS.exists("/config.json")) {
    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      size_t size = configFile.size();
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, buf.get());
      
      if (!error) {
        deviceName = doc["deviceName"].as<String>();
        locationName = doc["locationName"].as<String>();
        ssid = doc["ssid"].as<String>();
        password = doc["password"].as<String>();
        serverIP = doc["serverIP"].as<String>();
        serverPort = doc["serverPort"];
        activeZones = doc["activeZones"];
        
        for (int i = 0; i < 4; i++) {
          machineIDs[i] = doc["machineID" + String(i)].as<String>();
        }
        
        Serial.println("Loaded configuration from /config.json");
        return true;
      } else {
        Serial.println("Failed to parse config file");
      }
      configFile.close();
    }
  }
  return false;
}

void saveConfig() {
  DynamicJsonDocument doc(1024);
  
  doc["deviceName"] = deviceName;
  doc["locationName"] = locationName;
  doc["ssid"] = ssid;
  doc["password"] = password;
  doc["serverIP"] = serverIP;
  doc["serverPort"] = serverPort;
  doc["activeZones"] = activeZones;
  
  for (int i = 0; i < 4; i++) {
    doc["machineID" + String(i)] = machineIDs[i];
  }
  
  File configFile = LittleFS.open("/config.json", "w");
  if (configFile) {
    serializeJson(doc, configFile);
    configFile.close();
    Serial.println("Saved configuration to /config.json");
  } else {
    Serial.println("Failed to open config file for writing");
  }
  
  // Update mDNS settings
  MDNS.end();
  setupMDNS();
}

void handleRFID(int zone) {
  // Check if new card present
  if (rfidReaders[zone].PICC_IsNewCardPresent() && rfidReaders[zone].PICC_ReadCardSerial()) {
    // Extract the RFID ID
    String rfid = "";
    for (byte i = 0; i < rfidReaders[zone].uid.size; i++) {
      rfid += (rfidReaders[zone].uid.uidByte[i] < 0x10 ? "0" : "");
      rfid += String(rfidReaders[zone].uid.uidByte[i], HEX);
    }
    rfid.toUpperCase();
    
    Serial.print("Zone ");
    Serial.print(zone + 1);
    Serial.print(" RFID detected: ");
    Serial.println(rfid);
    
    // Check if it's a logout card (special system card)
    if (rfid == "LOGOUT" || rfid == "FF00FF00FF") {
      if (activeRFIDs[zone] != "") {
        logOutUser(zone);
      }
    } else {
      // Check user access
      checkUserAccess(rfid, zone);
    }
    
    // Halt PICC and stop encryption
    rfidReaders[zone].PICC_HaltA();
    rfidReaders[zone].PCD_StopCrypto1();
  }
}

void checkUserAccess(String rfid, int zone) {
  bool accessGranted = false;
  
  // If connected to WiFi, try online verification
  if (WiFi.status() == WL_CONNECTED) {
    // Create HTTP client
    WiFiClient client;
    // Updated API endpoint to match new PC-side application
    String url = "/api/check_user?rfid=" + rfid + "&machine_id=" + machineIDs[zone];
    
    if (client.connect(serverIP.c_str(), serverPort)) {
      // Send a GET request to the server
      client.print("GET " + url + " HTTP/1.1\r\n");
      client.print("Host: " + serverIP + "\r\n");
      client.print("Connection: close\r\n\r\n");
      
      unsigned long timeout = millis();
      while (client.available() == 0) {
        if (millis() - timeout > 5000) {
          Serial.println("Client Timeout");
          client.stop();
          break;
        }
      }
      
      // Read response
      while (client.available()) {
        String line = client.readStringUntil('\r');
        if (line.indexOf("ALLOW") != -1) {
          if (activeRFIDs[zone] != "" && activeRFIDs[zone] != rfid) {
            // If there's an active user and the RFID is different, log out the current user
            logOutUser(zone);
            delay(500); // Delay to ensure logout is fully processed
          }
          accessGranted = true;
          break;
        } else if (line.indexOf("LOGOUT") != -1) {
          logOutUser(zone);
          return;
        } else if (line.indexOf("DENY") != -1) {
          accessGranted = false;
          break;
        }
      }
      client.stop();
    } else {
      Serial.println("Connection to server failed. Checking offline access.");
      accessGranted = checkOfflineAccess(rfid, zone);
    }
  } else {
    // No WiFi connection, use offline access
    Serial.println("No WiFi connection. Checking offline access.");
    accessGranted = checkOfflineAccess(rfid, zone);
  }
  
  if (accessGranted) {
    grantAccess(rfid, zone);
  } else {
    denyAccess(zone);
  }
}

bool checkOfflineAccess(String rfid, int zone) {
  // Convert rfid to char array for comparison
  char rfidArr[16];
  rfid.toCharArray(rfidArr, sizeof(rfidArr));
  
  // Check if rfid is in offline cards list
  for (int i = 0; i < offlineCardCount; i++) {
    if (strcmp(offlineCards[i].rfid, rfidArr) == 0) {
      // Check if card has access to this zone or has admin override
      if (offlineCards[i].adminOverride || (offlineCards[i].zoneAccess & (1 << zone))) {
        return true;
      }
    }
  }
  
  return false;
}

void grantAccess(String rfid, int zone) {
  zoneStatus[zone] = ACCESS_GRANTED;
  activeRFIDs[zone] = rfid;
  digitalWrite(relayPins[zone], HIGH); // Activate relay
  setLedColor(CRGB::Green, zone);      // Set LED to green
  
  Serial.print("Access granted in Zone ");
  Serial.print(zone + 1);
  Serial.print(" for RFID: ");
  Serial.println(rfid);
  
  lastActivityTime[zone] = millis();   // Reset activity timer
  flashingYellow[zone] = false;        // Stop warning if active
}

void denyAccess(int zone) {
  zoneStatus[zone] = ACCESS_DENIED;
  setLedColor(CRGB::Red, zone);        // Set LED to red
  
  Serial.print("Access denied in Zone ");
  Serial.println(zone + 1);
  
  // After a delay, go back to idle
  delay(2000);
  zoneStatus[zone] = IDLE;
  setLedColor(CRGB::Blue, zone);
}

void logOutUser(int zone) {
  if (activeRFIDs[zone] != "") {
    zoneStatus[zone] = LOGGED_OUT;
    
    // Report final activity count
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      // Updated API endpoint to match new PC-side application
      String url = "/api/update_count?rfid=" + activeRFIDs[zone] + 
                  "&machine_id=" + machineIDs[zone] + 
                  "&count=" + String(activityCount[zone]);
      
      if (client.connect(serverIP.c_str(), serverPort)) {
        client.print("GET " + url + " HTTP/1.1\r\n");
        client.print("Host: " + serverIP + "\r\n");
        client.print("Connection: close\r\n\r\n");
        
        // Wait for response
        while (client.connected()) {
          if (client.available()) {
            client.read();
          }
        }
        client.stop();
      }
    }
    
    relayOffTime[zone] = millis();  // Record time for delayed relay off
    setLedColor(CRGB::Blue, zone);  // Set LED to blue
    
    Serial.print("User logged out from Zone ");
    Serial.println(zone + 1);
    
    // Send logout request to server
    if (WiFi.status() == WL_CONNECTED) {
      WiFiClient client;
      // Updated API endpoint to match new PC-side application
      String url = "/api/logout?rfid=" + activeRFIDs[zone] + "&machine_id=" + machineIDs[zone];
      
      if (client.connect(serverIP.c_str(), serverPort)) {
        client.print("GET " + url + " HTTP/1.1\r\n");
        client.print("Host: " + serverIP + "\r\n");
        client.print("Connection: close\r\n\r\n");
        
        // Wait for response
        while (client.connected()) {
          if (client.available()) {
            client.read();
          }
        }
        client.stop();
      }
    }
    
    activeRFIDs[zone] = "";          // Clear active RFID
    activityCount[zone] = 0;         // Reset activity counter
    flashingYellow[zone] = false;    // Stop warning
    
    // Turn off relay after delay
    delay(relayOffDelay);
    digitalWrite(relayPins[zone], LOW);
  }
}

void setLedColor(CRGB color, int zone) {
  leds[zone][0] = color;
  FastLED.show();
}

void flashYellowWarning(int zone) {
  static unsigned long lastFlashTime[4] = {0, 0, 0, 0};
  static bool ledState[4] = {false, false, false, false};
  
  if (millis() - lastFlashTime[zone] > 500) {  // Toggle every 500ms
    ledState[zone] = !ledState[zone];
    leds[zone][0] = ledState[zone] ? CRGB::Yellow : CRGB::Black;
    FastLED.show();
    lastFlashTime[zone] = millis();
  }
}

void checkActivityTimeouts() {
  // Check each zone for timeouts
  for (int zone = 0; zone < activeZones; zone++) {
    // Handle timeout - auto logout after inactivity
    if (activeRFIDs[zone] != "" && 
        millis() - lastActivityTime[zone] > activityTimeout && 
        zoneStatus[zone] == ACCESS_GRANTED) {
      Serial.print("Activity timeout in Zone ");
      Serial.println(zone + 1);
      logOutUser(zone);
    }
    
    // Handle relay off delay
    if (zoneStatus[zone] == LOGGED_OUT && 
        millis() - relayOffTime[zone] > relayOffDelay) {
      digitalWrite(relayPins[zone], LOW);  // Turn off relay
      zoneStatus[zone] = IDLE;
    }
  }
}

void reportActivityCounts() {
  // Check if it's time to report activity counts
  for (int zone = 0; zone < activeZones; zone++) {
    if (activeRFIDs[zone] != "" && 
        millis() - lastReportTime[zone] > reportInterval && 
        activityCount[zone] > 0) {
      
      // Report activity to server
      if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;
        // Updated API endpoint to match new PC-side application
        String url = "/api/update_count?rfid=" + activeRFIDs[zone] + 
                    "&machine_id=" + machineIDs[zone] + 
                    "&count=" + String(activityCount[zone]);
        
        if (client.connect(serverIP.c_str(), serverPort)) {
          client.print("GET " + url + " HTTP/1.1\r\n");
          client.print("Host: " + serverIP + "\r\n");
          client.print("Connection: close\r\n\r\n");
          
          // Wait for response
          while (client.connected()) {
            if (client.available()) {
              client.read();
            }
          }
          client.stop();
          
          // Reset activity count after reporting
          activityCount[zone] = 0;
          lastReportTime[zone] = millis();
          
          Serial.print("Reported activity for Zone ");
          Serial.println(zone + 1);
        }
      }
    }
  }
}

void syncOfflineCards() {
  // Only sync if WiFi is connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot sync offline cards - WiFi not connected");
    return;
  }
  
  WiFiClient client;
  // Updated API endpoint to match new PC-side application
  String url = "/api/offline_cards";
  
  if (client.connect(serverIP.c_str(), serverPort)) {
    client.print("GET " + url + " HTTP/1.1\r\n");
    client.print("Host: " + serverIP + "\r\n");
    client.print("Connection: close\r\n\r\n");
    
    String response = "";
    bool jsonStarted = false;
    
    // Read response
    while (client.connected() || client.available()) {
      if (client.available()) {
        char c = client.read();
        if (c == '{') {
          jsonStarted = true;
        }
        if (jsonStarted) {
          response += c;
        }
      }
    }
    
    client.stop();
    
    if (response.length() > 0) {
      // Parse JSON response
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error) {
        // Clear current offline cards
        clearOfflineCards();
        
        // Add cards from response
        JsonArray cards = doc["cards"].as<JsonArray>();
        for (JsonObject card : cards) {
          String rfid = card["rfid"].as<String>();
          uint8_t zoneAccess = 0;
          
          JsonArray zones = card["zones"].as<JsonArray>();
          for (int zone : zones) {
            // Set bit for zone (0-indexed)
            zoneAccess |= (1 << (zone - 1));
          }
          
          bool adminOverride = card["admin_override"].as<bool>();
          
          // Add to offline cards array
          char rfidArr[16];
          rfid.toCharArray(rfidArr, sizeof(rfidArr));
          addOfflineCard(rfidArr, zoneAccess, adminOverride);
        }
        
        Serial.print("Synced ");
        Serial.print(offlineCardCount);
        Serial.println(" offline cards");
      } else {
        Serial.println("Error parsing JSON response");
      }
    }
  } else {
    Serial.println("Failed to connect to server for offline card sync");
  }
}

void initOfflineCards() {
  // Read offline card count from EEPROM
  offlineCardCount = EEPROM.read(0);
  
  // Sanity check
  if (offlineCardCount > MAX_OFFLINE_CARDS) {
    offlineCardCount = 0;
    EEPROM.write(0, 0);
    EEPROM.commit();
  }
  
  // Read offline cards from EEPROM
  for (int i = 0; i < offlineCardCount; i++) {
    int offset = 1 + (i * sizeof(OfflineCard));
    EEPROM.get(offset, offlineCards[i]);
  }
  
  Serial.print("Loaded ");
  Serial.print(offlineCardCount);
  Serial.println(" offline cards from EEPROM");
}

void addOfflineCard(const char* rfid, uint8_t zoneAccess, bool adminOverride) {
  if (offlineCardCount < MAX_OFFLINE_CARDS) {
    // Check if card already exists
    for (int i = 0; i < offlineCardCount; i++) {
      if (strcmp(offlineCards[i].rfid, rfid) == 0) {
        // Update existing card
        offlineCards[i].zoneAccess = zoneAccess;
        offlineCards[i].adminOverride = adminOverride;
        
        // Save to EEPROM
        int offset = 1 + (i * sizeof(OfflineCard));
        EEPROM.put(offset, offlineCards[i]);
        EEPROM.commit();
        return;
      }
    }
    
    // Add new card
    strcpy(offlineCards[offlineCardCount].rfid, rfid);
    offlineCards[offlineCardCount].zoneAccess = zoneAccess;
    offlineCards[offlineCardCount].adminOverride = adminOverride;
    
    // Save to EEPROM
    int offset = 1 + (offlineCardCount * sizeof(OfflineCard));
    EEPROM.put(offset, offlineCards[offlineCardCount]);
    
    offlineCardCount++;
    EEPROM.write(0, offlineCardCount);
    EEPROM.commit();
  }
}

void clearOfflineCards() {
  offlineCardCount = 0;
  EEPROM.write(0, 0);
  EEPROM.commit();
}

String getStatusJSON() {
  DynamicJsonDocument doc(1024);
  
  doc["name"] = deviceName;
  doc["location"] = locationName;
  doc["ip"] = WiFi.localIP().toString();
  doc["mac"] = macAddress;
  doc["rssi"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["firmware"] = VERSION;
  doc["zone_count"] = activeZones;
  
  JsonArray machinesArray = doc.createNestedArray("machines");
  
  for (int zone = 0; zone < activeZones; zone++) {
    JsonObject machine = machinesArray.createNestedObject();
    machine["zone"] = zone + 1;
    machine["machine_id"] = machineIDs[zone];
    
    if (activeRFIDs[zone] != "") {
      machine["status"] = "active";
      machine["rfid"] = activeRFIDs[zone];
      machine["active_time"] = (millis() - lastActivityTime[zone]) / 1000;
    } else {
      machine["status"] = "idle";
    }
  }
  
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  return jsonOutput;
}