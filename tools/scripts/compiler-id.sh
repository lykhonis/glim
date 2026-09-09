#!/usr/bin/env bash
set -euo pipefail
# Written so Nx hashing can include compiler identity without caching object files.
cxx="${CXX:-c++}"
if command -v "$cxx" >/dev/null 2>&1; then
  echo "$cxx"
  "$cxx" --version | head -n 1
else
  echo "no-cxx"
fi
