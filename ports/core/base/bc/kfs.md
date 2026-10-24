## About
GNU bc is an arbitrary-precision calculator language, and the package also installs `dc`, the
reverse-Polish calculator. The [linux](#port-linux) port depends on it to build the kernel.

## Flags
- `--with-readline` — Give `bc`'s interactive prompt line editing and history through GNU readline.
  configure links `-lreadline` together with a terminal library, `-lncurses` when ncurses provides
  `tparm` and `-ltermcap` otherwise, which is why the recipe depends on [readline](#port-readline)
  and [ncurses](#port-ncurses). When readline is not found, configure prints no warning and builds
  without it.
- `--mandir=/usr/share/man` — Where the `bc(1)` and `dc(1)` pages go; with `--prefix=/usr` it is
  also the default.

## Notes
configure stops with "readline works only with flex" when readline is in use and the lexer it
finds is not flex. [flex](#port-flex) is not in the recipe's `depends`; it is in the phase's opening
run, which is installed before any shelf section.

The recipe depends on [texinfo](#port-texinfo) because `make` rebuilds the info manuals.
`bc.info` and `dc.info` ship in the source, but each depends on `texi-ver.incl`, which configure
writes, so the generated file is always newer and `makeinfo` runs.
