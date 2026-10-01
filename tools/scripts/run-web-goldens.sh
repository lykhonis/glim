#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${GLIM_BUILD_DIR:-$ROOT/build/web-webgpu-debug}"
SERVE="$BUILD/examples/web/serve"
GOT="${GLIM_GOT_DIR:-$BUILD/web-got}"
PORT="${GLIM_WEB_PORT:-8080}"
BUDGET="${GLIM_WEB_BUDGET_MS:-60000}"
ANGLE="${WEBGPU_ANGLE:-swiftshader}"

CHROME="${CHROME_BIN:-}"
if [ -z "$CHROME" ]; then
  for c in chrome google-chrome google-chrome-stable chromium chromium-browser "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"; do
    if command -v "$c" >/dev/null 2>&1; then CHROME="$c"; break; fi
    if [ -x "$c" ]; then CHROME="$c"; break; fi
  done
fi
if [ -z "$CHROME" ]; then
  echo "no Chrome found; set CHROME_BIN" >&2
  exit 1
fi

for f in index.html glass.html gradients.html foreign.html glim-app.js glim-app.wasm main.js lib/glim.js; do
  if [ ! -f "$SERVE/$f" ]; then
    echo "missing serve file $SERVE/$f; build with: cmake --preset web-webgpu-debug" >&2
    exit 1
  fi
done
python3 -c "import PIL.Image" 2>/dev/null || { echo "need pillow: pip install pillow" >&2; exit 1; }

mkdir -p "$GOT"
python3 -m http.server "$PORT" --directory "$SERVE" >/tmp/glim-web-goldens.log 2>&1 &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null || true' EXIT
sleep 2

shot() {
  "$CHROME" --headless=new --no-sandbox --disable-dev-shm-usage \
    --enable-unsafe-swiftshader --use-angle="$ANGLE" \
    --hide-scrollbars --force-device-scale-factor=1 \
    --window-size=720,513 --virtual-time-budget="$BUDGET" \
    --screenshot="$GOT/$1.png" "http://localhost:$PORT/$2?static=1" >/dev/null 2>&1
}

blank() {
  python3 -W ignore - "$GOT/$1.png" <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB").crop((0, 33, 720, 513)).resize((180, 120))
print("blank" if len(set(im.getdata())) < 10 else "ready")
EOF
}

shot_retry() {
  for _ in 1 2 3; do
    shot "$1" "$2"
    if [ "$(blank "$1")" = "ready" ]; then
      return 0
    fi
  done
  echo "$1 still blank after 3 shots" >&2
}

shot_retry web-hello index.html
shot_retry web-glass glass.html
shot_retry web-gradients gradients.html
curl --fail --silent --output /dev/null "http://localhost:$PORT/foreign.html?static=1"
echo "foreign smoke ok"

python3 -W ignore - "$GOT" "$ROOT" <<'EOF'
import sys
from PIL import Image
got_dir, root = sys.argv[1], sys.argv[2]
cases = [
    ("web-hello", "examples/hello/golden/hello.png", 2000, 48),
    ("web-gradients", "examples/gradients/golden/gradients.png", 4000, 48),
    ("web-glass", "examples/glass/golden/glass.png", 280000, 224),
]
failed = False
for name, golden, max_over, max_d in cases:
    shot = Image.open(f"{got_dir}/{name}.png").convert("RGB")
    canvas = shot.crop((0, 33, 720, 513))
    canvas.save(f"{got_dir}/{name}-canvas.png")
    want = Image.open(f"{root}/{golden}").convert("RGB")
    if canvas.size != want.size:
        print(f"{name}: size mismatch {canvas.size} vs {want.size}")
        failed = True
        continue
    gp, wp = canvas.load(), want.load()
    n = 0
    mx = 0
    for y in range(480):
        for x in range(720):
            d = max(abs(a - b) for a, b in zip(gp[x, y], wp[x, y]))
            if d > 2:
                n += 1
                mx = max(mx, d)
    status = "ok" if (n <= max_over and mx <= max_d) else "MISMATCH"
    print(f"{name}: over-delta-2={n} (limit {max_over}) maxd={mx} (limit {max_d}) {status}")
    if status == "MISMATCH":
        failed = True
sys.exit(1 if failed else 0)
EOF
