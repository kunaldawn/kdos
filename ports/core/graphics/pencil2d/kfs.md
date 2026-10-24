## About

Pencil2D is a hand-drawn 2D animation program with bitmap and vector layers, onion skinning and a
sound track. It is a qmake project built against Qt 6.

## Flags

- `CONFIG+=NO_TESTS` — Drop the `tests` subproject from `pencil2d.pro`, so the unit tests are not
  compiled.
- `QMAKE_CFLAGS+="$CFLAGS"` — Qt 6's qmake reads no compiler flags from the environment, so the
  tree's flags, and with them the reproducibility path maps, are appended to qmake's own on the
  command line. `QMAKE_CXXFLAGS+=` and `QMAKE_LFLAGS+=` do the same for C++ and the link (see
  [qmake](../../../../docs/kdos/05-developer/writing-ports.md#qmake)).

## Patches

- `no-palette-mime-glob.patch` — Remove the `application/x-pencil2d-palette` type from `app/data/org.pencil2d.Pencil2D.xml`. It claims every `*.xml` file by a glob as long as `application/xml`'s, and a lookup by the longest suffix then hands any XML file to whichever of the two was compiled first. Palettes are opened from Pencil2D's own palette menu, which does not consult the MIME database.

## Notes

Movie and GIF export and import run the `ffmpeg` program found on `PATH` when the menu entry is
used, so [ffmpeg](#port-ffmpeg) is in `depends`; without it those entries fail with an error.

The desktop entry is written over upstream's: it carries
`StartupWMClass=org.pencil2d.Pencil2D`, the Wayland app id `setDesktopFileName` sets, so the panel
ties the window to its launcher. Its two MIME types are Pencil2D's own project formats, from the
XML installed above.
