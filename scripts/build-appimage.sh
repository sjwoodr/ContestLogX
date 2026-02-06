#!/bin/bash
set -e

cd /src

# Configure
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTS=OFF

# Build
cmake --build build -j"$(nproc)"

# Install into AppDir
DESTDIR=/src/AppDir cmake --install build

# Run linuxdeploy to bundle dependencies and create AppImage
# --appimage-extract-and-run avoids needing FUSE inside Docker
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=/usr/bin/qmake6
export OUTPUT=ContestLogX-x86_64.AppImage

linuxdeploy \
    --appdir AppDir \
    --plugin qt \
    --output appimage \
    --desktop-file AppDir/usr/share/applications/ContestLogX.desktop \
    --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/contestlogx.png

# Copy result to mounted output directory
cp ContestLogX-*.AppImage /output/
echo ""
echo "AppImage created: /output/ContestLogX-x86_64.AppImage"
