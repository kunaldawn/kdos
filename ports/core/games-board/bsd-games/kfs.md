## About

bsd-games is the Linux packaging of the classic BSD games. KDOS builds three of the collection:
`adventure`, the original Colossal Cave Adventure; `tetris-bsd`; and `worm`. All three are
terminal programs, and the recipe writes a desktop entry for each.

The package's `configure` is not autoconf. It is a hand-written interactive script that asks its
questions on a terminal and writes the answers to `config.params`; when a `config.params` already
exists it reads it first, and `bsd_games_cfg_non_interactive=y` makes it take every answer from
there without asking. `build.sh` therefore writes the whole of `config.params` and then runs
`./configure` with no arguments.

## Flags

- `bsd_games_cfg_build_dirs` — The games to build: `adventure tetris worm`, and none of the
  others in the collection. `hangman` is left out because it opens `/usr/share/dict/words` at
  start and exits through `err(1)` when it cannot, and no port installs a word list.
- `bsd_games_cfg_other_cflags`, `bsd_games_cfg_other_ldflags` — Carry the tree's `CFLAGS` and
  `LDFLAGS` into the build. `Makeconfig` assigns `CFLAGS := $(OPTIMIZE) $(WARNING) @other_cflags@`
  and `LDFLAGS := @other_ldflags@` with `:=`, discarding whatever the environment exported, so
  the tree's flags (the prefix maps and build ID that keep a package the same bytes on every build
  among them) reach the compiler only through these two answers.
  `bsd_games_cfg_optimize_flags` and `bsd_games_cfg_warning_flags` are left empty, so that
  `configure`'s own defaults (`-g -O2` and a list of `-W` warnings) are not added before them.
- `bsd_games_cfg_install_prefix` — Install under `$PKG`, the package's staging root, instead of
  the running system.
- `bsd_games_cfg_do_chown` — `n`: do not change the owner or group of what is installed. The
  install runs into `$PKG`, and the games are not installed setgid.
- `bsd_games_cfg_vardata_perms` — `0666` for the files under `/var/games`, which is the tetris
  score file. tetris opens it read-write at the end of every game and exits through `err(1)` when
  it cannot, so a score file the player cannot write ends the program the first time a game is
  over. Upstream's `SECURITY` file gives two ways for a game to update a system-wide score file:
  a setgid binary, or a world-writable file. The binaries here are installed `0755` with no
  setgid bit, so the score file is the one that is writable, and it is the only writable file in
  the package.
- `bsd_games_cfg_tetris_scorefile` — Keep the score file at `/var/games/tetris-bsd.scores`.
- `bsd_games_cfg_ncurses_lib` — Link the games against `-lncurses`, the [ncurses](#port-ncurses)
  port's library.
- `bsd_games_cfg_gzip_manpages` — `n`: install the manual pages uncompressed.

## Patches

- `adventure-linkdata-bounds.patch` — In `adventure/init.c`, upstream's loop in `linkdata()` runs `for (i = 1; i <= LOCSIZ; i++)` over `ltext`, `travel` and `cond`, which are arrays of `LOCSIZ` elements, so its last pass reads `ltext` and `travel` one element past their end and can write `cond` one past its end. The patch stops the loop at `i < LOCSIZ`.

## Notes

[libbsd](#port-libbsd) is in `depends` although nothing links it. Every source file includes
bsd-games' own `include/sys/cdefs.h`, which is a wrapper that does `#include_next <sys/cdefs.h>`,
so a system header of that name has to exist. musl installs none; on KDOS it is the copy the
libbsd port installs into `/usr/include/sys`.
