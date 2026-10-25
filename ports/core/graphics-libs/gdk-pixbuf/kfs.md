## About

GdkPixbuf is GNOME's image-loading library: it decodes an image file into a `GdkPixbuf`, an
in-memory RGB or RGBA image, through a set of loaders, one per format, and scales and composites
the result. GTK 3 and GTK 4 load every icon and picture through it, as do
[libnotify](#port-libnotify), [librsvg](#port-librsvg) (whose SVG loader plugs into it) and the
many other ports that name it. Its commands include `gdk-pixbuf-query-loaders`, which writes the cache of
installed loaders, and `gdk-pixbuf-thumbnailer`.

## Flags

- `--wrap-mode=nodownload` — Never download a meson subproject. The release archive carries wrap
  files for glib, libpng, libjpeg-turbo and gi-docgen; with downloads forbidden, a missing libpng
  or libjpeg stops the configuration instead of being answered by a copy fetched and built inside
  the port.
- `-D png=enabled`, `-D jpeg=enabled`, `-D tiff=enabled`, `-D gif=enabled` — Build the PNG, JPEG,
  TIFF and GIF loaders, named rather than left at `auto`, so a missing [libpng](#port-libpng),
  [libjpeg-turbo](#port-libjpeg-turbo) or [libtiff](#port-libtiff) is an error and not a library
  that cannot open the format.
- `-D others=enabled` — Build the loaders upstream calls "weakly maintained": ANI, BMP, ICNS, ICO,
  PNM, QTIF, TGA, XBM and XPM. They default to disabled; without them no program that loads images
  through GdkPixbuf opens those formats.
- `-D glycin=disabled` — Build no glycin loader, which hands decoding to the glycin library.
  glycin is not a port, and upstream's `auto` turns the loader on, as a required dependency, on
  Linux, so without the switch the configuration stops. With glycin off, the PNG and JPEG loaders
  are the ones built into the library, and the rest are modules.
- `-D introspection=enabled` — Generate `GdkPixbuf-2.0.gir` and its typelib. GTK's, libnotify's
  and librsvg's own introspection data include it, so their GIRs cannot be generated without it. It
  is generated against the GLib GIR files that [glib-introspection](#port-glib-introspection)
  installs, which is why that port is in `depends`.
- `-D installed_tests=false` — Install none of the test programs, which upstream installs by
  default.
