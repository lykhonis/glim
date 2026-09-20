# Web example

Vanilla HTML + canvas. No frameworks, no bundler. One wasm runs all four
examples: `index.html` (hello), `glass.html`, `gradients.html`,
`foreign.html`. Each page passes its scene id through `GlimCanvas`
(`example: 0..3`); the record functions are reused from
`examples/{hello,glass,gradients,foreign}/src/main.cpp`, never copied.

## Build (Emscripten)

```sh
cmake --preset web-webgpu-debug
cmake --build --preset web-webgpu-debug
```

Requires Emscripten (see root README prerequisites); the preset resolves
it via `EMSDK` or `em-config`.

## Run in a browser

```sh
pnpm exec nx run web:run
```

Stages a self-contained bundle into
`build/web-webgpu-debug/examples/web/serve/`, serves it on
`http://localhost:8080/index.html`, and opens the browser.
CI builds the same bundle (`.github/workflows/web.yml`).
The canvas fills the viewport and tracks window resizes; click it
(or press Enter) to expand the stats overlay, same as native.
A page can cap the backing-store pixel ratio with
`<body data-pr="1">` (glass does: blur passes are fill-rate bound).

## Embed in any app

```html
<canvas id="glim"></canvas>
<script type="module">
  import { GlimCanvas } from './glim.js';
  const glim = new GlimCanvas(document.getElementById('glim'), { moduleURL: './glim-app.js' });
  await glim.init();
  glim.start();
</script>
```

Embed contract: the app-built wasm exports `glimWebSelect`,
`glimWebResize`, `glimWebFrame`, `glimWebEvent`. React/Vue/Svelte call the
same two methods from a canvas ref + effect. Glim never touches the DOM
outside its canvas. See `docs/webgpu.md`.
