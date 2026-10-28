## About

`kdos-mountd` is the root daemon behind removable media on the KDOS desktop: it lists the disks
and partitions a session may use and mounts, unmounts, ejects, unlocks, formats and writes images
to them, reads SMART health, and mounts network shares, for a desktop that does not run as root.
The client never names a device or a mountpoint; it asks for a row out of a list the daemon
published, and the daemon chooses the device, the mountpoint and the options. Its clients are the
`kdos-devices`, `kdos-disks` and `kdos-connect` surfaces of [kdos-shell](#port-kdos-shell), the
session helper `kdos-mediad`, and `kdos-mount` on the command line.
[udisks2](#port-udisks2) serves the toolkit applications beside it, and this daemon stays the
authority for the shell. Every verb, what it refuses and `/etc/kdos/mountd.conf` are in
[kdos-mountd](../../../docs/kdos/04-programs/daemons.md#kdos-mountd).

The port is KDOS's own C, with no upstream source. `build.sh` compiles `main.c` with the sources
of `libkbase` and nothing else, because every library a root daemon links is code running as
root; see
[How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).

## Notes

One binary has two names: `/usr/sbin/kdos-mountd` serves `/run/kdos-mountd.sock`, and the link
`/usr/bin/kdos-mount` runs it as the client. The name it was started under is the whole switch.

The recipe names no dependencies, but the daemon runs other ports' programs, each by absolute
path: `eject` from [util-linux](#port-util-linux), `cryptsetup` from
[cryptsetup](#port-cryptsetup), `smartctl` from [smartmontools](#port-smartmontools),
`mount.cifs` from [cifs-utils](#port-cifs-utils), `avahi-browse` and
`avahi-resolve-host-name` from [avahi](#port-avahi), `nmblookup` from [samba](#port-samba),
`modprobe` from [kmod](#port-kmod), and the `mkfs` programs of
[e2fsprogs](#port-e2fsprogs), [btrfs-progs](#port-btrfs-progs), [dosfstools](#port-dosfstools)
and [exfatprogs](#port-exfatprogs). They reach the image through other phases' lists, not through
this recipe, and a program that moves to another path stops the verb that runs it. The paths are
listed in [Writing an image](../../../docs/kdos/04-programs/daemons.md#writing-an-image).
