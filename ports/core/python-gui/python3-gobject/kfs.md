## About

PyGObject is the Python binding for GObject-based libraries. It contains no per-library code:
the `gi` module reads the typelibs that [gobject-introspection](#port-gobject-introspection)
generates and builds Python classes from them at import, so GLib, GTK, GStreamer and every other
introspected library are reachable from Python through `gi.repository`. Python applications on
GTK, and the Python tools that talk D-Bus through GLib, depend on it.

## Flags

- `-Dpython=python3` — The interpreter to build the extension for. The value is upstream's
  default.
- `-Dpycairo=enabled` — Build `gi._gi_cairo`, the converter between cairo's GObject types and
  [python3-cairo](#port-python3-cairo)'s Python objects, which a GTK drawing callback written in
  Python needs. The option's default is `auto`, which drops the module when cairo or pycairo is not
  found; `enabled` turns that into a failed configure.
- `--wrap-mode=nofallback` — The source ships wraps for `glib`, `libffi`, `gobject-introspection`
  and `pycairo`; with this mode none of them is downloaded and built in place of the port.

## Notes

The second `test` line stops the build unless `_gi_cairo.*.so` is in the package, so a pycairo
that meson found but could not use cannot ship a PyGObject without cairo support.

`pythoncapi-compat`, a header-only compatibility layer for the CPython C API, is built from the
copy upstream includes under `subprojects/` in the release tarball. It is the one subproject the
build uses, and `--wrap-mode=nofallback` does not stop it, because `meson.build` calls it with
`subproject()` rather than as a `dependency()` fallback.
