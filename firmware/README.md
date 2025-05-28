# ShopMonitor Firmware

This directory contains the firmware code for ESP32-based monitoring devices used in the ShopMonitor system. The firmware supports different hardware variants and node types.

## Firmware Variants

### 1. Standard ESP32 (esp32/)
The standard ESP32 firmware supports the following node types:
- **Machine Monitor**: Controls machine access with RFID and monitors activity
- **Office RFID Reader**: Registers new RFID cards into the system
- **Accessory IO Controller**: Manages external devices (fans, lights, etc.)

### 2. ESP32-CYD with Audio (esp32-cyd/)
The ESP32-CYD firmware includes all standard functionality plus:
- **Audio feedback** for user interactions
- **Bluetooth audio** connectivity for announcements and music
- **TFT display** with touch interface
- **Network audio** streaming support

## Building the Firmware

### Prerequisites
- PlatformIO (CLI or VSCode extension)
- Python 3.x
- Git

### Building Standard ESP32 Firmware
```bash
cd firmware/esp32
platformio run
```
The compiled binary will be available at `firmware/esp32/.pio/build/esp32dev/firmware.bin`

### Building ESP32-CYD Firmware
```bash
cd firmware/esp32-cyd
platformio run
```
The compiled binary will be available at `firmware/esp32-cyd/.pio/build/esp32-cyd/firmware.bin`

### Building Both Variants at Once
```bash
cd firmware
chmod +x package.sh  # Make the script executable (only needed once)
./package.sh
```
This will create two zip files:
- `esp32_shop_firmware.zip` - Standard ESP32 firmware
- `esp32_cyd_audio_firmware.zip` - ESP32-CYD firmware with audio support

## Flashing the Firmware

### Using PlatformIO
```bash
cd firmware/esp32  # or esp32-cyd
platformio run --target upload
```

### Using esptool.py (Alternative Method)
```bash
esptool.py --chip esp32 --port [PORT] --baud 921600 write_flash -z 0x10000 firmware.bin
```
Replace `[PORT]` with your device's serial port (e.g., COM3, /dev/ttyUSB0).

## Hardware Configuration

### ESP32 Standard Pinout
- **RFID Readers**: Up to 4 RFID-RC522 modules via SPI
- **Relay Control**: 4 relay outputs for machine control
- **Activity Sensors**: 4 input pins for activity monitoring
- **E-Stop**: Emergency stop input and relay output
- **Indicators**: Built-in LED and optional WS2812B LEDs

See `include/constants.h` for the complete pin mapping.

### ESP32-CYD Pinout
- **Display**: 320x240 TFT display with touch
- **RFID Reader**: Single RFID-RC522 module
- **Audio**: I2S DAC output for audio
- **Relay**: Single relay output
- **Activity Sensor**: Single activity input
- **E-Stop**: Emergency stop input and relay output

## Node Types and Features

### Machine Monitor
- RFID-based machine access control
- Activity monitoring and reporting
- Automatic logout after inactivity
- E-stop monitoring
- Offline access with saved RFID cards

### Office RFID Reader
- RFID card registration
- User authentication
- Server synchronization of user data

### Accessory IO Controller
- Control of accessory devices (fans, lights, etc.)
- Sensor input monitoring
- Scheduled operations
- Network control via REST API

## Configuration

On first boot, the device creates an access point for configuration:
- SSID: `ShopMonitor-Setup`
- Password: `shopsetup`

Connect to this network and navigate to `http://192.168.4.1` to configure:
- WiFi settings
- Node name and type
- Server connection details
- Machine IDs (for Machine Monitor type)

## API Endpoints

### Local Device API
- `/api/status` - Get device status
- `/api/config` - Get/set device configuration
- `/api/reboot` - Reboot device
- `/api/reset` - Factory reset
- `/api/relay` - Control relays
- `/api/wifi/scan` - Scan for WiFi networks

### Server Communication API
- `/api/check_user` - Check user access
- `/api/update_count` - Report activity count
- `/api/logout` - Log out user
- `/api/offline_cards` - Sync offline cards