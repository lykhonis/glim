/**
 * Glim for the web: loader + frame driver. No dependencies.
 * Attaches to an app-owned <canvas>; works from any framework.
 */

const DEFAULT_SELECTOR_SIZE = { width: 720, height: 480 };

function cssSize(canvas, width, height) {
  if (width > 0) canvas.style.width = `${width}px`;
  if (height > 0) canvas.style.height = `${height}px`;
}

function readDevicePixelRatio(pixelRatioOverride) {
  if (typeof pixelRatioOverride === 'number' && pixelRatioOverride > 0) {
    return pixelRatioOverride;
  }
  if (typeof window !== 'undefined' && window.devicePixelRatio > 0) {
    return window.devicePixelRatio;
  }
  return 1;
}

export class GlimCanvas {
  /**
   * @param {HTMLCanvasElement} canvas - app-owned canvas to paint into.
   * @param {object} options
   * @param {string} options.wasmURL - URL of the app-built glim.wasm.
   * @param {number} [options.width] - CSS width hint (canvas attribute wins).
   * @param {number} [options.height] - CSS height hint.
   * @param {number} [options.pixelRatio] - override for devicePixelRatio.
   * @param {object} [options.wasmImports] - extra imports for the wasm module.
   */
  constructor(canvas, options = {}) {
    if (!canvas || canvas.tagName !== 'CANVAS') {
      throw new Error('GlimCanvas requires an <canvas> element');
    }
    if (!options.wasmURL) {
      throw new Error('GlimCanvas requires options.wasmURL');
    }
    this.canvas = canvas;
    this.options = options;
    this.pixelRatio = readDevicePixelRatio(options.pixelRatio);
    this.wasm = null;
    this.running = false;
    this.rafId = 0;
    this.onFrame = null;
    this.onError = null;
    this.reduceTransparency = false;
    this._onResize = () => this.resize();
    this._disposers = [];
  }

  async init() {
    const { width = DEFAULT_SELECTOR_SIZE.width, height = DEFAULT_SELECTOR_SIZE.height } =
      this.options;
    cssSize(this.canvas, width, height);
    this.resize();
    if (typeof window !== 'undefined') {
      window.addEventListener('resize', this._onResize);
    }
    this._forwardDomEvents();
    return this;
  }

  resize() {
    const rect = this.canvas.getBoundingClientRect();
    const cssW = Math.max(1, Math.round(rect.width || this.options.width || 720));
    const cssH = Math.max(1, Math.round(rect.height || this.options.height || 480));
    this.pixelRatio = readDevicePixelRatio(this.options.pixelRatio);
    const dw = Math.round(cssW * this.pixelRatio);
    const dh = Math.round(cssH * this.pixelRatio);
    if (this.canvas.width !== dw) this.canvas.width = dw;
    if (this.canvas.height !== dh) this.canvas.height = dh;
    this.logicalSize = { width: cssW, height: cssH };
    if (this.wasm?.glimWebResize) {
      this.wasm.glimWebResize(cssW, cssH, this.pixelRatio);
    }
  }

  start() {
    if (this.running) return;
    this.running = true;
    const tick = (timeMs) => {
      if (!this.running) return;
      try {
        if (this.wasm?.glimWebFrame) {
          this.wasm.glimWebFrame(timeMs / 1000, this.reduceTransparency ? 1 : 0);
        }
        if (typeof this.onFrame === 'function') {
          this.onFrame({ time: timeMs / 1000, size: this.logicalSize });
        }
      } catch (err) {
        if (typeof this.onError === 'function') {
          this.onError(err);
        } else {
          console.error('[glim]', err);
        }
      }
      this.rafId = requestAnimationFrame(tick);
    };
    this.rafId = requestAnimationFrame(tick);
  }

  stop() {
    this.running = false;
    if (this.rafId) cancelAnimationFrame(this.rafId);
    this.rafId = 0;
  }

  setSize(width, height) {
    cssSize(this.canvas, width, height);
    this.resize();
  }

  setReduceTransparency(enabled) {
    this.reduceTransparency = Boolean(enabled);
  }

  /**
   * Forward a pointer/keyboard event as data.
   */
  dispatch(event) {
    if (this.wasm?.glimWebEvent) {
      this.wasm.glimWebEvent(event.type, event.x ?? 0, event.y ?? 0, event.key ?? 0);
      return;
    }
    if (typeof this.onFrame === 'function' && event.type === 'pointerdown') {
      this.onFrame({ type: 'pointerdown', ...event });
    }
  }

  dispose() {
    this.stop();
    if (typeof window !== 'undefined') {
      window.removeEventListener('resize', this._onResize);
    }
    for (const dispose of this._disposers) dispose();
    this._disposers = [];
  }

  _forwardDomEvents() {
    const canvas = this.canvas;
    const pointer = (type) => (e) => {
      const rect = canvas.getBoundingClientRect();
      this.dispatch({ type, x: e.clientX - rect.left, y: e.clientY - rect.top });
    };
    const key = (type) => (e) => {
      if (e.key === 'Escape') this.setReduceTransparency(!this.reduceTransparency);
      this.dispatch({ type, key: e.key });
    };
    canvas.addEventListener('pointerdown', pointer('pointerdown'));
    canvas.addEventListener('pointermove', pointer('pointermove'));
    canvas.addEventListener('pointerup', pointer('pointerup'));
    window.addEventListener('keydown', key('keydown'));
    window.addEventListener('keyup', key('keyup'));
    this._disposers.push(() => {
      canvas.removeEventListener('pointerdown', pointer);
    });
  }
}

export default GlimCanvas;
