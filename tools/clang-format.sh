#!/bin/sh
# Resolve a clang-format binary: PATH first, Xcode toolchain on macOS.
if command -v clang-format >/dev/null 2>&1; then
  CF=clang-format
elif command -v xcrun >/dev/null 2>&1 && xcrun -f clang-format >/dev/null 2>&1; then
  CF="xcrun clang-format"
else
  echo "format: clang-format not found (apt install clang-format, or Xcode toolchain)" >&2
  exit 1
fi
cd "$(git rev-parse --show-toplevel)" || exit 1
# shellcheck disable=SC2086
exec $CF "$@"
