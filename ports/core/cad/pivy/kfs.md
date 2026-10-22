## About

Pivy is the Python binding for [coin](#port-coin), the Open Inventor scene-graph library, and for
[soqt](#port-soqt), its Qt viewer. It is generated with [swig](#port-swig). In KDOS it is there
for [freecad](#port-freecad), whose workbenches build and change the 3D view through Pivy's
`coin` module.

## Flags

- `-DPyInt_AsLong=PyLong_AsLong` — With the five definitions after it in the same `CXXFLAGS`
  export, maps the Python 2 calls `PyInt_AsLong`, `PyInt_FromLong`, `PyString_Check`,
  `PyString_FromString`, `PyString_AsString` and `PyString_Size` onto their Python 3 equivalents.
  Pivy's typemaps call these names outside their Python 2 branches, and the code SWIG 4.3 and
  later generates does not define them; the flags supply the same mappings SWIG 4.2 made, for the
  names pivy uses.
- `-DPIVY_USE_QT6=ON` — Build `pivy.gui.soqt` against Qt 6 rather than upstream's default Qt 5. It must match the Qt that
  [soqt](#port-soqt) is built with: SoQt's CMake configuration is found first, and pivy then needs
  the same Qt's widgets.
- `-DDISABLE_SWIG_WARNINGS=ON` — Silence SWIG's warnings while generating the wrappers.
- `-DPython_EXECUTABLE=/usr/bin/python3` — Build and install the modules for the `python3` port.

## Patches

- `swig-4.4-init.patch` — SWIG 4.4 generates a multi-phase module initialisation, whose exec function returns an `int`. The `%init` block in `interfaces/soqt.i` returns `NULL`, the failure value of a single-phase init, when the file emulator fails to start, and that code lands inside the `int` function. The patch gives it the `SWIG_VERSION` switch `coin.i` already has, returning `-1`, an exec function's failure value, under SWIG 4.4 and later. (`coin.i` returns `0` in the same place.)
