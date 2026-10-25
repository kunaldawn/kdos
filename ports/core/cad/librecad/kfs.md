## About
[LibreCAD]{.idx} is a 2D CAD program for drafting: plans, technical drawings and parts drawn with
lines, arcs, dimensions, hatches and blocks on layers, saved natively as DXF and able to import
DWG. It is a Qt 5 application, and the port also installs `ttf2lff`, the tool that converts a
TrueType face into LibreCAD's own LFF stroke-font format.

## Patches
- `no-execinfo.patch` — makes the crash handler in `librecad/src/lib/debug/lc_crashhandler.cpp` build without `<execinfo.h>`. The header is included only when `__has_include` finds it, and the `backtrace()` and `backtrace_symbols_fd()` calls are compiled only then; otherwise the handler records zero frames and writes `(no frames captured)` in place of a backtrace. musl has no `<execinfo.h>` and the tree carries no library that provides one, so on KDOS a crash report carries the signal but no stack.

## Notes
Upstream's `install()` rules put the plugins in `bin/resources/plugins`, beside the executable,
and install none of the support data. `build.sh` moves the plugins to `/usr/lib/librecad/plugins`
and copies the fonts, hatch patterns and part library from `librecad/support/` to
`/usr/share/librecad`. Both are directories LibreCAD searches relative to its executable
(`../lib/librecad/plugins` and `../share/librecad/<kind>`, in `RS_System::getDirectoryList`), so
nothing is left under `/usr/bin` but the two programs.

The manual page, the icon and the AppStream file are installed from the source by hand, because
the CMake build installs none of them, and `build.sh` writes the desktop entry.
