#include <Arduino.h>
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "RFIDHandler.h"
#include "WebUI.h"
#include "constants.h"

// Define global objects
ConfigManager configManager;
NetworkManager networkManager;
RFIDHandler rfidHandler;
WebUI webUI(configManager, networkManager);

// State variables
String currentTags[MAX_RFID_READERS] = {"", "", "", ""};
unsigned long lastActivityTime[MAX_RFID_READERS] = {0, 0, 0, 0};
int activityCounts[MAX_RFID_READERS] = {0, 0, 0, 0};
unsigned long lastReportTime = 0;
bool relayStates[MAX_RFID_READERS] = {false, false, false, false};
bool inputStates[MAX_RFID_READERS] = {false, false, false, false};
bool estopActive = false;

// Forward declarations
void checkActivity();
void reportActivity();
void toggleRelay(int relayNum, bool state);
void factoryReset();
void blinkLED(int times, int delayMS);
void checkEstop();
void playTone(int duration, int frequency);

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nStarting ESP32 Standard Node " VERSION " ...");
  
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize relay pins
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  digitalWrite(RELAY1_PIN, LOW);
  digitalWrite(RELAY2_PIN, LOW);
  digitalWrite(RELAY3_PIN, LOW);
  digitalWrite(RELAY4_PIN, LOW);
  
  // Initialize activity sensor pins with pull-ups
  pinMode(ACTIVITY1_PIN, INPUT_PULLUP);
  pinMode(ACTIVITY2_PIN, INPUT_PULLUP);
  pinMode(ACTIVITY3_PIN, INPUT_PULLUP);
  pinMode(ACTIVITY4_PIN, INPUT_PULLUP);
  
  // Initialize buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize E-STOP pins
  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(ESTOP_RELAY_PIN, OUTPUT);
  digitalWrite(ESTOP_RELAY_PIN, HIGH); // Default to safety position (NC)
  
  // Initialize buttons
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  
  // Signal startup
  blinkLED(3, 200);
  
  // Load configuration
  bool configLoaded = configManager.load();
  if (!configLoaded) {
    Serial.println("Configuration not found or invalid - using defaults");
  }
  
  // Initialize RFID handler
  rfidHandler.begin(configManager.getNodeType());
  
  // Setup networking
  bool wifiConnected = networkManager.begin(
    configManager.getWifiSSID(),
    configManager.getWifiPassword(),
    configManager.getNodeName()
  );
  
  if (!wifiConnected) {
    Serial.println("WiFi connection failed - running in offline mode");
    blinkLED(5, 100);
  }
  
  // Setup web server
  webUI.begin();
  
  // Update WebUI with initial states
  webUI.setCurrentTags(currentTags);
  webUI.setActivityCounts(activityCounts);
  webUI.setRelayStates(relayStates);
  webUI.setInputStates(inputStates);
  
  Serial.println("Setup complete!");
  Serial.print("Node Type: ");
  Serial.println(configManager.getNodeType() == NODE_TYPE_MACHINE_MONITOR ? "Machine Monitor" : 
                configManager.getNodeType() == NODE_TYPE_OFFICE_READER ? "Office RFID Reader" : "Accessory IO Controller");
  Serial.print("Node Name: ");
  Serial.println(configManager.getNodeName());
  
  // Signal ready
  blinkLED(5, 100);
  playTone(500, 2000);
}

void loop() {
  // Check E-STOP status
  checkEstop();
  
  // Handle RFID readers
  if (configManager.getNodeType() == NODE_TYPE_MACHINE_MONITOR || 
      configManager.getNodeType() == NODE_TYPE_OFFICE_READER) {
    
    bool tagDetected = rfidHandler.checkReaders(currentTags, configManager.getNodeType());
    
    // If tag detected, update WebUI
    if (tagDetected) {
      webUI.setCurrentTags(currentTags);
      
      // If machine monitor, toggle relays based on current tags
      if (configManager.getNodeType() == NODE_TYPE_MACHINE_MONITOR) {
        for (int i = 0; i < MAX_RFID_READERS; i++) {
          toggleRelay(i + 1, currentTags[i] != "");
        }
      }
    }
  }
  
  // Check activity sensors and update relay states
  if (!estopActive) {
    checkActivity();
  }
  
  // Sync relay states with hardware (in case they were changed via Web UI)
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    int relayPin = 0;
    switch (i) {
      case 0: relayPin = RELAY1_PIN; break;
      case 1: relayPin = RELAY2_PIN; break;
      case 2: relayPin = RELAY3_PIN; break;
      case 3: relayPin = RELAY4_PIN; break;
    }
    
    // Only update if state has changed and not in E-STOP
    if (!estopActive && digitalRead(relayPin) == HIGH != relayStates[i]) {
      digitalWrite(relayPin, relayStates[i] ? HIGH : LOW);
    }
  }
  
  // Report activity periodically
  if (millis() - lastReportTime > REPORT_INTERVAL) {
    if (networkManager.isConnected()) {
      reportActivity();
    }
    lastReportTime = millis();
  }
  
  // Check for factory reset (both buttons pressed for 5 seconds)
  if (digitalRead(BUTTON1_PIN) == LOW && digitalRead(BUTTON2_PIN) == LOW) {
    static unsigned long buttonTime = 0;
    if (buttonTime == 0) {
      buttonTime = millis();
    } else if (millis() - buttonTime > 5000) {
      Serial.println("Factory reset triggered by buttons...");
      factoryReset();
      buttonTime = 0;
    }
  } else {
    static unsigned long buttonTime = 0;
  }
  
  // Process web server requests
  webUI.update();
  
  delay(50); // Small delay to prevent CPU overload
}

void checkEstop() {
  bool estopState = digitalRead(ESTOP_PIN) == LOW; // E-STOP is active when pin is LOW
  
  if (estopState != estopActive) {
    estopActive = estopState;
    
    if (estopActive) {
      Serial.println("E-STOP ACTIVATED!");
      
      // Turn off all relays immediately
      digitalWrite(RELAY1_PIN, LOW);
      digitalWrite(RELAY2_PIN, LOW);
      digitalWrite(RELAY3_PIN, LOW);
      digitalWrite(RELAY4_PIN, LOW);
      
      // Activate E-STOP relay
      digitalWrite(ESTOP_RELAY_PIN, LOW);
      
      // Alert with fast blinks
      for (int i = 0; i < 5; i++) {
        digitalWrite(LED_PIN, HIGH);
        playTone(100, 2000);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
      }
    } else {
      Serial.println("E-STOP cleared");
      
      // De-activate E-STOP relay
      digitalWrite(ESTOP_RELAY_PIN, HIGH);
      
      // Alert with slow blinks
      for (int i = 0; i < 2; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(500);
      }
    }
    
    // Update WebUI with E-STOP status
    webUI.setEstopStatus(estopActive);
  }
}

void checkActivity() {
  // Check activity pins
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    // Skip check if no user is active for machine monitor mode
    if (currentTags[i] == "" && configManager.getNodeType() == NODE_TYPE_MACHINE_MONITOR) {
      continue;
    }
    
    // Read activity pin
    int activityPin;
    switch (i) {
      case 0: activityPin = ACTIVITY1_PIN; break;
      case 1: activityPin = ACTIVITY2_PIN; break;
      case 2: activityPin = ACTIVITY3_PIN; break;
      case 3: activityPin = ACTIVITY4_PIN; break;
    }
    
    // Update state (active when pin is LOW due to pull-up)
    bool currentState = (digitalRead(activityPin) == LOW);
    inputStates[i] = currentState;
    
    // Update activity count if pin is active and debounce
    if (currentState) {
      if (millis() - lastActivityTime[i] > 1000) { // Debounce 1 second
        activityCounts[i]++;
        lastActivityTime[i] = millis();
        
        // Log activity
        Serial.print("Activity detected on pin ");
        Serial.print(activityPin);
        Serial.print(" (zone ");
        Serial.print(i);
        Serial.println(")");
        
        // Update WebUI
        webUI.setActivityCounts(activityCounts);
        webUI.setInputStates(inputStates);
      }
    }
  }
}

void reportActivity() {
  if (!networkManager.isConnected()) return;
  
  Serial.println("Reporting activity to server...");
  
  // Construct report data
  DynamicJsonDocument doc(1024);
  doc["node_id"] = configManager.getNodeName();
  doc["node_type"] = configManager.getNodeType();
  
  JsonArray machineData = doc.createNestedArray("machines");
  for (int i = 0; i < MAX_RFID_READERS; i++) {
    if (configManager.getMachineID(i).length() > 0) {
      JsonObject machine = machineData.createNestedObject();
      machine["machine_id"] = configManager.getMachineID(i);
      machine["activity_count"] = activityCounts[i];
      machine["status"] = currentTags[i] != "" ? "active" : "idle";
      machine["rfid"] = currentTags[i];
    }
  }
  
  // Send report to server
  String reportUrl = configManager.getServerUrl() + "/api/activity";
  String payload;
  serializeJson(doc, payload);
  
  String response;
  bool success = networkManager.sendPOST(reportUrl, payload, response);
  
  if (success) {
    // Reset activity counts after successful report
    for (int i = 0; i < MAX_RFID_READERS; i++) {
      activityCounts[i] = 0;
    }
    
    // Update WebUI
    webUI.setActivityCounts(activityCounts);
  }
}

void toggleRelay(int relayNum, bool state) {
  // Don't activate relays if in E-STOP mode
  if (estopActive && state) {
    return;
  }
  
  switch (relayNum) {
    case 1:
      digitalWrite(RELAY1_PIN, state ? HIGH : LOW);
      relayStates[0] = state;
      break;
    case 2:
      digitalWrite(RELAY2_PIN, state ? HIGH : LOW);
      relayStates[1] = state;
      break;
    case 3:
      digitalWrite(RELAY3_PIN, state ? HIGH : LOW);
      relayStates[2] = state;
      break;
    case 4:
      digitalWrite(RELAY4_PIN, state ? HIGH : LOW);
      relayStates[3] = state;
      break;
  }
  
  // Update WebUI
  webUI.setRelayStates(relayStates);
  
  // Log relay action
  Serial.print("Relay ");
  Serial.print(relayNum);
  Serial.print(" set to ");
  Serial.println(state ? "ON" : "OFF");
}

void factoryReset() {
  Serial.println("Performing factory reset...");
  
  // Reset configuration
  configManager.reset();
  
  // Reset hardware
  for (int i = 1; i <= MAX_RFID_READERS; i++) {
    toggleRelay(i, false);
  }
  
  // Signal reset complete
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    playTone(100, 2000);
    delay(100);
  }
  digitalWrite(LED_PIN, LOW);
  
  // Restart ESP32
  ESP.restart();
}

void blinkLED(int times, int delayMS) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(delayMS);
    digitalWrite(LED_PIN, LOW);
    delay(delayMS);
  }
}

void playTone(int duration, int frequency) {
  // Use PWM to generate a tone
  ledcSetup(0, frequency, 8);  // Channel 0, frequency, 8-bit resolution
  ledcAttachPin(BUZZER_PIN, 0);
  ledcWrite(0, 128);  // 50% duty cycle
  delay(duration);
  ledcWrite(0, 0);    // Turn off
}