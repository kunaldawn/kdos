## About

KiCad is the full electronics design suite: schematic capture (Eeschema), board layout (Pcbnew),
the Gerber viewer, the footprint and symbol editors, a SPICE simulator front end driving
[ngspice](#port-ngspice), and `kicad-cli` for plotting and exporting from scripts. It is built on
[wxwidgets](#port-wxwidgets) with GTK 3. The symbol, footprint and template libraries are ports
of their own, [kicad-symbols](#port-kicad-symbols), [kicad-footprints](#port-kicad-footprints) and
[kicad-templates](#port-kicad-templates).

## Flags

- `-DKICAD_UPDATE_CHECK=OFF` — Compile out the check for a new release, which is on by default.
  Preferences then hides its checkbox; the first-run start wizard's Updates & Privacy page does
  not consult the setting, so `startwizard-no-update-check.patch` hides the box there too and
  words the page for package updates only. The patch is to be offered upstream.
- `-DKICAD_USE_SENTRY=OFF` — No Sentry crash and metrics reporting. It is upstream's default, and
  turning it on needs a Sentry DSN set at configure time.
- `-DKICAD_BUILD_I18N=OFF` — Build no translations. It is upstream's default.
- `-DKICAD_BUILD_QA_TESTS=OFF` — Leave out the unit test suites, which upstream builds by
  default.
- `-DKICAD_SPICE_QA=OFF` — Leave out the simulator's own test suite, also on by default.
- `-DKICAD_SCRIPTING_WXPYTHON=ON` — Build the wxPython support of the Python scripting, through
  the [python3-wxpython](#port-python3-wxpython) port: Python plugins that build wx interfaces, and
  the `py.shell` console. It is upstream's default.
- `-DKICAD_WAYLAND=ON` — Compile KiCad's Wayland code in `libs/kiplatform`, which moves the pointer
  from the program (warping) through the pointer-constraints protocol under a Wayland compositor.
  On by default on Linux.
- `-DKICAD_IPC_API=ON` — The IPC interface through which external programs drive a running KiCad.
  It is upstream's default, and configure then requires [nng](#port-nng).
- `-DKICAD_SIGNAL_INTEGRITY=ON` — Build the signal integrity tools. It is upstream's default.
- `-DKICAD_IDF_TOOLS=ON` — Build `idfcyl`, `idfrect`, `dxf2idf` and `idf2vrml`, the helpers for IDF
  board exchange files. It is upstream's default.
- `-DKICAD_INSTALL_DEMOS=ON` — Install the demo projects and examples. It is upstream's default.
- `-DKICAD_BUILD_FLATPAK=OFF` — Install the desktop entries, icons and MIME data under their
  ordinary names, not flatpak's. It is upstream's default.

## Notes

`build.sh` builds the target `common/pcb_lexer.h` on its own before the full build. That generated
header is not ordered before its first consumer, so a parallel build can compile a file that
includes it before it exists.

SWIG from version 4.4 drops the Python 2 style `.next()` method of its iterators, which the
`pcbnew` bindings call; `fix-swig-iterator-next.patch`, upstream's merge request, calls Python 3's
`next()` instead. Without it a script that iterates over a board's item or track lists (the
`std::deque<BOARD_ITEM *>` and `std::deque<PCB_TRACK *>` wrappers in
`pcbnew/python/swig/board.i`) fails with an `AttributeError`.

Each program's Wayland app id is the name of its desktop entry, `org.kicad.kicad`,
`org.kicad.eeschema`, `org.kicad.pcbnew`, `org.kicad.gerbview`, `org.kicad.pcbcalculator` and
`org.kicad.bitmap2component`, which is the only name KWin looks a window's icon up by. KiCad sets
it through wxWidgets only from wxWidgets 3.3.1, and [wxwidgets](#port-wxwidgets) is 3.2, so
`app-id.patch` sets GTK's program name to the same id there; without it every window takes its
executable's name and shows the generic icon. An editor opened from the project manager runs in
its process and takes `org.kicad.kicad`. The patch is to be offered upstream.

The plugin and content manager reads an online repository, so on a machine with no network it
lists nothing.
