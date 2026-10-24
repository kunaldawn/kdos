## About

`libgsm` is the TU Berlin implementation of the [GSM 06.10]{.idx} full-rate speech codec, with
`toast`, the command that compresses a sound file with it (`untoast` and `tcat` are the same
program under other names). In KDOS, `sox` reads and writes GSM audio through it, finding the
header at `<gsm/gsm.h>`, and `gnuradio` links it for its vocoder blocks.

## Flags

- `CC=gcc` — Replaces upstream's `CC = gcc -ansi -pedantic`, whose `-ansi` would contradict the
  `-std=gnu11` in the tree's `CFLAGS`.
- `CCFLAGS="$_ccflags"` — Upstream's makefile builds its own `CFLAGS` out of several variables,
  and that assignment beats the environment's, so the tree's flags go in through `CCFLAGS`, which
  upstream sets to `-c -O2 -DNeedFunctionPrototypes=1 -Wall -Wno-comment`. The value here keeps `-c`,
  the prototypes and the warnings, takes the optimisation and hardening from the tree's
  [release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags), and names
  `-fPIC` again, though the tree's flags carry it already: the objects in `libgsm.a` must be
  position-independent to be linked into the shared library below.

## Patches

- `gsm-64bit.patch` — Declares `longword` and `ulongword` in `inc/private.h` as `int` and `unsigned int` instead of `long`. The coder's state assumes the 32-bit integer the comments name; with a 64-bit `long` the arithmetic saturates differently and the bitstream it writes is not GSM.

## Notes

- **Upstream builds only a static library.** `build.sh` makes the shared one itself, linking the
  whole of `lib/libgsm.a` into `libgsm.so.<version>` with the soname `libgsm.so.1`, and installs
  everything by hand: upstream's `install` target writes into `INSTALL_ROOT` with paths of its
  own and installs no shared library.
- **The header is in two places.** `gsm.h` goes to `/usr/include/gsm/gsm.h`, where `sox`'s
  `configure` looks first, with a link at `/usr/include/gsm.h` for code that includes it bare.
- The makefile's own `-DSASR` (`>>` is an arithmetic shift) and `-DWAV49` (the `GSM_OPT_WAV49`
  framing for GSM in WAVE files) are kept, because they are separate variables the build leaves
  alone.
