# Glim

[![CI](https://github.com/lykhonis/glim/actions/workflows/ci.yml/badge.svg)](https://github.com/lykhonis/glim/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

Glim is a **C++ paint and compositor library** for drawing user interfaces: rectangles, groups, opacity, and a little perspective so a panel can tilt or stack in space. It is **2D with slight 3D**, not a game engine and not a widget kit.

The same drawing model is meant to run on **televisions, embedded boards, phones, browsers, and desktops**. Each platform uses the GPU it actually has — Metal, Vulkan, or WebGPU — instead of a lowest-common-denominator GL stack that those platforms are already leaving behind.

Open source under the [MIT License](LICENSE). Created by [Volodymyr Lykhonis](https://lykhonis.com).

## Why

Interfaces today are asked to look rich on cheap hardware: glass bars, layered cards, motion, 4K TVs, 120 Hz phones. The usual answers are a poor fit.

**Legacy OpenGL / GLES** still shows up because it used to be “everywhere.” It is deprecated on Apple, second-class on Android, and the web has moved on. Shipping a new library on that path means inheriting a dead API and then rewriting it.

**Full UI toolkits** own widgets, text, input, and opinionated layout. That is the right product if you want an app framework. It is the wrong dependency if you already have a shell — a TV launcher, an embedded HMI, a custom window — and only need pixels composed well.

**General 3D engines** can draw a UI, at the cost of a scene graph, a frame graph, and a footprint that embedded and WASM budgets cannot spare. A settings screen does not need meshes, lights, or a physics world. It needs fast, correct **layering**.

Glim sits in the gap: you record paint (fills, groups, transforms). Compatible groups **collapse into one pass**. A group is **isolated** to an offscreen only when opacity or a non-flat transform requires it. That is how you get stacked, slightly 3D chrome without paying for a 3D renderer or for an offscreen per layer.

It is not Cairo-on-the-GPU, not a retained widget tree, and not “OpenGL with extra steps.” It is a compositor you can embed, small enough to take seriously on a device that does not have a desktop GPU.

## Try it

```sh
pnpm install
pnpm test
pnpm exec nx run hello:run
```

## License

[MIT](LICENSE)
