# Glim

[![CI](https://github.com/lykhonis/glim/actions/workflows/ci.yml/badge.svg)](https://github.com/lykhonis/glim/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A **C++ compositor and 2D paint library** for GUIs that need a little depth — stacked panels, perspective cards — without becoming a 3D engine.

One engine for **TVs, embedded devices, phones, the web, and desktop**. Native GPU backends (Metal, Vulkan, WebGPU) share the same paint API. Open source under [MIT](LICENSE).

## Why this exists

Most UI stacks either sit on **legacy GL/GLES** (deprecated on Apple, a dead end on modern Android and the web) or pull in a **full toolkit or 3D runtime**. Glim is the layer in between: you record fills and groups, it **merges** what can share a pass and **isolates** only when opacity or a 3D transform requires an offscreen.

It is **not** a widget library, **not** a scene graph for games, and **not** a wrapper around a desktop OpenGL context. Native APIs keep the compositor small enough for embedded and honest about what each OS actually ships.

## Build

**Nx** is the workflow: build, test, run, and CI. It invokes CMake/Ninja and caches install artifacts under `dist/` (not object files), so the graph stays one place as packages and examples grow.

```sh
pnpm install
pnpm test                      # nx run-many -t test
pnpm exec nx run hello:run
```

CMake presets remain for a single native target when you do not want Node. That is an escape hatch. New packages get an Nx project so `nx affected` and CI see them.

```sh
cmake --preset macos-metal-debug
cmake --build --preset macos-metal-debug --target glim-hello
```

## License

[MIT](LICENSE)
