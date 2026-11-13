# Using Glim from CMake

Install a configured preset, then find it from your project. The GPU and shell backends are fixed by the preset you installed.

```sh
cmake --build build/macos-metal-release
cmake --install build/macos-metal-release --prefix ~/glim
```

```cmake
find_package(glim 0.1 REQUIRED COMPONENTS toolkit-host)
target_link_libraries(app PRIVATE glim::toolkit-host)
```

Components are `core`, `image`, `gpu`, `paint`, `paint-cpu`, `paint-gpu`, `embed`, `toolkit`, `toolkit-host`, `toolkit-host-cpu`, `shell`, the installed `gpu-*` backend, and `glim` (everything). `tests/package` is a working sample; `pnpm package-test` installs and builds it.
