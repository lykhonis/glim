# `@glim/web`

Framework-agnostic loader + frame driver for the Glim WebGPU canvas.
**Zero runtime dependencies by policy** — do not add any.

## Use (any framework)

```html
<canvas id="glim" width="720" height="480"></canvas>
<script type="module">
  import { GlimCanvas } from './glim.js';
  const glim = new GlimCanvas(document.getElementById('glim'), {
    wasmURL: './glim.wasm',
  });
  await glim.init();
  glim.start();
</script>
```

React, Vue, Svelte, and vanilla apps all call the same two methods
(`init()` + `start()`); see `docs/webgpu.md` for per-framework snippets.
Glim never touches the DOM outside its own canvas.
