## About
[Qwt]{.idx}, Qt Widgets for Technical Applications, is a library of plot, dial, slider, thermometer
and scale widgets for Qt programs. This port builds it for Qt 6, as `libqwt-qt6`, with its headers
in `/usr/include/qwt`, its Qt Designer plugin, and its qmake feature files, so a qmake project can
say `CONFIG += qwt`. [qgis](#port-qgis) links it. The Qt 5 build is the separate
[qwt-qt5](#port-qwt-qt5) port, from the same source, and the two install side by side.

## Flags
- `QMAKE_CFLAGS_RELEASE="$CFLAGS"` — with `QMAKE_CXXFLAGS_RELEASE` and `QMAKE_LFLAGS_RELEASE` on
  the qmake line: replace the release flags of Qt 6's mkspec with the tree's, so the library is
  compiled and linked like every other port.

## Patches
- `10_install_paths.patch` — sets Qwt's install prefix to `/usr` in place of upstream's `/usr/local/qwt-<version>`, its headers to `include/qwt`, its documentation to `share/doc/qwt`, and moves the Designer plugin and the qmake feature files under `lib`.
- `20_fix_rpath.patch` — removes the line of `designer/designer.pro` that compiles the library's install directory into the Designer plugin as an `RPATH`. The library is in `/usr/lib`, where the loader looks anyway.
- `30_multibuild.patch` — names the library after the Qt it is built for, `libqwt-qt6` with the soname `libqwt-qt6.so.<major>.<minor>`, everywhere it is built and linked, and puts the Designer plugin and the feature files under that Qt's own directories, `/usr/lib/qt6/plugins/designer` and `/usr/lib/qt6/mkspecs/features`. This is what lets this port and `qwt-qt5` install together.

## Notes
`build.sh` ends by checking that `libqwt-qt6.so` and `/usr/include/qwt/qwt_plot.h` are in the
package, which fails the build if the patched names or paths did not take.

Upstream's `qwtconfig.pri` turns on its examples, playground and tests, so the build compiles
them; none of them is installed.
