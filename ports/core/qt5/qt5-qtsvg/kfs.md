## About
Qt SVG renders SVG Tiny 1.2 drawings for Qt 5: the `QSvgRenderer` and `QSvgWidget` classes of the
QtSvg library, and the two plugins through which every Qt 5 program reads SVG without asking for
it, the image format plugin behind `QImage` and the icon engine behind `QIcon`. Without it a Qt 5
program draws no SVG icon from a theme.

## Notes
The build is qmake's plain shape: `/usr/lib/qt5/bin/qmake` writes the makefiles from the module's
`.pro` files, reading the install paths and compiler flags of
[qt5-qtbase](#port-qt5-qtbase)'s mkspec, and `INSTALL_ROOT` stages the install. The module is at
`qt5-qtbase`'s version: it compiles against Qt's private headers (`core-private`,
`gui-private` and `widgets-private`), which carry no promise from one Qt release to the next.
