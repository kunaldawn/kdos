# A/B slots and rollback

An installed KDOS machine can keep two root filesystems, slot A and slot B, and boot an update in
the slot it is not running, falling back to the other when the update does not finish booting.
This page covers reading the boot state, the slots the installer makes, putting an update on
trial, rolling one back, and what to check when a slot will not boot or will not confirm. How
the initramfs counts attempts is explained in
[A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection), why each slot
has its own kernel in
[One kernel per slot](../../kdos/03-architecture/boot-and-init.md#one-kernel-per-slot), and how a
UEFI trial uses `BootNext` in
[One boot through BootNext](../../kdos/03-architecture/boot-and-init.md#one-boot-through-bootnext);
this page gives the procedures.

## Installation

Everything here belongs to [`kdos-bootctl`]{.idx}, one of the names of the `ksvc` binary in
[kdos-tools](#port-kdos-tools) (`/usr/bin/kdos-bootctl` is a link to `/usr/sbin/ksvc`). The image
build copies it into the initramfs, where `kdos-bootctl select` decides which root to boot.
`kdos update apply`, from the same port, is what writes the inactive slot.
[kdos-sysupdated](#port-kdos-sysupdated) is the root daemon that confirms a boot and serves the
slots to the desktop: Discover installs an update through it, and System Settings' **System
Versions** page, from [kcm-kdos-system](#port-kcm-kdos-system), shows both slots and starts the
other one next time.

The installer, [kdos-installer](#port-kdos-installer), sets a machine up for slots whenever the
install has an [ESP]{.idx term="EFI system partition"}. It writes slot A's kernel and initramfs into
`EFI/kdos/a/`, the first boot state, and `kdos_slot=a` and `bootstate=UUID=<esp>` on each `/KDOS`
menu entry. On the **A/B layout** it makes the second slot as well. That layout is what the erase
plan under LVM gives a disk with room for it: one volume group, `kdos`, holding `root_a` and
`root_b`, a `swap` volume when swap was asked for, and `home`, the rest, which both slots mount at
`/home`. Each slot is at least 8 GiB, the size of the install and half as much again; the home is
at least 4 GiB. The installer fills slot B with a copy of slot A as it stands at the end of the
install, accounts and accent included, gives slot B its own kernel pair under `EFI/kdos/b/`, and
records both slots in the boot state. A disk too small for two slots gets `root_a` alone, holding
the home as well, and no second slot.

The two halves of a boot's decision run at set points of every boot: `select` in the initramfs,
and `mark-good` once the boot has been confirmed. `/etc/init.d/46_sysupdated.sh` supervises
kdos-sysupdated, which runs `mark-good`.

The live medium has no slots and no boot state. Everything on this page applies to an installed
machine only.

## Configuration

### The boot state

The state is one file on the ESP, `EFI/kdos/bootstate`, which is
`/boot/efi/EFI/kdos/bootstate` on the running system. No file in the tree ships it; the installer
writes the first version:

<!-- kfs:include src/system/kinstall-core/install.c lines=2813-2825 pin=d5c38affc180 -->

`slot_b` is empty unless the installer made the A/B layout. `kdos-bootctl` writes every later
version, under a header of its own
(`Written by kdos-bootctl; the initramfs reads it before it mounts a root filesystem.`), and adds
three keys: `bootnext`, which is empty except during a UEFI trial, when it reads `yes`; `armed`, the
kernel's boot id of the boot that armed the trial; and `failed`, the slot whose last trial ended
without being confirmed, until the next `try` or confirmation clears it:

```text
slot_a   = c55c90d8-b72a-4e05-b729-bbd60388a06a
slot_b   = a5549778-64ed-4a32-88a1-bde080ddba46
crypt_a  =
crypt_b  =
active   = a
try      = b
attempts = 1
bootnext = yes
armed    = 1b0e4c52-6c8f-4b8e-9a51-2f1d7b0c9e3a
failed   =
```

The keys are described in
[A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection).
Edit the file only through `kdos-bootctl`: it writes through a temporary file and two `fsync`s
because the ESP is FAT and has no journal, and a file it cannot parse is treated as absent, which
boots the `root=` of the menu entry.

### The ESP layout

| Path on the ESP | Holds | Written by |
|---|---|---|
| `EFI/kdos/a/vmlinuz`, `EFI/kdos/a/initramfs.cpio.gz` | Slot A's kernel and initramfs | [`kinstall`]{.idx}, then `kdos-bootctl deploy` |
| `EFI/kdos/b/…` | Slot B's | `kinstall` on the A/B layout, then `kdos-bootctl deploy` |
| `EFI/kdos/<slot>/release` | The identity of the root that slot holds: its `/etc/kdos-release`, else its `os-release`. The other slot is not mounted when a session asks what it holds, so System Versions and the rollback notice read this file | `kdos-bootctl deploy` |
| `EFI/kdos/bootstate` | The boot state | `kinstall`, then `kdos-bootctl` |
| `limine.conf` | The menu; its `/KDOS` entries follow the state | `kinstall`, then `kdos-bootctl` |
| `EFI/kdos/trial/` | A copy of the ESP's `BOOTX64.EFI` (`BOOTIA32.EFI` on 32-bit firmware) and a trial `limine.conf`, during a UEFI trial only; removed when the trial ends | `kdos-bootctl try` |
| `EFI/kdos/memtest.efi`, `EFI/kdos/wallpaper.png`, `EFI/kdos/font.bin` | The memory test, the menu's backdrop and its typeface, shared by both slots; each only when the installer finds its source | `kinstall` |

Which lines of the menu `kdos-bootctl` rewrites is covered on
[Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md#what-kdos-bootctl-rewrites).

### Where each half runs

In the initramfs, the `BOOT SLOT` stage mounts the ESP, asks `select` for the root and `crypt` for
that root's container, and unmounts it again. The `\$` in this excerpt is the generator's escaping:
the init is written by a heredoc, and the generated script has a plain `$`.

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=789-844 pin=75069784095f -->

A boot is confirmed by two marks, both root's files under `/run/kdos/bless/`. The end of `rcS`,
after every service script has run, writes the first:

<!-- kfs:include fs/etc/init.d/rcS lines=190-214 pin=de5053d13197 -->

The second, `graphical`, is written by kdos-sysupdated when a Plasma session has answered for a
minute: `/etc/xdg/autostart/kdos-session-good.desktop` runs `kdos-notice --bless` in the last
autostart phase, which calls `io.kdos.Slots1.Bless`, granted by polkit to the active local
session only. On a machine nobody logs in to, the login screen staying up for a minute earns the
same mark; once any user session has existed in a boot, only the session's own `Bless` counts, so
a greeter that came back because the session crashed confirms nothing. With both marks present,
kdos-sysupdated runs `kdos-bootctl mark-good` once and writes `/run/kdos/bless/confirmed`. Either
mark alone confirms nothing: a slot whose services start and whose desktop does not still spends
its attempt and is rolled back.

A `mark-good` in the same boot that armed a trial leaves the trial as it is: an update applied
within the first minute of a boot has already called `try`, and the `armed` boot id is how
`mark-good` tells.

## Usage

Every `kdos-bootctl` command below that changes the state needs root, because the ESP is mounted
with `umask=0077`. Run as an ordinary user, `kdos-bootctl status` says the boot state is readable
by root alone, and `kdos-bootctl status --json` prints the copy kdos-bootctl publishes in
`/run/kdos` each time it writes the state.

### Reading the state

```bash
sudo kdos-bootctl status
sudo kdos-bootctl status --json
```

The text form starts with one `slot <x>` line for each slot that has a root: the version it
holds and its state, one of `running`, `confirmed`, `armed`, `trial`, `failed` or `inactive`. A
`failed` line follows when a trial ended without being confirmed. Then come the active slot and its
filesystem UUID, the other slot, the [LUKS]{.idx} container of each when it has one, the kernel
version each slot boots from the ESP, and the trial in progress, if any, ending with
`trying   nothing — this slot is confirmed good` when there is none. On a machine installed with
one root, the `other` line reads `other    b  (no root)`.

```bash
kdos-bootctl list --json        # the boot entries, one per slot, in the shape `bootctl list --json` prints
```

The same state is on the System Versions page in System Settings, which lists both slots with the
version, build date, tree commit and kernel each holds. The JSON form carries the same state as one object, starting
`{"configured": true, "active": "a", "trying": null, "attempts": 0, "bootnext": false, …}`. With
no boot state, the text form prints
`no boot state at /boot/efi/EFI/kdos/bootstate — this machine has one root and no rollback`, the
JSON form prints `{"configured": false}`, and both exit 1. On the live medium, where `/` is an
overlay, the text form prints `this is the live medium — it has no boot state and no slots; install
KDOS for A/B updates and rollback` and the JSON form `{"configured": false, "live": true}`.

### Adding slot B to a machine installed with one root

The installer makes slot B itself on the A/B layout. A machine installed with one root can be given
a second on another disk or partition by hand: make a filesystem, copy the system into it, record
it.

1. Make slot B's filesystem, on a partition or logical volume at least the size of the running
   root, for example:

   ```bash
   sudo mkfs.ext4 /dev/<partition>
   ```

2. Mount it, for example at `/mnt/b`, and copy the running system into it:

   ```bash
   sudo mkdir -p /mnt/b
   sudo mount /dev/<partition> /mnt/b
   sudo rsync -aHAX -x --numeric-ids / /mnt/b/
   ```

   These are the flags of the installer's own copy into slot B (`do_slotb` in
   `src/system/kinstall-core/install.c`). `-x` keeps the copy on the root filesystem, so
   `/boot/efi`, `/proc`, `/sys`, `/dev`, `/run`, `/tmp` and `/mnt/b` itself stay out. A `/home` on
   a filesystem of its own stays out too, and both slots then mount it.
3. The copy carries slot A's `/etc/fstab`, whose root line names slot A's filesystem by UUID and
   type. Edit `/mnt/b/etc/fstab` so that line names slot B's filesystem, and its type if it
   differs.
4. Read the UUIDs and record the slot. The second UUID is needed only when slot B is inside its
   own LUKS container; the first is always the filesystem's:

   ```bash
   sudo blkid -s UUID -o value /dev/<partition>
   sudo kdos-bootctl set-slot b <filesystem-uuid> [<luks-uuid>]
   ```

5. Put slot B's kernel on the ESP: `sudo kdos-bootctl deploy /mnt/b b`.

`set-slot` with fewer arguments prints
`usage: kdos-bootctl set-slot <a|b> <uuid> [<luks-uuid>]` and exits 2. It rewrites the slot's
container every time it runs, so running it again without the LUKS UUID records the slot as
unencrypted. It also regenerates the menu, which gains a `KDOS (slot b)` entry, commented
`The other root, for one boot: nothing is confirmed`, once slot B has a kernel on the ESP;
`deploy` reports that kernel as `slot b boots 7.2.11 from EFI/kdos/b`. Until then, `try b`
refuses with
`` bootctl: slot b has no kernel on the ESP — run `kdos-bootctl deploy <its root> b` first ``.

These steps have not been run by hand on a machine with this tree's installer, and a second slot
in a container of its own has not been booted.

### Updating through the inactive slot

Discover shows a KDOS update beside the applications' and installs it into the inactive slot; its
**Restart to update** button reboots into it. The same from a terminal:

1. Fetch and see what is offered: `sudo kdos update fetch`, then `kdos update check`.
2. Run `sudo kdos update apply --mount`. `--mount` mounts the inactive slot at
   `/run/kdos/slot-<x>` for this run; without it, `apply` uses the slot where it is already
   mounted and refuses when it is not. `apply` writes the slot by one of three lanes: the signed
   **slot image** of the newer release when one is published, the prebuilt packages of the signed
   **binhost** otherwise, or with `--source` a **build** on this machine. It then checks the slot
   with `kpkg check --all`, carries this machine's configuration into it (below), records its
   version, runs `kdos-bootctl deploy` for its kernel and `kdos-bootctl try` for the slot. When
   nothing is behind it prints `nothing to take: everything installed matches the ports tree` and
   puts nothing on trial.
3. Reboot. The candidate boots; once the boot is confirmed, `mark-good` makes it the active slot.
4. Check the outcome: `sudo kdos-bootctl status`.

**What the slot receives from this machine.** A slot is a whole root, and only `/home` (with the
pack store bound from it onto `/var/lib/kdos/packs`) is shared. Before a slot is armed, `apply`
carries the running root's configuration into it, file by file under `/etc`:

| The file in the running root | What the slot gets |
|---|---|
| Installed by a package and never edited | The slot's own copy, which is the newer default |
| Edited, or owned by no package (hostname, machine ID, host keys, network profiles) | The running root's copy; a different copy the slot held is kept beside it as `<file>.kdos-new` |
| `passwd`, `group`, `shadow`, `gshadow`, `subuid`, `subgid` | Merged by name: every running entry, then every entry only the slot has |
| `fstab` | The running root's, with the slot's own root line |
| `kdos-release`, `mtab`, `ld.so.cache`, `udev/hwdb.bin` | The slot's own, never carried |

Machine state under `/var` follows as whole trees: the world set, the origin of each package
(`/var/lib/kdos/origin`), the installer's records of boxed applications still to build and groups still to install (`apps-pending`, `groups-pending`), D-Bus's machine ID, Bluetooth pairings, AccountsService, SDDM's state, the ALSA mixer, NetworkManager's
leases, the users' crontabs and `/root`. A `carry = <path>` line in `/etc/kdos/update.conf` adds a
path of your own. After the switch, `find /etc -name '*.kdos-new'` lists every file where the new
version's default and this machine's copy differ. The record of grafted data packs is not carried:
it is `pack-manifest` in the shared pack store, and kdos-packd grafts what it names into the slot
it starts in (see [Packs](packs.md)).

`kdos update apply --in-place` updates the running root instead and gives up the rollback. The
whole update procedure, its tree and its keys are on [Updating](updating.md#applying-an-update).
The update path has not been run against a published tree
([Known gaps](../../kdos/06-reference/known-gaps.md#the-update-path-has-not-been-run-against-a-published-tree)).

### Putting a slot on trial by hand

```bash
sudo kdos-bootctl try b        # or: try b 2, on a menu-led trial
```

With UEFI variables available, `try` arms one boot through `BootNext` and prints
`will boot slot b once, through UEFI BootNext; any boot after that one that has not confirmed it rolls back to a`.
The firmware then starts a load option labelled `KDOS update trial`, which loads
`EFI/kdos/trial/BOOTX64.EFI` and its trial menu, whose `default_entry` is `KDOS (slot b)`. The main
menu's `default_entry` stays on the confirmed slot, and its `KDOS (slot b)` entry is commented
`The update on trial: one boot, then the confirmed root again`. The candidate's entry ends in
`panic=10`, unless the command line already sets `panic=`, so a candidate whose kernel panics
restarts on its own. While the trial is armed,
`status` reads `trying   b once, through UEFI BootNext — any boot after that one starts a`. When
the candidate's boot reaches `mark-good`, it becomes the active slot, `EFI/kdos/trial/` is
removed and the firmware is back on its usual boot order.

Without UEFI variables, `try` leads the trial from the menu. On a [BIOS]{.idx} boot it prints one
line,
`will boot slot b up to 3 time(s), then roll back to a; a kernel that dies before its initramfs is left by picking /KDOS (slot a)`.
On a UEFI boot whose firmware variables cannot be written, which includes one with `efivarfs` not
mounted, that line follows `bootctl: no UEFI BootNext — slot b is led by the menu instead`; with
`efivarfs` not mounted both are preceded by `bootctl: cannot write Boot0000: No such file or directory`. The menu's first
entry is then commented `Try the update in slot b`, and `status` counts down
`trying   b, 3 attempt(s) left`.

`try` refuses the active slot (`bootctl: slot a is already active`), a slot with no root
(`bootctl: slot b has no root filesystem`), and a slot with no kernel on the ESP; each exits 1.

`sudo kdos-bootctl cancel` forgets a trial that has not had its boot yet, firmware one-shot first,
so the next boot starts the confirmed slot. A trial the running boot is part of is not cancelled:
it ends by its confirmation or its rollback.

The System Versions page's **Start This Version Next Time** button arms the same one-boot trial
of the other slot, through kdos-sysupdated and polkit (`io.kdos.slots.try`).

### Rolling back

A trial that does not confirm rolls itself back. On UEFI, the candidate gets one boot. When that
boot is not confirmed (a panic, a hang and a reset, a service that wedges `rcS`, a desktop that
never comes up), the next boot starts the confirmed slot, the initramfs drops the candidate, and
the state records it as `failed`; the next session shows a notice that the update was rolled back.
On a menu-led trial, the boot after the last attempt starts the confirmed slot.

To leave a menu-led trial before its attempts are spent:

1. Reboot and stop the menu's countdown.
2. Pick `KDOS (slot <x>)`, where `<x>` is the confirmed slot. Its comment reads
   `The confirmed root: picking it abandons the update`.
3. After the boot, `sudo kdos-bootctl status` reports
   `trying   nothing — this slot is confirmed good`.

A candidate that has already confirmed is the active slot. To go back to the other one, put it on
trial: `sudo kdos-bootctl try a`.

### Booting the other slot once

Pick `KDOS (slot <x>)` with no trial in progress. Its comment reads
`The other root, for one boot: nothing is confirmed`; no attempt is spent and the active slot
does not change.

## Tips

- On UEFI, check that `rcS` mounted `efivarfs` (`findmnt /sys/firmware/efi/efivars`) before a
  `try`: without it, `try` cannot arm `BootNext` and falls back to a menu-led trial.
- The ESP the installer's erase plan creates holds several kernel and initramfs pairs. On a
  smaller ESP kept by the reuse plan, `deploy` refuses before writing anything when the pair will
  not fit. The installer measures this at install time: its log records the free space, and it
  warns when a second slot's kernel would not fit.
- `deploy` refuses an initramfs that carries no modules for the kernel beside it, and the slot
  keeps the kernel it had. The `linux` package's postinstall is what builds a root's
  `/boot/initramfs-kdos.cpio.gz`; see
  [A new kernel on an installed machine](initramfs-and-rescue.md#a-new-kernel-on-an-installed-machine).
- A word added by hand to the first `/KDOS` entry's command line survives every regeneration of the
  menu, and a word added to any other entry is dropped; see
  [What kdos-bootctl rewrites](boot-menu-and-kernel-parameters.md#what-kdos-bootctl-rewrites).
- On the A/B layout `/home` and the pack store are shared, and everything else is per slot. An
  update carries `/etc` and the machine state forward when it arms a slot; booting the other slot
  by hand, with no update between, boots that slot's own `/etc`, so a setting changed in one
  since the last update is not in the other.

## Troubleshooting

**`kdos-bootctl status` says there is no boot state.** Run it with `sudo`. If root sees none
either, check `findmnt /boot/efi`; a machine installed without an ESP has one root and no slots.

**`kdos update apply` refuses: the slot is not mounted.** Pass `--mount`, or mount the inactive
slot's filesystem (the UUID is in the message) and run it again, or pass `--in-place`.

**`kdos update apply` refuses: this boot runs the slot on trial.** While a trial is running, the
other slot is the confirmed way back, and neither is written until the trial is confirmed or rolled
back. Wait a minute into the session for the confirmation, or check
`ls /run/kdos/bless/`: `userspace` and `graphical` both present and no `confirmed` means
kdos-sysupdated has not run; see `service status sysupdated`.

**A trial is rolled back although the system worked.** The boot was not confirmed. The session
must stay up for a minute with `kdos-session-good.desktop` in `/etc/xdg/autostart`, and
kdos-sysupdated must be running; `/run/kdos/bless/` after a minute shows which mark is missing.

**`try` refuses: slot has no kernel on the ESP.** Run `sudo kdos-bootctl deploy <its root> <slot>`
with the slot's root mounted, then `try` again.

**An update was rolled back.** `sudo kdos-bootctl status` shows
`trying   nothing — this slot is confirmed good` with the old slot active. Mount the candidate and read its logs from that boot only if it reached its own root:
`/run` is a tmpfs, so the `/run/kdos-init.<name>.log` files of that boot are gone; its system log
is in `<mount>/var/log/messages`. A boot that never reached the root left nothing on disk; boot the
candidate's `KDOS (slot <x>)` entry and watch the splash, or use the verbose entry, to see where it
stops.

**A candidate kernel dies before its initramfs on a BIOS machine, and the menu keeps leading with
it.** Limine counts nothing, so no attempt is spent. Pick the confirmed slot's `KDOS (slot <x>)`
entry by hand.

**The boot stops at `UNLOCKING` after a switch of slot.** The slot's container is recorded in the
state as `crypt_a` or `crypt_b`. Check it against `blkid -t TYPE=crypto_LUKS` from the rescue shell
or the live medium, and record it again with both UUIDs:
`sudo kdos-bootctl set-slot <x> <filesystem-uuid> <luks-uuid>`.

## Known gaps

- [Per-slot kernels have been booted on virtual firmware only](../../kdos/06-reference/known-gaps.md#per-slot-kernels-have-been-booted-on-virtual-firmware-only)
- [The update path has not been run against a published tree](../../kdos/06-reference/known-gaps.md#the-update-path-has-not-been-run-against-a-published-tree)
- [A root filesystem on LVM has been booted in one shape only](../../kdos/06-reference/known-gaps.md#a-root-filesystem-on-lvm-has-been-booted-in-one-shape-only)
- [A disk-encryption key is typed, not sealed in the TPM](../../kdos/06-reference/known-gaps.md#a-disk-encryption-key-is-typed-not-sealed-in-the-tpm)

## See also

- [A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection): counting,
  confirming, one kernel per slot, `BootNext`
- [A/B root slots](../../kdos/02-user-guide/installation.md#ab-root-slots): what the installer
  sets up
- [Discover](../../kdos/04-programs/discover.md): installing a system update from the desktop
- [kdos update](../../kdos/04-programs/kdos-command.md#kdos-update) and
  [kdos-bootctl](../../kdos/04-programs/kdos-command.md#kdos-bootctl)
- [Keeping it current](../../kdos/02-user-guide/administration.md#keeping-it-current)
- [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md): the `KDOS (slot <x>)`
  entry and what `kdos-bootctl` rewrites in `limine.conf`
- [The initramfs and the rescue shell](initramfs-and-rescue.md): the initramfs each slot boots,
  and the shell it opens when a slot's root cannot be found
- [Recovering an unbootable system](recovering-an-unbootable-system.md): finding where a boot
  stopped and repairing it from the live medium
- [Updating](updating.md): checking, fetching and applying an update into the inactive slot
- [Encrypted root](encrypted-root.md): a LUKS root, and an encrypted second slot
- [LVM and RAID](lvm-and-raid.md): volume groups and arrays, and the slots' logical volumes

<!-- book-nav -->
---

*Part XVI. System administration, chapter 398.* Previous: [397. Microcode and firmware](microcode-and-firmware.md) · [Contents](../README.md) · Next: [399. Live persistence](live-persistence.md)
