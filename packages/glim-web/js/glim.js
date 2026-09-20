function cssSize(canvas, width, height) {
  if (width > 0) canvas.style.width = `${width}px`;
  if (height > 0) canvas.style.height = `${height}px`;
}

function readDevicePixelRatio(override) {
  if (typeof override === 'number' && override > 0) return override;
  if (typeof window !== 'undefined' && window.devicePixelRatio > 0) return window.devicePixelRatio;
  return 1;
}

export class GlimCanvas {
  constructor(canvas, options = {}) {
    if (!canvas || canvas.tagName !== 'CANVAS') throw new Error('GlimCanvas requires an <canvas>');
    if (!options.moduleURL) throw new Error('GlimCanvas requires options.moduleURL');
    this.canvas = canvas;
    this.options = options;
    this.pixelRatio = readDevicePixelRatio(options.pixelRatio);
    this.module = null;
    this.running = false;
    this.rafId = 0;
    this.onFrame = null;
    this.onError = null;
    this.reduceTransparency = false;
    this._onResize = () => this.resize();
    this._disposers = [];
  }

  async init() {
    const moduleURL = new URL(this.options.moduleURL, window.location.href).href;
    const createModule = (await import(moduleURL)).default;
    this.module = await createModule();
    const example = this.options.example || 0;
    if (example !== 0) this.module.ccall('glimWebSelect', null, ['number'], [example]);
    // Explicit options.width/height pin the canvas; otherwise CSS owns the size.
    const { width = 0, height = 0 } = this.options;
    cssSize(this.canvas, width, height);
    this.resize();
    if (typeof window !== 'undefined') window.addEventListener('resize', this._onResize);
    this._forwardDomEvents();
    return this;
  }

  call(name, types, args) {
    try {
      this.module.ccall(name, null, types, args);
    } catch (err) {
      this.fail(err);
    }
  }

  fail(err) {
    if (typeof this.onError === 'function') this.onError(err);
    else console.error('[glim]', err);
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
    if (this.module) this.call('glimWebResize', ['number', 'number', 'number'], [cssW, cssH, this.pixelRatio]);
  }

  start() {
    if (this.running) return;
    this.running = true;
    const tick = (timeMs) => {
      if (!this.running) return;
      if (this.module) {
        this.call('glimWebFrame', ['number', 'number'], [timeMs / 1000, this.reduceTransparency ? 1 : 0]);
      }
      if (typeof this.onFrame === 'function') {
        try {
          this.onFrame({ time: timeMs / 1000, size: this.logicalSize });
        } catch (err) {
          this.fail(err);
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

  dispatch(event) {
    if (this.module) {
      const codes = { pointerdown: 1, pointermove: 2, pointerup: 3, keydown: 4, keyup: 5 };
      const key = event.key === 'Enter' ? 13 : event.key === 'Escape' ? 27 : 0;
      this.call('glimWebEvent', ['number', 'number', 'number', 'number'],
        [codes[event.type] ?? 0, event.x ?? 0, event.y ?? 0, key]);
    }
  }

  dispose() {
    this.stop();
    if (typeof window !== 'undefined') window.removeEventListener('resize', this._onResize);
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
    const down = pointer('pointerdown');
    const move = pointer('pointermove');
    const up = pointer('pointerup');
    const kd = key('keydown');
    const ku = key('keyup');
    canvas.addEventListener('pointerdown', down);
    canvas.addEventListener('pointermove', move);
    canvas.addEventListener('pointerup', up);
    window.addEventListener('keydown', kd);
    window.addEventListener('keyup', ku);
    this._disposers.push(() => {
      canvas.removeEventListener('pointerdown', down);
      canvas.removeEventListener('pointermove', move);
      canvas.removeEventListener('pointerup', up);
      window.removeEventListener('keydown', kd);
      window.removeEventListener('keyup', ku);
    });
  }
}

export default GlimCanvas;
