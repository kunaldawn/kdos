## About

NetHack is the roguelike: a descent through the Dungeons of Doom drawn in characters, played in a
terminal. KDOS builds its tty and curses interfaces and installs it so that every player has a
playground of their own, under `${XDG_DATA_HOME:-~/.local/share}/nethack`, with no setgid binary
and no score file shared between players.

NetHack has no `configure`. `sys/unix/setup.sh` splices a hints file into each generated
`Makefile`, so the hints file that `build.sh` writes to `sys/unix/hints/kdos` is the port's whole
configuration. Assignments in its `#-PRE` segment land before the stock Makefile body and lose to
anything the body assigns; `util/Makefile`'s body sets `LIBS` empty, which is why `LIBS` is set in
the `#-POST` segment instead.

## Flags

- `-D_GNU_SOURCE` — Ask the C library for its GNU extensions in every file. `config1.h` defines
  `_GNU_SOURCE` itself and needs it to come before any system header, but
  `win/curses/cursmain.c` includes `curses.h` on the line above `hack.h`, so that file would
  configure the C library without it and then call `mempcpy()` through a fortifying `<string.h>`.
  Set on the command line, it is in force before the first include of every file.
- `-DDLB` — Pack most of the game's data files into one archive, `nhdat`, read through NetHack's
  data librarian, instead of installing them as separate files.
- `-DZLIB_COMP` — Compress save and bones files in-process with [zlib](#port-zlib) rather than by
  running an external `compress` program. It is why `LIBS = -lz` is in the hints.
- `-DHACKDIR` — The compiled-in directory of the read-only game data, `/usr/share/nethack`.
- `-DSYSCF`, `-DSYSCF_FILE` — Read a system-wide configuration file, and name it
  `/etc/nethack/sysconf`. `config.h` turns `SYSCF` on in any case, and a build with it refuses to
  start when it cannot read the file, so the recipe installs one. The path is absolute because the
  game runs in the player's playground, where a relative name would not find it.
- `-DTIMED_DELAY` — Pause for display effects with a timer, `usleep()`, rather than with extra
  screen output, whose delay varies with the terminal.
- `-DDUMPLOG` — Write an end-of-game dump of the character. The shipped `sysconf` names the file
  `dumplog.%n.%d.txt`, a relative name, so the dump lands in the player's own playground.
- `-DCURSES_GRAPHICS` — Build the curses interface beside the tty one.
- `-DCURSES_UNICODE`, `-D_XOPEN_SOURCE_EXTENDED` — Draw the curses interface through ncurses's
  wide-character API. The curses sources use `cchar_t` whenever the header defines
  `NCURSES_WIDECHAR`, and the ncurses header defines it to 0 unless X/Open's extended interface is
  asked for, which leaves the type undeclared and the build failing. `_XOPEN_SOURCE_EXTENDED`
  exposes it, and `CURSES_UNICODE` is upstream's switch for drawing through it; both go with the
  `-lncursesw` in `WINLIB`.
- `-DCONFIG_ERROR_SECURE=FALSE` — Report every error in a configuration file, with line numbers
  and context. The default, `TRUE`, shows only the first error when `NETHACKOPTIONS` points the
  game at a file.
- `-DNOCRASHREPORT` — Build no crash reporter. On Linux `config.h` otherwise compiles one in that,
  on a panic, runs `/usr/bin/xdg-open` to open a browser on the bug-report page `sysconf` names in
  `CRASHREPORTURL`, and turns on `NOSTATICFN`, which drops `static` from the game's internal
  functions. The shipped `sysconf` names no such page, so the reporter would never open one.
- `-DSELF_RECOVER` — Let the game itself offer to recover an interrupted game when it finds that
  character's lock and level files. Without it the player is told only to destroy the old game or
  quit, and `nethack-recover` is the one way back.
- `-DLUA_USE_POSIX` — Build the bundled Lua with its POSIX facilities, in `SYSCFLAGS`, which is
  what Lua's own build reads; it never sees `CFLAGS`, so the tree's flags are repeated there.
- `LUA_VERSION` — Passed to both `make` lines, so the directory the Makefiles look under,
  `lib/lua-<version>`, and the Lua release the recipe pins are the same version.
- `WINLIB` — `-lncursesw` alone. The [ncurses](#port-ncurses) port is built wide-character and
  without `--with-termlib`, so there is no `libtinfo` to link, and its `libncurses.so` is only a
  link to `libncursesw.so`.
- `CHOWN`, `CHGRP` — `true`, so the install changes no owner or group: the game runs as the player,
  and every file it writes belongs to the player.

## Notes

The game is linked against a static Lua built from `lib/lua-<version>/src`, a tree the release
archive does not carry (upstream's `make fetch-lua` downloads it). The recipe's second source is
that Lua release, and `build.sh` copies it into `lib/` before the build.

`hacklib.a` is built first, on its own: `src/Makefile` builds it, and so does `util/Makefile`, by
running a second `make hacklib.a` in `src/`. Under a parallel make the two runs of `ar` can meet
and leave an archive with no index, which fails the link of `makedefs`. `make install` then builds Lua, the game, `recover`, the data files and the DLB
archive; `make all` is not run because it also builds the Guidebook, which needs `nroff`, `tbl`
and `col`.

**The playground is per player.** NetHack keeps saves, bones, level files, locks and the score
file in its working directory, and the alternative, `VAR_PLAYGROUND`, is a compile-time string
that no runtime value can reach. The one way to give each player their own is to give each player
a working directory: `unixmain.c` reads `$NETHACKDIR` and changes into it before it opens a file.
`/usr/bin/nethack` is a wrapper that sets `NETHACKDIR`, creates `save/` (the game does not, and a
missing one is a save error mid-game), creates the empty `perm`, `logfile` and `xlogfile` that the
game locks but opens without `O_CREAT`, links `nhdat`, `license` and `symbols` from
`/usr/share/nethack`, and runs the real binary from `/usr/libexec/nethack`. `nethack-recover` runs
`recover` from the same directory.

For the same reason the binaries move out of `HACKDIR` into `/usr/libexec/nethack`, and the score
file, logs, lock and empty `save/` that `make install` seeds there are deleted: they are
per-player state, and shipped in a read-only directory they would be files nobody may write.

The shipped `sysconf` sets `WIZARDS=*` and `EXPLORERS=*`, opening debug and explore mode to every
player: with no shared playground and no privileged binary, such a game can only rewrite files
its player already owns.
