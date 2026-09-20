# Web example

Vanilla HTML + canvas. No frameworks, no bundler.

## Build (Emscripten)

```sh
source "$EMSDK/emsdk_env.sh"
cmake --preset web-webgpu-debug
cmake --build --preset web-webgpu-debug
```

## Run in a browser

```sh
pnpm exec nx run web:run
```

Builds the wasm, stages a self-contained bundle into
`build/web-webgpu-debug/examples/web/serve/`, serves it on
`http://localhost:8080/index.html`, and opens the browser
(`--port` overrides the port). CI builds the same bundle
(`.github/workflows/web.yml`).

## Embed in any app

```html
<canvas id="glim"></canvas>
<script type="module">
  import { GlimCanvas } from './glim.js';
  const glim = new GlimCanvas(document.getElementById('glim'), { wasmURL: './glim.wasm' });
  await glim.init();
  glim.start();
</script>
```

React/Vue/Svelte call the same two methods from a canvas ref + effect.
Glim never touches the DOM outside its canvas. See `docs/webgpu.md`.
