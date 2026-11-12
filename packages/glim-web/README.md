# `@glim/web`

Framework-agnostic loader and frame driver for the Glim WebGPU canvas, plus the
prebuilt glass module (`dist/glim-app.js` and `dist/glim-app.wasm`).
**Zero runtime dependencies by policy** — do not add any.

The wasm is the glass example. A page that paints its own scene builds its own
module with Emscripten and passes that URL instead; the loader is the same.

## Use

```html
<canvas id="glim"></canvas>
<script type="module">
  import { GlimCanvas } from '@glim/web';
  const glim = new GlimCanvas(document.getElementById('glim'), {
    moduleURL: new URL('@glim/web/dist/glim-app.js', import.meta.url).href,
  });
  await glim.init();
  glim.start();
</script>
```

React, Vue, Svelte, and vanilla apps all call the same two methods
(`init()` + `start()`).
Glim never touches the DOM outside its own canvas.
