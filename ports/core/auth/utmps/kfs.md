## About
utmps is skarnet's implementation of the `utmpx` interface: who is logged in at the moment, and the history
of logins, logouts, boots and shutdowns. musl's own `utmp` and `utmpx` functions are stubs that
store nothing, so on KDOS these records exist only because of this package. It installs
`libutmps`, which sends each call over a socket, and the two daemons that answer it,
`utmps-utmpd` and `utmps-wtmpd`, which `/etc/init.d/06_utmps.sh` starts under the supervisor. What
each daemon keeps, and who may write to it, is in
[Login records](../../../../docs/kdos/03-architecture/boot-and-init.md#login-records)
and [utmpd and wtmpd](../../../../docs/kdos/04-programs/daemons.md#utmpd-and-wtmpd).

Every reader and writer of the records must link `libutmps`, because utmps' record layout is not
musl's: [sysvinit](#port-sysvinit), [util-linux](#port-util-linux), [shadow](#port-shadow),
[coreutils](#port-coreutils) and [procps-ng](#port-procps-ng) are all built against it. It is built
on [skalibs](#port-skalibs), and the daemons run behind `s6-ipcserver` from [s6](#port-s6), which
is needed only when they run.

## Flags
- `--enable-shared` — Build `libutmps.so` as well as `libutmps.a`. skarnet's configure builds
  static libraries only unless this is given.
- `--disable-allstatic` — Link the programs against shared libraries, skalibs' `libskarnet.so`
  among them, instead of skarnet's default of preferring the static archives.
- `--enable-pkgconfig` — Write and install `libutmps.pc`.
- `--with-pkgconfig` — Find skalibs through `pkg-config` while configuring, and stop if it is
  missing or too old.
- `--enable-utmp` — Also install a `utmp.h` with the Linux-specific `utmp` declarations, beside
  `utmpx.h`, for programs that include the older header.
- `--disable-rpath` — Link `libutmps.so` with no run-time library path. Left on, it would carry
  `-rpath=/usr/lib`, a directory the dynamic loader searches anyway.

## Notes
The headers go to `/usr/include/utmps/`, not over musl's `<utmpx.h>` and `<utmp.h>`: the switch
that would overwrite musl's, `--enable-libc-includes`, is not passed. A consumer therefore has to
put that directory ahead of the C library's headers and link the library by name, which is why the
recipes of its consumers pass `-I/usr/include/utmps` (in `CPPFLAGS`, or in `CFLAGS` for sysvinit)
and `-lutmps -lskarnet`. A consumer
built without them compiles and links cleanly against musl's stubs, and its `who` or `last` then
reports no one.

`runlvl-replace.patch` makes `utmps-utmpd` keep a single runlevel record, replacing it at each
change of runlevel as glibc does, so that `who -r` prints one line and `runlevel` the current
runlevel rather than the one the machine booted into.

The socket paths are compiled in at their defaults, `/run/utmps/.utmpd-socket` and
`/run/utmps/.wtmpd-socket`, which are the paths `06_utmps.sh` serves.
