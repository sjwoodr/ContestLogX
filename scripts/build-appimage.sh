#!/bin/bash
set -e

cd /src

# Configure
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTS=OFF

# Build
cmake --build build -j"$(nproc)"

# Install into AppDir
DESTDIR=/src/AppDir cmake --install build

# Bundle license/attribution documents into AppDir
mkdir -p AppDir/usr/share/doc/contestlogx
cp /src/LICENSE         AppDir/usr/share/doc/contestlogx/LICENSE
cp /src/ATTRIBUTIONS.md AppDir/usr/share/doc/contestlogx/ATTRIBUTIONS.md

# Run linuxdeploy to bundle dependencies and Qt plugins into AppDir.
# --appimage-extract-and-run avoids needing FUSE inside Docker.
# Note: we do NOT pass --output appimage here; we package with appimagetool
# below so we can use the type2 runtime.
#
# Qt is the aqtinstall Qt 6.8 from the Dockerfile ($QTDIR). Its lib dir also
# holds the FFmpeg libraries Qt's multimedia plugin links against, so it goes
# on LD_LIBRARY_PATH for linuxdeploy to resolve them.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE="${QTDIR}/bin/qmake"
export LD_LIBRARY_PATH="${QTDIR}/lib:${LD_LIBRARY_PATH:-}"
export ARCH=x86_64

linuxdeploy \
    --appdir AppDir \
    --plugin qt \
    --desktop-file AppDir/usr/share/applications/ContestLogX.desktop \
    --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/contestlogx.png

# Package AppDir into an AppImage using appimagetool directly so it doesn't
# re-scan deps.
#
# --runtime-file points appimagetool at the modern type2-runtime instead of
# the fuse2-only runtime bundled inside AppImage/AppImageKit's appimagetool.
# Without this, the produced AppImage refuses to launch on Ubuntu 22.04+ /
# Debian 12+ hosts that no longer ship libfuse2 (only fuse3). The Dockerfile
# fetches this runtime as /usr/local/bin/appimage-runtime-x86_64.
echo ""
echo "Packaging AppImage with appimagetool..."
appimagetool --no-appstream \
    --runtime-file /usr/local/bin/appimage-runtime-x86_64 \
    AppDir ContestLogX-x86_64.AppImage

# Copy result to mounted output directory
cp ContestLogX-*.AppImage /output/
echo ""
echo "AppImage created: /output/ContestLogX-x86_64.AppImage"
