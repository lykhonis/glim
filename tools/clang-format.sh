#!/bin/sh
# Resolve a clang-format binary: PATH first, Xcode toolchain on macOS.
# CI pins WANT (see gate.yml); a local mismatch warns because the local
# result may not match CI - even patch releases reformat some constructs.
WANT="18.1.8"
if command -v clang-format >/dev/null 2>&1; then
  CF=clang-format
elif command -v xcrun >/dev/null 2>&1 && xcrun -f clang-format >/dev/null 2>&1; then
  CF="xcrun clang-format"
else
  echo "format: clang-format not found (apt install clang-format, or Xcode toolchain)" >&2
  exit 1
fi
# shellcheck disable=SC2086
HAVE=$($CF --version 2>/dev/null | grep -oE 'version [0-9]+\.[0-9]+\.[0-9]+' | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
if [ "$1" = "--require-version" ]; then
  if [ -z "$HAVE" ]; then
    echo "format: cannot determine clang-format version" >&2
    exit 1
  elif [ "$HAVE" != "$WANT" ]; then
    echo "format: have clang-format $HAVE, CI uses $WANT - skipping local check" >&2
    echo "format: pip install 'clang-format==$WANT' for a matching local check" >&2
    exit 1
  fi
  exit 0
fi
if [ -n "$HAVE" ] && [ "$HAVE" != "$WANT" ]; then
  echo "format: have clang-format $HAVE, CI uses $WANT - result may differ" >&2
fi
cd "$(git rev-parse --show-toplevel)" || exit 1
# shellcheck disable=SC2086
exec $CF "$@"
