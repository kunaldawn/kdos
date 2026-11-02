## About

Palapeli is a jigsaw puzzle game. A puzzle is cut from any picture by a slicer plugin (rectangles,
classic jigsaw shapes, or the Goldberg slicer's hexagonal, rotrex, cairo and irregular grids), and
its pieces move freely on an open table rather than snapping to a grid. Five puzzles come with the
package. It is a Qt 6 and KDE Frameworks application built on [libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The slicer interface is a library of its own, `libpala`, installed with its headers and CMake
package so that a slicer can be built outside the tree; the slicers the package carries are plugins
against it.

The Goldberg slicer's irregular grid runs `qvoronoi` from [qhull](#port-qhull), which is in
`depends` for run time: the slicer starts `qvoronoi` once as it loads, and offers the irregular grid
only when that succeeds. The configure looks for the program only to print a warning.

The package defines its own puzzle-file type, `application/x-palapeli-puzzle`, in
`/usr/share/mime/packages/palapeli-mimetypes.xml`, and [shared-mime-info](#port-shared-mime-info)
is a requirement of the configure. Upstream's install step updates the MIME database only when no
`DESTDIR` is set, so it writes nothing into the package, and kpkg regenerates the database when the
package is installed (see
[Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes)). A KIO
thumbnailer plugin previews puzzle files in Dolphin, and a service menu imports them into the
collection.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.palapeli.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.palapeli`, from the component name and the `apps.kde.org` home page, so it
is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.
