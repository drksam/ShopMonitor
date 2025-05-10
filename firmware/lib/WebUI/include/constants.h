#ifndef CONSTANTS_H
#define CONSTANTS_H

// Node types
#define NODE_TYPE_MACHINE_MONITOR 0
#define NODE_TYPE_OFFICE_READER 1
#define NODE_TYPE_ACCESSORY_IO 2

// Default configuration values
#define DEFAULT_NODE_NAME "ShopNode"
#define DEFAULT_SERVER_URL "http://192.168.1.100:5000"

// Pin definitions for SPI RFID readers
// RFID Reader 1 (SPI)
#define RFID1_SS_PIN 5
#define RFID1_RST_PIN 22
#define SS_PIN1 RFID1_SS_PIN  // Backward compatibility
#define RST_PIN RFID1_RST_PIN  // Common RST pin definition for backward compatibility

// RFID Reader 2 (SPI)
#define RFID2_SS_PIN 4
#define RFID2_RST_PIN 23
#define SS_PIN2 RFID2_SS_PIN  // Backward compatibility

// RFID Reader 3 (SPI)
#define RFID3_SS_PIN 2
#define RFID3_RST_PIN 21
#define SS_PIN3 RFID3_SS_PIN  // Backward compatibility

// RFID Reader 4 (SPI)
#define RFID4_SS_PIN 15
#define RFID4_RST_PIN 19
#define SS_PIN4 RFID4_SS_PIN  // Backward compatibility

// Common SPI pins (shared by all readers)
#define SPI_SCK_PIN 18
#define SPI_MISO_PIN 26
#define SPI_MOSI_PIN 25

// Relay control pins
#define RELAY1_PIN 32
#define RELAY2_PIN 33
#define RELAY3_PIN 27
#define RELAY4_PIN 14

// Activity sensor pins
#define ACTIVITY1_PIN 35
#define ACTIVITY2_PIN 34
#define ACTIVITY3_PIN 39
#define ACTIVITY4_PIN 36

// LED indicators
#define STATUS_LED_PIN 13
#define RGB_LED_PIN 12
#define LED_PIN STATUS_LED_PIN  // Alias for backward compatibility

// Buzzer
#define BUZZER_PIN 16

// Buttons
#define RESET_BUTTON_PIN 17
#define BUTTON1_PIN 16  // Using buzzer pin as dual-purpose button
#define BUTTON2_PIN 17  // Using reset button as dual-purpose button

// Network settings
#define DEFAULT_AP_PASSWORD "Pigfloors"
#define AP_PASSWORD DEFAULT_AP_PASSWORD  // Alias for backward compatibility
#define WIFI_CONNECTION_TIMEOUT 20000
#define WIFI_TIMEOUT_SECONDS 20
#define MDNS_SERVICE "_Shop-node"
#define MDNS_SERVICE_TYPE "_Shop-node"
#define MAX_WIFI_SCAN_RESULTS 20

// Server connection settings
#define SERVER_CHECK_INTERVAL 300000  // 5 minutes
#define ACTIVITY_REPORT_INTERVAL 600000  // 10 minutes
#define REPORT_INTERVAL ACTIVITY_REPORT_INTERVAL  // Alias for backward compatibility

// Web UI settings
#define WEB_SERVER_PORT 80

// EEPROM/Preferences settings
#define CONFIG_NAMESPACE "Shop"
#define PREFERENCES_NAMESPACE CONFIG_NAMESPACE  // Alias for backward compatibility

// Maximum buffer sizes
#define MAX_JSON_SIZE 2048
#define MAX_UID_SIZE 32

#endif // CONSTANTS_H