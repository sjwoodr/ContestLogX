#!/bin/bash
set -e

echo "Building macOS app bundle..."

APP_NAME="ContestLogX"
VERSION=$(grep 'project(ContestLogX VERSION' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')
JOBS=${JOBS:-$(sysctl -n hw.ncpu)}

mkdir -p build dist

# Configure and build as a universal binary (x86_64 + arm64) so the bundle
# runs natively on both Intel and Apple Silicon Macs. Qt 6.2+ ships
# universal frameworks by default so no separate Qt install is needed -
# clang cross-compiles the x86_64 slice even on an Apple Silicon runner.
# Deployment target is Big Sur (macOS 11.0), the oldest macOS with Apple
# Silicon support and the effective floor for Qt 6.5.
cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
      -DBUILD_TESTS=OFF ..
cmake --build . -j"${JOBS}"
cd ..

# Sanity-check that the binary actually contains both architectures. lipo
# prints "Architectures in the fat file: ... are: x86_64 arm64" on success;
# CI logs make this easy to spot.
echo ""
echo "Architectures in built binary:"
lipo -info "build/${APP_NAME}"

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

# Create Info.plist with version. NOTE: must be in place before macdeployqt
# runs because macdeployqt reads CFBundleExecutable to locate the binary it
# should introspect.
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
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <!-- NSMicrophoneUsageDescription is REQUIRED for the CW Decoder to
         receive any audio from a capture device. macOS TCC silently hands
         back a zero-filled stream if this key is missing - symptom is
         "CW Decoder produces no output at all, not even garbage." The
         string below is shown to the user in the mic-access prompt. -->
    <key>NSMicrophoneUsageDescription</key>
    <string>ContestLogX decodes Morse code from your radio's receive audio. Select an input device in Rig Connection Settings to enable the CW Decoder.</string>
</dict>
</plist>
EOF

# Bundle Qt frameworks, plugins, and rewrite rpaths so the .app is
# self-contained. Without this the binary's @rpath references point at the
# build-time Qt install (e.g. the CI runner's Qt path) and the app won't
# launch on any machine that doesn't happen to have Qt installed at the
# same location. macdeployqt introspects the binary's link dependencies
# and copies the required Qt frameworks (QtCore, QtGui, QtWidgets,
# QtNetwork, QtXml, QtMultimedia, etc.) into Contents/Frameworks/ along
# with their plugins in Contents/PlugIns/.
echo ""
echo "Running macdeployqt to bundle Qt frameworks..."
if ! command -v macdeployqt >/dev/null 2>&1; then
    echo "ERROR: macdeployqt not found on PATH."
    echo "       On CI this is provided by jurplel/install-qt-action."
    echo "       Locally, ensure \$(brew --prefix qt6)/bin is on PATH."
    exit 1
fi
macdeployqt "dist/${APP_NAME}.app" -always-overwrite

# Re-sign the bundle with an ad-hoc signature that seals all bundled
# resources. The linker's original ad-hoc signature on the main binary is
# invalidated the moment macdeployqt rewrites rpaths inside the frameworks
# and plugins - Gatekeeper then rejects the bundle with "code has no
# resources but signature indicates they must be present." The --deep flag
# walks into Contents/Frameworks and Contents/PlugIns so each nested
# bundle gets its own valid signature. The --force flag replaces the
# invalidated signatures instead of failing on conflict.
#
# "--sign -" means ad-hoc (no signing identity, no notarization). Users
# downloading from GitHub will still trip Gatekeeper's quarantine check
# (com.apple.quarantine xattr set on download) - they'll need to right-
# click → Open the first time, or run
#   xattr -dr com.apple.quarantine /Applications/ContestLogX.app
# to remove the attribute. Full notarization requires an Apple Developer
# account and is out of scope for this script.
echo ""
echo "Re-signing bundle (ad-hoc) to seal macdeployqt's rewrites..."
codesign --force --deep --sign - "dist/${APP_NAME}.app"

# Verify the signature is structurally valid. This doesn't check trust
# (ad-hoc signatures aren't trusted by default) but it catches issues like
# missing Sealed Resources which is what originally broke the 0.7.22 bundle.
codesign --verify --verbose "dist/${APP_NAME}.app"

echo ""
echo "✓ macOS app bundle created: dist/${APP_NAME}.app"
echo "  Version: ${VERSION}"
echo "  Bundled contest definitions, data files, and Qt frameworks"
