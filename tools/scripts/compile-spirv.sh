#!/usr/bin/env bash
# Compile GLSL to Vulkan 1.1 SPIR-V. Usage:
#   compile-spirv.sh <shader.vert|frag> <out.spv>
set -euo pipefail

src="${1:?glsl source}"
out="${2:?spir-v output}"

if [[ ! -f "$src" ]]; then
    echo "missing shader: $src" >&2
    exit 1
fi

if ! command -v glslangValidator >/dev/null 2>&1; then
    echo "glslangValidator not found (install glslang / glslang-tools)" >&2
    exit 1
fi

mkdir -p "$(dirname "$out")"
glslangValidator -V --target-env vulkan1.1 "$src" -o "$out"
