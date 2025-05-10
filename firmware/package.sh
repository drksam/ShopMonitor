#!/bin/bash
# Package the firmware for distribution

# Create a temporary directory
TEMP_DIR=$(mktemp -d)
PACKAGE_NAME="esp32_Shop_node_firmware"

# Copy all necessary files
cp -r include lib src platformio.ini README.md $TEMP_DIR/

# Create a zip file
cd $TEMP_DIR
zip -r ${PACKAGE_NAME}.zip ./*

# Move zip to original directory
mv ${PACKAGE_NAME}.zip $OLDPWD/

# Clean up
cd $OLDPWD
rm -rf $TEMP_DIR

echo "Package created: ${PACKAGE_NAME}.zip"