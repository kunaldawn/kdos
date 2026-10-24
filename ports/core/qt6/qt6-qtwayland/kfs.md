## About
Qt Wayland for Qt 6 is the Wayland compositor library, QtWaylandCompositor, with its Qt Quick
types, and three client plugins a Qt 6 Wayland window can load: the Adwaita-style window
decoration and the `qt-shell` and `ivi-shell` integrations. The Wayland platform plugin itself, and the xdg-shell integration it uses on
this desktop, are in [qt6-qtbase](#port-qt6-qtbase).

## Flags
- `-DFEATURE_wayland_server=ON` — with `-DFEATURE_wayland_compositor_quick=ON`: the compositor
  library, and the types that let a compositor be written in Qt Quick. Each named, so a missing
  dependency stops the configure.
- `-DTEST_dmabuf_client_buffer=ON` — give the configure test for dma-buf buffer sharing its
  answer in advance, so CMake does not run it. Run in this tree, the test fails whatever the
  headers say, and the dma-buf integration would be dropped (the
  [ports catalogue](../../../../docs/kdos/06-reference/ports-catalogue.md#qt6) records it). The
  test only checks that the EGL headers define `EGL_LINUX_DMA_BUF_EXT` and
  `EGL_EXT_image_dma_buf_import_modifiers`, which [mesa](#port-mesa)'s do.
- `-DFEATURE_wayland_dmabuf_client_buffer=ON` — the compositor's hardware integration that takes
  a client's dma-buf buffers through EGL, which that test gates.
- `-DFEATURE_wayland_decoration_adwaita=ON` — the GNOME-style client-side decoration plugin, which
  needs QtDBus and Qt SVG; [qt6-qtsvg](#port-qt6-qtsvg) is in `depends` for it.
- `-DFEATURE_wayland_client_qt_shell=ON` — with `-DFEATURE_wayland_client_ivi_shell=ON`: the
  shell integrations for Qt's own `qt-shell` protocol and for the in-vehicle `ivi-shell`. Both
  are upstream's defaults, named.

## Notes
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).
