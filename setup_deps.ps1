# Parsec-Lite Dependencies Setup Script
# This script downloads and extracts FFmpeg and ViGEmClient SDK into the 'deps' folder.

$DEPS_DIR = "deps"
$FFMPEG_URL = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-win64-gpl-shared.zip"
$VIGEM_URL = "https://github.com/ViGEm/ViGEmClient/releases/download/v1.16.16/ViGEmClient_SDK.zip"
$LIBSODIUM_URL = "https://download.libsodium.org/libsodium/releases/libsodium-1.0.18-msvc.zip"

if (-not (Test-Path $DEPS_DIR)) {
    New-Item -ItemType Directory -Path $DEPS_DIR
}

# --- FFmpeg ---
echo "[1/2] Downloading FFmpeg..."
$FFMPEG_ZIP = "$DEPS_DIR\ffmpeg.zip"
Invoke-WebRequest -Uri $FFMPEG_URL -OutFile $FFMPEG_ZIP

echo "Extracting FFmpeg..."
Expand-Archive -Path $FFMPEG_ZIP -DestinationPath "$DEPS_DIR\ffmpeg_temp"
$FFMPEG_EXTRACTED = Get-ChildItem -Path "$DEPS_DIR\ffmpeg_temp" -Directory | Select-Object -First 1

if (Test-Path "$DEPS_DIR\ffmpeg") { Remove-Item -Recurse -Force "$DEPS_DIR\ffmpeg" }
Move-Item -Path $FFMPEG_EXTRACTED.FullName -Destination "$DEPS_DIR\ffmpeg"

Remove-Item -Recurse -Force "$DEPS_DIR\ffmpeg_temp"
Remove-Item -Force $FFMPEG_ZIP

# --- ViGEmClient ---
echo "[2/2] Downloading ViGEmClient SDK..."
$VIGEM_ZIP = "$DEPS_DIR\vigem.zip"
Invoke-WebRequest -Uri $VIGEM_URL -OutFile $VIGEM_ZIP

echo "Extracting ViGEmClient..."
if (Test-Path "$DEPS_DIR\ViGEmClient") { Remove-Item -Recurse -Force "$DEPS_DIR\ViGEmClient" }
New-Item -ItemType Directory -Path "$DEPS_DIR\ViGEmClient"
Expand-Archive -Path $VIGEM_ZIP -DestinationPath "$DEPS_DIR\ViGEmClient"

Remove-Item -Force $VIGEM_ZIP

# --- libsodium ---
echo "[3/3] Downloading libsodium..."
$LIBSODIUM_ZIP = "$DEPS_DIR\libsodium.zip"
Invoke-WebRequest -Uri $LIBSODIUM_URL -OutFile $LIBSODIUM_ZIP

echo "Extracting libsodium..."
if (Test-Path "$DEPS_DIR\libsodium") { Remove-Item -Recurse -Force "$DEPS_DIR\libsodium" }
Expand-Archive -Path $LIBSODIUM_ZIP -DestinationPath "$DEPS_DIR\libsodium_temp"
$LIBSODIUM_EXTRACTED = Get-ChildItem -Path "$DEPS_DIR\libsodium_temp" -Directory | Select-Object -First 1
Move-Item -Path $LIBSODIUM_EXTRACTED.FullName -Destination "$DEPS_DIR\libsodium"
Remove-Item -Recurse -Force "$DEPS_DIR\libsodium_temp"
Remove-Item -Force $LIBSODIUM_ZIP

echo ""
echo "Dependencies setup complete!"
echo "FFmpeg: $DEPS_DIR\ffmpeg"
echo "ViGEmClient: $DEPS_DIR\ViGEmClient"
