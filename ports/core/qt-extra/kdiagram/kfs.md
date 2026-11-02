## About

KDiagram is two Qt widget libraries for drawing data: KChart, for bar, line, pie, polar, stock
and ternary charts with legends and axes, and KGantt, for Gantt diagrams with constraints between
tasks. KOrganizer's event views and incidence editor draw their charts with it, as do Massif
Visualizer and Calligra. The libraries are `libKChart6` and `libKGantt6`.

The recipe takes the 3.0.1 release from its own `3.0.1/` directory on KDE's download server. The
`kdiagram-2.7.90.tar.xz` that sits at the top of the same directory is an old beta.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Build none of the Qt translation catalogues under `poqm/`,
  which `ecm_install_po_files_as_qm()` would compile into `.qm` files; the package-wide
  English-only rule (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy))
  would remove them anyway.
- `-D BUILD_QCH=OFF` — Build no API documentation in Qt Help format, which needs Doxygen and
  Qt's help generator. Off is upstream's default too; the recipe names it.

## Notes

The examples, which include the only use of Qt SQL, are built with the tests, so
`BUILD_TESTING=OFF` leaves them out. The charts print through Qt Print Support in
[qt6-qtbase](#port-qt6-qtbase) and read SVG through [qt6-qtsvg](#port-qt6-qtsvg).
