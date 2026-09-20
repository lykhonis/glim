import { GlimCanvas } from './lib/glim.js';

const canvas = document.getElementById('glim');
const example = Number(document.body.dataset.example || 0);
// Optional per-page DPR cap (e.g. blur-heavy pages trading pixels for fill rate).
const pr = Number(document.body.dataset.pr || 0);
const glim = new GlimCanvas(canvas, { moduleURL: './glim-app.js', example,
  ...(pr > 0 ? { pixelRatio: pr } : {}) });
glim.onError = (err) => console.error('[glim]', err);
await glim.init();
glim.start();
