## About

gThumb is GNOME's photo and video browser: a thumbnail browser, a viewer, catalogues and tags,
import from removable devices and folders, and simple editing. It is built for GTK 4 and
libadwaita.

## Flags

- `CFLAGS="$CFLAGS -std=gnu23"` — The sources are C23: they use `bool` without `<stdbool.h>`, and
  the project names no standard because GCC's own default is GNU C23. The tree's `-std=gnu11`
  takes that default away, and the last `-std=` on a command line is the one GCC uses (see
  [A source written for C23](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-source-written-for-c23)).
- `-Ddeveloper-mode=false` — Upstream's default: no debug code or tests, and no resources loaded
  from the source tree.
- `-Dflatpak-build=false` — Upstream's default: an ordinary install, not a flatpak one.
- `-Dcolord=false` — Do not look for colord, which reads the monitor's colour profile. colord is
  not a port, so images are shown untransformed for the display.
- `-Dlibwebp=true` — Each format option, `-Dlibwebp`, `-Dlibrsvg`, `-Dlibheif`, `-Dlibjxl`,
  `-Dlibtiff`, `-Dlibgif` and `-Dlibraw`, enables a probe for that format's library, and each
  library is in `depends`. They are upstream's defaults, written out. The probes are not required:
  a missing library still configures and builds a gThumb without that loader, and only the
  `meson setup` summary shows it.

## Notes

GStreamer is a quiet probe as well, with no option: without
[gstreamer](#port-gstreamer) and its plug-in ports, videos show only as file icons.

Upstream installs only an SVG application icon. The loop after the install rasterises it with
`rsvg-convert` into hicolor PNGs at four sizes, because the panel draws application icons from
hicolor PNGs only (see
[Desktop entries belong to the port](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).

The desktop entry is written over upstream's. `StartupWMClass` is the GApplication id, which is
the Wayland app id. The entry carries no `MimeType`: the image types already have a handler, and a
second claim would make the default whichever entry sorts first.
