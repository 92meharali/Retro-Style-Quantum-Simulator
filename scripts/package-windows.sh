#!/usr/bin/env bash
# Cross-compile a portable Windows x64 zip from Linux (MinGW-w64).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${1:-0.1.0}"
BUILD="$ROOT/build-win"
NAME="QuantumCircuitLab-${VERSION}-win64"
DIST="$ROOT/dist/$NAME"
ARCHIVE="$ROOT/dist/${NAME}.zip"

if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    echo "ERROR: Install MinGW-w64 cross compiler:"
    echo "  sudo apt install mingw-w64 zip"
    exit 1
fi

echo "==> Configuring Windows cross-build..."
cmake -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DQSIM_BUILD_TESTS=OFF \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64-x86_64.cmake"

echo "==> Building quantum-lab.exe..."
cmake --build "$BUILD" --target quantum-lab -j"$(nproc)"

BIN="$BUILD/ui/quantum-lab.exe"
if [[ ! -f "$BIN" ]]; then
    echo "ERROR: $BIN not found"
    exit 1
fi

if command -v x86_64-w64-mingw32-strip >/dev/null 2>&1; then
    echo "==> Stripping debug symbols..."
    x86_64-w64-mingw32-strip --strip-unneeded "$BIN"
fi

echo "==> Assembling portable bundle..."
rm -rf "$DIST"
mkdir -p "$DIST/presets"
cp "$BIN" "$DIST/quantum-lab.exe"
cp -r "$ROOT/presets/." "$DIST/presets/"
cp "$ROOT/packaging/run-quantum-lab.bat" "$DIST/run.bat"
cp "$ROOT/packaging/README-portable-windows.txt" "$DIST/README.txt"

mkdir -p "$ROOT/dist"
    rm -f "$ARCHIVE"
    (cd "$ROOT/dist" && zip -rq "${NAME}.zip" "$NAME")

BYTES="$(wc -c < "$ARCHIVE" | tr -d ' ')"
echo ""
echo "Done."
echo "  Folder:  $DIST"
echo "  Archive: $ARCHIVE ($(numfmt --to=iec "$BYTES" 2>/dev/null || echo "${BYTES} bytes"))"
echo ""
echo "Share the .zip file. Recipients extract and double-click run.bat"
