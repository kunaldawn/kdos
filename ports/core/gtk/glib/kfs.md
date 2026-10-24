## About

GLib is the C utility library under GTK and most of the GNOME and freedesktop software stack. One
source tree builds several libraries: `libglib-2.0` (data structures, the main loop, Unicode and
string handling), `libgobject-2.0` (the GObject type and object system), `libgmodule-2.0`,
`libgio-2.0` (files, sockets, D-Bus, settings and application support) and `libgirepository-2.0`.
It also installs the tools other builds run, such as `glib-compile-resources`,
`glib-compile-schemas`, `gdbus-codegen` and `glib-mkenums`, and command-line programs such as
`gio`, `gdbus` and `gsettings`. It is one of the libraries the most ports in the tree name in
`depends`.

GLib is built twice from the same tarball. This port builds it with introspection off, because
[gobject-introspection](#port-gobject-introspection) needs GLib to exist first;
[glib-introspection](#port-glib-introspection) builds the same source again, once both are
installed, and packages only the GIR and typelib files. The split, and why its two versions must
stay equal, is in
[A second build of the same source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-second-build-of-the-same-source).

## Flags

- `-D introspection=disabled` — Generate no GIR or typelib files; they are glib-introspection's,
  and generating them needs gobject-introspection, which is built after this port.
- `-D wrap_mode=nodownload` — Never download a meson subproject. The release archive ships wrap
  files for [pcre2](#port-pcre2), [libffi](#port-libffi), [zlib](#port-zlib) and others; with
  downloads forbidden, a missing one stops the configuration instead of being fetched and built
  inside the port.
- `-D libmount=enabled` — Read mount information through `libmount` from
  [util-linux](#port-util-linux), which GIO's mount monitoring uses. Named `enabled`, a missing
  libmount is an error rather than a GIO built without it.
- `-D libelf=enabled` — Link [elfutils](#port-elfutils)' `libelf`, so that the `gresource` tool can
  list and extract the resources compiled into an ELF file.
- `-D selinux=disabled` — Build no SELinux support; the tree has no `libselinux`.
- `-D glib_debug=disabled` — Leave out GLib's debug infrastructure, which upstream's own option
  text says distributions typically want disabled in production.
- `-D dtrace=disabled`, `-D systemtap=disabled`, `-D sysprof=disabled` — Build none of GLib's
  tracing hooks. None of the three tracers is in the tree.
- `-D nls=disabled` — Build no message catalogues. The image ships English only, and the package
  policy would remove the catalogues anyway (see
  [Size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D documentation=false` — Build no API reference, which needs gi-docgen, not a port. The manual
  pages are built (`-D man-pages=enabled`).

## Notes

The two GLib ports carry `group = glib`, so the version checker offers them only as one bump, and
glib-introspection passes this port's meson options, with `introspection` turned on and the manual
pages, which it does not package, turned off. A switch changed here without the same change there
leaves the installed typelibs describing a different library from the one installed.
