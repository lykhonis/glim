import { GlimCanvas } from './lib/glim.js';

const canvas = document.getElementById('glim');
const example = Number(document.body.dataset.example || 0);
// Optional per-page DPR cap (e.g. blur-heavy pages trading pixels for fill rate).
const pr = Number(document.body.dataset.pr || 0);
const params = new URLSearchParams(window.location.search);
const fixed = params.get('static') === '1';
const glim = new GlimCanvas(canvas, { moduleURL: './glim-app.js', example,
  ...(pr > 0 && !fixed ? { pixelRatio: pr } : {}),
  ...(fixed ? { width: 720, height: 480, pixelRatio: 1 } : {}) });
glim.onError = (err) => console.error('[glim]', err);
await glim.init();
for (const el of document.querySelectorAll('[data-slot-id]')) {
  glim.setSlot(Number(el.dataset.slotId), el);
}
if (fixed) {
  glim.call('glimWebOverlay', ['number'], [0]);
  glim.startFixed(0);
} else {
  glim.start();
}
