#!/bin/bash
# Explicit golden regen: delete + run (strict off) + review the diff.
# Never runs in test/build/CI. Usage:
#   bash tools/scripts/regen-goldens.sh [preset]
# Regenerates CPU PNG goldens + showcase dump/focus/png for the given
# cmake preset build dir (default: macos-metal-debug).
set -euo pipefail

PRESET="${1:-macos-metal-debug}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$ROOT/build/$PRESET"

cmake --build "$BUILD" --target glim-showcase-golden glim-hello-golden glim-glass-golden glim-gradients-golden

echo "== deleting goldens (review the git diff after) =="
git -C "$ROOT" status --short -- examples/*/golden/ || true

"$BUILD/examples/showcase/glim-showcase-golden" "$ROOT/examples/showcase/golden"
"$BUILD/examples/hello/glim-hello-golden" "$ROOT/examples/hello/golden/hello.png"
"$BUILD/examples/glass/glim-glass-golden" "$ROOT/examples/glass/golden/glass.png"
"$BUILD/examples/gradients/glim-gradients-golden" "$ROOT/examples/gradients/golden/gradients.png"

echo "== review =="
git -C "$ROOT" status --short -- examples/*/golden/
