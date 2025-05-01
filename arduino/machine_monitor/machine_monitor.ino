#include <SPI.h>
#include <Ethernet.h>
#include <SoftwareSerial.h>
#include <FastLED.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <EthernetUdp.h>

// Configuration constants
#define NUM_ZONES 4        // Number of zones/machines to monitor
#define ACTIVITY_TIMEOUT 3600000 // Activity timeout in ms (1 hour)
#define WARNING_TIMEOUT 300000  // Warning time before timeout (5 minutes)
#define RELAY_OFF_DELAY 10000   // Delay before turning off relay (10 seconds)
#define COUNT_UPDATE_INTERVAL 600000 // Activity count update interval (10 minutes)

// Machine states
#define IDLE 0
#define ACCESS_GRANTED 1
#define ACCESS_DENIED 2
#define LOGGED_OUT 3

// FastLED configuration
#define LED_PIN     2
#define NUM_LEDS    4
CRGB leds[NUM_LEDS];

// Color definitions for machine statuses
CRGB statusColors[] = {
  CRGB::Blue,   // IDLE - Blue
  CRGB::Green,  // ACCESS_GRANTED - Green
  CRGB::Red,    // ACCESS_DENIED - Red
  CRGB::Purple  // LOGGED_OUT - Purple (transitioning)
};

// RFID reader pins (Software Serial)
SoftwareSerial rfidReaders[NUM_ZONES] = {
  SoftwareSerial(22, 23),  // RX, TX for Zone 0
  SoftwareSerial(24, 25),  // RX, TX for Zone 1
  SoftwareSerial(26, 27),  // RX, TX for Zone 2
  SoftwareSerial(28, 29)   // RX, TX for Zone 3
};

// Pin configurations
const int RELAY_PINS[NUM_ZONES] = {7, 8, 9, 10};  // Relay control pins for each zone
const int ACTIVITY_PINS[NUM_ZONES] = {A0, A1, A2, A3};  // Activity detection pins

// Ethernet configuration
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress server(192, 168, 1, 100);  // Default server IP - will be updated from EEPROM
EthernetClient client;

// NTP Client for time synchronization
EthernetUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// Status tracking variables
int machineStatus[NUM_ZONES] = {IDLE, IDLE, IDLE, IDLE};
String activeRFIDs[NUM_ZONES] = {"", "", "", ""};  // Currently active RFIDs for each zone
String machineIDs[NUM_ZONES] = {"01", "02", "03", "04"};  // Machine IDs (configurable)

// Timing variables
unsigned long lastActivityTimes[NUM_ZONES] = {0, 0, 0, 0};
unsigned long relayOffTimes[NUM_ZONES] = {0, 0, 0, 0};
const unsigned long activityTimeout = ACTIVITY_TIMEOUT;  // Activity timeout in ms (1 hour)
const unsigned long warningTimeout = WARNING_TIMEOUT;    // Warning time before timeout (5 min)
const unsigned long relayOffDelay = RELAY_OFF_DELAY;     // Delay for relay turn-off

// Activity count variables
unsigned long activityCounts[NUM_ZONES] = {0, 0, 0, 0};           // Activity counts for each zone
unsigned long lastCountUpdateTimes[NUM_ZONES] = {0, 0, 0, 0};     // Last time count was updated to server
boolean previousActivityState[NUM_ZONES] = {false, false, false, false}; // Previous activity state
const unsigned long countUpdateInterval = COUNT_UPDATE_INTERVAL;  // How often to send count to server (10 min)

// LED warning state variables for timeout
boolean flashingYellow[NUM_ZONES] = {false, false, false, false};
boolean yellowLedStates[NUM_ZONES] = {false, false, false, false};
unsigned long lastYellowFlashTimes[NUM_ZONES] = {0, 0, 0, 0};

void setup() {
  // Initialize serial for debugging
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port to connect (only for native USB port)
  }
  
  Serial.println("Machine Monitor Starting...");
  
  // Initialize FastLED 
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(100);
  
  // Set all LEDs to blue (IDLE)
  for (int i = 0; i < NUM_ZONES; i++) {
    leds[i] = statusColors[IDLE];
  }
  FastLED.show();
  
  // Initialize all RFID readers
  for (int i = 0; i < NUM_ZONES; i++) {
    rfidReaders[i].begin(9600);
    Serial.print("RFID reader ");
    Serial.print(i);
    Serial.println(" initialized");
  }
  
  // Initialize relay pins as outputs and set to LOW (off)
  for (int i = 0; i < NUM_ZONES; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], LOW);
    Serial.print("Relay pin ");
    Serial.print(RELAY_PINS[i]);
    Serial.println(" initialized");
  }
  
  // Initialize activity pins as inputs with pulldown
  for (int i = 0; i < NUM_ZONES; i++) {
    pinMode(ACTIVITY_PINS[i], INPUT_PULLUP);
    Serial.print("Activity pin ");
    Serial.print(ACTIVITY_PINS[i]);
    Serial.println(" initialized");
  }
  
  // Load configuration from EEPROM if available
  // (Machine IDs, server IP, etc.)
  
  // Start Ethernet connection
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Use static IP as fallback
    IPAddress ip(192, 168, 1, 177);
    Ethernet.begin(mac, ip);
  }
  
  // Print the IP address
  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());
  
  // Initialize NTP client and update time
  timeClient.begin();
  timeClient.update();
  
  // Print network information
  printNetworkInfo();
  
  // Initial setup complete
  Serial.println("Machine monitor initialized. Ready for RFID inputs on all zones.");
}

void loop() {
  // Check all RFID readers
  for (int zone = 0; zone < NUM_ZONES; zone++) {
    checkRFIDReader(zone);
  }

  // Monitor activity on all machines
  for (int zone = 0; zone < NUM_ZONES; zone++) {
    monitorActivity(zone);
  }

  // Check for timeouts and update LEDs for all machines
  for (int zone = 0; zone < NUM_ZONES; zone++) {
    checkTimeout(zone);
    updateLEDs(zone);
  }

  // Periodically update NTP time
  static unsigned long lastNtpUpdate = 0;
  if (millis() - lastNtpUpdate > 3600000) { // Once per hour
    timeClient.update();
    lastNtpUpdate = millis();
  }
  
  // Periodically synchronize the offline access card list
  static unsigned long lastOfflineSync = 0;
  if (millis() - lastOfflineSync > 3600000) { // Once per hour
    syncOfflineAccessList(); // Update offline access cards from server
    lastOfflineSync = millis();
  }
  
  // Periodically send activity count updates to server (every 10 minutes)
  for (int zone = 0; zone < NUM_ZONES; zone++) {
    if (machineStatus[zone] == ACCESS_GRANTED && 
        (millis() - lastCountUpdateTimes[zone] > countUpdateInterval)) {
      // Send count update to server
      sendCountUpdate(zone, activityCounts[zone]);
      lastCountUpdateTimes[zone] = millis();
    }
  }
}

// Check RFID reader for a specific zone
void checkRFIDReader(int zone) {
  if (rfidReaders[zone].available()) {
    // Buffer to store RFID data
    char rfidBuffer[16];
    int idx = 0;
    
    // Read until we get a newline or a timeout
    unsigned long startTime = millis();
    while (millis() - startTime < 500 && idx < 15) {
      if (rfidReaders[zone].available()) {
        char c = rfidReaders[zone].read();
        if (c == '\n' || c == '\r') {
          break;
        }
        rfidBuffer[idx++] = c;
      }
    }
    rfidBuffer[idx] = '\0'; // Null-terminate the string
    
    // Convert to a String
    String rfid = String(rfidBuffer);
    rfid.trim();
    
    if (rfid.length() > 0) {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.print(" RFID scanned: ");
      Serial.println(rfid);
      
      // Check with the server for access permission
      checkUserAccess(zone, rfid);
    }
  }
}

// Monitor activity on a specific machine
void monitorActivity(int zone) {
  // Check if the activity pin is high (machine in use)
  boolean currentActivity = (digitalRead(ACTIVITY_PINS[zone]) == HIGH);
  
  // If the machine is active and has a user
  if (machineStatus[zone] == ACCESS_GRANTED) {
    // Reset timeout timer when activity is detected
    if (currentActivity) {
      lastActivityTimes[zone] = millis();
      
      // If this is a new activity event (rising edge), increment the count
      if (!previousActivityState[zone]) {
        activityCounts[zone]++;
        sendHeartbeat(zone); // Send heartbeat to server
        
        Serial.print("Zone ");
        Serial.print(zone);
        Serial.print(" - Activity count: ");
        Serial.println(activityCounts[zone]);
      }
    }
    
    // Update previous state
    previousActivityState[zone] = currentActivity;
  }
}

// Check timeout status for a specific machine
void checkTimeout(int zone) {
  // Only check machines that have active users
  if (machineStatus[zone] == ACCESS_GRANTED) {
    unsigned long currentTime = millis();
    
    // Check for warning condition (approaching timeout)
    if (currentTime - lastActivityTimes[zone] > (activityTimeout - warningTimeout)) {
      flashingYellow[zone] = true;
    }
    
    // Check for timeout condition
    if (currentTime - lastActivityTimes[zone] > activityTimeout) {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" timed out due to inactivity.");
      logOutUser(zone);
    }
  }
  
  // Handle relay off delay
  if (machineStatus[zone] == LOGGED_OUT) {
    if (millis() - relayOffTimes[zone] > relayOffDelay) {
      digitalWrite(RELAY_PINS[zone], LOW);  // Deactivate relay after delay
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" relay deactivated after delay.");
      
      // Back to idle state
      machineStatus[zone] = IDLE;
      leds[zone] = statusColors[IDLE];
      FastLED.show();
    }
  }
}

// Update LEDs for each zone
void updateLEDs(int zone) {
  // Handle flashing yellow warning
  if (flashingYellow[zone]) {
    // Flash the LED yellow for warning
    if (millis() - lastYellowFlashTimes[zone] > 500) {
      yellowLedStates[zone] = !yellowLedStates[zone];
      leds[zone] = yellowLedStates[zone] ? CRGB::Yellow : CRGB::Black;
      FastLED.show();
      lastYellowFlashTimes[zone] = millis();
    }
  }
}

// Function to check user access with the server
void checkUserAccess(int zone, String rfid) {
  if (client.connect(server, 5000)) { // Connect to server port
    // Send a GET request to the server
    client.print("GET /api/check_user?rfid=" + rfid + "&machine_id=" + machineIDs[zone] + " HTTP/1.1\r\n");
    client.print("Host: ");
    client.print(server);
    client.print("\r\n");
    client.print("Connection: close\r\n\r\n");

    // Wait for the server's response
    String response = "";
    bool responseFound = false;
    
    unsigned long responseTimeout = millis() + 5000; // 5-second timeout
    while (client.connected() && millis() < responseTimeout) {
      if (client.available()) {
        char c = client.read();
        response += c;
        
        // Check if we've found our response code
        if (response.indexOf("ALLOW") != -1) {
          responseFound = true;
          
          // If another user is logged in, log them out first
          if (activeRFIDs[zone].length() > 0 && activeRFIDs[zone] != rfid) {
            sendLogoutRequest(zone);
            logOutUser(zone);
            delay(500); // Small delay to ensure logout is processed
          }
          
          // Grant access to the new user
          grantAccess(zone, rfid);
          break;
        } else if (response.indexOf("DENY") != -1) {
          responseFound = true;
          denyAccess(zone);
          break;
        } else if (response.indexOf("LOGOUT") != -1) {
          responseFound = true;
          logOutUser(zone);
          break;
        }
      }
    }
    
    client.stop();
    
    // If we didn't get a valid response
    if (!responseFound) {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" - No valid response from server");
      denyAccess(zone);
    }
  } else {
    Serial.print("Zone ");
    Serial.print(zone);
    Serial.println(" - Failed to connect to server, checking for offline access...");
    
    // Check if this RFID is stored in EEPROM as having offline access
    if (checkOfflineAccess(rfid, zone)) {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" - Offline access granted");
      grantAccess(zone, rfid);
    } else {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" - No offline access privileges");
      denyAccess(zone);
    }
  }
}

// Send a logout request to the server
void sendLogoutRequest(int zone) {
  if (activeRFIDs[zone].length() > 0) {
    // Send the final activity count when logging out
    sendCountUpdate(zone, activityCounts[zone]);
    
    if (client.connect(server, 5000)) {
      client.print("GET /api/logout?rfid=" + activeRFIDs[zone] + "&machine_id=" + machineIDs[zone] + " HTTP/1.1\r\n");
      client.print("Host: ");
      client.print(server);
      client.print("\r\n");
      client.print("Connection: close\r\n\r\n");

      // Wait briefly for the server's response
      delay(200);
      client.stop();
      
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.print(" - Logout request sent for RFID: ");
      Serial.println(activeRFIDs[zone]);
    }
  }
}

// Send a heartbeat to the server to indicate machine activity
void sendHeartbeat(int zone) {
  if (activeRFIDs[zone].length() > 0 && machineStatus[zone] == ACCESS_GRANTED) {
    if (client.connect(server, 5000)) {
      client.print("GET /api/heartbeat?machine_id=" + machineIDs[zone] + "&activity=1 HTTP/1.1\r\n");
      client.print("Host: ");
      client.print(server);
      client.print("\r\n");
      client.print("Connection: close\r\n\r\n");

      // Don't wait for response, just send and continue
      client.stop();
    }
  }
}

// Send activity count update to the server
void sendCountUpdate(int zone, unsigned long count) {
  if (activeRFIDs[zone].length() > 0) {
    if (client.connect(server, 5000)) {
      client.print("GET /api/update_count?machine_id=" + machineIDs[zone] + 
                  "&rfid=" + activeRFIDs[zone] + 
                  "&count=" + String(count) + " HTTP/1.1\r\n");
      client.print("Host: ");
      client.print(server);
      client.print("\r\n");
      client.print("Connection: close\r\n\r\n");

      // Wait briefly for the server's response
      delay(200);
      client.stop();
      
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.print(" - Count update sent: ");
      Serial.println(count);
    } else {
      Serial.print("Zone ");
      Serial.print(zone);
      Serial.println(" - Failed to connect to server for count update");
    }
  }
}

// Grant access to a machine
void grantAccess(int zone, String rfid) {
  machineStatus[zone] = ACCESS_GRANTED;
  activeRFIDs[zone] = rfid;        // Store the active RFID
  digitalWrite(RELAY_PINS[zone], HIGH); // Activate relay (turn on)
  
  // Reset activity counters
  activityCounts[zone] = 0;
  lastCountUpdateTimes[zone] = millis();
  previousActivityState[zone] = false;
  
  // Set LED to green for access granted
  leds[zone] = statusColors[ACCESS_GRANTED];
  flashingYellow[zone] = false;
  FastLED.show();
  
  Serial.print("Zone ");
  Serial.print(zone);
  Serial.print(" - Access granted for RFID: ");
  Serial.println(rfid);
  
  lastActivityTimes[zone] = millis();   // Reset the activity timer
}

// Deny access to a machine
void denyAccess(int zone) {
  machineStatus[zone] = ACCESS_DENIED;
  
  // Set LED to red for access denied
  leds[zone] = statusColors[ACCESS_DENIED];
  FastLED.show();
  
  Serial.print("Zone ");
  Serial.print(zone);
  Serial.println(" - Access denied");
  
  // Flash red LED briefly
  for (int i = 0; i < 5; i++) {
    leds[zone] = CRGB::Black;
    FastLED.show();
    delay(100);
    leds[zone] = CRGB::Red;
    FastLED.show();
    delay(100);
  }
  
  // Return to blue idle state after denial
  delay(1000);
  leds[zone] = statusColors[IDLE];
  machineStatus[zone] = IDLE;
  FastLED.show();
}

// Log out the user from a machine
void logOutUser(int zone) {
  if (activeRFIDs[zone].length() > 0) {
    // Send the final activity count when logging out
    sendCountUpdate(zone, activityCounts[zone]);
    
    machineStatus[zone] = LOGGED_OUT;
    relayOffTimes[zone] = millis();  // Record the time when logout happens
    
    // Set LED to blue (waiting for next user)
    leds[zone] = statusColors[LOGGED_OUT];
    flashingYellow[zone] = false;
    FastLED.show();
    
    Serial.print("Zone ");
    Serial.print(zone);
    Serial.print(" - User with RFID: ");
    Serial.print(activeRFIDs[zone]);
    Serial.println(" logged out");
    
    activeRFIDs[zone] = "";  // Clear the active RFID
  }
}

// Check if an RFID card has offline access for a zone
bool checkOfflineAccess(String rfid, int zone) {
  // In a real implementation, this would check EEPROM or flash memory
  // for a list of offline access cards and their authorized zones
  
  // For emergency use, check if card has offline access and is authorized for this zone
  // We'll implement a simple hash-based checking system for demo purposes
  
  // Read cards with offline access from EEPROM
  // Format of EEPROM:
  // Each card entry is stored as a hash in EEPROM:
  // - Bytes 0-9: First 10 offline access cards (10 bytes total)
  // - Bytes 10-19: Corresponding machine authorizations (bit flags, 1 byte per card)
  
  // Demo implementation using a simple hash as a consistency check
  // (In a real implementation, you would use a proper hash function and verify credentials)
  byte cardHash = calculateCardHash(rfid);
  
  // Check through stored offline access cards in EEPROM
  for (int i = 0; i < 10; i++) {
    byte storedHash = EEPROM.read(i);
    
    // Skip empty entries (0xFF means uninitialized EEPROM)
    if (storedHash == 0xFF) {
      continue;
    }
    
    // If we find a matching hash
    if (storedHash == cardHash) {
      // Read machine authorizations (each bit represents a zone)
      byte auth = EEPROM.read(10 + i);
      
      // Check if this zone bit is set (0 = zone 0, 1 = zone 1, etc.)
      if (auth & (1 << zone)) {
        return true;  // Card has offline access for this zone
      }
      
      // Check for admin override (0x80 = high bit set)
      if (auth & 0x80) {
        return true;  // Admin override - access to all zones
      }
    }
  }
  
  return false; // No offline access
}

// Calculate a simple hash for a card ID (for demo purposes only)
byte calculateCardHash(String rfid) {
  byte hash = 0;
  for (int i = 0; i < rfid.length(); i++) {
    hash = (hash + rfid.charAt(i)) % 256;
  }
  return hash;
}

// This function is called periodically to sync server-side offline access lists
// during normal operation - ensuring offline access works when needed
void syncOfflineAccessList() {
  if (client.connect(server, 5000)) {
    Serial.println("Connecting to server to sync offline access list...");
    
    client.print("GET /api/offline_cards HTTP/1.1\r\n");
    client.print("Host: ");
    client.print(server);
    client.print("\r\n");
    client.print("Connection: close\r\n\r\n");
    
    // Wait for server response
    unsigned long timeout = millis();
    String response = "";
    bool jsonStarted = false;
    
    while (client.connected() && millis() - timeout < 10000) {
      if (client.available()) {
        char c = client.read();
        
        // Only start recording when we reach the JSON part (after headers)
        if (c == '{') {
          jsonStarted = true;
        }
        
        if (jsonStarted) {
          response += c;
        }
      }
    }
    
    client.stop();
    
    // Process JSON if we have a valid response
    if (response.length() > 0 && response[0] == '{') {
      Serial.println("Received offline cards data from server");
      
      // Find the offline_cards array in the JSON
      int startPos = response.indexOf("\"offline_cards\":[");
      if (startPos != -1) {
        // Clear the current EEPROM storage for offline cards
        for (int i = 0; i < 20; i++) {
          EEPROM.write(i, 0xFF);  // 0xFF indicates empty slot
        }
        
        Serial.println("Processing offline cards...");
        
        // Simple JSON parsing - extract each card entry
        startPos = response.indexOf('{', startPos);
        int cardCount = 0;
        
        while (startPos != -1 && cardCount < 10) {
          int endPos = response.indexOf('}', startPos);
          if (endPos == -1) break;
          
          // Extract this card's JSON object
          String cardJson = response.substring(startPos, endPos + 1);
          
          // Extract hash and auth byte
          int hashPos = cardJson.indexOf("\"hash\":");
          int authPos = cardJson.indexOf("\"auth_byte\":");
          
          if (hashPos != -1 && authPos != -1) {
            // Extract hash value
            hashPos += 7;  // Skip "hash":
            int hashEndPos = cardJson.indexOf(',', hashPos);
            int hash = cardJson.substring(hashPos, hashEndPos).toInt();
            
            // Extract auth byte value
            authPos += 11;  // Skip "auth_byte":
            int authEndPos = cardJson.indexOf(',', authPos);
            if (authEndPos == -1) authEndPos = cardJson.indexOf('}', authPos);
            int auth = cardJson.substring(authPos, authEndPos).toInt();
            
            // Store in EEPROM if valid
            if (hash >= 0 && hash < 256 && auth >= 0 && auth < 256) {
              EEPROM.write(cardCount, (byte)hash);         // Store hash in first 10 bytes
              EEPROM.write(cardCount + 10, (byte)auth);    // Store auth in next 10 bytes
              Serial.print("Added offline card #");
              Serial.print(cardCount);
              Serial.print(" - Hash: ");
              Serial.print(hash);
              Serial.print(", Auth: ");
              Serial.println(auth);
              cardCount++;
            }
          }
          
          // Find the next card entry
          startPos = response.indexOf('{', endPos);
        }
        
        Serial.print("Successfully synchronized ");
        Serial.print(cardCount);
        Serial.println(" offline access cards");
      }
    } else {
      Serial.println("Failed to parse server response");
    }
  } else {
    Serial.println("Failed to connect to server for offline cards sync");
  }
}

// Print network information (IP, subnet, gateway, DNS)
void printNetworkInfo() {
  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());
  Serial.print("Subnet Mask: ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("DNS Server: ");
  Serial.println(Ethernet.dnsServerIP());
  Serial.print("Server: ");
  Serial.print(server);
  Serial.print(":");
  Serial.println(5000);
}