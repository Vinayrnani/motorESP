#!/bin/bash
set -e

CURRENT_VERSION=$(grep '#define FIRMWARE_VERSION' updates.h | cut -d'"' -f2)

if [ $# -eq 1 ]; then
    NEW_VERSION="$1"
else
    MAJOR=$(echo "$CURRENT_VERSION" | cut -d. -f1)
    MINOR=$(echo "$CURRENT_VERSION" | cut -d. -f2)
    PATCH=$(echo "$CURRENT_VERSION" | cut -d. -f3)
    NEW_VERSION="${MAJOR}.${MINOR}.$((PATCH + 1))"
fi
TAG="v$NEW_VERSION"

echo "=== Bumping version: $CURRENT_VERSION → $NEW_VERSION ==="
sed -i "s/#define FIRMWARE_VERSION \"$CURRENT_VERSION\"/#define FIRMWARE_VERSION \"$NEW_VERSION\"/" updates.h
echo "$NEW_VERSION" > version.txt

echo ""
echo "=== Compiling firmware ==="
mkdir -p build
arduino-cli compile -b esp8266:esp8266:nodemcu -j "$(nproc)" --build-path build/.cache --output-dir build motorESP.ino
cp build/motorESP.ino.bin firmware.bin

echo ""
echo "=== Committing ==="
git add updates.h version.txt firmware.bin
git commit -m "release: $TAG"
git push origin HEAD

echo ""
echo "=== Tagging $TAG ==="
git tag "$TAG"
git push origin "$TAG"

echo ""
echo "=== Creating GitHub Release ==="
gh release create "$TAG" firmware.bin \
    --title "Release $TAG" \
    --notes "Firmware version $NEW_VERSION"

echo ""
echo "=== Done: $TAG released ==="
