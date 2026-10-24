## About

[wlroots]{.idx} is the library a Wayland compositor is built from: the backends that talk to the
display controller and the input devices, the renderers, buffer allocation, and the server half
of most Wayland protocols. `kdos-comp`, a hard fork of the labwc compositor, links it, and it is
the only port that does; see [kdos-comp](../../../../docs/kdos/04-programs/kdos-comp.md).

wlroots changes its API with every release, and `kdos-comp`'s `meson.build` asks for it by a
pkg-config name that carries the release series. A bump of this port to a new series is therefore
a change to `kdos-comp` in the same change.

The recipe also installs the tarball's `protocol/*.xml` into `/usr/share/wlroots/protocols/`,
which upstream's install does not. `kdos-shell`, `kdos-term`, `kdos-res` and `kdos-lock`
generate their wlr protocol code from there (layer-shell, foreign-toplevel management, and in
`kdos-shell`'s case output management and data control); that is why they depend on this port,
and none of them links it.

## Flags

- `-Dexamples=false` — Do not build the example compositors and clients, which upstream builds by
  default.
- `-Dbackends=drm,libinput` — Build the DRM backend, which drives displays through the kernel's
  mode setting, and the libinput backend for keyboards, pointers and touch. The third choice,
  `x11`, which runs the compositor as a window on an X server, is left out: KDOS has
  [no X server](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out)
  other than Xwayland, which runs under the compositor. The headless backend is always built,
  chosen at run time with `WLR_BACKENDS=headless`, and naming it here fails configure.
- `-Drenderers=gles2,vulkan` — Build the OpenGL ES 2 and Vulkan renderers. The Vulkan renderer's
  shaders are compiled to SPIR-V at build time by `glslang`, which is why that port is a
  dependency. The pixman software renderer is always built, chosen with `WLR_RENDERER=pixman`, and
  naming it fails configure.
- `-Dallocators=gbm,udmabuf` — Build the GBM allocator, from Mesa, and the udmabuf allocator.
  Named rather than left at `auto`, a missing piece stops configure: udmabuf needs
  `memfd_create()`, `F_ADD_SEALS` and `<linux/udmabuf.h>`.
- `-Dsession=enabled` — Build session support, which takes the seat and opens the DRM and input
  devices through libseat (the `seatd` port) and finds them through libudev (`eudev`). The DRM
  and libinput backends cannot work without it, and `enabled` makes a missing library a configure
  error.
- `-Dxwayland=enabled` — Build Xwayland support, against `xwayland`, `libxcb`, and the
  `xcb-ewmh` and `xcb-icccm` libraries of `xcb-util-wm`; `enabled` makes a missing one a configure
  error. `kdos-comp` is configured with Xwayland on and its
  build stops with `no wlroots Xwayland support` when wlroots lacks it. Xwayland is how an
  X11-only program runs on this desktop; see
  [Xwayland](../../../../docs/kdos/04-programs/kdos-comp.md#xwayland).
- `-Dcolor-management=enabled` — Link Little CMS (`lcms2`), so a colour transform can be built
  from an ICC profile. Without it wlroots compiles a stand-in that refuses every ICC profile with
  `LCMS2 is compile-time disabled`.
- `-Dlibliftoff=disabled` — Build without libliftoff, the library the DRM backend can use to place
  surfaces on the display controller's hardware planes. It is not a port here.
- `-Dxcb-errors=disabled` — Build without the xcb-errors utility library, which Xwayland support
  uses to print X11 errors by name. It is not a port here.

## Patches

- `idle-capture-frame.patch` — In `types/wlr_ext_image_copy_capture_v1.c`, a capture request asks the capture source for a new frame every time, not only when the session has damage waiting. Without it, a capture of a screen where nothing moves is answered once, for the damage a new session starts with, and then never again, so a recording of a still desk receives one frame and `pipewiresrc` drops the empty cycles after it. wlroots' screencopy implementation already asks unconditionally. The cost is one output commit per capture of an unchanged screen, bounded by the client's capture rate; `kdos-record` relies on it, see [Recording](../../../../docs/kdos/03-architecture/session.md#recording).
