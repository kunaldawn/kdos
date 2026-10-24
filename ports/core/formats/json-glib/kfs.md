## About
JSON-GLib reads and writes JSON for GLib and GObject programs: a parser and generator built on
GLib's types and main loop, and a serializer between JSON and GObject properties. It also installs
`json-glib-format` and `json-glib-validate`. GNOME-family programs such as [gimp](#port-gimp),
[gegl](#port-gegl), [darktable](#port-darktable), [geoclue](#port-geoclue) and
[libblockdev](#port-libblockdev) link it.

## Flags
- `-Dintrospection=enabled` — Generate the `Json-1.0` GIR and typelib, so the library can be used
  from Python, JavaScript and other languages through GObject introspection. Named `enabled`, a
  missing `g-ir-scanner` is a configure error rather than an omission. Both
  [gobject-introspection](#port-gobject-introspection) and
  [glib-introspection](#port-glib-introspection) are in `depends`, because the GIR includes GLib's
  and GObject's (see
  [A second build of the same source](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#a-second-build-of-the-same-source)).
- `-Dnls=enabled` — Build and install the message catalogs; named `enabled`, a missing `xgettext`
  is a configure error. kpkg removes every catalog but the English ones (see the
  [size and language policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so the package carries only `en_GB`.
- `-Ddocumentation=disabled` — Build no API reference; it needs gi-docgen.
- `-Dgtk_doc=disabled` — The deprecated name meson maps onto `documentation`, set to the same
  value.
- `-Dman=true` — Build the manual pages of the two tools from their reStructuredText sources with
  `rst2man`, from [python3-docutils](#port-python3-docutils). Off by upstream's default; on, a
  missing `rst2man` is a configure error.
- `-Dtests=false` — Build no test programs.
