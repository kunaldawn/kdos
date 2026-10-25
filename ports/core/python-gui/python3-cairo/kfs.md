## About

pycairo is the Python binding for the [cairo](#port-cairo) 2D drawing library: the `cairo` module
with its surfaces, contexts and patterns. Besides programs that draw with it directly, it is the
cairo half of [python3-gobject](#port-python3-gobject): GTK's drawing callbacks hand Python a
`cairo.Context`, and PyGObject converts it through the C API that pycairo installs as a header
and the `py3cairo.pc` pkg-config file.

## Flags

- `-Dpython=python3` — The interpreter to build the extension for and install it under. The
  value is upstream's default.

## Notes

The two `test` lines after the install stop the build unless the package holds the compiled
`_cairo.*.so` module and `/usr/lib/pkgconfig/py3cairo.pc`. python3-gobject looks for pycairo first
by locating the `cairo` module and the `include/` directory installed beside it, and falls back to
`py3cairo.pc`. The pkg-config file points at the second copy of the header, under
`/usr/include/pycairo`.
