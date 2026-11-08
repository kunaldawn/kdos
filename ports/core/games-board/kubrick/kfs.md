## About

Kubrick is a Rubik's Cube game drawn in 3D: cubes, bricks and mats of any size up to 6×6×6, with
preset puzzles at several levels from a 2×2×1 mat to a 6×6×6 cube, a 6×3×2 brick and a 6×4×1
mat, turned with the pointer or the keyboard. It shows demonstrations of patterns and solution
moves, and every move can be undone back to the start. It is a Qt 6 and KDE Frameworks application
built on [libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_DISABLE_FIND_PACKAGE_7Zip=ON` — Compress the theme's SVG to SVGZ with gzip, never with
  7-Zip. Upstream takes 7-Zip whenever it finds one, and [7zip](#port-7zip) is in the build root
  from `41_system` on, so the tool would follow whatever happens to be installed. Refused, the build
  runs `gzip -9 -n`, which upstream then requires and which stores no file name or time stamp in the
  output; [gzip](#port-gzip) is in `depends`.

## Notes

The cube is drawn with desktop OpenGL in a `QOpenGLWidget`. The program links `OpenGL::GL` through
[libglvnd](#port-libglvnd) and `OpenGL::GLU` from [glu](#port-glu), which the top-level configure
does not name but the program's link line does: without glu the generate step stops at an unknown
`OpenGL::GLU` target. [mesa](#port-mesa) is the driver libglvnd dispatches to at run time.

`KF_SKIP_PO_PROCESSING` stops only `ki18n_install()`: `kdoctools_install()` still builds the
handbook in every translated language, and kpkg's language rule removes `/usr/share/doc/HTML/<lang>/`
from the package.

The desktop entry is upstream's `org.kde.kubrick.desktop`. `KAboutData` derives the window's
Wayland app id, `org.kde.kubrick`, from the component name and the `apps.kde.org` home page, so it
is the entry's file name and Plasma pairs the window with the entry without a `StartupWMClass`.
