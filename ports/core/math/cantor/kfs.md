## About

Cantor is KDE's mathematical worksheet: a document of input cells and their results, rendered with
formulas and plots, run against a backend chosen per worksheet. Python, R, Qalculate, KAlgebra
and Lua are built in as plugins that link their libraries; Maxima, Octave, Sage and Scilab
backends are always built and drive the program itself, so each works once that program is
installed.

## Flags

- `-D BUILD_DOC=ON` — Build the handbook with [kdoctools](#port-kdoctools), which the option then
  requires. It is the option's own default, named in the recipe.
- `-D ENABLE_EMBEDDED_DOCUMENTATION=ON` — Build the panel that shows a backend's own documentation
  inside the worksheet. It requires Qt Help from [qt6-qttools](#port-qt6-qttools) and the
  [qt6-qtwebengine](#port-qt6-qtwebengine) widgets; on by default upstream.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Poppler=ON` — Make poppler's Qt 6 binding a requirement.
  Upstream looks for it as optional, but `cantorlibs` links `Poppler::Qt6` unconditionally, so a
  missing one would fail only at link time.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_Analitza6=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_R=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_Qalculate=ON`, `-D CMAKE_REQUIRE_FIND_PACKAGE_Python3=ON`,
  `-D CMAKE_REQUIRE_FIND_PACKAGE_LuaJIT=ON` — The backends that link a library: KAlgebra through
  [analitza](#port-analitza), R through `libR.so` from [R](#port-R), Qalculate through
  [libqalculate](#port-libqalculate), Python through [python3](#port-python3), and Lua through
  [luajit](#port-luajit). Upstream builds each only when its library is found; required, a missing
  one stops the configure.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Julia=ON` — Never build the Julia backend. Julia is not a port,
  and the switch keeps the result from depending on what is installed.

## Notes

`build.sh` replaces upstream's desktop entry. Upstream's runs `cantor -qwindowicon cantor
-qwindowtitle %c %U`; the replacement runs `cantor %U` and sets `StartupWMClass=org.kde.cantor`.
`application/x-cantor` is the type defined by the `cantor.xml` the port installs, and `build.sh`
checks afterwards that it and the 48-pixel `cantor.png` icon the entry names are in the package.

The Markdown cells are rendered by a patched copy of Discount that upstream carries in
`thirdparty/` and builds from the source tarball.
