#!/bin/bash
echo "Looking for ESP8266 device..."

# Try to find the device
for dev in /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyUSB2 /dev/ttyACM0 /dev/ttyACM1; do
    if [ -e "$dev" ]; then
        echo "Found device: $dev"
        echo "Flashing firmware..."
        esptool.py --chip esp8266 --port "$dev" --baud 115200 write_flash -z 0x00000 firmware.bin
        echo "Done!"
        exit 0
    fi
done

echo "No ESP8266 device found. Try:"
echo "  ls -la /dev/tty* /dev/ttyUSB* /dev/ttyACM*"
echo ""
echo "If using Termux OTG, make sure to grant USB permission when prompted"
