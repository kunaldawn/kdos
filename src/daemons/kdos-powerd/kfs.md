## About

`kdos-powerd` is the root daemon that lets a desktop which does not run as root change the
machine's settings that live in root's files: which named services the firewall lets in, the
accent colour of everything drawn before a login (the boot menu, the boot splash, the text console
and the login screen), and the splash's motion; and it starts Samba once an account can share folders.
It offers the same verbs on two faces. The `kdos`
command and the `kdos-power` client write one-line requests to `/run/kdos-powerd.sock`, where the
caller's credentials are read from the socket with `SO_PEERCRED`; System Settings calls
`io.kdos.Power1` on the system bus, where polkit decides each change. Starting Samba is on the bus
face alone, for [kded-kdos](#port-kded-kdos), which calls it at the login of a `sambashare` member. Suspend, power-off and
reboot are elogind's, and `kdos-power suspend`, `poweroff` and `reboot` run `loginctl` as the
caller without reaching the daemon. Every verb, the rules for each caller and the bus interface
are in [kdos-powerd](../../../docs/kdos/04-programs/daemons.md#kdos-powerd), and the pattern every
root daemon shares is in
[The shape they share](../../../docs/kdos/04-programs/daemons.md#the-shape-they-share).

The port is KDOS's own C, with no upstream source. `build.sh` compiles `main.c` and `bus.c` with
the sources of `libkbase` and `libkcolor` and links [basu](#port-basu) for the bus face and nothing
else, because every library a root daemon links is code running as root; see
[How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).

## Notes

One binary has two names. Installed as `/usr/sbin/kdos-powerd` it serves the socket and the bus;
the link `/usr/bin/kdos-power` runs it as the command-line client, which sends one request and
prints the answer. The name it was started under is the whole switch, so the link must keep that
name.

The recipe installs the bus policy `/usr/share/dbus-1/system.d/io.kdos.Power1.conf`, which lets
root own the name and anybody call it, and the polkit actions
`/usr/share/polkit-1/actions/io.kdos.power1.policy`: `io.kdos.firewall.set`, which needs an
administrator's password once per agent session, `io.kdos.accent.set`, which the person in the
active session is granted and which covers the splash's motion as well, and
`io.kdos.filesharing.start`, granted the same way, because the call takes no argument and starts
only what the next boot would. It installs no D-Bus
activation file: `/etc/init.d/55_powerd.sh` starts the daemon under `ksvc` and waits for the
name, and an activation file would let the bus start a second, unsupervised copy for a caller that
came early. The daemon answers the socket before it reaches the bus and retries a bus it cannot
reach every five seconds, so the `kdos` command still reaches the firewall on a machine whose
system bus is down.

`libkcolor` is linked for the accent. Its argument is matched against the closed list of colour
schemes compiled into the palette, which is the whole check on a verb a session that is not root
can reach, so the list has to be the palette's own rather than a copy in this program.

The firewall file it writes, `/etc/nftables.d/50-kdos-services.nft`, carries a named counter,
`kdos_knock_<name>`, for every closed service, which counts the packets from another machine that
service would have accepted before the input chain's policy drops them. Each counting line starts
with `fib saddr type != local`: this machine's own broadcasts, kdeconnectd's identity packet to
port 1716 among them, loop back in on the network interface rather than on `lo`, and counted they
would offer to let a phone in at every boot with no phone there. `ListFirewallKnocks` and `kdos-power firewall
knocks` read them with `nft list counters`; the Firewall page and `kdos-fwask` from
[kcm-kdos-firewall](#port-kcm-kdos-firewall) use them to offer to let a blocked program in. The
file shipped in the image is the daemon's own output with every service closed, so the counters
exist from the first boot.

The accent also restyles the login screen. The daemon copies one accent's set from
`/usr/share/kdos/greeter/<accent>/`, which [kdos-plasma-look](#port-kdos-plasma-look) builds, into
the `sddm` account's `~/.config` (`kdeglobals`, `plasmarc`, `kcminputrc`) and its
`~/kdos-background.png`, in a child process running as that account, so no file in a home another
account owns is ever written as root.

The `firewall` verb runs `/usr/sbin/nft` from [nftables](#port-nftables) and answers an error
without it, and `firewall ssh on|off` runs `/etc/init.d/70_sshd.sh`. `StartFileSharing` runs
`/etc/init.d/83_samba.sh start` from [samba](#port-samba), which starts `smbd` only when the
`sambashare` group has a member or a share exists, and refuses while `/etc/service.disabled/samba` exists or the script is missing. The accent runs `kdos-bootctl
theme` and the splash motion `kdos-bootctl splash`, from [kdos-tools](#port-kdos-tools), found
through the daemon's inherited `PATH` and skipped when absent; the reply then says the boot menu is
unchanged. The client's `suspend`, `poweroff` and `reboot` exec `loginctl` from
[elogind](#port-elogind) through `PATH`. polkitd, from [polkit](#port-polkit), is asked over the
bus, and a polkit that does not answer is a refusal. None of these is a dependency: each reaches
the image through a phase list, and the daemon still serves the rest without it.
