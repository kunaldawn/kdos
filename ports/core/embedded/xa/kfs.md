## About

xa (xa65) is a cross-assembler for the 6502 family: the NMOS 6502, the 65C02 and R65C02, and the
16-bit 65816, with a C-like preprocessor, local labels and segments. It writes plain binaries or
relocatable `o65` objects, and the package carries the `o65` tools beside it: `ldo65` links them,
`reloc65` relocates one, `file65` describes one, `printcbm` lists a Commodore BASIC file, and
`uncpk` manages C64 `cpk` archives. [vice](#port-vice) and [libsidplayfp](#port-libsidplayfp) look
for it when they are configured, to assemble the 6502 code in their sources.

## Flags

- `-j1` — One job, for the build and for the install, which remakes `all`. The `all` target lists
  `killxa`, which deletes `./xa`, beside the `xa` target that links it, and under a parallel make
  the delete can land after the link.
- `CFLAGS="$CFLAGS $LDFLAGS"` — The makefile's own `CFLAGS` is only `-O2 -g`, so the tree's flags
  are given on the command line instead. The top-level makefile hands its sub-makes `CC="${CC}
  ${CFLAGS}"`, and the programs in `misc/` are compiled and linked in one command from that, reading
  no `LDFLAGS`; the link flags reach them only inside `CFLAGS`.
- `LDFLAGS="$LDFLAGS"` — Replaces the makefile's `LDFLAGS = -lc`, and is what `src/Makefile` links
  `xa` with.
- `DESTDIR="$PKG/usr"` — In this makefile `DESTDIR` is the installation prefix, `/usr/local` by
  default, not a staging root: `BINDIR` and `MANDIR` are built from it. Pointing it at `$PKG/usr`
  stages the programs in `/usr/bin` and the manual pages in `/usr/share/man/man1`.

## Patches

- `binfnam-bound.patch` — Three places in `src/xat.c`, one for `.include` and two for the passes of `.bin`, copy the directive's file name into a 255-byte buffer and stop with an error only once the index passes 255, so a long name writes the byte at index 255 and the terminator past the end. The patch makes each buffer 256 bytes and stops at 255, which leaves room for the terminator.
