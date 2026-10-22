## About

ZBar reads barcodes and QR codes: `zbarimg` from an image file and `zbarcam` from a camera, each
printing the decoded text, with the `libzbar` library under both. It is the other direction from
[qrencode](#port-qrencode): `zbarimg` reads a QR code out of a screenshot, so a code on the screen
becomes text without a phone, and `zbarcam` reads one held up to the camera.

## Flags

- `autoreconf -f -i` — The source is GitHub's archive of the release tag, which carries
  `configure.ac` and no generated `configure`.
- `XML_CATALOG_FILES=/etc/xml/catalog` — Names the XML catalogue libxml2 reads. The manual pages
  are DocBook rendered by `xmlto`, and the stylesheet is named by its sourceforge URL; only
  [docbook-xsl](#port-docbook-xsl)'s catalogue entry maps that to the installed copy, and without it
  `xsltproc` tries to fetch it and the build stops inside `xmlto`.
- `--without-gtk`, `--without-qt` — Build no GTK or Qt widgets. They draw zbar's X overlay into an
  X window id, which a Wayland-native GTK or Qt window does not have, and upstream marks its Qt 6
  widget broken.
- `--without-java`, `--without-python` — No Java or Python bindings.
- `--without-gir` — No introspection data; it describes the GTK widget, which is off.
- `--with-x` — Build the X11 viewfinder of `zbarcam`, the one front end built. Without it
  `zbarcam` runs only with `--nodisplay` and the camera is aimed blind; with it the picture is an
  X window under Xwayland.
- `--with-xshm` — Use the MIT shared-memory extension of [libXext](#port-libXext), so the picture
  is not copied through the X socket.
- `--without-xv` — No XVideo; the XImage path draws the picture, and XVideo under Xwayland depends
  on the GPU driver.
- `--enable-video` — Camera support through Video4Linux 2.
- `--with-imagemagick`, `--without-graphicsmagick` — Read image files through
  [imagemagick](#port-imagemagick)'s MagickWand. With the GraphicsMagick fallback left at its
  default, `check`, a MagickWand that pkg-config does not find turns image scanning off with only a
  notice and `zbarimg` is not built; ruled out, a missing ImageMagick stops the configure.
- `--with-jpeg` — JPEG image conversions in libzbar, through
  [libjpeg-turbo](#port-libjpeg-turbo); upstream's default, `check`, uses it only if found.
- `--with-dbus` — Let `zbarcam` and `zbarimg` publish each decoded code on D-Bus, as
  `org.linuxtv.Zbar`, with the bus policy file installed for it.
- `--enable-doc` — Build the manual pages.
- `XMLTO=xmlto` — Name `xmlto` instead of searching for it. `--enable-doc` only asks, and a
  missing `xmlto` turns the manual pages off with no message; named, the program is run at build
  time and its absence stops the build.
- `ac_cv_header_libv4l2_h=yes` — Take `libv4l2.h` as present. `zbarcam` reads a camera through
  libv4l2 when configure finds that header, which converts the pixel formats many webcams deliver
  and zbar cannot read raw; there is no switch for it. Preset, configure goes on to
  `PKG_CHECK_MODULES([V4L2], [libv4l2])`, which fails when [v4l-utils](#port-v4l-utils) is
  missing, instead of building a `zbarcam` that cannot see those cameras.

## Notes

configure detects X rather than requiring it, so `build.sh` fails the build when the generated
`include/config.h` defines `X_DISPLAY_MISSING`: without libX11 `zbarcam` would have no window.

The desktop entry that `build.sh` writes runs `zbarcam` in a terminal (`Terminal=true`): the camera
picture is its own window, and each code it reads is printed where it can be copied.
