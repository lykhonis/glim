# wlr-protocols

Wayland protocols that wlroots compositors (sway) ship but `wayland-protocols` does not.

- **File:** `input-method-unstable-v2.xml` from wlroots `9b9d7d84` (sha256 `aaf68b2a…7630d`)
- **File:** `wlr-virtual-pointer-unstable-v1.xml` from wlr-protocols `c1140894` (sha256 `3ff6d540…bb524`)
- **License:** MIT (notice in the file)

Used only by `glim-wayland-input-probe` (`packages/glim-shell-wayland/tests`), the
pointer and input method the Wayland text e2e drives. The shell itself never links it.
