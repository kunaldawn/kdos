## About

KiCad is the full electronics design suite: schematic capture (Eeschema), board layout (Pcbnew),
the Gerber viewer, the footprint and symbol editors, a SPICE simulator front end driving
[ngspice](#port-ngspice), and `kicad-cli` for plotting and exporting from scripts. It is built on
[wxwidgets](#port-wxwidgets) with GTK 3. The symbol, footprint and template libraries are ports
of their own, [kicad-symbols](#port-kicad-symbols), [kicad-footprints](#port-kicad-footprints) and
[kicad-templates](#port-kicad-templates).

## Flags

- `-DKICAD_UPDATE_CHECK=OFF` — Compile out the check for a new release, which is on by default.
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
`next()` instead. Without it every board iteration from a script raises.

The plugin and content manager reads an online repository, so on a machine with no network it
lists nothing.
