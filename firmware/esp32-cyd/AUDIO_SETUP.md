# Audio Broadcasting Setup for ESP32-CYD

This document explains how to set up audio broadcasting from a Linux PC to ESP32-CYD devices on your network.

## Prerequisites

- Linux PC with PipeWire audio system
- Network that allows UDP multicast (most home/office networks)
- ESP32-CYD devices with the ShopMonitor firmware installed

## Setup Instructions

### 1. Install Required Linux Packages

```bash
# For Ubuntu/Debian-based systems
sudo apt update
sudo apt install pipewire pipewire-pulse pipewire-audio-client-libraries gstreamer1.0-tools gstreamer1.0-plugins-good

# For Fedora/RHEL-based systems
sudo dnf install pipewire pipewire-pulseaudio gstreamer1-plugins-good
```

### 2. Create Audio Broadcasting Script

Create a file called `broadcast_audio.sh` on your Linux PC with the following content:

```bash
#!/bin/bash

MULTICAST_ADDR="239.255.12.42"
PORT=3333
AUDIO_FORMAT="audio/x-raw,channels=2,rate=44100,format=S16LE"

echo "Starting audio broadcast to ESP32-CYD devices..."
echo "Broadcasting to $MULTICAST_ADDR:$PORT"
echo "Press Ctrl+C to stop broadcasting"

# Create a virtual sink for broadcasting
pactl load-module module-null-sink sink_name=broadcast_sink sink_properties=device.description="ESP32-CYD-Broadcast"

# Start the broadcast
gst-launch-1.0 -v pulsesrc device=broadcast_sink.monitor ! audioconvert ! audioresample ! $AUDIO_FORMAT ! udpsink host=$MULTICAST_ADDR port=$PORT auto-multicast=true ttl=1

# Clean up when script exits
trap 'pactl unload-module module-null-sink' EXIT
```

Make the script executable:

```bash
chmod +x broadcast_audio.sh
```

### 3. Start Audio Broadcasting

1. Run the script:
   ```bash
   ./broadcast_audio.sh
   ```

2. In your sound settings, select "ESP32-CYD-Broadcast" as the output device for any audio you want to send to the ESP32-CYD devices.

3. Play audio from any application, and it will be broadcast to all ESP32-CYD devices on the network.

### 4. ESP32-CYD Configuration

The ESP32-CYD devices are pre-configured to receive audio broadcasts on the default multicast group (239.255.12.42) and port (3333). No additional configuration is needed on the devices.

You can adjust the volume and enable/disable the built-in audio or Bluetooth broadcasting through the touch interface on the ESP32-CYD.

## Troubleshooting

### No Audio on ESP32-CYD Devices

- Ensure the ESP32-CYD devices are connected to the same network as the broadcasting PC
- Check that multicast traffic is allowed on your network
- Verify that audio is playing and routed to the "ESP32-CYD-Broadcast" output
- Make sure the ESP32-CYD built-in audio is enabled in the device settings

### Poor Audio Quality

- Try reducing the system load on your ESP32-CYD devices
- Ensure good WiFi signal strength for all devices
- Consider using a wired network connection for the broadcasting PC

### Bluetooth Audio Issues

- Try re-scanning for Bluetooth devices on the ESP32-CYD
- Make sure the Bluetooth device is in pairing mode
- Check the battery level of your Bluetooth devices
- Keep Bluetooth devices within reasonable range of the ESP32-CYD

## Advanced Configuration

The default settings should work for most situations, but you can modify the script to change:

- Audio format and quality (change the `AUDIO_FORMAT` variable)
- Multicast address and port (change `MULTICAST_ADDR` and `PORT` variables)
- Buffer sizes for smoother audio (add buffer settings to the GStreamer pipeline)

If you change these settings, you'll need to update the corresponding values in the ESP32-CYD firmware as well.