## About

Horizon EDA designs printed circuit boards: schematic capture, board layout, and Gerber, ODB++,
PDF and STEP export. Its parts live in a pool in which every unit, symbol, package and part has a
unique identity, so a project refers to a part by that identity rather than by a library path.
`horizon-eda` is the pool and project manager that opens the editors; `horizon-imp` is the editor
program itself. Its 3D board view reads STEP models through [opencascade](#port-opencascade).

## Notes

The podofo port is a 1.x release, and Horizon's PDF export is written against the older podofo
interface; `podofo-1.patch` is upstream's commit that compiles it against both.

`app-id.patch` sets the program name, which GTK gives a window as its Wayland app id, to
`org.horizon_eda.HorizonEDA` in the project manager and the editor alike: the entry's file name,
its `StartupWMClass` and the metainfo's launchable. Upstream sets `horizon-eda` in the editor and
leaves the manager on its executable's name, and KWin finds a Wayland window's icon only in the
entry whose file name is its app id. The patch is to be offered upstream.

The build has no option file. Every dependency is found by pkg-config, and OpenCASCADE through its
CMake package, which is why [cmake](#port-cmake) is in `depends` of a meson build. 3D mouse support
compiles in only when [libspnav](#port-libspnav) is found, and its absence passes without a word,
so the last line of `build.sh` fails the build unless `HAVE_SPNAV` reached the compile commands.

The parts pool is not part of the package. The project manager downloads one from its pools
window, so on a machine with no network it has no pool until one is copied in.
