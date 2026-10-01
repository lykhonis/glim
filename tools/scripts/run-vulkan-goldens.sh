#!/bin/bash
set -euo pipefail

if [ -n "${VK_ICD_FILENAMES:-}" ]; then
  echo "using caller VK_ICD_FILENAMES=$VK_ICD_FILENAMES"
elif [ -f /usr/share/vulkan/icd.d/vk_swiftshader_icd.json ]; then
  export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/vk_swiftshader_icd.json
  echo "using SwiftShader ICD"
else
  icds=(/usr/share/vulkan/icd.d/lvp_icd.*.json)
  if [[ -e ${icds[0]} ]]; then
    export VK_ICD_FILENAMES=${icds[0]}
    echo "using lavapipe ICD: $VK_ICD_FILENAMES"
  else
    echo "no software Vulkan ICD found; CPU goldens still run without GPU present"
  fi
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${GLIM_BUILD_DIR:-$ROOT/build/linux-vulkan-debug}"

if [ ! -d "$BUILD" ]; then
  echo "missing build dir $BUILD; configure with: cmake --preset linux-vulkan-debug" >&2
  exit 1
fi

ctest --test-dir "$BUILD" -R "glim-hello-golden|glim-glass-golden|glim-gradients-golden|glim-hello-vulkan-golden|glim-glass-vulkan-golden|glim-gradients-vulkan-golden|glim-cpu-test|glim-gradient-test|glim-reuse-test" --output-on-failure
