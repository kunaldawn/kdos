# A/B slots and rollback

An installed KDOS machine can keep two root filesystems, slot A and slot B, and boot an update in
the slot it is not running, falling back to the other when the update does not finish booting.
This page covers reading the boot state, describing a second slot, putting an update on trial,
rolling one back by hand, and what to check when a slot will not boot or will not confirm. How
the initramfs counts attempts, why each slot has its own kernel and how a UEFI trial uses
`BootNext` are explained in
[A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection); this page
gives the procedures.

## Installation

Everything here belongs to [`kdos-bootctl`]{.idx}, one of the names of the `ksvc` binary in
[kdos-tools](#port-kdos-tools) (`/usr/bin/kdos-bootctl` is a link to `/usr/sbin/ksvc`). The image
build copies it into the initramfs, where `kdos-bootctl select` decides which root to boot.
`kdos update apply`, from the same port, is what installs into the inactive slot. The installer,
[kdos-installer](#port-kdos-installer), sets a machine up for slots whenever the install has an
[ESP]{.idx term="EFI system partition"}: it writes slot A's kernel and initramfs into `EFI/kdos/a/`, the first boot state, and
`kdos_slot=a` and `bootstate=UUID=<esp>` on each of the three `/KDOS` menu entries.

There is no [service script]{.idx}. The two halves run at set points of every boot: `select` in the
initramfs, `mark-good` as the last step of `rcS`.

The live medium has no slots and no boot state. Everything on this page applies to an installed
machine only.

## Configuration

### The boot state

The state is one file on the ESP, `EFI/kdos/bootstate`, which is
`/boot/efi/EFI/kdos/bootstate` on the running system. No file in the tree ships it; the installer
writes the first version:

<!-- kfs:include src/system/kdos-installer/install.c lines=2367-2380 pin=ab8cb81addd9 -->

`kdos-bootctl` writes every later version, under a header of its own
(`Written by kdos-bootctl; the initramfs reads it before it mounts a root filesystem.`), and adds
a `bootnext` key, which is empty except during a UEFI trial, when it reads `yes`:

```text
slot_a   = c55c90d8-b72a-4e05-b729-bbd60388a06a
slot_b   = a5549778-64ed-4a32-88a1-bde080ddba46
crypt_a  =
crypt_b  =
active   = a
try      = b
attempts = 1
bootnext = yes
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
| `EFI/kdos/b/…` | Slot B's, once a deploy has put them there | `kdos-bootctl deploy` |
| `EFI/kdos/bootstate` | The boot state | `kinstall`, then `kdos-bootctl` |
| `limine.conf` | The menu; its `/KDOS` entries follow the state | `kinstall`, then `kdos-bootctl` |
| `EFI/kdos/trial/` | `BOOTX64.EFI` and a trial `limine.conf`, during a UEFI trial only; removed when the trial ends | `kdos-bootctl try` |
| `EFI/kdos/memtest.efi`, `EFI/kdos/wallpaper.png` | The memory test and the menu's backdrop, shared by both slots | `kinstall` |

The menu is covered on [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md).

### Where each half runs

In the initramfs, the `BOOT SLOT` stage mounts the ESP, asks `select` for the root and `crypt` for
that root's container, and unmounts it again. The `\$` in this excerpt is the generator's escaping:
the init is written by a heredoc, and the generated script has a plain `$`.

<!-- kfs:include script/phases/70_image/090_initramfs.sh lines=760-815 pin=75069784095f -->

At the end of `rcS`, after every service script has run:

<!-- kfs:include fs/etc/init.d/rcS lines=177-189 pin=31fd40a83831 -->

`mark-good`'s own messages are discarded there; `kdos-bootctl status` afterwards shows what it
decided.

## Usage

Every `kdos-bootctl` command below needs root, because the ESP is mounted with `umask=0077`. Run
as an ordinary user, `kdos-bootctl status` cannot read the state file and reports a machine with
one root.

### Reading the state

```bash
sudo kdos-bootctl status
sudo kdos-bootctl status --json
```

The text form names the active slot and its filesystem UUID, the other slot, the [LUKS]{.idx} container
of each when it has one, the kernel version each slot boots from the ESP, and the trial in
progress, if any:

```text
active   a  c55c90d8-b72a-4e05-b729-bbd60388a06a
other    b  a5549778-64ed-4a32-88a1-bde080ddba46
kernel   a  7.2.11 from EFI/kdos/a
kernel   b  7.2.11 from EFI/kdos/b
trying   nothing — this slot is confirmed good
```

On a fresh install the second line reads `other    b  (no root)` and there is no `kernel   b`
line. The JSON form carries the same state as one object, starting
`{"configured": true, "active": "a", "trying": null, "attempts": 0, "bootnext": false, …}`. With
no boot state, the text form prints
`no boot state at /boot/efi/EFI/kdos/bootstate — this machine has one root and no rollback`, the
JSON form prints `{"configured": false}`, and both exit 1.

### Describing slot B

Nothing in KDOS creates or fills slot B
([Known gaps](../../kdos/06-reference/known-gaps.md#nothing-creates-or-first-fills-the-second-root-slot)).
The steps [Installation](../../kdos/02-user-guide/installation.md#ab-root-slots) gives are: make a
filesystem, copy the system into it, record it.

1. Make slot B's filesystem. On an LVM install to a disk with room for two copies of the system,
   the installer gives slot A (`kdos/root_a`) half of the volume group and leaves the rest free;
   otherwise slot A takes all of it, and slot B needs another disk or partition:

   ```bash
   sudo lvcreate -n root_b -l 100%FREE kdos
   sudo mkfs.ext4 /dev/kdos/root_b
   ```

2. Mount it, for example at `/mnt/b`, and copy the running system into it. The installer's own
   copy uses `rsync -aHAX -x --numeric-ids` with the exclusions in
   `src/system/kdos-installer/install.c` (`do_copy`); `-x` keeps the copy off `/boot/efi` and the
   pseudo-filesystems.
3. The copy carries slot A's `/etc/fstab`, whose root line names slot A's filesystem by UUID.
   Edit `/mnt/b/etc/fstab` so that line names slot B's filesystem instead.
4. Read the UUIDs and record the slot. The second UUID is needed only when slot B is inside its
   own LUKS container; the first is always the filesystem's:

   ```bash
   sudo blkid -s UUID -o value /dev/kdos/root_b
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

Steps 3 to 5 have been run on an installed virtual machine, with slot B made as an LVM snapshot
of slot A given a new filesystem UUID (`lvcreate -s`, `e2fsck -f`, `tune2fs -U random`) in place
of steps 1 and 2; that machine then booted slot B's own kernel. The `rsync` copy of step 2 has
not been run by hand, and a second encrypted slot has not been booted.

### Updating through the inactive slot

1. Mount the inactive slot anywhere. `kdos update apply` finds it by its UUID in the boot state
   and refuses when it is not mounted.
2. Fetch the tree and see what is behind: `sudo kdos update fetch`, then `kdos update check`.
3. Run `sudo kdos update apply`. It builds every package that is behind into the mounted slot,
   checks the slot with `kpkg check`, copies `/ports` into it, runs `kdos-bootctl deploy` for its
   kernel and `kdos-bootctl try` for the slot.
4. Reboot. The candidate boots; if its boot reaches the end of `rcS`, `mark-good` makes it the
   active slot.
5. Check the outcome: `sudo kdos-bootctl status`.

`kdos update apply --in-place` updates the running root instead and gives up the rollback. The
update path has not been run against a published tree
([Known gaps](../../kdos/06-reference/known-gaps.md#the-update-path-has-not-been-run-against-a-published-tree)).

### Putting a slot on trial by hand

```bash
sudo kdos-bootctl try b        # or: try b 2, on a menu-led trial
```

With UEFI variables available, `try` arms one boot through `BootNext` and prints
`will boot slot b once, through UEFI BootNext; any boot after that one that has not confirmed it rolls back to a`.
The firmware then starts a load option labelled `KDOS update trial`, which loads
`EFI/kdos/trial/BOOTX64.EFI` and its trial menu. During the trial the main menu's
`default_entry` also points at `KDOS (slot b)`, commented
`The update on trial: one boot, then the confirmed root again`. The candidate boots with a command
line ending in `panic=10`, so a candidate whose kernel panics restarts on its own. While the trial is armed,
`status` reads `trying   b once, through UEFI BootNext — any boot after that one starts a`. When
the candidate's boot reaches `mark-good`, it becomes the active slot, `EFI/kdos/trial/` is
removed and the firmware is back on its usual boot order.

Without UEFI variables (every [BIOS]{.idx} boot, or a UEFI boot with `efivarfs` not mounted), `try`
prints `bootctl: no UEFI BootNext — slot b is led by the menu instead` and
`will boot slot b up to 3 time(s), then roll back to a; a kernel that dies before its initramfs is left by picking /KDOS (slot a)`.
The menu's first entry is then commented `Try the update in slot b`, and `status` counts down
`trying   b, 3 attempt(s) left`. With `efivarfs` missing on a UEFI machine the first line is
preceded by `bootctl: cannot write Boot0000: No such file or directory`.

`try` refuses the active slot (`bootctl: slot a is already active`), a slot with no root
(`bootctl: slot b has no root filesystem`), and a slot with no kernel on the ESP; each exits 1.

### Rolling back

A trial that does not confirm rolls itself back. On UEFI, the candidate gets one boot. When that
boot does not reach `mark-good` (a panic, a hang and a reset, a service that wedges `rcS`), the
next boot starts the confirmed slot, and the initramfs drops the candidate.
On a menu-led trial, the boot after the last attempt starts the confirmed slot.

On a virtual machine's UEFI firmware a candidate whose `rcS` never ended (a service script that
did not return, its stage left spinning on the splash) was reset by hand; the firmware then
started the ordinary `KDOS` entry, the machine came up on the confirmed slot, and `status` read
`trying   nothing — this slot is confirmed good` with `bootnext` empty again.

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
  `/boot/initramfs-kdos.cpio.gz`; see [The initramfs and the rescue shell](initramfs-and-rescue.md).
- A word added by hand to the first `/KDOS` entry's command line survives every regeneration of the
  menu; see [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md).

## Troubleshooting

**`kdos-bootctl status` says there is no boot state.** Run it with `sudo`. If root sees none
either, check `findmnt /boot/efi`; a machine installed without an ESP has one root and no slots.

**`kdos update apply` refuses: the slot is not mounted.** Mount the inactive slot's filesystem
(the UUID is in the message) and run it again, or pass `--in-place`.

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

- [Nothing creates or first fills the second root slot](../../kdos/06-reference/known-gaps.md#nothing-creates-or-first-fills-the-second-root-slot)
- [Per-slot kernels have been booted on virtual firmware only](../../kdos/06-reference/known-gaps.md#per-slot-kernels-have-been-booted-on-virtual-firmware-only)
- [The update path has not been run against a published tree](../../kdos/06-reference/known-gaps.md#the-update-path-has-not-been-run-against-a-published-tree)
- [A root filesystem on LVM has been booted in one shape only](../../kdos/06-reference/known-gaps.md#a-root-filesystem-on-lvm-has-been-booted-in-one-shape-only)
- [A disk-encryption key is typed, not sealed in the TPM](../../kdos/06-reference/known-gaps.md#a-disk-encryption-key-is-typed-not-sealed-in-the-tpm)

## See also

- [A/B slot selection](../../kdos/03-architecture/boot-and-init.md#ab-slot-selection): counting,
  confirming, one kernel per slot, `BootNext`
- [A/B root slots](../../kdos/02-user-guide/installation.md#ab-root-slots): what the installer
  sets up
- [kdos update](../../kdos/04-programs/kdos-command.md#kdos-update) and
  [kdos-bootctl](../../kdos/04-programs/kdos-command.md#kdos-bootctl)
- [Keeping it current](../../kdos/02-user-guide/administration.md#keeping-it-current)
- `boot-menu-and-kernel-parameters.md`: [Boot menu and kernel parameters](boot-menu-and-kernel-parameters.md)
- `initramfs-and-rescue.md`: [The initramfs and the rescue shell](initramfs-and-rescue.md)
- `recovering-an-unbootable-system.md`: [Recovering an unbootable system](recovering-an-unbootable-system.md)
- `updating.md`: [Updating](updating.md)
- `encrypted-root.md`: [Encrypted root](encrypted-root.md)
- `lvm-and-raid.md`: [LVM and RAID](lvm-and-raid.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 372.* Previous: [371. Microcode and firmware](microcode-and-firmware.md) · [Contents](../README.md) · Next: [373. Live persistence](live-persistence.md)
