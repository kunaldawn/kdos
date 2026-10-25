## About
This is [Qwt](#port-qwt), the plot, dial and scale widget library, built for Qt 5 from the same
source as the Qt 6 port: `libqwt-qt5`, its headers in `/usr/include/qwt-qt5`, the Qt 5 Designer
plugin, the qmake feature files under `/usr/lib/qt5/mkspecs/features` and the pkg-config module
`Qt5Qwt6`. [gnuradio](#port-gnuradio)'s `gr-qtgui` links it.

## Flags
- `QMAKE_CFLAGS_RELEASE="$CFLAGS"` — with `QMAKE_CXXFLAGS_RELEASE` and `QMAKE_LFLAGS_RELEASE` on
  the qmake line: the tree's compile and link flags as the release flags.

## Patches
- `10_install_paths.patch` — the same patch as the Qt 6 port's: Qwt's install prefix `/usr` in place of `/usr/local/qwt-<version>`, its documentation in `share/doc/qwt`, and the Designer plugin and qmake feature files under `lib`.
- `20_fix_rpath.patch` — removes the `RPATH` that `designer/designer.pro` compiles into the Designer plugin; the library is in `/usr/lib`, where the loader looks anyway.
- `30_multibuild.patch` — names the library `libqwt-qt5`, with the soname `libqwt-qt5.so.<major>.<minor>`, and puts the Designer plugin and the feature files under `/usr/lib/qt5`, so the two builds take no name from each other.
- `40_headers_per_qt.patch` — the one patch the Qt 6 port does not carry: it installs the headers in `include/qwt-qt5` rather than `include/qwt`, which the Qt 6 build keeps.

## Notes
The source is the same release archive as [qwt](#port-qwt)'s. The `::` form of `source =` saves
it as `qwt-<version>.tar.bz2`, the name the Qt 6 port's file has, instead of a name built from this
port's own.

`build.sh` ends by checking that `libqwt-qt5.so`, `/usr/include/qwt-qt5/qwt_plot.h` and
`Qt5Qwt6.pc` are in the package. As with the Qt 6 port, the build compiles Qwt's examples,
playground and tests and installs none of them.
