# Disk health

A KDOS machine watches its disks with [smartd]{.idx}, which polls every disk that answers [SMART]{.idx} and
writes what it finds to the system log, and with `xfs_healer` on each [XFS]{.idx} [filesystem]{.idx} mounted at
boot. This page shows which ports provide the tools, how the two watchers are started and
configured, and the tasks: reading a disk's health, running a self-test, routing smartd's warnings,
checking an NVMe drive, setting a drive's power behaviour and copying data off a disk that is
failing. Why each watcher skips itself or stops for good on some exits is in the KDOS book's
[Services that skip themselves](../../kdos/02-user-guide/administration.md#services-that-skip-themselves);
this page does not repeat it.

## Installation

| Port | Provides |
|---|---|
| [smartmontools](#port-smartmontools) | [`smartctl`]{.idx}, which reads a disk's SMART data and starts its self-tests, and the `smartd` daemon |
| [nvme-cli](#port-nvme-cli) | `nvme`, for an NVMe drive's own logs and settings |
| [hdparm](#port-hdparm) | `hdparm`, which sets an ATA drive's power management and spindown |
| [xfsprogs](#port-xfsprogs) | `xfs_healer`, `xfs_scrub` and [`xfs_property`]{.idx} |
| [ddrescue](#port-ddrescue) | [`ddrescue`]{.idx}, which copies a failing disk with retries and a resumable map file |
| [partclone](#port-partclone) | Images a healthy filesystem by copying only its used blocks |
| [f3](#port-f3) | `f3probe`, `f3write` and `f3read`, which find a stick that reports a false size |
| [libatasmart](#port-libatasmart) | The SMART library udisks2 reads for the native disk applications |

All are on the live medium and on an installed system. Two boot scripts come from the tree's `fs/`
overlay, and both are enabled on the image (`/etc/service.disabled/` holds no marker for either):

| Script | Service name | Starts | Skips or stops |
|---|---|---|---|
| `52_smartd` | `smartd` | `smartd -n` under the [supervisor]{.idx term="supervisor (ksvc)"} | Stops for good, without a restart, when no disk answers SMART (exit 17), as in any [virtual machine]{.idx}, or when [`/etc/smartd.conf`]{.idx} cannot be parsed (2) or read (6) |
| `53_xfs_healer` | `xfs_healer` | One `xfs_healer` per XFS filesystem mounted when it runs, each named `xfs_healer-N` | Skips a filesystem whose kernel has no XFS health monitor or whose `autofsck` property is `none`, and prints `no XFS filesystem to watch` when none is left |

[`kdos-mountd`]{.idx} answers a SMART health question about a removable disk for the desktop, because no
session may open a raw block device; see [SMART](../../kdos/04-programs/daemons.md#smart).

## Configuration

**`/etc/smartd.conf`** is smartmontools' own file, installed by its `make install` into the
`--sysconfdir` the port names. Its one active line is `DEVICESCAN`: watch every disk smartd can
find, with the default checks, every thirty minutes. The port's configure line, from
`ports/core/disk/smartmontools/build.sh`:

<!-- kfs:include ports/core/disk/smartmontools/build.sh lines=12-23 pin=b0c001196f6b -->

`--with-nvme-devicescan` makes `DEVICESCAN` include NVMe drives. `--without-update-smart-drivedb`
installs no `update-smart-drivedb`, so the drive database is the one built into this release of
smartmontools. `--with-initscriptdir=no` and the `systemd` switches install no service file of
upstream's; the tree's own script starts the daemon.

**[`/etc/init.d/52_smartd.sh`]{.idx}**, from `fs/etc/init.d/52_smartd.sh`:

<!-- kfs:include fs/etc/init.d/52_smartd.sh lines=21-30 pin=e006444ce1da -->

`-n` keeps `smartd` in the foreground, where the supervisor needs it, and the three
`--final-exit` codes are the exits a restart cannot change.

**[`/etc/init.d/53_xfs_healer.sh`]{.idx}**, from `fs/etc/init.d/53_xfs_healer.sh`, walks the mount table
once at boot:

<!-- kfs:include fs/etc/init.d/53_xfs_healer.sh lines=34-57 pin=f1f26584457b -->

**The `autofsck` property** of each XFS filesystem decides what its healer does, and is read and
set with `xfs_property` (`xfs_healer(8)`):

| `autofsck` | `xfs_healer` |
|---|---|
| `repair` | Has the kernel repair damaged metadata online |
| `check`, `optimize` | Logs the damage |
| `none` | Exits; the boot script skips that filesystem |
| not set | Logs the damage when the filesystem has back-reference metadata, and exits when it has none |

## Usage

Every command here needs root, because SMART is read from the raw block device. In a virtual
machine the disks answer no SMART and every command below reports that it cannot:
`smartctl --scan` lists nothing, a virtio disk answers `Unable to detect device type`, and an
emulated USB stick `Unknown USB bridge`.

### Reading a disk's health

1. List the disks smartctl can reach:

   ```bash
   sudo smartctl --scan
   ```

2. Ask one for its verdict, and for everything it reports:

   ```bash
   sudo smartctl -H /dev/sda
   sudo smartctl -x /dev/sda
   ```

   `-H` prints `PASSED` or `FAILED`. In `-x`, the reallocated, pending and uncorrectable sector
   counts, and the error log, show a drive in decline before the verdict changes.

For a removable disk from the desktop, select it in `kdos-disks` and press `s`, or from a terminal
as yourself:

```bash
kdos-mount list
kdos-mount smart 0
```

which prints `ok` and the model, the serial and the health line, tab-separated, or `ok -	-	-`
for a disk that answers no SMART, such as an emulated USB stick in a virtual machine. Only disks
`kdos-mountd` lists can be asked this way, which leaves out every internal disk.

### Running a self-test

1. Start a short test; the drive runs it in the background:

   ```bash
   sudo smartctl -t short /dev/sda
   ```

   `smartctl` prints how long it will take. `-t long` reads the whole surface and takes hours.

2. When it is done, read the self-test log:

   ```bash
   sudo smartctl -l selftest /dev/sda
   ```

### Checking an NVMe drive

```bash
sudo nvme list
sudo nvme smart-log /dev/nvme0
sudo nvme error-log /dev/nvme0
```

`smart-log` reports the drive's spare capacity, the percentage of its rated endurance used, its
temperature and its media errors. `sudo smartctl -x /dev/nvme0` reports the same through
smartmontools.

### Reading what smartd found

`smartd` logs through syslog under its own name, and its supervisor keeps its output in `/run`:

```bash
grep -a smartd /var/log/messages
cat /run/kdos-svc.smartd.log
```

A failing health check, a growing count of pending or uncorrectable sectors, a new entry in the
drive's error log and a failed self-test are each logged. The supervisor's own lines go to the
system log as well, tagged `smartd:`, so the first grep also shows when the daemon started and
how it exited.

### Sending smartd's warnings somewhere else

The system log is the only route on the image. To add one, give the `DEVICESCAN` line in
`/etc/smartd.conf` a `-m` address and an `-M exec` program, which smartd runs with the warning in
its environment (`smartd.conf(5)`):

1. Edit the line with `sudoedit /etc/smartd.conf`, for example to
   `DEVICESCAN -m root -M exec /usr/local/sbin/disk-warning`, and write that program.
2. Check the file and the disks once, without leaving a daemon running:

   ```bash
   sudo smartd -q onecheck
   ```

   It prints how it read each line (`Drive: DEVICESCAN, implied '-a' Directive on line 18 of file
   /etc/smartd.conf` for the shipped one) and what it found, and exits with the code the daemon
   would: 17 when no disk answers SMART.

3. Restart the daemon:

   ```bash
   sudo service restart smartd
   ```

smartd's own warning script, [`/etc/smartd_warning.sh`]{.idx}, sends `-m` mail with the `mail` command,
which the image does not carry, so `-m` alone delivers nothing. `-M exec` with a program of your own
is the route that works.

### Repairing XFS online

A filesystem made by this `mkfs.xfs` already carries `autofsck=repair`; `xfs_property /srv/data
get autofsck` says so. For one made elsewhere, or set otherwise:

1. Set the property on the mounted filesystem:

   ```bash
   sudo xfs_property /srv/data set autofsck=repair
   ```

2. Restart the healers so the boot script reads it again:

   ```bash
   sudo service restart xfs_healer
   service status xfs_healer
   ```

   The restart prints `[KDOS] Starting xfs_healer for /srv/data...`, and the status lists one
   healer per filesystem, `[ OK ] xfs_healer-17  (pid …)`; the number is the filesystem's line
   in `/proc/self/mounts`. A restart prints its output to the terminal and leaves
   `/run/kdos-init.xfs_healer.log` holding what the boot printed.

`sudo xfs_scrub /srv/data` checks the same filesystem on demand, and ends with what it found and
what it changed (`/srv/data: optimizations made: 3.`). An XFS filesystem mounted after boot, by
hand or by `kdos-mountd`, has no healer until the service is restarted.

### Setting a drive's power behaviour

For an ATA drive, as root:

```bash
sudo hdparm -I /dev/sda          # what the drive supports
sudo hdparm -B 254 /dev/sda      # advanced power management: 128 to 254 never spin down, 1 to 127 may
sudo hdparm -S 120 /dev/sda      # spin down after 120 × 5 seconds idle
```

No boot script applies either setting; `hdparm(8)` describes each value.

### Copying data off a failing disk

A disk that is failing is read as little as possible, and once. Copy it whole to an image on another
disk with `ddrescue`, keeping the map file, which lets the copy stop and resume (`ddrescue`'s
manual, `info ddrescue`):

1. Copy everything that reads easily, skipping the hard areas:

   ```bash
   sudo ddrescue -n /dev/sdb /srv/rescue/disk.img /srv/rescue/disk.map
   ```

2. Go back over the bad areas, with direct access and three retries:

   ```bash
   sudo ddrescue -d -r3 /dev/sdb /srv/rescue/disk.img /srv/rescue/disk.map
   ```

   Each run ends `Finished`; the second reads the map first (`Initial status (read from
   mapfile)`) and goes over only what the first could not read.

3. Work on the image, never the disk: check and mount the image file as a loop device, for example
   `sudo mount -o loop,ro /srv/rescue/disk.img /mnt` for a filesystem without a partition table.

For a healthy disk, `partclone` images a filesystem by its used blocks only; see [Backups](backups.md).

## Tips

- A `PASSED` verdict says only that no attribute has crossed the maker's own threshold. A non-zero
  count of pending or reallocated sectors in `smartctl -x` is the earlier sign, and the one smartd
  logs.
- A disk behind a USB bridge may answer SMART only when told the bridge's type, for example
  `sudo smartctl -d sat -H /dev/sdb`. A bridge that passes no ATA commands answers even that with
  `Read Device Identity failed: scsi error unsupported scsi opcode`, and has no SMART to read.
- `smartd` in a virtual machine logs `In the system's table of devices NO devices found to scan`,
  then `Unable to monitor any SMART enabled devices. Exiting.`, and stops; that is expected, and
  `service status smartd` reports it as `[DOWN] smartd`.
- Test a new stick or card with `f3` before trusting data to it; see
  [Storage and removable media](storage-and-removable-media.md#checking-that-a-stick-is-genuine).

## Troubleshooting

**`service status smartd` says it is not running.** Read the boot's output and the supervisor's
log:

```bash
cat /run/kdos-init.smartd.log
cat /run/kdos-svc.smartd.log
```

`[KDOS] (smartd) Exited with code 17, a final status: not restarting` means no disk answered
SMART, which is the case in a virtual machine and for some USB bridges. Code 2 is a line in `/etc/smartd.conf` it
cannot parse; `sudo smartd -q onecheck` names it. Code 6 is a file it cannot read.

**`smartctl` reports `Unable to detect device type` or `Unknown USB bridge`.** Give the type with
`-d`, as in [Tips](#tips); `smartctl --scan` lists what it detected.

**`service status xfs_healer` says `not watching any filesystem`.** No XFS filesystem was mounted at
boot, or each was skipped. `/run/kdos-init.xfs_healer.log`, and the output of a
`sudo service restart xfs_healer`, give the reason per mount point:
`[SKIP] xfs_healer: /srv/data: no health monitor, or autofsck=none` is a kernel without the XFS
health monitor or a property set to `none`, and `[SKIP] xfs_healer: no XFS filesystem to watch`
follows when nothing was left.

**smartd logs errors on a disk that seems fine.** Run a long self-test as in
[Running a self-test](#running-a-self-test), and back up the disk first; see [Backups](backups.md).

## Known gaps

- [`kdos doctor` cannot check hardware a machine does not have](../../kdos/06-reference/known-gaps.md#kdos-doctor-cannot-check-hardware-a-machine-does-not-have):
  neither can smartd, which in a virtual machine stops at boot
- [The native applications have not been run](../../kdos/06-reference/known-gaps.md#the-native-applications-have-not-been-run),
  among them GNOME Disks, the one that draws udisks2's SMART data

## See also

- [Services that skip themselves](../../kdos/02-user-guide/administration.md#services-that-skip-themselves): final exits and the supervisor
- [Services](../../kdos/02-user-guide/administration.md#services): `52_smartd` and `53_xfs_healer` among the boot scripts
- [SMART](../../kdos/04-programs/daemons.md#smart): the question `kdos-mountd` answers for the desktop
- [Supervision and logging](../../kdos/04-programs/daemons.md#supervision-and-logging): the `/run/kdos-svc.*.log` files
- [Logs](../../kdos/02-user-guide/administration.md#logs): where the system log is kept
- [Filesystems and fstab](filesystems-and-fstab.md), [LVM and RAID](lvm-and-raid.md), [Storage and removable media](storage-and-removable-media.md), [Backups](backups.md), [Services](services.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 378.* Previous: [377. LVM and RAID](lvm-and-raid.md) · [Contents](../README.md) · Next: [379. Backups](backups.md)
