#pragma once

#define VERSION "1.3.0"  // Firmware version

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

// Standard ESP32 pin definitions
#ifdef ESP32_STANDARD
  // SPI bus pins
  #define PIN_SPI_MOSI           23  // SPI MOSI pin
  #define PIN_SPI_MISO           19  // SPI MISO pin
  #define PIN_SPI_SCK            18  // SPI SCK pin

  // RFID readers (up to 4 zones)
  #define MAX_RFID_READERS        4   // Maximum number of RFID readers
  #define PIN_RFID_RST           22   // Shared RST pin for all RFID readers
  #define PIN_RFID_SS_0          21   // SS pin for reader 0
  #define PIN_RFID_SS_1          17   // SS pin for reader 1
  #define PIN_RFID_SS_2          16   // SS pin for reader 2
  #define PIN_RFID_SS_3           4   // SS pin for reader 3

  // LED indicators
  #define PIN_LED_DATA            5   // WS2812B data pin
  #define NUM_LEDS                4   // Number of LEDs (one per zone)

  // Relay control pins
  #define RELAY1_PIN             13   // Relay 1 control pin
  #define RELAY2_PIN             12   // Relay 2 control pin
  #define RELAY3_PIN             14   // Relay 3 control pin
  #define RELAY4_PIN             27   // Relay 4 control pin

  // Activity monitoring pins
  #define ACTIVITY1_PIN          36   // Activity sensor 1 pin
  #define ACTIVITY2_PIN          39   // Activity sensor 2 pin
  #define ACTIVITY3_PIN          34   // Activity sensor 3 pin
  #define ACTIVITY4_PIN          35   // Activity sensor 4 pin

  // Auxiliary components
  #define BUZZER_PIN             15   // Buzzer pin
  #define LED_PIN                 2   // Built-in LED pin
  #define BUTTON1_PIN            33   // Mode button 1 pin
  #define BUTTON2_PIN            32   // Mode button 2 pin
  #define BOOT_BUTTON_PIN         0   // Boot button (built-in)

  // ESTOP 
  #define ESTOP_PIN              25   // Emergency stop input pin
  #define ESTOP_RELAY_PIN        26   // Emergency stop relay output pin
#endif

// ESP32-CYD (with TFT display and audio) pin definitions
#ifdef ESP32_CYD
  // SPI bus for RFID readers
  #define PIN_SPI_MOSI           23  // SPI MOSI pin
  #define PIN_SPI_MISO           19  // SPI MISO pin
  #define PIN_SPI_SCK            18  // SPI SCK pin
  
  // SPI bus for TFT display (separate from RFID readers)
  #define PIN_TFT_MOSI           13  // TFT MOSI pin (possibly shared if using Hardware SPI)
  #define PIN_TFT_MISO           12  // TFT MISO pin (possibly shared if using Hardware SPI)
  #define PIN_TFT_SCK            14  // TFT SCK pin (possibly shared if using Hardware SPI)
  #define PIN_TFT_CS             15  // TFT Chip Select pin
  #define PIN_TFT_DC              2  // TFT Data/Command pin
  #define PIN_TFT_RST             4  // TFT Reset pin
  #define PIN_TFT_BACKLIGHT      21  // TFT Backlight control pin

  // RFID readers (only 1 for CYD since space is limited)
  #define MAX_RFID_READERS        1  // Maximum number of RFID readers
  #define PIN_RFID_RST           22  // RFID reader reset pin
  #define PIN_RFID_SS            5   // RFID reader SS pin

  // Touch screen pins
  #define PIN_TOUCH_CS           27  // Touch controller chip select
  #define PIN_TOUCH_IRQ          26  // Touch controller interrupt pin

  // Audio pins
  #define I2S_BCLK_PIN           25  // I2S bit clock pin
  #define I2S_LRCLK_PIN          26  // I2S word select pin
  #define I2S_DATA_PIN           22  // I2S data pin
  #define AUDIO_ENABLE_PIN       17  // Audio amplifier enable pin

  // ESTOP
  #define ESTOP_PIN              33  // Emergency stop input pin
  #define ESTOP_RELAY_PIN        32  // Emergency stop relay output pin

  // LEDs
  #define LED_PIN                 2  // Built-in LED pin
  
  // Onboard RGB LED pins
  #define LED_PIN_R               4  // RGB LED Red pin
  #define LED_PIN_G              16  // RGB LED Green pin
  #define LED_PIN_B              17  // RGB LED Blue pin - overlaps with AUDIO_ENABLE_PIN
  
  // Light Dependent Resistor (LDR)
  #define LDR_PIN                34  // Light sensor pin (ADC6)
  
  // I2C Port Expander (PCF8575)
  #define I2C_SDA_PIN            21  // I2C SDA pin
  #define I2C_SCL_PIN            22  // I2C SCL pin - overlaps with PIN_RFID_RST
  #define PCF8575_ADDR          0x20  // I2C address of PCF8575 (adjust as needed)
  
  // Relay and Activity Sensor - Using PCF8575 port expander
  #define PIN_RELAY              0   // PCF8575 P0 for relay control
  #define PIN_ACTIVITY_SENSOR    1   // PCF8575 P1 for activity sensor
  
  // Button
  #define BOOT_BUTTON_PIN         0  // Boot button (built-in)
#endif

// Common constants
#define REPORT_INTERVAL        60000  // Activity reporting interval (ms)
#define ACTIVITY_TIMEOUT     3600000  // Activity timeout period (ms)
#define WARNING_TIMEOUT       300000  // Warning timeout period before logout (ms)
#define RELAY_OFF_DELAY        10000  // Delay before turning relay off (ms)