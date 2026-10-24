## About
[PySide6]{.idx} is the Qt Company's set of Python bindings for Qt 6, and [Shiboken6]{.idx} is the
binding generator they are made with. The port builds both from one source tree: the `PySide6`
and `shiboken6` Python packages, `libpyside6` and `libshiboken6`, the `shiboken6` generator, their
headers, CMake packages and `pkg-config` files. In KDOS it is FreeCAD's scripting interface, and
Falkon's Python plugin support (`PyFalkon`), which Falkon's recipe makes required, is built
against it.

## Flags
- `-DMODULES` — the list of Qt modules to generate bindings for. Left unset, upstream builds every
  essential module and every optional one it finds installed; named, every module in the list is
  required. The list is fixed so that the set of bindings does not grow or shrink with whichever Qt
  modules happen to be installed when the port is built. It holds what FreeCAD imports, the rest of
  qtbase, `WebEngineCore` and `WebEngineWidgets`, whose typesystems Falkon's `PyFalkon` plugin
  loads, and `WebChannel`, which upstream's `WebEngineCore` binding depends on; the matching Qt
  ports are in `depends`.
- `-DNO_QT_TOOLS=yes` — stop pyside-tools from copying Qt's own `uic`, `rcc`, `designer`,
  `lupdate`, `qmllint` and the other Qt programs into `/usr/bin` beside its `pyside6-*` wrappers.
  Those programs are installed by [qt6-qtbase](#port-qt6-qtbase) and
  [qt6-qttools](#port-qt6-qttools).
- `-DNUMPY_INCLUDE_DIR` — the directory of numpy's C headers, read from the installed
  [numpy](#port-numpy). With it, `libshiboken` is compiled with `HAVE_NUMPY` and can convert numpy
  arrays; without it, CMake reports "NUMPY not found" and builds without that support.
- `-DSHIBOKEN_PYTHON_LIBRARIES` — the libraries `libshiboken` links for the Python C API, here
  `pkg-config --libs python3-embed`, the flags for `libpython`. Upstream leaves it empty except on
  Windows, and empty, the Python symbols are resolved from the interpreter that loads the module; set,
  `libshiboken` and everything linked to it carries a dependency on `libpython`.
- `-DBUILD_TESTS=OFF` — skip shiboken's and PySide's test suites, which upstream builds by
  default.
- `-DCMAKE_SKIP_INSTALL_RPATH=ON` — install the libraries and modules without the run path
  upstream sets, which names `$ORIGIN`, the Qt library directory and the `shiboken6` package
  directory. Everything they link is in `/usr/lib`.
- `-DPython_EXECUTABLE=/usr/bin/python3` — the interpreter CMake's `FindPython` uses, named
  rather than searched for, so the modules are built for the tree's Python.

## Patches
- `pc-dirs.patch` — in `pyside6.pc.in`, makes `libdir` absolute (`@LIB_INSTALL_DIR@` is the relative `lib`) and points `includedir` at `/usr/include/PySide6`, where `fix-header-install-dir.patch` puts the headers, instead of upstream's `/usr/PySide6/include`.
- `pyi-build-tree-shiboken.patch` — adds the build tree's `sources` directory to the `--sys-path` of the step that writes each module's `.pyi` stub. That step imports the module just built, and the module imports `shiboken6`, which upstream looks for in the installed site-packages; in a build of both packages from one tree, `shiboken6` is in the build tree's `sources` directory and is not installed yet.

## Notes
`LLVM_INSTALL_DIR=/usr` is read from the environment by shiboken's CMake to find libclang, which
the generator parses Qt's headers with. `ninja` runs with `PYTHONPATH` set to the build tree's
`sources` directory for the same reason as the `.pyi` patch: the build imports the `shiboken6`
package it has just built.

The install step writes a `PySide6Config.abi3.cmake` that names the build tree's typesystem and
glue directories. `build.sh` installs the build tree's own copy over it, which names the installed
ones, so that a CMake project using `find_package(PySide6)` (Falkon is one) finds the typesystems
under `/usr/share/PySide6`.

pyside-tools installs its deployment scripts, `deploy.py`, `project.py` and the rest, with their
`_lib` directories, straight into `/usr/bin`. `build.sh` moves them into the `scripts` directory of
the `PySide6` package, so that generic names such as `project.py` are not commands on the `PATH`.
