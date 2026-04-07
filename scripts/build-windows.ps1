#!/usr/bin/env pwsh
# Build a self-contained Windows distribution of ContestLogX.
# Expects Qt6 on PATH (provided by install-qt-action in CI).

$ErrorActionPreference = "Stop"

$AppName = "ContestLogX"
$Version = (Select-String -Path CMakeLists.txt -Pattern 'project\(ContestLogX VERSION (\d+\.\d+\.\d+)' |
    ForEach-Object { $_.Matches[0].Groups[1].Value })

Write-Host "Building $AppName $Version for Windows..."

# Build
New-Item -ItemType Directory -Force -Path build | Out-Null
Push-Location build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF ..
cmake --build . --config Release
Pop-Location

# Create distribution directory
$distDir = "dist\$AppName"
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

# Find executable (MSVC multi-config puts it in build/Release/, single-config in build/)
$exe = Get-ChildItem -Path build -Recurse -Filter "$AppName.exe" | Select-Object -First 1
if (-not $exe) {
    Write-Error "Could not find $AppName.exe in build directory"
    exit 1
}
Write-Host "Found executable: $($exe.FullName)"
Copy-Item $exe.FullName "$distDir\$AppName.exe"

# Run windeployqt to bundle Qt DLLs and plugins
windeployqt --release --no-translations --no-opengl-sw "$distDir\$AppName.exe"

# Copy contest definitions
New-Item -ItemType Directory -Force -Path "$distDir\contests" | Out-Null
Copy-Item "contests\*.json" "$distDir\contests\"

# Copy data files
New-Item -ItemType Directory -Force -Path "$distDir\data" | Out-Null
Copy-Item "data\*.json" "$distDir\data\"

# Copy icon
Copy-Item "resources\contestlogx.png" "$distDir\"

Write-Host ""
Write-Host "Windows build complete: dist\$AppName\"
Write-Host "  Version: $Version"
Write-Host "  Bundled contest definitions, data files, and Qt runtime"
