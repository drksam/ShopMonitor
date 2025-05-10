# ESP32 Multi-Function Node Firmware

This firmware is designed for ESP32 DevKit V1 boards to create modular IoT nodes for the ShopMonitoring System, with multiple operating modes and seamless web configuration.

## Hardware Requirements

- ESP32 DevKit V1 board (ESP-WROOM-32)
- RFID-RC522 module(s) - Up to 4 for Machine Monitor mode
- Relay modules (up to 4) for Machine control
- Activity sensors (up to 4) for monitoring machine usage
- Status LED (connected to GPIO5)
- Buzzer for audio feedback (connected to GPIO15)
- Push buttons for manual control

## Features

- **Multiple Operating Modes:**
  - **Machine Monitor**: Controls up to 4 machines with RFID access and activity tracking
  - **Office RFID Reader**: Single RFID reader for registration or authentication
  - **Accessory IO Controller**: Controls relays and monitors inputs for accessories
- **Web Configuration Interface**: Easy setup through browser-based config portal
- **Automatic Device Discovery**: mDNS-based discovery for easy integration
- **WiFi Management**: Fallback AP mode with network scanning capability
- **OTA Updates**: Firmware updates over-the-air
- **Offline Mode**: Continued operation when server connection is unavailable
- **Activity Counting**: Tracks machine activity and reports periodically

## Project Structure

```
firmware/
├── include/             # Global header files
│   └── constants.h      # System-wide constants and pin definitions
├── lib/                 # Component libraries
│   ├── ConfigManager/   # Configuration persistence
│   ├── NetworkManager/  # WiFi and server communication
│   ├── RFIDHandler/     # RFID reader management
│   └── WebUI/           # Web interface components
├── src/                 # Main application code
│   ├── main.cpp         # Primary application logic
│   ├── dashboard.h      # Web interface HTML (embedded)
│   └── wiring_diagram.h # Hardware connection diagram (embedded)
├── platformio.ini       # PlatformIO configuration
└── README.md            # This file
```

## Setup Instructions

### PlatformIO Setup

1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install [PlatformIO Extension](https://platformio.org/install/ide?install=vscode)
3. Clone or download this repository
4. Open the firmware folder in VS Code
5. PlatformIO will automatically detect the project and install dependencies

### Building and Uploading

```bash
# Build the project
platformio run

# Upload to ESP32
platformio run --target upload

# Monitor serial output
platformio device monitor
```

### First Time Setup

When first powered on, the ESP32 will create a WiFi access point named "ShopNode_XXXXXX".

1. Connect to this WiFi network (password: Shop123)
2. Open a web browser and navigate to http://192.168.4.1
3. Configure the node name, type, WiFi credentials and server URL
4. The device will restart and connect to your WiFi network

## Pin Configuration

### SPI Pins (Fixed on ESP32)
- MOSI: GPIO23
- MISO: GPIO19
- SCK: GPIO18

### RFID Reader Pins
- RFID 1 SS: GPIO21
- RFID 2 SS: GPIO17
- RFID 3 SS: GPIO16
- RFID 4 SS: GPIO4
- Shared RST: GPIO22

### Output Pins
- Relay 1: GPIO13
- Relay 2: GPIO12
- Relay 3: GPIO14
- Relay 4: GPIO27
- Status LED: GPIO5
- Buzzer: GPIO15

### Input Pins
- Activity Sensor 1: GPIO36
- Activity Sensor 2: GPIO39
- Activity Sensor 3: GPIO34
- Activity Sensor 4: GPIO35
- Button 1: GPIO33
- Button 2: GPIO32

## API Endpoints

The firmware exposes the following REST API endpoints:

- **GET /**: Web configuration dashboard
- **GET /wiring**: Wiring diagram and pin connections
- **GET /api/status**: Returns the current status of the node
- **GET /api/config**: Returns the current configuration
- **POST /api/config**: Updates the configuration
- **GET /api/scan_wifi**: Scans for available WiFi networks
- **GET /api/reboot**: Reboots the device
- **GET /api/reset**: Factory resets the device
- **GET /api/relay**: Controls relay outputs (Accessory IO mode)

## Reset and Recovery

- To perform a factory reset, press and hold both buttons (GPIO33 and GPIO32) for 5 seconds
- If WiFi connection fails, the device will automatically enter AP mode for reconfiguration

## License

This firmware is licensed under the MIT License.

## Acknowledgments

- ESP32 Arduino Core
- MFRC522 library by GitHub user miguelbalboa
- AsyncTCP and ESPAsyncWebServer by me-no-dev
- ArduinoJSON by Benoit Blanchon
- NTPClient by Fabrice Weinberg