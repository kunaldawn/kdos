## About

Uranium is Ultimaker's Python application framework for 3D printing software: the scene graph,
mesh handling, settings containers, plugin system and Qt Quick interface (through
[python3-pyqt6](#port-python3-pyqt6)) that [cura](#port-cura) is built on. It installs no
program of its own; Cura's launcher imports it.

## Notes

Uranium's `CMakeLists.txt` installs only through paths that Conan, the C++ package manager Ultimaker
builds with, provides, so `build.sh` lays the framework out by hand where Uranium itself looks for it relative
to the launcher's prefix (`/usr`, the parent of `/usr/bin`): the `UM` package in `site-packages`,
the plugins under `/usr/lib/uranium/plugins`, and the resources under
`/usr/share/uranium/resources`. The three `test` lines at the end stop the build unless the QML
module's `qmldir`, the STL reader's `plugin.json` and the two vendored modules are in place.

Three things are left out on purpose. The `UpdateChecker` plugin, which asks ultimaker.com for
new releases at every start, is not copied. The translation catalogues under
`resources/i18n` are removed, so the interface keeps its English source strings. The `tests` and
`__pycache__` directories are removed, and `compileall` then writes fresh byte code with the
package root stripped from the recorded paths (`-s "$PKG" -p /`).

The vendor bundle holds the four modules only Uranium imports, named in `pypackages` with their
versions: `pyclipper` (polygon offsetting), `numpy-stl` with its `python-utils` (the STL reader)
and `colorlog`. Their build backends are ports, so the install runs with
`--no-build-isolation` and fetches nothing; see
[The three Python keys](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#the-three-python-keys).
numpy-stl's three command-line converters (`stl`, `stl2ascii`, `stl2bin`) are deleted, and the
`rmdir` of `/usr/bin` after them fails the build if a vendored module ever installs a program
there.
