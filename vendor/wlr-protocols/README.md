# wlr-protocols

Wayland protocols that wlroots compositors (sway) ship but `wayland-protocols` does not.

- **File:** `input-method-unstable-v2.xml` from wlroots `9b9d7d84` (sha256 `aaf68b2a…7630d`)
- **License:** MIT (notice in the file)

Used only by `glim-wayland-ime-probe` (`packages/glim-shell-wayland/tests`), the
input method the Wayland text e2e drives. The shell itself never links it.
