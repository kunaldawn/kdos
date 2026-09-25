# Installation

`kinstall` copies the live system onto a disk and makes it bootable. It ships on every image, runs
on a bare console or over a serial line, and can install a machine unattended from an answer file.
This page takes you through it: what to settle before you start, what each page asks for, and what
ends up written where. For how the installer is put together, see
[kinstall](../04-programs/kinstall.md).

## Before you start

Nothing reaches your disk until you press `BEGIN INSTALL` on the Summary page. Every page before it
fills in an answer and does nothing else, so `Back` genuinely goes back, and quitting the wizard
leaves the machine exactly as you found it. `Next` on the Summary page is refused with a message
pointing at the button — a page-forward keystroke is one key away from a page-back keystroke, and
neither is consent to erase a disk.

Two answers are worth settling before you begin, because changing either one afterwards means
installing again:

- **The root filesystem.** It is chosen once and written into the `mkfs`, the `fstab` line and the
  swapfile step together. See [The root filesystem](#the-root-filesystem).
- **Whether the root is encrypted.** LUKS2 is applied when the partition is created, so it cannot
  be added to or removed from a system that already exists. See
  [Encrypting the root](#encrypting-the-root).

You need root, a target disk that is not the medium you booted from, and a disk large enough to
hold the payload with about 768 MB to spare. The wizard checks all three and says which one failed.

## Running the installer

From a terminal on the live system:

```sh
sudo kinstall
```

If something else already owns the console — you are on a serial line, or a session is running —
give the installer a terminal of its own:

```sh
sudo kinstall < /dev/tty3 > /dev/tty3
```

The interface is full-screen and answers both keyboard and mouse. On a bare console it reads the
pointer straight from the input devices, so clicking works on `tty1` with no additional software.

| Key | Action |
|---|---|
| `Tab` / `Shift+Tab` | Move between controls |
| `↑` `↓` `←` `→` | Move inside a list or a field |
| `Enter` / `Space` | Activate, toggle, choose |
| `Alt+←` / `Alt+→` | Previous and next page |
| `Esc` | Back a page |
| `Ctrl+U` | Clear the field you are in |
| `F1` | The key card |
| `L` | The full log, on the Install page |
| `Ctrl+Q`, `Ctrl+C` | Quit. It always confirms, and while an install is running it says the target is left half written |

## The pages

Eleven pages in a fixed order. The sidebar shows where you are and which are behind you; click a
page you have finished to go back to it. Pages ahead are not clickable, because nothing has
validated them yet and their state would be a guess. The `Next` button reads `Review` on the
Applications page, which is the last signal that the questionnaire is over.

### 1. Welcome

The machine as the installer measures it: processor and thread count, memory, how many disks are
visible, how much the install will copy, and the firmware. Under that is a preflight list — running
as root, at least one writable disk, `rsync`, `mkfs.ext4` with `mkfs.vfat`, and the Limine payload.
The first two stop you on this page. The other three are enforced by the install's first step,
which refuses before anything has been written.

The firmware line is a note and never a refusal. Limine goes onto the disk for both firmwares
whichever way this machine started, so what the line changes is which path the firmware will use.
`legacy BIOS` means this machine booted that way and gets the same install, minus the firmware boot
entry, which cannot be created in legacy mode. `32-bit firmware` means a 64-bit CPU whose firmware
is not — an early Atom tablet — and the boot entry then names the 32-bit EFI binary. `Secure Boot
enabled` does not stop the install, but it will stop the result: Limine's EFI binary is unsigned
and KDOS enrols no keys, so the machine refuses to load it until Secure Boot is turned off in
firmware setup.

### 2. Keyboard

The console keymap, with a filter box because the list is long. It is applied as you move through
the list, so you can try it in the box below; on an image with no `loadkeys` the preview is skipped
and the choice is still written. It lands in `/etc/keymap`, which `kdos-getty` loads on every
terminal.

It reaches the graphical session as well. `kdos-desktop` maps the console keymap name onto an XKB
layout, because the console and XKB use different vocabularies for the same layouts. A name whose
XKB layout is not on the image is reported rather than silently defaulted — the alternative is a
session, lock prompt included, that quietly falls back to US QWERTY.

### 3. Time

Choose a zone by name. The list is tzdata's own `zone1970.tab` as shipped on the image, with a
filter box, and the page shows the current local time in the zone under the cursor.

The install writes both halves of the setting. `/etc/localtime` is linked to the zone file, and
`/etc/profile.d/20-timezone.sh` exports `TZ=':/etc/localtime'`. The leading colon is what makes
musl read that same file, so the environment and the symlink cannot name different rules for one
zone. The hardware clock is left exactly as it is.

If the target carries no zone file for the name you picked, the install log says so and the machine
keeps UTC.

The zone also sets the Wi-Fi country. `zone.tab` gives each zone one country code, and the install
writes it to `/etc/modprobe.d/kdos-regdom.conf` as cfg80211's regulatory domain; without it the
radio stays in the world domain, with every 5 GHz DFS channel closed and transmit power capped. A
zone with no country, such as `UTC`, writes nothing.

### 4. Disk

Which disk, and nothing else. Each row carries the size, the transport and the model, and the
existing partition table is drawn as a proportional bar so you can see what is about to go. Under
it sit the disk's capacity and what this install needs.

The medium you booted from is marked and refused — installing onto it pulls the filesystem out from
under the installer. A read-only disk and a disk too small for the payload are refused as well.

### 5. Layout

How that disk is used: the plan, the root filesystem, swap, and encryption.

| Plan | What happens |
|---|---|
| Erase the whole disk | GPT, laid out for KDOS: a 512 MB EFI system partition, a swap partition if you asked for one, then root |
| Use partitions that already exist | You assign the ESP and the root; every other partition on the disk is left alone, and the Partition step is skipped. The root can be a partition or an existing logical volume, and whichever you assign is reformatted, always |
| Partition it myself | `cfdisk` takes over the screen. Make an EFI System partition and a Linux partition, write the table and quit; the installer comes back on the reuse plan for you to assign them |

Swap is a file on the root filesystem, a partition of its own, or nothing. The size box starts at
4096 MiB and the page tells you how much RAM this machine has.

On the reuse plan there is a checkbox to reformat the ESP as FAT32. Leave it off when another
operating system boots from the same ESP.

On the erase plan, `Put the root on LVM` makes the root partition the one physical volume of a
volume group named `kdos`, and the root is its logical volume `root_a`. The ESP and a swap partition
stay plain partitions. `root_a` is slot A's root and takes half the group, and the other half is
left free for slot B's `root_b` — see [A/B root slots](#ab-root-slots). On a disk where half would
not hold the install and its swap file, `root_a` takes all of it; the page and the Summary say which. With
encryption on as well, the group is inside the LUKS container, so one passphrase opens both slots.
A volume group already named `kdos` with any part of it on another disk is refused on this page, because creating the
new one would fail after the disk had been erased.

On the reuse plan the root list holds the disk's partitions and then every logical volume on the
machine, named `vg/lv`. The installer activates every volume group when it probes, so a disk
carrying LVM shows its volumes. A thin or cached volume is offered and boots like any other; a
thin pool's or a cache's internal volumes are not listed. An LVM physical volume, and a partition
or volume something else holds open, are refused as the root, because formatting either would
fail after the point of no return or destroy the group on it.

A software RAID array and an opened LUKS container cannot be chosen as the root.

Encryption is offered on the erase plan only, because encrypting a partition destroys what is on
it and the reuse plan exists for people who are keeping something. Tick it and the passphrase
fields appear.

### 6. Accounts

Hostname, your full name and user name, your password, whether you are an administrator — a member
of `wheel`, and so able to use `sudo` and the desktop's administrative actions; unticked, the
account is left out of `wheel` altogether and keeps suspend, power-off, reboot and mounting its own
media, which answer `seat` as well — and whether the root account is locked. Those last two are
validated together: root locked with you not an administrator is a machine nobody could ever gain
privileges on, and `Next` says so rather than installing it.

The installed machine asks for that password at `tty1`. The live image logs in as `kdos`/`kdos`
without asking, because an image with one account and no password has nothing to ask; a machine
somebody installed has a real account with a real password. Only an answer file turns autologin
back on — see [Installing unattended](#installing-unattended). The live image's root password is
not carried over either way.

Choosing a user name other than `kdos` is a real rename. It rewrites `passwd`, `group` — the
membership lists and the primary group's own name — `shadow`, and the owner of the
`/etc/subuid` and `/etc/subgid` ranges rootless boxes map users through, moves the home directory, and
writes the new name into the `autologin` key of `/etc/kdos/login.conf`, which is the only place the
desktop account is named. `/etc/inittab` carries no account at all.

### 7. System

Three things: the accent, the alien app library, and which services start at boot.

The accent has eight choices and the installer repaints in the one under the cursor, so you are
choosing from the colours rather than from the names. It is applied to the desktop, GTK, the icons,
the cursors, foot and btop.

The alien app library is the pre-baked Debian container behind every graphical application. Leaving
it out installs a much smaller system that can still fetch it later.

Five services are a choice rather than part of the system: NetworkManager, Bluetooth and ALSA state
restore start by default; CUPS and sshd do not. Each one is a flag file in `/etc/service.disabled/`
on the installed machine, so any of them can be flipped afterwards with `service enable` or
`service disable`.

### 8. Applications

The application **groups** this medium knows how to build, with `essential` already ticked. Space,
`Enter` or a click toggles a group; the size total updates as you go and is labelled an estimate,
because it counts each member once, counts no runtime, and what apt resolves on the day depends on
the snapshot.

The runtimes are not choices. An application is built on one, and whichever is needed comes with
it.

The page also says how the applications will arrive, which matters more than the number:

| It says | Meaning |
|---|---|
| from the set on the stick | an exported `.ktar` was found on a mounted device — offline, and verified where each pack mounts |
| built during the install, over the network | this machine has a default route, and they are built now |
| recorded; the first session offers them | neither — so the choice is written down and the first login offers it, because building is minutes of apt and an installer that did that silently looks like one that has hung |

On a medium with no catalogue the page says so and there is nothing to choose; applications are
then installed later with the store.

### 9. Summary

Everything you chose on one screen, with the destructive parts in the error colour and a rule
across the page reading `EVERYTHING ABOVE THIS LINE IS STILL REVERSIBLE`. Two buttons sit under it:
`BEGIN INSTALL`, which is the point of no return, and `Save answer file`, which writes the answers
to `/tmp/kinstall.conf` and installs nothing. Under `--dry-run` the first button reads `REHEARSE
INSTALL`.

### 10. Install

The work, with a progress bar and a live log. It runs in a separate process that reports back over
a pipe, so the interface stays responsive during a multi-gigabyte copy. Steps report as they start,
finish, skip or fail; `L` opens the full log.

A failed step stops there and offers `Retry step`, `View log`, `Shell` and `Abort`, so a fixable
failure — a tool that is missing, a device that went away — can be fixed from the shell and the run
continued from the step that failed.

### 11. Done

What was installed and where, with `Reboot now`, `Back to the live desktop` and `View log`. The log
of the whole run is at `/var/log/kinstall.log`.

## The root filesystem

Four choices. The row you pick is read by the menu, the `mkfs` invocation, the `fstab` line and the
swapfile step, so they cannot disagree.

| Filesystem | `fstab` pass | Swapfile made with | Notes |
|---|---|---|---|
| `ext4` | 1 | `fallocate` | The default: journalled, boring, built into the kernel |
| `btrfs` | 0 | `btrfs filesystem mkswapfile` | Snapshots and transparent zstd compression |
| `xfs` | 0 | `dd` | Large files and parallel I/O; it cannot be shrunk |
| `f2fs` | 0 | `dd` | Log-structured, for flash: a stick, an SD card, a cheap eMMC |

Two details in that table are not arbitrary. Only ext4 gets a non-zero `fstab` pass number, because
a non-zero pass is an instruction to run a checker at boot and there is no checker worth running for
the other three. And the swapfile is made differently on each, because `fallocate` leaves unwritten
extents, which xfs and f2fs refuse to swap on — a mistake there fails at the *next* boot's
`swapon -a`, with no swap and nothing saying why.

ext4 and btrfs are built into the kernel; xfs and f2fs are modules the initramfs carries. A
filesystem whose `mkfs` is missing from the image is still listed, with the row saying so, and is
refused before anything is written rather than failing partway through.

## Encrypting the root

Ticking encryption on the Layout page puts LUKS2 on the root partition. There is no recovery key
and no way into the machine without the passphrase; it is asked for at every boot.

The installer runs `cryptsetup luksFormat` and then `open`, feeding the passphrase on **stdin**
both times. An argument would publish it through `/proc/<pid>/cmdline` to every process on the
machine for as long as `cryptsetup` runs.

Everything after that point talks to `/dev/mapper/kdosroot`, so one name means "where the root
filesystem is" for the `mkfs`, the mount, the `fstab` UUID and the copy.

The boot options then carry two UUIDs, and confusing them is the trap:

```
cryptdevice=UUID=<the LUKS container>:kdosroot
root=UUID=<the filesystem inside it>
```

The second does not exist until the first is open. At boot the initramfs writes the prompt through
the splash, which owns the framebuffer, and reads the keystrokes from `/dev/tty1` rather than from
`/dev/console`, because tty1 is where the keyboard is and `/dev/console` is wherever the kernel
command line last sent it. The passphrase reaches `cryptsetup` on stdin. Three attempts, each
counted on screen, then a shell rather than a reboot loop.

There is no per-keystroke feedback while typing the passphrase. The splash owns the framebuffer and
the shell owns the terminal, so a masked field would mean moving the read into the splash.

An image with no `cryptsetup` refuses the passphrase on the Layout page rather than at the install
step. Discovering it after the point of no return would be the wrong place.

## A/B root slots

The installer writes the initial boot state onto the ESP: slot A is the filesystem it has just
made, slot B is empty, each slot's LUKS container is recorded beside it, and
`bootstate=UUID=<esp>` is added to the kernel options. On an LVM install slot B's room is left free
in the volume group, so its volume is `lvcreate -n root_b -l 100%FREE kdos` rather than a new
partition. Slot A's kernel goes into its own ESP
directory and each menu entry names its slot with `kdos_slot=a`; an update into slot B puts B's
kernel beside it, so a rollback boots the old kernel with the old root.

That is the state machine's starting position rather than a working dual-root setup — filling slot
B is an updater's job. What it gives you now is the machinery: a candidate slot is booted once
through UEFI `BootNext` — three times led by the boot menu on BIOS, or where the firmware request
cannot be written — counted down in the initramfs
before anything is mounted, and is promoted to active only when `rcS` reaches its end. Through
`BootNext` every boot after the candidate's one starts the confirmed slot, so even a kernel that panics before
its initramfs rolls back with no help from anything; on the menu-led trial that kernel is left by
picking the confirmed slot in the menu.

Recording a container per slot is what joins A/B to encryption. The kernel command line can name
exactly one `cryptdevice=`, so the initramfs asks the state file for the chosen slot's container
after `kdos-bootctl select` has chosen. A second *encrypted* slot has never been booted, because
nothing fills one — see [Known gaps](../06-reference/known-gaps.md).

## Installing unattended

An answer file is flat `key = value`, with `#` for comments. Write one, check it, then install from
it:

```sh
kinstall --save answers.conf      # a file of the current answers; installs nothing
kinstall --config answers.conf    # preload them, still interactive
kinstall --unattended --config answers.conf
```

`--save` writes and exits, so on its own it gives you a template of the defaults. To capture a set
of real answers, walk the wizard to the Summary page and press `Save answer file`, which writes
`/tmp/kinstall.conf`; copy that off the machine and hand it back with `--config`.

### The keys

| Key | Value |
|---|---|
| `keymap` | A console keymap name, as `/etc/keymap` carries it |
| `autologin` | `yes` or `1` for autologin at `tty1`; anything else, including the key being absent, means a password prompt |
| `timezone` | The `TZ` value. `:/etc/localtime` is what the wizard writes |
| `timezone_label` | The zoneinfo name `/etc/localtime` is linked to, such as `Europe/Berlin` |
| `disk` | The whole-disk device, such as `/dev/sda` |
| `plan` | `wipe`, `reuse` or `manual`; anything else reads as `wipe` |
| `esp`, `root` | What to use on the `reuse` plan: the ESP is a partition, the root a partition or a logical volume as `/dev/<vg>/<lv>` |
| `format_esp` | `1` to reformat the ESP as FAT32, `0` to keep it |
| `fstype` | `ext4`, `btrfs`, `xfs` or `f2fs` |
| `swap` | `file`, `partition` or `none` |
| `swap_mb` | Size in MiB |
| `luks` | `1` for an encrypted root |
| `lvm` | `1` to put the root on LVM on the `wipe` plan |
| `luks_passphrase` | The passphrase, in the clear |
| `hostname`, `username`, `fullname` | The machine and the account |
| `password` | The user's password, in the clear |
| `root_locked` | `1` to lock root, `0` to set `root_password` |
| `root_password` | The root password, in the clear |
| `theme` | An accent name, such as `bone` |
| `alien_apps` | `1` to install the alien app library, `0` to leave it out |
| `apps` | Space-separated group or application ids |
| `services` | Space-separated names from `networkmanager bluetooth alsa cups sshd`; a name that is absent is a service that does not start |
| `reboot` | `1` to reboot when an unattended run finishes, `0` to exit |

Autologin is the key whose default surprises people. The live medium ships with it on, and an
answer file that omits it installs a machine that asks for a password at `tty1`. The install edits
the key in the target's `/etc/kdos/login.conf` in place — `autologin = <username>` for on, the same
line commented out for off — and that single write is also what carries a renamed account to
`tty1`.

### Passwords

No password is ever written to an answer file. `--save` and the Summary button both leave
`password`, `root_password` and `luks_passphrase` out, and say so in the file's header. Adding them
yourself is how an unattended install gets credentials, and it is a plaintext credential on
whatever medium the file lives on.

Set `password` for an unattended run. Nothing asks, and without it the user's `shadow` entry is
left exactly as the medium shipped it — under the old name, if you also renamed the account.

### What keeps a scripted run safe

- An answer file naming an unknown application falls back to the recommended set. It is read before
  the point of no return, and refusing there would leave a machine with no operating system on it
  over the spelling of one application.
- An answer file naming an unknown filesystem falls back to ext4 rather than failing at the `mkfs`.
- An unattended run always terminates. It holds the finished screen for a few seconds so a watcher
  sees what happened, then reboots or exits according to `reboot`. It exits non-zero when the
  install failed, so a scripted install that returns 0 really did work.

## Looking without installing

```sh
kinstall --dry-run                    # log every command, execute none
kinstall --dump probe                 # the machine as the installer sees it
kinstall --dump plan                  # the steps that would run, including the skips
kinstall --dump plan --json           # the same, machine-readable
```

`--dump probe` is the right thing to paste into a bug report: firmware, processor, memory, and
every disk and partition. `--dump plan` calls the same planner the wizard does, so the step list and
its skips are the real ones — choosing `reuse` skips the Partition step, the default accent skips
the Theme step, and a medium with no catalogue skips the Packs step.

No password appears in either dump. The configuration holds them in the clear because hashing is
the next thing that happens to them, and a dump is what ends up in a log.

`--dry-run` runs the whole wizard and the whole install, logging every command and executing none.
The header carries a `DRY RUN` marker and the Summary button reads `REHEARSE INSTALL`, so there is
no run in which you are unsure which mode you are in.

## What the installer writes

| Path | What |
|---|---|
| The ESP | Limine — both EFI binaries, `BOOTX64.EFI` and `BOOTIA32.EFI` — its generated `limine.conf`, the BIOS second stage, and slot A's kernel and initramfs in `EFI/kdos/a/` |
| The ESP | The A/B boot state file, and the menu's font and wallpaper where the medium has them |
| Root | The system, copied from the medium with `rsync` |
| `/etc/fstab` | Appended to, never replaced — the shipped file carries the `/tmp` entry that every graphical application depends on |
| `/etc/hostname`, `/etc/hosts` | The hostname, and the `127.0.1.1` line that resolves it locally |
| `/etc/keymap` | The console keymap |
| `/etc/localtime`, `/etc/profile.d/20-timezone.sh` | The zone, as a symlink and as `TZ` |
| `/etc/modprobe.d/kdos-regdom.conf` | The zone's country, as the Wi-Fi regulatory domain |
| `/etc/kdos/login.conf` | The `autologin` key, edited in place |
| `/etc/passwd`, `/etc/group`, `/etc/shadow` | The account, renamed and hashed; in `wheel` only when you chose administrator |
| `/etc/subuid`, `/etc/subgid` | The account's subordinate ID ranges, under its new name |
| `/etc/service.disabled/` | One flag file per service you turned off |
| `/etc/resolv.conf` | Copied from the live system, so the installed machine resolves names on first boot |
| `/swapfile` | The swap file, when you chose one |
| `/var/lib/kdos/packs` | The pack store, and the staging directory an import goes through |
| `/var/lib/kdos/apps-pending` | The application groups the first session should offer, on the pending route |

The kernel and initramfs are copied **onto the ESP**, into slot A's directory, and the generated
`limine.conf` points at those FAT paths. Each root slot boots its own kernel from its own directory
there — see [One kernel per slot](../03-architecture/boot-and-init.md#one-kernel-per-slot). Limine reads FAT and ISO9660; it does not read xfs, f2fs or anything under LUKS,
all of which the installer will happily give you as a root. Putting the kernel where the loader can
always reach it is what keeps those choices bootable.

`limine.conf` is written at the root of the ESP rather than beside `BOOTX64.EFI`, because that is
the one location both firmwares search — a BIOS boot never looks in `/EFI/BOOT/`.

Both EFI binaries go on, and firmware reads only the one it can execute. A 64-bit CPU does not imply
a 64-bit firmware, so `BOOTIA32.EFI` sits beside `BOOTX64.EFI` for about a hundred kilobytes and the
two never compete; a Limine built without the 32-bit target installs none, which is a warning in the
log rather than a failed install. The boot entry written into NVRAM names one path, chosen from
`/sys/firmware/efi/fw_platform_size` — the removable-media fallback picks by itself, but an NVRAM
entry pointing at a binary the firmware cannot load is an option that fails rather than one that
falls through.

The BIOS boot code is written whichever way the installing machine booted. It costs one sector, and
it means a disk imaged on a UEFI machine still starts when it is moved to a legacy one. A failure
there is reported and does not abandon the install: on a UEFI machine the EFI path is already
complete.

The generated menu counts down for ten seconds before booting `KDOS`. Press any key during it to
stop the countdown and keep the menu: `KDOS (verbose)`, `KDOS (single user)` and the memory test are
reachable only from there, and a machine that will not boot needs one of them.

## See also

- [Getting started](getting-started.md) — building the image you are installing from
- [kinstall](../04-programs/kinstall.md) — how the installer works inside
- [Administration](administration.md) — services, users and storage after the install
- [Boot and init](../03-architecture/boot-and-init.md) — A/B slots, encryption and the boot path
- [Applications](applications.md) — installing applications after the install
