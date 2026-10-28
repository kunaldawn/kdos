## About
`kdos-bb` is the audio-visual demo that ships with KDOS: a hard fork of `bb`, the AA-project's
text-mode demo, drawn by [aalib](#port-aalib) and scored with three S3M tracker modules played by
[libmikmod](#port-libmikmod). What it does, how the fork differs from upstream and why, is
[the program's chapter](../../../docs/kdos/04-programs/kdos-bb.md);
`KDOS-FORK` at the root of the port lists every change file by file. No other port depends on it.

The build has no configure step. Upstream's autoconf script fails under GCC 14 (its compiler
probe is a K&R `main()` that GCC 14 rejects), so the fork carries `src/aconfig.h`, the answers that
script would have given for Linux, musl and x86-64, and `build.sh` compiles the source list in one
`cc` call. It installs the binary, the three modules under `/usr/share/kdos-bb`, the manual page,
and `COPYING` and `AUTHORS` as the licence.

## Flags
- `-O2` — The optimisation level the tree's `CFLAGS` already carries, named again on the command
  line.
- `-DSOUNDDIR='"/usr/share/kdos-bb"'` — The directory `main.c` prefixes to a module's name when
  it loads one, which is where `build.sh` installs `bb.s3m`, `bb2.s3m` and `bb3.s3m`.
- `-Wno-implicit-function-declaration`, `-Wno-implicit-int`, `-Wno-int-conversion`,
  `-Wno-incompatible-pointer-types`, `-Wno-return-mismatch`,
  `-Wno-declaration-missing-parameter-type` — The six diagnostics GCC 14 turned from warnings into
  errors: a call to an undeclared function, a declaration with no type, an integer used as a
  pointer, a pointer of the wrong type, a `return` that does not match its function, and an old
  style parameter with no type. The flags turn them back into warnings for the demo's
  pre-standard C. [aalib](#port-aalib) passes the same six.
- `-laa`, `-lmikmod`, `-lncurses`, `-lm`, `-lpthread` — The libraries, named by hand because no
  build system finds them. AAlib draws through its curses driver, so ncurses is linked as well;
  `-lpthread` is for the mixer thread that feeds libmikmod
  ([The mixer runs on its own thread](../../../docs/kdos/04-programs/kdos-bb.md#the-mixer-runs-on-its-own-thread)).

## Notes
The source list in `build.sh` is the build: a `.c` file added under `src/` is not compiled until
it is named there, and nothing reports one that is missing until the link fails.

`src/aconfig.h` states what one target has. On another C library or architecture it has to be
checked by hand. `timers.c` takes its clock from the first of `HAVE_GETTIMEOFDAY` and
`HAVE_FTIME` that is defined; `aconfig.h` defines only the first, so the demo is timed with
`gettimeofday`.

`src/kdostux.c`, the mascot as an LZO-compressed image, is the output of `genimg.py`, which runs on
a development host with Pillow and a C compiler: it compiles the fork's own `minilzo.c` to
compress the picture, because only the matching LZO1X implementation is guaranteed to decompress
it. The build never runs it, so a changed picture means running it and committing its output.

The port builds only where aalib and libmikmod are installed, which is inside the build tree,
not on a bare host; rebuilding it alone is in
[Building and changing it](../../../docs/kdos/04-programs/kdos-bb.md#building-and-changing-it).
