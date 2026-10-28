## About

glmark2 is an OpenGL and OpenGL ES 2.0 benchmark: a set run of scenes (shading, texturing, buffers,
effects) that ends in one score. Unless `--swap-mode fifo` is given it asks for a swap interval of
0, so the score is not capped at the display's refresh rate. KDOS builds six programs, one per
flavour: `glmark2-wayland` and `glmark2-es2-wayland` in a Wayland window, `glmark2-drm` and
`glmark2-es2-drm` straight to the display from a text console, and `glmark2-gbm` and
`glmark2-es2-gbm` rendering offscreen with no display at all. Each reaches GL through EGL. Which
one answers which question is in
[Which tool answers which question](../../../../docs/kdos/05-developer/testing.md#which-tool-answers-which-question).

## Flags

- `-Dflavors=wayland-gl,wayland-glesv2,drm-gl,drm-glesv2,gbm-gl,gbm-glesv2` — The six flavours
  above. Upstream's default is an empty list, and configure stops on it. The three X11 flavours
  (`x11-gl`, `x11-glesv2`, `x11-gl-egl`) are left out: the graphics tools on the host are the
  Wayland and EGL ones, and an X11 client's view of GL is tested from inside a box.

## Patches

- `wayland-generated-header.patch` — Adds the generated `xdg-shell-client-protocol.h` to the sources of glmark2's internal `native_wayland_dep` dependency in `src/meson.build`. `main.cpp` includes `native-state-wayland.h`, which includes that header, yet without the patch only the static library wrapping the Wayland code waits for the header to be generated; with the header among the dependency's sources, every executable that uses the dependency waits for it too, so a parallel build cannot compile `main.cpp` before the header exists.
