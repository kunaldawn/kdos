## About

mandoc is the manual-page system of KDOS: it formats `man` and `mdoc` pages and installs the
commands people type, `man`, `apropos`, `whatis` and `makewhatis`, under their usual names. No
other port provides those programs, and mandoc formats the pages itself, so reading a manual page
on KDOS needs neither man-db, which the tree does not carry, nor groff.

mandoc has no autoconf. Its `configure` is a hand-written shell script that reads its settings from
a file named `configure.local` in the source directory, and ignores any variable it does not know,
so `build.sh` writes that file and runs `./configure` with no arguments. Every variable written
there is one the script reads.

`apropos` and `whatis` search an index, `mandoc.db`, in each manual root, and `man` finds a page by
walking the tree when no index is there. The build writes no index while ports install (the chroot
phases set `KPKG_SKIP_INDEX=man`), and `70_image/080_whatis.sh` builds it once over the finished
tree; see [Shared indexes](../../../../docs/kdos/05-developer/writing-ports.md#shared-indexes).

## Flags

- `CFLAGS="$CFLAGS"` — `configure` empties `CFLAGS` before it reads `configure.local`, so the
  tree's release flags reach the compile only through this line. Without it mandoc builds with
  `-g` and no optimisation.
- `LDFLAGS="$LDFLAGS"` — The same for the linker flags, which `configure` also empties.
- `UTF8_LOCALE="C.UTF-8"` — The locale mandoc switches to for `-Tutf8` output. Left unset, the
  script looks for one in the output of `locale -a`, a command that only `musl-locales` installs,
  in `40_lang`; finding none, it builds mandoc with no wide-character support, which `-Tutf8` output
  needs. `C.UTF-8` is built into musl, so it exists whatever locales are installed.
- `BINM_MAN="man"` — Install the viewer as `man`. The four `BINM_` names (`BINM_MAN`,
  `BINM_APROPOS`, `BINM_WHATIS`, `BINM_MAKEWHATIS`) restate mandoc's defaults; no other port
  installs a program under any of these names. `080_whatis.sh` runs the indexer by the name `makewhatis`.
- `BINM_PAGER="less"` — Page output through [less](#port-less), which is in `depends`. Left unset,
  the script runs `less` once and picks it if that works and `more` otherwise, so the default pager
  would follow what the build root holds.
- `MANPATH_DEFAULT="/usr/share/man:/usr/local/share/man"` — The manual roots `man` and `apropos`
  search when neither `-M` nor `MANPATH` names any and there is no `/etc/man.conf`, which KDOS does
  not ship. mandoc's own default also lists
  `/usr/X11R6/man` and `/usr/local/man`, which no KDOS port installs into. `080_whatis.sh` indexes
  these two roots, so a root added here needs adding there too, or `apropos` never sees its pages.
- `PREFIX="/usr"`, `BINDIR="/usr/bin"`, `SBINDIR="/usr/sbin"`, `MANDIR="/usr/share/man"`,
  `INCLUDEDIR="/usr/include"`, `LIBDIR="/usr/lib"` — Where each kind of file goes. `makewhatis`
  installs into `SBINDIR`, `/usr/sbin`.
