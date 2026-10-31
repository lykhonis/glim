#!/bin/sh
# Format every tracked C++ file except vendor/ (nearest .clang-format wins).
cd "$(git rev-parse --show-toplevel)" || exit 1
files=$(git ls-files '*.cpp' '*.h' ':!:vendor/**' | while IFS= read -r f; do
  [ -e "$f" ] && printf '%s\n' "$f"
done)
# shellcheck disable=SC2086
tools/clang-format.sh -i $files
