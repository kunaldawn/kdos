## About

GNU awk is the pattern-scanning language most shell scripts and build files reach for to cut a
field out of a line or total a column. On KDOS it is the only awk: the package installs
`/usr/bin/awk` as a link to `gawk` (see
[The base userland](../../../../docs/kdos/03-architecture/packaging.md#the-base-userland)).
Configure scripts and makefiles call it as plain `awk` without the port naming it in `depends`,
and the GMP and GCC builds of `20_selfhost` are among them, which is why that phase rebuilds it as
a package. A few ports do name it, among them [tzdata](#port-tzdata) and
[iana-etc](#port-iana-etc), whose builds generate files with gawk programs.

The bootstrap build in [Part III](#03-bootstrap-065-gawk) has neither readline nor MPFR. This one
links both, so `20_selfhost` installs [gmp](#port-gmp), [mpfr](#port-mpfr), [ncurses](#port-ncurses)
and [readline](#port-readline) before it.

## Flags

- `--sysconfdir=/etc` — gawk installs `gawk.sh`, the shell functions that set `AWKPATH` and
  `AWKLIBPATH`, into `$sysconfdir/profile.d`. With `/etc` the login shell's profile finds it.
- `--with-mpfr` — build arbitrary-precision arithmetic, `gawk -M`, against MPFR and GMP. This is
  configure's default; the switch does not stop configure when MPFR is missing, it only states that
  it is wanted. The `depends` line is what guarantees MPFR and GMP are there.
- `--with-readline` — give the awk debugger, `gawk -D`, command-line editing and history through
  readline. As with MPFR, this is the default and missing readline would be skipped silently;
  `depends` makes it present.

## Notes

The two `rm` lines leave out two files the install writes. `gawk.csh` is the C-shell version of the
profile script, and no csh or tcsh is a port. `gawknotes.info` is *GNU Awk Maintainer Notes*, a
manual for people who work on gawk's own source.

Every optional library gawk can use is either named in `depends` or switched off
(`--disable-nls`). A library left to configure's detection would turn its feature on or off
according to what is installed when the port builds, and the same recipe builds in `20_selfhost`
and again in `30_foundation` whenever its hash changes.
