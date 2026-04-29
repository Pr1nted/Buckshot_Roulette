#!/bin/bash

# =============================================================
# docker_inner_build.sh
# Runs INSIDE the Ubuntu Docker container.
# Assembles AppImage manually using mksquashfs + AppImage runtime,
# avoiding any need to execute AppImage binaries (which fail in Docker).
# =============================================================

set -e

# === FIX: Detect Output Filename from Environment ===
# If autocompile.sh passes OUTPUT_FILENAME, use it. Otherwise default.
OUTPUT="${OUTPUT_FILENAME:-Buckshot_Roulette-x86_64.AppImage}"
# ==================================================

APP_NAME="Buckshot_Roulette"
EXE_NAME="Buckshot_roulette"
BUILD_DIR="build_linux"
APPDIR="$APP_NAME.AppDir"

echo "[Docker] Cleaning old build artifacts..."
rm -rf "$BUILD_DIR" "$APPDIR" "$OUTPUT"

# --- Compile ---
echo "[Docker] Compiling..."
mkdir "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

if [ ! -f "$EXE_NAME" ]; then
    echo "[Docker] ERROR: Compilation failed."
    exit 1
fi
cd ..
echo "[Docker] Compilation successful."

# --- Build AppDir ---
echo "[Docker] Building AppDir..."
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"

cp "$BUILD_DIR/$EXE_NAME" "$APPDIR/usr/bin/$EXE_NAME"
chmod +x "$APPDIR/usr/bin/$EXE_NAME"

# Bundle ncurses libs for portability
for LIB in libncurses.so.6 libtinfo.so.6; do
    LIB_PATH=$(ldconfig -p | grep "$LIB " | awk '{print $NF}' | head -1)
    if [ -n "$LIB_PATH" ]; then
        cp "$LIB_PATH" "$APPDIR/usr/lib/"
        echo "[Docker] Bundled: $LIB_PATH"
    fi
done

# --- AppRun ---
cat > "$APPDIR/AppRun" << 'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$LD_LIBRARY_PATH"
EXE="$HERE/usr/bin/Buckshot_roulette"

launch_in_terminal() {
    if command -v gnome-terminal &>/dev/null; then
        gnome-terminal --full-screen -- bash -c "$EXE; exec bash"
    elif command -v konsole &>/dev/null; then
        konsole --fullscreen -e bash -c "$EXE; exec bash"
    elif command -v xfce4-terminal &>/dev/null; then
        xfce4-terminal --fullscreen -e "bash -c '$EXE; exec bash'"
    elif command -v xterm &>/dev/null; then
        xterm -maximized -e bash -c "$EXE; exec bash"
    elif command -v alacritty &>/dev/null; then
        alacritty -e bash -c "$EXE; exec bash"
    elif command -v kitty &>/dev/null; then
        kitty bash -c "$EXE; exec bash"
    else
        echo "ERROR: No supported terminal emulator found."
        echo "Install one of: gnome-terminal, konsole, xfce4-terminal, xterm, alacritty, kitty"
        exit 1
    fi
}

if [ -t 1 ]; then
    exec "$EXE"
else
    launch_in_terminal
fi
EOF
chmod +x "$APPDIR/AppRun"

# --- .desktop file ---
cat > "$APPDIR/$APP_NAME.desktop" << EOF
[Desktop Entry]
Name=Buckshot Roulette
Exec=Buckshot_roulette
Icon=$APP_NAME
Type=Application
Categories=Game;
EOF

# --- Icon (256x256 or fallback) ---
# We look for /src/icon.png because the project root is mounted there
if [ -f "/src/icon.png" ]; then
    echo "[Docker] Using custom icon.png"
    cp "/src/icon.png" "$APPDIR/$APP_NAME.png"
else
    echo "[Docker] WARNING: icon.png not found in /src. Using fallback."
    # --- Minimal 1x1 PNG icon (fallback) ---
    printf '\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90wS\xde\x00\x00\x00\x0cIDATx\x9cc\xf8\x0f\x00\x00\x01\x01\x00\x05\x18\xd8N\x00\x00\x00\x00IEND\xaeB`\x82' \
        > "$APPDIR/$APP_NAME.png"
fi

# --- Manually assemble AppImage ---
# An AppImage is: [runtime binary] + [squashfs filesystem]
# We build it without needing to *run* any AppImage binary.
echo "[Docker] Creating squashfs..."
mksquashfs "$APPDIR" "$APP_NAME.squashfs" \
    -root-owned \
    -noappend \
    -comp gzip \
    -no-progress \
    2>&1 | grep -v "^$"  # suppress blank lines

echo "[Docker] Assembling AppImage..."
# Concatenate: runtime header + squashfs blob = valid AppImage
cat /opt/appimage-runtime "$APP_NAME.squashfs" > "$OUTPUT"
chmod +x "$OUTPUT"

# Embed the magic bytes that mark this as an AppImage type 2
# Offset 8 in the ELF, bytes: 0x41 0x49 0x02 (ASCII "AI" + version 2)
printf '\x41\x49\x02' | dd of="$OUTPUT" bs=1 seek=8 conv=notrunc 2>/dev/null

rm "$APP_NAME.squashfs"

if [ ! -f "$OUTPUT" ]; then
    echo "[Docker] ERROR: AppImage assembly failed."
    exit 1
fi

echo "[Docker] Done: $OUTPUT ($(du -sh "$OUTPUT" | cut -f1))"