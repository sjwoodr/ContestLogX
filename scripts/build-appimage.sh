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
# Note: we do NOT pass --output appimage here; we bundle GStreamer and
# rewrite AppRun before packaging with appimagetool below.
export APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=/usr/bin/qmake6
export ARCH=x86_64

linuxdeploy \
    --appdir AppDir \
    --plugin qt \
    --desktop-file AppDir/usr/share/applications/ContestLogX.desktop \
    --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/contestlogx.png

# Bundle a minimal GStreamer plugin set sufficient for Qt6 Multimedia's
# audio capture pipeline (pulsesrc|alsasrc → audioconvert → audioresample
# → appsink) plus the plugin scanner helper. Self-contained bundle so we
# don't depend on the host's GStreamer stack.
#
# Why not bundle every plugin: many unrelated plugins (camerabin, rtp,
# rtsp, soup, flac, vpx, gdkpixbuf, pango, etc.) link against second-level
# libs we don't bundle (libgstbasecamerabinsrc, libgstcontroller, libgstrtp,
# libFLAC.so.8, libvpx.so.7, …). At runtime those dlopen calls fall back
# to the host's copies, which on modern distros are compiled against glib
# 2.80+ and blow up with "undefined symbol: g_once_init_leave_pointer"
# during gst-plugin-scanner probing. None of these plugins are used for
# audio capture, so the cleanest fix is to simply not bundle them.
echo ""
echo "Bundling minimal GStreamer plugin set..."
mkdir -p AppDir/usr/lib/gstreamer-1.0
GST_PLUGINS_NEEDED=(
    libgstcoreelements.so     # queue, tee, identity, fakesink, valve
    libgstapp.so              # appsrc, appsink — Qt Multimedia data sink
    libgstaudioconvert.so     # sample format conversion
    libgstaudioresample.so    # sample rate conversion
    libgstaudiorate.so        # audio buffer timing
    libgstvolume.so           # software volume control
    libgstpulseaudio.so       # pulsesrc / pulsesink — PulseAudio/PipeWire
    libgstalsa.so             # alsasrc / alsasink — ALSA fallback
    libgstautodetect.so       # autoaudiosrc / autoaudiosink — Qt uses these
)
for plugin in "${GST_PLUGINS_NEEDED[@]}"; do
    src="/usr/lib/x86_64-linux-gnu/gstreamer-1.0/$plugin"
    if [ -f "$src" ]; then
        cp -a "$src" AppDir/usr/lib/gstreamer-1.0/
        echo "  + $plugin"
    else
        echo "  ! missing: $plugin"
    fi
done

# gst-plugin-scanner lives under libexec on Debian/Ubuntu. Path varies by
# version — try both known locations.
mkdir -p AppDir/usr/libexec/gstreamer-1.0
for scanner in /usr/libexec/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner \
               /usr/lib/x86_64-linux-gnu/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner; do
    if [ -x "$scanner" ]; then
        cp "$scanner" AppDir/usr/libexec/gstreamer-1.0/
        break
    fi
done

# Override AppRun so GStreamer uses only our bundled plugin path. linuxdeploy's
# default AppRun is a symlink to the binary; we need a wrapper script that sets
# GST_PLUGIN_SYSTEM_PATH_1_0 before the Qt6 Multimedia backend initializes.
echo ""
echo "Writing AppRun wrapper..."
rm -f AppDir/AppRun
cat > AppDir/AppRun <<'APPRUN'
#!/bin/bash
HERE="$(dirname "$(readlink -f "${0}")")"
export APPDIR="${HERE}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="${HERE}/usr/plugins/platforms"
# Force GStreamer to use only the bundled plugin directory. Without this,
# libgstreamer falls back to its compiled-in default (/usr/lib/.../gstreamer-1.0
# on the host) whose plugins require a newer glib than we bundle.
export GST_PLUGIN_SYSTEM_PATH_1_0="${HERE}/usr/lib/gstreamer-1.0"
export GST_PLUGIN_PATH_1_0="${HERE}/usr/lib/gstreamer-1.0"
export GST_PLUGIN_SCANNER="${HERE}/usr/libexec/gstreamer-1.0/gst-plugin-scanner"
# Keep GStreamer's plugin registry in the per-user cache so we don't try to
# write inside the read-only AppImage mount.
export GST_REGISTRY_1_0="${HOME}/.cache/ContestLogX/gst-registry.bin"
mkdir -p "${HOME}/.cache/ContestLogX"
exec "${HERE}/usr/bin/ContestLogX" "$@"
APPRUN
chmod +x AppDir/AppRun

# Package AppDir into an AppImage using appimagetool directly so it doesn't
# re-scan deps or touch our custom AppRun.
echo ""
echo "Packaging AppImage with appimagetool..."
appimagetool --no-appstream AppDir ContestLogX-x86_64.AppImage

# Copy result to mounted output directory
cp ContestLogX-*.AppImage /output/
echo ""
echo "AppImage created: /output/ContestLogX-x86_64.AppImage"
