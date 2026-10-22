## About

mpdecimal is a C library for arbitrary-precision decimal floating-point arithmetic, rounded
correctly: a complete implementation of IBM's General Decimal Arithmetic Specification. It installs
`libmpdec`, the C library, and `libmpdec++`, a C++ wrapper around it, with a `.pc` file for each.

In KDOS it exists for one consumer: the [python3](#port-python3) port links its `_decimal` module,
the C implementation of Python's `decimal`, against `libmpdec`. CPython carries a copy of the
library in its own source tree, `Modules/_decimal/libmpdec`; python3 is configured with
`--with-system-libmpdec` so that the module uses this port's shared library instead. This port is
built with upstream's defaults: its configure line names only the prefix.

## Notes

python3's configure does not stop when it cannot find this library. It warns, "no system libmpdec
found; falling back to bundled libmpdec", and compiles the copy from its own source into `_decimal`.
A python3 built without this port installed first therefore still works, but carries the bundled
copy, and the build does not fail. The `depends` line of python3 names `mpdecimal`, so the phase
installs it first; to confirm a build used it, check that `_decimal`'s `.so` under
`/usr/lib/python3.*/lib-dynload` has a `NEEDED` entry naming `libmpdec` (`readelf -d`).
