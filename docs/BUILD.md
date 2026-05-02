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
     python -m aqt install-qt windows desktop 6.5.3 win64_msvc2019_64 -m qtmultimedia qtnetworkauth -O C:\Qt
     ```

3. **Git** — https://git-scm.com/download/win

### Build & run

Open **"x64 Native Tools Command Prompt for VS 2022"** from the Start menu — *not* a regular PowerShell window, *not* "Developer Command Prompt." The "x64 Native Tools" shortcut runs `vcvarsall.bat x64` for you, putting MSVC's `cl.exe` and the matching linker first on PATH. Without it CMake can pick up the wrong toolchain (e.g. MinGW from Strawberry Perl, Git for Windows' bash, or anything else with a `g++` on PATH) and produce confusing `undefined reference to '__imp__ZNxxx'` link errors against Qt-MSVC binaries — see the gotcha below.

Then in that cmd prompt:

```cmd
:: Sanity check — should print MSVC's compiler, NOT MinGW or anything from
::   C:\Strawberry, C:\msys64, C:\Program Files\Git\..., etc.
where cl

:: Add Qt's bin to PATH so windeployqt + the Qt runtime DLLs resolve.
:: Replace the path if you installed Qt somewhere other than C:\Qt\6.5.3\.
set PATH=C:\Qt\6.5.3\msvc2019_64\bin;%PATH%

:: Tell CMake where to find Qt's CMake config files. Without this the
:: configure step fails with "Could not find a package configuration file
:: provided by Qt6" / "Qt6Config.cmake or qt6-config.cmake".
:: Path should be the Qt install root (containing bin\, lib\, include\),
:: NOT the bin\ subdir.
set CMAKE_PREFIX_PATH=C:\Qt\6.5.3\msvc2019_64

cd path\to\ContestLogX
git pull
powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1
```

The `-ExecutionPolicy Bypass` flag on the `powershell` invocation handles the "*cannot be loaded because running scripts is disabled on this system*" error from PowerShell's default policy — it only applies to that single invocation, no global / persistent change. (If you'd rather flip the policy permanently for your user: from any PowerShell window run `Set-ExecutionPolicy -Scope CurrentUser -ExecutionPolicy RemoteSigned`.)

`set` in cmd applies for the current window only, so closing the prompt resets everything.

CI doesn't need `CMAKE_PREFIX_PATH` set explicitly because `jurplel/install-qt-action` (the GitHub Action wrapping `aqtinstall`) sets `Qt6_DIR` and `CMAKE_PREFIX_PATH` automatically. Local Qt installs don't.

Result lands in `dist\ContestLogX\`. Just double-click `ContestLogX.exe` to run; no install needed for testing iteration. The `dist\` tree contains the executable plus all the Qt DLLs and plugins `windeployqt` bundled — exactly what the CI installer would deploy.

The `iscc.exe scripts\installer.iss` step that CI runs to wrap `dist\` into the Inno Setup `.exe` installer is **not** needed for local testing.

### Iteration loop after first setup

In the same x64 Native Tools Command Prompt (env vars persist for the window):

```cmd
git pull
rmdir /s /q build dist 2>nul     :: only when you've changed Qt or generator settings
powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1
.\dist\ContestLogX\ContestLogX.exe
```

For routine source-only changes you don't need to nuke `build\` — `cmake --build build --config Release` from inside the build dir will rebuild incrementally. The full `build-windows.ps1` re-runs `windeployqt` to refresh the bundled Qt DLLs, which you only need when you switched Qt versions or modified resources.

### Gotcha: MinGW / Strawberry Perl shadowing MSVC

If the link step fails with errors like:

```
C:\Strawberry\c\bin\...\ld.exe: undefined reference to `__imp__ZN5QFontD1Ev`
collect2.exe: error: ld returned 1 exit status
ninja: build stopped: subcommand failed.
```

…you're not actually in the MSVC environment — CMake picked up `g++` and `ld` from MinGW (typically Strawberry Perl, MSYS2, or Git for Windows) instead of MSVC's `cl.exe`. MinGW can't link against MSVC-built Qt DLLs because of name mangling and calling-convention differences. Fix:

1. Close the current prompt.
2. Open **x64 Native Tools Command Prompt for VS 2022** specifically.
3. `where cl` — should print `C:\Program Files\Microsoft Visual Studio\…\cl.exe`. If it doesn't, you're still in the wrong environment.
4. Nuke the stale build dir (CMake caches the discovered compiler in `build\CMakeCache.txt`, so even a fresh PATH won't help unless you delete it):
   ```cmd
   rmdir /s /q build dist 2>nul
   ```
5. Re-run with the env vars set as above.

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
