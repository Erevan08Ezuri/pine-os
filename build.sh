#!/bin/sh
set -eu
TARGET="${1:-desktop}"
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$TARGET" = tab5 ]; then exec python "$ROOT/platform/tab5/tab5.py" build; fi
[ "$TARGET" = desktop ] || { echo "Usage: $0 desktop|tab5" >&2; exit 2; }
command -v cmake >/dev/null 2>&1 || { echo "CMake 3.24+ is required." >&2; exit 1; }
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cmake -S "$ROOT" -B "$ROOT/build/desktop" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$ROOT/build/desktop" --parallel
echo "[PINE][BUILD] Desktop build complete."
