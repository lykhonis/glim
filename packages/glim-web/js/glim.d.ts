export interface GlimCanvasOptions {
  moduleURL: string;
  example?: number;
  width?: number;
  height?: number;
  pixelRatio?: number;
}

export interface GlimFrame {
  time: number;
  size: { width: number; height: number };
}

export declare class GlimCanvas {
  constructor(canvas: HTMLCanvasElement, options: GlimCanvasOptions);
  canvas: HTMLCanvasElement;
  onFrame: ((frame: GlimFrame) => void) | null;
  onError: ((err: unknown) => void) | null;
  init(): Promise<this>;
  resize(): void;
  start(): void;
  stop(): void;
  setSize(width: number, height: number): void;
  setReduceTransparency(enabled: boolean): void;
  dispatch(event: { type: number; x?: number; y?: number; key?: number }): void;
  dispose(): void;
}

export default GlimCanvas;
