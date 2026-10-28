## About

LVM2 is the Linux [logical volume manager]{.idx}: it pools disks and partitions (physical volumes)
into volume groups and carves those into logical volumes that can be resized, snapshotted, mirrored,
thin-provisioned or cached, all through the kernel's device-mapper. The port builds both halves of
the source tree: the `lvm` tool with its `pvcreate`, `vgchange`, `lvcreate` and other names, and
`libdevmapper` with `dmsetup`, which other storage ports link: [cryptsetup](#port-cryptsetup),
[parted](#port-parted), [libblockdev](#port-libblockdev) and [udisks2](#port-udisks2) among them.
`/etc/init.d/03_lvm.sh` activates the volume groups at boot, and the initramfs activates the one a
root on a logical volume needs before that.

## Flags

- `THIN_CONFIGURE_WARN=y`, `CACHE_CONFIGURE_WARN=y` — Set in configure's environment, these mark the
  thin and cache tools as already warned about, which makes configure skip its `thin_check -V` and
  `cache_check -V` probes. The tools come from
  [thin-provisioning-tools](#port-thin-provisioning-tools), which links this port's `libdevmapper`
  and is therefore built after it, so the probes would find nothing and turn `--needs-check` off.
  configure's closing warning that the tools are missing then describes the build machine, not the
  system the package goes on.
- `CONFIG_SHELL=/bin/bash` — The shell autoconf's configure runs its tests under and writes into the
  scripts it generates, such as `config.status`.
- `--libexecdir=/usr/lib` — lvm2's configure resets `libexecdir` from its own `--with-libexecdir`
  option, which defaults to `$prefix/libexec`, so this switch moves nothing: `lvresize_fs_helper` is
  installed in `/usr/libexec`.
- `--enable-cmdlib` — Builds `liblvm2cmd`, the LVM commands as a shared library. dmeventd's LVM
  plugin links it to run LVM commands. `--enable-dmeventd` turns it on by itself and configure
  refuses `--disable-cmdlib` beside it; the switch states the setting outright.
- `--enable-pkgconfig` — Installs `devmapper.pc` and the other pkg-config files, which is how the
  ports that link `libdevmapper` find it.
- `--enable-udev_sync` — LVM and device-mapper wait for udev to finish processing a device before
  they go on, so the `/dev/<vg>/<lv>` links exist when a command returns. It also installs the
  device-mapper udev rules, which create those links. Needs [eudev](#port-eudev).
- `--enable-dmeventd` — Builds dmeventd, the device-mapper event daemon. It is what makes
  `lvm.conf`'s monitoring work: a thin pool that extends itself when it fills, and repair of a RAID
  or mirror volume after a failed leg.
- `--enable-readline` — Command-line editing and history in the interactive `lvm` shell, from
  [readline](#port-readline). Named, a missing readline is a configure error rather than a shell
  without editing.
- `--with-blkid`, `--enable-blkid_wiping` — Use util-linux's libblkid to detect the filesystem and
  RAID signatures `lvcreate` and `pvcreate` offer to wipe, in place of LVM's own scanner. Enabled
  outright, a missing libblkid stops configure.
- `--with-libnvme`, `--enable-nvme-wwid` — Use [libnvme](#port-libnvme) to find an NVMe device's
  alternate WWIDs, among the device IDs `lvmdevices` records for a device. Enabled outright, a
  missing libnvme stops configure.
- `--with-thin-check=/usr/sbin/thin_check` and the seven other `--with-thin-…` and `--with-cache-…`
  paths — The programs from thin-provisioning-tools that LVM runs before it activates a thin pool or
  a cached volume. Each is written into the installed `lvm.conf` and into the tools as the default.
  Given outright, configure does not search for them; built before thin-provisioning-tools, this
  port would find none. `/usr/sbin` is where that port installs them.
- `--enable-thin_check_needs_check`, `--enable-cache_check_needs_check` — LVM passes the tools
  `--clear-needs-check-flag`, which `thin_check` understands from 0.3 and `cache_check` from 0.5.4.
  Both are on by default, and configure turns each off when its version probe finds an older tool
  or none at all; with the probes skipped, the switches state the setting outright.
- `--without-systemd` — No `libsystemd`, for the journal and the machine ID. KDOS has no systemd
  (see
  [No systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--with-default-event-activation=0` — Writes `event_activation = 0` as the default in `lvm.conf`.
  Event activation means udev activates a volume group as soon as its last physical volume appears;
  upstream does that through `69-dm-lvm.rules`, which runs `systemd-run … vgchange -aay`. With it
  off, `lvm pvscan --checkcomplete` in that rule does nothing.
- `--disable-selinux` — No libselinux; the tree has no SELinux.

## Patches

- `musl-stdio-freopen.patch` — musl declares `stdin`, `stdout` and `stderr` as `FILE *const`, so a program cannot assign a new stream to them. In `tools/lvmcmdline.c`, where LVM opens `/dev/null` on whichever of the three descriptors is closed, the patch replaces each `stdin = fopen(…)` with `freopen(…, stdin)`. In `lib/commands/toolcontext.c` it turns off, everywhere except glibc, the block that changes the buffering of the standard streams by reopening them, which stores the new stream through those same pointers and faults.

## Notes

- **`69-dm-lvm.rules` is deleted from the package.** With event activation off it would only run
  `lvm pvscan` on every block device event, and the activation it exists for goes through
  `systemd-run`, which does not exist here. Nothing on hotplug activates a volume group: the boot
  does it in `03_lvm.sh`, and by hand it is `vgchange -aay`.
- **Thin and cached volumes need [thin-provisioning-tools](#port-thin-provisioning-tools).** LVM
  refuses to activate a thin pool or a cached volume when `thin_check` or `cache_check` cannot be
  run, and that port is in the phase list for that reason.
