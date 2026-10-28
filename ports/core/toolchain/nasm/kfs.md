## About

NASM is an assembler for the x86 instruction set, with its own Intel-style syntax. KDOS does not
run it directly; it is a build tool for the ports that carry hand-written SIMD kernels in NASM
syntax. The codecs (`ffmpeg`, `x264`, `x265`, `dav1d`, `svt-av1`, `libvpx`, `openh264`),
`libjpeg-turbo`, `libass`, `firefox-esr`, `librewolf`, `thunderbird`, `qemu` and the `limine`
boot loader all name it in `depends`. The tree builds those ports with their assembly on, as
[Writing ports](../../../../docs/kdos/05-developer/writing-ports.md#every-port)
sets out, and NASM is the assembler that assembly is written for.

The package is two programs, `nasm` and `ndisasm`, with their manual pages.

## Flags

- `CFLAGS="$CFLAGS -std=gnu17"` — adds `-std=gnu17` to the tree's compile flags, which already
  carry `-std=gnu11`. NASM's `configure` then appends the first C standard it finds the compiler
  accepts, and GCC takes the last `-std=` on its command line, so the standard NASM is compiled
  with is the one `configure` picks.
- `pa_cv_CFLAGS__std_c23=no` — answers `configure`'s "does the compiler accept `-std=c23`?" probe
  in advance with "no". NASM's `configure.ac` tries `-std=c23`, `-std=c17`, `-std=c11` and
  `-std=c99` in that order and keeps the first that works; with C23 ruled out it settles on
  `-std=c17`, so NASM is compiled as C17 whatever the compiler's newest standard is. The build log
  shows `-std=gnu11 … -std=gnu17 -std=c17 -U__STRICT_ANSI__` on every compile;
  `-U__STRICT_ANSI__` is a separate flag `configure` adds whichever standard it picks.
- `--with-zlib` — given without a path, this is what `configure` does anyway: it searches for
  zlib's `inflate` in `-lz` and links the system library when it finds it. Only when it finds none
  does it compile the copy of zlib's inflate code NASM carries under `zlib/`. `nasm` links
  `libz.so.1`; `ndisasm` links only the C library.
- `--disable-gdb`, `pa_cv_CFLAGS__g3=no`, `pa_cv_CFLAGS__ggdb=no` — NASM's `configure` adds `-g3`
  to every compile unconditionally and `-ggdb` unless `--disable-gdb` is given
  (`autoconf/m4/pa_option_debug.m4`). The `g3` answer stops `-g3`; `--disable-gdb` stops `-ggdb`,
  and the `ggdb` answer stops it a second time. No port ships debug information, and these are
  how this recipe keeps upstream's own `-g` out (see
  [What kpkg does around build.sh](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).
  The `--disable-gdb` here is NASM's switch for its debug flags, not a feature tied to the `gdb`
  port, so the "Optional, off in KDOS" entry above is not a lost feature.

## Notes

A `pa_cv_CFLAGS_<flag>` answer only works while upstream's probe keeps its name. The names are built
from the flag text by NASM's `PA_SHSYM` macro (`autoconf/m4/pa_shsym.m4`): every character that
cannot appear in a shell variable becomes `_`, and `+` becomes `x`. If a release renames a probe,
the answer is silently ignored and the flag it was meant to stop returns; read the `checking whether
gcc accepts …` lines of the build log after a version bump.