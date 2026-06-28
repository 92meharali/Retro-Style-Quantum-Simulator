#!/usr/bin/env bash
# Build a portable Linux x86_64 tarball for distribution.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="${1:-0.1.0}"
BUILD="$ROOT/build"
NAME="QuantumCircuitLab-${VERSION}-linux-x86_64"
DIST="$ROOT/dist/$NAME"
ARCHIVE="$ROOT/dist/${NAME}.tar.gz"

echo "==> Configuring Release build..."
cmake -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DQSIM_BUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX="$DIST"

echo "==> Building quantum-lab..."
cmake --build "$BUILD" --target quantum-lab -j"$(nproc)"

BIN="$BUILD/ui/quantum-lab"
if command -v strip >/dev/null 2>&1; then
    echo "==> Stripping debug symbols..."
    strip --strip-unneeded "$BIN"
fi

echo "==> Assembling portable bundle..."
rm -rf "$DIST"
mkdir -p "$DIST/presets"
cp "$BIN" "$DIST/quantum-lab"
cp -r "$ROOT/presets/." "$DIST/presets/"
cp "$ROOT/packaging/run-quantum-lab.sh" "$DIST/run.sh"
cp "$ROOT/packaging/README-portable.txt" "$DIST/README.txt"
cp "$ROOT/packaging/quantum-circuit-lab.desktop" "$DIST/" 2>/dev/null || true
chmod +x "$DIST/quantum-lab" "$DIST/run.sh"

echo "==> Generating taskbar icon..."
"$BIN" --write-icon "$DIST/quantum-lab.png"

mkdir -p "$ROOT/dist"
rm -f "$ARCHIVE"
tar -czf "$ARCHIVE" -C "$ROOT/dist" "$NAME"

BYTES="$(wc -c < "$ARCHIVE" | tr -d ' ')"
echo ""
echo "Done."
echo "  Folder:  $DIST"
echo "  Archive: $ARCHIVE ($(numfmt --to=iec "$BYTES" 2>/dev/null || echo "${BYTES} bytes"))"
echo ""
echo "Share the .tar.gz file. Recipients run:"
echo "  tar xzf ${NAME}.tar.gz && cd $NAME && ./run.sh"
