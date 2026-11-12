#!/usr/bin/env bash
# =============================================================================
# Build a single-file, self-contained emotiv_lsl binary (macOS / Linux).
# =============================================================================
# Configures + builds the project with liblsl, hidapi, and xdfwriter linked
# statically into the emotiv_lsl executable. On macOS, LSL_FRAMEWORK is also
# disabled so liblsl is built as a plain static archive instead of a .framework.
#
# Uses a dedicated build-static/ directory so it does not clobber the default
# dynamic build/. Final binary is copied to:
#   dist/emotiv_lsl-<version>-<os>-<arch>
#
# After build, runs `ldd` (Linux) or `otool -L` (macOS) and fails if any of
# liblsl, hidapi, or xdfwriter still appear as dynamic dependencies.
#
# Usage:
#   ./scripts/build_single_file.sh
# =============================================================================

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-static"
DIST_DIR="${ROOT}/dist"

# Pick generator: prefer Ninja, fall back to Unix Makefiles.
if command -v ninja >/dev/null 2>&1; then
    GEN="Ninja"
else
    GEN="Unix Makefiles"
fi

UNAME_S="$(uname -s)"
case "$UNAME_S" in
    Darwin) OS_NAME="macos" ;;
    Linux)  OS_NAME="linux" ;;
    *)      OS_NAME="$(echo "$UNAME_S" | tr '[:upper:]' '[:lower:]')" ;;
esac

ARCH="$(uname -m)"

EXTRA_ARGS=()
if [[ "$OS_NAME" == "macos" ]]; then
    EXTRA_ARGS+=("-DLSL_FRAMEWORK=OFF")
fi

echo "==> cmake configure (static link) -> $BUILD_DIR"
cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GEN" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLSLTEMPLATE_BUILD_GUI=OFF \
    -DLSLTEMPLATE_BUILD_CLI=OFF \
    -DEMOTIVLSL_BUILD_EMOTIV=ON \
    -DLSL_BUILD_STATIC=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_INSTALL_PREFIX="$BUILD_DIR/install" \
    "${EXTRA_ARGS[@]}"

echo "==> cmake --build $BUILD_DIR"
cmake --build "$BUILD_DIR" -j

BIN="$BUILD_DIR/src/emotiv/emotiv_lsl"
if [[ ! -f "$BIN" ]]; then
    echo "Built binary not found at: $BIN" >&2
    exit 1
fi

# Pull project version from CMakeLists.txt: project(LSLTemplate VERSION x.y.z ...)
VERSION="$(sed -nE 's/.*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$ROOT/CMakeLists.txt" | head -n1)"
VERSION="${VERSION:-0.0.0}"

mkdir -p "$DIST_DIR"
OUT="$DIST_DIR/emotiv_lsl-$VERSION-$OS_NAME-$ARCH"
cp "$BIN" "$OUT"
chmod +x "$OUT"

echo "==> Verifying no dynamic dependencies on liblsl / hidapi / xdfwriter"
BAD=""
if [[ "$OS_NAME" == "macos" ]]; then
    DEPS="$(otool -L "$OUT" 2>/dev/null || true)"
    BAD="$(echo "$DEPS" | grep -Ei '(liblsl|lsl\.framework|hidapi|xdfwriter)' || true)"
else
    DEPS="$(ldd "$OUT" 2>/dev/null || true)"
    BAD="$(echo "$DEPS" | grep -Ei '(liblsl|hidapi|xdfwriter)' || true)"
fi

if [[ -n "$BAD" ]]; then
    echo "FAIL: binary still depends on:" >&2
    echo "$BAD" >&2
    exit 1
fi
echo "    OK (no bundled-library dynamic dependencies)"

# Pretty-print size in MB
if command -v stat >/dev/null 2>&1; then
    if [[ "$OS_NAME" == "macos" ]]; then
        SIZE_BYTES="$(stat -f%z "$OUT")"
    else
        SIZE_BYTES="$(stat -c%s "$OUT")"
    fi
    SIZE_MB="$(awk "BEGIN { printf \"%.2f\", $SIZE_BYTES / (1024*1024) }")"
else
    SIZE_MB="?"
fi

echo ""
echo "==> Single-file build complete"
echo "    $OUT ($SIZE_MB MB)"
