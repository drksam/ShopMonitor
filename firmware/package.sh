#!/bin/bash
# Package the firmware for distribution

# Create temporary directories
TEMP_DIR_ESP32=$(mktemp -d)
TEMP_DIR_CYD=$(mktemp -d)
ESP32_PACKAGE="esp32_shop_firmware"
CYD_PACKAGE="esp32_cyd_audio_firmware"

echo "Building ESP32 Standard Firmware..."
# Build ESP32 standard firmware
cd esp32
platformio run
cp .pio/build/esp32dev/firmware.bin $TEMP_DIR_ESP32/
cd ..

echo "Building ESP32-CYD Audio Firmware..."
# Build ESP32-CYD firmware with audio
cd esp32-cyd
platformio run
cp .pio/build/esp32-cyd/firmware.bin $TEMP_DIR_CYD/
cd ..

echo "Packaging ESP32 Standard Firmware..."
# Copy all necessary files for ESP32
cp -r include lib esp32/platformio.ini README.md $TEMP_DIR_ESP32/
cp -r esp32/src $TEMP_DIR_ESP32/

echo "Packaging ESP32-CYD Audio Firmware..."
# Copy all necessary files for ESP32-CYD
cp -r include lib esp32-cyd/platformio.ini README.md $TEMP_DIR_CYD/
cp -r esp32-cyd/src $TEMP_DIR_CYD/

# Create zip files
cd $TEMP_DIR_ESP32
zip -r ${ESP32_PACKAGE}.zip ./*
cd $TEMP_DIR_CYD
zip -r ${CYD_PACKAGE}.zip ./*

# Move zips to original directory
mv ${TEMP_DIR_ESP32}/${ESP32_PACKAGE}.zip $OLDPWD/
mv ${TEMP_DIR_CYD}/${CYD_PACKAGE}.zip $OLDPWD/

# Clean up
rm -rf $TEMP_DIR_ESP32
rm -rf $TEMP_DIR_CYD

echo "Packages created:"
echo "- ${ESP32_PACKAGE}.zip (Standard ESP32)"
echo "- ${CYD_PACKAGE}.zip (ESP32-CYD with Audio)"