#!/bin/bash
# Flash script for ESP8266 - run this in Termux on your phone

echo "=== ESP8266 Flash Script ==="

# Install esptool if not present
if ! command -v esptool &> /dev/null; then
    echo "Installing esptool..."
    pip install --user esptool
fi

# Find ESP8266 device
echo "Looking for ESP8266..."
DEVICE=""

for dev in /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyACM0 /dev/ttyACM1; do
    if [ -e "$dev" ]; then
        DEVICE="$dev"
        break
    fi
done

if [ -z "$DEVICE" ]; then
    echo "ERROR: No ESP8266 device found!"
    echo "Try: ls -la /dev/tty* /dev/ttyACM*"
    echo ""
    echo "Make sure:"
    echo "1. ESP8266 is connected via OTG"
    echo "2. You granted USB permission in Termux (check popup)"
    exit 1
fi

echo "Found device: $DEVICE"
echo "Flashing firmware..."

# Flash the firmware
esptool.py --chip esp8266 --port $DEVICE --baud 115200 write_flash -z 0x00000 firmware.bin

if [ $? -eq 0 ]; then
    echo "SUCCESS! Firmware flashed."
    echo "Reset the ESP8266 and it should connect to WiFi."
else
    echo "ERROR: Flash failed!"
fi
