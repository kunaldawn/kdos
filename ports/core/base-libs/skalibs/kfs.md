## About
skalibs is skarnet.org's general-purpose C library, `libskarnet`, the base every other skarnet
program is written on. In KDOS it is there for [utmps](#port-utmps), the login-records service that
`who`, `last`, `login` and the rest read and write through (see
[Login records](../../../../docs/kdos/03-architecture/boot-and-init.md#login-records)),
and for [execline](#port-execline) and [s6](#port-s6), whose `s6-ipcserver`, `s6-setuidgid` and
`execline-cd` run utmps' two daemons. Every program built against utmps also
links libskarnet: [coreutils](#port-coreutils) passes `LIBS="-lutmps -lskarnet"`.

skalibs is not configured with autoconf: its `configure` is skarnet's own shell script, with its
own options and defaults.

## Flags
- `--prefix=/usr` — Install under `/usr`. This configure's own default prefix is `/`, which would put
  the shared library in `/lib`.
- `--enable-pkgconfig` — Install `libskarnet.pc`, which upstream leaves out by default. utmps,
  execline and s6 are configured with `--with-pkgconfig` and find skalibs through it.
- `--disable-rpath` — Do not embed an `RPATH` naming the library directory in the shared library,
  which upstream does by default. With `--prefix=/usr` that entry would be `/usr/lib`, a directory
  the loader searches anyway, and the package lint warns about such an entry (see
  [The package lint](../../../../docs/kdos/03-architecture/packaging.md#the-package-lint)).

## Notes
The package installs `/usr/lib/skalibs/sysdeps`, the record of what configure found on this system.
The configure scripts of utmps, execline and s6 read it from there by default and build against what
it records.
