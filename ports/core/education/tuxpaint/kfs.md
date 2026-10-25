## About

Tux Paint is a drawing program for children: brushes, stamps, shapes, lines, text and the
"magic" tools, which are effects shipped as plugins under `/usr/lib/tuxpaint/plugins`. It is an
SDL 2 program, drawn here by [sdl2-compat](#port-sdl2-compat) on SDL 3, with its own large-button
interface and no toolkit.

## Flags

The makefile assigns `CFLAGS` and `LDFLAGS` itself and never reads them from the environment. It
does read `CPPFLAGS`, at the head of its own `CFLAGS`, so `build.sh` hands the tree's compile flags
over in `CPPFLAGS`, and everything else on the `make` command line in `_make`.

- `-D_POSIX_PRIORITY_SCHEDULING` — The font loader in `src/fonts.c` runs in a forked child that
  calls `sched_yield()` to let the parent run. With this macro undefined the file defines
  `sched_yield()` away to nothing. glibc's headers define the macro; musl's do not.
- `-DHAVE_STRCASESTR` — Tell `src/tuxpaint.c` that the C library has `strcasestr`. Without it, the
  file defines its own copy whenever `__USE_GNU`, a glibc-internal macro, is undefined, as it
  always is under musl.
- `-Wno-implicit-function-declaration` — Let a call to a function with no declaration in scope
  compile silently instead of failing with the error GCC makes of it.
- `LDFLAGS="$LDFLAGS"` — Give the link the tree's flags. The makefile's own `LDFLAGS` on Linux is
  empty, so this replaces nothing.
- `OPTFLAGS='$(FASTMATH)'` — Replace the makefile's `-O2 -g $(FASTMATH)` for the program with
  `-ffast-math` alone; the optimisation level comes from the tree's flags in `CPPFLAGS`, and the
  program ships no DWARF.
- `MAGIC_CFLAGS` — Replace the makefile's flags for the magic plugins, `-g3 -O2 $(FASTMATH)` and
  warnings, with the tree's `CFLAGS` plus `$(FASTMATH)`, `-fno-common`, the SDL include flags and
  `-Isrc/`, so the plugins are built with the tree's hardening and without DWARF.
- `PACKAGE_ONLY=yes` — Install the desktop entries and hicolor icons into `DESTDIR` instead of
  registering them with xdg-utils on the build machine.
- `COMPLETIONDIR=$PKG/usr/share/bash-completion/completions` — Install the bash completion where
  bash-completion looks for it, instead of upstream's `/etc/bash_completion.d`. `build.sh` then
  renames the file to `tuxpaint`, the command it completes, which is the name bash-completion
  loads it by.

## Notes

The makefile is not safe in parallel, so both `make` runs pass `-j1`.

`build.sh` keeps only the English documentation and manual pages: every directory under
`/usr/share/doc/tuxpaint-<version>` other than `en` and `html`, and every directory under
`/usr/share/man` other than the `man<section>` ones, is removed. It also removes
`maketuxfont.py`, a Python helper that upstream installs with the zh_TW font documents.

The entries are replaced. Tux Paint asks for the window class `TuxPaint.TuxPaint` by setting SDL
2's `SDL_VIDEO_X11_WMCLASS` with `putenv()` in `main()`, but sdl2-compat renames that variable to
`SDL_APP_ID` only in its load-time constructor, before `main()` runs, so SDL 3 falls back to the
executable's name and the app_id is `tuxpaint`.
