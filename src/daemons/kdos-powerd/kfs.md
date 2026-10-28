## About

`kdos-powerd` is the root daemon that lets a desktop which does not run as root suspend, power
off and reboot the machine, and lets an administrator's session make the few `/etc` changes the
desktop offers: the time zone, the autologin account, the firewall's named services and the
accent colour of the boot splash, the boot menu and the text console. It answers one-line
requests on `/run/kdos-powerd.sock` and decides who may ask from
the caller's credentials, read from the socket with `SO_PEERCRED`; KDOS has no logind, and no polkit
prompt can answer for a closing lid. Its clients are the panel's and the Start menu's power items,
the power and sleep keys, the compositor's lid handler, several `kdos-shell` surfaces and
`kdos theme`. Every verb, the rules for each caller and the order of a suspend are in
[kdos-powerd](../../../docs/kdos/04-programs/daemons.md#kdos-powerd),
and the pattern every root daemon shares is in [The shape they share](../../../docs/kdos/04-programs/daemons.md#the-shape-they-share).

The port is KDOS's own C, with no upstream source. `build.sh` compiles `main.c` with the sources
of `libkbase` and `libkcolor` and nothing else, because every library a root daemon links is code
running as root; see
[How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).

## Notes

One binary has two names. Installed as `/usr/sbin/kdos-powerd` it serves the socket; the link
`/usr/bin/kdos-power` runs it as the command-line client, which sends one request and prints the
answer. The name it was started under is the whole switch, so the link must keep that name.

`libkcolor` is linked for one verb, `accent`. Its argument is matched against the closed list
of colour schemes compiled into the palette, which is the whole check on a verb a session that is
not root can reach, so the list has to be the palette's own rather than a copy in this program.

The recipe depends on [sysvinit](#port-sysvinit) because `poweroff` and `reboot` run
`/sbin/telinit`. Every other program it uses reaches the image through a phase list, not through
this recipe. The suspend hooks run `/usr/bin/dbus-send` (to put NetworkManager to sleep) and
`/usr/sbin/tlp` when they are installed and are skipped when they are not, so neither
[dbus](#port-dbus) nor [tlp](#port-tlp) is a dependency. The `firewall` verb runs `/usr/sbin/nft`
from [nftables](#port-nftables) and answers an error without it. `timezone` runs `iw` from
[iw](#port-iw) and `accent` runs `kdos-bootctl` from [kdos-tools](#port-kdos-tools), each found
through the daemon's inherited `PATH` and skipped when absent. The `kdos-power suspend` client
starts `kdos-lock` from [kdos-lock](#port-kdos-lock) before it asks for the suspend.
`/etc/init.d/55_powerd.sh` starts the daemon at boot under `ksvc`.
