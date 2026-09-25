# kinstall

`kinstall` installs the live image onto a disk. It is a full-screen wizard that works on a bare
Linux console, in a terminal emulator and over a serial line, and it can also run unattended from
an answer file.

This page describes how it is built and why. For using it, see
[Installation](../02-user-guide/installation.md), which repeats none of this.

## Synopsis

```
kinstall [options]
```

| Option | Does |
|---|---|
| `--config FILE` | Read answers from `FILE` |
| `--save FILE` | Write the current answers and exit |
| `--unattended` | Skip the wizard and install from `--config` |
| `--dry-run` | Log every command, execute none |
| `--dump probe` | What the installer sees on this machine |
| `--dump plan` | The steps these answers would run |
| `--json` | Render `--dump` as JSON instead of text |
| `--theme NAME` | One of the eight accents |
| `--no-mouse` | Keyboard only |
| `--ascii` | Box drawing with `-`, `|` and `+`, for odd terminals |
| `--version`, `--help` | Print and exit |

The log of a run is at `/var/log/kinstall.log`.

An unattended run exits non-zero when the install failed.

## Dependencies

`kinstall` links `libkbase`, `libktui` and `libkcolor`, and nothing else — not even a terminal
library. That is what lets it be cross-compiled in phase 1 and exist on every tree from the first
bootable image onward.

Do not give it, or any of those three libraries, a new dependency without moving its build to a
later phase.

The colour library is on the build line because the toolkit's theme file includes the palette
header; the numbers live there and nowhere else. Leaving it out builds fine on a development host
and fails only in the cross build.

A recipe sits beside the sources, so a running KDOS can rebuild the installer natively.

## The file split

| File | Owns |
|---|---|
| `probe.c` | The `/sys` and superblock reader, the partition-table reader, volume-group activation and the logical-volume list, the catalogue and group reader, and the archive hunt |
| `pages.c` | The eleven wizard pages |
| `install.c` | The forked install child and its line protocol |
| `conf.c` | The answer file, and the filesystem table |
| `dump.c` | The non-interactive dumps |
| `main.c` | Chrome, the poll loop, the command line |

The terminal, the cell buffer, the input layer, the widgets, the modals and the palette all belong
to [libktui](../05-developer/c-libraries.md).

## The page model

Eleven pages in one table, each with an identifier, a title, an icon, an optional entry hook, a
draw function and input handlers. The identifiers are the only spelling:

```
welcome  keyboard  time  disk  layout  accounts  system  apps  summary  install  done
```

Pages are found by identifier and never by index. An unattended install jumps straight to the
install page, and a magic number there is right until a page is added in front of it — after which
the child is already forked and the wrong screen is on the display.

## Five decisions that carry the design

### Nothing is written before the summary

Every page fills the configuration structure and only the configuration structure. The install step
is the single point of no return, which is what makes `Back` mean something.

`Next` on the summary page is deliberately refused: the install starts from the button and only
from the button.

### The whole interface is eight colours

A 512-glyph console font makes the terminal steal the foreground intensity bit for the ninth glyph
bit, so the bright colours are unreachable as a foreground and the bold attribute changes the *font
page* rather than the weight. Never emit bold on a console.

Eight slots mean a console and a modern terminal emulator render the same picture. On a console the
installer saves the palette, installs its own, and restores exactly what was there; elsewhere the
same slots go out as true colour or indexed escapes.

### The mouse works on a bare console

The Linux console has no mouse reporting at all, so the input layer opens the input devices
directly and keeps its own pointer: relative devices scaled by the real cell size read from the
framebuffer's reported dimensions, absolute devices mapped straight through, and the pointer drawn
as an inverted cell. Under a terminal emulator it uses the ordinary reporting mode instead and
never touches the input devices.

### The install runs in a forked child

The work stays straight-line sequential code and the parent stays a single-threaded poll loop that
never blocks on a multi-gigabyte copy.

The child redirects its own standard streams to nothing and keeps the protocol on its own
descriptor, so nothing but the emitter can reach the pipe. The protocol is one letter per line:

| Letter | Means |
|---|---|
| `S` | Step *n* has started |
| `K` | Step *n* is skipped |
| `P` | Progress within the current step, as a fraction |
| `N` | A note on the current step |
| `L` | A log line |
| `F` | Failed, with a message |
| `D` | Done |

A step start closes out every earlier step, not just the previous one. A skipped step sits between
them often enough — no repartition, no theme regeneration — that closing only the predecessor
leaves the real one spinning for the rest of the run.

There is no shell and no command string anywhere. Device paths and user names all arrive from
menus, and everything is executed through an argument vector.

### Chrome takes hit identifiers from a reserved range

The sidebar draws before the page, so claiming ordinary focus identifiers there pushes every
control on every page down the focus ring and leaves the caret parked on a decoration, where typing
does nothing at all. Chrome registers in a reserved range that never joins the focus ring.

## The filesystem table

One row per filesystem, and every consumer reads the same row: the menu, the `mkfs` argument
vector, the `fstab` line and the swapfile step.

| Filesystem | `fstab` pass | Swapfile method | Offered for |
|---|---|---|---|
| `ext4` | 1 | `fallocate` | The default: journalled, boring, and what the kernel has built in |
| `btrfs` | 0 | btrfs-specific | Snapshots and transparent zstd compression |
| `xfs` | 0 | `dd` | Large files and parallel I/O; it cannot be shrunk |
| `f2fs` | 0 | `dd` | Log-structured, for flash: a stick, an SD card or a cheap eMMC |

Three things the row encodes, each a way this goes wrong later:

- **The `fstab` pass number is non-zero only for the default filesystem.** A non-zero pass is an
  instruction to run a checker at boot, and there is no checker worth running for the others —
  `f2fs.fsck` exists, but it is a repair tool rather than a boot-time check.
- **The swapfile method differs per filesystem.** Preallocation leaves unwritten extents, which xfs
  and f2fs refuse to swap on, so the failure lands at the *next* boot's swap activation with no
  swap and nothing saying why.
- **xfs and f2fs are kernel modules**, so both appear in the initramfs module list in
  `script/06_packaging/01_initramfs.sh`. ext4 and btrfs are built in. A root on a module the
  initramfs cannot load installs perfectly and never boots again.

A filesystem whose `mkfs` is missing from the image is still listed, with the row saying so, and is
refused before anything is written. A control that snaps back under the cursor is worse than one
that explains itself.

## A root on LVM

The erase plan can put the root on LVM, and the reuse plan can put it on a logical volume that
already exists. Both end in the same place: `root=` and the `fstab` line name the filesystem's UUID,
exactly as for a partition, and the initramfs activates every volume group before it looks for it —
see [Activating volume groups](../03-architecture/boot-and-init.md#activating-volume-groups).

**The erase plan's layout is fixed.** The ESP and any swap partition stay plain partitions. The
root partition is typed Linux LVM and becomes the one physical volume of the group `kdos`, and the
root is the volume `root_a`. With encryption on, the container is opened on the partition and the
physical volume is the container, so the group is inside it and one passphrase opens everything in
it. The commands go through `lvm pvcreate`, `lvm vgcreate` and `lvm lvcreate` rather than their
symlinked names, which an image need not carry.

**The volumes are named for the A/B slots.** `root_a` is slot A's root and slot B's goes beside it
as `root_b`, so a second slot needs no repartitioning. `root_a` takes half the group when half still
holds the install, the swap file when the swap is a file, and 256 MiB to spare, and all of it
otherwise. The swap file counts because it is written onto the same root after the copy. `ki_lvm_half()` makes that
decision, and the Layout page, the Summary, `--dump plan` and the Format step all call it, so the
size shown is the size created. The other half is left unallocated; the installer makes no second
volume, because an empty `root_b` is not a slot anything can boot or update.

**The group name is refused before the point of no return** when a group called `kdos` has any
physical volume on another disk: `vgcreate` would fail after the disk had been erased. The check
asks `lvm pvs` for every physical volume and its group, so it sees a group with no volume, one whose
volumes did not activate, and a group spanning the target disk and another. A group wholly on the
target disk is no conflict, because the erase takes it.

**What was on the disk is taken down first.** The prober activates every group, so a disk that held
a previous install arrives at the Prepare step with its volumes live, and the kernel does not
re-read a partition table under a partition something holds. On the erase plan Prepare walks the
disk's and each partition's
`holders` in `/sys`, top first: a logical volume's whole group is deactivated, a container is
closed, any other device-mapper device is removed, and each is unmounted by device number first.
The Format step does the same again, so a retried Format meets nothing the failed one left open.
The old physical-volume label is then wiped from the new partition, because the erase writes the
same layout at the same offsets and `pvcreate` would find the previous install's label there.

**The reuse plan lists volumes from `/sys`, not from `lvs`.** A logical volume is a device-mapper
device whose uuid is `LVM-` followed by two 32-character uuids and nothing more; a suffix marks a
layer such as a thin pool's `-tpool`, and those are skipped. The dm name `vg-lv`, with each `-`
inside a name doubled, gives the two names, and a volume whose name carries one of LVM's reserved
sub-volume suffixes (`_tdata`, `_cmeta`, `_rimage_`, …) is skipped as well. A thin volume and a
cached volume are ordinary volumes here. The list is every volume on the machine, not only the
target disk's: the root need not share a disk with the ESP. The group activation that makes them
visible runs once per process, as root only, so `--dump probe` run by a user does not fail on it.

A physical volume, and any partition or volume with a holder, is refused as the root. `mkfs` fails
on a held device after the point of no return, and a physical volume it did not fail on would be a
volume group destroyed.

## The applications step

The Applications page lists **groups**, not applications. Seven named bundles is a thing to read
during an install; 180 rows is not. An answer file may still name an application by id — the
catalogue's expander takes either — and `essential` is what an empty answer file gets.

What it reads is the shipped catalogue at `/usr/share/kdos/appstore/catalogue`, which carries a
group's description and each application's own byte estimate — the two things a page anybody can
choose from needs, and neither of them derivable from a list of identifiers. Nothing is baked onto
the medium: an application is built by the container engine on the installed machine, so there is
no pack index here and no pack to copy.

The catalogue is read by linking `kdos-appbox`'s `catalogue.c` directly rather than by running
`kdos-appbox`. That file uses `kb_*` alone, so compiling it in costs no library and the installer
stays a phase-1 program — and a live installer cannot assume anything is on `$PATH` in the target
it is building.

The base and the runtimes are not a choice, and the page says so in a line rather than offering
them as rows: a group holds application identifiers and the expander pulls each one's chain, so a
runtime arrives with whatever needs it. Leaving one out would install applications that cannot
start, which is the one outcome a page of checkboxes must not be able to produce.

A group whose members are all absent from this catalogue is not offered at all. A tick that
installs nothing is worse than a row that is not there.

The entry hook runs before the plan is computed on every path that plans without walking the
wizard — `--dump plan` and `--unattended` — because the selection an answer file names is what the
plan is about.

The store's directories are made whichever route runs, and the staging directory is left mode
`01777`. `kdos-packd` sets that at startup, but a first boot that inherited `0755` would refuse an
import until the daemon had run once — which reads as the feature not working.

### How the applications actually arrive

Nothing is baked onto the medium, so there is no copy to make. The install step resolves in this
order, and the Applications page shows the answer before anything is written: a person must not
discover at first boot that nothing was installed.

| Route | When | What happens |
|---|---|---|
| **import** | An exported `.ktar` is on a mounted device | Staged through `kdos-packd` in the target, which verifies each pack where it mounts it. Offline, and the only route on a machine with no network |
| **network** | A default route exists | Built during the install |
| **pending** | Neither | Written to `/var/lib/kdos/apps-pending`; the first session offers them |

The network test is a route, not a ping. `/proc/net/route` carries a default gateway or it does
not. Opening a socket to somebody else's host to decide what to draw would be an installer reaching
the network to ask whether it can reach the network.

An import or a build that fails falls back to pending rather than failing the install. The system
is bootable by then, and abandoning a disk mid-install over an archive that would not unpack would
leave a machine with no operating system on it.

A stick with an archive on it is bound out of the target's way before the target is mounted. The
target mountpoint is `/mnt` and a mounted stick is at `/mnt/<something>`, so mounting the target
hides the very file the archive is read *from*, and every path into it then resolves inside the
filesystem created empty a moment earlier. It is a bind, not a second mount of the device: the
device is not the installer's to name, since something else mounted it, and a path is the only
handle anything has on it. An archive under `/media` or `/run/media` is left alone, because
mounting `/mnt` does not cover those.

## Answer files

Flat `key = value`, written by `--save` and read by `--config`.

```
keymap timezone timezone_label disk plan esp root format_esp fstype
swap swap_mb luks luks_passphrase lvm hostname username fullname password
root_password root_locked theme alien_apps apps autologin reboot services
```

`autologin = yes|no` is the one key whose default differs from what the live medium ships. The
medium ships `autologin = kdos` — a machine with one account and no password has nothing to ask —
while an answer file that omits the key installs a machine that asks for a password, because a
system somebody installed has a real account with a real password.

The key is written by editing the line in the target's `/etc/kdos/login.conf` —
`autologin = <username>` for yes, `#autologin = <username>` for no — rather than by replacing the
file. That file is mostly the explanation of what the key does, and a one-line rewrite would leave
the installed system with a configuration file nobody can read. Off is a commented line and not an
empty value: both readers of the file hand `agetty --autologin` only a non-empty name, so an
emptied key reads as a setting and behaves as none.

Two fallbacks keep an unattended run from failing over a spelling, and both are deliberate: an
unknown application falls back to the recommended set, and an unknown filesystem falls back to the
default. Both are read before the point of no return, and refusing there would leave a machine with
no operating system on it.

An unattended run ends by itself, whichever way it went. The event loop has two exits — a key, and
the reboot branch — and the reboot branch is gated on a key in the answer file, so an unattended
install told not to reboot would otherwise do all its work and then spin on its own last screen
forever. The test is done **or** failed, and the second half is not decoration: done is set only by
a run that reached the last step, so testing it alone leaves exactly the install that went wrong
spinning on its error screen.

## Dumping

```sh
kinstall --dump probe [--json]
kinstall --dump plan  [--json]
```

Neither needs a terminal and neither writes to a disk. Run as root, `probe` activates every volume
group first, exactly as the wizard's probe does, so the logical volumes it lists are the ones the
reuse plan would offer; run as a user it lists only the ones already active.

`probe` is the machine as the prober sees it. `plan` calls the same planner the wizard does, so the
step list and its skips are the real ones. The structured form is a rendering of the same
traversal, not a second walk.

No password reaches either form. The configuration holds them in the clear because hashing is the
next thing that happens to them, and a dump is what ends up in a log. The test suite asserts this
with a sentinel value.

`--dump probe` is deliberately not in the test suite: it walks the root filesystem to measure the
payload, which is seconds on a live image and far longer on a development machine.

## Iterating without booting

```sh
kinstall --dry-run                      # log every command, execute none
kinstall --save answers.conf
kinstall --config answers.conf
kinstall --unattended --config answers.conf
kinstall --ascii                        # force the lowest glyph tier
kinstall --no-mouse
```

Give it a terminal of its own when something else owns the console.
`kinstall < /dev/tty3 > /dev/tty3` puts the interface where nothing competes for it — and pointing
a full-screen redraw at a serial console makes it the only thing on that wire for as long as the
install runs.

The disk-install harness does exactly that, and prints a heartbeat, because a run that says nothing
until it finishes cannot be told from one that never will.

## What the rest of the tree provides

Each of these exists for the installer and would otherwise be decorative:

- `kdos-getty` loads the keymap the installer writes.
- `rcS` activates swap after mounting, or the swap option would do nothing.
- `fstab` is appended to, never replaced — the shipped file carries the temporary-filesystem entry
  every graphical application depends on.
- Renaming the user rewrites the account files, the primary group's own name, the home directory,
  the owner of the `/etc/subuid` and `/etc/subgid` ranges and `login.conf`'s `autologin`, which is
  what tty1 logs in. The subordinate ranges are looked up by name: one left on `kdos` gives the
  renamed account no mapping, and no rootless box starts.
- **Administrator is membership of `wheel` and nothing else.** The live image ships the account
  in `wheel`, and the sudo port's `%wheel ALL=(ALL) ALL`, the polkit admin rules, `kdos-resctl`,
  `kdos-packd`, `kdos-energyd` and `kdos-powerd`'s configuration verbs (firewall, autologin,
  accent, timezone) grant on that membership — so a non-administrator is taken out of the group,
  and no separate sudoers file is written for one who stays in it. Every other group membership is
  kept, `seat` among them, and `seat` is what a non-administrator's desktop runs on: seatd hands it
  the display, and `kdos-powerd`'s suspend, power-off and reboot, `kdos-mountd` and `kdos-oomd`
  admit it as well as `wheel`. A non-administrator therefore keeps the lid, the power keys, the
  panel's power items and removable-media mounting, and loses sudo, the polkit admin actions and
  everything above.
- The chosen hostname replaces the `127.0.1.1` line of `/etc/hosts` as well as `/etc/hostname`.
  musl resolves the machine's own name from that file alone before the DNS, and the shipped line
  names `kdos`.
- The kernel and initramfs are copied onto the ESP into slot A's directory, `EFI/kdos/a/`, and
  the boot configuration's entries point at those paths and carry `kdos_slot=a`. `kdos-bootctl`
  regenerates those entries later, from the command line written here, and a later kernel reaches
  the ESP through `kdos-bootctl deploy` — see
  [A new kernel](../03-architecture/boot-and-init.md#a-new-kernel). An ESP with no room left for a
  second slot's kernel is reported, because every A/B update would be refused.
- **Both EFI binaries go onto the ESP**, so a disk written on one machine starts on the other: a
  64-bit CPU does not imply a 64-bit firmware, and firmware reads only the
  `EFI/BOOT/BOOT<arch>.EFI` it can execute. `BOOTIA32.EFI` is copied where it exists and skipped
  with a warning where it does not — a Limine built without `--enable-uefi-ia32` installs none, and
  failing the install over a fallback for firmware this machine does not have would throw away a
  working system.
- **The NVRAM entry names the binary this firmware can load**, from
  `/sys/firmware/efi/fw_platform_size`. The removable-media fallback chooses by itself;
  `efibootmgr --create` cannot, so an entry pointing at the wrong one is a boot option that fails.
- **The entry names the partition the ESP is actually on**, read from the kernel's
  `/sys/class/block/<node>/partition` rather than derived from the device name — only the wipe plan
  lays the ESP out at index 1, and a reuse install takes whichever partition was picked. Where that
  index cannot be read the entry is not created and the install warns: the removable-media fallback
  still starts the disk, and a guessed index is a boot option the firmware cannot load.

The ESP is a FAT filesystem, so nothing is copied onto it with an archive-preserving copy. FAT has
no ownership to preserve: such a copy calls the ownership change on every file, the kernel refuses
each, and the copy exits non-zero with a page of errors naming the *source* paths — which reads as
a problem with the boot loader rather than a filesystem that cannot hold what was asked of it.

## See also

- [Installation](../02-user-guide/installation.md) — using the installer
- [Boot and init](../03-architecture/boot-and-init.md) — what it writes, and what boots it
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the catalogue the applications page reads
- [The C libraries](../05-developer/c-libraries.md) — the three it links
- [Testing](../05-developer/testing.md) — the dumps and the disk-install harness
