# Building ContestLogX

## Prerequisites

### Ubuntu/Debian
```bash
sudo apt-get update -qq
sudo apt-get install -y qt6-base-dev qt6-base-dev-tools qt6-tools-dev libqt6network6 qt6-serialport-dev qt6-multimedia-dev cmake build-essential libxml2-dev
```

### Other Linux distributions
Install equivalent packages for:
- C++ compiler (g++ or clang++)
- CMake (3.16 or higher)
- Qt6 development libraries (Core, Widgets, Network)

## Build Steps

1. **Create build directory:**
   ```bash
   mkdir build
   cd build
   ```

2. **Configure with CMake:**
   ```bash
   cmake ..
   ```

3. **Build:**
   ```bash
   make -j$(nproc)
   ```

4. **Run:**
   ```bash
   ./ContestLogX
   ```

## Notes

- The build directory is git-ignored and should not be committed
- All build artifacts are generated in the `build/` directory
- To clean and rebuild: `rm -rf build && mkdir build && cd build && cmake .. && make`
- The application looks for contest definitions in `../contests/` relative to the source directory
- Debug logs are written to `clx_debug.log` in the current working directory

## Directory Structure After Build

```
ContestLogX/
├── build/              (git-ignored, created during build)
│   ├── ContestLogX     (executable)
│   ├── CMakeFiles/
│   └── ...
├── contests/           (contest JSON definitions, read-only)
├── data/               (static bundled files like default_layout.json, read-only)
└── src files...
```

## Runtime Data Directories

**User Data** (writable, per-user):
- Linux: `~/.local/share/ContestLogX/`
- macOS: `~/Library/Application Support/ContestLogX/`
- Windows: `%APPDATA%\ContestLogX\`

Contains:
- `cty.dat` - DXCC database (download via File menu)
- `master.scp` - Super Check Partial database (download via File menu)
- `history.json` - Call history records (auto-generated)

**Configuration** (writable, per-user):
- Linux: `~/.config/ContestLogX/`
- macOS: `~/Library/Preferences/ContestLogX/`
- Windows: `%APPDATA%\ContestLogX\`

Contains:
- `ContestLogX.json` - Application settings
