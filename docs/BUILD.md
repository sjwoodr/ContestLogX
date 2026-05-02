# Building ContestLogX

ContestLogX builds on Linux, macOS, and Windows. CI builds against Qt 6.5.* via [aqtinstall](https://github.com/miurahr/aqtinstall) (the same tool [`jurplel/install-qt-action`](https://github.com/jurplel/install-qt-action) wraps in `.github/workflows/ci.yml`); local builds work against any Qt 6.4+ install.

## Linux (Ubuntu / Debian)

### Prerequisites
```bash
sudo apt-get update -qq
sudo apt-get install -y \
    qt6-base-dev qt6-base-dev-tools qt6-tools-dev \
    libqt6network6 qt6-serialport-dev qt6-multimedia-dev \
    cmake build-essential libxml2-dev
```

### Build & run
```bash
make              # configures + builds (drives cmake + ninja under the hood)
./clx             # run
make test         # unit tests
make test-logs    # contest log validation
```

The repo's top-level `Makefile` is just a convenience wrapper around CMake; if you prefer the raw CMake flow:

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./ContestLogX
```

## macOS

### Prerequisites
```bash
brew install qt6 cmake
export CMAKE_PREFIX_PATH=$(brew --prefix qt6)   # only if Qt6 isn't auto-detected
```

### Build & run
Same as Linux:
```bash
make
./clx
```

To produce a `.app` bundle that mirrors the CI release:
```bash
chmod +x scripts/build-macos-bundle.sh
scripts/build-macos-bundle.sh
# → dist/ContestLogX.app
```

The bundle is universal (x86_64 + arm64), ad-hoc signed, and includes `Info.plist` with `NSMicrophoneUsageDescription` so the CW Decoder triggers a TCC permission prompt instead of silently receiving zeros.

## Windows

### Prerequisites

1. **Visual Studio 2022 (Community is fine)** — install from https://visualstudio.microsoft.com/downloads/. In the installer pick the workload **"Desktop development with C++"**. That pulls in the MSVC compiler, Windows SDK, CMake, and the developer command prompts. The smaller **Build Tools for Visual Studio** package also works if you don't want the IDE.

2. **Qt 6.5+ for Windows / MSVC 2019 64-bit**. Two options:

   - **Qt Online Installer** (GUI, requires a free Qt account):
     https://www.qt.io/download-qt-installer → Custom installation → under "Qt 6.5.x" check **MSVC 2019 64-bit**, **Qt Multimedia**, and **Qt Network Authorization**. Default install path `C:\Qt\` is fine.

   - **aqtinstall** (CLI, no Qt account, matches CI exactly):
     ```powershell
     pip install aqtinstall
     aqt install-qt windows desktop 6.5.3 win64_msvc2019_64 -m qtmultimedia qtnetworkauth -O C:\Qt
     ```

3. **Git** — https://git-scm.com/download/win

### Build & run

Open the **"x64 Native Tools Command Prompt for VS 2022"** from the Start menu (this sets up the MSVC environment so `cl.exe`, `cmake`, and `windeployqt` resolve correctly), then:

```cmd
:: Add Qt's bin to PATH for this session — replace path with where you installed
set PATH=C:\Qt\6.5.3\msvc2019_64\bin;%PATH%

cd path\to\ContestLogX
git pull
powershell -File scripts\build-windows.ps1
```

Result lands in `dist\ContestLogX\`. Just double-click `ContestLogX.exe` to run; no install needed for testing iteration. The `dist\` tree contains the executable plus all the Qt DLLs and plugins `windeployqt` bundled — exactly what the CI installer would deploy.

The `iscc.exe scripts\installer.iss` step that CI runs to wrap `dist\` into the Inno Setup `.exe` installer is **not** needed for local testing.

### Iteration loop after first setup

```cmd
git pull
powershell -File scripts\build-windows.ps1
.\dist\ContestLogX\ContestLogX.exe
```

Or just `cmake --build build --config Release` from inside the build dir if the Qt DLLs are already bundled in `dist\` and you only changed source.

## CMake-only flow (any platform)

If you prefer pure CMake without the wrapper scripts:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release -j
```

To skip the unit tests (useful for distribution builds):
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF ..
```

## Notes

- The build directory is git-ignored and should not be committed.
- To clean and rebuild: `rm -rf build && make` (or `Remove-Item -Recurse build` on Windows).
- The application looks for contest definitions in `../contests/` relative to the source directory in dev builds; in installed/bundled builds it reads from the platform-specific install dir.

## Runtime Data Directories

**User Data** (writable, per-user, via `QStandardPaths::AppLocalDataLocation`):
- Linux: `~/.local/share/ContestLogX/`
- macOS: `~/Library/Application Support/ContestLogX/`
- Windows: `C:\Users\<user>\AppData\Local\ContestLogX\`

Contains:
- `cty.dat` — DXCC database (download via File menu)
- `master.scp` — Super Check Partial database (download via File menu)
- `history.json` — Call history records (auto-generated)
- `clx_debug.log` — debug log (since 0.7.30; previously was a relative path that broke on Windows when CLX ran from `C:\Program Files\…`)

**Configuration** (writable, per-user, via `QSettings::IniFormat / UserScope`):
- Linux: `~/.config/ContestLogX/`
- macOS: `~/Library/Preferences/ContestLogX/`
- Windows: `C:\Users\<user>\AppData\Roaming\ContestLogX\`

Contains:
- `ContestLogX.json` — application settings (rig backend, panels, decoder configuration, etc.)
