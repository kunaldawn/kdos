## About

glib-introspection is the introspection data for GLib's own libraries: the GIR files and typelibs
`GLib-2.0`, `GLibUnix-2.0`, `GObject-2.0`, `GModule-2.0`, `Gio-2.0`, `GioUnix-2.0` and
`GIRepository-3.0`, under `/usr/share/gir-1.0` and `/usr/lib/girepository-1.0`. It is the second
build of [glib](#port-glib)'s tarball: glib is built with introspection off, because
[gobject-introspection](#port-gobject-introspection) needs glib first, and this port builds the
same source again once both are installed. A port that generates introspection data for a
GLib-based library names it in `depends`, because the GIR it writes includes GLib's, GObject's or
Gio's.

The arrangement is described in
[A second build of the same source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-second-build-of-the-same-source).

## Flags

- `-D introspection=enabled` — Generate the GIR and typelib files, with `g-ir-scanner` from the
  installed gobject-introspection. This and `man-pages` are the two switches that differ from
  glib's build.
- `-D man-pages=disabled` — Build no manual pages. This build packages none of them; they are the
  glib port's.

Every other switch is the glib port's, so that the introspection data describes the library that
is installed; see the [glib](#port-glib) page for what each one does.

## Notes

The package holds only the GIR and typelib files. `meson install` writes the whole of GLib into a
staging directory, `stage/`, inside the source tree, and `build.sh` copies the two sets of files
from there into `$PKG`. Packaging anything more would give two packages the same paths.

The recipe names glib's tarball under glib's own file name, `glib-<version>.tar.xz`, with the same
`sha256`, so the source archive and the cache hold one file for both ports. Its `version` must be
glib's, since a typelib describes the library installed beside it, and both recipes carry
`group = glib`, so the version checker offers the two only as one bump.
