## About

GNU Go is a program that plays Go. Its `--mode gtp` speaks the Go Text Protocol on standard input
and output, which is how [kigo](#port-kigo), KDE's Go board, runs it as the computer opponent; it
also has an ASCII board of its own in a terminal. The package carries `gnugo`, its manual page in
section 6 and its Texinfo manual.

## Flags

- `export CFLAGS="$CFLAGS -fcommon"` — Place uninitialised global variables in a common block.
  `patterns/mkmcpat.c` and `engine/globals.c` each define `meaningless_white_moves` and
  `meaningless_black_moves` without `extern`, and under GCC's default `-fno-common` the link of
  the `mkmcpat` pattern generator fails with a multiple-definition error.
- `--with-readline` — Line editing and history at the ASCII board's move prompt, through
  [readline](#port-readline). Upstream's default is without it.
- `--with-curses` — Coloured debugging output through [ncurses](#port-ncurses)'s terminfo;
  upstream's default, stated.

## Notes

The engine's strength settings (level 10, the owl and semeai node limits, the cache size) and its
rules (Japanese counting, resignation allowed) are upstream's compiled-in defaults; Kigo passes
the level it wants on the command line. Socket support for GTP over TCP is on by default.
