# kinstall

`kinstall` copies the running live system onto a disk and makes it bootable. It is a full-screen
wizard that works on a bare Linux console, inside a terminal emulator and over a serial line, and
it can also install unattended from an answer file.

This page is the reference for the program: its options, its answer-file keys, what each install
step does and why the design is shaped the way it is. It is for administrators scripting installs
and for contributors changing the installer. If you only want to install KDOS, read
[Installation](../02-user-guide/installation.md) instead; it walks through the wizard and repeats
none of this.

After reading it you will be able to:

- run an install unattended from an answer file, and check its plan before anything is written;
- read what the installer sees on a machine with `--dump probe`;
- know which files the installed system gets and which accounts and groups it ends up with.

## Synopsis

```
kinstall [options]
```

| Option | Does |
|---|---|
| `--config FILE` | Read answers from `FILE`. Later options on the command line still apply |
| `--save FILE` | Write the current answers (defaults plus any `--config`) to `FILE` and exit |
| `--unattended` | Skip the wizard and install from the answers |
| `--dry-run` | Log every command and execute none. The header shows **DRY RUN** |
| `--dump probe` | Print what the installer sees on this machine, and exit |
| `--dump plan` | Print the steps these answers would run, and exit |
| `--json` | Print `--dump` as JSON instead of text |
| `--theme NAME` | The accent: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect` or `paper`. The default is `bone` |
| `--no-mouse` | Keyboard only |
| `--ascii` | Draw boxes with `-`, `\|` and `+`, for terminals without line-drawing characters |
| `--version` | Print `kinstall 4.0` and exit |
| `--help`, `-h` | Print the usage and exit |

The log of a run is `/var/log/kinstall.log`. On the install page, `L` shows it in full.

Only `--dump`, `--save`, `--help` and `--version` work without root. The wizard, `--dry-run`
included, stops on its first page with "kinstall must run as root — try: sudo kinstall" until it
runs as root. An `--unattended` run does not pass through that page, so nothing stops it early:
run it as root, or its steps fail.

The Welcome page shows a preflight list, and marks each item that fails:

- running as root;
- at least one writable disk;
- `rsync` present;
- `mkfs.ext4` and `mkfs.vfat` present;
- the Limine payload, `/usr/share/limine`, present.

The page refuses to continue without root or without a disk. On a machine that booted in legacy
BIOS mode it also says that the installed system will start, but that the firmware's own boot
entry cannot be created.

| Exit status | Means |
|---|---|
| 0 | Finished: help, version, a dump, a saved answer file, a completed install, or the wizard quit |
| 1 | An unknown option, an unreadable `--config` file, a `--save` that could not be written, or an `--unattended` install that failed |
| 2 | `--dump` given something other than `probe` or `plan` |

## Keys

| Key | Does |
|---|---|
| Tab, Shift-Tab | Move between controls |
| Arrows | Move inside a list or a field |
| Enter, Space | Activate, toggle, choose |
| Alt+Left, Alt+Right | Previous or next page |
| Esc | Back a page, or close the help |
| PgUp, PgDn | Scroll a page taller than the screen |
| F1 | Help |
| L | The full log, on the install page |
| Ctrl+U | Clear a text field |
| Ctrl+Q, Ctrl+C | Quit the installer. It asks first, and while an install is running it warns that stopping leaves the target half written |

The installer needs a terminal of at least 62 columns by 18 rows; below that it shows a
screen-too-small notice instead of the wizard. On the 25-row Linux console some pages are taller
than the screen, and PgUp, PgDn and the mouse wheel scroll them.

The mouse works everywhere, including on a bare console (see [The mouse works on a bare
console](#the-mouse-works-on-a-bare-console)). Clicking a finished page in the sidebar walks back to
it; clicking a page ahead does nothing, because pages ahead have not been checked yet.

## The pages

The wizard is eleven pages. Each has a fixed identifier, and the program always finds a page by
that identifier, never by its position.

| # | Identifier | Title | Asks for |
|---|---|---|---|
| 1 | `welcome` | Welcome | Nothing; shows the processor, memory, storage, firmware and how much space the install needs |
| 2 | `keyboard` | Keyboard | The console keymap |
| 3 | `time` | Time | The zone and clock |
| 4 | `disk` | Disk | Which disk |
| 5 | `layout` | Layout | How to use it — erase it, reuse existing partitions, or partition it yourself in `cfdisk` first — then the partitions, filesystem, swap, LVM and LUKS2 encryption |
| 6 | `accounts` | Accounts | Hostname, user, passwords, whether the user is an administrator, whether root is locked |
| 7 | `system` | System | Accent, the boxed-application container library, services |
| 8 | `apps` | Applications | Which application groups this machine builds |
| 9 | `summary` | Summary | Nothing; the point of no return |
| 10 | `install` | Install | Nothing; shows progress |
| 11 | `done` | Done | Reboot |

The button before the Summary reads **Review** rather than **Next**. On the Summary, **Next** is
refused ("press BEGIN INSTALL to start — Next does not"): the install starts from the **BEGIN
INSTALL** button (**REHEARSE INSTALL** under `--dry-run`) and only from it.

Choosing "Partition it myself first (cfdisk)" shows an **Open cfdisk** button, which runs `cfdisk`
on the chosen disk. When `cfdisk` exits, the installer re-reads the disks and switches to reusing
the partitions you made.

## The install steps

| # | Step | Does | Skipped when |
|---|---|---|---|
| 0 | Prepare | Checks the target, stops swap, takes down anything holding the disk | — |
| 1 | Partition | Writes the GPT layout: a 512 MiB EFI system partition, an optional swap partition, then the root | The plan is not "erase" |
| 2 | Format | Encrypts and creates the volume group if asked, then makes the filesystems | — |
| 3 | Mount | Mounts the target at `/mnt` | — |
| 4 | Copy system | Copies the live tree with `rsync` | — |
| 5 | Packs | Imports, builds or records the chosen applications | No application catalogue, or nothing chosen |
| 6 | Configure | `fstab`, hostname, keymap, time zone, autologin, services, swap file | — |
| 7 | Accounts | Users, passwords, groups, sudo | — |
| 8 | Theme | Runs `kdos theme <accent>` for the new home | The accent is the default, `bone` |
| 9 | Bootloader | Limine on the ESP for UEFI and BIOS, kernel and initramfs, the NVRAM entry | — |
| 10 | Finish | Flushes and unmounts | — |

The copy leaves out what must be unique to each machine — `/var/lib/dbus/machine-id` and
`/etc/ssh/ssh_host_*` are generated on the installed system's first boot — as well as the log of the
run itself. Without the boxed-application library (`alien_apps = 0`), the live user's container
storage under `/home/kdos/.local/share/containers` is left out as well. `rsync` exit status 24
("some files vanished") is accepted, since files come and go on a running live system.

## Answer files

An answer file is plain `key = value` lines; `#` starts a comment. `--save` writes one with every
key except the three secrets, and `--config` reads one. A key the installer does not recognise is
ignored.

```sh
kinstall --save answers.conf                      # the defaults, to edit
kinstall --config answers.conf --dump plan        # what they would do
kinstall --config answers.conf --unattended       # do it
```

| Key | Values | Default | Notes |
|---|---|---|---|
| `keymap` | A console keymap name | `us` | Written to `/etc/keymap` |
| `timezone` | A `TZ` value | `:/etc/localtime` | Written to `/etc/profile.d/20-timezone.sh`. Keep the default: the colon form makes musl read the same file `/etc/localtime` points at |
| `timezone_label` | A zone name, `Area/City` | `UTC` | `/etc/localtime` is linked to `/usr/share/zoneinfo/<zone>`, and the Wi-Fi country is taken from it |
| `disk` | A whole-disk device, such as `/dev/nvme0n1` | none | Required |
| `plan` | `wipe`, `reuse` or `manual` | `wipe` | **Any other value means `wipe`, which erases the disk.** `manual` behaves as `reuse` in an unattended run |
| `esp` | A partition device | none | The EFI system partition, for `reuse` |
| `root` | A partition or logical-volume device | none | The root, for `reuse` |
| `format_esp` | `1` or `0` | `1` | Reformat the ESP as FAT32 |
| `fstype` | `ext4`, `btrfs`, `xfs` or `f2fs` | `ext4` | An unknown name falls back to `ext4` |
| `swap` | `file`, `partition` or `none` | `file` | **Any other value means `none`** |
| `swap_mb` | Megabytes | `4096` | |
| `luks` | `1` or `0` | `0` | Encrypt the root with LUKS |
| `luks_passphrase` | Text | none | Never written by `--save` |
| `lvm` | `1` or `0` | `0` | Put the root on LVM, with the `wipe` plan |
| `hostname` | Text | `kdos` | |
| `username` | Text | `kdos` | The live account is renamed to this |
| `fullname` | Text | `KDOS User` | |
| `password` | Text | the live password, `kdos` | The user's password. Never written by `--save`. **Left out, the installed account keeps the live image's well-known password** |
| `root_password` | Text | none | Used only when `root_locked = 0`. Never written by `--save` |
| `root_locked` | `1` or `0` | `1` | A locked root has no password and cannot log in |
| `theme` | An accent name | `bone` | |
| `alien_apps` | `1` or `0` | `1` | Copy the boxed-application container library |
| `apps` | Space-separated group ids | `essential` | See [The applications step](#the-applications-step) |
| `autologin` | `yes` or `1` for on; anything else is off | `no` | See below |
| `reboot` | `1` or `0` | `1` | Reboot when an unattended install ends |
| `services` | Space-separated service names | `networkmanager bluetooth alsa` | Every listed service not named here is disabled |

The services an answer file can turn on or off, and whether each is on by default:

| Name | Service | Default |
|---|---|---|
| `networkmanager` | NetworkManager — wired, Wi-Fi and VPN | on |
| `bluetooth` | The bluez daemon | on |
| `alsa` | Restore mixer levels at boot | on |
| `cups` | The printing daemon | off |
| `sshd` | The OpenSSH server | off |

A disabled service gets an empty file `/etc/service.disabled/<name>` on the installed system. Other
services (udev, D-Bus, seatd and the rest) are not choices and cannot be turned off here.

**Whether the user is an administrator has no key.** An answer-file install always keeps the user
in `wheel`; only the Accounts page can make a non-administrator. That page refuses a
non-administrator while root is locked ("root is locked and <user> is not an administrator — nobody
could ever gain privileges"): a non-administrator needs an unlocked root with a password.

**Passwords in an answer file are in plain text.** `--save` never writes `password`,
`root_password` or `luks_passphrase`; add them yourself only if you accept that credential sitting
on the medium the file lives on.

**An answer file without `password` installs a machine whose account password is `kdos`.** The
installer changes the password only when it is given one, so the renamed account keeps the live
image's password hash. The wizard's Accounts page refuses an empty password; an unattended install
does not ask. Always set `password` in an answer file for a machine anybody else can reach.

### autologin

`autologin` is the one key whose default differs from the live medium. The live medium ships
`autologin = kdos` — a machine with one account and no password has nothing to ask — while an
answer file that leaves the key out installs a machine that asks for a password, because a system
somebody installed has a real account with a real password.

The installer edits the key's line in the target's `/etc/kdos/login.conf` rather than replacing the
file, because the file is mostly an explanation of what the key does:

- `yes` writes `autologin = <username>`;
- `no` writes `#autologin = <username>`.

Off is a commented line, not an empty value. The login program only hands `agetty --autologin` a
non-empty name, so an empty value would read as a setting and behave as none. The key is also the
only place the desktop account is named for tty1 (`/etc/inittab` names no account), so this is what
carries a renamed user to the login.

### Fallbacks, and how an unattended run ends

Three kinds of mistake fall back rather than fail, because each is read before the point of no
return, and refusing there would leave nothing installed over a spelling mistake:

- an unknown filesystem becomes `ext4`;
- an `apps` line that matches no group becomes `essential`;
- an unknown `plan` becomes `wipe`, and an unknown `swap` becomes `none`. Check these two with
  `--dump plan` before running unattended.

A missing `password` is not a mistake the installer can see, and it does not fall back to anything
safer: the account keeps the live password, `kdos` (see [Answer files](#answer-files)).

An unattended run ends by itself whichever way it went. When the install finishes **or fails**, the
last screen stays up for five seconds, then:

- with `reboot = 1` (the default) the machine reboots — after a failure as well as after success;
- with `reboot = 0` the installer exits, with status 1 if the install failed and 0 if it succeeded.

Set `reboot = 0` when a script needs the exit status. The failed case ends the run too because a run
that waited for success alone would sit on its error screen forever, and that is the run somebody
most needs an answer from.

## Checking before installing: the dumps

```sh
kinstall --dump probe [--json]
kinstall --dump plan  [--json]
```

Neither needs a terminal, neither writes to a disk, and both print to standard output only.

- **`probe`** is the machine as the installer sees it: firmware, processor, memory, the kind of
  session it is running in, the size of the system it would copy (and of the container library
  within it), the disks and their partitions, and the logical volumes. It is what to paste into a bug report. Run as root it first activates every LVM volume
  group, exactly as the wizard does, so the logical volumes it lists are the ones the reuse plan
  would offer; run as an ordinary user it lists only those already active. It measures the size of
  the live system by walking the root filesystem, which takes seconds on a live image and much
  longer on a development machine, so the test suite does not run it.
- **`plan`** runs the same planner the wizard does, so the steps and their skips are the real ones,
  including how the applications would arrive.

The JSON form is a rendering of the same data, not a second walk. **No password appears in either
form** — the dump shows whether an account has a password, never the password — and the test suite
checks this with a sentinel value.

## Trying it without installing

```sh
kinstall --dry-run                      # every command logged, none executed
kinstall --save answers.conf
kinstall --config answers.conf
kinstall --unattended --config answers.conf
kinstall --ascii                        # the lowest glyph tier
kinstall --no-mouse
```

To keep the installer away from anything else on the console, give it a terminal of its own:

```sh
kinstall < /dev/tty3 > /dev/tty3
```

A full-screen program redraws constantly, so pointed at a serial console it is the only thing on
that line for the whole install. `testing/install-to-disk.sh`, the disk-install harness, runs an
unattended install on `/dev/tty3` this way and prints its own heartbeat on the serial line, because
a run that says nothing until it finishes cannot be told from one that never will.

## The filesystem table

Every choice about the root filesystem comes from one table row — the menu, the `mkfs` command,
the `fstab` line and the swap-file step all read the same row.

| Filesystem | `mkfs` | Mount options | `fstab` pass | Swap file made with | Offered for |
|---|---|---|---|---|---|
| `ext4` | `mkfs.ext4 -F` | `defaults,noatime` | 1 | `fallocate` | The default: journalled, well understood, built into the kernel |
| `btrfs` | `mkfs.btrfs -f` | `defaults,noatime,compress=zstd:3` | 0 | btrfs's own swap-file method | Snapshots and transparent zstd compression |
| `xfs` | `mkfs.xfs -f` | `defaults,noatime` | 0 | `dd` | Large files and parallel I/O; it cannot be shrunk |
| `f2fs` | `mkfs.f2fs -f` | `defaults,noatime` | 0 | `dd` | Log-structured, for flash: a USB stick, an SD card or cheap eMMC |

Three columns each prevent a failure that would only show up later:

- **Only ext4 gets a non-zero `fstab` pass.** A non-zero pass asks for a filesystem check at boot,
  and none of the others has a checker worth running then (`f2fs.fsck` exists, but it is a repair
  tool).
- **The swap file is made differently per filesystem.** `fallocate` leaves unwritten extents, which
  xfs and f2fs refuse to swap on. Using it there would install cleanly and then boot with no swap
  and nothing saying why.
- **xfs and f2fs are kernel modules**, so both are in the initramfs module list in
  `script/06_packaging/01_initramfs.sh`; ext4 and btrfs are built in. A root on a filesystem the
  initramfs cannot load installs perfectly and never boots again.

A filesystem whose `mkfs` is missing from the image is still listed, marked as unavailable, and is
refused before anything is written. A control that silently snaps back is harder to understand than
one that says why.

## A root on LVM

The erase plan can put the root on LVM, and the reuse plan can put it on a logical volume that
already exists. Either way `root=` and the `fstab` line name the filesystem's UUID, exactly as for
a partition, and the initramfs activates every volume group before looking for it — see
[Activating volume groups](../03-architecture/boot-and-init.md#activating-volume-groups).

**The erase plan's layout is fixed.**

- The ESP and any swap partition stay plain partitions.
- The root partition is typed Linux LVM and becomes the only physical volume of a volume group
  named `kdos`.
- The root is the logical volume `root_a`.
- With encryption on, LUKS is opened on the partition and the physical volume is inside it, so one
  passphrase opens the whole group.
- The commands are `lvm pvcreate`, `lvm vgcreate` and `lvm lvcreate`, not the short names, which an
  image need not carry.

**The volumes are named for the A/B root slots.** `root_a` is slot A's root, and slot B's goes
beside it as `root_b`, so adding a second slot needs no repartitioning. `root_a` takes half the
group when half still holds the system, the swap file (when swap is a file) and 256 MiB to spare,
and the whole group otherwise. The swap file counts because it is written onto the same root after
the copy. `ki_lvm_half()` makes that decision, and the Layout page, the Summary, `--dump plan` and
the Format step all call it, so the size shown is the size created. The other half is left
unallocated: the installer makes no `root_b`, because an empty volume is not a slot anything can
boot or update.

**A name clash is refused before the point of no return.** If a volume group called `kdos` has any
physical volume on another disk, `vgcreate` would fail after this disk had been erased. The check
asks `lvm pvs` for every physical volume and its group, so it catches a group with no volumes, one
whose volumes did not activate, and one spanning the target disk and another. A group wholly on the
target disk is no clash, because the erase removes it.

**Whatever held the disk is taken down first.** The installer activates every volume group when it
probes, so a disk holding an earlier install reaches the Prepare step with its volumes live — and
the kernel will not re-read a partition table under a partition something holds. On the erase plan,
Prepare walks the `holders` of the disk and each partition in `/sys`, top first: a logical volume's
whole group is deactivated, a LUKS container is closed, any other device-mapper device is removed,
and anything mounted is unmounted by device number. The Format step does the same again, so a
retried Format meets nothing a failed one left open. It then wipes the old physical-volume label
from the new partition, because the erase writes the same layout at the same offsets and `pvcreate`
would find the previous install's label there.

**The reuse plan lists volumes from `/sys`, not from `lvs`.** A logical volume is a device-mapper
device whose uuid is `LVM-` followed by exactly two 32-character uuids; anything with a suffix is an
internal layer (a thin pool's `-tpool`, for example) and is skipped. The device-mapper name
`vg-lv`, with each `-` inside a name doubled, gives the two names, and a volume whose name carries
one of LVM's reserved sub-volume suffixes (`_tdata`, `_tmeta`, `_cdata`, `_cmeta`, `_corig`,
`_cpool`, `_rimage_`, …) is skipped as well. Thin and cached volumes are ordinary volumes here. The
list covers every volume on the machine, not only those on the target disk: the root need not share
a disk with the ESP. The volume-group activation that makes them visible runs once per process and
only as root, so `--dump probe` run as a user does not fail on it.

A physical volume, and any partition or volume that something holds, is refused as the root. `mkfs`
would fail on a held device after the point of no return, and on a physical volume it did not fail
on it would destroy a volume group.

## The applications step

Applications are not on the installation medium. Each one is built by the container engine on the
installed machine from the shipped catalogue. The Applications page therefore chooses what to
build, not what to copy.

The installer reads the catalogue from the first of these that exists:

1. the file `$KDOS_CATALOGUE` names, when it is set (the test suite points it at a fixture);
2. `/usr/share/kdos/appstore/catalogue`, the live system's own copy;
3. `/mnt/iso/appstore/catalogue`, on the boot medium.

**It lists groups, not applications.** Seven named bundles is something to read during an install;
180 rows is not. The catalogue defines these groups:

| Group | Description |
|---|---|
| `essential` | What a new machine starts with — ticked by default |
| `office` | Documents, spreadsheets and reading |
| `creative` | Images, audio and video |
| `dev` | Programming and electronics |
| `science` | Computation, modelling and data |
| `make` | Three dimensions: model it, slice it, cut it |
| `games` | Games and emulators |

Each row shows the number of applications and a size estimate. The estimate counts each member once
and no runtime at all, since shared runtime layers are stored once on disk. A group whose members
are all missing from this catalogue is not offered: a tick that installs nothing is worse than a
row that is not there.

The base system and the runtimes are not choices, and the page says so in one line rather than
offering them as rows. Each application pulls in the runtime it needs, so leaving a runtime out
could only produce applications that cannot start.

In an answer file, `apps` names group ids. Names that are not group ids are ignored, and if none
match, the selection is `essential`. With no `apps` key the selection is `essential`. The page's
selection logic runs before planning on every path that plans without walking the wizard
(`--dump plan` and `--unattended`), because the selection is what the plan is about.

The catalogue is read by compiling `kdos-appbox`'s `catalogue.c` into the installer rather than by
running `kdos-appbox`. That file uses only `libkbase`, so it costs no extra library, and a live
installer cannot assume anything is on the target's `$PATH`.

### How the applications arrive

The installer picks one route, and the Applications page shows which before anything is written,
so nobody discovers at first boot that nothing was installed.

| Route | Chosen when | What happens |
|---|---|---|
| **import** | An exported set (a `.ktar` file) is on a mounted device | `kdos-appbox import <file>`. `kdos-packd` verifies each pack where it mounts it. Offline — the only route on a machine with no network. It imports the archive's whole selection (its own `SELECTION` file), whichever groups were ticked |
| **network** | There is a default route | `kdos-appbox install <group>…` builds the ticked groups during the install. This can take a long time |
| **pending** | Neither | The group ids are written to `/var/lib/kdos/apps-pending` on the installed system; the first session offers them, and `kdos app install --pending` builds them |

**The import and network routes run on the live system, not inside the target.** The Packs step
comes after the copy and runs `kdos-appbox` as the live system's own program, with no change of
root: it asks the live `kdos-packd` on `/run/kdos-packd.sock`, and what it imports or builds goes
into the live system's store, not onto the installed disk. The installed system receives the empty
store directories (below) and, when the step falls back, the pending list. Until this is changed,
the **pending** route is the one that reliably carries a selection to the installed machine.

The archive is searched for once, at probe time: the first `*.ktar` directly inside any directory
under `/mnt`, `/media` or `/run/media` wins, and only that one is offered.

The network test reads `/proc/net/route` for a default gateway. It does not ping anything: opening
a connection to somebody else's host just to decide what to draw would be an installer reaching the
network to ask whether it can reach the network.

An import or a build that fails falls back to **pending** rather than failing the install; the
screen and `/var/log/kinstall.log` do not say that it did (see [The install runs in a child
process](#the-install-runs-in-a-child-process)). By then
the system is installed and bootable, and abandoning it over an archive that would not unpack would
leave a machine with no operating system.

When the archive is under `/mnt`, the installer bind-mounts `/mnt` itself to `/run/kdos-medium`
before mounting the target there, and reads the archive as `/run/kdos-medium/<path below /mnt>`.
The target mounts at `/mnt`, and a stick mounted at `/mnt/<something>` would be hidden underneath
it, so the original path would lead into the empty filesystem just created. It is a bind of the
directory, not a second mount of the device, because the device was mounted by something else and
its path is the only handle the installer has on it. If the bind fails, the log says so and the
archive is dropped. An archive under `/media` or `/run/media` needs no bind, since mounting `/mnt`
does not cover those.

Whichever route runs, the store's directories are created on the target —
`/var/lib/kdos/packs`, `/var/lib/kdos/packs/staging` (mode 01777) and `/var/lib/kdos/packs/mnt`.
`kdos-packd` sets the staging mode when it starts, but a first boot that found it `0755` would
refuse an import until the daemon had run once, which would look like the feature not working.

## What the rest of the tree provides

The installed system depends on these, and each exists because of the installer:

- **`kdos-getty` loads the keymap** the installer writes to `/etc/keymap`.
- **`rcS` turns swap on after mounting** (`swapon -a`), because `mount -a` ignores swap lines.
  Without it the swap choice would do nothing.
- **`fstab` is appended to, never replaced.** The shipped file carries the `tmpfs` line for `/tmp`
  with `mode=1777`, which every graphical application depends on.
- **Renaming the user** rewrites `/etc/passwd`, `/etc/shadow` and `/etc/group` (the membership
  lists and the primary group's own name), the home directory, the owner of the `/etc/subuid` and
  `/etc/subgid` ranges, and `login.conf`'s `autologin`. The subordinate ranges are looked up by
  name: a range left on `kdos` would give the renamed account no mapping, and no rootless box would
  start.
- **Administrator means membership of `wheel` and nothing else.** The live image ships the account
  in `wheel`. Everything that grants administrator rights keys on that membership: the sudo rule
  `%wheel ALL=(ALL) ALL`, the polkit admin rules, `kdos-resctl`, `kdos-packd`, `kdos-energyd`, and
  `kdos-powerd`'s configuration verbs (firewall, autologin, accent, timezone). So a
  non-administrator is simply taken out of `wheel`, and no separate sudoers file is written. Every
  other group is kept, `seat` among them. `seat` is what a non-administrator's desktop runs on:
  seatd hands it the display, and `kdos-powerd`'s suspend, power-off and reboot, `kdos-mountd` and
  `kdos-oomd` all admit it. A non-administrator therefore keeps the lid, the power keys, the panel's
  power items and USB-stick mounting, and loses sudo, the polkit admin actions and everything
  listed above — see [The daemons](daemons.md#at-a-glance).
- **The hostname** replaces both `/etc/hostname` and the `127.0.1.1` line of `/etc/hosts`. musl
  resolves the machine's own name from that file before asking DNS, and the shipped line names
  `kdos`.
- **The time zone** writes `/etc/profile.d/20-timezone.sh`, links `/etc/localtime`, and writes the
  Wi-Fi country to `/etc/modprobe.d/kdos-regdom.conf`, exactly as `kdos-powerd`'s `timezone` verb
  does later. A zone the target does not carry is logged and leaves the machine on UTC.
- **The kernel and initramfs go into slot A's directory on the ESP,** `EFI/kdos/a/`, and the boot
  menu entries point there and carry `kdos_slot=a`. `kdos-bootctl` regenerates those entries later
  from the command line written here, and later kernels reach the ESP through
  `kdos-bootctl deploy` — see [A new kernel](../03-architecture/boot-and-init.md#a-new-kernel). The
  installer checks the ESP has room for a second slot's kernel and initramfs beside the first
  (every A/B update needs it; the 512 MiB ESP the erase plan makes has plenty, a reused 100 MiB one
  may not). When it does not, the installer's message about it is discarded, so neither the
screen nor the log says so; check the ESP's free space yourself on a reused ESP.
- **Both UEFI loaders go onto the ESP,** `EFI/BOOT/BOOTX64.EFI` and `EFI/BOOT/BOOTIA32.EFI`, so a
  disk written on one machine starts on another. A 64-bit processor does not imply 64-bit firmware,
  and firmware only reads the `BOOT<arch>.EFI` it can run. `BOOTIA32.EFI` is copied where Limine
  provides it and skipped where it does not (a Limine built without `--enable-uefi-ia32` has
  none), with nothing on the screen or in the log to say so; failing the install over a fallback for firmware this machine does
  not have would throw away a working system.
- **The BIOS boot code is written too,** whatever firmware the install ran on: `limine-bios.sys`
  goes to the root of the ESP and `limine bios-install <disk>` writes the boot sector. A disk
  installed on a UEFI machine and moved to a legacy BIOS one still starts. If it fails, the install
  carries on, because the UEFI path is already complete, and nothing on the screen or in the log
  says it failed.
- **The NVRAM boot entry names the loader this firmware can run,** chosen from
  `/sys/firmware/efi/fw_platform_size` (a machine that does not report it is treated as 64-bit),
  with the label `KDOS`. It is created only when the install ran under UEFI and `efibootmgr` is
  present. The removable-media fallback path picks between the two loaders by itself;
  `efibootmgr --create` cannot, and an entry pointing at the wrong one is a boot option that fails.
- **The entry names the partition the ESP is actually on,** read from
  `/sys/class/block/<node>/partition` rather than guessed from the device name: only the erase plan
  puts the ESP at index 1, and a reuse install takes whichever partition was picked. Where the index
  cannot be read no entry is created, and nothing says so; the removable-media fallback still starts the
  disk, whereas a guessed index would be a boot option the firmware cannot load.

Nothing is copied onto the ESP with an ownership-preserving copy. The ESP is FAT, which has no
ownership: such a copy tries to change the owner of every file, the kernel refuses each one, and the
copy fails with a page of errors naming the *source* paths — which looks like a problem with the
boot loader rather than a filesystem that cannot store what was asked of it.

## How it is built

### Dependencies

`kinstall` links `libkbase`, `libktui` and `libkcolor` — compiled from source into the binary — and
no other library, not even a terminal library. That is what lets it be cross-compiled in phase 1 of
the build (`script/01_phase1/13_kinstall.sh`) and exist on every tree from the first bootable image
onward. `libkcolor` is on the list because `libktui`'s theme code includes its palette header; the
colour values live there and nowhere else.

Giving `kinstall`, or any of those three libraries, a new dependency means moving the installer's
build to a later phase.

A recipe (`src/packages/kdos-installer/kpkgbuild`, built by `build.sh`) sits beside the sources so
a running KDOS can rebuild the installer natively. Both builds compile every `.c` file in the
directory by glob, plus `kdos-appbox`'s `catalogue.c`, so the phase-1 installer and the packaged
one are always the same program.

### The file split

| File | Owns |
|---|---|
| `probe.c` | The `/sys` and superblock reader, the partition-table reader, volume-group activation and the logical-volume list, the catalogue and group reader, and the search for an exported application set |
| `pages.c` | The eleven wizard pages |
| `install.c` | The install steps, run in a child process, and the line protocol it reports on |
| `conf.c` | The defaults, the answer file, the service list and the filesystem table |
| `dump.c` | `--dump probe` and `--dump plan` |
| `main.c` | The header, sidebar and navigation, the event loop, the command line |

The terminal handling, cell buffer, input layer, widgets, dialogs and palette belong to
[libktui](../05-developer/c-libraries.md).

## Design decisions

### Nothing is written before the summary

Every page fills in the configuration and does nothing else. The install is the single point of no
return, which is what makes **Back** safe to press on any page before it.

### The whole interface is eight colours

A 512-glyph console font makes the Linux console use the foreground intensity bit as a ninth glyph
bit. The bright colours are then unreachable as foregrounds, and the bold attribute switches the
*font page* instead of the weight — so the installer never emits bold on a console.

Using eight colour slots means a console and a modern terminal emulator show the same picture. On a
console the installer saves the palette, installs its own, and restores exactly what was there when
it exits; elsewhere the same slots are sent as true-colour, 256-colour or basic colour escapes. The
sidebar's footer shows which (`24-bit`, `256`, `vt palette` or `ansi`).

### The mouse works on a bare console

The Linux console has no mouse reporting at all, so on a console the input layer reads the input
devices under `/dev/input` directly, with no gpm, and keeps its own pointer: relative devices are
scaled by the real cell size worked out from the framebuffer's dimensions, absolute devices (a
tablet, a virtual machine's pointer) are mapped straight through, and the pointer is drawn as an
inverted cell. Under a terminal emulator it uses the terminal's ordinary mouse reporting instead
and never touches the input devices. The sidebar footer shows `evdev`, `sgr` or `off`.

### The install runs in a child process

The install is ordinary top-to-bottom code in a forked child, and the parent stays a
single-threaded loop that keeps drawing and never blocks on a multi-gigabyte copy.

The child points its own standard streams at `/dev/null` and reports on a pipe of its own, so
nothing but its reporting function can write to that pipe. Each line starts with one letter:

| Letter | Means |
|---|---|
| `S` | Step *n* has started |
| `K` | Step *n* is skipped |
| `P` | Progress within the current step, as a fraction |
| `N` | A note on the current step |
| `L` | A log line |
| `W` | A warning |
| `F` | Failed, with a message |
| `D` | Done |

The parent acts on every letter except `W`: it has no handler for warnings and discards them, so
they reach neither the screen nor `/var/log/kinstall.log`. The child sends a `W` for a missing
`BOOTIA32.EFI`, a failed `limine bios-install`, a missing ESP partition index, an ESP with no room
for a second slot, and an application import or build that fell back to pending — which is why the
sections above say that nothing reports those.

A step starting closes every earlier step still marked running, not only the one before it. A
skipped step often sits between two real ones (no repartition, no theme), and closing only the
predecessor would leave the real one spinning for the rest of the run.

There is no shell and no command string anywhere. Device paths and user names come from menus or
the answer file, and every command runs from an argument list.

### The sidebar does not take focus

The sidebar is drawn before the page. If its rows took ordinary focus positions, every control on
every page would move down the Tab order and the cursor would start on a decoration, where typing
does nothing. The sidebar's rows register as *chrome*, in a separate range that never joins the
Tab order.

## See also

- [Installation](../02-user-guide/installation.md) — using the installer
- [Boot and init](../03-architecture/boot-and-init.md) — what it writes, and what boots it
- [The daemons](daemons.md) — what `wheel` and `seat` membership grant on the installed system
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the catalogue the applications page reads
- [The C libraries](../05-developer/c-libraries.md) — the three it links
- [Testing](../05-developer/testing.md) — the dumps and the disk-install harness
