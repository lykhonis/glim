#!/usr/bin/env bash
# Text input and clipboard on Wayland (N8.9). Runs the text example under a
# headless sway, types through text-input-v3 with glim-wayland-ime-probe as
# the input method, and round-trips the clipboard through Glim's edit menu.
#
#   tools/wayland-text-e2e.sh
#
# Needs sway, wlrctl and wl-clipboard. Env: GLIM_BUILD_DIR.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
build=${GLIM_BUILD_DIR:-$root/build/linux-vulkan-debug}
app=$build/examples/text/glim-text
probe=$build/packages/glim-shell-wayland/glim-wayland-ime-probe
[ -x "$app" ] && [ -x "$probe" ] || { echo "FAIL: build glim-text and glim-wayland-ime-probe in $build"; exit 1; }

tmp=$(mktemp -d)
export XDG_RUNTIME_DIR=$tmp
chmod 700 "$tmp"
pids=()
cleanup() {
  for p in "${pids[@]}"; do kill "$p" 2>/dev/null || true; done
  rm -rf "$tmp"
}
trap cleanup EXIT
fail() {
  echo "FAIL: $1"
  for log in sway ime app; do
    echo "--- $log"
    tail -40 "$tmp/$log.log" 2>/dev/null || true
  done
  exit 1
}
# until <file> <fixed string> <what>: up to 20 s.
until_log() {
  for _ in $(seq 100); do
    grep -qxF -- "$2" "$tmp/$1.log" 2>/dev/null && return
    sleep 0.2
  done
  fail "never $3"
}

cat > "$tmp/sway.conf" <<'EOF'
default_border none
output HEADLESS-1 resolution 800x600 position 0 0
EOF
WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1 \
  sway -c "$tmp/sway.conf" > "$tmp/sway.log" 2>&1 &
pids+=($!)
for _ in $(seq 50); do
  socket=$(cd "$tmp" && ls wayland-[0-9] 2>/dev/null | head -1) && [ -n "$socket" ] && break
  sleep 0.2
done
[ -n "${socket:-}" ] || fail "sway never opened a Wayland socket"
export WAYLAND_DISPLAY=$socket

mkfifo "$tmp/ime.in"
"$probe" < "$tmp/ime.in" > "$tmp/ime.log" 2>&1 &
pids+=($!)
exec 3> "$tmp/ime.in"
for _ in $(seq 50); do grep -q '^ime ready$' "$tmp/ime.log" && break; sleep 0.2; done
grep -q '^ime ready$' "$tmp/ime.log" || fail "the input method never bound"

"$app" > "$tmp/app.log" 2>&1 &
pids+=($!)

# wlrctl moves the pointer relatively: pin it to the top left, then move.
click() {
  wlrctl pointer move -5000 -5000
  wlrctl pointer move "$1" "$2"
  wlrctl pointer click "$3"
}

# The field sits at the window's top left, and the window fills the output.
# Click until the field takes focus and the input method activates on it,
# with an empty normal (not password) text.
for _ in $(seq 30); do
  click 40 16 left
  sleep 0.5
  grep -qxF 'ime state active=1 purpose=0 hint=0 cursor=0 anchor=0 surrounding=' "$tmp/ime.log" && break
done
until_log ime 'ime state active=1 purpose=0 hint=0 cursor=0 anchor=0 surrounding=' "activated the input method"

# Composition shows in the field, the commit replaces it, and the shell
# reports the new text and caret back as surrounding text (bytes).
echo 'preedit にほん' >&3
until_log app 'text value にほん' "showed the composition"
echo 'commit 日本' >&3
until_log app 'text value 日本' "committed the composition"
until_log ime 'ime state active=1 purpose=0 hint=0 cursor=6 anchor=6 surrounding=日本' "sent the surrounding text"
echo 'delete 3 0' >&3
until_log app 'text value 日' "deleted surrounding text"

# Clipboard out: on a field holding text with no selection, the edit menu
# offers Select All first; then Cut is first. The cut text reaches other
# clients.
click 60 16 right
sleep 0.5
click 78 30 left
sleep 0.5
click 60 16 right
sleep 0.5
click 78 30 left
until_log app 'text value ' "cut from the edit menu"
[ "$(wl-paste -n)" = 日 ] || fail "another client read '$(wl-paste -n 2>&1)', not the cut text"

# Clipboard in: text copied by another client is the empty field's only
# menu item, Paste.
wl-copy outside
sleep 0.5
click 60 16 right
sleep 0.5
click 78 30 left
until_log app 'text value outside' "pasted from another client"

echo "wayland text: composed 日本, deleted to 日, cut to and pasted from other clients"
