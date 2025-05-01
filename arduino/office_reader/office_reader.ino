#include <SPI.h>
#include <Ethernet.h>
#include <FastLED.h>
#include <MFRC522.h>

#define LED_PIN 6          // Pin for WS2812B LED status indicator
#define NUM_LEDS 1         // Number of WS2812B LEDs
#define BUTTON_PIN 4       // Pin for the registration button
#define RST_PIN 9          // RFID reader reset pin
#define SS_PIN 10          // RFID reader SS pin

MFRC522 rfidReader(SS_PIN, RST_PIN);  // Create MFRC522 instance

CRGB leds[NUM_LEDS];

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC Address for Ethernet
IPAddress server(10, 4, 1, 7); // Server IP address - will be replaced dynamically
EthernetClient client;

String lastScannedRFID = "";
bool readyForRegistration = false;
unsigned long buttonPressTime = 0;
unsigned long lastLedUpdate = 0;

void setup() {
  Serial.begin(9600);
  
  // Initialize button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize SPI bus
  SPI.begin();
  
  // Initialize RFID reader
  rfidReader.PCD_Init();
  Serial.print("RFID Reader firmware version: ");
  Serial.println(rfidReader.PCD_GetVersion(), HEX);
  
  // Initialize WS2812B LED
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  setLedColor(CRGB::Blue);  // Default color (waiting for button press)
  
  // Start Ethernet with DHCP
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    while (true) {
      // Flash red LED to indicate failure
      leds[0] = CRGB::Red;
      FastLED.show();
      delay(300);
      leds[0] = CRGB::Black;
      FastLED.show();
      delay(300);
    }
  }

  delay(1000); // Allow some time for DHCP to assign IP

  // Print network information
  printNetworkInfo();

  Serial.println("Office RFID Reader Ready");
  Serial.println("Press the button to enter registration mode.");
}

void loop() {
  // Check for button press to enter registration mode
  if (digitalRead(BUTTON_PIN) == LOW) {
    buttonPressTime = millis();
    while (digitalRead(BUTTON_PIN) == LOW) {
      // Wait for button release or long press
      if (millis() - buttonPressTime > 2000) {
        // Long press detected - cancel registration mode
        if (readyForRegistration) {
          readyForRegistration = false;
          lastScannedRFID = "";
          setLedColor(CRGB::Blue);
          Serial.println("Registration mode canceled");
          
          // Wait for button release
          while (digitalRead(BUTTON_PIN) == LOW) {
            delay(10);
          }
          
          break;
        }
      }
      delay(10);
    }
    
    // If it was a short press and we're not in registration mode, enter registration mode
    if (millis() - buttonPressTime < 2000 && !readyForRegistration) {
      readyForRegistration = true;
      Serial.println("Registration mode activated! Scan an RFID card now.");
      // Start flashing green indicating ready to scan
      setLedColor(CRGB::Green);
    }
  }
  
  // Flash LED when in registration mode
  if (readyForRegistration && millis() - lastLedUpdate > 500) {
    static bool ledOn = true;
    ledOn = !ledOn;
    if (ledOn) {
      setLedColor(CRGB::Green);
    } else {
      setLedColor(CRGB::Black);
    }
    lastLedUpdate = millis();
  }
  
  // Check for RFID scan in registration mode
  if (readyForRegistration) {
    // Look for new cards
    if (!rfidReader.PICC_IsNewCardPresent()) {
      return;
    }
    
    // Select one of the cards
    if (!rfidReader.PICC_ReadCardSerial()) {
      return;
    }
    
    // Convert the UID to a string
    String rfid = "";
    for (byte i = 0; i < rfidReader.uid.size; i++) {
      // Add leading zero for values less than 0x10
      if (rfidReader.uid.uidByte[i] < 0x10) {
        rfid += "0";
      }
      rfid += String(rfidReader.uid.uidByte[i], HEX);
    }
    rfid.toUpperCase();
    
    if (rfid.length() > 0) {
      lastScannedRFID = rfid;
      Serial.println("RFID Card Scanned: " + lastScannedRFID);
      Serial.println("Press the button again to confirm and send to server.");
      
      // Show solid purple to indicate card scanned, waiting for confirmation
      setLedColor(CRGB::Purple);
      readyForRegistration = false;
      
      // Wait for confirmation button press
      bool waiting = true;
      unsigned long waitStartTime = millis();
      
      while (waiting && millis() - waitStartTime < 30000) { // 30 second timeout
        // Check for button press to confirm
        if (digitalRead(BUTTON_PIN) == LOW) {
          delay(50); // Debounce
          
          // Make sure it's a real press
          if (digitalRead(BUTTON_PIN) == LOW) {
            // Wait for release
            while (digitalRead(BUTTON_PIN) == LOW) {
              delay(10);
            }
            
            // Send the RFID to server
            sendRFIDtoServer(lastScannedRFID);
            waiting = false;
          }
        }
        delay(10);
      }
      
      // If we timed out, reset
      if (waiting) {
        Serial.println("Confirmation timeout. Registration canceled.");
        lastScannedRFID = "";
        setLedColor(CRGB::Blue);
      }
    }
  }
}

// Send the RFID card to the server for registration
void sendRFIDtoServer(String rfid) {
  setLedColor(CRGB::Yellow); // Indicate we're connecting to server
  
  if (client.connect(server, 5000)) { // Connect to server port
    Serial.println("Connected to server, sending RFID for registration...");
    
    // Send a POST request to the server
    client.println("POST /api/register_rfid HTTP/1.1");
    client.print("Host: ");
    client.println(server);
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    
    // Calculate content length
    String postData = "rfid=" + rfid;
    client.println(postData.length());
    client.println();
    client.println(postData);
    
    // Wait for response
    unsigned long timeout = millis();
    while (client.connected() && millis() - timeout < 10000) {
      if (client.available()) {
        String response = client.readString();
        
        if (response.indexOf("SUCCESS") != -1) {
          Serial.println("RFID registration successful!");
          successAnimation();
        } else {
          Serial.println("Failed to register RFID:");
          Serial.println(response);
          failureAnimation();
        }
        break;
      }
      delay(50);
    }
    
    // If we timed out
    if (millis() - timeout >= 10000) {
      Serial.println("Timeout waiting for server response");
      failureAnimation();
    }
    
    client.stop();
  } else {
    Serial.println("Failed to connect to server");
    failureAnimation();
  }
  
  // Reset to ready state
  setLedColor(CRGB::Blue);
}

// Success animation - flash green 3 times
void successAnimation() {
  for (int i = 0; i < 3; i++) {
    setLedColor(CRGB::Green);
    delay(200);
    setLedColor(CRGB::Black);
    delay(200);
  }
  setLedColor(CRGB::Green);
  delay(1000);
}

// Failure animation - flash red 3 times
void failureAnimation() {
  for (int i = 0; i < 3; i++) {
    setLedColor(CRGB::Red);
    delay(200);
    setLedColor(CRGB::Black);
    delay(200);
  }
  setLedColor(CRGB::Red);
  delay(1000);
}

// Set the color of the WS2812B LED
void setLedColor(CRGB color) {
  leds[0] = color;
  FastLED.show();
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
}