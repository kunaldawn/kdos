## About

seatd is a small [seat management]{.idx} daemon, and libseat the library programs use to talk to
it. A seat is the set of devices one person at the machine uses, the display and the input
devices; seatd opens them on behalf of an unprivileged compositor and takes them back when the
session switches away. On other distributions systemd-logind does this. KDOS has no logind or
elogind, so the compositor, `kdos-comp`, reaches the display through libseat and seatd: the
[wlroots](#port-wlroots) port depends on this one. See
[What kdos-desktop does](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#what-kdos-desktop-does).

`/etc/init.d/45_seatd.sh` starts it at boot under the supervisor as `seatd -g seat`, so members of
the `seat` group can take the seat without root. The installer keeps the desktop account in that
group.

## Flags

- `CFLAGS="$CFLAGS -Uunix"` — Undefines the macro `unix`. The tree's `CFLAGS` carry
  `-std=gnu11`, which comes after meson's `-std=c11` and wins, and in a GNU C dialect GCC predefines
  `unix` as `1`. seatd's socket code names a union member `unix` (`addr.unix.sun_family`), which
  the macro turns into `addr.1.sun_family`, a syntax error.
- `-Dlibseat-builtin=enabled` — Builds a seatd server into libseat itself, a backend a program
  can run without a separate daemon. It is off by default.
- `-Dlibseat-logind=disabled` — Builds no logind or elogind backend into libseat. Neither is
  installed, and `auto` would build one if either library were found.
- `-Dserver=enabled` — Builds the `seatd` daemon itself. It is upstream's default.
- `-Dexamples=disabled` — Builds no example programs. It is upstream's default.
- `-Dman-pages=enabled` — Builds the `seatd(1)` and `seatd-launch(1)` manual pages with
  [scdoc](#port-scdoc), which is in `depends` for this. `enabled` makes a missing `scdoc` a
  configure error.
