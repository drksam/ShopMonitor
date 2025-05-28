#include <Arduino.h>
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "RFIDHandler.h"
#include "WebUI.h"
#include "constants.h"

#ifdef WITH_AUDIO
#include "AudioManager.h"
#endif

// Include display libraries for ESP32-CYD
#include <TFT_eSPI.h>
#include <SPI.h>

// Include I2C and port expander libraries
#include <Wire.h>
#include <PCF8575.h>

// Define global objects
ConfigManager configManager;
NetworkManager networkManager;
RFIDHandler rfidHandler;
WebUI webUI(configManager, networkManager);

// Create PCF8575 instance
PCF8575 pcf8575(PCF8575_ADDR);

#ifdef WITH_AUDIO
AudioManager audioManager;
#endif

// Display object
TFT_eSPI tft = TFT_eSPI();

// State variables
String currentTag = "";
unsigned long lastActivityTime = 0;
int activityCount = 0;
unsigned long lastReportTime = 0;
bool relayState = false;
bool inputState = false;
bool estopActive = false;
int ldrValue = 0;      // Value from Light Dependent Resistor
unsigned long lastLdrReadTime = 0; // Last time LDR was read

// Touch variables
bool touchEnabled = true;
unsigned long lastTouchTime = 0;

// Forward declarations
void initDisplay();
void initI2CAndPortExpander();
void initRgbLed();
void updateDisplay();
void drawAudioControls();
void drawBluetoothControls();
void checkActivity();
void reportActivity();
void toggleRelay(bool state);
void factoryReset();
void blinkLED(int times, int delayMS);
void setRgbLed(uint8_t r, uint8_t g, uint8_t b);
void checkLdr();
void checkEstop();
void checkTouch();
void drawMainUI();
void drawLdrSection();
void drawVolumeSlider(int x, int y, int width, int height, int volume);
void playTone(int duration, int frequency);
bool handleTouch(int16_t x, int16_t y);
void scanBluetoothDevices();

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nStarting ESP32-CYD Node " VERSION " with Audio...");
  
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize RGB LED
  initRgbLed();
  
  // Set initial RGB LED color (blue during startup)
  setRgbLed(0, 0, 100);
  
  // Initialize TFT display
  initDisplay();
  
  // Initialize I2C and port expander
  initI2CAndPortExpander();
  
  // Display startup screen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("ESP32-CYD Node");
  tft.setCursor(10, 40);
  tft.println(VERSION);
  tft.setCursor(10, 70);
  tft.println("Starting up...");
  
  // Initialize E-STOP pins
  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(ESTOP_RELAY_PIN, OUTPUT);
  digitalWrite(ESTOP_RELAY_PIN, HIGH); // Default to safety position (NC)
  
  // Signal startup
  blinkLED(3, 200);
  
  // Load configuration
  bool configLoaded = configManager.load();
  if (!configLoaded) {
    Serial.println("Configuration not found or invalid - using defaults");
    tft.setCursor(10, 100);
    tft.println("No config - using defaults");
  }
  
  // Initialize audio system
#ifdef WITH_AUDIO
  tft.setCursor(10, 130);
  tft.println("Initializing audio...");
  audioManager.begin();
  audioManager.setEStopStatus(estopActive);
  tft.println("Audio initialized");
#endif
  
  // Initialize RFID handler
  rfidHandler.begin(configManager.getNodeType());
  
  // Setup networking
  tft.setCursor(10, 160);
  tft.println("Connecting to WiFi...");
  
  bool wifiConnected = networkManager.begin(
    configManager.getWifiSSID(),
    configManager.getWifiPassword(),
    configManager.getNodeName()
  );
  
  if (!wifiConnected) {
    Serial.println("WiFi connection failed - running in offline mode");
    tft.setCursor(10, 190);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.println("WiFi connection failed");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    blinkLED(5, 100);
  } else {
    tft.setCursor(10, 190);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("WiFi connected");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  }
  
  // Setup web server
  webUI.begin();
  
  // Update WebUI with initial states
  webUI.setCurrentTags(&currentTag);
  webUI.setActivityCount(activityCount);
  webUI.setRelayState(relayState);
  webUI.setInputState(inputState);
  
  Serial.println("Setup complete!");
  Serial.print("Node Name: ");
  Serial.println(configManager.getNodeName());
  
  // Signal ready
  blinkLED(5, 100);
  
#ifdef WITH_AUDIO
  // Play startup sound
  audioManager.playAlertSound(1);
#endif
  
  // Small delay to show startup screen
  delay(2000);
  
  // Draw the main UI
  drawMainUI();
}

void loop() {
  // Check E-STOP status
  checkEstop();
  
  // Handle RFID reader
  bool tagDetected = rfidHandler.checkReader(&currentTag);
  
  // If tag detected, update WebUI and display
  if (tagDetected) {
    webUI.setCurrentTag(currentTag);
    
    // Update display
    tft.fillRect(100, 50, 220, 30, TFT_BLACK);
    tft.setCursor(100, 50);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.println(currentTag);
    
#ifdef WITH_AUDIO
    // Play tag detected sound
    audioManager.playAlertSound(2);
#endif
  }
  
  // Check activity sensors if not in E-STOP
  if (!estopActive) {
    checkActivity();
  }
  
  // Report activity periodically
  if (millis() - lastReportTime > REPORT_INTERVAL) {
    if (networkManager.isConnected()) {
      reportActivity();
    }
    lastReportTime = millis();
  }
  
  // Check for touch input
  checkTouch();
  
  // Process web server requests
  webUI.update();
  
  // Check if display needs refreshing
  static unsigned long lastDisplayUpdate = 0;
  if (millis() - lastDisplayUpdate > 1000) {  // Update every second
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  
  // Check LDR value
  checkLdr();
  
  delay(20); // Small delay to prevent CPU overload
}

void initDisplay() {
  tft.begin();
  tft.setRotation(0); // Portrait mode
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  
  // Init touch
  uint16_t calData[5] = {275, 3494, 361, 3528, 4}; // Default calibration data
  tft.setTouch(calData);
}

// Initialize I2C bus and PCF8575 port expander
void initI2CAndPortExpander() {
  Serial.println("Initializing I2C and PCF8575 port expander...");
  
  // Initialize I2C bus
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  
  // Initialize PCF8575
  if (pcf8575.begin()) {
    Serial.println("PCF8575 initialized successfully");
  } else {
    Serial.println("PCF8575 initialization failed!");
  }
  
  // Set relay pin as output on PCF8575
  pcf8575.pinMode(PIN_RELAY, OUTPUT);
  
  // Set activity sensor pin as input on PCF8575 with pull-up
  pcf8575.pinMode(PIN_ACTIVITY_SENSOR, INPUT_PULLUP);
  
  // Initialize relay state to OFF
  pcf8575.digitalWrite(PIN_RELAY, LOW);
  
  // Update all pins on the PCF8575
  pcf8575.write();
  
  // Configure LDR pin
  pinMode(LDR_PIN, INPUT);
}

// Initialize RGB LED pins
void initRgbLed() {
  pinMode(LED_PIN_R, OUTPUT);
  pinMode(LED_PIN_G, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  
  // Set all RGB LED pins to LOW (LED off)
  digitalWrite(LED_PIN_R, LOW);
  digitalWrite(LED_PIN_G, LOW);
  digitalWrite(LED_PIN_B, LOW);
  
  Serial.println("RGB LED initialized");
}

// Set the RGB LED color (0-255 for each channel)
void setRgbLed(uint8_t r, uint8_t g, uint8_t b) {
  // ESP32 PWM is inverted (255 is off, 0 is full brightness)
  // Map the 0-255 range to 255-0 for proper LED control
  analogWrite(LED_PIN_R, 255 - r);
  analogWrite(LED_PIN_G, 255 - g);
  analogWrite(LED_PIN_B, 255 - b);
  
  Serial.printf("RGB LED set to R:%d G:%d B:%d\n", r, g, b);
}

// Read the LDR value and update the display
void checkLdr() {
  // Only read LDR every 500ms to avoid excessive updates
  if (millis() - lastLdrReadTime > 500) {
    lastLdrReadTime = millis();
    
    // Read LDR analog value
    int newLdrValue = analogRead(LDR_PIN);
    
    // Only update if value has changed significantly
    if (abs(newLdrValue - ldrValue) > 50) {
      ldrValue = newLdrValue;
      
      // Map LDR value to a percentage (0-100%)
      int lightPercent = map(ldrValue, 0, 4095, 0, 100);
      
      // Update display
      drawLdrSection();
      
      // Update RGB LED color based on light level
      // Dark = blue, medium = green, bright = yellow
      if (lightPercent < 30) {
        setRgbLed(0, 0, 100); // Blue for dark
      } else if (lightPercent < 70) {
        setRgbLed(0, 100, 0); // Green for medium
      } else {
        setRgbLed(100, 100, 0); // Yellow for bright
      }
      
      // Log to serial
      Serial.printf("Light level: %d (%d%%)\n", ldrValue, lightPercent);
    }
  }
}

void checkEstop() {
  bool estopState = digitalRead(ESTOP_PIN) == LOW; // E-STOP is active when pin is LOW
  
  if (estopState != estopActive) {
    estopActive = estopState;
    
    if (estopActive) {
      Serial.println("E-STOP ACTIVATED!");
      
      // Activate E-STOP relay
      digitalWrite(ESTOP_RELAY_PIN, LOW);
      
      // Update display
      tft.fillRect(0, 0, 320, 40, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.setTextSize(2);
      tft.setCursor(80, 10);
      tft.println("E-STOP ACTIVE");
      tft.setTextSize(1);
      
      // Alert with fast blinks
      for (int i = 0; i < 5; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
      }
      
#ifdef WITH_AUDIO
      // Silence audio when E-STOP is active
      audioManager.setEStopStatus(true);
#endif
    } else {
      Serial.println("E-STOP cleared");
      
      // De-activate E-STOP relay
      digitalWrite(ESTOP_RELAY_PIN, HIGH);
      
      // Update display - reset the header
      tft.fillRect(0, 0, 320, 40, TFT_NAVY);
      tft.setTextColor(TFT_WHITE, TFT_NAVY);
      tft.setTextSize(2);
      tft.setCursor(10, 10);
      tft.println(configManager.getNodeName());
      tft.setTextSize(1);
      
      // Alert with slow blinks
      for (int i = 0; i < 2; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
        delay(500);
      }
      
#ifdef WITH_AUDIO
      // Resume audio when E-STOP is cleared
      audioManager.setEStopStatus(false);
      audioManager.playAlertSound(1); // Play alert to indicate E-STOP cleared
#endif
    }
    
    // Update WebUI with E-STOP status
    webUI.setEstopStatus(estopActive);
  }
}

void checkActivity() {
  // We only have one activity sensor on the CYD
  // Read activity sensor from PCF8575 port expander
  pcf8575.read();
  bool currentState = !pcf8575.digitalRead(PIN_ACTIVITY_SENSOR); // Active LOW with pull-up
  inputState = currentState;
  
  // Update activity count if pin is active and debounce
  if (currentState) {
    if (millis() - lastActivityTime > 1000) { // Debounce 1 second
      activityCount++;
      lastActivityTime = millis();
      
      // Log activity
      Serial.print("Activity detected! Count: ");
      Serial.println(activityCount);
      
      // Update WebUI
      webUI.setActivityCount(activityCount);
      webUI.setInputState(inputState);
      
      // Update display with new activity count
      tft.fillRect(160, 120, 60, 20, TFT_BLACK);
      tft.setCursor(160, 120);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.println(activityCount);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      
      // Flash the RGB LED when activity is detected
      setRgbLed(0, 255, 0);  // Green flash
      delay(50);
      
      // Return to the light-based color
      int lightPercent = map(ldrValue, 0, 4095, 0, 100);
      if (lightPercent < 30) {
        setRgbLed(0, 0, 100); // Blue for dark
      } else if (lightPercent < 70) {
        setRgbLed(0, 100, 0); // Green for medium
      } else {
        setRgbLed(100, 100, 0); // Yellow for bright
      }
    }
  }
}

void reportActivity() {
  if (!networkManager.isConnected()) return;
  
  Serial.println("Reporting activity to server...");
  
  // Construct report data
  DynamicJsonDocument doc(512);
  doc["node_id"] = configManager.getNodeName();
  doc["node_type"] = configManager.getNodeType();
  
  JsonArray machineData = doc.createNestedArray("machines");
  JsonObject machine = machineData.createNestedObject();
  machine["machine_id"] = configManager.getMachineID(0);
  machine["activity_count"] = activityCount;
  machine["status"] = currentTag != "" ? "active" : "idle";
  machine["rfid"] = currentTag;
  
  // Send report to server
  String reportUrl = configManager.getServerUrl() + "/api/activity";
  String payload;
  serializeJson(doc, payload);
  
  String response;
  bool success = networkManager.sendPOST(reportUrl, payload, response);
  
  if (success) {
    // Reset activity count after successful report
    activityCount = 0;
    
    // Update WebUI
    webUI.setActivityCount(activityCount);
    
    // Update display
    tft.fillRect(160, 120, 60, 20, TFT_BLACK);
    tft.setCursor(160, 120);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println(activityCount);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
  }
}

void toggleRelay(bool state) {
  // Don't activate relays if in E-STOP mode
  if (estopActive && state) {
    return;
  }
  
  // Control relay through PCF8575 port expander
  pcf8575.digitalWrite(PIN_RELAY, state ? HIGH : LOW);
  pcf8575.write(); // Update PCF8575 outputs
  relayState = state;
  
  // Update WebUI
  webUI.setRelayState(relayState);
  
  // Log relay action
  Serial.print("Relay set to ");
  Serial.println(state ? "ON" : "OFF");
  
  // Update display
  tft.fillRect(160, 90, 60, 20, TFT_BLACK);
  tft.setCursor(160, 90);
  tft.setTextColor(state ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.println(state ? "ON" : "OFF");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void factoryReset() {
  Serial.println("Performing factory reset...");
  
  // Reset configuration
  configManager.reset();
  
  // Reset hardware
  toggleRelay(false);
  
  // Signal reset complete
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(100);
  }
  digitalWrite(LED_PIN, LOW);
  
  // Show reset message on display
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(40, 100);
  tft.println("Factory Reset");
  tft.setCursor(40, 130);
  tft.println("Restarting...");
  
  delay(2000);
  
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

void checkTouch() {
  if (!touchEnabled) return;
  
  uint16_t x = 0, y = 0;
  bool pressed = tft.getTouch(&x, &y);
  
  if (pressed) {
    // Debounce touch
    if (millis() - lastTouchTime < 500) return;
    lastTouchTime = millis();
    
    // Process touch
    if (handleTouch(x, y)) {
      // If touch was handled, update display
      updateDisplay();
    }
  }
}

bool handleTouch(int16_t x, int16_t y) {
  bool handled = false;
  
  // For simplicity, dividing screen into functional regions
  
  // Audio section (bottom half of screen)
  if (y > 160) {
    // Volume controls
    if (x < 80) {  // Left side (decrease volume)
#ifdef WITH_AUDIO
      uint8_t vol = constrain(audioManager.getBuiltinVolume() - 20, 0, 255);
      audioManager.setBuiltinVolume(vol);
      audioManager.playAlertSound(1);  // Play tone to indicate volume change
      handled = true;
#endif
    } else if (x > 240) {  // Right side (increase volume)
#ifdef WITH_AUDIO
      uint8_t vol = constrain(audioManager.getBuiltinVolume() + 20, 0, 255);
      audioManager.setBuiltinVolume(vol);
      audioManager.playAlertSound(1);  // Play tone to indicate volume change
      handled = true;
#endif
    } else if (x > 80 && x < 160) {  // Middle-left (toggle built-in audio)
#ifdef WITH_AUDIO
      bool enabled = !audioManager.isBuiltinAudioEnabled();
      audioManager.setBuiltinAudio(enabled);
      handled = true;
#endif
    } else if (x > 160 && x < 240) {  // Middle-right (toggle Bluetooth audio)
#ifdef WITH_AUDIO
      bool enabled = !audioManager.isBluetoothEnabled();
      audioManager.setBluetooth(enabled);
      handled = true;
#endif
    }
  }
  // Bluetooth device section (upper part of screen)
  else if (y > 120 && y < 160) {
    if (x > 240) {  // Scan button
#ifdef WITH_AUDIO
      scanBluetoothDevices();
      handled = true;
#endif
    }
  }
  
  return handled;
}

#ifdef WITH_AUDIO
void scanBluetoothDevices() {
  // Show scanning message
  tft.fillRect(0, 120, 320, 40, TFT_BLACK);
  tft.setCursor(10, 130);
  tft.println("Scanning for Bluetooth devices...");
  
  // Perform scan
  audioManager.scanBluetoothDevices();
  
  // Show results
  tft.fillRect(0, 120, 320, 40, TFT_BLACK);
  tft.setCursor(10, 130);
  
  int count = audioManager.getBluetoothDeviceCount();
  if (count > 0) {
    tft.println("Found " + String(count) + " devices");
    // Display first device
    if (count > 0) {
      tft.setCursor(10, 145);
      tft.println(audioManager.getBluetoothDeviceName(0));
    }
  } else {
    tft.println("No devices found");
  }
}
#endif

void updateDisplay() {
  // Update WiFi status
  tft.fillRect(240, 10, 70, 20, TFT_NAVY);
  tft.setCursor(240, 10);
  tft.setTextColor(networkManager.isConnected() ? TFT_GREEN : TFT_RED, TFT_NAVY);
  tft.println(networkManager.isConnected() ? "ONLINE" : "OFFLINE");
  
  // Update time (top right corner)
  unsigned long uptime = millis() / 1000;
  int hours = uptime / 3600;
  int minutes = (uptime % 3600) / 60;
  int seconds = uptime % 60;
  
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);
  tft.fillRect(240, 30, 70, 20, TFT_BLACK);
  tft.setCursor(240, 30);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.println(timeStr);
  
#ifdef WITH_AUDIO
  // Update audio status
  static int lastBuiltinVol = -1;
  static int lastBtVol = -1;
  static bool lastBuiltinEnabled = false;
  static bool lastBtEnabled = false;
  
  int builtinVol = audioManager.getBuiltinVolume();
  bool builtinEnabled = audioManager.isBuiltinAudioEnabled();
  int btVol = audioManager.getBluetoothVolume();
  bool btEnabled = audioManager.isBluetoothEnabled();
  
  // Only redraw if values changed
  if (lastBuiltinVol != builtinVol || lastBuiltinEnabled != builtinEnabled ||
      lastBtVol != btVol || lastBtEnabled != btEnabled) {
    
    drawAudioControls();
    drawBluetoothControls();
    
    // Update last values
    lastBuiltinVol = builtinVol;
    lastBuiltinEnabled = builtinEnabled;
    lastBtVol = btVol;
    lastBtEnabled = btEnabled;
  }
#endif
}

void drawMainUI() {
  // Clear screen
  tft.fillScreen(TFT_BLACK);
  
  // Draw header
  tft.fillRect(0, 0, 320, 40, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println(configManager.getNodeName());
  tft.setTextSize(1);
  
  // Draw sections
  tft.drawFastHLine(0, 40, 320, TFT_WHITE);
  
  // Info section
  tft.setCursor(10, 50);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("RFID Tag:");
  
  tft.setCursor(10, 70);
  tft.println("IP Address:");
  tft.setCursor(100, 70);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println(WiFi.localIP().toString());
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(10, 90);
  tft.println("Relay Status:");
  
  tft.setCursor(10, 120);
  tft.println("Activity Count:");
  tft.setCursor(160, 120);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("0");
  
  // Draw dividers
  tft.drawFastHLine(0, 160, 320, TFT_WHITE);
  tft.drawFastHLine(0, 180, 320, TFT_WHITE);
  tft.drawFastVLine(160, 160, 20, TFT_WHITE);
  
  // Audio control section
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(10, 165);
  tft.println("Audio Controls");
  
  // Draw Bluetooth section
  tft.drawFastHLine(0, 220, 320, TFT_WHITE);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setCursor(10, 225);
  tft.println("Bluetooth Devices");
  
  // Draw scan button
  tft.drawRect(260, 225, 50, 20, TFT_CYAN);
  tft.setCursor(265, 230);
  tft.println("Scan");
  
  // Draw volume controls and status
  drawAudioControls();
  drawBluetoothControls();
}

void drawVolumeSlider(int x, int y, int width, int height, int volume) {
  int fillWidth = map(volume, 0, 255, 0, width);
  tft.drawRect(x, y, width, height, TFT_WHITE);
  tft.fillRect(x, y, fillWidth, height, TFT_GREEN);
}

void drawAudioControls() {
#ifdef WITH_AUDIO
  // Clear audio controls area
  tft.fillRect(0, 180, 320, 40, TFT_BLACK);
  
  // Draw volume slider
  tft.setCursor(10, 185);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Volume:");
  
  drawVolumeSlider(10, 200, 300, 10, audioManager.getBuiltinVolume());
  
  // Draw audio status
  tft.fillRect(10, 190, 80, 15, TFT_BLACK);
  tft.setCursor(80, 185);
  tft.setTextColor(audioManager.isBuiltinAudioEnabled() ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.println(audioManager.isBuiltinAudioEnabled() ? "ON" : "OFF");
#endif
}

void drawBluetoothControls() {
#ifdef WITH_AUDIO
  // Clear Bluetooth status area
  tft.fillRect(0, 240, 320, 40, TFT_BLACK);
  
  // Draw Bluetooth status
  tft.setCursor(10, 245);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Bluetooth:");
  
  tft.setCursor(80, 245);
  tft.setTextColor(audioManager.isBluetoothEnabled() ? TFT_GREEN : TFT_RED, TFT_BLACK);
  tft.println(audioManager.isBluetoothEnabled() ? "ON" : "OFF");
  
  // Show connected devices
  tft.setCursor(10, 260);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  String devices = audioManager.getConnectedBluetoothDevices();
  tft.println(devices.length() > 0 ? devices : "No connected devices");
#endif
}

void drawLdrSection() {
  // Create a section for the light sensor on screen
  tft.fillRect(180, 120, 140, 40, TFT_BLACK);
  
  // Draw label
  tft.setCursor(180, 120);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Light Level:");
  
  // Draw value
  int lightPercent = map(ldrValue, 0, 4095, 0, 100);
  tft.setCursor(180, 135);
  
  // Different colors based on light level
  if (lightPercent < 30) {
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
  } else if (lightPercent < 70) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  } else {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  }
  
  tft.println(String(lightPercent) + "% (" + String(ldrValue) + ")");
  
  // Draw light level bar
  int barWidth = 100;
  int barHeight = 10;
  int x = 180;
  int y = 150;
  int fillWidth = map(lightPercent, 0, 100, 0, barWidth);
  
  // Draw outline
  tft.drawRect(x, y, barWidth, barHeight, TFT_WHITE);
  
  // Fill based on level
  if (lightPercent < 30) {
    tft.fillRect(x, y, fillWidth, barHeight, TFT_BLUE);
  } else if (lightPercent < 70) {
    tft.fillRect(x, y, fillWidth, barHeight, TFT_GREEN);
  } else {
    tft.fillRect(x, y, fillWidth, barHeight, TFT_YELLOW);
  }
}