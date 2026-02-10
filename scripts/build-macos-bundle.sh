#!/bin/bash
set -e

echo "Building macOS app bundle..."

APP_NAME="ContestLogX"
VERSION=$(grep 'project(ContestLogX VERSION' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')
JOBS=${JOBS:-$(sysctl -n hw.ncpu)}

mkdir -p build dist

# Configure and build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF ..
cmake --build . -j"${JOBS}"
cd ..

# Create app bundle structure
mkdir -p "dist/${APP_NAME}.app/Contents/MacOS"
mkdir -p "dist/${APP_NAME}.app/Contents/Resources"
mkdir -p "dist/${APP_NAME}.app/Contents/Resources/share/contestlogx/contests"
mkdir -p "dist/${APP_NAME}.app/Contents/Resources/share/contestlogx/data"

# Copy executable
cp "build/${APP_NAME}" "dist/${APP_NAME}.app/Contents/MacOS/${APP_NAME}"
chmod +x "dist/${APP_NAME}.app/Contents/MacOS/${APP_NAME}"

# Copy icon
cp resources/contestlogx.png "dist/${APP_NAME}.app/Contents/Resources/"

# Copy contests
cp contests/*.json "dist/${APP_NAME}.app/Contents/Resources/share/contestlogx/contests/"

# Copy data files
cp data/*.json "dist/${APP_NAME}.app/Contents/Resources/share/contestlogx/data/"

# Create PkgInfo file
echo "APPL????" > "dist/${APP_NAME}.app/Contents/PkgInfo"

# Create Info.plist with version
cat > "dist/${APP_NAME}.app/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>ContestLogX</string>
    <key>CFBundleIdentifier</key>
    <string>com.contestlogx.app</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>ContestLogX</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
EOF

echo ""
echo "✓ macOS app bundle created: dist/${APP_NAME}.app"
echo "  Version: ${VERSION}"
echo "  Bundled contest definitions and data files"
