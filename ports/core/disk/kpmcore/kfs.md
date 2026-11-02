## About

KPMcore is the engine of KDE Partition Manager: it reads the disks and their partition tables,
models every filesystem it knows, and runs the operations (create, delete, resize, move, copy,
check, label, format, encrypt) as a queue of jobs. The library itself runs as the user. Every
command that touches a device goes through `kpmcore_externalcommand`, a root helper that D-Bus
starts on the system bus when the library first asks for it, after polkit has allowed the
request. [partitionmanager](#port-partitionmanager) and the KDOS installer's manual layout are
built on it.

## Flags

- `-D PARTMAN_SFDISKBACKEND=ON` — Builds the sfdisk backend, `pmsfdiskbackendplugin`, the one
  backend for Linux disks and the library's default. It reads disks with `lsblk`, `blkid` and
  `sfdisk --json` and writes partition tables with `sfdisk`, all from
  [util-linux](#port-util-linux). Upstream's default is the same; the switch is pinned because
  without it the library finds no disks.
- `-D PARTMAN_DUMMYBACKEND=OFF` — Builds no dummy backend. It is a stand-in for the test suite
  that reports an invented disk, and Partition Manager's settings would offer it as a backend a
  person could choose.
- `-D KF_SKIP_PO_PROCESSING=ON` — Builds none of the translation catalogues under `po/`, which
  the package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Notes

The helper is installed as `/usr/lib/libexec/kpmcore_externalcommand` with a system-bus activation
file, `org.kde.kpmcore.helperinterface.service` (`User=root`) under
`/usr/share/dbus-1/system-services`, and a bus policy under
`/usr/share/dbus-1/system.d` that lets only root own the name. D-Bus starts it through the setuid
`dbus-daemon-launch-helper` of [dbus](#port-dbus). Its one polkit action,
`org.kde.kpmcore.externalcommand.init`, is `auth_admin_keep` for an active session and `no` for
an inactive one, with no `allow_any`: a request from outside an active elogind session is refused
outright, and one from the person at the machine raises the authentication agent's prompt for an
administrator's password.

The helper runs only the commands on its compiled allowlist, and only from the trusted prefixes
`/`, `/usr` and `/usr/local`, looking in `bin/` before `sbin/` under each. A toybox applet in
`/usr/bin` therefore wins over a util-linux tool of the same name in `/usr/sbin`; each tool it
runs must be the real one wherever both exist.

Every filesystem is supported only as far as its tools are installed, so the run-time tools are
in `depends`: [e2fsprogs](#port-e2fsprogs), [dosfstools](#port-dosfstools),
[exfatprogs](#port-exfatprogs), [btrfs-progs](#port-btrfs-progs), [xfsprogs](#port-xfsprogs),
[f2fs-tools](#port-f2fs-tools), [ntfs-3g](#port-ntfs-3g), [udftools](#port-udftools),
[nilfs-utils](#port-nilfs-utils) and [jfsutils](#port-jfsutils); LUKS through
[cryptsetup](#port-cryptsetup), LVM through [lvm2](#port-lvm2), software RAID through
[mdadm](#port-mdadm), and the SMART status page through `smartctl` from
[smartmontools](#port-smartmontools). `udevadm` from [eudev](#port-eudev) waits for the kernel to
settle after a table changes. ReiserFS, OCFS2, HFS+ and bcachefs have no tools in the tree, so
Partition Manager offers no operation on those filesystems beyond deleting the partition.
