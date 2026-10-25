## About

GTK 4 is the current GTK widget toolkit, and the one [libadwaita](#port-libadwaita) is built on. It installs beside [gtk3](#port-gtk3). It draws with
OpenGL or Vulkan through its GSK renderers and plays video through GStreamer. Like every toolkit in
the tree it is for applications only, and no part of the KDOS desktop links it (see
[Toolkits are for applications](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#toolkits-are-for-applications-not-the-desktop)).

## Flags

- `-Db_ndebug=if-release` — Define `NDEBUG` in a release build, which removes the `assert()`
  checks in the roaring bitmaps under `GtkBitset`, walked on every list-view update. gtk4 is one of
  the few ports that pass it; GTK's own `G_DISABLE_ASSERT` follows the optimisation level and needs
  no switch (see [meson](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#meson)).
- `-Dwayland-backend=true`, `-Dx11-backend=true` — Build both GDK backends, upstream's defaults on
  Linux. `GDK_BACKEND` is left unset in the session, so an application tries Wayland first and the
  X11 backend serves the ones that force it, under Xwayland (see
  [A graphical application](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-graphical-application)).
- `-Dbroadway-backend=false` — Build no Broadway backend, which draws a GTK program into a web
  browser over HTML5. It is upstream's default.
- `-Dmedia-gstreamer=enabled` — Build the GStreamer media backend behind `GtkVideo` and
  `GtkMediaFile`. It needs `gstreamer-play-1.0` from [gst-plugins-bad](#port-gst-plugins-bad) and
  the GL and allocator libraries of [gst-plugins-base](#port-gst-plugins-base). `enabled`, which
  is upstream's default, makes a missing one stop the configure.
- `-Dprint-cups=enabled` — Build the [cups](#port-cups) print backend, and fail if CUPS is not
  found. Upstream's `auto` builds without it.
- `-Dprint-cpdb=disabled` — No Common Print Dialog Backends backend. cpdb is not a port, and the
  option is off by default.
- `-Dvulkan=enabled` — Build the Vulkan renderer. The configure requires the
  [vulkan-loader](#port-vulkan-loader) and `glslc` from [shaderc](#port-shaderc), which compiles the
  renderer's shaders at build time. It is upstream's default.
- `-Dcloudproviders=disabled`, `-Dsysprof=disabled`, `-Dtracker=disabled`, `-Dcolord=disabled`,
  `-Daccesskit=disabled` — No cloud-provider entries in the file chooser (libcloudproviders), no
  sysprof tracing, no Tracker search in the file chooser, no colord support in the CUPS backend,
  and no AccessKit accessibility backend; accessibility goes through AT-SPI. None of these
  libraries is a port, and each option is off by default.
- `-Dman-pages=true` — Build the manual pages of the installed tools, such as
  `gtk4-builder-tool`, with `rst2man` from [python3-docutils](#port-python3-docutils).
- `-Dbuild-testsuite=false`, `-Dbuild-demos=false`, `-Dbuild-examples=false`,
  `-Dbuild-tests=false` — Build none of the test suite, `gtk4-demo`, `gtk4-widget-factory` or the
  examples, all on by default upstream.
