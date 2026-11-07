## About

KSudoku generates and solves Sudoku puzzles, from 4×4 grids through jigsaw and killer variants to
the 3-D Roxdoku cubes. It is built on [libkdegames](#port-libkdegames).

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the interface translations in `po/`; the image is
  English only (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-D CMAKE_REQUIRE_FIND_PACKAGE_OpenGL=ON` — Requires OpenGL, from [libglvnd](#port-libglvnd) and
  [mesa](#port-mesa), for the 3-D Roxdoku puzzles. Upstream leaves Roxdoku out without OpenGL and
  GLU, and [glu](#port-glu) is in `depends` for the second.

## Notes

The desktop entry is KDOS's own, written over upstream's. Its `StartupWMClass` is
`org.kde.ksudoku`, the desktop file name KAboutData sets and the window's Wayland app_id, so the task
manager can match the window to its entry.
