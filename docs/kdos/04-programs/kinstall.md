# kinstall

`kinstall` copies the running live system onto a disk and makes it bootable. It is a full-screen
wizard that works on a bare Linux console, inside a terminal emulator and over a serial line, and
it can also install unattended from an answer file. This chapter is the reference for the program:
its options and keys, what each page asks, what each install step writes, the answer-file keys, and
why the design is shaped the way it is. It is for administrators scripting installs and for
contributors changing the installer. If you only want to install KDOS, read
[Installation](../02-user-guide/installation.md) instead; it walks through the wizard with the
reasoning left out. The chapter assumes the vocabulary of [Boot and
init](../03-architecture/boot-and-init.md) (the ESP, the A/B root slots, the initramfs); the
[glossary](../06-reference/glossary.md) defines each term.

Three terms recur. The **ESP** is the EFI system partition, the FAT32 partition the firmware and
the bootloader read. The **point of no return** is the Summary page's install button: nothing is
written to any disk before it. A **plan** is how the chosen disk is used: `wipe` (erase and
repartition it), `reuse` (keep its partitions and format only the root), or `manual` (partition
it in `cfdisk` first, then reuse).

## How the installer works

The wizard's pages only fill in a configuration: the keymap, the time zone, the disk and plan, the
accounts, the services and the applications. None of them writes to a disk. The one exception is
the Layout page's `cfdisk`, which writes whatever partition table is saved in it. Everything else
waits for the Summary page's **BEGIN INSTALL** button, the point of no return; see [Nothing is
written before the summary](#nothing-is-written-before-the-summary).

The install itself is eleven steps run in a forked child process. The child reports to the wizard
over a pipe, one line per event, each line starting with a letter that says what it is: a step
started or skipped, progress, a note, a log line, a warning, a failure, or done. The wizard draws
the screen and writes `/var/log/kinstall.log` from those lines. See [The install runs in a child
process](#the-install-runs-in-a-child-process).

The wizard has no handler for the warning letter, `W`, and discards it. A warning therefore reaches
neither the screen nor the log. Several sections below name a condition that the child reports only
as a warning. For each of them the install carries on, and at most an indirect trace reaches the
log; see [The install runs in a child process](#the-install-runs-in-a-child-process).

## Starting the installer

On the live desktop the launcher entry **Install KDOS**
(`/etc/skel/.local/share/applications/kdos-install.desktop`) runs
`foot --title="Install KDOS" --app-id=kdos-install -- sudo kinstall`: the installer needs root,
and `sudo` needs a real terminal to prompt in. From a console, run it directly:

```sh
sudo kinstall
```

Before it takes over the screen the installer prints `kinstall: measuring the live system...` on
standard error and walks the whole root filesystem to measure how much it will copy. The walk
does not cross filesystem boundaries, so `/proc`, `/sys`, `/dev`, `/run` and `/tmp` are never
entered. It takes seconds on the live image and much longer on a development machine.

To keep the installer away from anything else on the console, give it a terminal of its own. A
virtual console device is owned by root, so the redirection has to be made by a root shell, not
by an ordinary user's:

```sh
sudo sh -c 'kinstall < /dev/tty3 > /dev/tty3 2>&1'
```

Then switch to that console with Ctrl+Alt+F3.

A full-screen program redraws the whole terminal constantly, so whatever terminal it is given, a
spare VT or a serial line, carries nothing else for the whole install. The disk-install harness,
`testing/install-to-disk.sh`, runs its unattended install on `/dev/tty3` this way; see
[Testing](../05-developer/testing.md).

## Synopsis

```
kinstall [options]
```

| Option | Does |
|---|---|
| `--config FILE` | Read answers from `FILE`. Options later on the command line still apply |
| `--save FILE` | Write the current answers (defaults plus any `--config`) to `FILE` and exit |
| `--unattended` | Skip the wizard and install from the answers |
| `--dry-run` | Log every command and execute none. The header shows **DRY RUN** |
| `--dump probe` | Print what the installer sees on this machine, and exit |
| `--dump plan` | Print the steps these answers would run, and exit |
| `--json` | Print `--dump` as JSON instead of text |
| `--theme NAME` | The accent: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect` or `paper`. The default is `bone`. An unknown name draws the installer in `bone` |
| `--no-mouse` | Keyboard only |
| `--ascii` | Draw boxes with `-`, `\|` and `+`, for terminals without line-drawing characters. It sets `KDOS_ASCII=1`, the lowest glyph tier of [libktui](../05-developer/c-libraries.md) |
| `--version` | Print `kinstall` followed by its version (`KI_VERSION` in `kinstall.h`) and exit |
| `--help`, `-h` | Print the usage and exit |

Options are read left to right. `--config` loads its file at the point it appears, so an option
after it overrides the file; `--save` and `--dump` act only after the whole command line is read.
An option that needs an argument and is last on the line counts as unknown.

The log of a run is `/var/log/kinstall.log`, opened for appending with mode `0600`. Each line
carries an `[HH:MM:SS]` stamp. On the Install page, `L` shows it in full (an unattended run only;
see [The pages](#the-pages)).

Only `--dump`, `--save`, `--help` and `--version` work without root. The wizard, `--dry-run`
included, stops on its first page with "kinstall must run as root — try: sudo kinstall" until it
runs as root. An `--unattended` run does not pass through that page; its first step, Prepare,
fails with "kinstall must run as root" instead.

| Exit status | Means |
|---|---|
| 0 | Finished: help, version, a dump, a saved answer file, a completed install, or the wizard quit |
| 1 | An unknown option, an unreadable `--config` file, a `--save` that could not be written, or an `--unattended` install that failed (with `reboot = 0`) |
| 2 | `--dump` given something other than `probe` or `plan` |

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
| L | The full log, on the Install page (an unattended run only; see [The pages](#the-pages)) |
| Ctrl+U | Clear a text field |
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

The wizard is eleven pages. Each has a fixed identifier, and an unattended install finds the
Install page by that identifier rather than by its position.

| # | Identifier | Title | Asks for |
|---|---|---|---|
| 1 | `welcome` | Welcome | Nothing; shows the processor, memory, storage, firmware and how much space the install needs |
| 2 | `keyboard` | Keyboard | The console keymap |
| 3 | `time` | Time | The time zone |
| 4 | `disk` | Disk | Which disk |
| 5 | `layout` | Layout | The plan, then the partitions, filesystem, swap, LVM and LUKS2 encryption |
| 6 | `accounts` | Accounts | Hostname, user, passwords, whether the user is an administrator, whether root is locked |
| 7 | `system` | System | Accent, the boxed-application container library, services |
| 8 | `apps` | Applications | Which application groups this machine builds |
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
- at least one disk found (the line reads "at least one writable disk");
- `rsync` present;
- `mkfs.ext4` and `mkfs.vfat` present;
- the Limine payload, `/usr/share/limine`, present.

The page refuses to continue without root or without a disk. On a machine that booted in legacy
BIOS mode it also says that the installed system will start, but that the firmware's own boot
entry cannot be created.

**Keyboard.** Lists every keymap under `/usr/share/keymaps` and `/usr/share/kbd/keymaps`, with a
filter field. Moving the selection applies the map at once with `loadkeys`, and a test field
shows what the keys produce. The choice is written to `/etc/keymap` on the target.

**Time.** Lists the zones in `/usr/share/zoneinfo/zone1970.tab` (`$KDOS_ZONEINFO` replaces the
directory, for testing), with a filter and a preview of the local time in the selected zone. The
hardware clock is left as it is. An image with no zone table offers `UTC` alone.

**Disk.** Lists every disk except loop, RAM, optical, zram, device-mapper, MD, floppy and NBD
devices, with its size, transport (`sata`, `nvme`, `usb`, `virtio` or `mmc`) and model, and draws
its current partition table. It preselects the first disk that is neither the boot medium nor
read-only. It refuses the medium KDOS is running from, a read-only disk, and a disk smaller than
the install size plus 768 MiB.

**Layout.** Offers the three plans:

- **Erase the whole disk and lay it out for KDOS** (`wipe`): a GPT table with a 512 MiB ESP, an
  optional swap partition and the root. The page offers the filesystem, swap (none, a file or a
  partition, with its size in MiB), **Put the root on LVM**, and **Encrypt the root filesystem
  (LUKS2)** with its passphrase typed twice. A bar and a table show exactly what will be written.
- **Use partitions that already exist** (`reuse`): two lists, the ESP and the root. The root list
  holds the disk's partitions and then every logical volume on the machine (see [A root on
  LVM](#a-root-on-lvm)). **Reformat the ESP as FAT32** is ticked by default; leave it off when
  another operating system boots from the same ESP. The root is always reformatted.
- **Partition it myself first (cfdisk)** (`manual`): an **Open cfdisk** button runs `cfdisk` on
  the chosen disk. When `cfdisk` exits, the installer re-reads the disks and switches to `reuse`.

The encryption checkbox is drawn on the erase plan only, but its value is not cleared when the plan
changes. A tick made on the erase plan and followed by a switch to `reuse` (directly, or through
`cfdisk`) encrypts the reused root, with no checkbox on the page to show it. The Summary mentions
LUKS2 only in the erase plan's LVM line, so on the reuse plan, or on the erase plan without LVM,
it does not show that encryption is on. Untick the box before leaving the erase plan if the reused
root is to stay unencrypted.

The page refuses an empty or mismatched passphrase, encryption on an image without `cryptsetup`,
LVM on an image without `lvm`, a swap size of zero, the `manual` plan before `cfdisk` has run, an
ESP and root that are the same partition, a root smaller than the install, a root that is an LVM
physical volume or that something holds, and a volume-group name clash (see [A root on
LVM](#a-root-on-lvm)).

**Accounts.** Hostname (letters, digits and `-`), full name, user name (lowercase letters,
digits, `-` and `_`, not starting with a digit), and the password twice, with a strength meter.
**Administrator** (membership of `wheel`) is ticked by default. **Lock the root account** is
ticked by default; unticked, a root password is asked for twice. The page refuses an empty
password, and it refuses a non-administrator while root is locked ("root is locked and <user> is
not an administrator — nobody could ever gain privileges").

**System.** The accent, as eight radio buttons with a swatch each; choosing one repaints the
installer in it. **Install the alien app library** (the live user's container storage, with its
size) is ticked by default. Then the five services of [the service table](#answer-files), each a
checkbox.

**Applications.** The application groups; see [The applications step](#the-applications-step).

**Summary.** Every answer on one screen, above a line reading "EVERYTHING ABOVE THIS LINE IS STILL
REVERSIBLE". **Next** is refused here ("press BEGIN INSTALL to start — Next does not"): the install
starts from the **BEGIN INSTALL** button (**REHEARSE INSTALL** under `--dry-run`) and only from
it. **Save answer file** writes the answers to `/tmp/kinstall.conf` and installs nothing.

**Install.** An overall progress bar, each step with its state and time, the elapsed time and the
tail of the log. When a step fails, four buttons appear: **Retry step** runs the install again
from the failed step, **View log** opens the full log, **Shell** runs a login `bash` on the
console, and **Abort** quits. When every step has finished, **Continue** appears.

**Done.** No run reaches this page: the Install page's Continue button moves to page index 9,
which is the Install page itself (see the next paragraph). The page would show the time taken, the
disk and the log path, a reminder to remove the boot medium, and three buttons: **Reboot now**
(after a confirmation), **Back to the live desktop** (quit), and **View log** (in `less`, or
`more` where `less` is missing).

The Summary's install button and the Install page's Continue button move to a page by its
position: to page index 8 and 9 respectively, counted from 0, which in the eleven-page table are
the Summary and the Install page themselves. An interactive install therefore never leaves the
Summary. It shows no progress and no failure, the **Retry step**, **View log**, **Shell** and
**Abort** buttons are never offered, and **BEGIN INSTALL** stays live while the child runs. Follow
an interactive install in `/var/log/kinstall.log` from another console. An unattended run opens
the Install page by identifier, so it shows progress and the failure buttons. Its **Continue**
button stays on the same page, and the run ends from there after five seconds (see [Fallbacks, and
how an unattended run ends](#fallbacks-and-how-an-unattended-run-ends)).

## The install steps

The install is eleven steps, run in order in a child process (see [The install runs in a child
process](#the-install-runs-in-a-child-process)). A step that does not apply is marked skipped
rather than run.

| # | Step | Does | Skipped when |
|---|---|---|---|
| 1 | Prepare | Checks the target and the tools, stops swap, takes down anything holding the disk | — |
| 2 | Partition | Writes the GPT layout: a 512 MiB ESP, an optional swap partition, then the root | The plan is not `wipe` |
| 3 | Format | Encrypts and creates the volume group if asked, then makes the filesystems | — |
| 4 | Mount | Mounts the root at `/mnt` and the ESP at `/mnt/boot/efi` | — |
| 5 | Copy system | Copies the live tree with `rsync` | — |
| 6 | Packs | Imports, builds or records the chosen applications | No application catalogue, or nothing chosen |
| 7 | Configure | `fstab`, hostname, time zone, keymap, autologin, services, swap file | — |
| 8 | Accounts | Users, passwords, groups | — |
| 9 | Theme | Runs `kdos theme <accent>` for the new home and `/etc/skel` | The accent is the default, `bone` |
| 10 | Bootloader | Limine on the ESP for UEFI and BIOS, kernel and initramfs, the menu, the NVRAM entry | — |
| 11 | Finish | Flushes and unmounts | — |

**Prepare** fails when no disk is chosen, the disk has gone, the installer is not root, or a tool
is missing: `mount`, `umount`, `mkfs.vfat`, `rsync`, the chosen filesystem's `mkfs`, `cryptsetup`
when encrypting, and `lvm` for a root on LVM. It then runs `swapoff -a`, unmounts everything under
`/mnt` and `/mnt` itself, and lazily unmounts every mount whose device name begins with the
disk's. On the `wipe` plan it takes down whatever holds the disk (see [A root on
LVM](#a-root-on-lvm)); on `reuse` it unmounts the chosen root by device number.

**Partition** runs `wipefs -a` on the disk, feeds `sfdisk --wipe always` the layout (ESP type
`U`, swap type `S`, root type `L`, or `V` for an LVM physical volume), then `partprobe` and
`udevadm settle`, and waits up to five seconds for the partition nodes to appear. Partition names
follow the kernel's rule: a disk whose name ends in a digit gets a `p` before the number
(`nvme0n1p1`), any other does not (`sda1`).

**Format** opens LUKS and builds the volume group when asked (see [An encrypted
root](#an-encrypted-root)), makes the root filesystem with the label `KDOS`, formats the ESP with
`mkfs.vfat -F 32 -n KDOS_EFI` when `format_esp` is set, and runs `mkswap` on a swap partition.

**Copy system** runs:

```sh
rsync -aHAX -x --numeric-ids --info=progress2 --no-inc-recursive <excludes> / /mnt/
```

The excludes are the contents of `/dev`, `/proc`, `/sys`, `/tmp`, `/run`, `/mnt` and `/media`;
`/lost+found`; the run's own log; and what must be unique to each machine,
`/var/lib/dbus/machine-id` and `/etc/ssh/ssh_host_*`, which the installed system generates on
its first boot. Without the boxed-application library (`alien_apps = 0`), the live user's
container storage under `/home/kdos/.local/share/containers` is left out as well. `rsync` exit
status 24 ("some files vanished") is accepted, since files come and go on a running live system.
The excluded directories are then created empty, `/tmp` with mode `01777`.

**Configure** writes the files listed in [What the rest of the tree
provides](#what-the-rest-of-the-tree-provides), copies the live `/etc/resolv.conf`, and makes the
swap file. The `fstab` lines it puts in front of the shipped file are:

```
UUID=<root>  /          <fs>   <options>            0 <pass>
UUID=<esp>   /boot/efi  vfat   defaults,umask=0077  0 2
UUID=<swap>  none       swap   defaults             0 0     # a swap partition
/swapfile    none       swap   defaults             0 0     # a swap file
```

**Accounts** hashes the passwords with SHA-512 `crypt()` and a random salt, renames the live
account when the user name changed, and rewrites `/etc/passwd`, `/etc/group`, `/etc/shadow`,
`/etc/subuid` and `/etc/subgid`. The root entry in `/etc/shadow` is always rewritten: locked
(`!`) or with the root password. The live image's root password never reaches the installed
system.

**Theme** runs the live system's `kdos theme <accent>` twice, with `HOME` pointed at
`/mnt/etc/skel` and at `/mnt/home/<user>`, rather than changing root into the target: the two trees
carry the same programs, and this way the generators need no `/proc`, `/dev` or bind mounts.
Without `kdos` on the `PATH` the step logs that and leaves the accent the image was seeded with.

**Bootloader** is described in [What the bootloader step writes](#what-the-bootloader-step-writes).

**Finish** runs `sync`, unmounts `/run/kdos-medium` when that directory exists (the Mount step
binds the medium there when the application archive is under `/mnt`; see [How the applications
arrive](#how-the-applications-arrive)), and unmounts everything under `/mnt`.

## Answer files

An answer file is plain `key = value` lines; `#` starts a comment line, and spaces around the key
and value are trimmed. `--save` writes one with every key except the three secrets, and
`--config` reads one. A key the installer does not recognise is ignored.

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
| `format_esp` | `1` or `0` | `1` | Format the ESP as FAT32. It applies on every plan; with `wipe` the new ESP has no filesystem until it is formatted, so leave it at `1` |
| `fstype` | `ext4`, `btrfs`, `xfs` or `f2fs` | `ext4` | An unknown name falls back to `ext4` |
| `swap` | `file`, `partition` or `none` | `file` | **Any other value means `none`**. A swap partition is made only on the `wipe` plan |
| `swap_mb` | Megabytes | `4096` | |
| `luks` | `1` or `0` | `0` | Encrypt the root with LUKS2. The wizard offers it only on the `wipe` plan; an answer file applies it to whichever root the plan names |
| `luks_passphrase` | Text | none | Never written by `--save` |
| `lvm` | `1` or `0` | `0` | Put the root on LVM, with the `wipe` plan |
| `hostname` | Text | `kdos` | |
| `username` | Text | `kdos` | The live account is renamed to this |
| `fullname` | Text | `KDOS User` | |
| `password` | Text | none | The user's password. Never written by `--save`. **See the warning below** |
| `root_password` | Text | none | Used only when `root_locked = 0`. Never written by `--save` |
| `root_locked` | `1` or `0` | `1` | A locked root has no password and cannot log in. With `root_locked = 0` and no `root_password`, root is locked anyway |
| `theme` | An accent name | `bone` | |
| `alien_apps` | `1` or `0` | `1` | Copy the boxed-application container library |
| `apps` | Space-separated group ids | `essential` | See [The applications step](#the-applications-step) |
| `autologin` | `yes` or `1` for on; anything else is off | `no` | See [autologin](#autologin) |
| `reboot` | `1` or `0` | `1` | Reboot when an unattended install ends |
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
turns the service back on. Other services (udev, D-Bus, seatd and the rest) are not choices and
cannot be turned off here.

Whether the user is an administrator has no key. An answer-file install always keeps the user
in `wheel`; only the Accounts page can make a non-administrator.

Passwords in an answer file are in plain text. `--save` never writes `password`,
`root_password` or `luks_passphrase`, and the file's header says so; add them yourself only if you
accept that credential sitting on the medium the file lives on.

**An answer file without `password` does not set the account's password.** The installer rewrites
the user's line in `/etc/shadow` only when it is given a password. With `username` left at `kdos`,
the installed account keeps the live image's well-known password, `kdos`. With a new user name,
`/etc/passwd` and `/etc/group` are renamed but the `/etc/shadow` line keeps the name `kdos`, so the
renamed account has no shadow entry and cannot log in with a password. The wizard's Accounts page
refuses an empty password; an unattended install does not ask. Always set `password` in an answer
file.

### autologin

`autologin` is the one key whose default differs from the live medium. The live medium ships
`autologin = kdos` in `/etc/kdos/login.conf`, because a machine with one account and no password
has nothing to ask. An answer file that leaves the key out installs a machine that asks for a
password, because a system somebody installed has a real account with a real password.

The Configure step edits the key's line in the target's `/etc/kdos/login.conf` rather than
replacing the file, because the file is mostly an explanation of what the key does:

- on writes `autologin = <username>`;
- off writes `#autologin = <username>`.

Off is a commented line, not an empty value. `kdos-login` hands `agetty --autologin` whatever
name follows the key, so an empty value would read as a setting and behave as none. A file with no
such line gains one only when autologin is on. The key is also the only place the desktop account
is named for tty1 (`/etc/inittab` runs `kdos-getty tty1 kdos-login tty1` and names no account),
so this line is what carries a renamed user to the login.

### Fallbacks, and how an unattended run ends

Three kinds of mistake fall back rather than fail, because each is read before the point of no
return, and refusing there would leave nothing installed over a spelling mistake:

- an unknown filesystem becomes `ext4`;
- an `apps` line that matches no group becomes `essential`;
- an unknown `plan` becomes `wipe`, and an unknown `swap` becomes `none`. Check these two with
  `--dump plan` before running unattended.

**An unattended run skips every check the wizard's pages make.** Nothing refuses the boot medium,
a read-only or too-small disk, a physical volume as the root, a volume-group clash, or a missing
passphrase; only the Prepare step's checks run. Read `--dump probe` and `--dump plan` first.

An unattended run ends by itself whichever way it went. When the install finishes or fails, the
last screen stays up for five seconds, then:

- with `reboot = 1` (the default) the machine reboots, after a failure as well as after success;
- with `reboot = 0` the installer exits, with status 1 if the install failed and 0 if it succeeded.

Set `reboot = 0` when a script needs the exit status. A failed run ends too because a run that
waited for success alone would sit on its error screen indefinitely, and that is the run somebody
most needs an answer from.

## Checking before installing: the dumps

```sh
kinstall --dump probe [--json]
kinstall --dump plan  [--json]
```

Neither needs a terminal, neither writes to a disk, and both print to standard output only.

- **`probe`** is the machine as the installer sees it: firmware (with its width when it is 32-bit,
  and Secure Boot), processor, memory, whether the session is live, the size of the system it
  would copy and of the container library within it, the disks and their partitions (with
  filesystem, label, and whether each is an ESP, mounted, removable, read-only or the boot
  medium), and the logical volumes. It is what to paste into a bug report. Run as root it first
  activates every LVM volume group, as the wizard does, so the logical volumes it lists are the
  ones the reuse plan would offer; run as an ordinary user it lists only those already active. It
  measures the live system by walking the root filesystem, so the test suite does not run it.
- **`plan`** runs the same planner the wizard does, so the steps and their skips are the real
  ones. It also prints the `mkfs` command and the root's `fstab` line that the filesystem choice
  becomes, the services turned off, and for the applications the selection, the route and an
  estimated size. It probes neither the disks nor the live system, so its LVM line reports the
  volume as taking all of the group whatever the disk's size.

The JSON form is a rendering of the same data in the same pass, not a second walk. **No password
appears in either form**: the plan says whether root is locked and whether the root is encrypted,
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
walks every step, logs each command with "(dry run: not executed)", and writes no file on any
disk; `/var/log/kinstall.log` is still appended to.

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
- **The swap file is made differently per filesystem.** `fallocate` leaves unwritten extents,
  which xfs and f2fs refuse to swap on, and btrfs needs a file that is not copy-on-write and not
  compressed. The wrong method installs cleanly and then boots with no swap and nothing saying
  why. Where `fallocate` or `btrfs` is missing, the step falls back to `dd`.
- **f2fs is a kernel module**, so it is in the initramfs module list in
  `script/06_packaging/01_initramfs.sh`. ext4, btrfs and xfs are built in: `kdos.config` in the
  `linux` port sets `CONFIG_XFS_FS=m`, and its `build.sh` then enables it as built-in. The list
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

- The ESP and any swap partition stay plain partitions.
- The root partition is typed Linux LVM and becomes the only physical volume of a volume group
  named `kdos`.
- The root is the logical volume `root_a`.
- With encryption on, LUKS is opened on the partition and the physical volume is inside it, so one
  passphrase opens the whole group.
- The commands are `lvm pvcreate`, `lvm vgcreate` and `lvm lvcreate`, not the short names, which an
  image need not carry.

### Volume names and sizes

The volumes are named for the A/B root slots. `root_a` is slot A's root, and slot B's goes
beside it as `root_b`, so adding a second slot needs no repartitioning. `root_a` takes half the
group (`-l 50%VG`) when half still holds the system, the swap file (when swap is a file) and 256
MiB to spare, after 32 MiB is set aside for the LUKS header and LVM metadata; otherwise it takes
the whole group (`-l 100%FREE`). The swap file counts because it is written onto the same root
after the copy. `ki_lvm_half()` makes that decision, and the Layout page, the Summary and the Format
step all call it, so the size shown is the size created. The other half is left unallocated: the
installer makes no `root_b`, because an empty volume is not a slot anything can boot or update.

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
a retried Format meets nothing a failed one left open. It then wipes the old physical-volume label
from the new partition, because the erase writes the same layout at the same offsets and `pvcreate`
would find the previous install's label there.

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

A physical volume, and any partition or volume that something holds, is refused as the root. `mkfs`
would fail on a held device after the point of no return, and on a physical volume it did not fail
on it would destroy a volume group.

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
first and looks second. The container's UUID is also recorded as `crypt_a` in the boot state (see
[What the bootloader step writes](#what-the-bootloader-step-writes)). The passphrase is asked for at
every boot; there is no recovery key.

## The applications step

Applications are not on the installation medium. Each one is built by the container engine on the
installed machine from the shipped catalogue. The Applications page therefore chooses what to
build, not what to copy.

The installer reads the catalogue from the first of these that exists:

1. the file `$KDOS_CATALOGUE` names, when it is set (the test suite points it at a fixture);
2. `/usr/share/kdos/appstore/catalogue`, the live system's own copy;
3. `/mnt/iso/appstore/catalogue`, on the boot medium.

With no catalogue, the page says that nothing can be chosen and the Packs step is skipped.

**It lists groups, not applications.** Seven named bundles is something to read during an
install; the shipped catalogue's 180 applications is not. The catalogue
(`src/packages/kdos-appbox/catalogue`) defines these groups:

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

In an answer file, `apps` names group ids. An application id such as `app.krita` matches no group
and is ignored, and if nothing on the line matches, the selection is `essential`. With no `apps`
key the selection is `essential`. The page's selection logic runs before planning on every path
that plans without walking the wizard (`--dump plan` and `--unattended`), because the selection is
what the plan is about.

The catalogue is read by compiling `kdos-appbox`'s `catalogue.c` into the installer rather than by
running `kdos-appbox`. That file uses only `libkbase`, so it costs no extra library, and a live
installer cannot assume anything is on the target's `$PATH`.

### How the applications arrive

The installer picks one route, and the Applications page, the Summary and `--dump plan` show which
before anything is written, so nobody discovers at first boot that nothing was installed.

| Route | Chosen when | What happens |
|---|---|---|
| **import** | An exported set (a `.ktar` file) is on a mounted device | `kdos-appbox import <file>`. `kdos-packd` verifies each pack where it mounts it. Offline, and the only route on a machine with no network. It imports the archive's whole selection (its own `SELECTION` file), whichever groups were ticked |
| **network** | There is a default route | `kdos-appbox install <group>…` builds the ticked groups during the install. This can take a long time |
| **pending** | Neither | The group ids are written to `/var/lib/kdos/apps-pending` on the installed system; the first session offers them, and `kdos app install --pending` builds them |

**The import and network routes run on the live system, not inside the target.** The Packs step
comes after the copy and runs `kdos-appbox` as the live system's own program, with no change of
root: it asks the live `kdos-packd` on `/run/kdos-packd.sock`, and what it imports or builds goes
into the live system's store, not onto the installed disk. The installed system receives the empty
store directories (below) and, when the step falls back, the pending list. The **pending** route is
therefore the one that reliably carries a selection to the installed machine.

The archive is searched for each time the Applications page is entered, and once for `--dump plan`
and for `--unattended`: the first `*.ktar` directly inside any directory one level under `/mnt`,
`/media` or `/run/media` wins, and only that one is offered. The search goes no deeper than
`<root>/<entry>/*.ktar`. `kdos-mountd` mounts a stick at `/media/<user>/<label>`, two levels
under `/media`, so an archive at the top of a stick the desktop mounted is not found; mount the
stick by hand at `/mnt/<name>` (or anywhere one level under the three roots) before entering the
page. See [The daemons](daemons.md) for `kdos-mountd`.

The network test reads `/proc/net/route` for a default gateway and contacts no host: deciding
which route to offer must not send traffic to a third party, and a default route is enough to
attempt a build.

An import or a build that fails falls back to **pending** rather than failing the install. The
warning that says why is discarded (see [How the installer works](#how-the-installer-works)); the
only trace on the screen's log tail and in `/var/log/kinstall.log` is the line `<n> group(s)
recorded for the first login` where `imported from <file>` or `<n> group(s) built` would have
been. By then the system is installed and bootable, and abandoning it over an archive that would
not unpack would leave a machine with no operating system.

When the archive is under `/mnt`, the Mount step bind-mounts `/mnt` itself to `/run/kdos-medium`
before mounting the target there, and reads the archive as `/run/kdos-medium/<path below /mnt>`.
The target mounts at `/mnt`, and a stick mounted at `/mnt/<something>` would be hidden underneath
it, so the original path would lead into the empty filesystem the Format step made. It is a bind
of the directory, not a second mount of the device, because the device was mounted by something
else and its path is the only handle the installer has on it. If the bind fails, the log says so
and the archive is dropped. An archive under `/media` or `/run/media` needs no bind, since
mounting `/mnt` does not cover those.

Whichever route runs, the store's directories are created on the target:
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
| `EFI/kdos/font.bin` | The menu typeface, from `/boot/limine/font.bin` or `/mnt/iso/boot/limine/font.bin`, when present |
| `EFI/kdos/wallpaper.png` | `/usr/share/kdos/boot/kdos-backdrop.png`, else `kdos-banner.png`, when present |
| `EFI/kdos/memtest.efi` | `/usr/share/kdos/memtest86plus/memtest.efi`, when present |

`limine.conf` sits at the root of the ESP because that is the one path Limine searches on both
firmwares. It sets `timeout: 10` and `default_entry: 1`, takes its colours from the chosen accent
through the same `libkcolor` function that writes the medium's own menu, and has three entries,
each booting slot A's kernel with `kdos_slot=a`:

| Entry | Command line after the common part |
|---|---|
| `KDOS` | `rw console=tty0 quiet loglevel=3` |
| `KDOS (verbose)` | `rw console=tty0 loglevel=7` |
| `KDOS (single user)` | `rw console=tty0 loglevel=7 single` |

The common part is `kdos_slot=a bootstate=UUID=<ESP> [cryptdevice=UUID=<container>:kdosroot]
root=UUID=<root>`. When the memtest86+ payload is present a fourth entry, **Memory Test
(memtest86+)**, boots it on UEFI only. The ten-second countdown matches the medium's: the menu is
the only way to reach the verbose entry and the memory test, which a machine that will not boot
needs. Any key cancels the countdown. The `KDOS (single user)` entry passes `single`, which nothing
acts on, so it boots like `KDOS (verbose)`; see
[There is no single-user mode](../06-reference/known-gaps.md#there-is-no-single-user-mode).

The boot state file records the machine as slot A with nothing to roll back to:

```ini
slot_a   = <root filesystem UUID>
slot_b   =
crypt_a  = <LUKS container UUID, or empty>
crypt_b  =
active   = a
try      =
attempts = 0
```

It is written directly rather than through `kdos-bootctl`, because the tool's default path is the
running system's ESP and this runs against a target at `/mnt`. `kdos-bootctl` maintains the file
and regenerates the `KDOS` entries from then on; see [A new
kernel](../03-architecture/boot-and-init.md#a-new-kernel).

### Room for a second slot

The kernel and initramfs go into slot A's directory, `EFI/kdos/a/`, and later kernels reach the
ESP through `kdos-bootctl deploy`. Every A/B update needs room for a second slot's kernel and
initramfs beside the first, so the installer checks that the ESP has that much free plus 1 MiB. The
512 MiB ESP the erase plan makes has plenty; a reused 100 MiB one may not. The free space and the
need are logged, but the warning when the room is short is discarded, so check the ESP's free space
yourself when reusing one.

### Both firmwares

Both UEFI loaders go onto the ESP, so a disk written on one machine starts on another. A 64-bit
processor does not imply 64-bit firmware, and firmware only reads the `BOOT<arch>.EFI` it can run.
`BOOTIA32.EFI` is copied where Limine provides it and skipped where it does not (a Limine built
without `--enable-uefi-ia32` has none), with nothing on the screen or in the log to say so; failing
the install over a fallback for firmware this machine does not have would throw away a working
system.

The BIOS boot code is written whatever firmware the install ran on: `limine-bios.sys` goes to the
root of the ESP and `limine bios-install <disk>` writes the boot sector, so a disk installed on a
UEFI machine and moved to a legacy BIOS one still starts. If it fails, or `limine` is not on the
`PATH`, the install carries on, because the UEFI path is already complete, and nothing on the
screen or in the log says so.

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
cannot be read no entry is created, and nothing says so; the removable-media fallback still starts
the disk, whereas a guessed index would be a boot option the firmware cannot load.

### No ownership-preserving copies

Nothing is copied onto the ESP with an ownership-preserving copy. The ESP is FAT, which has no
ownership: such a copy tries to change the owner of every file, the kernel refuses each one, and the
copy fails with a page of errors naming the *source* paths, which looks like a problem with the
boot loader rather than a filesystem that cannot store what was asked of it.

## What the rest of the tree provides

The installed system depends on these, and each exists because of the installer:

- `kdos-getty` loads the keymap the installer writes to `/etc/keymap`, with `loadkeys`.
- `rcS` turns swap on after mounting (`swapon -a`), because `mount -a` ignores swap lines.
  Without it the swap choice would do nothing.
- `fstab` is added to, never replaced. The shipped file carries the `tmpfs` line for `/tmp`
  with `mode=1777`, which every graphical application depends on; the installer's lines go in
  front of it.
- Renaming the user rewrites `/etc/passwd` (the name, the full name and the home directory),
  `/etc/group` (the membership lists and the primary group's own name), `/etc/shadow` (when a
  password is given), the home directory itself, the owner of the `/etc/subuid` and `/etc/subgid`
  ranges, and `login.conf`'s `autologin`. The subordinate ranges are looked up by name: a range
  left on `kdos` would give the renamed account no mapping, and no rootless box would start.
- Administrator means membership of `wheel` and nothing else. The live image ships the account
  in `wheel`. Everything that grants administrator rights keys on that membership: the sudo rule
  `%wheel ALL=(ALL) ALL`, the polkit admin rules, `kdos-resctl`, `kdos-packd`, `kdos-energyd`, and
  `kdos-powerd`'s configuration verbs (firewall, autologin, accent, timezone). So a
  non-administrator is taken out of `wheel`, and no separate sudoers file is written. Every other
  group is kept, `seat` among them. `seat` is what a non-administrator's desktop runs on: seatd
  hands it the display, and `kdos-powerd`'s suspend, power-off and reboot, `kdos-mountd` and
  `kdos-oomd` all admit it. A non-administrator therefore keeps the lid, the power keys, the
  panel's power items and USB-stick mounting, and loses sudo, the polkit admin actions and
  everything listed above; see [The daemons](daemons.md#at-a-glance).
- The hostname replaces both `/etc/hostname` and the `127.0.1.1` line of `/etc/hosts`. musl
  resolves the machine's own name from that file before asking DNS, and the shipped line names
  `kdos`.
- The time zone writes `/etc/profile.d/20-timezone.sh`, links `/etc/localtime`, and writes the
  Wi-Fi country from `zone.tab` to `/etc/modprobe.d/kdos-regdom.conf` as
  `options cfg80211 ieee80211_regdom=<CC>`, as `kdos-powerd`'s `timezone` verb does later. A zone
  the target does not carry is logged and leaves the machine on UTC; a zone with no country (UTC)
  writes no regulatory file.

## How it is built

### Dependencies

`kinstall` links `libkbase`, `libktui` and `libkcolor`, compiled from source into the binary, and
no other library, not even a terminal library. Password hashing uses `crypt()` from the C library,
which musl carries; a glibc host build, such as the one in `testing/selftest.sh`, adds `-lcrypt`.
That is what lets it be cross-compiled in phase 1 of the build (`script/01_phase1/13_kinstall.sh`;
see [Phase 1: a minimal KDOS](../05-developer/how-kdos-is-built.md#phase-1-a-minimal-kdos)) and
exist on every tree from the first bootable image onward. `libkcolor` is on the list because
`libktui`'s theme code includes its palette header; the colour values live there and nowhere
else.

Giving `kinstall`, or any of those three libraries, a new dependency means moving the installer's
build to a later phase.

A recipe (`src/packages/kdos-installer/kpkgbuild`, built by `build.sh`) sits beside the sources so
a running KDOS can rebuild the installer natively; the [ports
catalogue](../06-reference/ports-catalogue.md#phases-0-and-1-built-by-script) lists it with the
other packages phase 1 builds by script. Both builds compile every `.c` file in the
directory by glob, plus `kdos-appbox`'s `catalogue.c`, so the phase-1 installer and the packaged
one are always the same program.

The phase-1 step skips itself when its marker `build/mark/phase1/kinstall` exists, and
`--rebuild` does not reach it because it is not a port. `testing/preflight.sh` checks that the
installer in `build/fs/usr/bin/kinstall` carries a string `install.c` owns (the Limine path), and
when it does not, says to remove the marker and rebuild phase 1.

### The file split

| File | Owns |
|---|---|
| `probe.c` | The `/sys` and superblock reader, the partition-table reader, volume-group activation and the logical-volume list, the catalogue and group reader, the search for an exported application set, and the route decision |
| `pages.c` | The eleven wizard pages |
| `install.c` | The install steps, run in a child process, and the line protocol it reports on |
| `conf.c` | The defaults, the answer file, the service list, the filesystem table and the LVM sizing |
| `dump.c` | `--dump probe` and `--dump plan` |
| `main.c` | The header, sidebar and navigation, the event loop, the command line |
| `kinstall.h` | The shared types and the version, `KI_VERSION` |

The superblock reader recognises LUKS, NTFS, FAT, xfs, squashfs, ext2/3/4, btrfs, swap and LVM
physical volumes itself rather than parsing `blkid` or `lsblk` output, so the installer does not
inherit another program's output format or exit codes. An ESP is identified by its GPT type GUID,
not by looking like FAT.

The terminal handling, cell buffer, input layer, widgets, dialogs and palette belong to
[libktui](../05-developer/c-libraries.md).

## Design decisions

### Nothing is written before the summary

Every page fills in the configuration and does nothing else. The install is the single point of no
return, which is what makes **Back** safe to press on any page before it. The one exception is
`cfdisk` on the Layout page, which writes whatever partition table you save in it; the Keyboard
page's `loadkeys` changes only the live console.

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
nothing but its reporting function can write to that pipe. Each line starts with one letter:

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

The parent acts on every letter except `W`: it has no handler for warnings and discards them, so
they reach neither the screen nor `/var/log/kinstall.log`. The child sends a `W` for a missing
`BOOTIA32.EFI`, a failed `limine bios-install` or a missing `limine`, a missing ESP partition
index, an ESP with no room for a second slot, and an application import or build that fell back
to pending. The ESP check logs the free space and the need separately, and the fallback shows as
the pending route's log line, `<n> group(s) recorded for the first login`; the others leave no
record of what went wrong.

A step starting closes every earlier step still marked running, not only the one before it. A
skipped step often sits between two real ones (no repartition, no theme), and closing only the
predecessor would leave the real one spinning for the rest of the run. A child that exits without
reporting done or failed is shown as failed with its exit status.

There is no shell and no command string anywhere. Device paths and user names come from menus or
the answer file, and every command runs from an argument list.

### The sidebar does not take focus

The sidebar is drawn before the page. If its rows took ordinary focus positions, every control on
every page would move down the Tab order and the cursor would start on a decoration, where typing
does nothing. The sidebar's rows register as *chrome*, in a separate range that never joins the
Tab order.

## See also

- [Installation](../02-user-guide/installation.md): using the installer, page by page
- [Boot and init](../03-architecture/boot-and-init.md): what it writes, and what boots it
- [The daemons](daemons.md): what `wheel` and `seat` membership grant on the installed system
- [Packs and boxes](../03-architecture/packs-and-boxes.md): the catalogue the applications page reads
- [The C libraries](../05-developer/c-libraries.md): the three it links
- [Testing](../05-developer/testing.md): the dumps and the disk-install harness
- [How KDOS is built](../05-developer/how-kdos-is-built.md): the build that produces the live system the installer copies
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#updates): the A/B update scheme the installer's layout prepares for

<!-- book-nav -->
---

*Part IV — Programs, chapter 27.* Previous: [26. The daemons](daemons.md) · [Contents](../README.md) · Next: [28. The kdos command](kdos-command.md)
