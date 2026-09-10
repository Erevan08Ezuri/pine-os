#!/bin/sh
set -eu
TARGET="${1:-desktop}"
[ "$TARGET" = desktop ] || { echo "Pine OS v0.1 supports the desktop target. Usage: $0 desktop" >&2; exit 2; }
command -v cmake >/dev/null 2>&1 || { echo "CMake 3.24+ is required." >&2; exit 1; }
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$ROOT" -B "$ROOT/build/desktop" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$ROOT/build/desktop" --parallel
echo "[PINE][BUILD] Desktop build complete."
