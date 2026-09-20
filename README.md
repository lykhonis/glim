<p align="center">
  <img src="assets/glim-icon.png" width="200" alt="Glim">
</p>

# Glim

[![macOS](https://github.com/lykhonis/glim/actions/workflows/macos.yml/badge.svg)](https://github.com/lykhonis/glim/actions/workflows/macos.yml)
[![iOS](https://github.com/lykhonis/glim/actions/workflows/ios.yml/badge.svg)](https://github.com/lykhonis/glim/actions/workflows/ios.yml)
[![tvOS](https://github.com/lykhonis/glim/actions/workflows/tvos.yml/badge.svg)](https://github.com/lykhonis/glim/actions/workflows/tvos.yml)
[![Linux](https://github.com/lykhonis/glim/actions/workflows/linux.yml/badge.svg)](https://github.com/lykhonis/glim/actions/workflows/linux.yml)
[![Android](https://github.com/lykhonis/glim/actions/workflows/android.yml/badge.svg)](https://github.com/lykhonis/glim/actions/workflows/android.yml)
[![Web](https://github.com/lykhonis/glim/actions/workflows/web.yml/badge.svg)](https://github.com/lykhonis/glim/actions/workflows/web.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A **modular UI stack**. Host, Graphics, and Toolkit are separate layers you can use, replace, or skip. One Scene graph carries paint, native views, and semantics — renderer, hit-test, and accessibility walk the same tree.

It targets a wide range of platforms: **macOS, iOS, tvOS, Android** (phone, TV, AAOS), **Linux (Wayland)**; Windows and the web are next. Apple is **Metal**. Android and Linux are **Vulkan**. Later: **WebGPU**. No GPU: the same `draw` rasters **CPU**.

- **Host** — window, vsync, GPU natives, events. Swap the shell (AppKit, UIKit, Wayland, NativeActivity, or embed).
- **Graphics** — compositor: record a scene, merge compatible groups into one pass, isolate only when opacity or a tilt needs it. **2D with slight 3D.** Not a game engine.
- **Toolkit** — UI controls that record that same scene (coming; language TBD). Hello stays toolkit-free.

Layers compose: a UI block, raw paint, and semantic ids can share one graph — any mix, or none.

- **In-process:** `draw` the scene. No packet, no extra copy.
- **Embedded:** a sandboxed guest (WASM, out-of-process UI) `encode`s one frame packet per vsync; a native host `submit`s it. The OEM keeps the swapchain and the run loop.

## Try it

```sh
pnpm install
pnpm test
pnpm exec nx run hello:run
```

---

[MIT](LICENSE) · [Volodymyr Lykhonis](https://lykhonis.com)
