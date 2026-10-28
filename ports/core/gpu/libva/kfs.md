## About

libva is the VA-API loader: the library a program calls to decode or encode video on the GPU. It
opens a display (a DRM render node or a Wayland connection), loads the VA-API driver for the GPU
behind it from `/usr/lib/dri` (`iHD` or `i965` for Intel, Mesa's Gallium driver for AMD, nouveau
and virgl) and passes every call through. ffmpeg, mpv, GStreamer, Kodi, VLC, OBS Studio, Qt
Multimedia, Qt WebEngine and Chromium reach the hardware decoders through it. Which driver serves
which GPU, and how to see what a machine offers, is in
[Media, colour and time](../../../../docs/kdos/02-user-guide/administration.md#media-colour-and-time).

## Flags

- `-Dwith_wayland=yes` — Builds `libva-wayland`, the Wayland display, and makes
  [wayland](#port-wayland) a requirement rather than a probe.
- `-Ddisable_drm=false` — Builds `libva-drm`, the display opened on a DRM render node with no
  window system at all. This is upstream's default, named here. The option is a boolean,
  `disable_drm`, where the others are `with_*` choices, and meson refuses an option name it does
  not know.
- `-Dwith_x11=no` — Builds no `libva-x11`, the X11 display. Every consumer here reaches VA-API
  through a DRM render node or a Wayland display: ffmpeg through DRM, mpv built with
  `-Dvaapi-x11=disabled`, Kodi on its Wayland platform, and Chromium, whose VA-API wrapper loads
  `libva` and `libva-drm` alone. A program under Xwayland that wants `libva-x11` gets no hardware
  decoding. On `auto`, the default, the X11 display would be built whenever [libX11](#port-libX11),
  libXext, libXfixes and libxcb with its DRI3 extension are in the root, none of which is in this
  port's `depends`.
- `-Dwith_glx=no` — Builds no `libva-glx`, the GLX texture interop, which needs the X11 display.
- `-Dwith_win32=no` — Builds no Windows display; on `auto` the probe finds nothing on Linux.
- `-Denable_docs=false` — Builds no Doxygen API reference, upstream's default.

## Notes

libva tries the Intel drivers in order, `iHD` from [intel-media-driver](#port-intel-media-driver)
before `i965` from [libva-intel-driver](#port-libva-intel-driver), so each machine loads the one
that serves it. When no driver loads, ffmpeg and mpv fall back to software decoding without saying
so; `vainfo`, from [libva-utils](#port-libva-utils), shows which profiles the loaded driver offers.
