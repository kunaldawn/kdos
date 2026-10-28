## About

GNU diffutils is `diff`, `cmp`, `diff3` and `sdiff`: the programs that compare files line by line
or byte by byte. KDOS builds them early because other builds run them. An autoconf configure
script runs `diff` while it picks a `grep` and a `sed`, and again before it rewrites `config.h`,
and the self-tests some builds run as part of `make` compare their output with `cmp`:
[bzip2](#port-bzip2)'s default target checks its own output with `cmp`, so without diffutils its
build stops. `20_selfhost` rebuilds the bootstrap's copy as a package, with
`m4` and `gawk`, because the GMP and GCC builds of that phase run these tools (see
[The toolchain rebuilt as packages](../../../../docs/kdos/05-developer/how-kdos-is-built.md#the-toolchain-rebuilt-as-packages-20_selfhost)).

## Flags

- `--enable-threads=posix` — tells gnulib, the portability library diffutils is built with, to
  use POSIX threads for its locks and once-only initialisation. On Linux configure's own default,
  `yes`, takes the same branch of gnulib's thread check and ends in the same choice; the switch
  names it.
