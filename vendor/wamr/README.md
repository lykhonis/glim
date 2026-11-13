# WAMR

WebAssembly Micro Runtime. https://github.com/bytecodealliance/wasm-micro-runtime

- **Version:** 2.4.5 (tag `WAMR-2.4.5`, source archive sha256 `1ab09d51099f276ca4a1d6629f6b589aab2bd0caa01445e05031a4bed22c199b`)
- **License:** Apache-2.0 WITH LLVM-exception (see `LICENSE`)

Only what the interpreter build needs is vendored: `build-scripts`, `core/config.h`,
`core/version.h{,.in}`, `core/iwasm/{include,common,interpreter}`, the AOT and
compilation headers `common` includes, and `core/shared/{mem-alloc,utils}` with the
`common`, `include`, `linux` and `darwin` platforms. Files are unmodified.

Used by `packages/glim-runtime-wamr` (K81), only when `GLIM_GUEST=ON`. Do not add
other third-party trees under `packages/`.
