// @ts-nocheck
// Browser producer for the video clip (WebClip.cpp): WebGL2 renders the
// shared scene, the canvas becomes a VideoFrame, and the frame is copied
// into a texture on Glim's WebGPU device. GL's y is up, the scene's down.
addToLibrary({
  glimClipDraw__deps: ['$WebGPU', '$UTF8ToString'],
  glimClipDraw: (device, texture, scene, w, h, t) => {
    if (Module.glimClip === undefined) {
      Module.glimClip = null;
      if (typeof VideoFrame === 'undefined' || typeof OffscreenCanvas === 'undefined') return 0;
      const canvas = new OffscreenCanvas(w, h);
      const gl = canvas.getContext('webgl2', {antialias: false, alpha: false});
      if (!gl) return 0;
      const shader = (type, src) => {
        const s = gl.createShader(type);
        gl.shaderSource(s, src);
        gl.compileShader(s);
        if (!gl.getShaderParameter(s, gl.COMPILE_STATUS)) console.error(gl.getShaderInfoLog(s));
        return s;
      };
      const prog = gl.createProgram();
      // Arrays keep '#' off line starts, where emscripten's preprocessor looks.
      gl.attachShader(prog, shader(gl.VERTEX_SHADER, [
        '#version 300 es',
        'void main() {',
        '  vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);',
        '  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);',
        '}',
      ].join('\n')));
      gl.attachShader(prog, shader(gl.FRAGMENT_SHADER, [
        '#version 300 es',
        'precision highp float;',
        '#define float2 vec2',
        '#define float3 vec3',
        '#define CONST const',
        'vec3 saturate(vec3 c) { return clamp(c, 0.0, 1.0); }',
        UTF8ToString(scene),
        'uniform vec2 size;',
        'uniform float t;',
        'out vec4 color;',
        'void main() {',
        '  vec2 p = vec2(gl_FragCoord.x, size.y - gl_FragCoord.y);',
        '  color = vec4(shade(p / size, t, floor(p)), 1.0);',
        '}',
      ].join('\n')));
      gl.linkProgram(prog);
      if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) return 0;
      Module.glimClip = {canvas, gl, prog, size: gl.getUniformLocation(prog, 'size'),
                         t: gl.getUniformLocation(prog, 't')};
    }
    const c = Module.glimClip;
    if (!c) return 0;
    const gl = c.gl;
    gl.viewport(0, 0, w, h);
    gl.useProgram(c.prog);
    gl.uniform2f(c.size, w, h);
    gl.uniform1f(c.t, t);
    gl.drawArrays(gl.TRIANGLES, 0, 3);
    const frame = new VideoFrame(c.canvas, {timestamp: Math.round(t * 1e6)});
    WebGPU.getJsObject(device).queue.copyExternalImageToTexture(
      {source: frame}, {texture: WebGPU.getJsObject(texture)}, [w, h]);
    frame.close();
    return 1;
  },
});
