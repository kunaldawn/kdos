## About

timg shows images, animations, PDF pages and video in a terminal. It draws with the kitty or
iTerm2 graphics protocols or sixel where the terminal speaks one, and with coloured Unicode block
characters everywhere else. Its desktop entry opens PNG, JPEG, GIF, WebP, BMP and TIFF in
[kdos-term](#port-kdos-term), which draws them as pictures; GIF, BMP and TIFF are the three of
those that `kdos-pix` does not open (see
[`kdos-pix`](../../../../docs/kdos/04-programs/kdos-shell.md#kdos-pix)).

## Flags

- `-DTIMG_VERSION_FROM_GIT=Off` — Takes the version from the project's own `CMakeLists.txt`
  instead of running `git describe`, which on a release archive would need git and find no
  repository.
- `-DWITH_GRAPHICSMAGICK=On` — The general image loader, through
  [graphicsmagick](#port-graphicsmagick)'s Magick++, which opens most formats the specific
  loaders below do not.
- `-DWITH_STB_IMAGE=On`, `-DWITH_QOI_IMAGE=On` — The self-contained STB loader, the fallback for
  common formats, and the QOI format. Neither has a port: each uses its header from
  `third_party/` when no system copy is found.
- `-DWITH_TURBOJPEG=On` — Fast JPEG loading through [libjpeg-turbo](#port-libjpeg-turbo). The
  same branch requires [libexif](#port-libexif), which reads the orientation tag, so a photo taken
  sideways is turned upright.
- `-DWITH_RSVG=On` — SVG through [librsvg](#port-librsvg) and cairo.
- `-DWITH_POPPLER=On` — PDF through [poppler](#port-poppler)'s GLib binding. Each page is a frame,
  so `--frame-offset` and `--frames` choose the pages.
- `-DWITH_LIBSIXEL=On` — Sixel output through [libsixel](#port-libsixel), for terminals that speak
  neither graphics protocol.
- `-DWITH_VIDEO_DECODING=On` — Video and animated formats through [ffmpeg](#port-ffmpeg)'s
  libraries. With it on, every image is also scaled through libswscale instead of STB's
  resizer.
- `-DWITH_VIDEO_DEVICE=On` — Reading from video devices through libavdevice, which makes
  `timg /dev/video0` a live camera preview in the terminal.
- `-DWITH_OPENSLIDE_SUPPORT=Off` — No whole-slide microscopy formats; OpenSlide is not a port.
  Upstream's default, named.

Every `On` here is upstream's default, named. Each option that is on and needs a library finds it
with a required `pkg_check_modules`, so a missing one stops configure rather than building a timg
that cannot open that format; STB and QOI need none.

## Notes

The desktop entry is written in `build.sh`. It is `Terminal=true` with
`X-KDOS-Term=kdos-term`, so the launcher opens it in the terminal that draws pictures rather
than the session's own (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)),
and its menu name is `Image Viewer (timg)`.
