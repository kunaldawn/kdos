## About

This port is util-linux's `login`, the program `agetty` hands a console login to, and nothing
else from util-linux. It is built against PAM, so a login on a text console runs the
`/etc/pam.d/login` stack this port installs, which includes `system-login` from
[pam](#port-pam): the password is checked by `pam_unix`, [turnstile](#port-turnstile) gives the
user `/run/user/<uid>`, the session bus and audio, and [elogind](#port-elogind) registers the
login as a session on `seat0`. A console login is then an active local session like a Plasma one,
so polkit's `allow_active` answers apply to it.

It is a second build of the [util-linux](#port-util-linux) tarball. util-linux is a
`30_foundation` port, built before PAM exists, and its `login` needs PAM's headers; this port
configures the same tarball again in `41_system`, once [pam](#port-pam) is installed, and packages
only `login` and its manual page. Its `version` and `sha256` must equal util-linux's, and both
recipes carry `group = util-linux`, so the version checker offers the two only as one bump (see
[A second build of the same source](../../../../docs/kdos/05-developer/writing-ports.md#a-second-build-of-the-same-source)).
[shadow](#port-shadow) installs no `login`, so this is the only one.

## Flags

- `--disable-all-programs`, `--enable-login` — Configure for `login` alone. Every other program
  and library of the tarball is util-linux's package, and building them again here would give two
  packages the same paths.
- `--enable-login-lastlog` — `login` records each login in `/var/log/lastlog`, which shadow's
  `lastlog` reads. util-linux's default is off.
- `--without-systemd`, `--without-systemdsystemunitdir`, `--without-udev`, `--without-python`,
  `--without-econf` — The same as util-linux's: no systemd journal, units, libudev, Python
  bindings or libeconf.
- `--without-selinux`, `--without-audit` — No SELinux context and no audit record for a login.
  Neither library is a port; stated, so a build root that happens to have either cannot change
  the program.
- `--disable-asciidoc`, `--disable-poman`, `--disable-nls` — The same as util-linux's: the
  release tarball carries `login.1` already built, and there are no translations.
- `CPPFLAGS=-I/usr/include/utmps` — Compile against [utmps](#port-utmps)' `<utmpx.h>`, so `login`
  writes the utmp and wtmp records `who` and `last` read. The `make` line is given
  `LIBS="-lutmps -lskarnet"`, as util-linux's is.

## Notes

`make login` builds the one program, and `build.sh` installs it and `login.1` by hand, so the
package holds `/usr/bin/login`, `/usr/share/man/man1/login.1` and the PAM service file, and
nothing util-linux's package also installs.

`/etc/pam.d/login` includes `system-login` for all four module types, so a console login is
registered with [elogind](#port-elogind) and handed to [turnstile](#port-turnstile) exactly as a
desktop login is. Before that, `pam_faildelay` holds a failed attempt for three seconds and
`pam_nologin` refuses everybody but root while `/etc/nologin` exists. Without the file `login`
would fall through to `/etc/pam.d/other`, which refuses everybody, and the recovery console on tty2
would refuse every password; `pam_elogind` and `pam_turnstile` are optional in `system-login`, so
it still logs in with elogind or `turnstiled` down. util-linux's `login` opens
the PAM service `login` for a console login and `remote` for one started with `-h`; nothing on
KDOS starts it with `-h`, so no `remote` file ships, and `/etc/pam.d/other` would refuse such a
login.

`login` sets `MAIL` to `/var/mail/<user>` whatever `login.defs` says. Nothing on KDOS delivers to
that directory, which does not exist; a mail reader that follows `MAIL` finds no mailbox.

The recipe names [shadow](#port-shadow) in `replaces` and `depends`: shadow's package does not
install `login`, and in a tree whose installed shadow package still holds `/usr/bin/login`,
`replaces` lets this package take the path over instead of being refused.
