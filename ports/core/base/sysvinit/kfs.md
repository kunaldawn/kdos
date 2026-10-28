## About
sysvinit supplies PID 1. KDOS installs eight commands from it and nothing else: `init`, its alias
`telinit`, `halt` with the aliases `reboot` and `poweroff`, `shutdown`, `runlevel` and `killall5`,
with their manual pages and those of `inittab(5)` and `initctl(5)`. `init` reads `/etc/inittab`,
runs `/etc/init.d/rcS` at boot and `rcK` at shutdown, and respawns the console logins, each of
which ends in `agetty`; services are kept running by `ksvc`, not by init. Why sysvinit was chosen, and the one way to ask it for a
runlevel, are in
[sysvinit as PID 1](../../../../docs/kdos/01-philosophy/decisions.md#sysvinit-as-pid-1);
the boot it drives is in
[rcS and the service scripts](../../../../docs/kdos/03-architecture/boot-and-init.md#rcs-and-the-service-scripts).

`init` and `halt` also write the boot, runlevel and shutdown records that `who -b`, `who -r` and
`last` read, so the build is against [utmps](#port-utmps), like every other reader and writer of
those records ([Login records](../../../../docs/kdos/03-architecture/boot-and-init.md#login-records)).
`init` writes a record into `/run/utmps/utmp` only if that file already exists and is writable,
and skips it without a word otherwise, which is why `06_utmps.sh` creates the file empty before
`utmpd` starts rather than leaving it to `utmpd`'s first request.

## Flags
- `VERSION=$version` — The version string compiled into the programs. Upstream's top-level Makefile
  reads it from `doc/Changelog` and passes it down; `build.sh` runs `make -C src` directly, so it
  passes the recipe's own version instead.
- `-I/usr/include/utmps` — Added to `CFLAGS`, so `<utmpx.h>` is utmps' header rather than musl's,
  whose functions are stubs that store nothing.
- `LDLIBS="-lutmps -lskarnet"` — Link the programs against `libutmps` and the skalibs library it
  is built on.

## Notes
`build.sh` names its five targets and installs them by hand; it does not run `make install`.
Upstream's install would also put down `last`, `mesg`, `wall`, `utmpdump`, `sulogin`,
`bootlogd`, `logsave`, `fstab-decode`, `readbootlog`, a `lastb` link and a `pidof` link. Several of
those names belong to other packages here (`last`, `lastb`, `mesg`, `wall`, `utmpdump` and `sulogin` to
[util-linux](#port-util-linux), `logsave` to [e2fsprogs](#port-e2fsprogs), `pidof`
to [procps-ng](#port-procps-ng)), and kpkg refuses a path a second package claims.

The programs go to `/sbin`, which on the installed system is a link to `usr/sbin`.
`telinit`, `reboot` and `poweroff` are symbolic links: `init` and `halt` look at the name they were
started under to decide what to do.
