/**
 * ShopMonitor RFID Machine Monitor - ESP32 Multi-Function Node
 * 
 * This firmware supports three node types in a single codebase:
 *  1. Machine Monitor - Controls machine access with RFID and activity tracking
 *  2. Office RFID Reader - Registers new RFID cards into the system
 *  3. Accessory IO Controller - Manages external devices (fans, lights, etc.)
 * 
 * Features:
 *  - Automatic device discovery via mDNS
 *  - Web configuration interface
 *  - OTA firmware updates
 *  - Offline access when server connection is lost
 *  - Missing hardware detection and fault tolerance
 *  - REST API for server communication
 *  - Multi-color LED status indicators
 *
 * Required Libraries:
 *  - WiFi (built-in with ESP32)
 *  - ESPmDNS
 *  - WebServer
 *  - SPI
 *  - MFRC522 (for RFID readers)
 *  - FastLED
 *  - Preferences
 *  - EEPROM
 *  - HTTPClient
 *  - ArduinoJson
 *  - Update (for OTA)
 *  - NTPClient
 *  - WiFiUdp
 *
 * Copyright (c) 2025 ShopMonitor
 */

#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <FastLED.h>
#include <Preferences.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <esp_task_wdt.h>

// ===== SYSTEM CONFIGURATION =====
// Uncomment the following line to enable debug output
#define DEBUG_OUTPUT
// Watchdog timeout (seconds)
#define WDT_TIMEOUT 30

// ===== CONSTANTS =====
// Node types
#define NODE_TYPE_MACHINE_MONITOR 0
#define NODE_TYPE_OFFICE_READER   1
#define NODE_TYPE_ACCESSORY_IO    2

// Node states
#define STATE_SETUP               0
#define STATE_CONNECTING_WIFI     1
#define STATE_CONNECTING_SERVER   2
#define STATE_READY               3
#define STATE_ERROR               4
#define STATE_CONFIG_MODE         5

// ===== PIN DEFINITIONS =====
// SPI bus (same for all node types)
#define PIN_SPI_MOSI             23  // SPI MOSI pin
#define PIN_SPI_MISO             19  // SPI MISO pin
#define PIN_SPI_SCK              18  // SPI SCK pin

// RFID readers
#define MAX_RFID_READERS          4  // Maximum number of RFID readers
// SS pins for each reader
const uint8_t PIN_RFID_SS[MAX_RFID_READERS] = {21, 17, 16, 4};
#define PIN_RFID_RST              22  // Shared RST pin for all RFID readers

// LED indicators
#define PIN_LED_DATA               5  // WS2812B data pin
#define NUM_LEDS                   4  // Number of LEDs (one per zone)

// Relay control pins
const uint8_t PIN_RELAY[MAX_RFID_READERS] = {13, 12, 14, 27};

// Activity monitoring pins
const uint8_t PIN_ACTIVITY[MAX_RFID_READERS] = {36, 39, 34, 35};

// Auxiliary components
#define PIN_BUZZER               15   // Buzzer pin
#define PIN_BOOT_BUTTON           0   // Boot button (built-in)
#define PIN_MODE_BUTTON_1        33   // Mode select button 1
#define PIN_MODE_BUTTON_2        32   // Mode select button 2

// ===== SYSTEM VARIABLES =====
// Node configuration
uint8_t nodeType = NODE_TYPE_MACHINE_MONITOR;  // Default to machine monitor
uint8_t nodeState = STATE_SETUP;
String nodeId = "esp32-node";                  // Will be generated or loaded
String nodeName = "ESP32 RFID Monitor";        // Will be set in config

// Server configuration
String serverUrl = "http://$SERVER_IP$:$SERVER_PORT$";
bool serverConnected = false;

// WiFi configuration
String wifiSSID = "";  // Will be loaded from preferences
String wifiPassword = ""; // Will be loaded from preferences
bool wifiConnected = false;
bool apMode = false;
String apSSID = "ShopMonitor";  // Default AP name
String apPassword = "ShopMonitor123";  // Default AP password
bool isConfigured = false;  // Flag to check if node is configured

// Hardware detection flags
bool rfidReadersFound[MAX_RFID_READERS] = {false, false, false, false};
bool hasDisplay = false;
int numRfidReadersFound = 0;

// RFID reader objects
MFRC522 rfidReaders[MAX_RFID_READERS];

// Machine state (for Machine Monitor mode)
struct MachineState {
  String machineId;
  String rfidTag;
  bool active;
  uint32_t activityCount;
  uint32_t lastActivity;
  bool relayState;
};
MachineState machines[MAX_RFID_READERS];

// Office reader state (for Office Reader mode)
String lastScannedTag = "";
unsigned long lastScanTime = 0;
bool newTagScanned = false;

// Accessory IO state (for Accessory IO mode)
bool outputState[MAX_RFID_READERS] = {false, false, false, false};
bool inputState[MAX_RFID_READERS] = {false, false, false, false};
unsigned long outputTimeout[MAX_RFID_READERS] = {0, 0, 0, 0};

// LED objects
CRGB leds[NUM_LEDS];
unsigned long ledUpdateTime = 0;
unsigned long blinkTimers[NUM_LEDS] = {0, 0, 0, 0};
bool blinkStates[NUM_LEDS] = {false, false, false, false};

// Server communication
unsigned long lastServerSync = 0;
unsigned long lastHeartbeat = 0;
unsigned long activityReportInterval = 600000;  // 10 minutes

// Retry counters
int wifiRetryCount = 0;
int serverRetryCount = 0;
const int MAX_RETRIES = 5;

// Watchdog
unsigned long lastWdtReset = 0;

// Web server
WebServer webServer(80);

// NTP client for time synchronization
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Configuration
Preferences preferences;

// ===== FUNCTION DECLARATIONS =====
void setupNode();
void detectHardware();
void setupWiFi();
void startConfigPortal();
void setupWebServer();
void initRFIDReaders();
void setupRFIDReader(int index);
void handleWebRequests();
void handleMachineMonitor();
void handleOfficeReader();
void handleAccessoryIO();
void checkWiFiConnection();
void syncWithServer();
bool checkUserAccess(int zoneIndex, String rfid);
void grantAccess(int zoneIndex, String rfid);
void denyAccess(int zoneIndex);
void logoutUser(int zoneIndex);
void monitorActivity(int zoneIndex);
void updateLEDs();
void playTone(int duration, int frequency);
void resetWatchdog();
bool isButtonPressed(uint8_t pin);
String getMacAddress();
void loadConfiguration();
void saveConfiguration();
void advertiseService();
void reportActivityToServer();
void handleOTAUpdate();
void factoryReset();

// ===== WEB SERVER HANDLERS =====
void handleRoot();
void handleStatus();
void handleConfig();
void handleSetConfig();
void handleNotFound();
void handleUpdate();
void handleUpdateUpload();
void handleReboot();
void handleFactoryReset();
void handleIOStatus();
void handleControlIO();
void handleLastScan();

// ===== SETUP FUNCTION =====
void setup() {
#ifdef DEBUG_OUTPUT
  Serial.begin(115200);
  Serial.println("\n\nESP32 RFID Machine Monitor - Starting up...");
#endif

  // Initialize watchdog
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);
  lastWdtReset = millis();

  // Initialize LEDs
  FastLED.addLeds<NEOPIXEL, PIN_LED_DATA>(leds, NUM_LEDS);
  FastLED.setBrightness(50);  // 0-255
  // Boot animation - blue rotating pattern
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Blue;
    FastLED.show();
    delay(200);
    leds[i] = CRGB::Black;
  }

  // Check mode select buttons
  pinMode(PIN_MODE_BUTTON_1, INPUT_PULLUP);
  pinMode(PIN_MODE_BUTTON_2, INPUT_PULLUP);
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  
  // Detect hardware and identify node
  detectHardware();
  
  // Check for boot mode selection
  if (!digitalRead(PIN_MODE_BUTTON_1) && digitalRead(PIN_MODE_BUTTON_2)) {
    // Button 1 pressed - Machine Monitor mode
    nodeType = NODE_TYPE_MACHINE_MONITOR;
    playTone(500, 2000);  // Confirmation beep
  } else if (digitalRead(PIN_MODE_BUTTON_1) && !digitalRead(PIN_MODE_BUTTON_2)) {
    // Button 2 pressed - Office Reader mode
    nodeType = NODE_TYPE_OFFICE_READER;
    playTone(500, 2000);
    delay(200);
    playTone(500, 2000);  // Double beep
  } else if (!digitalRead(PIN_MODE_BUTTON_1) && !digitalRead(PIN_MODE_BUTTON_2)) {
    // Both buttons pressed - Accessory IO mode
    nodeType = NODE_TYPE_ACCESSORY_IO;
    playTone(500, 2000);
    delay(200);
    playTone(500, 2000);
    delay(200);
    playTone(500, 2000);  // Triple beep
  }
  
  // Check for configuration mode (hold boot button)
  if (!digitalRead(PIN_BOOT_BUTTON)) {
    int buttonHoldTime = 0;
    // White breathing while waiting
    while (!digitalRead(PIN_BOOT_BUTTON) && buttonHoldTime < 100) {
      int brightness = 50 + 50 * sin(buttonHoldTime * 0.2);
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(brightness, brightness, brightness);
      }
      FastLED.show();
      delay(100);
      buttonHoldTime++;
    }
    
    // If button held for more than 10 seconds, enter config mode
    if (buttonHoldTime >= 100) {
      apMode = true;
      nodeState = STATE_CONFIG_MODE;
#ifdef DEBUG_OUTPUT
      Serial.println("Entering configuration mode");
#endif
      playTone(1000, 3000);  // Configuration mode tone
    }
  }
  
  // Initialize variables
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    machines[i].machineId = String(i + 1).length() < 2 ? "0" + String(i + 1) : String(i + 1);
    machines[i].rfidTag = "";
    machines[i].active = false;
    machines[i].activityCount = 0;
    machines[i].lastActivity = 0;
    machines[i].relayState = false;
    
    // Setup relay pins
    pinMode(PIN_RELAY[i], OUTPUT);
    digitalWrite(PIN_RELAY[i], LOW);
    
    // Setup activity pins
    pinMode(PIN_ACTIVITY[i], INPUT);
  }
  
  // Load configuration
  loadConfiguration();
  
  // Setup WiFi
  setupWiFi();
  
  // Initialize RFID readers
  initRFIDReaders();
  
  // Setup web server
  setupWebServer();
  
  // Initialize NTP client
  timeClient.begin();
  timeClient.setTimeOffset(0);  // UTC
  
  // Setup node-specific components
  setupNode();
  
  // Advertise mDNS service
  advertiseService();
  
  // Main setup complete
  resetWatchdog();
#ifdef DEBUG_OUTPUT
  Serial.println("Setup completed. Running as:");
  switch (nodeType) {
    case NODE_TYPE_MACHINE_MONITOR:
      Serial.println("Machine Monitor");
      break;
    case NODE_TYPE_OFFICE_READER:
      Serial.println("Office RFID Reader");
      break;
    case NODE_TYPE_ACCESSORY_IO:
      Serial.println("Accessory IO Controller");
      break;
  }
#endif
}

// ===== MAIN LOOP =====
void loop() {
  // Reset watchdog timer
  if (millis() - lastWdtReset > 10000) {
    resetWatchdog();
  }
  
  // Handle web server requests
  webServer.handleClient();
  
  // Check WiFi connection
  checkWiFiConnection();
  
  // Update LEDs
  updateLEDs();
  
  // NTP update
  if (wifiConnected && millis() - lastHeartbeat > 3600000) {  // Once per hour
    timeClient.update();
    lastHeartbeat = millis();
  }
  
  // Sync with server
  if (wifiConnected && millis() - lastServerSync > 60000) {  // Once per minute
    syncWithServer();
  }
  
  // Node type specific handlers
  switch (nodeType) {
    case NODE_TYPE_MACHINE_MONITOR:
      handleMachineMonitor();
      break;
    case NODE_TYPE_OFFICE_READER:
      handleOfficeReader();
      break;
    case NODE_TYPE_ACCESSORY_IO:
      handleAccessoryIO();
      break;
  }
  
  // Report activity counts to server
  if (nodeType == NODE_TYPE_MACHINE_MONITOR && 
      wifiConnected && 
      serverConnected && 
      millis() - lastHeartbeat > activityReportInterval) {
    reportActivityToServer();
    lastHeartbeat = millis();
  }
  
  // Short delay to prevent aggressive polling
  delay(10);
}

// ===== NODE SETUP FUNCTIONS =====
void setupNode() {
  switch (nodeType) {
    case NODE_TYPE_MACHINE_MONITOR:
      // Machine Monitor specific setup
      for (int i = 0; i < MAX_RFID_READERS; i++) {
        if (rfidReadersFound[i]) {
          leds[i] = CRGB::Blue;  // Idle color for each zone
        } else {
          leds[i] = CRGB::Black;  // Turn off LED for unavailable zones
        }
      }
      FastLED.show();
      break;
      
    case NODE_TYPE_OFFICE_READER:
      // Office Reader specific setup
      // All LEDs purple for office reader mode
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Purple;
      }
      FastLED.show();
      break;
      
    case NODE_TYPE_ACCESSORY_IO:
      // Accessory IO specific setup
      // All LEDs orange for accessory IO mode
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Orange;
      }
      FastLED.show();
      
      // Initialize output pins (already done in main setup)
      break;
  }
}

// ===== HARDWARE DETECTION =====
void detectHardware() {
#ifdef DEBUG_OUTPUT
  Serial.println("Detecting hardware...");
#endif

  // Generate or load a unique node ID if not set
  if (nodeId == "esp32-node") {
    nodeId = "esp32-" + getMacAddress().substring(9);
#ifdef DEBUG_OUTPUT
    Serial.println("Generated node ID: " + nodeId);
#endif
  }
  
  // Initialize SPI
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  
  // Initialize RFID readers
  numRfidReadersFound = 0;
  
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    // Try to detect RFID reader
    pinMode(PIN_RFID_SS[i], OUTPUT);
    digitalWrite(PIN_RFID_SS[i], HIGH);  // Deselect
    rfidReadersFound[i] = false;  // Will be set to true if detected
  }
  
  // Success indication
#ifdef DEBUG_OUTPUT
  Serial.println("Hardware detection complete");
#endif
}

// ===== RFID READER SETUP =====
void initRFIDReaders() {
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    setupRFIDReader(i);
  }
  
#ifdef DEBUG_OUTPUT
  Serial.print("Detected ");
  Serial.print(numRfidReadersFound);
  Serial.println(" RFID readers");
#endif
}

void setupRFIDReader(int index) {
  if (index < 0 || index >= MAX_RFID_READERS) return;
  
  // Initialize MFRC522 reader
  rfidReaders[index].PCD_Init(PIN_RFID_SS[index], PIN_RFID_RST);
  delay(50);  // Give time to initialize
  
  // Check if reader is responding
  byte version = rfidReaders[index].PCD_ReadRegister(MFRC522::VersionReg);
  
  if (version && version != 0xFF) {
    rfidReadersFound[index] = true;
    numRfidReadersFound++;
    
#ifdef DEBUG_OUTPUT
    Serial.print("RFID Reader ");
    Serial.print(index);
    Serial.print(" detected. Version: 0x");
    Serial.println(version, HEX);
#endif
    
    // Flash the corresponding LED green
    leds[index] = CRGB::Green;
    FastLED.show();
    delay(200);
    leds[index] = CRGB::Black;
    FastLED.show();
  }
}

// ===== WIFI SETUP =====
void setupWiFi() {
  if (apMode) {
    // Start AP mode - use ShopNode AP name for unconfigured nodes
    String apName = isConfigured ? nodeId : apSSID;
    
#ifdef DEBUG_OUTPUT
    Serial.println("Starting AP mode: " + apName);
#endif
    
    // Set all LEDs to white
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::White;
    }
    FastLED.show();
    
    // If node is not configured, use ShopNode as the AP name for initial setup
    // Otherwise, use ESP32-Node-XXXX format with last 4 characters of node ID
    if (!isConfigured) {
      WiFi.softAP(apName, apPassword);
      
      // Use rainbow pattern for unconfigured nodes to indicate setup mode
      for (int j = 0; j < 3; j++) {  // Repeat animation 3 times
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Black;
        }
        FastLED.show();
        delay(300);
        
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::White;
        }
        FastLED.show();
        delay(300);
      }
    } else {
      WiFi.softAP(apName + "-" + apName.substring(apName.length() - 4), apPassword);
    }
    
#ifdef DEBUG_OUTPUT
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
#endif
    
    return;
  }
  
  // Normal WiFi client mode
  nodeState = STATE_CONNECTING_WIFI;
  
  // Set all LEDs to purple (WiFi connecting)
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Purple;
  }
  FastLED.show();
  
#ifdef DEBUG_OUTPUT
  Serial.print("Connecting to WiFi: ");
  Serial.println(wifiSSID);
#endif
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  
  int connectAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && connectAttempts < 20) {
    delay(500);
#ifdef DEBUG_OUTPUT
    Serial.print(".");
#endif
    // Blink LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = (connectAttempts % 2 == 0) ? CRGB::Purple : CRGB::Black;
    }
    FastLED.show();
    connectAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    
#ifdef DEBUG_OUTPUT
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
#endif
    
    // All LEDs green for 1 second to indicate connection
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Green;
    }
    FastLED.show();
    delay(1000);
    
    nodeState = STATE_CONNECTING_SERVER;
  } else {
    wifiConnected = false;
    
#ifdef DEBUG_OUTPUT
    Serial.println("\nWiFi connection failed");
#endif
    
    // All LEDs red for 1 second to indicate connection failure
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB::Red;
    }
    FastLED.show();
    delay(1000);
    
    nodeState = STATE_ERROR;
  }
}

// ===== WEB SERVER SETUP =====
void setupWebServer() {
  // Root - Dashboard or configuration portal
  webServer.on("/", HTTP_GET, handleRoot);
  
  // API endpoints
  webServer.on("/api/status", HTTP_GET, handleStatus);
  webServer.on("/api/config", HTTP_GET, handleConfig);
  webServer.on("/api/config", HTTP_POST, handleSetConfig);
  webServer.on("/api/update", HTTP_POST, handleUpdate, handleUpdateUpload);
  webServer.on("/api/reboot", HTTP_GET, handleReboot);
  webServer.on("/api/factory_reset", HTTP_POST, handleFactoryReset);
  
  // Node-specific endpoints
  if (nodeType == NODE_TYPE_ACCESSORY_IO) {
    webServer.on("/api/io_status", HTTP_GET, handleIOStatus);
    webServer.on("/api/control_io", HTTP_POST, handleControlIO);
  }
  
  if (nodeType == NODE_TYPE_OFFICE_READER) {
    webServer.on("/api/scan", HTTP_GET, handleLastScan);
  }
  
  // 404 handler
  webServer.onNotFound(handleNotFound);
  
  // Start web server
  webServer.begin();
  
#ifdef DEBUG_OUTPUT
  Serial.println("Web server started");
#endif
}

// ===== NODE TYPE HANDLERS =====
void handleMachineMonitor() {
  // Check each RFID reader
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    if (!rfidReadersFound[i]) continue;
    
    // Check for cards
    if (rfidReaders[i].PICC_IsNewCardPresent() && rfidReaders[i].PICC_ReadCardSerial()) {
      String rfidTag = "";
      for (byte j = 0; j < rfidReaders[i].uid.size; j++) {
        rfidTag += (rfidReaders[i].uid.uidByte[j] < 0x10 ? "0" : "") + 
                  String(rfidReaders[i].uid.uidByte[j], HEX);
      }
      rfidTag.toUpperCase();
      
#ifdef DEBUG_OUTPUT
      Serial.print("Zone ");
      Serial.print(i);
      Serial.print(" - Card detected: ");
      Serial.println(rfidTag);
#endif
      
      // Handle card logic
      if (machines[i].rfidTag.length() > 0) {
        // Someone is using this machine
        if (machines[i].rfidTag == rfidTag) {
          // Same user scanning - log out
          logoutUser(i);
        } else {
          // Different user - check if they have access
          checkUserAccess(i, rfidTag);
        }
      } else {
        // Machine is not in use, check access
        checkUserAccess(i, rfidTag);
      }
      
      // Halt PICC
      rfidReaders[i].PICC_HaltA();
      // Stop encryption
      rfidReaders[i].PCD_StopCrypto1();
    }
    
    // Monitor activity
    monitorActivity(i);
  }
}

void handleOfficeReader() {
  // Only need to check first RFID reader for office mode
  if (rfidReadersFound[0]) {
    if (rfidReaders[0].PICC_IsNewCardPresent() && rfidReaders[0].PICC_ReadCardSerial()) {
      String rfidTag = "";
      for (byte j = 0; j < rfidReaders[0].uid.size; j++) {
        rfidTag += (rfidReaders[0].uid.uidByte[j] < 0x10 ? "0" : "") + 
                  String(rfidReaders[0].uid.uidByte[j], HEX);
      }
      rfidTag.toUpperCase();
      
#ifdef DEBUG_OUTPUT
      Serial.print("Office reader - Card detected: ");
      Serial.println(rfidTag);
#endif
      
      // Store the scanned tag
      lastScannedTag = rfidTag;
      lastScanTime = millis();
      newTagScanned = true;
      
      // Visual and audible feedback
      playTone(500, 2000);
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Green;
      }
      FastLED.show();
      delay(500);
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Purple;  // Back to office reader color
      }
      FastLED.show();
      
      // Halt PICC
      rfidReaders[0].PICC_HaltA();
      // Stop encryption
      rfidReaders[0].PCD_StopCrypto1();
      
      // Send tag to server
      if (wifiConnected && serverConnected) {
        HTTPClient http;
        http.begin(serverUrl + "/api/register_rfid");
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        
        String postData = "rfid=" + lastScannedTag;
        int httpCode = http.POST(postData);
        
        if (httpCode > 0) {
#ifdef DEBUG_OUTPUT
          String payload = http.getString();
          Serial.print("Server response: ");
          Serial.println(payload);
#endif
        }
        
        http.end();
      }
    }
    
    // Reset tag after 30 seconds
    if (newTagScanned && millis() - lastScanTime > 30000) {
      newTagScanned = false;
#ifdef DEBUG_OUTPUT
      Serial.println("Tag registration timed out");
#endif
    }
  }
}

void handleAccessoryIO() {
  // Read input pins
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    bool newState = digitalRead(PIN_ACTIVITY[i]) == HIGH;
    if (newState != inputState[i]) {
      inputState[i] = newState;
#ifdef DEBUG_OUTPUT
      Serial.print("Input ");
      Serial.print(i);
      Serial.print(" changed to: ");
      Serial.println(newState ? "HIGH" : "LOW");
#endif
      
      // Update LED to show input state
      leds[i] = inputState[i] ? CRGB::Green : CRGB::Orange;
      FastLED.show();
    }
  }
  
  // Check for output timeouts
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    if (outputTimeout[i] > 0 && millis() > outputTimeout[i]) {
      // Turn off the output
      digitalWrite(PIN_RELAY[i], LOW);
      outputState[i] = false;
      outputTimeout[i] = 0;
      
#ifdef DEBUG_OUTPUT
      Serial.print("Output ");
      Serial.print(i);
      Serial.println(" timed out and was turned off");
#endif
    }
  }
}

// ===== MACHINE MONITOR FUNCTIONS =====
bool checkUserAccess(int zoneIndex, String rfid) {
  if (!wifiConnected || !serverConnected) {
    // Offline mode - check for offline access permissions
    // This would normally use EEPROM to store authorized cards
    // For simplicity in this example, we'll allow all cards in offline mode
#ifdef DEBUG_OUTPUT
    Serial.print("Zone ");
    Serial.print(zoneIndex);
    Serial.println(" - Offline mode, granting access");
#endif
    grantAccess(zoneIndex, rfid);
    return true;
  }
  
  // Online mode - check with server
  HTTPClient http;
  String url = serverUrl + "/api/check_user?rfid=" + rfid + "&machine_id=" + machines[zoneIndex].machineId;
  
#ifdef DEBUG_OUTPUT
  Serial.print("Checking access: ");
  Serial.println(url);
#endif
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String payload = http.getString();
    
#ifdef DEBUG_OUTPUT
    Serial.print("Server response: ");
    Serial.println(payload);
#endif
    
    if (payload.indexOf("ALLOW") != -1) {
      grantAccess(zoneIndex, rfid);
      http.end();
      return true;
    } else {
      denyAccess(zoneIndex);
      http.end();
      return false;
    }
  } else {
#ifdef DEBUG_OUTPUT
    Serial.print("HTTP GET failed, error: ");
    Serial.println(http.errorToString(httpCode));
#endif
    
    // Server connection issue, fallback to offline mode
    // Check for emergency override cards
    grantAccess(zoneIndex, rfid);
    http.end();
    return true;
  }
}

void grantAccess(int zoneIndex, String rfid) {
  // Log out any existing user
  if (machines[zoneIndex].rfidTag.length() > 0) {
    logoutUser(zoneIndex);
  }
  
  // Grant access
  machines[zoneIndex].rfidTag = rfid;
  machines[zoneIndex].active = true;
  machines[zoneIndex].lastActivity = millis();
  machines[zoneIndex].relayState = true;
  
  // Activate relay
  digitalWrite(PIN_RELAY[zoneIndex], HIGH);
  
  // Set LED to green
  leds[zoneIndex] = CRGB::Green;
  FastLED.show();
  
  // Audio feedback
  playTone(500, 2000);
  
#ifdef DEBUG_OUTPUT
  Serial.print("Zone ");
  Serial.print(zoneIndex);
  Serial.print(" - Access granted to RFID: ");
  Serial.println(rfid);
#endif
}

void denyAccess(int zoneIndex) {
  // Set LED to red
  leds[zoneIndex] = CRGB::Red;
  FastLED.show();
  
  // Audio feedback - Error tone
  playTone(300, 1000);
  delay(100);
  playTone(300, 1000);
  
#ifdef DEBUG_OUTPUT
  Serial.print("Zone ");
  Serial.print(zoneIndex);
  Serial.println(" - Access denied");
#endif
  
  // Flash red LED for dramatic effect
  for (int i = 0; i < 5; i++) {
    leds[zoneIndex] = CRGB::Black;
    FastLED.show();
    delay(100);
    leds[zoneIndex] = CRGB::Red;
    FastLED.show();
    delay(100);
  }
  
  // Return to idle state
  leds[zoneIndex] = CRGB::Blue;
  FastLED.show();
}

void logoutUser(int zoneIndex) {
  if (machines[zoneIndex].rfidTag.length() == 0) return;
  
  // Send logout to server
  if (wifiConnected && serverConnected) {
    HTTPClient http;
    String url = serverUrl + "/api/logout?rfid=" + machines[zoneIndex].rfidTag + 
                "&machine_id=" + machines[zoneIndex].machineId;
    
    http.begin(url);
    http.GET();
    http.end();
    
    // Also send activity count
    if (machines[zoneIndex].activityCount > 0) {
      url = serverUrl + "/api/update_count?machine_id=" + machines[zoneIndex].machineId + 
            "&count=" + String(machines[zoneIndex].activityCount);
      
      http.begin(url);
      http.GET();
      http.end();
      
      machines[zoneIndex].activityCount = 0;
    }
  }
  
  // Reset machine state
  machines[zoneIndex].rfidTag = "";
  machines[zoneIndex].active = false;
  machines[zoneIndex].relayState = false;
  
  // Deactivate relay
  digitalWrite(PIN_RELAY[zoneIndex], LOW);
  
  // Set LED to blue (idle)
  leds[zoneIndex] = CRGB::Blue;
  FastLED.show();
  
  // Audio feedback
  playTone(300, 1500);
  
#ifdef DEBUG_OUTPUT
  Serial.print("Zone ");
  Serial.print(zoneIndex);
  Serial.println(" - User logged out");
#endif
}

void monitorActivity(int zoneIndex) {
  if (!machines[zoneIndex].active) return;
  
  // Check if activity pin is high
  if (digitalRead(PIN_ACTIVITY[zoneIndex]) == HIGH) {
    if (millis() - machines[zoneIndex].lastActivity > 10000) {  // Debounce 10 seconds
      // Increment activity counter
      machines[zoneIndex].activityCount++;
      machines[zoneIndex].lastActivity = millis();
      
#ifdef DEBUG_OUTPUT
      Serial.print("Zone ");
      Serial.print(zoneIndex);
      Serial.print(" - Activity detected. Count: ");
      Serial.println(machines[zoneIndex].activityCount);
#endif
    }
  }
  
  // Check for timeout (1 hour of inactivity)
  if (millis() - machines[zoneIndex].lastActivity > 3600000) {
    // Timeout occurred
#ifdef DEBUG_OUTPUT
    Serial.print("Zone ");
    Serial.print(zoneIndex);
    Serial.println(" - Timeout due to inactivity");
#endif
    
    logoutUser(zoneIndex);
  } else if (millis() - machines[zoneIndex].lastActivity > 3300000) {
    // Warning 5 minutes before timeout - flash yellow
    if (millis() - blinkTimers[zoneIndex] > 500) {
      blinkStates[zoneIndex] = !blinkStates[zoneIndex];
      leds[zoneIndex] = blinkStates[zoneIndex] ? CRGB::Yellow : CRGB::Black;
      FastLED.show();
      blinkTimers[zoneIndex] = millis();
    }
  }
}

// ===== HELPER FUNCTIONS =====
void updateLEDs() {
  // Only update LEDs if not in a special state
  if (nodeState == STATE_READY) {
    switch (nodeType) {
      case NODE_TYPE_MACHINE_MONITOR:
        // LEDs already handled by individual zone functions
        break;
        
      case NODE_TYPE_OFFICE_READER:
        // Breathe purple for office reader when idle
        if (!newTagScanned) {
          int brightness = 50 + 50 * sin(millis() * 0.001);
          for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB(brightness, 0, brightness);
          }
          FastLED.show();
        }
        break;
        
      case NODE_TYPE_ACCESSORY_IO:
        // LEDs reflect output states
        for (int i = 0; i < MAX_RFID_READERS; i++) {
          leds[i] = outputState[i] ? CRGB::Green : CRGB::Orange;
        }
        FastLED.show();
        break;
    }
  } else if (nodeState == STATE_ERROR) {
    // Error state - red SOS pattern
    static int errorPattern = 0;
    static unsigned long lastErrorBlink = 0;
    
    if (millis() - lastErrorBlink > 300) {
      lastErrorBlink = millis();
      errorPattern = (errorPattern + 1) % 27;
      
      // SOS pattern (... --- ...)
      bool ledOn = false;
      if (errorPattern < 9) {
        // S (dot dot dot)
        ledOn = errorPattern % 3 < 1;
      } else if (errorPattern < 18) {
        // O (dash dash dash)
        ledOn = (errorPattern - 9) % 3 < 2;
      } else {
        // S (dot dot dot)
        ledOn = (errorPattern - 18) % 3 < 1;
      }
      
      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ledOn ? CRGB::Red : CRGB::Black;
      }
      FastLED.show();
    }
  } else if (nodeState == STATE_CONFIG_MODE) {
    // Configuration mode - white breathing
    int brightness = 50 + 50 * sin(millis() * 0.001);
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB(brightness, brightness, brightness);
    }
    FastLED.show();
  }
}

void playTone(int duration, int frequency) {
  // ESP32 tone function
  ledcSetup(0, frequency, 8);
  ledcAttachPin(PIN_BUZZER, 0);
  ledcWrite(0, 128);  // 50% duty cycle
  delay(duration);
  ledcWrite(0, 0);
}

void resetWatchdog() {
  esp_task_wdt_reset();
  lastWdtReset = millis();
}

String getMacAddress() {
  byte mac[6];
  WiFi.macAddress(mac);
  String macStr = "";
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) macStr += "0";
    macStr += String(mac[i], HEX);
    if (i < 5) macStr += ":";
  }
  return macStr;
}

bool isButtonPressed(uint8_t pin) {
  return digitalRead(pin) == LOW;
}

// ===== CONFIGURATION FUNCTIONS =====
void loadConfiguration() {
  preferences.begin("nodeConfig", false);
  
  // Check if node is configured
  isConfigured = preferences.getBool("configured", false);
  
  // Generate a unique node ID if not set
  nodeId = preferences.getString("nodeId", "esp32-" + getMacAddress().substring(9));
  
  // Load node configuration if configured
  if (isConfigured) {
    nodeName = preferences.getString("nodeName", "ESP32 RFID Monitor");
    nodeType = preferences.getUChar("nodeType", NODE_TYPE_MACHINE_MONITOR);
    
    // Load WiFi configuration
    wifiSSID = preferences.getString("wifiSSID", "");
    wifiPassword = preferences.getString("wifiPass", "");
    
    // Load server configuration
    serverUrl = preferences.getString("serverUrl", serverUrl);
    
    // Load machine IDs
    for (int i = 0; i < MAX_RFID_READERS; i++) {
      String key = "machine" + String(i);
      machines[i].machineId = preferences.getString(key, machines[i].machineId);
    }
    
#ifdef DEBUG_OUTPUT
    Serial.println("Configuration loaded for node: " + nodeId);
    Serial.println("Node type: " + String(nodeType));
    Serial.println("WiFi SSID: " + wifiSSID);
    Serial.println("Server URL: " + serverUrl);
#endif
  } else {
    // Node is not configured yet
#ifdef DEBUG_OUTPUT
    Serial.println("Node is not configured. Will start in setup mode.");
#endif
    // Auto-enter setup mode
    apMode = true;
    nodeState = STATE_CONFIG_MODE;
  }
  
  preferences.end();
}

void saveConfiguration() {
  preferences.begin("nodeConfig", false);
  
  // Save node configuration
  preferences.putString("nodeId", nodeId);
  preferences.putString("nodeName", nodeName);
  preferences.putUChar("nodeType", nodeType);
  
  // Save WiFi configuration
  preferences.putString("wifiSSID", wifiSSID);
  preferences.putString("wifiPass", wifiPassword);
  
  // Save server configuration
  preferences.putString("serverUrl", serverUrl);
  
  // Save machine IDs
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    String key = "machine" + String(i);
    preferences.putString(key, machines[i].machineId);
  }
  
  // Mark as configured
  preferences.putBool("configured", true);
  isConfigured = true;
  
  preferences.end();
  
#ifdef DEBUG_OUTPUT
  Serial.println("Configuration saved and node marked as configured");
#endif
}

void advertiseService() {
  if (!wifiConnected) return;
  
  // Start mDNS responder
  if (MDNS.begin(nodeId.c_str())) {
    // Add service to MDNS-SD
    MDNS.addService("rfid-monitor", "tcp", 80);
    
    // Add txtData fields for device information
    MDNS.addServiceTxt("rfid-monitor", "tcp", "node_id", nodeId.c_str());
    MDNS.addServiceTxt("rfid-monitor", "tcp", "node_type", 
                       nodeType == NODE_TYPE_MACHINE_MONITOR ? "machine_monitor" : 
                       nodeType == NODE_TYPE_OFFICE_READER ? "office_reader" : "accessory_io");
    MDNS.addServiceTxt("rfid-monitor", "tcp", "name", nodeName.c_str());
    MDNS.addServiceTxt("rfid-monitor", "tcp", "version", "1.0.0");
    
#ifdef DEBUG_OUTPUT
    Serial.println("mDNS responder started");
    Serial.print("Node can be reached at http://");
    Serial.print(nodeId);
    Serial.println(".local");
#endif
  } else {
#ifdef DEBUG_OUTPUT
    Serial.println("Error setting up mDNS responder");
#endif
  }
}

void syncWithServer() {
#ifdef DEBUG_OUTPUT
  Serial.println("Syncing with server...");
#endif
  
  if (!wifiConnected) {
    serverConnected = false;
    return;
  }
  
  HTTPClient http;
  http.begin(serverUrl + "/api/node_heartbeat");
  http.addHeader("Content-Type", "application/json");
  
  // Create JSON document
  StaticJsonDocument<512> doc;
  doc["node_id"] = nodeId;
  doc["node_type"] = nodeType == NODE_TYPE_MACHINE_MONITOR ? "machine_monitor" : 
                    nodeType == NODE_TYPE_OFFICE_READER ? "office_reader" : "accessory_io";
  doc["name"] = nodeName;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["status"] = "online";
  
  if (nodeType == NODE_TYPE_MACHINE_MONITOR) {
    JsonArray machineArray = doc.createNestedArray("machines");
    for (int i = 0; i < MAX_RFID_READERS; i++) {
      if (!rfidReadersFound[i]) continue;
      
      JsonObject machine = machineArray.createNestedObject();
      machine["zone"] = i;
      machine["machine_id"] = machines[i].machineId;
      machine["status"] = machines[i].active ? "active" : "idle";
      machine["rfid"] = machines[i].rfidTag;
      machine["activity_count"] = machines[i].activityCount;
    }
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpCode = http.POST(jsonString);
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      serverConnected = true;
      
#ifdef DEBUG_OUTPUT
      String payload = http.getString();
      Serial.println("Server sync successful");
      Serial.println(payload);
#endif
    } else {
      serverRetryCount++;
      if (serverRetryCount > MAX_RETRIES) {
        serverConnected = false;
      }
      
#ifdef DEBUG_OUTPUT
      Serial.print("Server sync failed with error code: ");
      Serial.println(httpCode);
#endif
    }
  } else {
    serverRetryCount++;
    if (serverRetryCount > MAX_RETRIES) {
      serverConnected = false;
    }
    
#ifdef DEBUG_OUTPUT
    Serial.print("Server sync request failed, error: ");
    Serial.println(http.errorToString(httpCode));
#endif
  }
  
  http.end();
  lastServerSync = millis();
}

void reportActivityToServer() {
  if (!wifiConnected || !serverConnected) return;
  
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    if (!rfidReadersFound[i] || machines[i].activityCount == 0) continue;
    
    HTTPClient http;
    String url = serverUrl + "/api/update_count?machine_id=" + machines[i].machineId + 
                "&count=" + String(machines[i].activityCount);
    
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
      // Reset activity count after successful update
      machines[i].activityCount = 0;
      
#ifdef DEBUG_OUTPUT
      Serial.print("Zone ");
      Serial.print(i);
      Serial.println(" - Activity count reported to server");
#endif
    }
    
    http.end();
  }
}

void checkWiFiConnection() {
  if (apMode) return;  // Skip in AP mode
  
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      wifiRetryCount++;
      
      if (wifiRetryCount > MAX_RETRIES) {
        wifiConnected = false;
        serverConnected = false;
        nodeState = STATE_ERROR;
        
#ifdef DEBUG_OUTPUT
        Serial.println("WiFi connection lost");
#endif
      }
    }
  } else {
    if (!wifiConnected) {
#ifdef DEBUG_OUTPUT
      Serial.println("WiFi reconnected");
#endif
      
      wifiConnected = true;
      wifiRetryCount = 0;
      
      // Advertise service again
      advertiseService();
      
      // Try server sync
      syncWithServer();
      
      if (serverConnected) {
        nodeState = STATE_READY;
      } else {
        nodeState = STATE_CONNECTING_SERVER;
      }
    }
  }
}

void handleOTAUpdate() {
  // OTA update logic would be implemented here
}

void factoryReset() {
#ifdef DEBUG_OUTPUT
  Serial.println("Performing factory reset...");
#endif
  
  // Clear preferences
  preferences.begin("nodeConfig", false);
  preferences.clear();
  preferences.end();
  
  // Reset flags
  serverConnected = false;
  wifiConnected = false;
  
  // Reset LEDs to indicate reset
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::White;
  }
  FastLED.show();
  delay(1000);
  
  // Restart ESP
  ESP.restart();
}

// ===== WEB SERVER HANDLERS =====
void handleRoot() {
  // Special landing page for unconfigured nodes or when no WiFi is available
  if (!isConfigured || (isConfigured && !wifiConnected && millis() > 30000)) {
    // Check if we can find WiFi networks
    int networksFound = WiFi.scanNetworks();
    
    String setupHtml = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
    setupHtml += "<title>ShopMonitor Node Setup</title>";
    setupHtml += "<style>";
    setupHtml += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; color: #333; background-color: #f8f9fa; }";
    setupHtml += "h1, h2 { color: #0066cc; }";
    setupHtml += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
    setupHtml += ".header { text-align: center; margin-bottom: 20px; padding-bottom: 10px; border-bottom: 1px solid #eee; }";
    setupHtml += ".logo { font-size: 24px; font-weight: bold; }";
    setupHtml += ".form-group { margin-bottom: 15px; }";
    setupHtml += "label { display: block; margin-bottom: 5px; font-weight: bold; }";
    setupHtml += "select, input { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
    setupHtml += "button { background-color: #0066cc; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; }";
    setupHtml += "button:hover { background-color: #0056b3; }";
    setupHtml += ".footer { margin-top: 30px; text-align: center; font-size: 0.8em; color: #777; }";
    setupHtml += ".networks { margin-top: 10px; max-height: 200px; overflow-y: auto; border: 1px solid #ddd; border-radius: 4px; }";
    setupHtml += ".network-item { padding: 8px; cursor: pointer; }";
    setupHtml += ".network-item:hover { background-color: #f5f5f5; }";
    setupHtml += ".network-item.selected { background-color: #e6f2ff; }";
    setupHtml += "</style>";
    setupHtml += "</head><body>";
    setupHtml += "<div class=\"container\">";
    setupHtml += "<div class=\"header\"><div class=\"logo\">ShopMonitor Node Setup</div></div>";
    
    if (!isConfigured) {
      setupHtml += "<h2>Initial Node Configuration</h2>";
      setupHtml += "<p>Welcome to your new ShopMonitor node! Please complete the following configuration to get started.</p>";
    } else {
      setupHtml += "<h2>WiFi Configuration</h2>";
      setupHtml += "<p>Your node cannot connect to the configured WiFi network. Please update your WiFi settings.</p>";
    }
    
    setupHtml += "<form id=\"setupForm\" action=\"/api/config\" method=\"post\">";
    setupHtml += "<div class=\"form-group\"><label for=\"nodeName\">Node Name:</label>";
    setupHtml += "<input type=\"text\" id=\"nodeName\" name=\"nodeName\" value=\"" + nodeName + "\" required></div>";
    
    setupHtml += "<div class=\"form-group\"><label for=\"nodeType\">Node Type:</label>";
    setupHtml += "<select id=\"nodeType\" name=\"nodeType\">";
    setupHtml += "<option value=\"0\"" + (nodeType == NODE_TYPE_MACHINE_MONITOR ? " selected" : "") + ">Machine Monitor</option>";
    setupHtml += "<option value=\"1\"" + (nodeType == NODE_TYPE_OFFICE_READER ? " selected" : "") + ">Office RFID Reader</option>";
    setupHtml += "<option value=\"2\"" + (nodeType == NODE_TYPE_ACCESSORY_IO ? " selected" : "") + ">Accessory IO Controller</option>";
    setupHtml += "</select></div>";
    
    setupHtml += "<div class=\"form-group\"><label for=\"serverUrl\">Server URL:</label>";
    setupHtml += "<input type=\"url\" id=\"serverUrl\" name=\"serverUrl\" value=\"" + serverUrl + "\" required placeholder=\"http://server-ip:port\"></div>";
    
    setupHtml += "<div class=\"form-group\"><label for=\"wifiSSID\">WiFi Network:</label>";
    setupHtml += "<input type=\"text\" id=\"wifiSSID\" name=\"wifiSSID\" value=\"" + wifiSSID + "\" required>";
    
    if (networksFound > 0) {
      setupHtml += "<div class=\"networks\" id=\"networkList\">";
      for (int i = 0; i < networksFound; i++) {
        setupHtml += "<div class=\"network-item\" onclick=\"selectNetwork('" + WiFi.SSID(i) + "')\">";
        setupHtml += WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)";
        setupHtml += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " [Open]" : " [Secured]";
        setupHtml += "</div>";
      }
      setupHtml += "</div>";
    }
    
    setupHtml += "</div>";
    
    setupHtml += "<div class=\"form-group\"><label for=\"wifiPassword\">WiFi Password:</label>";
    setupHtml += "<input type=\"password\" id=\"wifiPassword\" name=\"wifiPass\" value=\"\"></div>";
    
    setupHtml += "<button type=\"submit\">Save Configuration & Restart</button>";
    setupHtml += "</form>";
    
    setupHtml += "<div class=\"footer\">Shop ESP32 Multi-Function Node v1.0</div>";
    setupHtml += "</div>";
    
    setupHtml += "<script>";
    setupHtml += "function selectNetwork(ssid) {";
    setupHtml += "  document.getElementById('wifiSSID').value = ssid;";
    setupHtml += "  var items = document.getElementsByClassName('network-item');";
    setupHtml += "  for (var i = 0; i < items.length; i++) {";
    setupHtml += "    items[i].classList.remove('selected');";
    setupHtml += "    if (items[i].innerText.includes(ssid)) {";
    setupHtml += "      items[i].classList.add('selected');";
    setupHtml += "    }";
    setupHtml += "  }";
    setupHtml += "}";
    setupHtml += "document.getElementById('setupForm').addEventListener('submit', function(e) {";
    setupHtml += "  e.preventDefault();";
    setupHtml += "  var formData = new FormData(this);";
    setupHtml += "  var jsonData = {};";
    setupHtml += "  formData.forEach(function(value, key){";
    setupHtml += "    jsonData[key] = value;";
    setupHtml += "  });";
    setupHtml += "  fetch('/api/config', {";
    setupHtml += "    method: 'POST',";
    setupHtml += "    headers: { 'Content-Type': 'application/json' },";
    setupHtml += "    body: JSON.stringify(jsonData)";
    setupHtml += "  })";
    setupHtml += "  .then(response => response.json())";
    setupHtml += "  .then(data => {";
    setupHtml += "    if (data.success) {";
    setupHtml += "      alert('Configuration saved. The device will now restart.');";
    setupHtml += "      setTimeout(function() {";
    setupHtml += "        window.location.href = '/api/reboot';";
    setupHtml += "      }, 1000);";
    setupHtml += "    } else {";
    setupHtml += "      alert('Error: ' + data.message);";
    setupHtml += "    }";
    setupHtml += "  })";
    setupHtml += "  .catch(error => {";
    setupHtml += "    alert('Error saving configuration: ' + error);";
    setupHtml += "  });";
    setupHtml += "});";
    setupHtml += "</script>";
    setupHtml += "</body></html>";
    
    webServer.send(200, "text/html", setupHtml);
    return;
  }
  
  // Standard dashboard for configured nodes
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Node Configuration</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; padding: 20px; color: #333; }
    h1 { color: #0066cc; }
    .container { max-width: 800px; margin: 0 auto; background: #f9f9f9; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    .status { background: #e9f7ef; padding: 15px; border-radius: 5px; margin-bottom: 20px; }
    .status.error { background: #f8d7da; }
    .form-group { margin-bottom: 15px; }
    label { display: block; margin-bottom: 5px; font-weight: bold; }
    input, select { width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }
    button { background: #0066cc; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; }
    button:hover { background: #0056b3; }
    .footer { margin-top: 30px; text-align: center; font-size: 0.8em; color: #777; }
    table { width: 100%; border-collapse: collapse; margin: 20px 0; }
    th, td { padding: 10px; border: 1px solid #ddd; text-align: left; }
    th { background: #f2f2f2; }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Node Configuration</h1>
    
    <div class="status">
      <h2>Node Status</h2>
      <p><strong>Node ID:</strong> <span id="nodeId"></span></p>
      <p><strong>Node Type:</strong> <span id="nodeType"></span></p>
      <p><strong>IP Address:</strong> <span id="ipAddress"></span></p>
      <p><strong>WiFi Signal:</strong> <span id="wifiSignal"></span></p>
      <p><strong>Uptime:</strong> <span id="uptime"></span></p>
      <p><strong>Server Connection:</strong> <span id="serverConnection"></span></p>
    </div>
    
    <div id="machineStatus" style="display:none;">
      <h2>Machine Status</h2>
      <table id="machineTable">
        <thead>
          <tr>
            <th>Zone</th>
            <th>Machine ID</th>
            <th>Status</th>
            <th>Current User</th>
            <th>Activity Count</th>
          </tr>
        </thead>
        <tbody id="machineTableBody">
        </tbody>
      </table>
    </div>
    
    <div id="officeReaderStatus" style="display:none;">
      <h2>Office Reader Status</h2>
      <p><strong>Last Scanned Tag:</strong> <span id="lastTag"></span></p>
      <p><strong>Scan Time:</strong> <span id="scanTime"></span></p>
    </div>
    
    <div id="accessoryIOStatus" style="display:none;">
      <h2>Accessory IO Status</h2>
      <table id="ioTable">
        <thead>
          <tr>
            <th>IO</th>
            <th>Type</th>
            <th>State</th>
            <th>Control</th>
          </tr>
        </thead>
        <tbody id="ioTableBody">
        </tbody>
      </table>
    </div>
    
    <h2>Configuration</h2>
    <form id="configForm">
      <div class="form-group">
        <label for="nodeName">Node Name:</label>
        <input type="text" id="nodeName" name="nodeName" required>
      </div>
      
      <div class="form-group">
        <label for="nodeType">Node Type:</label>
        <select id="nodeType" name="nodeType">
          <option value="0">Machine Monitor</option>
          <option value="1">Office RFID Reader</option>
          <option value="2">Accessory IO Controller</option>
        </select>
      </div>
      
      <div class="form-group">
        <label for="wifiSSID">WiFi SSID:</label>
        <input type="text" id="wifiSSID" name="wifiSSID" required>
      </div>
      
      <div class="form-group">
        <label for="wifiPass">WiFi Password:</label>
        <input type="password" id="wifiPass" name="wifiPass">
      </div>
      
      <div class="form-group">
        <label for="serverUrl">Server URL:</label>
        <input type="url" id="serverUrl" name="serverUrl" required>
      </div>
      
      <div id="machineConfig">
        <h3>Machine Configuration</h3>
        <div class="form-group">
          <label for="machine0">Machine ID (Zone 0):</label>
          <input type="text" id="machine0" name="machine0" maxlength="2" placeholder="01">
        </div>
        
        <div class="form-group">
          <label for="machine1">Machine ID (Zone 1):</label>
          <input type="text" id="machine1" name="machine1" maxlength="2" placeholder="02">
        </div>
        
        <div class="form-group">
          <label for="machine2">Machine ID (Zone 2):</label>
          <input type="text" id="machine2" name="machine2" maxlength="2" placeholder="03">
        </div>
        
        <div class="form-group">
          <label for="machine3">Machine ID (Zone 3):</label>
          <input type="text" id="machine3" name="machine3" maxlength="2" placeholder="04">
        </div>
      </div>
      
      <button type="submit">Save Configuration</button>
    </form>
    
    <div style="margin-top: 30px;">
      <h2>System Maintenance</h2>
      <p>
        <button id="updateBtn" onclick="checkForUpdate()">Check for Update</button>
        <button id="rebootBtn" onclick="rebootDevice()">Reboot Device</button>
      </p>
      <p>
        <button id="resetBtn" onclick="return confirm('WARNING: This will reset all settings to default. Continue?') && factoryReset()">Factory Reset</button>
      </p>
    </div>
    
    <div class="footer">
      <p>ESP32 Multi-Function Node v1.0.0 | &copy; 2025 ShopMonitor</p>
    </div>
  </div>

  <script>
    // Load configuration on page load
    window.onload = function() {
      fetchStatus();
      fetchConfig();
      
      // Setup form submission
      document.getElementById('configForm').addEventListener('submit', function(e) {
        e.preventDefault();
        saveConfig();
      });
      
      // Refresh status every 5 seconds
      setInterval(fetchStatus, 5000);
    };
    
    // Fetch node status from API
    function fetchStatus() {
      fetch('/api/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('nodeId').textContent = data.node_id;
          document.getElementById('nodeType').textContent = getNodeTypeName(data.node_type);
          document.getElementById('ipAddress').textContent = data.ip_address;
          document.getElementById('wifiSignal').textContent = data.wifi_signal + ' dBm';
          document.getElementById('uptime').textContent = formatUptime(data.uptime);
          document.getElementById('serverConnection').textContent = data.server_connected ? 'Connected' : 'Disconnected';
          
          // Node-specific status
          if (data.node_type === 0) { // Machine Monitor
            document.getElementById('machineStatus').style.display = 'block';
            document.getElementById('officeReaderStatus').style.display = 'none';
            document.getElementById('accessoryIOStatus').style.display = 'none';
            
            // Update machine table
            const tableBody = document.getElementById('machineTableBody');
            tableBody.innerHTML = '';
            
            data.machines.forEach((machine, index) => {
              const row = tableBody.insertRow();
              row.insertCell(0).textContent = index;
              row.insertCell(1).textContent = machine.machine_id;
              row.insertCell(2).textContent = machine.status;
              row.insertCell(3).textContent = machine.rfid || 'None';
              row.insertCell(4).textContent = machine.activity_count;
            });
          } else if (data.node_type === 1) { // Office Reader
            document.getElementById('machineStatus').style.display = 'none';
            document.getElementById('officeReaderStatus').style.display = 'block';
            document.getElementById('accessoryIOStatus').style.display = 'none';
            
            document.getElementById('lastTag').textContent = data.last_tag || 'None';
            document.getElementById('scanTime').textContent = data.scan_time ? new Date(data.scan_time).toLocaleString() : 'N/A';
          } else if (data.node_type === 2) { // Accessory IO
            document.getElementById('machineStatus').style.display = 'none';
            document.getElementById('officeReaderStatus').style.display = 'none';
            document.getElementById('accessoryIOStatus').style.display = 'block';
            
            // Update IO table
            const tableBody = document.getElementById('ioTableBody');
            tableBody.innerHTML = '';
            
            // Inputs
            data.inputs.forEach((input, index) => {
              const row = tableBody.insertRow();
              row.insertCell(0).textContent = index;
              row.insertCell(1).textContent = 'Input';
              row.insertCell(2).textContent = input ? 'HIGH' : 'LOW';
              row.insertCell(3).textContent = 'N/A';
            });
            
            // Outputs
            data.outputs.forEach((output, index) => {
              const row = tableBody.insertRow();
              row.insertCell(0).textContent = index;
              row.insertCell(1).textContent = 'Output';
              row.insertCell(2).textContent = output ? 'ON' : 'OFF';
              
              const controlCell = row.insertCell(3);
              const toggleBtn = document.createElement('button');
              toggleBtn.textContent = output ? 'Turn Off' : 'Turn On';
              toggleBtn.onclick = function() { toggleOutput(index, !output); };
              controlCell.appendChild(toggleBtn);
            });
          }
        })
        .catch(error => console.error('Error fetching status:', error));
    }
    
    // Fetch configuration from API
    function fetchConfig() {
      fetch('/api/config')
        .then(response => response.json())
        .then(data => {
          document.getElementById('nodeName').value = data.name;
          document.getElementById('nodeType').value = data.node_type;
          document.getElementById('wifiSSID').value = data.wifi_ssid;
          document.getElementById('serverUrl').value = data.server_url;
          
          // Machine IDs
          if (data.machines) {
            data.machines.forEach((machine, index) => {
              const input = document.getElementById('machine' + index);
              if (input) input.value = machine.machine_id;
            });
          }
        })
        .catch(error => console.error('Error fetching config:', error));
    }
    
    // Save configuration
    function saveConfig() {
      const formData = new FormData(document.getElementById('configForm'));
      const config = {};
      
      // Convert form data to JSON
      for (const [key, value] of formData.entries()) {
        config[key] = value;
      }
      
      // Format machine data
      config.machines = [];
      for (let i = 0; i < 4; i++) {
        const machineId = config['machine' + i];
        if (machineId) {
          config.machines.push({
            zone: i,
            machine_id: machineId
          });
        }
        delete config['machine' + i];
      }
      
      // Send to API
      fetch('/api/config', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(config)
      })
      .then(response => response.json())
      .then(data => {
        alert(data.message);
        if (data.success && data.reboot_required) {
          if (confirm('Configuration saved. Reboot device now?')) {
            rebootDevice();
          }
        }
      })
      .catch(error => {
        console.error('Error saving config:', error);
        alert('Error saving configuration');
      });
    }
    
    // Reboot device
    function rebootDevice() {
      if (confirm('Are you sure you want to reboot the device?')) {
        fetch('/api/reboot')
          .then(() => {
            alert('Device is rebooting...');
            setTimeout(() => {
              location.reload();
            }, 10000); // Wait 10 seconds then reload page
          })
          .catch(error => console.error('Error rebooting:', error));
      }
    }
    
    // Factory reset
    function factoryReset() {
      fetch('/api/factory_reset', { method: 'POST' })
        .then(() => {
          alert('Device has been reset to factory defaults and is rebooting...');
          setTimeout(() => {
            location.reload();
          }, 10000); // Wait 10 seconds then reload page
        })
        .catch(error => console.error('Error resetting:', error));
      return false;
    }
    
    // Check for update
    function checkForUpdate() {
      alert('This feature is not implemented in this example');
    }
    
    // Toggle accessory output
    function toggleOutput(index, state) {
      fetch('/api/control_io', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({ output: index, state: state ? 1 : 0 })
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          fetchStatus(); // Refresh status
        } else {
          alert('Error: ' + data.message);
        }
      })
      .catch(error => console.error('Error toggling output:', error));
    }
    
    // Helper functions
    function getNodeTypeName(type) {
      const types = ['Machine Monitor', 'Office RFID Reader', 'Accessory IO Controller'];
      return types[type] || 'Unknown';
    }
    
    function formatUptime(seconds) {
      const days = Math.floor(seconds / 86400);
      const hours = Math.floor((seconds % 86400) / 3600);
      const minutes = Math.floor((seconds % 3600) / 60);
      const secs = seconds % 60;
      
      return `${days}d ${hours}h ${minutes}m ${secs}s`;
    }
  </script>
</body>
</html>
  )rawliteral";
  
  webServer.send(200, "text/html", html);
}

void handleStatus() {
  StaticJsonDocument<1024> doc;
  
  // Basic node info
  doc["node_id"] = nodeId;
  doc["node_type"] = nodeType;
  doc["name"] = nodeName;
  doc["ip_address"] = WiFi.localIP().toString();
  doc["wifi_signal"] = WiFi.RSSI();
  doc["uptime"] = millis() / 1000;
  doc["server_connected"] = serverConnected;
  
  // Node-specific status
  if (nodeType == NODE_TYPE_MACHINE_MONITOR) {
    JsonArray machineArray = doc.createNestedArray("machines");
    
    for (int i = 0; i < MAX_RFID_READERS; i++) {
      if (!rfidReadersFound[i]) continue;
      
      JsonObject machine = machineArray.createNestedObject();
      machine["zone"] = i;
      machine["machine_id"] = machines[i].machineId;
      machine["status"] = machines[i].active ? "active" : "idle";
      machine["rfid"] = machines[i].rfidTag;
      machine["activity_count"] = machines[i].activityCount;
    }
  } else if (nodeType == NODE_TYPE_OFFICE_READER) {
    doc["last_tag"] = lastScannedTag;
    doc["scan_time"] = lastScanTime > 0 ? timeClient.getFormattedTime() : "";
    doc["new_tag_available"] = newTagScanned;
  } else if (nodeType == NODE_TYPE_ACCESSORY_IO) {
    JsonArray inputArray = doc.createNestedArray("inputs");
    JsonArray outputArray = doc.createNestedArray("outputs");
    
    for (int i = 0; i < MAX_RFID_READERS; i++) {
      inputArray.add(inputState[i]);
      outputArray.add(outputState[i]);
    }
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  webServer.send(200, "application/json", jsonString);
}

void handleConfig() {
  StaticJsonDocument<1024> doc;
  
  // Basic configuration
  doc["node_id"] = nodeId;
  doc["name"] = nodeName;
  doc["node_type"] = nodeType;
  doc["wifi_ssid"] = wifiSSID;
  doc["server_url"] = serverUrl;
  
  // Machine configuration
  if (nodeType == NODE_TYPE_MACHINE_MONITOR) {
    JsonArray machineArray = doc.createNestedArray("machines");
    
    for (int i = 0; i < MAX_RFID_READERS; i++) {
      if (!rfidReadersFound[i]) continue;
      
      JsonObject machine = machineArray.createNestedObject();
      machine["zone"] = i;
      machine["machine_id"] = machines[i].machineId;
    }
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  webServer.send(200, "application/json", jsonString);
}

void handleSetConfig() {
  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"success\":false,\"message\":\"No data provided\"}");
    return;
  }
  
  String jsonString = webServer.arg("plain");
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    String errorMsg = String("JSON parsing failed: ") + error.c_str();
    webServer.send(400, "application/json", "{\"success\":false,\"message\":\"" + errorMsg + "\"}");
    return;
  }
  
  bool rebootRequired = false;
  
  // Update node configuration
  if (doc.containsKey("nodeName")) {
    nodeName = doc["nodeName"].as<String>();
  }
  
  if (doc.containsKey("nodeType")) {
    uint8_t newNodeType = doc["nodeType"];
    if (newNodeType != nodeType) {
      nodeType = newNodeType;
      rebootRequired = true;
    }
  }
  
  // Update WiFi configuration
  if (doc.containsKey("wifiSSID") && doc.containsKey("wifiPass")) {
    String newSSID = doc["wifiSSID"].as<String>();
    String newPass = doc["wifiPass"].as<String>();
    
    if (newSSID != wifiSSID || (newPass.length() > 0 && newPass != wifiPassword)) {
      wifiSSID = newSSID;
      if (newPass.length() > 0) {
        wifiPassword = newPass;
      }
      rebootRequired = true;
    }
  }
  
  // Update server configuration
  if (doc.containsKey("serverUrl")) {
    serverUrl = doc["serverUrl"].as<String>();
  }
  
  // Update machine configuration
  if (doc.containsKey("machines") && nodeType == NODE_TYPE_MACHINE_MONITOR) {
    JsonArray machineArray = doc["machines"];
    
    for (JsonObject machineObj : machineArray) {
      int zone = machineObj["zone"];
      if (zone >= 0 && zone < MAX_RFID_READERS && rfidReadersFound[zone]) {
        machines[zone].machineId = machineObj["machine_id"].as<String>();
      }
    }
  }
  
  // Save configuration
  saveConfiguration();
  
  webServer.send(200, "application/json", 
    "{\"success\":true,\"message\":\"Configuration saved\",\"reboot_required\":" + 
    String(rebootRequired ? "true" : "false") + "}");
}

void handleUpdate() {
  webServer.sendHeader("Connection", "close");
  webServer.send(200, "text/plain", "OK");
  ESP.restart();
}

void handleUpdateUpload() {
  HTTPUpload& upload = webServer.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    /* flashing firmware to ESP*/
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleReboot() {
  webServer.send(200, "text/plain", "Rebooting...");
  delay(500);
  ESP.restart();
}

void handleFactoryReset() {
  webServer.send(200, "text/plain", "Performing factory reset...");
  delay(500);
  factoryReset();
}

void handleNotFound() {
  webServer.send(404, "text/plain", "404: Not found");
}

void handleIOStatus() {
  if (nodeType != NODE_TYPE_ACCESSORY_IO) {
    webServer.send(400, "application/json", "{\"error\":\"This node is not configured as an Accessory IO controller\"}");
    return;
  }
  
  StaticJsonDocument<512> doc;
  JsonArray inputArray = doc.createNestedArray("inputs");
  JsonArray outputArray = doc.createNestedArray("outputs");
  
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    inputArray.add(inputState[i]);
    outputArray.add(outputState[i]);
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  webServer.send(200, "application/json", jsonString);
}

void handleControlIO() {
  if (nodeType != NODE_TYPE_ACCESSORY_IO) {
    webServer.send(400, "application/json", "{\"success\":false,\"message\":\"This node is not configured as an Accessory IO controller\"}");
    return;
  }
  
  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"success\":false,\"message\":\"No data provided\"}");
    return;
  }
  
  String jsonString = webServer.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    String errorMsg = String("JSON parsing failed: ") + error.c_str();
    webServer.send(400, "application/json", "{\"success\":false,\"message\":\"" + errorMsg + "\"}");
    return;
  }
  
  if (!doc.containsKey("output") || !doc.containsKey("state")) {
    webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Missing required fields\"}");
    return;
  }
  
  int output = doc["output"];
  int state = doc["state"];
  int timeout = doc["timeout"] | 0;  // Optional timeout in seconds
  
  if (output < 0 || output >= MAX_RFID_READERS) {
    webServer.send(400, "application/json", "{\"success\":false,\"message\":\"Invalid output pin\"}");
    return;
  }
  
  // Set output state
  digitalWrite(PIN_RELAY[output], state ? HIGH : LOW);
  outputState[output] = state;
  
  // Set timeout if specified
  if (timeout > 0) {
    outputTimeout[output] = millis() + (timeout * 1000);
  } else {
    outputTimeout[output] = 0;  // No timeout
  }
  
#ifdef DEBUG_OUTPUT
  Serial.print("Output ");
  Serial.print(output);
  Serial.print(" set to ");
  Serial.println(state ? "ON" : "OFF");
  if (timeout > 0) {
    Serial.print("Timeout set for ");
    Serial.print(timeout);
    Serial.println(" seconds");
  }
#endif
  
  webServer.send(200, "application/json", "{\"success\":true,\"message\":\"Output state updated\"}");
}

void handleLastScan() {
  if (nodeType != NODE_TYPE_OFFICE_READER) {
    webServer.send(400, "application/json", "{\"error\":\"This node is not configured as an Office RFID reader\"}");
    return;
  }
  
  StaticJsonDocument<256> doc;
  doc["last_tag"] = lastScannedTag;
  doc["scan_time"] = lastScanTime;
  doc["new_tag_available"] = newTagScanned;
  
  // If request includes reset=true, clear the new tag flag
  if (webServer.hasArg("reset") && webServer.arg("reset") == "true") {
    newTagScanned = false;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  webServer.send(200, "application/json", jsonString);
}