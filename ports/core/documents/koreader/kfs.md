## About
[KOReader]{.idx} is an e-book and document reader for EPUB, PDF, DjVu, FB2, MOBI and comic book
archives, written in Lua on LuaJIT, with crengine for reflowable formats and MuPDF and DjVuLibre
for fixed-layout ones. It is made for e-ink readers, and its desktop build, the one this port
makes, draws its own interface in an SDL3 window with no toolkit. Every library it runs on, apart
from SDL3 and the system's C and C++ runtimes, is built by its own `base` tree and installed
privately under `/usr/lib/koreader`.

## Flags
- `TARGET=` — An empty target is upstream's desktop build, which it calls the emulator: SDL for
  the window, input and audio, against the system's [SDL3](#port-sdl3).
- `KODEBUG=` — An empty `KODEBUG` is a release build; the desktop build defaults to a debug one
  (`-Og`, no `NDEBUG`).
- `VERBOSE=` — Leave the build's quiet output on.
- `TARGET_CFLAGS=` — Replace the desktop build's `-march=native`, which would tune every library
  to the processor of the machine that built it, with nothing.
- `QFLAGS=-fstack-clash-protection`, `COMPAT_LDFLAGS=-Wl,-z,now` — `base` assigns its own
  `CFLAGS`, `CXXFLAGS` and `LDFLAGS` and does not read the tree's, so the tree's hardening would
  be lost. `QFLAGS` (empty under GCC) is part of every C and C++ compile line, and
  `COMPAT_LDFLAGS` is appended to every link, so these two carry the tree's
  `-fstack-clash-protection` and `-z now` into the third-party libraries and KOReader's own
  code.
- `unset PKG_CONFIG_PATH` — `base` points `PKG_CONFIG_LIBDIR` at its staging directory and, for
  the desktop build, leaves `PKG_CONFIG_PATH` alone, which pkg-config searches first. The
  chroot's `PKG_CONFIG_PATH` names `/usr/lib/pkgconfig`, so the system's Leptonica and Tesseract
  would be found before `base`'s patched copies, and k2pdfopt, which calls their additions,
  stops compiling.

## Patches
- `koreader-sh-enable-ko-multiuser.patch` — Exports `KO_MULTIUSER=1` in the `koreader.sh` launcher, so settings, history and caches go to `~/.config/koreader` instead of the read-only install directory.
- `koreader-sdl-app-id.patch` — Exports `SDL_APP_ID=rocks.koreader.KOReader` in the launcher. The process SDL runs in is `luajit`, whose name would otherwise be the window's app_id; the menu entry's `StartupWMClass` names the same id.

## Notes
- **Two sources, one made by hand.** The first is the KOReader superproject with its submodules
  (`base`, crengine, fonts and translations), as Alpine mirrors it. `base` builds its
  third-party libraries (LuaJIT, MuPDF, crengine's, DjVuLibre, HarfBuzz, SQLite and the rest) at
  pinned versions, each downloaded at build time into
  `base/thirdparty/<project>/build/downloads`, as a tarball checked by MD5 or a git clone at a
  pinned revision. The second source is those downloads, packed by hand after
  `make TARGET= KODEBUG= download-all`; with each archive present and each clone at its revision,
  the download step reaches nothing and git checks the revisions out offline. How to rebuild it
  on a version bump is in
  [A bundle no tool writes](../../../../docs/kdos/05-developer/writing-ports.md#a-bundle-no-tool-writes).
  `build.sh` unpacks it over the source tree before anything else.
- **What ships as upstream made it.** The fonts in the tree and the certifi CA bundle are
  installed as they come; both are exempt under
  [What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source).
- **The install is the emulator tree.** `make` leaves the finished program in
  `koreader-emulator-*/koreader`; `build.sh` copies it with links resolved to
  `/usr/lib/koreader`, links `/usr/bin/koreader` to its `koreader.sh`, and removes the debug
  files (`*.dbg`) and the test suite (`spec`). `git-rev` is written by hand, because the tarball
  is not a git checkout and KOReader shows that file as its version.
- **English only.** Every translation is a directory under `l10n`, read as `.po` at run time,
  which the tree-wide language prune does not match; `build.sh` removes them, and the interface
  falls back to its built-in English strings.
- **The menu entry is KDOS's own.** `build.sh` writes `rocks.koreader.KOReader.desktop`, which
  claims no MIME type, and rasterises upstream's SVG icon to PNGs, which the session's icon lookup
  reads. Upstream's own entry claims PDF, PNG, ZIP, HTML and plain text among many types that
  belong to other programs.
