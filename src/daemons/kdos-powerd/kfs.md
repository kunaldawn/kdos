## About

`kdos-powerd` is the root daemon that lets a desktop which does not run as root suspend, power
off and reboot the machine, and lets an administrator's session make the few `/etc` changes the
desktop offers: the time zone, the autologin account, the firewall's named services and the boot
accent colour. It answers one-line requests on `/run/kdos-powerd.sock` and decides who may ask from
the caller's credentials, read from the socket with `SO_PEERCRED`; KDOS has no logind, and no polkit
prompt can answer for a closing lid. Its clients are the panel's and the Start menu's power items,
the power and sleep keys, the compositor's lid handler, several `kdos-shell` surfaces and
`kdos theme`. Every verb, the rules for each caller and the order of a suspend are in
[kdos-powerd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/daemons.md#kdos-powerd),
and the pattern every root daemon shares is in [The shape they share](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/daemons.md#the-shape-they-share).

The port is KDOS's own C, with no upstream source. `build.sh` compiles `main.c` with the sources
of `libkbase` and `libkcolor` and nothing else, because every library a root daemon links is code
running as root; see
[How the libraries are built](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).

## Notes

One binary has two names. Installed as `/usr/sbin/kdos-powerd` it serves the socket; the link
`/usr/bin/kdos-power` runs it as the command-line client, which sends one request and prints the
answer. The name it was started under is the whole switch, so the link must keep that name.

`libkcolor` is linked for one verb, `accent`. Its argument is matched against the closed list
of colour schemes compiled into the palette, which is the whole check on a verb an unprivileged
session can reach, so the list has to be the palette's own rather than a copy in this program.

The recipe depends on [sysvinit](#port-sysvinit) because `poweroff` and `reboot` run
`/sbin/telinit`. The suspend hooks run `/usr/bin/dbus-send` and `/usr/sbin/tlp` when they are
installed, and are skipped when they are not, so neither [dbus](#port-dbus) nor
[tlp](#port-tlp) is a dependency. `/etc/init.d/55_powerd.sh` starts it at boot under `ksvc`.
