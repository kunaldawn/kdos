# kinstall

`kinstall` copies the running live system onto a disk and makes it bootable. It has two front ends
over one backend. **Install KDOS**, the graphical installer on the live desktop, is a Kirigami
window that runs `kinstall` through `pkexec` and draws its progress. `kinstall` itself is a
full-screen terminal wizard that works on a bare Linux console, inside Konsole and over a serial
line, and it installs unattended from an answer file. Both describe the same answers, are checked
by the same rules and run the same install steps. This chapter is the reference for the program:
its options and keys, what each page asks, what each install step writes, the answer-file keys, the
event stream the graphical installer reads, and why the design is shaped the way it is. It is for
administrators scripting installs and for contributors changing the installer. If you only want to
install KDOS, read [Installation](../02-user-guide/installation.md) instead; it walks through the
installer with the reasoning left out. The chapter assumes the vocabulary of [Boot and
init](../03-architecture/boot-and-init.md) (the ESP, the A/B root slots, the initramfs); the
[glossary](../06-reference/glossary.md) defines each term.

Three terms recur. The **ESP** is the EFI system partition, the FAT32 partition the firmware and
the bootloader read. The **point of no return** is the Summary page's install button: nothing is
written to any disk before it. A **plan** is how the chosen disk is used: `wipe` (erase and
repartition it), `reuse` (keep its partitions and format only the root), or `manual` (partition
it first, then reuse; in the terminal wizard with `cfdisk`, in the graphical one with Partition
Manager).

## How the installer works

The installer is three pieces:

| Piece | Where | What it is |
|---|---|---|
| `kinstall-core` | `src/system/kinstall-core` | The probe, the answers and the two checks every front end applies to them (the answers, and the target they name), the install steps, the files that make the machine a KDOS Plasma desktop, the dumps and the event stream. It uses `libkbase` and `libkcolor` and nothing that draws |
| `kinstall` | `src/system/kdos-installer` | The terminal wizard, drawn with `libktui`, linked with `kinstall-core` into one binary, `/usr/bin/kinstall` |
| `kdos-installer-gui` | `src/desktop/kdos-installer-gui` | The graphical installer, C++ and QML on Kirigami. It never installs anything itself: it reads `kinstall --dump … --json` and runs `pkexec /usr/bin/kinstall --config - --unattended --events json`. See [The graphical installer](#the-graphical-installer) |

The pages of either front end only fill in a configuration: the keyboard, the time zone, the disk
and plan, the account, the look, the services and the applications. None of them writes to a
disk. The one exception, on a real run, is the partition editor the manual plan opens, which writes
whatever partition table is saved in it; a dry run does not open it. Everything else waits for the
Summary page's install button, the point of no return; see [Nothing is written before the
summary](#nothing-is-written-before-the-summary).

The install itself is thirteen steps run in a forked child process. The child reports over a pipe,
one line per event, each line starting with a letter that says what it is: a step started or
skipped, progress, a note, a log line, a warning, a failure, or done. The terminal wizard draws the
screen from those lines, `--events json` turns each into one JSON line on standard output, and both
write `/var/log/kinstall.log`. See [The install runs in a child
process](#the-install-runs-in-a-child-process).

A warning, the letter `W`, is a condition the installed system will lack while the install carries
on. It is written to the log with a `warning:` prefix and listed on the Install page and again on
the Done page.

## Starting the installer

On the live desktop, **Install KDOS** on the desktop and in the application menu starts the
graphical installer, `kdos-installer-gui`. The entry exists only on the medium: the boot step
`/etc/init.d/63_live-installer.sh` copies it from `/usr/share/kdos/installer` into
`/usr/share/applications` and onto the live account's desktop at every live boot, together with
the polkit rule that lets the live session start an install without a password, and the copy of
the system leaves both out. An installed machine therefore offers no **Install KDOS**, and running
the program there asks for an administrator's password. The welcome tour's live page opens the
same entry.

The terminal wizard needs root and a terminal. From Konsole on the live desktop, or from a console:

```sh
sudo kinstall
```

Before it takes over the screen the wizard prints `kinstall: measuring the live system...` on
standard error and walks the whole root filesystem to measure how much it will copy. The walk
does not cross filesystem boundaries, so `/proc`, `/sys`, `/dev`, `/run` and `/tmp` are never
entered. It takes seconds on the live image and much longer on a development machine.

To keep the wizard away from anything else on the console, give it a terminal of its own. A
virtual console device is owned by root, so the redirection has to be made by a root shell, not
by an ordinary user's:

```sh
sudo sh -c 'kinstall < /dev/tty3 > /dev/tty3 2>&1'
```

Then switch to that console with Ctrl+Alt+F3.

A full-screen program redraws the whole terminal constantly, so whatever terminal it is given, a
spare VT or a serial line, carries nothing else for the whole install. The disk-install harness,
`testing/install-to-disk.sh`, runs its unattended install on `/dev/tty3` this way; see
[The other harnesses](../05-developer/testing.md#the-other-harnesses).

## Synopsis

```
kinstall [options]
```

| Option | Does |
|---|---|
| `--config FILE` | Read answers from `FILE`; `-` reads them from standard input, which is how the graphical installer passes the passwords without a file. Options later on the command line still apply |
| `--save FILE` | Write the current answers (the defaults, any `--config` file and any `--theme`) to `FILE` and exit |
| `--unattended` | Skip the wizard and install from the answers |
| `--events json` | With `--unattended`: draw nothing, and write one JSON event per line on standard output. See [The event stream](#the-event-stream) |
| `--dry-run` | Log every install command and execute none. The header shows **DRY RUN**, and the Layout page's **Open cfdisk** is disabled: see [Trying it without installing](#trying-it-without-installing) |
| `--dump probe` | Print what the installer sees on this machine, and exit |
| `--dump plan` | Print the steps these answers would run, and exit |
| `--dump apps` | Print the boxed application groups and the native groups, ticked as the answers tick them, and the route the boxed ones would take, and exit |
| `--dump look` | Print the accents and desks the answers may name, and exit |
| `--json` | Print `--dump` as JSON instead of text |
| `--theme NAME` | The accent: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect` or `paper`. The default is `bone`. The wizard draws itself in it, and the install gives it to the installed machine; see the `theme` key under [Answer files](#answer-files) |
| `--no-mouse` | Keyboard only |
| `--ascii` | Draw boxes with `-`, `\|` and `+`, for terminals without line-drawing characters. It sets `KDOS_ASCII=1`, the lowest glyph tier of [libktui](../05-developer/c-libraries.md#libktui) |
| `--version` | Print `kinstall` followed by its version (`KI_VERSION` in `kinstall.h`) and exit |
| `--help`, `-h` | Print the usage and exit |

Options are read left to right. `--config` loads its file at the point it appears, so an option
after it overrides the file; `--save` and `--dump` act only after the whole command line is read.
An option that needs an argument and is last on the line counts as unknown. An answer file read
from standard input longer than the reader's buffer is refused rather than read in part, because a
cut line is a key with half a value.

The log of a run is `/var/log/kinstall.log`, opened for appending and set to mode `0640`, owner
root, group `wheel`, at every open (`0600` where the image has no `wheel` group), so the live
account, an administrator, can open it from the window's Done page. It holds commands and their
output and no secret: passwords and the passphrase reach their programs on a pipe, never in an
argument, and nothing logs them. Each line carries an `[HH:MM:SS]` stamp. On the Install page `L` shows it in full, and on the Done page `L`
opens it in the same pager as **View log**.

Only `--dump`, `--save`, `--help` and `--version` work without root. The wizard, `--dry-run`
included, stops on its first page with "kinstall must run as root — try: sudo kinstall" until it
runs as root. An `--unattended` run does not pass through that page; its first step, Prepare,
fails with "kinstall must run as root" instead.

| Exit status | Means |
|---|---|
| 0 | Finished: help, version, a dump, a saved answer file, a completed install, or the wizard quit |
| 1 | An unknown option, an unreadable `--config` file, a `--save` that could not be written, `--events` without `--unattended` or with anything but `json`, or an `--unattended` install that failed (with `reboot = 0`, or under `--events`) |
| 2 | `--dump` given something other than `probe`, `plan`, `apps` or `look`, or, under `--events`, answers refused before anything was written |

## Keys

| Key | Does |
|---|---|
| Tab, Shift-Tab | Move between controls |
| Arrows | Move inside a list or a field |
| Enter, Space | Activate, toggle, choose |
| Alt+Left, Alt+Right | Previous or next page |
| Esc | Back a page, or close the help |
| PgUp, PgDn | Scroll a page taller than the screen; in the full log, scroll ten lines |
| F1 | Show or hide the key help |
| L | The full log: drawn in the page on the Install page, in `less` (or `more`) on the Done page |
| Ctrl+U | Clear a text field |
| Ctrl+W, Ctrl+Backspace | Delete the word before the caret; Ctrl+Left and Ctrl+Right move by word |
| Ctrl+Q, Ctrl+C | Quit the installer. It asks first, and while an install is running it warns that stopping leaves the target half written |

Alt+Left, Alt+Right and Esc do nothing on the Install and Done pages, which have no navigation
buttons.

The installer needs a terminal of at least 62 columns by 18 rows; below that it shows a
screen-too-small notice instead of the wizard. The sidebar listing the pages appears only at 78
columns or wider. On the 25-row Linux console some pages are taller than the screen; PgUp, PgDn
and the mouse wheel scroll them, a scroll bar marks the overflow, and moving the focus with Tab
scrolls the focused control into view.

The mouse works everywhere, including on a bare console (see [The mouse works on a bare
console](#the-mouse-works-on-a-bare-console)). Clicking a finished page in the sidebar walks back to
it; clicking a page ahead does nothing, because pages ahead have not been checked yet. The
sidebar's footer names the accent, the mouse source (`evdev`, `sgr` or `off`) and the colour mode
(`24-bit`, `256`, `vt palette` or `ansi`).

## The pages

The terminal wizard is eleven pages. Each has a fixed identifier, and an unattended install finds
the Install page by that identifier rather than by its position. The graphical installer's pages
are in [The graphical installer](#the-graphical-installer).

| # | Identifier | Title | Asks for |
|---|---|---|---|
| 1 | `welcome` | Welcome | Nothing; shows the processor, memory, storage, firmware and how much space the install needs |
| 2 | `keyboard` | Keyboard | The console keymap |
| 3 | `time` | Time | The time zone |
| 4 | `disk` | Disk | Which disk |
| 5 | `layout` | Layout | The plan, then the partitions, filesystem, swap, LVM and LUKS2 encryption |
| 6 | `accounts` | Accounts | Hostname; then your account now, or an OEM install that leaves it to the first person to start the machine; the login |
| 7 | `system` | System | Accent, desk, the boxed-application container library, services |
| 8 | `apps` | Applications | Which native groups the install downloads, and which boxed application groups this machine builds |
| 9 | `summary` | Summary | Nothing; the point of no return |
| 10 | `install` | Install | Nothing; shows progress |
| 11 | `done` | Done | Reboot |

Pressing **Next** runs the page's checks; a refusal is shown in the bottom bar for eight seconds
and the page stays. The button on the page before the Summary reads **Review** rather than
**Next**.

**Welcome.** Shows the machine: processor and thread count, memory, the number of disks, the
install size, and the firmware (`UEFI` or `legacy BIOS`, with "32-bit firmware" and "Secure Boot
enabled" added when they apply). A preflight list marks each item that fails:

- running as root;
- at least one disk that is not read-only (the line reads "at least one writable disk");
- `rsync` present;
- `mkfs.ext4` and `mkfs.vfat` present;
- the Limine payload, `/usr/share/limine`, present.

The page refuses to continue without root or without a disk that is not read-only. Neither firmware is refused or
warned about: a disk the installer partitions starts on both (see [Both
firmwares](#both-firmwares)).

**Keyboard.** Lists every keymap under `/usr/share/keymaps` and `/usr/share/kbd/keymaps`, with a
filter field. Moving the selection applies the map at once with `loadkeys`, and a test field
shows what the keys produce. The choice is written to `/etc/keymap` on the target, and the login
screen and the session take the graphical layout nearest it (see
[What the rest of the tree provides](#what-the-rest-of-the-tree-provides)).

**Time.** Lists the zones in `/usr/share/zoneinfo/zone1970.tab` (`$KDOS_ZONEINFO` replaces the
directory, for testing), with a filter and a preview of the local time in the selected zone. The
hardware clock is left as it is. An image with no zone table offers `UTC` alone.

**Disk.** Lists every disk except loop, RAM, optical, zram, device-mapper, MD, floppy and NBD
devices, with its size, transport (`sata`, `nvme`, `usb`, `virtio` or `mmc`) and model, and draws
its current partition table. A disk whose driver reports no model (virtio-blk is one) is named by
its transport, `Virtual disk`, `NVMe disk`, `Memory card` or `USB disk`, with its serial number
where the driver has one. It preselects the first disk that is neither the boot medium nor
read-only. It refuses the medium KDOS is running from, a read-only disk, and a disk smaller than
the install size plus 768 MiB, by `ki_disk_check()`, the rule the window and every install share
(see [Fallbacks, and how an unattended run ends](#fallbacks-and-how-an-unattended-run-ends)); a disk
it would refuse is drawn in the warning colour in the list. Its **install needs** line is drawn in the error colour exactly when the
selected disk is that small, so a disk drawn in the plain colour passes this check. The install size is what the live root holds, since
the copy carries the live system whole, less the container library when **Install the alien app library**
is unticked; a native group the Applications page ticks is downloaded into the target and is not
counted. That box is on the System page, after this one, so unticking it lowers the requirement
only when the Disk page is passed again.

**Layout.** Offers the three plans:

- **Erase the whole disk and lay it out for KDOS** (`wipe`): a GPT table with a 512 MiB ESP, a
  1 MiB BIOS boot partition, an optional swap partition and the root. The page offers the
  filesystem, swap (none, a file or a partition, with its size in MiB), **Put the root on LVM**,
  which is ticked by default and is what gives the machine two root slots (see [A root on
  LVM](#a-root-on-lvm)), and **Encrypt the root filesystem (LUKS2)** with its passphrase typed
  twice. A bar and a table show exactly what will be written.
- **Use partitions that already exist** (`reuse`): two lists, the ESP and the root. The root list
  holds the disk's partitions and then every logical volume on the machine (see [A root on
  LVM](#a-root-on-lvm)). **Reformat the ESP as FAT32** is ticked by default; leave it off when
  another operating system boots from the same ESP. The root is always reformatted. A line under
  the lists says where Limine's BIOS stage will go: the disk's BIOS boot partition, the gap after
  a dos table's MBR, or nowhere ("no BIOS boot partition: this disk will start on UEFI only").
- **Partition it myself first (cfdisk)** (`manual`): an **Open cfdisk** button runs `cfdisk` on
  the chosen disk. When `cfdisk` exits, the installer re-reads the disks and switches to `reuse`.
  Under `--dry-run` the button is disabled, and the page says that `cfdisk` would write a real
  partition table and points to **Use partitions that already exist** to rehearse with the table
  as it is. The page asks for an EFI System partition, a Linux partition and, for legacy BIOS, a 1 MiB BIOS
  boot partition.

The reuse plan shows the filesystem but no swap controls, and the swap choice made on the erase
plan still applies: a swap file of the chosen size (4096 MiB by default) on the reused root, or no
swap at all when a swap partition was chosen, because the reuse plan makes no partitions.

The encryption checkbox is drawn on the erase plan only, but its value is not cleared when the plan
changes. A tick made on the erase plan and followed by a switch to `reuse` (directly, or through
`cfdisk`) encrypts the reused root, with no checkbox on the page to show it. The Summary names
LUKS2 on every plan, in the erase plan's LVM line or else in an **encryption** line, so it is the
place to check. Untick the box before leaving the erase plan if the reused root is to stay
unencrypted.

The page refuses an empty or mismatched passphrase, a swap size of zero, the `manual` plan before
`cfdisk` has run and a reuse plan with no ESP or root, then asks `ki_target_check()`, the target
check every install shares (see [Fallbacks, and how an unattended run
ends](#fallbacks-and-how-an-unattended-run-ends)): encryption on an image without `cryptsetup`,
LVM on an image without `lvm`, a volume-group name clash (see [A root on LVM](#a-root-on-lvm)), an
ESP and root that are the same partition, the disk's BIOS boot partition as either, an ESP that is
held or, when it is not to be formatted, holds no FAT filesystem, a root that is an EFI system
partition, an LVM physical volume or held, a root smaller than the install, and, on a machine that
booted in legacy BIOS mode, a reuse plan on a GPT disk with no BIOS boot partition, which could
not start there.

**Accounts.** The hostname (letters, digits and `-`, not starting with `-`, at most 63
characters), then **Who sets it up**:

- **Set up my account now**: full name, user name (lowercase letters, digits, `-` and `_`, not
  starting with a digit, at most 32 characters), and the password twice, with a strength meter.
  **Administrator (member of wheel, may use sudo)** is ticked by default. Under **Login**, **Log in
  automatically after the disk passphrase** is offered only when the root is encrypted, and with it
  **Start the session locked (unlock with this password)**, ticked by default. On an unencrypted
  root the page says that the login screen asks for this password.
- **Let the first person to start it set it up (OEM)**: no account is made. The first time the
  machine is started, the setup wizard asks for the language, keyboard, time zone and network and
  makes the account. See [An OEM install](#an-oem-install).

**Lock the root account (log in as yourself, use sudo)** is ticked by default; unticked, a root
password is asked for twice. The page refuses a malformed hostname or user name, a full name
holding `:` or a control character (it would split the `/etc/passwd` line), an empty or mismatched
password, an empty or mismatched root password while root is unlocked (whether or not the install
is OEM), a user name that belongs to one of the accounts the image carries (`root`, `sddm`,
`plasma-setup` and the rest of `ki_reserved_names`, flagged beside the field as you type), and a
non-administrator while root is locked ("root is locked and <user> is not an administrator —
nobody could ever gain privileges").

**System.** The accent, as eight radio buttons with a swatch each; choosing one repaints the
installer in it. The desk, the panel layout the first login lays out: `kdos` (the default), `dock` or
`tiling`, the last with KWin's auto-tiling script on from the first login. **Install the alien app library** (the live user's
container storage, with its size) is ticked by default. Then the five services of [the service
table](#answer-files), each a checkbox.

**Applications.** One list: the native groups first, each marked `native` with its number of
ports, then the catalogue's boxed groups. Under it, `boxed` is the boxed selection and its
estimated size, `how` the route the boxed groups take, `native` how many native groups are
ticked, downloaded during the install, and `language`, when the keyboard or the time zone points at
a language group, which one and why (`lang-ja ticked for the Japanese keyboard`). See [The
applications step](#the-applications-step) and [Native groups](#native-groups).

**Summary.** Every answer on one screen, above a line reading "EVERYTHING ABOVE THIS LINE IS STILL
REVERSIBLE". **Next** is refused here ("press BEGIN INSTALL to start — Next does not"): the install
starts from the **BEGIN INSTALL** button (**REHEARSE INSTALL** under `--dry-run`) and only from
it. **Save answer file** writes the answers to `/tmp/kinstall.conf` and installs nothing. The
Summary also says which firmwares the disk will start on.

**Install.** An overall progress bar, each step with its state and time, the elapsed time, the
warnings so far under **WARNINGS**, and the tail of the log. When a step fails, four buttons
appear: **Retry step** runs the install again from the failed step, **View log** opens the full
log, **Shell** runs a login `bash` on the console, and **Abort** quits. When every step has
finished, **Continue** appears and moves to the Done page.

**Done.** The time taken, the disk and the log path, the warnings under **FINISHED WITH
WARNINGS** when there were any, a reminder to remove the boot medium, and three buttons: **Reboot
now** (after a confirmation), **Back to the live desktop** (quit), and **View log** (in `less`, or
`more` where `less` is missing).

The Summary's install button, the Install page's Continue button and an unattended run all find
their page by its identifier (`install`, `done`) rather than by its position, so adding a page in
front of them sends none of them to the wrong screen. The Install page has no way back, so the
Summary and its button are not reached again while the child runs. An unattended run stays on the Install page and
ends from there five seconds after the last step (see [Fallbacks, and how an unattended run
ends](#fallbacks-and-how-an-unattended-run-ends)).

## The install steps

The install is thirteen steps, run in order in a child process (see [The install runs in a child
process](#the-install-runs-in-a-child-process)). A step that does not apply is marked skipped
rather than run.

| # | Step | Does | Skipped when |
|---|---|---|---|
| 1 | Prepare | Checks the target and the tools, stops every swap area but zram, takes down anything holding the disk | — |
| 2 | Partition | Writes the GPT layout: a 512 MiB ESP, a 1 MiB BIOS boot partition, an optional swap partition, then the root | The plan is not `wipe` |
| 3 | Format | Encrypts and creates the volume group and its volumes if asked, then makes the filesystems | — |
| 4 | Mount | Mounts the root at `/mnt/kinstall`, the ESP at `/mnt/kinstall/boot/efi`, and on the A/B layout the shared home and the pack store | — |
| 5 | Copy system | Copies the live tree with `rsync`, without the live session | — |
| 6 | Packs | Installs the chosen native groups into the target, and records the chosen boxed applications for the first login | No native group chosen, and no application catalogue or no boxed group chosen |
| 7 | Firmware | Installs the firmware this machine's devices need and the medium does not carry, with `kdos firmware sync --root /mnt/kinstall` ([The firmware step](#the-firmware-step)) | — |
| 8 | Configure | `fstab`, hostname, time zone, keyboard, login screen, services, serial console, swap file | — |
| 9 | Accounts | Users, passwords, groups, the new account's home | — |
| 10 | Theme | Writes the accent and the desk the first login takes, the splash's and console's accent, and the login screen's look | — |
| 11 | Slot B | Copies slot A, as it now stands, into the second root | The machine has one root |
| 12 | Bootloader | Limine on the ESP, kernel and initramfs for each slot, the menu, the BIOS boot code ([Both firmwares](#both-firmwares)), the NVRAM entry | — |
| 13 | Finish | Flushes and unmounts | — |

**Prepare** fails when no disk is chosen, the answers check refuses the answers, the disk has gone,
the installer is not root, the target check refuses the target (both checks are in [Fallbacks, and
how an unattended run ends](#fallbacks-and-how-an-unattended-run-ends)), or a tool is missing:
`mount`, `umount`, `mkfs.vfat`, `rsync`, the chosen filesystem's `mkfs`, `cryptsetup` when
encrypting, `lvm` for a root on LVM, and `limine` on a machine that booted in legacy BIOS mode.
All of it is asked before anything is written. It then runs `swapoff` on every entry of `/proc/swaps` except `/dev/zram*`,
so a swap partition or file on the disk does not keep it busy, while the live session's compressed
swap in memory stays on through the copy. It unmounts everything under `/mnt/kinstall` and
`/mnt/kinstall` itself, and lazily unmounts every mount whose device name begins with the disk's.
The boot medium at `/mnt/iso` and a stick mounted at `/mnt/<name>` are beside the target, not
under it, so this leaves them mounted. On the `wipe` plan it takes down whatever holds the disk (see [A root on
LVM](#a-root-on-lvm)); on `reuse` it unmounts the chosen root by device number.

**Partition** runs `wipefs -a` on the disk, feeds `sfdisk --wipe always` the layout (ESP type
`U` as partition 1, the BIOS boot type `21686148-6449-6E6F-744E-656564454649` as partition 2, swap
type `S` as partition 3 when swap is a partition, and the root last, type `L`, or `V` for an LVM
physical volume), then `partprobe` and `udevadm settle`, waits up to five seconds for the root's
node to appear, and writes zeros over the BIOS boot partition: `limine bios-install` refuses one
whose first sector still reads as FAT or NTFS from an earlier layout. Partition names
follow the kernel's rule: a disk whose name ends in a digit gets a `p` before the number
(`nvme0n1p1`), any other does not (`sda1`).

**Format** opens LUKS and builds the volume group when asked (see [An encrypted
root](#an-encrypted-root) and [A root on LVM](#a-root-on-lvm)), makes the root filesystem with the
label `KDOS`, and on the A/B layout slot B's with `KDOS_B` and the home's with `KDOS_HOME`, formats
the ESP with `mkfs.vfat -F 32 -n KDOS_EFI` when `format_esp` is set, and runs `mkswap` on a swap
partition or the swap volume (`KDOS_SWAP`). Every filesystem is made here, so every UUID the boot
state and `fstab` name exists before either is written.

**Mount** mounts the root at `/mnt/kinstall` and the ESP at `/mnt/kinstall/boot/efi`. On the A/B
layout it then mounts the home volume at `/mnt/kinstall/home` and binds its `.kdos/packs` onto
`/mnt/kinstall/var/lib/kdos/packs`, as `fstab` binds it at every boot, so the box store and the
packs the copy carries land on the volume both slots share.

**Copy system** runs:

```sh
rsync -aHAX -x --numeric-ids --info=progress2 --no-inc-recursive <rules> / /mnt/kinstall/
```

The copy leaves out:

- the contents of `/dev`, `/proc`, `/sys`, `/tmp`, `/run`, `/mnt` and `/media`, and `/lost+found`;
- every file under `/var/log` and `/var/cache`, this run's own log among them, so the installed
  system's log and `last` begin at its own first boot. The directories are kept: packages own them,
  and `kpkg check` reports one that is missing;
- root's `.bash_history`, `.cache` and `.local/share/atuin`;
- **the live session.** The live account's home is the desktop that ran the install: its panel
  layout, its wallet, its file index, the welcome tour marked as seen, its recent files and
  history. Carried over, the installed account's first login would find a layout already laid out,
  the desk chosen here never applied, and the tour already shown. Two things the image built into
  it are kept: bat's syntax cache, and the box store under `.local/share/containers` when the
  alien app library is wanted and the install is not OEM. The Accounts step fills the rest of the
  home as a new account's is;
- what the live boot step `63_live-installer.sh` added: the **Install KDOS** entry in
  `/usr/share/applications` and the polkit rule `/etc/polkit-1/rules.d/49-kdos-installer-live.rules`.
  On an installed system the entry would offer to rerun the installer over the disk the machine
  boots from, and the rule would let any session do it unasked;
- what must be unique to each machine, `/var/lib/dbus/machine-id` and `/etc/ssh/ssh_host_*`, which
  the installed system generates on its first boot;
- the account files' backup copies, `/etc/passwd-`, `/etc/group-`, `/etc/shadow-` and
  `/etc/gshadow-`, which shadow's tools write beside each file they change and which would carry
  over a password set in the live session.

`rsync` exit status 24 ("some files vanished") is accepted, since files come and go on a running
live system. The excluded directories are then created empty, `/tmp` with mode `01777`. The step's
note reads the copy's progress from `--info=progress2` as `<copied> of <install size>, <speed>,
about <time> left`, the byte count turned into a size a person reads; Slot B's copy says the same
against the system without the box store.

The copy streams the tree through the page cache and can fire `kdos-oomd`'s pressure trigger on a
machine with little memory. The daemon kills nothing while most of memory is still available, and
it never picks the copy: when an install starts (other than a dry run), the wizard sets its own
`oom_score_adj` to -900, which the install child and every program it runs (`rsync`,
`cryptsetup`, `mkfs`) inherit, and the daemon passes over any process at or below -500 (see
[kdos-oomd](daemons.md#how-it-decides)). -900 is above the kernel's "never kill", so the kernel's
own OOM killer can still end the install when memory is truly gone.

**Configure** writes the files listed in [What the rest of the tree
provides](#what-the-rest-of-the-tree-provides), copies the live `/etc/resolv.conf`, cuts the
serial console's root login (see [The login](#the-login)), and makes the swap file. The `fstab`
lines it puts in front of the shipped file are:

```
UUID=<root>  /                    <fs>   <options>            0 <pass>
UUID=<esp>   /boot/efi            vfat   defaults,umask=0077  0 2
UUID=<home>  /home                <fs>   <options>            0 <pass>   # the A/B layout
/home/.kdos/packs  /var/lib/kdos/packs  none  bind             0 0       # the A/B layout
UUID=<swap>  none                 swap   defaults             0 0        # a swap partition or volume
/swapfile    none                 swap   defaults             0 0        # a swap file, one root only
```

The home's pass is 2 where the root's is 1, and 0 otherwise. Slot B gets the same file with its own
root's UUID on the first line; every other line names a volume both slots share.

**Accounts** hashes the passwords with SHA-512 `crypt()` and a random salt, renames the live
account when the user name changed, and rewrites `/etc/passwd`, `/etc/group`, `/etc/shadow`,
`/etc/subuid` and `/etc/subgid`. The root entry in `/etc/shadow` is always rewritten: locked
(`!`) or with the root password. The live image's root password never reaches the installed
system: neither the image nor the copy carries the `-` backups of these files. The account's home
is then made a new account's: `/etc/skel` laid under whatever the copy kept, the folders
`/etc/xdg/user-dirs.defaults` lists (the first login's `xdg-user-dirs-update` writes the file over
them), and everything handed to the account by number, because the target's `/etc/passwd` is not
the live system's. An OEM install instead takes the live account out of every database: its
`passwd` and `shadow` lines, its own group, its memberships and its subordinate ranges.

**Theme** writes the look the first login takes, as files, and runs nothing: startplasma applies
the global theme at the first login, and the theme's layout script lays out the desk, so the
account starts exactly as a new account on this machine would.

| File | Holds |
|---|---|
| `/etc/xdg/kdeglobals`, `[KDE] LookAndFeelPackage` | The global theme, `org.kdos.<accent>.desktop`: colours, Plasma style, icons, cursor, splash, fonts and the window decoration |
| `/etc/xdg/kdosrc`, `[Desk] Layout` | The desk the theme's layout script lays out |
| `/etc/xdg/kwinrc`, `[Plugins] krohnkiteEnabled=true` | The tiling desk only: KWin loads its scripts before the first login's layout writes the same key into `~/.config/kwinrc`, so without it auto-tiling would start at the second login |
| `/etc/kdos/accent` | The boot splash's accent, read by `rcS` |
| `/etc/vtrgb` | The text consoles' palette |
| The `sddm` account's `kdeglobals`, `plasmarc`, `kcminputrc` and `kdos-background.png` | The login screen's look, copied from `/usr/share/kdos/greeter/<accent>/` and handed to the account |

The `/etc/xdg` files are the installer's and nobody else's. KDOS's own defaults live in
`/usr/share/kdos/xdg`, last in `XDG_CONFIG_DIRS`; `/etc/xdg` is the machine owner's, and the
installer puts the owner's answers there, where they outrank the distribution's and are themselves
outranked by each person's own settings. Each file is edited in place where the image ships one,
the key it owns replaced and every other line and comment kept. An accent no `libkcolor` scheme names becomes the default, and an unknown desk
becomes `kdos`, so the step never names a theme the image does not carry. The boot menu's colours are written with `limine.conf` itself.

**Slot B** mounts the second root at `/mnt/kinstall-b`, copies slot A into it with
`rsync -aHAX -x`, which keeps the copy on slot A's own filesystem so the shared home and the ESP
mounted under the target are not copied into a slot, rewrites the root line of slot B's `fstab` to
slot B's own UUID, and unmounts it. Both slots then hold the same system, configured, with its
accounts and its accent, and the first update has an inactive slot to be written into.

**Bootloader** is described in [What the bootloader step writes](#what-the-bootloader-step-writes).

**Finish** runs `sync` and unmounts everything under `/mnt/kinstall`, then `/mnt/kinstall` itself.

## Answer files

An answer file is plain `key = value` lines; `#` starts a comment line, and spaces around the key
and value are trimmed. `--save` writes one with every key except the three secrets, and
`--config` reads one, from a file or, with `-`, from standard input. A key the installer does not
recognise is ignored and named on standard error, `kinstall: <file>: unknown key '<key>' ignored`,
before anything else runs, so a misspelt key shows in `--dump plan` rather than installing that
key's default unnoticed.

```sh
kinstall --save answers.conf                      # the defaults, to edit
kinstall --config answers.conf --dump plan        # what they would do
kinstall --config answers.conf --unattended       # do it
```

A yes-or-no key written `yes`, `true` or `1` is on and anything else is off, except those read as a
number (`format_esp`, `luks`, `lvm`, `root_locked`, `alien_apps`, `reboot`), where `1` is on.

| Key | Values | Default | Notes |
|---|---|---|---|
| `keymap` | A console keymap name | `us` | Written to `/etc/keymap` |
| `xkb_layout` | An XKB layout, such as `de` or `us,ru` | none | The graphical keyboard. Empty: the login screen and the session take the layout nearest `keymap`. See [The keyboard](#the-keyboard) |
| `xkb_variant` | An XKB variant | none | With `xkb_layout` |
| `timezone` | A `TZ` value | `:/etc/localtime` | Written to `/etc/profile.d/20-timezone.sh`. Keep the default: the colon form makes musl read the same file `/etc/localtime` points at |
| `timezone_label` | A zone name, `Area/City` | `UTC` | `/etc/localtime` is linked to `/usr/share/zoneinfo/<zone>`, and the Wi-Fi country is taken from it |
| `disk` | A whole-disk device, such as `/dev/nvme0n1` | none | Required |
| `plan` | `wipe`, `reuse` or `manual` | `wipe` | **Any other value means `wipe`, which erases the disk.** `manual` behaves as `reuse` in an unattended run |
| `esp` | A partition device | none | The EFI system partition, for `reuse` |
| `root` | A partition or logical-volume device | none | The root, for `reuse` |
| `format_esp` | `1` or `0` | `1` | Format the ESP as FAT32. It applies on every plan; with `wipe` the new ESP has no filesystem until it is formatted, so leave it at `1` |
| `fstype` | `ext4`, `btrfs`, `xfs` or `f2fs` | `ext4` | An unknown name falls back to `ext4` |
| `swap` | `file`, `partition` or `none` | `file` | **Any other value means `none`**. A swap partition is made only on the `wipe` plan; on `reuse`, `partition` means no swap at all. On the A/B layout either is a swap volume |
| `swap_mb` | Megabytes | `4096` | |
| `luks` | `1` or `0` | `0` | Encrypt the root with LUKS2. The wizard offers it only on the `wipe` plan; an answer file applies it to whichever root the plan names |
| `luks_passphrase` | Text | none | Never written by `--save` |
| `lvm` | `1` or `0` | `1` | Put the root on LVM, with the `wipe` plan. This is what gives the machine two root slots; see [A root on LVM](#a-root-on-lvm) |
| `hostname` | Text | `kdos` | |
| `oem` | yes or no | no | Make no account; the first person to start the machine makes one. See [An OEM install](#an-oem-install) |
| `username` | Text | `kdos` | The live account is renamed to this |
| `fullname` | Text | `KDOS User` | No `:` and no control character |
| `password` | Text | none | The user's password. Never written by `--save`. **See the warning below** |
| `admin` | yes or no | yes | Keep the user in `wheel` |
| `root_password` | Text | none | Used only when `root_locked = 0`. Never written by `--save` |
| `root_locked` | `1` or `0` | `1` | A locked root has no password and cannot log in |
| `autologin` | yes or no | no | See [The login](#the-login). Refused unless `luks = 1`, and on an OEM install |
| `autologin_lock` | yes or no | yes | With `autologin`: the session starts behind the lock screen |
| `theme` | An accent name | `bone` | One of the eight `libkcolor` schemes. The answers check refuses any other name; the terminal wizard given one draws itself in `bone` |
| `desk` | `kdos`, `dock` or `tiling` | `kdos` | The panel layout the first login lays out. The answers check refuses any other name |
| `alien_apps` | `1` or `0` | `1` | Copy the boxed-application container library |
| `apps` | Space-separated group ids, or `none` | `essential` | The catalogue's boxed groups. Empty or absent means `essential`. See [The applications step](#the-applications-step) |
| `groups` | Space-separated native group ids, or `none` | `none` | The groups of `/ports/groups`, installed during the install. Empty means `none`, and an id no group has is ignored. See [Native groups](#native-groups) |
| `reboot` | `1` or `0` | `1` | Reboot when an unattended terminal install ends |
| `services` | Space-separated service names | `networkmanager bluetooth alsa` | A listed service is on when its name appears anywhere in the value; every other listed service is disabled |

The services an answer file can turn on or off, and whether each is on by default:

| Name | Service | Default |
|---|---|---|
| `networkmanager` | NetworkManager — wired, Wi-Fi and VPN | on |
| `bluetooth` | The bluez daemon | on |
| `alsa` | Restore mixer levels at boot | on |
| `cups` | The printing daemon | off |
| `sshd` | The OpenSSH server, which accepts remote logins | off |

Each name matches an init script `/etc/init.d/NN_<name>.sh`. A disabled service gets an empty file
`/etc/service.disabled/<name>` on the installed system, which `rcS` skips; removing the file
turns the service back on. The live image ships `sshd`'s marker, and choosing the SSH server here
is what removes it on the installed system; port 22 stays closed in the firewall until `ssh` is
opened there (see [firewall](daemons.md#firewall)). Other services (udev, D-Bus, elogind, the
display manager and the rest) are not choices and cannot be turned off here.

Passwords in an answer file are in plain text. `--save` never writes `password`,
`root_password` or `luks_passphrase`, and the file's header says so; add them yourself only if you
accept that credential sitting on the medium the file lives on. The graphical installer never
writes a file: it hands the answers to `kinstall` on standard input.

**An answer file without `password` installs nothing.** The answers check, which the Prepare step
applies to every install, refuses an account with no password (`set a password for <user>`) before
anything is written, in a terminal `--unattended` run as on the wizard's Accounts page and under
`--events json`. An OEM install makes no account and needs none. Always set `password` in an answer
file that makes an account.

### The login

The installed machine boots to SDDM's login screen. `[Autologin]` in
`/etc/sddm.conf.d/kde_settings.conf` is the one place automatic login is set, the same file System
Settings' Login Screen page writes, so the installer and that page never disagree. The image ships
the live medium's group (the live account, `Relogin=true`); every install replaces it:

- **With `autologin` on an encrypted root**, the group names the account with `Relogin=false` and
  `Session=plasma`. The disk's passphrase is the question the boot already asked, so the login
  screen is not asked again. With `autologin_lock` on (the default), the installer also writes
  `/etc/kdos/autologin-xdg/kscreenlockerrc` with `LockOnStart=true`. SDDM's autologin PAM stack
  puts that directory first in `XDG_CONFIG_DIRS` for an automatic login and for nothing else, so
  the session comes up behind the lock screen, and turning autologin off later in System Settings
  cannot leave every password login starting locked as well.
- **Anywhere else** the group is removed and the login screen asks for the password. An unencrypted
  machine that logged in by itself would open for whoever switched it on, so `autologin` without
  `luks` is refused.

Every install that makes an account writes `/etc/plasma-setup-done`, which keeps the first-boot
wizard asleep; an OEM install leaves it absent.

The same step makes the serial console ask for a login. The image's `/etc/inittab` runs
`serial-getty --autologin root … ttyS0` on the `S0` line, which hands its arguments to `agetty`
and logs root in on the serial port with no password: right for a live medium, and on an installed
machine a root shell for anyone who plugs into the port. The installer cuts `--autologin root` from
that one line of the target's copy and keeps the rest of the file as it ships, so `ttyS0` on the
installed system is a plain `agetty` login prompt. tty1's display manager and tty2's recovery
prompt are not touched.

### The keyboard

One choice sets the console and the session:

- `/etc/keymap` is the console's keymap, which `kdos-getty` loads on every text console. An answer
  that named only an XKB layout gets the console keymap nearest it.
- With an XKB layout, `/etc/kdos/xkb.conf` holds it for `kdos-settingsd`, which serves it as
  locale1's keyboard: the login screen's KWin reads it before anybody types, and startplasma hands
  it to every session. `/etc/xdg/kxkbrc` carries the same layout as the Keyboard page of System
  Settings shows it.
- Without one, `xkb.conf` is left as the image ships it, with an empty layout, which
  `kdos-settingsd` reads as the layout nearest the console keymap, and no `kxkbrc` is written.

### An OEM install

`oem = yes`, or **Let the first person to start it set it up** on the Accounts page, installs a
machine for somebody else. No account is made: the live account is taken out of every database,
because one left behind would be a second login with the image's published password, and
plasma-setup skips its account page on a machine that already has a user. Root stays locked unless
a root password is given (the window's Account page offers one under OEM as well), and automatic
login is refused.

`/etc/plasma-setup-done` is left absent, and the boot step `/etc/init.d/62_plasma-setup.sh`, which
runs before SDDM starts, arms plasma-setup's first-boot wizard while the file is missing: it
prepares the wizard's account and home and runs plasma-setup's boot utility, which has SDDM log
that account in. The wizard asks for the language, keyboard, time zone and network, makes the
first real account, an administrator, writes `/etc/plasma-setup-done` and removes its automatic
login, so the next boot shows the login screen and the step does nothing.

### Fallbacks, and how an unattended run ends

Every front end applies one check to the answers, `ki_answers_check()` in `kinstall-core`, before
anything is written. It refuses: no disk, a disk path too long to hold, an accent or a desk that is
not one of the names above, an XKB layout or variant holding a character XKB does not use, a
malformed hostname, a time zone that is not a zone name (`..`, an absolute path, a quote or a
control character, because the `TZ` value is written between single quotes into a script every
login shell sources), a missing or mismatched passphrase on an encrypted root, a reuse plan with no
ESP or root, a swap size of zero, an unlocked root with no root password or two that differ, and,
unless the install is OEM, a malformed user name or one of the accounts the image carries (such as
`root`, `sddm`, `polkitd` or `plasma-setup`), a full name with `:` or a control
character, a missing or mismatched password, a non-administrator with root locked, and automatic
login on an unencrypted root. A second check, `ki_target_check()`, then holds the answers against the
machine as the probe saw it: the disk is one the probe found, by `ki_disk_check()` not the boot
medium, not read-only and at least the install size plus 768 MiB; the tools the plan needs
(`cryptsetup` for an encrypted root, `lvm` for the erase plan on LVM) are on the image; no volume
group named `kdos` has a physical volume on another disk; and on the reuse plan the ESP and root
pass the rules the Layout page lists. `--events json` asks the answers check before the install
starts and exits 2 on a refusal; the Prepare step asks both in every install, the terminal
wizard's, a terminal `--unattended` run's and an `--events json` run's included, and fails the
install before anything is written (in an `--events json` run, status 1 with the reason in step
0's `F` line); the wizard's Disk and Layout pages ask the target check before they let you on; and
`--dump plan` prints the first refusal of either, which is how the graphical installer's Summary page refuses
what the install would. A user name the answers check refuses for being an account's of the image
is listed as `reserved_names` in `--dump look`, from which the window's Account page refuses it
too.

Three kinds of mistake fall back rather than fail, because each is read before the point of no
return, and refusing there would leave nothing installed over a spelling mistake:

- an unknown filesystem becomes `ext4`;
- an `apps` line that matches no group becomes `essential`, and a `groups` id that names no group
  is dropped;
- an unknown `plan` becomes `wipe`, and an unknown `swap` becomes `none`. Check these two with
  `--dump plan` before running unattended.

A terminal unattended run ends by itself whichever way it went. When the install finishes or fails,
the last screen stays up for five seconds, then:

- with `reboot = 1` (the default) the machine reboots, after a failure as well as after success;
- with `reboot = 0` the installer exits, with status 1 if the install failed and 0 if it succeeded.

Set `reboot = 0` when a script needs the exit status. A failed run ends too because a run that
waited for success alone would sit on its error screen indefinitely, and that is the run somebody
most needs an answer from. An `--events json` run never reboots: it exits with the install's status
and leaves the reboot to the front end that started it.

## Checking before installing: the dumps

```sh
kinstall --dump probe [--json]
kinstall --dump plan  [--json]
kinstall --dump apps  [--json]
kinstall --dump look  [--json]
```

None needs a terminal, none writes to a disk, and each prints to standard output only. The
graphical installer reads all four, with `--config -` for the answers so far.

- **`probe`** is the machine as the installer sees it: firmware (with Secure Boot, and in the text
  form its width when it is 32-bit), processor, memory, whether the session is live, the size of
  the system it would copy and of the container library within it, the disks (whether each is
  removable, read-only or the boot medium) and their partitions (filesystem, label, and whether
  each is an ESP, a BIOS boot partition or mounted), and the logical volumes. For each disk it
  also says whether the install fits (`fits`, and the reason in `problem`, by `ki_disk_check()`)
  and whether two slots do (`ab_fits`, by `ki_ab()` on that disk erased onto LVM), and at the top
  what the copy takes (`install_bytes`), what a disk must hold (`need_bytes`) and what two slots
  need (`ab_need_bytes`); the text form prints a refused disk's reason and `one slot` under it. The
  sizes follow the answers given with `--config`: the swap, and whether the box store is copied. It
  is what to paste into a bug report. Run as root it first activates every LVM volume group, as the
  wizard does, so the logical volumes it lists are the ones the reuse plan would offer; run as an
  ordinary user it lists only those already active. A device it may not open, which is every disk
  when it runs as an ordinary user, is described from udev's database instead,
  `/run/udev/data/b<major>:<minor>`, world-readable: `ID_FS_TYPE`, `ID_FS_LABEL` and `ID_FS_UUID`
  for the filesystem, `ID_PART_ENTRY_TYPE` for the ESP and BIOS boot types and
  `ID_PART_TABLE_TYPE` for the table, so the window, whose dumps run as the session's user, sees
  what is on each partition. It measures the live system by walking the root filesystem, so the
  test suite does not run it.
- **`plan`** runs the same planner the wizard does, so the steps and their skips are the real
  ones, and the verdict of the answers check and then the target check (see [Fallbacks, and how
  an unattended run ends](#fallbacks-and-how-an-unattended-run-ends)) as `refused`. It also prints the `mkfs` command and the
  root's `fstab` line that the filesystem choice becomes, the services turned off, and for the
  boxed applications the selection, the route and an estimated size, and the native groups chosen
  (`groups` in the text form, a `"groups"` array of ids in the JSON). It probes the firmware and the disks
  as the wizard does, so it names the partitions the install will use: the erase plan's ESP, BIOS
  boot partition and root by their device names, and on the reuse plan the disk's own BIOS boot
  partition, the MBR gap, or `none: the disk starts on UEFI only`. Whenever the answers name a
  disk the probe found it also measures the live system, because the target check, the A/B
  decision and the slot size depend on it. The swap it reports is the one the install makes: on the
  reuse plan a `swap = partition` answer makes none, and the plan says `none`.
- **`apps`** lists the catalogue's groups, each with its number of applications, its size and
  whether the answers tick it, then the route the ticked ones take: `pending` when any is ticked,
  `none` otherwise. Then the native groups: in the text form a `native groups <n>` line and a row
  for each, ticked or not, with its id, its number of ports and its title, and `(for the Japanese
  keyboard)` after a language group the answers point at; in the JSON a `"native"` array of
  `{"id", "title", "ports", "chosen", "why"}`, `why` being that reason (`the Japanese keyboard`,
  `the time zone Asia/Tokyo`) or empty.
- **`look`** lists the accents, each with its label and its global theme `org.kdos.<name>.desktop`,
  the desks, the default of each marked, and the user names the answers check refuses
  (`reserved_names`).

The JSON form is a rendering of the same data in the same pass, not a second walk. Its
`"firmware_bits"` is the firmware's width, `32` or `64`, and `0` when the machine booted in legacy
BIOS or its kernel does not report the width. **No password
appears in any form**: the plan says whether root is locked and whether the root is encrypted,
never a password or passphrase. `testing/selftest.sh` checks this with a sentinel value, and
checks that `--save` never writes one either.

## Trying it without installing

```sh
kinstall --dry-run                                  # every command logged, none executed
kinstall --config answers.conf --dry-run --unattended # rehearse an answer file
kinstall --ascii                                    # the lowest glyph tier
kinstall --no-mouse                                 # keyboard only
```

A dry run still needs root: the Welcome page and the Prepare step both refuse without it. It
walks every step, logs each command with "(dry run: not executed)", and its install steps write
no file on any disk; `/var/log/kinstall.log` is still appended to. What a real install would
learn from the copied system it reads from the live root instead, so the login screen's set for
the accent is found as the copy would carry it; a fact only a real run produces is logged as such
("root UUID: read back from … after mkfs, which a dry run does not run"), and the Packs step says
`would record` and `would install group <id>`. The Layout page's **Open
cfdisk** button is disabled, because `cfdisk` writes its table itself, outside every command the
rehearsal holds back; the reuse plan rehearses with the disk's table as it is.

An unattended rehearsal ends as an unattended install does: with `reboot = 1`, the default, it
reboots the machine five seconds after its last step. Put `reboot = 0` in the answer file to
rehearse without a reboot.

## The filesystem table

Every choice about the root filesystem comes from one table row in `conf.c`: the menu, the `mkfs`
command, the `fstab` line and the swap-file step all read the same row.

| Filesystem | `mkfs` | Mount options | `fstab` pass | Swap file made with | Offered for |
|---|---|---|---|---|---|
| `ext4` | `mkfs.ext4 -F` | `defaults,noatime` | 1 | `fallocate` | The default: journalled, well understood, built into the kernel |
| `btrfs` | `mkfs.btrfs -f` | `defaults,noatime,compress=zstd:3` | 0 | `btrfs filesystem mkswapfile` | Snapshots and transparent zstd compression |
| `xfs` | `mkfs.xfs -f` | `defaults,noatime` | 0 | `dd` | Large files and parallel I/O; it cannot be shrunk |
| `f2fs` | `mkfs.f2fs -f` | `defaults,noatime` | 0 | `dd` | Log-structured, for flash: a USB stick, an SD card or cheap eMMC |

Three columns each prevent a failure that would only show up later:

- **Only ext4 gets a non-zero `fstab` pass.** A non-zero pass asks for a filesystem check at boot,
  and none of the others has a checker worth running then (`f2fs.fsck` exists, but it is a repair
  tool).
- **The swap file is made differently per filesystem.** btrfs needs a file that is not
  copy-on-write and not compressed, and its `swapon` answers `Invalid argument` for one `fallocate`
  made. `fallocate` leaves unwritten extents, and whether `swapon` takes those on xfs and f2fs
  depends on the kernel (the KDOS kernel does), so on those two the file is written out in full
  with `dd`. The wrong method installs cleanly and then boots with no swap and nothing saying
  why. Where `fallocate` or `btrfs` is missing, the step falls back to `dd`.
- **f2fs is a kernel module**, so it is in the initramfs module list in
  `script/phases/70_image/090_initramfs.sh`. ext4, btrfs and xfs are built in: `kdos.config` in the
  `linux` port sets all three to `=y`. The list
  names xfs as well, and copying a built-in driver is a no-op. A root on a filesystem the
  initramfs cannot load installs without error and does not boot.

A filesystem whose `mkfs` is missing from the image is still listed, marked "no mkfs on this
image", and the Prepare step refuses it before anything is written. A control that silently snaps
back is harder to understand than one that says why.

## A root on LVM

The erase plan can put the root on LVM, and the reuse plan can put it on a logical volume that
already exists. Either way `root=` and the `fstab` line name the filesystem's UUID, as for a
partition, and the initramfs activates every volume group before looking for it; see
[Activating volume groups](../03-architecture/boot-and-init.md#activating-volume-groups).

### The erase plan's layout

The erase plan's layout is fixed:

- The ESP, the BIOS boot partition and any swap partition stay plain partitions.
- The root partition is typed Linux LVM and becomes the only physical volume of a volume group
  named `kdos`.
- With encryption on, LUKS is opened on the partition and the physical volume is inside it, so one
  passphrase opens the whole group.
- The commands are `lvm pvcreate`, `lvm vgcreate` and `lvm lvcreate`, not the short names, which an
  image need not carry.

### The A/B layout

LVM on the erase plan is the default, because it is what gives the machine two root slots: an
update is written whole into the slot that is not running, and a machine with one root has nowhere
to write it. When the group has room, the Format step makes four volumes:

| Volume | Size | Mounted at |
|---|---|---|
| `root_a` | One slot | `/` when slot A runs |
| `root_b` | One slot | `/` when slot B runs |
| `swap` | `swap_mb`, when swap was asked for (a file or a partition both become this volume) | — |
| `home` | The rest of the group (`-l 100%FREE`) | `/home`, by both slots |

A slot is the system and room for its next version: the live system less the container library,
plus half as much again for an update to grow into, never less than 8 GiB, rounded up to a whole
GiB (`ki_slot_mb()`). The group has room when, after 32 MiB for the LUKS header and LVM metadata,
it holds two slots, the swap and a home of at least 4 GiB plus the container library the copy
carries into it (`ki_ab()`). The Layout page, the Summary, `--dump plan` and the Format step all
ask the same two functions, so the layout shown is the layout created.

What both slots share lives on the home volume: the homes themselves, and the pack store, which
`fstab` binds from `/home/.kdos/packs` onto `/var/lib/kdos/packs` at every boot. A slot is
replaced whole by an update, and packs and boxes are never part of one; kept in a slot they would
be gone after the switch. The [Slot B](#the-install-steps) step fills the second root, and the boot
state names both (see [What the bootloader step writes](#what-the-bootloader-step-writes)).

A group too small for that holds one root: `root_a` takes the whole group, with no `root_b`, no
`home` volume and swap as a file. The reuse plan and an erase plan without LVM give one root too.

### Volume-group name clashes

A name clash is refused before the point of no return. If a volume group called `kdos` has any
physical volume on another disk, `vgcreate` would fail after this disk had been erased. The
Layout page asks `lvm pvs` for every physical volume and its group, so it catches a group with no
volumes, one whose volumes did not activate, and one spanning the target disk and another. A group
wholly on the target disk is no clash, because the erase removes it.

### Taking down what holds the disk

Whatever held the disk is taken down first. The installer activates every volume group when it
probes, so a disk holding an earlier install reaches the Prepare step with its volumes live, and
the kernel will not re-read a partition table under a partition something holds. On the erase
plan, Prepare walks the `holders` of the disk and each partition in `/sys`, top first: a logical
volume's whole group is deactivated, a LUKS container is closed, any other device-mapper device is
removed, and anything mounted is unmounted by device number. A holder that is not a device-mapper
device, such as an MD array, is named in the log and left. The Format step does the same again, so
a retried Format meets nothing a failed one left open. On an unencrypted root it then wipes the old
physical-volume label from the new partition, because the erase writes the same layout at the same
offsets and `pvcreate` would find the previous install's label there; an encrypted root needs no
wipe, since `luksFormat` has just written over the partition.

### Listing volumes for the reuse plan

The reuse plan lists volumes from `/sys`, not from `lvs`. A logical volume is a device-mapper
device whose uuid is `LVM-` followed by exactly two 32-character uuids; anything with a suffix is an
internal layer (a thin pool's `-tpool`, for example) and is skipped. The device-mapper name
`vg-lv`, with each `-` inside a name doubled, gives the two names, and a volume whose name carries
one of LVM's reserved sub-volume suffixes (`_tdata`, `_tmeta`, `_cdata`, `_cmeta`, `_corig`,
`_cpool`, `_rimage_`, …) is skipped as well. Thin and cached volumes are ordinary volumes here. The
list covers every volume on the machine, not only those on the target disk: the root need not share
a disk with the ESP. The activation that makes them visible (`lvm vgchange -aay`, then `udevadm
settle`) runs once per process and only as root, so `--dump probe` run as a user does not fail on
it.

The Layout page refuses a physical volume, and any partition or volume that something holds, as the
root. `mkfs` would fail on a held device after the point of no return, and on a physical volume it
did not fail on it would destroy a volume group.

## An encrypted root

With encryption on, whatever the plan (see the Layout page in [The pages](#the-pages)), the Format
step runs `cryptsetup luksFormat --type luks2 --batch-mode --key-file=-` on the root partition and opens it as `/dev/mapper/kdosroot`. The passphrase goes to
`cryptsetup` on its standard input both times, never as an argument, because an argument is
readable through `/proc/<pid>/cmdline` by every process on the machine while the command runs.
From then on the filesystem, the mount, the `fstab` UUID and the copy all use the mapper device;
with LVM, the mapper device becomes the physical volume.

The boot entries carry two UUIDs, and they are different things: `root=UUID=` names the filesystem
inside the container, and `cryptdevice=UUID=<container>:kdosroot` names the container itself. The
filesystem's UUID does not exist until the container is open, which is why the initramfs unlocks
first and looks second. The container's UUID is also recorded as `crypt_a` in the boot state, and
on the A/B layout as `crypt_b` too, because both slots are volumes of the one group inside the one
container (see [What the bootloader step writes](#what-the-bootloader-step-writes)). The passphrase
is asked for at every boot; there is no recovery key.

## The applications step

The installation medium carries the base system, and applications beyond it come one of two ways.
A **native group** is a set of ports the release's binary host holds prebuilt, downloaded into the
target during the install ([Native groups](#native-groups)). A **boxed group** is a group of the
shipped catalogue, whose applications the container engine builds on the installed machine after
the first login. The Applications page lists both, native groups first, and so chooses what to
download and what to build, not what to copy.

The installer reads the catalogue from the first of these that exists:

1. the file `$KDOS_CATALOGUE` names, when it is set (the test suite points it at a fixture);
2. `/usr/share/kdos/appstore/catalogue`, the live system's own copy;
3. `/mnt/iso/appstore/catalogue`, on the boot medium.

With no catalogue the page lists the native groups alone, and with no groups of either kind it says
that nothing can be chosen; the Packs step is skipped when nothing of either kind is chosen.

**It lists groups, not applications.** Seven named bundles is something to read during an
install; the shipped catalogue's 70 applications is not. The catalogue
(`src/system/kdos-appbox/catalogue`, described in [The
catalogue](../03-architecture/packs-and-boxes.md#the-catalogue)) defines these groups:

| Group | Description |
|---|---|
| `essential` | What a new machine starts with; ticked by default |
| `office` | Documents, spreadsheets and reading |
| `creative` | Images, audio and video |
| `dev` | Programming and electronics |
| `science` | Computation, modelling and data |
| `make` | Three dimensions: model it, slice it, cut it |
| `games` | Games and emulators |

Each row shows the number of applications and a size estimate; Space, Enter or a click toggles it.
The estimate counts each member once and no runtime at all, since shared runtime layers are stored
once on disk. A group whose members are all missing from this catalogue is not offered: a tick
that installs nothing is worse than a row that is not there.

The base system and the runtimes are not choices, and the page says so in one line rather than
offering them as rows. Each application pulls in the runtime it needs, so leaving a runtime out
could only produce applications that cannot start.

In an answer file, `apps` names group ids, or `none` for no boxed applications at all. An application id such as `app.scribus` matches no group and is ignored, and if nothing
on the line matches, the selection is `essential`. With no `apps` key, or an empty one, the
selection is `essential`. The page's selection logic runs before planning
on every path that plans without walking the wizard (`--dump plan` and `--unattended`), because the selection is
what the plan is about.

The catalogue is read by compiling `kdos-appbox`'s `catalogue.c` into the installer rather than by
running `kdos-appbox`. That file uses only `libkbase`, so it costs no extra library, and a live
installer cannot assume anything is on the target's `$PATH`.

### Native groups

The native groups are read from `/ports/groups` on the live system, the file
[`kdos update groups`](kdos-command.md#install-routes-and-groups) reads (`$KDOS_GROUPS` names
another, for a test): a `group <id> <title>` line, then its member ports. Each row shows the title
and the number of ports; none is ticked by default, because each one downloads, except the
language group the answers point at. In an answer file `groups` names their ids, or `none`; it is
a key of its own beside `apps` because ids such as `office`, `games` and `science` are both a
native group and a boxed one. `--save` writes it.

**The language group starts ticked.** The base image carries no Chinese, Japanese or Korean input
method or fonts; the groups `lang-zh`, `lang-ja` and `lang-ko` do. One table in
`kinstall-core/probe.c` maps a choice to its group: the XKB layout `cn` or `tw`, or the time zones
`Asia/Shanghai`, `Asia/Urumqi`, `Asia/Hong_Kong`, `Asia/Macau` and `Asia/Taipei`, to `lang-zh`;
the layout `jp`, the console keymap `jp106` or `Asia/Tokyo` to `lang-ja`; the layout `kr`,
`Asia/Seoul` or `Asia/Pyongyang` to `lang-ko`. Each layout of a list such as `us,jp` counts. The
installer asks for no locale, so the keyboard and the time zone are what it has; the keyboard is
checked first, so its reason is the one shown. The group is ticked only when the answers have no
`groups` line (an answer file that names `groups`, even `groups = none`, gets what it names, and
`--save` always writes one), and the person can untick it. Each front end says why: the terminal
wizard's `language` line, the graphical Applications page's "Ticked for the Japanese keyboard"
beside the row. Going back to the Keyboard or Time zone page and changing the choice applies only
what changed: a group the new choice points at is ticked, one the old choice ticked and the new one
does not is unticked, and an untick of a group still pointed at stays.

The Packs step installs them. With `KDOS_UPDATE_CACHE` set to the target's
`/var/lib/kdos/binhost`, because the live system's cache is in RAM and a group of a few gigabytes
would fill it, it runs `kdos update fetch --binhost-only`, which takes the medium's own release's
binary-host index and not a newer release's, whose packages are built against a newer base; then
`kdos update install --prebuilt --root /mnt/kinstall <id>` for each chosen group, which puts its
packages into the target with the target's own world set and origin file, so the installed machine
updates them like anything else installed on purpose, and starts with its release's index in
place. When the fetch fails, every chosen group, and otherwise each group that did not install,
is written to the target's `/var/lib/kdos/groups-pending` with a warning, never a failure: the
system is installed and boots without them, and
[`kdos update install --pending`](kdos-command.md#install-routes-and-groups) installs them later.

### The firmware step

The base image carries the firmware of the common graphics, Wi-Fi and sound devices; the rest of
`linux-firmware` is packages in the release's binary host. The Firmware step runs
`kdos firmware sync --root /mnt/kinstall`, which reads this machine's devices (the live system runs
on the hardware being installed) and installs the firmware packages they name into the target,
prebuilt, with `KDOS_UPDATE_CACHE` at the target's `/var/lib/kdos/binhost`. It uses the index the
Packs step fetched, or fetches it with `kdos update fetch --binhost-only` when no native group was
chosen, so it runs only after the index arrived. Every line the command prints goes into the
install log, which is how the log says what went in, followed by `installed the firmware this
machine's devices name`. When the index cannot be had, when the command fails, or when the medium's
`kdos` has no `firmware` command, the step ends with a warning saying to run `sudo kdos firmware
sync` after the first boot, listed on the Install and Done pages; it never fails the install,
because the machine boots on the medium's firmware.

### How the applications arrive

The installer builds no boxed application. The ticked boxed group ids are written to
`/var/lib/kdos/apps-pending` on the installed system; the first session offers them ([Applications chosen during
installation](../03-architecture/session.md#applications-chosen-during-installation)), and
[`kdos app install --pending`](kdos-command.md#kdos-app) builds them. The Applications page, the
Summary and `--dump plan` all say so before anything is written, so nobody discovers at first boot
that nothing was installed; `--dump apps` and `--dump plan` report the route as `pending`, or
`none` when nothing is ticked.

**`kdos-appbox` has no target root**, which is why the install does not run it. Run from the Packs
step it would ask the live `kdos-packd` on `/run/kdos-packd.sock`, and whatever it imported or
built would go into the live system's pack store and container storage and end with the live
medium. An exported set (a `.ktar` file, see [export and import](kdos-appbox.md#export-and-import))
is imported on the installed machine instead, with `kdos-appbox import`.

The Packs step logs `recorded <n> group(s) for the first login: <ids>` for the boxed groups and
`installing group <id> (<title>)` for each native one. A list it cannot write is a
warning, not a failure, listed on the Install and Done pages: by then the system is installed and
bootable.

Whatever is ticked, the store's directories are created on the target:
`/var/lib/kdos/packs`, `/var/lib/kdos/packs/staging` (mode `01777`) and `/var/lib/kdos/packs/mnt`.
`kdos-packd` sets the staging mode when it starts, but a first boot that found it `0755` would
refuse an import until the daemon had run once, which would look like the feature not working.

## What the bootloader step writes

Everything the bootloader reads lives on the ESP. The ESP is FAT, which Limine reads from BIOS and
from UEFI alike and which UEFI firmware can read by itself; a kernel on the root filesystem would
need a driver for filesystems (xfs, f2fs, LUKS) that no bootloader reads. The Limine files come
from `/usr/share/limine` on the live system, or from the target's copy; with neither the step
fails.

| Path on the ESP | From |
|---|---|
| `EFI/BOOT/BOOTX64.EFI` | Limine's 64-bit UEFI loader |
| `EFI/BOOT/BOOTIA32.EFI` | Limine's 32-bit UEFI loader, where Limine provides one |
| `limine-bios.sys` | Limine's BIOS second stage |
| `limine.conf` | The menu, written by the installer |
| `EFI/kdos/a/vmlinuz` | The target's `/boot/vmlinuz-kdos` |
| `EFI/kdos/a/initramfs.cpio.gz` | The target's `/boot/initramfs-kdos.cpio.gz`, else `/boot/initramfs.cpio.gz` |
| `EFI/kdos/bootstate` | The initial boot state |
| `EFI/kdos/font.bin` | The menu typeface, from the boot medium's `/mnt/iso/boot/limine/font.bin`, else the running system's own `/boot/efi/EFI/kdos/font.bin`, when present. Neither is in a root filesystem, and the medium stays readable because the target is mounted beside it at `/mnt/kinstall` |
| `EFI/kdos/wallpaper.png` | `/usr/share/kdos/boot/kdos-backdrop.png`, else `kdos-banner.png`, when present |
| `EFI/kdos/memtest.efi` | `/usr/share/kdos/memtest86plus/memtest.efi`, when present |

On the A/B layout slot B gets its own copy of the kernel and initramfs, `EFI/kdos/b/vmlinuz` and
`EFI/kdos/b/initramfs.cpio.gz`, because every later update replaces one slot's pair and never the
other's.

`limine.conf` sits at the root of the ESP because that is the one path Limine searches on both
firmwares. It sets `timeout: 10` and `default_entry: 1`, takes its colours from the chosen accent
through the same `libkcolor` function that writes the medium's own menu, and has three entries,
each booting slot A's kernel with `kdos_slot=a`:

| Entry | Command line after the common part |
|---|---|
| `KDOS` | `rw console=tty0 quiet loglevel=3 vt.global_cursor_default=0` |
| `KDOS (verbose)` | `rw console=tty0 loglevel=7` |
| `KDOS (single user)` | `rw console=tty0 loglevel=7 single` |

The common part is `kdos_slot=a bootstate=UUID=<ESP> [cryptdevice=UUID=<container>:kdosroot]
root=UUID=<root>`. The `KDOS` entry's three quiet words keep the kernel's messages and the
console's cursor off the boot splash; they are the same words `kdos-bootctl` writes when it
regenerates the menu. On the A/B layout a fourth entry, **KDOS (slot b)**, boots slot B's kernel
with `kdos_slot=b`, slot B's root and the quiet words, for one boot with nothing confirmed, in the
shape `kdos-bootctl` gives the other slot. When the memtest86+ payload is present a further entry,
**Memory Test (memtest86+)**, boots it on UEFI only. The ten-second countdown matches the medium's: the menu is
the only way to reach the verbose entry and the memory test, which a machine that will not boot
needs. Up or Down cancels the countdown; Enter, Space and Right boot the highlighted entry at
once. The verbose and single-user entries carry no `quiet`, so the splash does not run on them and
the boot's messages are on the screen. The `KDOS (single user)` entry passes `single`, which the
initramfs hands on to init: the services start and `sulogin` asks for root's password on the
console, with no desktop. With root locked, the installer's default, `sulogin` gives no shell and
the boot carries on to the normal system, so that entry is useful only once a root password is
set; see
[Single-user and emergency mode](../03-architecture/boot-and-init.md#single-user-and-emergency-mode).

The boot state file records the machine as running slot A, with slot B filled beside it on the
A/B layout:

```ini
slot_a   = <slot A's root filesystem UUID>
slot_b   = <slot B's root filesystem UUID, or empty with one root>
crypt_a  = <LUKS container UUID, or empty>
crypt_b  = <the same container on the A/B layout, or empty>
active   = a
try      =
attempts = 0
```

It is written directly rather than through `kdos-bootctl`, because the tool's default path is the
running system's ESP and this runs against a target at `/mnt/kinstall`. From then on
`kdos-bootctl` maintains the file (see [A/B slot
selection](../03-architecture/boot-and-init.md#ab-slot-selection)) and regenerates the `KDOS`
entries on every change of state (see [One kernel per
slot](../03-architecture/boot-and-init.md#one-kernel-per-slot)).

### Room for a second slot

The kernel and initramfs go into slot A's directory, `EFI/kdos/a/`, and later kernels reach the
ESP through `kdos-bootctl deploy` (see [A new
kernel](../03-architecture/boot-and-init.md#a-new-kernel)). On a machine with one root the
installer still checks that the ESP has room for a second slot's kernel and initramfs beside the
first, plus 1 MiB, for a second slot made later. The 512 MiB ESP the erase plan makes has plenty; a
reused 100 MiB one may not. The free space and the need are logged, and when the room is short a
warning says so on the Install and Done pages.

### Both firmwares

Both UEFI loaders go onto the ESP, so a disk written on one machine starts on another. A 64-bit
processor does not imply 64-bit firmware, and firmware only reads the `BOOT<arch>.EFI` it can run.
`BOOTIA32.EFI` is copied where Limine provides it and skipped where it does not (a Limine built
without `--enable-uefi-ia32` has none), with a warning that the disk will not start on 32-bit UEFI;
failing the install over a fallback for firmware this machine does not have would throw away a
working system.

The BIOS boot code is written whatever firmware the install ran on, so the disk also starts on the
other. `limine-bios.sys` goes to the root of the ESP, and `limine bios-install` writes Limine's boot
sector into the disk's first sector and its second stage where the partition table leaves room:

| The disk | `limine bios-install` is given | Second stage goes |
|---|---|---|
| Erase plan | `<disk> 2` | The 1 MiB BIOS boot partition the layout made |
| Reused GPT disk with a BIOS boot partition | `<disk> <its number>` | That partition |
| Reused dos (MBR) disk | `<disk>` | The gap between the boot sector and the first partition |
| Reused GPT disk with none | Not run | Nowhere: the disk starts on UEFI only |

On a GPT disk the gap after the boot sector holds the partition table itself, which is why Limine
needs a partition there and refuses a GPT disk without one
(`error: Installing to a GPT device, but no BIOS boot partition specified or detected.`). The
partition is recognised by its type GUID, `21686148-6449-6E6F-744E-656564454649`, which
`--dump probe` reports as `BIOS boot`.

What a failure costs depends on the machine. Booted from legacy BIOS, the BIOS stage is this
machine's only way to start the disk, so a reused GPT disk with no BIOS boot partition is refused
on the Layout page and by the Prepare step, before anything is written, and a failed `limine
bios-install`, or no `limine` on the `PATH`, fails the Bootloader step. Booted from UEFI, the EFI
path is complete and each of these is a warning on the Install and Done pages; Limine's own
`error:` lines are in `kinstall.log`.

A disk laid out by the erase plan, with `limine bios-install <disk> 2` run on it, has started
Limine (`Limine 12.9.0 (x86-64, BIOS)`) and then its kernel on SeaBIOS in the rig. No whole
installed system has been booted on legacy BIOS.

### The NVRAM boot entry

The NVRAM boot entry, labelled `KDOS`, names the loader this firmware can run, chosen from
`/sys/firmware/efi/fw_platform_size` (a machine that does not report it is treated as 64-bit). The
removable-media fallback path picks between the two loaders by itself; `efibootmgr --create`
cannot, and an entry pointing at the wrong one is a boot option that fails. The entry is created
only when the install ran under UEFI and `efibootmgr` is present, and a failure of `efibootmgr` is
logged and ignored.

The entry names the partition the ESP is actually on, read from
`/sys/class/block/<node>/partition` rather than guessed from the device name: only the erase plan
puts the ESP at index 1, and a reuse install takes whichever partition was picked. Where the index
cannot be read no entry is created, and a warning on the Install and Done pages says so; the
removable-media fallback still starts the disk, whereas a guessed index would be a boot option the
firmware cannot load.

### No ownership-preserving copies

Nothing is copied onto the ESP with an ownership-preserving copy. The ESP is FAT, which has no
ownership: such a copy tries to change the owner of every file, the kernel refuses each one, and the
copy fails with a page of errors naming the *source* paths, which looks like a problem with the
boot loader rather than a filesystem that cannot store what was asked of it.

## What the rest of the tree provides

The installed system depends on these, and each exists because of the installer:

- `kdos-getty` loads the keymap the installer writes to `/etc/keymap`, with `loadkeys`, and
  `kdos-settingsd` serves the graphical layout from `/etc/kdos/xkb.conf`, or the one nearest the
  keymap (see [kdos-settingsd](daemons.md#kdos-settingsd)).
- `rcS` turns swap on after mounting (`swapon -a`), because `mount -a` ignores swap lines.
  Without it the swap choice would do nothing.
- `fstab` is added to, never replaced. The shipped file carries the `tmpfs` line for `/tmp`
  with `mode=1777`, which every graphical application depends on; the installer's lines go in
  front of it.
- Renaming the user rewrites `/etc/passwd` (the name, the full name and the home directory),
  `/etc/group` (the membership lists and the primary group's own name), `/etc/shadow` (when a
  password is given), the home directory itself, and the owner of the `/etc/subuid` and
  `/etc/subgid` ranges. The subordinate ranges are looked up by name: a range left on `kdos` would
  give the renamed account no mapping, and no rootless box would start. `[Autologin] User=` in
  SDDM's settings is written with the new name.
- Administrator means membership of `wheel` and nothing else. The live image ships the account
  in `wheel`. Everything that grants administrator rights keys on that membership: the sudo rule
  `%wheel ALL=(ALL) ALL`, the polkit rules in `/etc/polkit-1/rules.d/50-kdos.rules`, and the root
  daemons' sockets (`kdos-powerd`'s changes, `kdos-energyd`, `kdos-packd`). So a non-administrator
  is taken out of `wheel`, and no separate sudoers file is written. Every other group the image put
  the live account in is kept. What a desktop needs at the machine, the devices, suspend, power-off
  and removable media, elogind and polkit grant to the active local session, not to a group, so a
  non-administrator keeps all of it and loses sudo and the administrators' actions; see
  [Who may ask for what](daemons.md#who-may-ask-for-what).
- The hostname replaces both `/etc/hostname` and the `127.0.1.1` line of `/etc/hosts`. musl
  resolves the machine's own name from that file before asking DNS, and the shipped line names
  `kdos`.
- The time zone writes `/etc/profile.d/20-timezone.sh`, links `/etc/localtime`, and writes the
  Wi-Fi country from `zone.tab` to `/etc/modprobe.d/kdos-regdom.conf` as
  `options cfg80211 ieee80211_regdom=<CC>`, the files `kdos-settingsd` keeps when the zone is
  changed later (see [kdos-settingsd](daemons.md#kdos-settingsd)). A zone the image does not carry
  is logged and leaves the machine on UTC; a zone with no country (UTC) writes no regulatory file.

## The event stream

`kinstall --config <answers> --unattended --events json` installs with no screen and writes one
JSON object per line on standard output. The first line is the plan, so a front end draws the same
steps the install will run, skips included; every line the install's child sends after it is the
same event in JSON; the last is the result:

```text
{"t":"plan","steps":[{"index":0,"title":"Prepare","detail":"…","state":"pending"}, …]}
{"t":"S","step":4,"title":"Copy system"}    a step begins
{"t":"K","step":1}                          a step is skipped
{"t":"P","step":4,"frac":0.42}              its progress, -1 unknown
{"t":"N","step":4,"text":"1.2G copied …"}   its one-line note
{"t":"L","text":"$ mkfs.ext4 …"}            a log line
{"t":"W","text":"…"}                        a warning; the install goes on
{"t":"F","step":9,"text":"…"}               the failure, with its reason
{"t":"D"}                                   every step finished
{"t":"exit","status":0}                     the last line, always
```

Status 0 is a finished install, 1 a failed one, and 2 answers refused by the answers check before
anything was written, with the reason in an `F` line whose step is -1. The process exits with the
same status. A child that dies without its `F` line is still reported as failed, with why.

Each line is written whole and flushed, because the reader is a pipe and a half line held in a
buffer is a progress bar that stops until the next step. Nothing but these lines reaches standard
output; the child's commands write into the log. A front end that closes the pipe has gone away,
and the install goes on to its end, because stopping a copy half way leaves the disk with no
operating system; its log is still `/var/log/kinstall.log`.

## The graphical installer

**Install KDOS**, `kdos-installer-gui`, is a Kirigami window with ten steps down its side; a step
already answered can be returned to by clicking it, and a step ahead cannot:

| Step | Asks for |
|---|---|
| Welcome | The time zone, from `/usr/share/zoneinfo/zone1970.tab` with UTC first, narrowed by a typed filter (one match is chosen), with the local time there; beside it the processor, memory, firmware, how many of the disks found can take the install, and the install size |
| Keyboard | The XKB layout and variant, from `xkeyboard-config`'s `evdev.lst`, with a search field; a layout with no file under `xkb/symbols` (such as `custom`) is not listed. The console keymap is the one nearest the layout |
| Disk | Which disk, and how. Each disk that cannot take the install says why on its row, from the probe's `problem`; The page probes the disks again on every visit and, after a pause, whenever the swap or the box-images answer changes, since both move what a disk must hold; **Look Again** probes them at once, for a disk attached since. **Erase the disk and install KDOS** lays it out with `kinstall`, with **Two system slots** ticked and a line saying whether this disk holds two or how much they need; **Use partitions that already exist** offers an ESP (partitions typed as one or holding FAT, never the BIOS boot partition) and a root (never an ESP, the BIOS boot partition or an LVM physical volume; logical volumes too), with **Format it** for the ESP; **Partition Manager…** opens Partition Manager and comes back to the existing partitions with the disks probed again when it closes. Then the filesystem (ext4, btrfs, xfs or f2fs), the swap (none, a file, or a partition; the reuse plan offers none or a file) and its size |
| Encryption | LUKS2 under the system and its passphrase twice, with a strength hint. With the two system slots the whole volume group is inside the one container, so one passphrase at every boot opens all of it; the page says there is no recovery key before anyone types one |
| Account | Your account (full name, user name, password twice with a strength hint, administrator), or an OEM install; automatic login on an encrypted root; the hostname, and whether root is locked or its password. A user name of the image's own accounts is refused on the page |
| Look | The accent and the desk |
| Applications | From `kinstall --dump apps`, read once without the answers for the boxed preselection and then with them: under **KDOS applications**, the native groups as checkboxes, downloaded during the install, the language group the keyboard or time zone points at ticked and labelled "Ticked for the Japanese keyboard" (the page applies only what changed in that answer since its last dump, as the terminal wizard does); under **Boxed applications**, the catalogue's groups, recorded for the first login; and whether the live session's box images are kept |
| Summary | `kinstall --dump plan` for these answers, so what it shows is what runs (the native groups as **KDOS applications**), and answers the core would refuse (the answers check, then the target check) are refused here |
| Install | The steps and their progress, from the event stream |
| Done | How it ended, the warnings and **Restart Now**; on a failure `kinstall`'s reason, the end of the log when `kinstall` sent any, and **Back to the Summary**; **Open the Log** whenever `kinstall` ran and the session may read the log |

It is a front end and nothing more. The machine is described to it by `kinstall --dump
probe|look|apps|plan --json`, run as the session's user, `probe`, `apps` (after the first) and
`plan` with the answers so far on standard input; the probe reads what it may not open from udev's
database. The install is `pkexec /usr/bin/kinstall --config - --unattended --events json`, with
the answers, the passwords among them, written to `kinstall`'s standard input. Every program it
starts is an argument vector given to `QProcess`, never a shell.

**One window per session.** The program is unique on the session bus as
`io.kdos.kdos-installer-gui` (`KDBusService`); launched again, from the desktop icon after the
Welcome tour, it raises the window already open and exits, so two installers never run against one
disk.

**Keys.** Alt+Left and Alt+Right (and the Back and Forward keys) go back and on; Alt+Right does not
leave the Summary, whose next step is the install and is the **Install…** button's alone. **Quit**
closes the window. Return belongs to the field being typed in.

**Closing during an install** asks first. While `pkexec` still waits for permission nothing has
been written, and the dialog says so and offers **Close Anyway**, which ends `pkexec` and the
request with it; once `kinstall` has sent its first event, closing does not stop it, the disk is
still being written, and the log is `/var/log/kinstall.log`. The dialog closes by itself when the
install ends.

**How a failure is named.** The window checks that `/usr/bin/kinstall` exists and that `pkexec` is
installed before it starts the install, and says which is missing. After that, a `pkexec` that
ends before `kinstall` sent an event was refused permission: 126 when the authentication dialog was
dismissed, 127 when polkit did not authorise, which is what the KDE agent's Cancel gives. Either
reads "Permission to install was not given.", with no log named, since none was written. After the
first event a missing exit line is `kinstall` stopping unexpectedly.

`pkexec` runs `kinstall` under the polkit action `io.kdos.installer.run`, whose `exec.path` is
`/usr/bin/kinstall`, compiled into the program and never looked up on `PATH`, because `pkexec`
matches the path exactly. Its default is `auth_admin` for the active session and `no` otherwise. On
the live medium, `63_live-installer.sh` installs `49-kdos-installer-live.rules`, which grants the
action, and kpmcore's helper for Partition Manager, to the active local session with no password;
the copy leaves the rule out, so an installed machine asks for an administrator's password.

A password or passphrase that starts or ends with a space is refused on its page, because the
answer-file parser trims the ends of every value, and a line break cannot reach a value, so a
value can never become a second key.

## How it is built

### Dependencies

`kinstall` links `kinstall-core`, `libkbase`, `libktui` and `libkcolor`, compiled from source into
the binary, and no other library, not even a terminal library. `kinstall-core` itself uses only
`libkbase` and `libkcolor`, and it is compiled without `libktui` on its include path and archived
as `libkinstall-core.a` before the wizard links it, so a `ktui.h` include or a `libktui` call in
the backend fails the build. Password hashing uses `crypt()` from the C library, which musl
carries; a glibc host build, such as the one in `testing/selftest.sh`, adds `-lcrypt`. That is what
lets it be cross-compiled in the bootstrap phase of the build
(`script/phases/10_bootstrap/130_kinstall.sh`; see
[How KDOS is built](../05-developer/how-kdos-is-built.md)) and
exist on every tree from the first bootable image onward. `libkcolor` is on the list because the
accents, the boot menu's colours and the console palette the Theme step writes come from it.

Giving `kinstall`, `kinstall-core` or any of those libraries a new dependency means moving the
installer's build to a later phase.

A recipe (`src/system/kdos-installer/kpkgbuild`, built by `build.sh`) sits beside the sources so
a running KDOS can rebuild the installer natively; the [ports
catalogue](../06-reference/ports-catalogue.md#built-by-script) lists it with the
other packages the bootstrap phase builds by script. Both builds must compile every `.c` file in
`src/system/kdos-installer` and `src/system/kinstall-core` by glob, plus `kdos-appbox`'s
`catalogue.c`, so the bootstrap installer and the packaged one are always the same program. The
graphical installer is its own port, `kdos-installer-gui`, built with CMake in the desktop phase;
it does not depend on the installer's port, because the image's `kinstall` is the bootstrap
phase's.

The bootstrap step skips itself when its marker `build/mark/bootstrap/kinstall` exists, and
`--rebuild` does not reach it because it is not a port. Removing the marker, or naming the step
with `--steps 10_bootstrap:130_kinstall.sh`, runs it again; see [Known
gaps](../06-reference/known-gaps.md#the-images-installer-is-rebuilt-only-when-its-mark-is-gone).
`testing/preflight.sh` checks that the installer in `build/fs/usr/bin/kinstall` carries a string
`install.c` owns (the Limine path), and when it does not, says to remove the marker and rebuild the
bootstrap phase. The check proves only that the binary names that path: one built from an earlier
edit of the same sources passes it.

### The file split

| File | Owns |
|---|---|
| `kinstall-core/probe.c` | The `/sys` and superblock reader, the partition-table reader, volume-group activation and the logical-volume list, the catalogue and group reader, the native groups reader, the search for an exported application set, and the route decision |
| `kinstall-core/install.c` | The install steps, run in a child process, and the line protocol it reports on |
| `kinstall-core/desktop.c` | The files that make the machine a KDOS Plasma desktop: SDDM's autologin, the look in `/etc/xdg`, the login screen's set, and the keyboard |
| `kinstall-core/conf.c` | The defaults, the answer file, the answers check, the service list, the filesystem table and the slot sizing |
| `kinstall-core/dump.c` | `--dump probe`, `plan`, `apps` and `look` |
| `kinstall-core/events.c` | `--events json` |
| `kinstall-core/kinstall-core.h` | The shared types |
| `kdos-installer/pages.c` | The eleven wizard pages |
| `kdos-installer/main.c` | The header, sidebar and navigation, the event loop, the command line |
| `kdos-installer/kinstall.h` | The wizard's types and the version, `KI_VERSION` |

The superblock reader recognises LUKS, NTFS, FAT, xfs, f2fs, squashfs, ext2/3/4, btrfs, swap and LVM
physical volumes itself rather than parsing `blkid` or `lsblk` output, so the installer does not
inherit another program's output format or exit codes. An ESP is identified by its GPT type GUID,
not by looking like FAT.

The terminal handling, cell buffer, input layer, widgets, dialogs and palette belong to
[libktui](../05-developer/c-libraries.md#libktui).

## Design decisions

### Nothing is written before the summary

Every page fills in the configuration and does nothing else. The install is the single point of no
return, which is what makes **Back** safe to press on any page before it. The one exception, on a
real run, is the partition editor of the manual plan, `cfdisk` in the wizard or Partition Manager in
the graphical installer, which writes whatever partition table you save in it; the wizard's dry run
disables it, so a rehearsal writes nothing at all. The Keyboard page's `loadkeys` changes only the
live console.

### The whole interface is eight colours

A 512-glyph console font makes the Linux console use the foreground intensity bit as a ninth glyph
bit. The bright colours are then unreachable as foregrounds, and the bold attribute switches the
*font page* instead of the weight, so the installer never emits bold on a console.

Using eight colour slots means a console and a terminal emulator show the same picture. On a
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
nothing but its reporting function can write to that pipe. Each line starts with one letter, and
`--events json` writes each as one JSON line (see [The event stream](#the-event-stream)):

| Letter | Means |
|---|---|
| `S` | Step *n* has started |
| `K` | Step *n* is skipped |
| `P` | Progress within the current step, as a fraction, or `-1` for unknown |
| `N` | A note on the current step |
| `L` | A log line |
| `W` | A warning |
| `F` | Failed, with a message |
| `D` | Done |

A warning is written to `/var/log/kinstall.log` with a `warning:` prefix and kept, up to eight of
them, for the Install page's **WARNINGS** list and the Done page's **FINISHED WITH WARNINGS**; a
retried step that sends the same warning again adds no second entry. The child sends a `W` for a
missing `BOOTIA32.EFI`; on a UEFI machine, for a reused GPT disk with no BIOS boot partition, a
failed `limine bios-install` or a missing `limine` (on a legacy BIOS machine each of those is an
`F`); a missing ESP partition index; an ESP with no room for a second slot; and an application
import or build that fell back to pending.

A step starting closes every earlier step still marked running, not only the one before it. A
skipped step often sits between two real ones (no repartition, no second slot), and closing only the
predecessor would leave the real one spinning for the rest of the run. A child that exits without
reporting done or failed is shown as failed with its exit status.

There is no shell and no command string anywhere. Device paths and user names come from menus or
the answer file, and every command runs from an argument list.

### The sidebar does not take focus

The sidebar is drawn before the page. If its rows took ordinary focus positions, every control on
every page would move down the Tab order and the cursor would start on a decoration, where typing
does nothing. The sidebar's rows register as *chrome*, in a separate range that never joins the
Tab order.

### Fields that come and go

Some controls exist only while a box above them is ticked: the passphrase pair under *Encrypt the
root filesystem*, the swap size while there is swap, the root password while root is not locked.
A control's focus position is otherwise its place on the page, so ticking the box would renumber
every control after the group. The focus would land elsewhere, and the new passphrase field would
take the swap size field's cursor. Each such group is drawn in its own named id scope
(`ktui_id_push`), which keeps every control after it where it was. Tab still visits the group's
fields in the order they are drawn.

## See also

- [Installation](../02-user-guide/installation.md): using the installer, page by page
- [Boot and init](../03-architecture/boot-and-init.md): what it writes, and what boots it
- [The daemons](daemons.md): what `wheel` membership and the active session grant on the installed
  system, and `kdos-settingsd`, which keeps the files the installer writes for the clock and the
  keyboard
- [The security model](../03-architecture/security-model.md): who is asked for a password when, and
  the polkit action the graphical installer runs under
- [Packs and boxes](../03-architecture/packs-and-boxes.md#the-catalogue): the catalogue the applications page reads, and [what an install carries](../03-architecture/packs-and-boxes.md#what-an-install-carries)
- [The C libraries](../05-developer/c-libraries.md): the three it links
- [Testing](../05-developer/testing.md): the dumps and the disk-install harness
- [How KDOS is built](../05-developer/how-kdos-is-built.md): the build that produces the live system the installer copies
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#updates): the A/B update scheme the installer's layout prepares for

<!-- book-nav -->
---

*Part IV — Programs, chapter 27.* Previous: [26. The daemons](daemons.md) · [Contents](../README.md) · Next: [28. The kdos command](kdos-command.md)
