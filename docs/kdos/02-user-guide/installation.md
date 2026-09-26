# Installation

This page is for anyone putting KDOS onto a disk. `kinstall` is the installer: it copies the live
system onto a disk and makes it bootable. It ships on every image, runs on a bare text console or
over a serial line, and can install a machine unattended from an answer file.

Read [Before you start](#before-you-start) first, then follow [The pages](#the-pages) as the wizard
shows them. The later sections are reference: the filesystem choices, encryption, the A/B root
slots, answer files for unattended installs, and a list of every file the installer writes. For how
the installer is built inside, see [kinstall](../04-programs/kinstall.md).

## Before you start

Nothing reaches your disk until you press `BEGIN INSTALL` on the Summary page, with one exception:
the *Partition it myself* plan. Its `Open cfdisk` button runs `cfdisk` on the target disk, and
whatever table you write in `cfdisk` is on the disk the moment you quit it. Every other page before
the Summary only records an answer, so `Back` really does go back, and quitting leaves the machine
exactly as you found it. Pressing `Next` on the Summary page does not start the install — it tells you to use
the button — so a stray keystroke can never erase a disk.

Two answers are worth settling before you begin, because changing either one later means installing
again:

- **The root filesystem** — ext4, btrfs, xfs or f2fs. See [The root filesystem](#the-root-filesystem).
- **Whether the root is encrypted.** Encryption is applied when the partition is created, so it
  cannot be added to or removed from an existing install. See
  [Encrypting the root](#encrypting-the-root).

You need:

- root (run it with `sudo`);
- a target disk that is not the medium you booted from;
- a disk large enough for the system with about 768 MB to spare.

The wizard checks all three and says which one failed.

The installer can also take care of applications, but read [8. Applications](#8-applications)
before relying on it: of its three routes, only the one that records your choice for the first
login reaches the installed machine.

## Running the installer

From a terminal on the live system:

```sh
sudo kinstall
```

If something else already owns your terminal — you are on a serial line, or the desktop is running
— give the installer a text console of its own:

```sh
sudo kinstall < /dev/tty3 > /dev/tty3
```

The interface is full-screen and works with both keyboard and mouse. On a bare text console it
reads the mouse straight from the input devices, so clicking works on `tty1` with no extra
software. It needs a terminal of at least 62x18 characters.

| Key | Action |
|---|---|
| `Tab` / `Shift+Tab` | Move between controls |
| `↑` `↓` `←` `→` | Move inside a list or a field |
| `Enter` / `Space` | Activate, toggle, choose |
| `Alt+←` / `Alt+→` | Previous and next page |
| `Esc` | Back a page |
| `PgUp` / `PgDn`, mouse wheel | Scroll a page taller than the screen |
| `Ctrl+U` | Clear the field you are in |
| `F1` | The key card |
| `L` | The full log, on the Install page |
| `Ctrl+Q`, `Ctrl+C` | Quit. It always asks first, and during an install it warns that the target is left half written |

### Command-line options

| Option | Effect |
|---|---|
| `--config FILE` | Read answers from `FILE`. See [Installing unattended](#installing-unattended) |
| `--save FILE` | Write the current answers to `FILE` and exit without installing |
| `--unattended` | Skip the wizard and install straight from `--config` |
| `--dry-run` | Run the wizard and the install, logging every command and executing none |
| `--dump probe` | Print the machine as the installer sees it, and exit |
| `--dump plan` | Print the steps these answers would run, and exit |
| `--json` | Print `--dump` as JSON |
| `--theme NAME` | Draw the installer in this accent, and preselect it for the installed system: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect` or `paper` |
| `--no-mouse` | Keyboard only |
| `--ascii` | Draw boxes with `-`, `|` and `+`, for terminals that cannot show line-drawing characters |
| `--version`, `--help` | Print and exit |

The log of every run is `/var/log/kinstall.log`.

## The pages

There are eleven pages, always in this order. The sidebar shows where you are and which pages are
behind you; click a finished page to go back to it. Pages ahead cannot be clicked, because nothing
has checked them yet. On the Applications page the `Next` button reads `Review`, to mark the end of
the questions.

### 1. Welcome

What the installer measured about this machine: the processor and thread count, memory, how many
disks are visible, how much the install will copy, and the firmware. Under that is a preflight
list:

- running as root;
- at least one writable disk;
- `rsync` present;
- `mkfs.ext4` and `mkfs.vfat` present;
- the Limine bootloader files present.

The first two stop you on this page; the other three only warn. The install's first step checks
`rsync`, `mkfs.vfat`, `mount`, `umount` and the chosen filesystem's `mkfs` again and refuses before
anything is written. It does not check the Limine files again: if they are missing, the install
writes the disk and then fails at the Bootloader step, leaving a machine that cannot boot. Do not
start an install while that line is marked.

The firmware line is information, never a refusal. Limine is written for both firmware types
whichever way this machine started, so the line only tells you which path the firmware will use:

- `legacy BIOS` — this machine booted that way. It gets the same install, minus the firmware boot
  entry, which cannot be created in legacy mode.
- `32-bit firmware` — a 64-bit processor with 32-bit UEFI firmware, as on some early Atom tablets.
  The firmware boot entry then names the 32-bit EFI loader.
- `Secure Boot enabled` — the install works, but the installed machine will not start until you
  turn Secure Boot off in firmware setup. Limine's EFI loader is unsigned and KDOS enrols no keys.

### 2. Keyboard

The console keymap, with a filter box because the list is long. The keymap is applied as you move
through the list, so you can try it in the box below; on an image without `loadkeys` the preview is
skipped and the choice is still saved. It is written to `/etc/keymap`, which `kdos-getty` loads on
every text console.

The desktop uses it too. When the session starts it translates the console keymap name into the
matching XKB layout, because the console and XKB name the same layouts differently. A keymap whose
XKB layout is not on the image is reported rather than silently replaced by US QWERTY.

### 3. Time

Choose a time zone by name. The list is tzdata's `zone1970.tab`, with a filter box, and the page
shows the current local time in the zone under the cursor.

The install writes the zone in two places that always agree: `/etc/localtime` becomes a link to
the zone file, and `/etc/profile.d/20-timezone.sh` exports `TZ=':/etc/localtime'` — the leading
colon makes the C library read that same file. The hardware clock is left as it is.

If the installed system has no zone file for the name you picked, the install log says so and the
machine keeps UTC.

The zone also sets the Wi-Fi country. The installer looks up the zone's country in `zone.tab` and
writes it to `/etc/modprobe.d/kdos-regdom.conf` as the wireless regulatory domain. Without it the
radio stays in the restrictive "world" domain, with the 5 GHz channels that need radar detection
closed and transmit power capped. A zone with no country, such as `UTC`, writes nothing.

### 4. Disk

Which disk, and nothing else. Each row shows the size, the connection type and the model, and the
disk's current partitions are drawn as a proportional bar so you can see what is about to go. Below
that are the disk's capacity and what this install needs.

The medium you booted from is marked and refused, because installing onto it would pull the system
out from under the installer. A read-only disk and a disk too small for the install are refused as
well.

### 5. Layout

How the chosen disk is used: the plan, the root filesystem, swap, and encryption.

| Plan | What happens |
|---|---|
| Erase the whole disk | A new GPT partition table: a 512 MiB EFI System Partition (ESP), a swap partition if you asked for one, then the root |
| Use partitions that already exist | You choose the ESP and the root; every other partition on the disk is left alone and the Partition step is skipped. The root can be a partition or an existing LVM logical volume, and whichever you choose is always reformatted |
| Partition it myself | `Open cfdisk` hands the screen to `cfdisk`. Create an EFI System partition and a Linux partition, write the table and quit, then assign them on the reuse plan |

**Swap** is a file on the root filesystem, a partition of its own, or none. The size starts at
4096 MiB, and the page shows how much RAM the machine has.

**On the reuse plan** there is a checkbox to reformat the ESP as FAT32. Leave it off when another
operating system boots from the same ESP. The root list shows the disk's partitions, then every LVM
logical volume on the machine, named `vg/lv`. The installer activates every volume group when it
starts, so volumes on this disk appear. Thin and cached volumes are offered and boot like any
other; their internal volumes are not listed. The page refuses as the root:

- an LVM physical volume — choose one of its logical volumes instead;
- a partition or volume that something else holds open;
- a partition smaller than the install.

A software RAID array and an opened LUKS container are not offered as the root.

**On the erase plan**, `Put the root on LVM` makes the root partition the single physical volume of
a volume group named `kdos`, with the root on its logical volume `root_a`. The ESP and a swap
partition stay plain partitions. `root_a` is slot A's root and takes half the group, leaving the
other half free for a second root slot — see [A/B root slots](#ab-root-slots). On a disk where half
would not hold the install and its swap file, `root_a` takes all of it, and the page and the
Summary say so. With encryption on as well, the volume group sits inside the encrypted container,
so one passphrase opens both slots. If a volume group named `kdos` already exists with any part of
it on another disk, the page refuses, because creating the new group would fail after this disk
had been erased; rename the old one with `vgrename` or leave LVM off.

**Encryption** is offered on the erase plan only, because encrypting a partition destroys what is
on it and the reuse plan exists for people who are keeping something. Tick it and the passphrase
fields appear.

### 6. Accounts

- **Hostname** — letters, digits and dashes.
- **Full name and user name** — the user name is lowercase letters, digits, `-` and `_`, and does not
  start with a digit.
- **Password** — required, typed twice.
- **Administrator** — ticked, the account is in the `wheel` group and can use `sudo` and the
  desktop's administrative actions. Unticked, it is left out of `wheel` and can still suspend,
  power off, reboot and mount its own removable media.
- **Lock the root account** — ticked by default. Unticked, you set a root password.

Root locked together with a non-administrator account would leave nobody able to gain privileges,
so `Next` refuses that combination.

The installed machine asks for your password at `tty1`. Only the live image logs in automatically.
The wizard has no automatic-login option: set `autologin = yes` in an answer file (see
[Installing unattended](#installing-unattended)), or after installing, set
`autologin = <username>` in `/etc/kdos/login.conf`. The live image's root password is never carried
over.

A user name other than `kdos` is a real rename. The installer rewrites `/etc/passwd`, `/etc/group`
(the membership lists and the primary group's name), `/etc/shadow` and the owner of the
`/etc/subuid` and `/etc/subgid` ranges that rootless containers use, moves the home directory, and
writes the new name into the `autologin` line of `/etc/kdos/login.conf` — the only place the desktop
account is named.

### 7. System

Three things: the accent, the live container store, and which services start at boot.

**The accent.** There are eight, and the installer repaints itself in the one under the cursor, so
you choose by colour rather than by name. It is applied to the desktop, GTK, the icons, the cursors,
foot and btop. See [Theming](theming.md).

**Install the alien app library.** An *alien app* is a graphical application that KDOS does not
compile itself and runs in a container instead — see
[Applications](applications.md#what-an-alien-app-is). This checkbox, shown with its size, decides whether the live
account's container store — `/home/kdos/.local/share/containers`, the images Podman keeps for
containerised applications — is copied to the disk. The medium ships no prebuilt applications, so
this holds only what the live session has itself pulled or built, often nothing. Leaving it out
never stops you installing applications later.

**Services.** Five services are a choice rather than part of the system:

| Service | Default |
|---|---|
| NetworkManager (wired, Wi-Fi and VPN) | On |
| Bluetooth | On |
| ALSA state (restore mixer levels at boot) | On |
| CUPS (printing) | Off |
| OpenSSH server | Off |

A service you turn off becomes a flag file in `/etc/service.disabled/` on the installed machine, so
any of them can be changed later with `service enable` or `service disable`.

### 8. Applications

The application **groups** this medium's catalogue can build, with `essential` already ticked:

| Group | What it holds |
|---|---|
| `essential` | Firefox ESR, LibreOffice, GIMP, Zathura and KeePassXC |
| `office` | Documents, spreadsheets and reading |
| `creative` | Images, audio and video |
| `dev` | Programming and electronics |
| `science` | Computation, modelling and data |
| `make` | 3D modelling, slicing and CAD |
| `games` | Games and emulators |

`Space`, `Enter` or a click toggles a group. The size total updates as you go and is labelled an
estimate: it counts each application once, counts no shared runtime, and what Debian's package
manager resolves on the day depends on the snapshot. The shared runtimes are not choices; each
application brings the one it is built on.

The page also says how the applications will arrive, which matters more than the size:

| It says | Meaning |
|---|---|
| from the set on the stick | An exported application set (a `.ktar` file) was found. The install runs `kdos-appbox import` on it |
| built during the install, over the network | The machine has a network route, so the install runs `kdos-appbox install` with the ticked groups |
| recorded; the first session offers them | Neither. The choice is saved to `/var/lib/kdos/apps-pending` on the installed disk, and your first login shows a notification offering to install them |

A set on the stick takes precedence over everything else. Once any group is ticked, the install
imports the **whole** set — every application in it, not only the ticked groups — and never builds
over the network, even on a machine that has a network.

Only the recorded route is certain to reach the installed machine. The import and the network build
run the live session's own `kdos-appbox`, as root, after the system has already been copied, and
nothing points them at the new disk: what they install goes into the live session's pack store
and container storage, which the installed machine never sees. Until that changes, the dependable
way to get applications onto an installed machine is to install with no network and no set on a
stick, so the choice is recorded, or to skip the groups and install from the store after the first
login.

The installer looks for an exported set once, when it starts: the first file ending in `.ktar` at
the top level of a directory directly under `/mnt`, `/media` or `/run/media`. Only that one set is
offered. See [Applications](applications.md#carrying-a-set-to-another-machine) for making one.

Building takes minutes per application, which is why the "recorded" route waits for you rather
than starting a build unannounced at first boot. If an import or a build fails during the install,
the install still completes and the choice is recorded for your first login instead.

On a medium with no catalogue the page says so and there is nothing to choose; install applications
later with the store.

### 9. Summary

Everything you chose on one screen, with the destructive parts in the error colour and a rule
across the page reading `EVERYTHING ABOVE THIS LINE IS STILL REVERSIBLE`. Two buttons sit under
it:

- `BEGIN INSTALL` — the point of no return. Under `--dry-run` it reads `REHEARSE INSTALL`.
- `Save answer file` — writes your answers to `/tmp/kinstall.conf` and installs nothing.

### 10. Install

The work, with a progress bar and a live log. It runs in a separate process, so the screen stays
responsive during a multi-gigabyte copy. Each step reports as it starts, finishes, is skipped or
fails; `L` opens the full log.

| Step | What it does | Skipped when |
|---|---|---|
| Prepare | Unmounts the target and stops swap on it | |
| Partition | Writes the GPT layout | The plan is not "erase" |
| Format | Encrypts and creates the volume group if asked, then makes the filesystems | |
| Mount | Attaches the target at `/mnt` | |
| Copy system | Copies the live system with `rsync` | |
| Packs | Installs the applications you chose | Nothing was chosen, or the medium has no catalogue |
| Configure | `fstab`, hostname, keymap, time zone, automatic login, services | |
| Accounts | Users, passwords, `sudo` | |
| Theme | Regenerates the chosen accent for the new home directory | The accent is the default, `bone` |
| Bootloader | Limine on the ESP, for BIOS and UEFI | |
| Finish | Flushes and unmounts | |

A failed step stops there and offers `Retry step`, `View log`, `Shell` and `Abort`. A fixable
failure — a missing tool, a device that went away — can be fixed from the shell and the install
continued from the step that failed.

### 11. Done

What was installed and where, with `Reboot now`, `Back to the live desktop` and `View log`. The log
of the whole run is `/var/log/kinstall.log`.

## The root filesystem

Four choices. The one you pick sets the `mkfs` command, the mount options, the `fstab` line and how
the swap file is made, together, so they cannot disagree.

| Filesystem | Mount options | `fstab` pass | Swap file made with | Good for |
|---|---|---|---|---|
| `ext4` | `defaults,noatime` | 1 | `fallocate` | The default: journalled, dependable, built into the kernel |
| `btrfs` | `defaults,noatime,compress=zstd:3` | 0 | `btrfs filesystem mkswapfile` | Snapshots and transparent zstd compression |
| `xfs` | `defaults,noatime` | 0 | `dd` | Large files and parallel I/O. It cannot be shrunk |
| `f2fs` | `defaults,noatime` | 0 | `dd` | Flash storage: a stick, an SD card, a cheap eMMC |

Only ext4 gets a non-zero `fstab` pass number, which asks for a filesystem check at boot; the other
three have no boot-time checker worth running. The swap file is made differently on each because
xfs and f2fs refuse to swap on a file created with `fallocate` — and that refusal would only show
at the next boot, as a machine with no swap and no message.

ext4 and btrfs are built into the kernel; xfs and f2fs are modules the boot image carries. A
filesystem whose `mkfs` tool is missing from the image is still listed, marked as unavailable, and
is refused before anything is written.

## Encrypting the root

Ticking encryption on the Layout page puts LUKS2 on the root partition. There is no recovery key
and no way into the machine without the passphrase, which is asked for at every boot.

The installer runs `cryptsetup luksFormat` and then `cryptsetup open`, feeding the passphrase on
standard input both times. Passing it as an argument would expose it, through
`/proc/<pid>/cmdline`, to every process on the machine while `cryptsetup` runs. The opened
container is `/dev/mapper/kdosroot`, and everything after that — the `mkfs`, the mount, the
`fstab` entry and the copy — works on that one name.

The boot options then carry two different UUIDs, and it is easy to confuse them:

```
cryptdevice=UUID=<the LUKS container>:kdosroot
root=UUID=<the filesystem inside it>
```

The second only exists once the first is unlocked. At boot, the passphrase prompt appears on the
splash screen and the keystrokes are read from `tty1`, where the keyboard is. You get three
attempts, each counted on screen, and then a shell rather than a reboot loop. Nothing is shown as
you type — not even asterisks.

An image without `cryptsetup` refuses encryption on the Layout page, before the point of no return.

## A/B root slots

An installed KDOS machine is set up for two root filesystems, *slot A* and *slot B*, so that an
update can be installed into the slot you are not running and rolled back if it fails to boot.

The installer creates slot A only. It writes the initial boot state to the ESP
(`EFI/kdos/bootstate`): slot A is the filesystem it has just made, slot B is empty, slot A's
encrypted container (if any) is recorded beside it, and `bootstate=UUID=<esp>` is added to the
kernel options. Slot A's kernel and initramfs go into their own ESP directory, `EFI/kdos/a/`, and
each menu entry names its slot with `kdos_slot=a`.

**Slot B has to be created by hand.** Make a filesystem for it — on an LVM install, in the space
left free in the volume group with `lvcreate -n root_b -l 100%FREE kdos` — copy the system into it,
and record it with `kdos-bootctl set-slot b <filesystem-uuid> [<luks-uuid>]`. The second UUID is
needed only when slot B is encrypted: it names the LUKS container, and the first names the
filesystem inside it. `blkid` gives both, for example
`blkid -s UUID -o value /dev/kdos/root_b`. `set-slot` with fewer arguments prints its usage and
exits 2. From then on, `kdos update apply` installs updates
into the inactive slot, puts that slot's kernel in its own ESP directory, and marks the slot to be
tried. See [Keeping it current](administration.md#keeping-it-current).

How a trial works:

1. On UEFI, the candidate slot is booted once through the firmware's `BootNext` setting. Every boot
   after that starts the confirmed slot, so even a kernel that crashes before its initramfs is
   rolled back with no help. On BIOS, or where the firmware setting cannot be written, the boot
   menu leads the trial instead, for up to three boots.
2. The initramfs counts the attempt before anything is mounted.
3. The slot is promoted to active only when the boot scripts reach their end.

Recording a container per slot is what lets A/B work with encryption. The kernel command line can
name only one `cryptdevice=`, so the initramfs looks up the chosen slot's container after it has
picked the slot. A second *encrypted* slot has not been booted yet — see
[Known gaps](../06-reference/known-gaps.md#boot-and-updates). [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection)
describes the whole mechanism.

## Installing unattended

An answer file is plain `key = value` lines, with `#` for comments. Write one, check it, then
install from it:

```sh
kinstall --save answers.conf                   # a file of the current answers; installs nothing
kinstall --config answers.conf                 # preload them, still interactive
kinstall --config answers.conf --dump plan     # check what they would do
kinstall --unattended --config answers.conf    # install with no questions
```

`--save` on its own gives you a template of the defaults. To capture a real set of answers, walk
the wizard to the Summary page and press `Save answer file`, which writes `/tmp/kinstall.conf`; copy
that off the machine and hand it back with `--config`.

### The keys

| Key | Value | Default |
|---|---|---|
| `keymap` | A console keymap name, as `/etc/keymap` holds it | `us` |
| `autologin` | `yes` or `1` for automatic login at `tty1`; anything else, or no key, means a password prompt | off |
| `timezone` | The `TZ` value. The wizard writes `:/etc/localtime` | `:/etc/localtime` |
| `timezone_label` | The zone name `/etc/localtime` links to, such as `Europe/Berlin` | `UTC` |
| `disk` | The whole-disk device, such as `/dev/sda` | none |
| `plan` | `wipe`, `reuse` or `manual`; anything else reads as `wipe` | `wipe` |
| `esp`, `root` | On the `reuse` plan: the ESP partition, and the root as a partition or a logical volume (`/dev/<vg>/<lv>`) | none |
| `format_esp` | `1` to reformat the ESP as FAT32, `0` to keep it | `1` |
| `fstype` | `ext4`, `btrfs`, `xfs` or `f2fs`; anything else reads as `ext4` | `ext4` |
| `swap` | `file` or `partition`; anything else means no swap | `file` |
| `swap_mb` | Swap size in MiB | `4096` |
| `luks` | `1` for an encrypted root | `0` |
| `lvm` | `1` to put the root on LVM, on the `wipe` plan | `0` |
| `luks_passphrase` | The encryption passphrase, in plain text | none |
| `hostname` | The machine's name | `kdos` |
| `username`, `fullname` | The account | `kdos`, `KDOS User` |
| `password` | The user's password, in plain text | none |
| `root_locked` | `1` to lock root, `0` to set `root_password` | `1` |
| `root_password` | The root password, in plain text | none |
| `theme` | An accent name, such as `bone` | `bone` |
| `alien_apps` | `1` to copy the live container store, `0` to leave it out | `1` |
| `apps` | Space-separated **group** ids, such as `essential office` | `essential` |
| `services` | Space-separated names from `networkmanager bluetooth alsa cups sshd`; a service not named does not start | `networkmanager bluetooth alsa` |
| `reboot` | `1` to reboot when an unattended run finishes, `0` to exit | `1` |

Things to watch for:

- **Automatic login is off unless you ask for it.** The live medium logs in automatically, but an
  answer file without `autologin = yes` installs a machine that asks for a password at `tty1`. The
  install edits the target's `/etc/kdos/login.conf` in place — `autologin = <username>` for on, the
  same line commented out for off.
- **`apps` takes groups, not applications.** An application id such as `app.krita` is ignored. If
  none of the names is a known group, the selection falls back to `essential` rather than failing
  the install over a spelling mistake.
- **There is no key for the administrator choice.** An account installed from an answer file is
  always in `wheel`.

### Passwords

No password is ever written to an answer file. `--save` and the Summary button both leave out
`password`, `root_password` and `luks_passphrase`, and the file's header says so. Adding them
yourself is how an unattended install gets credentials — and it puts a plain-text credential on
whatever medium the file lives on.

Set `password` for an unattended run. Nothing asks for one, and without it the account keeps the
live medium's password entry — under the old name, if you also renamed the account.

### What keeps a scripted run safe

- An unknown filesystem falls back to ext4, and an unknown set of groups falls back to `essential`,
  so a typo cannot leave a half-installed disk.
- An unattended run always ends by itself. It shows the finished screen for five seconds so a
  watcher can see what happened, then reboots or exits according to `reboot`.
- With `reboot = 0` it exits with status 0 when the install worked and 1 when it failed. With
  `reboot = 1` it reboots either way, so set `reboot = 0` when a script needs the result.

## Looking without installing

```sh
kinstall --dry-run                    # log every command, execute none
kinstall --dump probe                 # the machine as the installer sees it
kinstall --dump plan                  # the steps that would run, including the skips
kinstall --dump plan --json           # the same, machine-readable
```

`--dump probe` is the right thing to paste into a bug report: firmware, processor, memory, and
every disk and partition. `--dump plan` uses the same planner as the wizard, so its step list and
skips are the real ones — choosing `reuse` skips the Partition step, the default accent skips the
Theme step, and a medium with no catalogue skips the Packs step.

No password appears in either dump; it only says whether one is set.

`--dry-run` runs the whole wizard and the whole install, logging every command and executing none.
The header shows `DRY RUN` and the Summary button reads `REHEARSE INSTALL`, so you always know which
mode you are in.

## What the installer writes

| Path | What |
|---|---|
| The ESP | Limine — both EFI loaders, `BOOTX64.EFI` and `BOOTIA32.EFI` — its generated `limine.conf`, the BIOS second stage, and slot A's kernel and initramfs in `EFI/kdos/a/` |
| The ESP | The A/B boot state `EFI/kdos/bootstate`, memtest86+ as `EFI/kdos/memtest.efi`, and the menu's font and wallpaper where the medium has them |
| Root | The system, copied from the live medium with `rsync` |
| `/etc/fstab` | Appended to, never replaced — the shipped file carries the `/tmp` entry that graphical applications depend on. The ESP is mounted at `/boot/efi` |
| `/etc/hostname`, `/etc/hosts` | The hostname, and the `127.0.1.1` line that resolves it locally |
| `/etc/keymap` | The console keymap |
| `/etc/localtime`, `/etc/profile.d/20-timezone.sh` | The time zone, as a link and as `TZ` |
| `/etc/modprobe.d/kdos-regdom.conf` | The zone's country, as the Wi-Fi regulatory domain |
| `/etc/kdos/login.conf` | The `autologin` line, edited in place |
| `/etc/passwd`, `/etc/group`, `/etc/shadow` | The account, renamed and with its password hashed; in `wheel` only if you chose administrator |
| `/etc/subuid`, `/etc/subgid` | The account's subordinate ID ranges, under its new name |
| `/etc/service.disabled/` | One flag file per service you turned off |
| `/etc/resolv.conf` | Copied from the live system, so the installed machine resolves names on first boot |
| `/swapfile` | The swap file, when you chose one |
| `/var/lib/kdos/packs` | The pack store, with the `staging` directory an import goes through |
| `/var/lib/kdos/apps-pending` | The application groups your first login offers, on the "recorded" route |

Two files are deliberately **not** copied: `/var/lib/dbus/machine-id` and the SSH host keys in
`/etc/ssh/`. Each installed machine generates its own on first boot; copied, every machine
installed from one stick would share the same identity and the same host keys.

The kernel and initramfs are copied **onto the ESP**, and `limine.conf` points at them there. Limine
reads only FAT and ISO9660 — not xfs, f2fs or anything encrypted, all of which the installer offers
as a root — so keeping the kernel on the ESP is what keeps every choice bootable. Each root slot
boots its own kernel from its own directory; see
[One kernel per slot](../03-architecture/boot-and-init.md#one-kernel-per-slot).

`limine.conf` sits at the root of the ESP rather than beside `BOOTX64.EFI`, because that is the one
location both firmware types search.

Both EFI loaders are installed, and firmware runs only the one it can execute: `BOOTIA32.EFI` costs
about a hundred kilobytes and makes the disk start on 32-bit UEFI firmware. If the Limine build has
no 32-bit loader, the log carries a warning and the install continues. The firmware boot entry
written to NVRAM names one loader, chosen from `/sys/firmware/efi/fw_platform_size`.

The BIOS boot code is written whichever way the installing machine booted. It costs one sector, and
it means a disk installed on a UEFI machine still starts when moved to a legacy one. A failure there
is reported but does not stop the install, because on a UEFI machine the EFI path is already
complete.

The installed boot menu counts down for ten seconds before booting `KDOS`. Press any key to stop the
countdown and choose another entry:

| Entry | Boots |
|---|---|
| `KDOS` | The normal system |
| `KDOS (verbose)` | Every kernel message on the console |
| `KDOS (single user)` | The same as `KDOS (verbose)`. The entry passes `single`, but nothing in KDOS acts on it, so services and the desktop start as usual; KDOS has no single-user mode. For a shell without the desktop, use `tty2` |
| `Memory Test (memtest86+)` | memtest86+. UEFI only |

When a second slot exists, the menu also shows an entry for it. A machine that will not boot needs
one of these entries, which is why the countdown is long enough to catch.

## See also

- [Getting started](getting-started.md) — building the image you are installing from
- [kinstall](../04-programs/kinstall.md) — how the installer works inside
- [Administration](administration.md) — services, users and storage after the install
- [Boot and init](../03-architecture/boot-and-init.md) — A/B slots, encryption and the boot path
- [Applications](applications.md) — installing applications after the install
