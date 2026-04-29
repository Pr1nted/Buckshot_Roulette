#!/bin/bash

# =============================================================
# autocompile.sh — Unified Build Script for Buckshot Roulette
#
# Usage:
#   ./autocompile.sh -all              # Build all (Mac + Linux Amd64/Arm64 + Win)
#   ./autocompile.sh -mac              # Build macOS only
#   ./autocompile.sh -linux            # Build Linux (both Amd64 & Arm64)
#   ./autocompile.sh -windows          # Build Windows (MinGW)
# =============================================================

set -e

# --- Configuration ---
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_NAME="Buckshot_Roulette"
EXE_NAME="Buckshot_roulette"
BUILD_ROOT="$SCRIPT_DIR/build"

# Flags
DO_MAC=0
DO_LINUX=0
DO_WINDOWS=0

# --- Colors ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# --- Argument Parsing ---
if [ "$#" -eq 0 ]; then
    echo -e "${YELLOW}Usage: $0 [-all] [-mac] [-linux] [-windows]${NC}"
    exit 1
fi

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -all) DO_MAC=1; DO_LINUX=1; DO_WINDOWS=1 ;;
        -mac) DO_MAC=1 ;;
        -linux) DO_LINUX=1 ;;
        -windows) DO_WINDOWS=1 ;;
        *) echo -e "${RED}Unknown option: $1${NC}"; exit 1 ;;
    esac
    shift
done

# --- Helper Functions ---
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

check_dockerfile() {
    if [ ! -f "$SCRIPT_DIR/Dockerfile.linux" ]; then
        log_error "Dockerfile.linux not found in $SCRIPT_DIR"
        exit 1
    fi
}

check_docker_inner() {
    if [ ! -f "$SCRIPT_DIR/docker_inner_build.sh" ]; then
        log_error "docker_inner_build.sh not found in $SCRIPT_DIR"
        exit 1
    fi
}

# ==========================================
# 1. macOS Build Logic
# ==========================================
build_mac() {
    log_info "Starting macOS build..."

    local OUT_DIR="$BUILD_ROOT/macos"
    local BUILD_SUBDIR="$OUT_DIR/build_mac"
    local OUTPUT_APP="$OUT_DIR/$APP_NAME.app"

    rm -rf "$BUILD_SUBDIR" "$OUTPUT_APP"
    mkdir -p "$OUT_DIR"

    log_info "Compiling Universal Binary (arm64 + x86_64)..."
    mkdir "$BUILD_SUBDIR"
    cd "$BUILD_SUBDIR"

    cmake "$SCRIPT_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
        -DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=OFF \
        -DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=ON

    if ! make -j$(sysctl -n hw.ncpu); then
        log_error "macOS compilation failed."
        exit 1
    fi
    cd "$SCRIPT_DIR"

    log_info "Creating .app Bundle..."
    mkdir -p "$OUTPUT_APP/Contents/MacOS"
    mkdir -p "$OUTPUT_APP/Contents/Resources"

    cp "$BUILD_SUBDIR/$EXE_NAME" "$OUTPUT_APP/Contents/Resources/"
    chmod +x "$OUTPUT_APP/Contents/Resources/$EXE_NAME"

    # === ICON SETUP ===
    if [ -f "$SCRIPT_DIR/icon.icns" ]; then
        cp "$SCRIPT_DIR/icon.icns" "$OUTPUT_APP/Contents/Resources/"
        log_info "Icon copied to .app bundle."
    else
        log_warn "icon.icns not found. Bundle will have default icon."
    fi
    # =================

    cat > "$OUTPUT_APP/Contents/MacOS/launcher" <<'EOF'
#!/bin/bash
RESOURCES_DIR="$(cd "$(dirname "$0")/../Resources" && pwd)"

osascript -e 'tell application "Terminal"
    activate
    do script ""
    tell application "Finder"
        set screenBounds to bounds of window of desktop
    end tell
    set screenW to item 3 of screenBounds
    set screenH to item 4 of screenBounds
    set bounds of window 1 to {0, 0, screenW, screenH}
    do script "cd \"'"$RESOURCES_DIR"'\" && ./Buckshot_roulette" in window 1
end tell'
EOF
    chmod +x "$OUTPUT_APP/Contents/MacOS/launcher"

    cat > "$OUTPUT_APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>launcher</string>
    <key>CFBundleIdentifier</key>
    <string>com.buckshot.roulette</string>
    <key>CFBundleName</key>
    <string>Buckshot Roulette</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleVersion</key>
    <string>1.0</string>
    <key>CFBundleIconFile</key>
    <string>icon</string>
</dict>
</plist>
EOF

    xattr -cr "$OUTPUT_APP"

    log_info "SUCCESS! macOS App created at: $OUTPUT_APP"
}

# ==========================================
# 2. Linux Build Logic (Multi-Arch)
# ==========================================
build_linux() {
    check_dockerfile
    check_docker_inner

    # Check Docker
    if ! command -v docker &>/dev/null; then
        log_error "Docker is not installed. Cannot build Linux version."
        exit 1
    fi
    if ! docker info &>/dev/null; then
        log_error "Docker is installed but not running."
        exit 1
    fi

    # Loop through both architectures
    for ARCH in amd64 arm64; do
        log_info "Starting Linux build for architecture: $ARCH..."

        local OUT_DIR="$BUILD_ROOT/linux/$ARCH"
        local IMAGE_NAME="buckshot-linux-builder-$ARCH"
        local OUTPUT_APPIMAGE="$OUT_DIR/${APP_NAME}-${ARCH}.AppImage"

        # Clean previous output
        rm -rf "$OUTPUT_APPIMAGE"
        mkdir -p "$OUT_DIR"

        # Determine Docker Platform Flag
        local DOCKER_PLATFORM="linux/$ARCH"

        # Build Docker Image for specific architecture
        log_info "Building Docker image ($DOCKER_PLATFORM)..."
        if ! docker build \
            --platform "$DOCKER_PLATFORM" \
            -f "$SCRIPT_DIR/Dockerfile.linux" \
            -t "$IMAGE_NAME" \
            "$SCRIPT_DIR"; then
            log_error "Docker image build failed for $ARCH."
        fi

        # Run Build
        log_info "Running build inside container ($ARCH)..."
        docker run --rm \
            -v "$SCRIPT_DIR:/src" \
            -e "OUTPUT_FILENAME=${APP_NAME}-${ARCH}.AppImage" \
            "$IMAGE_NAME"

        # Move output to organized folder
        if [ -f "$SCRIPT_DIR/${APP_NAME}-${ARCH}.AppImage" ]; then
            mv "$SCRIPT_DIR/${APP_NAME}-${ARCH}.AppImage" "$OUTPUT_APPIMAGE"
            log_info "SUCCESS! Linux AppImage ($ARCH) created at: $OUTPUT_APPIMAGE"
        else
            log_warn "AppImage not found for $ARCH build."
        fi
    done
}

# ==========================================
# 3. Windows Build Logic
# ==========================================
build_windows() {
    log_info "Starting Windows build..."

    local OUT_DIR="$BUILD_ROOT/windows"
    local BUILD_SUBDIR="$OUT_DIR/build_windows"
    local PDCURSES_DIR="$OUT_DIR/PDCurses"
    local DIST_DIR="$OUT_DIR/${APP_NAME}_Windows"
    local ZIP_OUTPUT="$OUT_DIR/${APP_NAME}_Windows.zip"
    local EXE_NAME="Buckshot_roulette.exe"

    rm -rf "$BUILD_SUBDIR" "$DIST_DIR" "$ZIP_OUTPUT"
    mkdir -p "$OUT_DIR"

    if command -v x86_64-w64-mingw32-gcc &>/dev/null; then
        MINGW_CC="x86_64-w64-mingw32-gcc"
        MINGW_PREFIX="x86_64-w64-mingw32"
    else
        log_error "MinGW cross-compiler not found."
        log_error "Install with: brew install mingw-w64 (macOS) or apt install mingw-w64 (Linux)"
        exit 1
    fi

    if ! command -v git &>/dev/null; then
        log_error "git is not installed. Required for PDCurses."
        exit 1
    fi

    MINGW_STRIP="${MINGW_PREFIX}-strip"

    if [ ! -d "$PDCURSES_DIR" ]; then
        log_info "Cloning PDCurses..."
        git clone --depth=1 https://github.com/wmcbrine/PDCurses.git "$PDCURSES_DIR"
        if [ $? -ne 0 ]; then
            log_error "Failed to clone PDCurses."
            exit 1
        fi
    fi

    log_info "Building PDCurses..."
    cd "$PDCURSES_DIR/wincon"
    make -f Makefile \
        CC="$MINGW_CC" \
        AR="${MINGW_PREFIX}-ar" \
        STRIP="${MINGW_PREFIX}-strip" \
        WIDE=Y \
        UTF8=Y \
        2>&1 | tail -10

    if [ ! -f "pdcurses.a" ]; then
        log_error "PDCurses build failed."
        exit 1
    fi
    cd "$SCRIPT_DIR"

    # === ICON SETUP (Windows) ===
    log_info "Embedding Windows Icon..."

    # Create build directory
    mkdir -p "$BUILD_SUBDIR"

    cat > "$BUILD_SUBDIR/resource.rc" << EOF
1 ICON "$SCRIPT_DIR/icon.ico"
EOF

    "${MINGW_PREFIX}-windres" "$BUILD_SUBDIR/resource.rc" -O coff -o "$BUILD_SUBDIR/resource.o"

    if [ ! -f "$BUILD_SUBDIR/resource.o" ]; then
        log_warn "Icon resource compilation failed (windres missing?). Continuing without icon."
    fi
    # ===========================

    log_info "Compiling Game..."
    SOURCES="main.c screens.c ui.c gameplay.c network.c"

    $MINGW_CC \
        "$BUILD_SUBDIR/resource.o" \
        $SOURCES \
        -I"$PDCURSES_DIR" \
        -I"." \
        -L"$PDCURSES_DIR/wincon" \
        -o "$BUILD_SUBDIR/$EXE_NAME" \
        -DPDCURSES \
        -D_WIN32_WINNT=0x0601 \
        -DWINVER=0x0601 \
        -std=c11 \
        -O2 \
        -static \
        -l:pdcurses.a \
        -lws2_32 \
        -liphlpapi \
        -lpthread \
        -lm

    if [ ! -f "$BUILD_SUBDIR/$EXE_NAME" ]; then
        log_error "Game compilation failed."
        exit 1
    fi

    "$MINGW_STRIP" "$BUILD_SUBDIR/$EXE_NAME"

    log_info "Packaging..."
    mkdir "$DIST_DIR"
    cp "$BUILD_SUBDIR/$EXE_NAME" "$DIST_DIR/"

    cat > "$DIST_DIR/Play.bat" << 'BATEOF'
@echo off
title Buckshot Roulette
set "GAME_DIR=%~dp0"
mode con cols=220 lines=50
start "" /MAX cmd /k "%GAME_DIR%Buckshot_roulette.exe"
BATEOF

    cat > "$DIST_DIR/README.txt" << 'READEOF'
Buckshot Roulette - Windows
============================

HOW TO RUN:
  Double-click "Play.bat" to launch game in a maximized terminal window.

  Alternatively, open Command Prompt or Windows Terminal, navigate to this
  folder, and run:  Buckshot_roulette.exe

REQUIREMENTS:
  - Windows 7 or later (64-bit)
  - No installation required — fully self-contained.
READEOF

    if command -v zip &>/dev/null; then
        cd "$OUT_DIR"
        zip -r "${APP_NAME}_Windows.zip" "${APP_NAME}_Windows/" -x "*.DS_Store"
        cd "$SCRIPT_DIR"
        log_info "SUCCESS! Windows Distribution created at: $ZIP_OUTPUT"
    else
        log_warn "zip command not found. Unzipped folder located at: $DIST_DIR"
    fi
}

# ==========================================
# Main Execution
# ==========================================

mkdir -p "$BUILD_ROOT"

if [ "$DO_MAC" -eq 1 ]; then
    build_mac
fi

if [ "$DO_LINUX" -eq 1 ]; then
    build_linux
fi

if [ "$DO_WINDOWS" -eq 1 ]; then
    build_windows
fi

log_info "All requested builds completed successfully."