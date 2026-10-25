## About
[QCAD]{.idx} is a 2D CAD program for technical drawings, plans and parts, working in DXF and
reading DWG. Most of its interface and tools are written in ECMAScript, run by `QJSEngine` from Qt 6's QML
module through two bindings layers released beside it, `qtjsapi` and `qcadjsapi`; the port builds
the community edition with all three, which is why the recipe has three sources at one version.

## Flags
- `-DBUILD_QT6=ON` — build against Qt 6. It is upstream's default, named because the main tree's
  `CMakeInclude.txt` selects Qt 5 when it is off, while both bindings layers find Qt 6
  unconditionally.
- `-DCMAKE_SKIP_RPATH=ON` — write no RPATH into any binary. `build.sh` copies the program and its
  libraries straight out of the build directories, with no install step to rewrite them, so CMake's
  build-tree RPATH would ship pointing into the work directory, which the
  [package lint](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)
  refuses. The libraries are found through the wrapper described below instead.

## Patches
- `musl-fcloseall.patch` — in the bundled openNURBS (`src/3rdparty/opennurbs/opennurbs_defines.cpp`), `ON::CloseAllFiles()` returns `EOF` on a Linux C library that is not glibc, the branch upstream already takes on macOS, Android and WebAssembly. `fcloseall()` is a GNU extension that musl does not declare, so the unpatched call does not compile.
- `no-execinfo.patch` — in `src/core/RDebug.cpp`, the `<execinfo.h>` include and the body of `RDebug::printBacktrace()` are compiled only when `__has_include(<execinfo.h>)` holds. musl has no such header, so on KDOS the function prints nothing.

## Notes
The two bindings layers are separate CMake projects that find the main tree by relative path,
`../qcad`, and write their libraries back into it: `qtjsapi` into `release/` and `qcadjsapi` into
`plugins/`. `qcadjsapi` also reads `../qcad/CMakeInclude.txt` and the headers under `../qcad/src`
and `../qtjsapi`. kpkg unpacks the three sources beside each other as `qcad-<version>`,
`qtjsapi-<version>` and `qcadjsapi-<version>`, so `build.sh` first makes the symbolic links
`qcad` and `qtjsapi` in `$SRC_ROOT` that those paths name. The order matters: `qcadjsapi` links
the main tree's libraries (`qcadcore`, `qcadgui` and the rest) and `qtjsapi`'s, so it is built
after both.

Without the two layers QCAD starts with no menus, since its menus and tools are scripts.

The scripts are compiled into the `qcadscripts` plugin as Qt resources; the `.qrc` lists are
regenerated at build time by `src/scripts/update_qrc.sh`, a bash script, which is why
[bash](#port-bash) is in `depends`.

Everything is installed under `/usr/lib/qcad` as upstream lays out a run-in-place tree: `qcad-bin`,
its libraries, `plugins/`, and the fonts, patterns, line types, part libraries, themes and
examples it reads relative to itself. `/usr/bin/qcad` is a small wrapper that puts
`/usr/lib/qcad` on `LD_LIBRARY_PATH` and runs `qcad-bin`. `fonts/qt`, a set of DejaVu TrueType
faces that upstream's documentation offers as a `QT_QPA_FONTDIR` for running QCAD in a server
environment, is removed; nothing in the program loads it by itself, and on the desktop Qt finds
fonts through fontconfig. QCAD's own CXF stroke fonts in `fonts/` stay.
