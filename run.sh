#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PINE="$ROOT/build/desktop/bin/pine"
[ -x "$PINE" ] || { echo "Pine OS is not built. Run ./build.sh desktop first." >&2; exit 1; }
cd "$ROOT"
exec "$PINE"
