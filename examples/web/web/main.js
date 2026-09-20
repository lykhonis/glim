import { GlimCanvas } from './lib/glim.js';

const canvas = document.getElementById('glim');
const glim = new GlimCanvas(canvas, { wasmURL: './glim.wasm' });
glim.onError = (err) => console.error('[glim]', err);
await glim.init();
glim.start();
