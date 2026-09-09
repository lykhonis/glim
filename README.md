# Glim

Cross-platform compositor and 2D graphics library.

**Status:** greenfield rewrite. The in-tree `glim/` OpenGL / NSOpenGL / GLM path is **legacy** and will be removed. The product GPU APIs are **Metal** (Apple), **Vulkan** (Android / Linux Wayland / Windows), and **WebGPU** (web).

Architecture: [docs/architecture.md](docs/architecture.md)

License: [MIT](LICENSE)

## Build (macOS 13+, Metal)

CMake 3.22+ and Ninja. Node is **not** required for this path.

```sh
cmake --preset macos-metal-debug
cmake --build --preset macos-metal-debug --target glim-hello
```

Optional workspace orchestration (pnpm / Nx); caches install artifacts under `dist/` only:

```sh
pnpm install
pnpm exec nx build hello
```

The GLM git submodule is unused by new packages. Do not add GLM includes to public headers.
