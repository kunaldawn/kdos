## About
Qt SVG renders SVG Tiny 1.2 drawings for Qt 6: the `QSvgRenderer` and `QSvgWidget` classes of the
QtSvg and QtSvgWidgets libraries, and the two plugins through which every Qt 6 program reads SVG
without asking for it, the image format plugin behind `QImage` and the icon engine behind
`QIcon`. Without it a Qt 6 program draws no SVG icon from a theme, and Qt Quick's `VectorImage`
item is not built.

## Notes
Like every Qt 6 module but `qt6-qtbase`, it passes no install paths of its own and is built with
the shape every module shares (see [How Qt 6 is built](../../../../docs/kdos/05-developer/writing-ports.md#how-qt-6-is-built)).
