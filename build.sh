#!/bin/sh
set -eu
TARGET="${1:-desktop}"
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

# School-managed Macs may not expose a system CMake. PineOS supports the
# portable Kitware bundle we install under ~/tools without requiring admin.
if ! command -v cmake >/dev/null 2>&1; then
    PORTABLE_CMAKE="$HOME/tools/cmake-4.1.1-macos-universal/CMake.app/Contents/bin"
    if [ -x "$PORTABLE_CMAKE/cmake" ]; then
        PATH="$PORTABLE_CMAKE:$PATH"
        export PATH
        echo "[PINE][BUILD] Using portable CMake at $PORTABLE_CMAKE/cmake"
    fi
fi

if [ "$TARGET" = tab5 ]; then
    command -v cmake >/dev/null 2>&1 || { echo "CMake 3.24+ is required for ESP-IDF." >&2; exit 1; }
    if command -v python3 >/dev/null 2>&1; then
        exec python3 "$ROOT/platform/tab5/tab5.py" build
    elif command -v python >/dev/null 2>&1; then
        exec python "$ROOT/platform/tab5/tab5.py" build
    else
        echo "Python 3 is required for the Tab5 build." >&2
        exit 1
    fi
fi

[ "$TARGET" = desktop ] || { echo "Usage: $0 desktop|tab5" >&2; exit 2; }
command -v cmake >/dev/null 2>&1 || { echo "CMake 3.24+ is required." >&2; exit 1; }
cmake -S "$ROOT" -B "$ROOT/build/desktop" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$ROOT/build/desktop" --parallel
echo "[PINE][BUILD] Desktop build complete."
