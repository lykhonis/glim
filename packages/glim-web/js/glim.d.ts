export interface GlimSize {
  width: number;
  height: number;
}

export interface GlimEvent {
  type: string;
  x?: number;
  y?: number;
  key?: string | number;
}

export interface GlimCanvasOptions {
  wasmURL: string;
  width?: number;
  height?: number;
  pixelRatio?: number;
  wasmImports?: Record<string, WebAssembly.Imports>;
}

export declare class GlimCanvas {
  constructor(canvas: HTMLCanvasElement, options: GlimCanvasOptions);
  canvas: HTMLCanvasElement;
  pixelRatio: number;
  logicalSize: GlimSize | undefined;
  onFrame: ((frame: { time: number; size?: GlimSize; type?: string }) => void) | null;
  onError: ((err: unknown) => void) | null;
  init(): Promise<GlimCanvas>;
  resize(): void;
  start(): void;
  stop(): void;
  setSize(width: number, height: number): void;
  setReduceTransparency(enabled: boolean): void;
  dispatch(event: GlimEvent): void;
  dispose(): void;
}

export default GlimCanvas;
