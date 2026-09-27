# Installation

This chapter is for anyone putting KDOS onto a disk. The installer, `kinstall`, copies the running
live system onto a target disk, configures the copy for its new machine and makes it bootable on
both UEFI and legacy BIOS firmware. It ships on every KDOS image, draws its own full-screen
interface on a bare text console, a terminal window or a serial line, and can install a machine
unattended from an answer file. Read [Getting started](getting-started.md) first if you do not yet
have a booted live image. Terms this chapter does not define are in the
[Glossary](../06-reference/glossary.md).

The chapter follows the order in which you meet things. [Before you start](#before-you-start) lists
what to decide and what the installer needs; [The pages](#the-pages) walks through the wizard screen
by screen. The sections after that are reference: the filesystem choices, encryption, the A/B root
slots, answer files for unattended installs, the inspection modes, and every file the installer
writes. How the installer is built inside is in [kinstall](../04-programs/kinstall.md).

## Before you start

Nothing reaches your disk until you press `BEGIN INSTALL` on the Summary page, with one exception:
the *Partition it myself* plan. Its `Open cfdisk` button runs `cfdisk` on the target disk, and a
partition table you write in `cfdisk` is on the disk the moment you quit it. Every other page before
the Summary only records an answer, so `Back` returns to a page unchanged and quitting leaves the
machine as you found it. Pressing `Next` on the Summary page does not start the install; it tells
you to press `BEGIN INSTALL`, so a stray page-forward keystroke cannot erase a disk.

Two answers are worth settling before you begin, because changing either one later means installing
again:

- **The root filesystem**: ext4, btrfs, xfs or f2fs. See [The root
  filesystem](#the-root-filesystem).
- **Whether the root is encrypted.** Encryption is applied when the partition is created, so it
  cannot be added to or removed from an existing install. See
  [Encrypting the root](#encrypting-the-root).

You need:

- root privileges (run the installer with `sudo`);
- a target disk that is not the medium you booted from;
- a disk at least 768 MiB larger than the install, which the Welcome page reports as *install
  size*.

The wizard checks each of these and names the one that failed.

Applications are optional during the install. Read [8. Applications](#8-applications) before
relying on them: of the three ways the installer can deliver applications, only the one that records
your choice for the first login reaches the installed machine. The other two run the live session's
own `kdos-appbox`, which writes into the live system's pack store and container storage rather than
onto the new disk.

## Running the installer

From a terminal on the live system:

```sh
sudo kinstall
```

If the desktop owns the screen, give the installer a text console of its own, and switch to that
console with `Ctrl+Alt+F3`:

```sh
sudo sh -c 'kinstall < /dev/tty3 > /dev/tty3 2>&1'
```

The redirections sit inside the root shell because a virtual console belongs to root: opened by your
own shell before `sudo` runs, they fail with `Permission denied` and the installer never starts.

Before it takes over the screen, the installer prints `kinstall: measuring the live system...` and
walks the whole live tree once to measure how much it will copy. On a large live session this takes
a few seconds.

The interface works with both keyboard and mouse. On a bare text console it reads the mouse directly
from `/dev/input`, so clicking works on `tty1` with no mouse daemon. It needs a terminal of at least
62 columns by 18 rows; below that it shows only a message asking for a larger one. The sidebar that
lists the pages appears at 78 columns and wider, and its foot shows the accent, the mouse source
(`evdev`, read from the kernel's input devices; `sgr`, reported by the terminal; or `off`) and the
colour depth the installer detected. A page taller than the screen scrolls, and a scroll bar marks
it.

| Key | Action |
|---|---|
| `Tab` / `Shift+Tab` | Move between controls |
| `↑` `↓` `←` `→` | Move inside a list or a field |
| `Enter` / `Space` | Activate, toggle, choose |
| `Alt+←` / `Alt+→` | Previous and next page |
| `Esc` | Back a page, or close the key card |
| `PgUp` / `PgDn`, mouse wheel | Scroll a page taller than the screen |
| `Ctrl+U` | Clear the field you are in |
| `F1` | Show or hide the key card |
| `L` | Toggle the full log, on the Install page |
| `Ctrl+Q`, `Ctrl+C` | Quit. The installer always asks first, and during an install it warns that the target is left half written |

### Command-line options

| Option | Effect |
|---|---|
| `--config FILE` | Read answers from `FILE`. See [Installing unattended](#installing-unattended) |
| `--save FILE` | Write the current answers to `FILE` and exit without installing |
| `--unattended` | Skip the wizard and install from the answers loaded with `--config` |
| `--dry-run` | Run the wizard and the install, logging every command and executing none |
| `--dump probe` | Print the machine as the installer sees it, and exit |
| `--dump plan` | Print the steps these answers would run, and exit |
| `--json` | Print `--dump` output as JSON |
| `--theme NAME` | Draw the installer in this accent and preselect it for the installed system: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect` or `paper` |
| `--no-mouse` | Keyboard only |
| `--ascii` | Draw boxes with `-`, `\|` and `+`, for terminals that cannot show line-drawing characters |
| `--version`, `--help` | Print and exit |

An unknown option prints the usage and exits with status 1, as does a `--config` file that cannot be
read. `--save` and `--dump` act before the root check and the disk probe, so neither needs root.

Every install, including a `--dry-run` rehearsal, appends to the log `/var/log/kinstall.log`.

## The pages

The wizard has eleven pages, always in this order. The sidebar marks the page you are on and the
pages behind you; click a finished page to go back to it. Pages ahead cannot be clicked, because
nothing has checked them yet. On the Applications page the `Next` button reads `Review`, to mark the
end of the questions. `Next` runs the page's checks, and a refusal appears in the bottom line for
eight seconds.

### 1. Welcome

What the installer measured about this machine: the processor and its thread count, memory, the
number of disks visible, the *install size* (how much the install will copy), and the firmware.
Below that is a preflight list:

- running as root;
- at least one writable disk;
- `rsync` present;
- `mkfs.ext4` and `mkfs.vfat` present;
- the Limine payload present (the directory `/usr/share/limine`).

The first two stop you on this page; the other three only mark a warning. The disk line counts every
disk the installer found, read-only ones included; a read-only disk is refused on the Disk page. The
install's first step checks `mount`, `umount`, `mkfs.vfat`, `rsync` and the chosen filesystem's
`mkfs` again, and refuses before anything is written. It does not check for Limine again: if the
Limine files are missing, the install writes the disk and then fails at the Bootloader step, leaving
a machine that cannot boot. Do not start an install while that line is marked.

The disk count leaves out loop, RAM, optical, zram, device-mapper, software RAID, floppy and network
block devices.

The firmware line is information, never a refusal. Limine is written for both firmware types
whichever way this machine started, so the line only tells you which path the firmware will use:

- `legacy BIOS`: this machine booted that way. It gets the same install, without the firmware boot
  entry, which cannot be created in legacy mode.
- `UEFI  (32-bit firmware)`: a 64-bit processor with 32-bit UEFI firmware, as on some early Atom
  tablets. The firmware boot entry then names the 32-bit EFI loader.
- `(Secure Boot enabled)`: the install completes, but the installed machine will not start until
  you turn Secure Boot off in firmware setup. Limine's EFI loader is unsigned and KDOS enrols no
  keys; see [Known gaps](../06-reference/known-gaps.md#hardware-and-platform).

### 2. Keyboard

The console keymap, chosen from every map under `/usr/share/keymaps` and `/usr/share/kbd/keymaps`,
with a filter box because the list is long. Moving through the list applies each map with
`loadkeys`, so you can try it in the box beside the list. On an image without `loadkeys` the preview
is skipped, the page says so, and the choice is still saved. The install writes it to `/etc/keymap`,
which `kdos-getty`, the program that runs the login on each text console, loads on every console.

The desktop uses the same setting. When the session starts it translates the console keymap name
into the matching XKB layout (XKB is the keyboard-layout system the desktop uses), because the
console and XKB name layouts differently: `uk` becomes `gb`, `sg` becomes `ch`, `dvorak` becomes
`us` with the `dvorak` variant, and most other names are cut to their first two letters. A layout
that is not present in the image's XKB data is named in the session log, and the desktop keeps its
default layout.

### 3. Time

A time zone by name. The list is tzdata's `zone1970.tab`, with a filter box, and the page shows the
current local time in the zone under the cursor. An image with no zone data offers `UTC` alone.

The install records the zone in two places that always agree: `/etc/localtime` becomes a symbolic
link to the zone file, and `/etc/profile.d/20-timezone.sh` exports `TZ=':/etc/localtime'`. The
leading colon makes the C library read that same file, so the link and the variable cannot describe
different rules. The hardware clock is left as it is. If there is no zone file for the name you
picked, the install log says so and the machine keeps UTC.

The zone also sets the Wi-Fi country. The installer looks up the zone's country in tzdata's
`zone.tab` and writes it to `/etc/modprobe.d/kdos-regdom.conf` as the wireless regulatory domain
(`options cfg80211 ieee80211_regdom=<country>`). Without it the radio stays in the restrictive
"world" domain, with the 5 GHz channels that need radar detection closed and transmit power capped.
A zone with no country, such as `UTC`, writes nothing.

### 4. Disk

Which disk, and nothing else. Each row shows the device, its size, its connection type and its
model. The first disk that is neither the boot medium nor read-only is selected when you arrive.
Below the list, the disk's current partitions are drawn as a proportional bar and listed with their
size, type, label and mount point, so you can see what is about to go. Under that are the disk's
capacity and what this install needs.

The medium you booted from is marked `[boot medium]` and refused, because installing onto it would
pull the running system out from under the installer. A read-only disk is refused, and so is a disk
smaller than the install plus 768 MiB.

### 5. Layout

How the chosen disk is used: the plan, the root filesystem, swap, LVM (Linux's logical volume
manager, which divides a partition into resizable volumes) and encryption.

| Plan | What happens |
|---|---|
| Erase the whole disk | A new GPT (GUID partition table): a 512 MiB EFI System Partition (ESP), a swap partition if you asked for one, then the root |
| Use partitions that already exist | You choose the ESP and the root; every other partition on the disk is left alone and the Partition step is skipped. The root can be a partition or an existing LVM logical volume, and whichever you choose is always reformatted |
| Partition it myself | `Open cfdisk` hands the screen to `cfdisk`. Create an EFI System partition and a Linux partition, write the table and quit. The installer rereads the disk and switches to the reuse plan, where you assign the two partitions |

The root filesystem is chosen on both the erase and the reuse plan. A filesystem whose `mkfs` tool
is missing from the image is still listed, labelled `no mkfs on this image`, and the install refuses
it before writing anything.

#### The erase plan

On the erase plan the page offers swap, LVM and encryption, and draws what will be written: the
partitions by name and size, as a bar and as a list.

#### Swap

Swap is a file on the root filesystem, a partition of its own, or none. The size starts at
4096 MiB, and the page shows how much RAM the machine has beside it.

#### LVM

`Put the root on LVM` makes the root partition the single physical volume of a volume group named
`kdos`, with the root on its logical volume `root_a`. The ESP and a swap partition stay plain
partitions. `root_a` is slot A's root and takes half the group, leaving the other half free for a
second root slot; see [A/B root slots](#ab-root-slots). On a disk where half would not hold the
install, its swap file and a 256 MiB margin, `root_a` takes all of it, and the page and the Summary
say which. With encryption on as well, the volume group sits inside the encrypted container, so one
passphrase opens both slots. The page refuses LVM on an image without the `lvm` tool, and refuses
when a volume group named `kdos` already has any part of itself on another disk, because creating
the new group would then fail after this disk had been erased. Rename the old group with `vgrename`
or leave LVM off.

#### Encryption

`Encrypt the root filesystem (LUKS2)` puts LUKS2, the standard Linux disk-encryption format, on the
root. The checkbox is drawn on the erase plan only, because encrypting a partition destroys what is
on it and the reuse plan exists for people who are keeping something. Tick it and two passphrase
fields appear; they must match, and an image without `cryptsetup` is refused on this page.

The tick is kept if you then switch to the reuse plan, or pick *Partition it myself*, which switches
to the reuse plan after `cfdisk`. The root you choose there is then encrypted, destroying what was
on it, although the reuse plan shows no checkbox and the Summary shows no encryption line. Untick
encryption on the erase plan before you switch.

#### The reuse plan

On the reuse plan the page shows two lists, the ESP and the root, and a checkbox to reformat the ESP
as FAT32. Leave that off when another operating system boots from the same ESP. The root list shows
the disk's partitions, then every LVM logical volume on the machine, named `vg/lv`. The installer
activates every volume group when it starts, so volumes on this disk appear. Thin and cached volumes
are offered as ordinary volumes, and the initramfs carries `thin_check` and `cache_check` for them;
their internal volumes are not listed. A root on LVM of any kind has not been booted; see [Known
gaps](../06-reference/known-gaps.md#hardware-and-platform).

The reuse plan refuses as the root:

- the ESP you chose;
- an LVM physical volume (choose one of its logical volumes instead);
- a partition or volume that something else holds open, such as an open encrypted container or an
  active volume group;
- a partition smaller than the install.

A software RAID array and an opened LUKS container are not offered as the root. The reuse plan shows
no swap choice; the swap setting it installs with is the one the erase plan last showed, which is a
4096 MiB swap file unless you changed it. A swap *partition* is only created by the erase plan, so
on the reuse plan that setting installs no swap at all.

### 6. Accounts

- **Hostname**: letters, digits and dashes.
- **Full name and user name**: the user name is lowercase letters, digits, `-` and `_`, and does
  not start with a digit.
- **Password**: required, typed twice, with a strength meter.
- **Administrator**: ticked by default. Ticked, the account is in the `wheel` group and can use
  `sudo` and the desktop's administrative actions. Unticked, the account is taken out of `wheel`; it
  stays in `seat`, so it still has the desktop and can suspend, power off, reboot and mount
  removable media.
- **Lock the root account**: ticked by default. Unticked, you set a root password, typed twice.

Root locked together with a non-administrator account would leave nobody able to gain privileges,
so `Next` refuses that combination.

The installed machine asks for your password at `tty1`; only the live image logs in automatically.
The wizard has no automatic-login option. Set `autologin = yes` in an answer file (see
[Installing unattended](#installing-unattended)), or after installing set `autologin = <username>`
in `/etc/kdos/login.conf`. The live image's root password is never carried over: the root entry in
`/etc/shadow` is always rewritten, locked or with the password you set.

A user name other than `kdos` is a real rename. The installer rewrites `/etc/passwd` (name, full
name and home directory), `/etc/group` (the membership lists and the primary group's name),
`/etc/shadow`, and the owner of the `/etc/subuid` and `/etc/subgid` ranges that rootless containers
use. It moves `/home/kdos` to the new name, and writes the new name into the `autologin` line of
`/etc/kdos/login.conf`, which is the only place the desktop account is named.

### 7. System

Three settings: the accent, the live container store, and which services start at boot.

#### Accent

There are eight accents, and the installer repaints itself in the one you select, so you choose by
colour rather than by name. The installed system's desktop, GTK theme, icons, cursors, foot, btop
and boot menu use it. See [Theming](theming.md).

#### The alien app library

The checkbox is `Install the alien app library`. An *alien app* is a graphical application that KDOS
does not compile itself and runs in a container instead; see
[Applications](applications.md#what-an-alien-app-is). This checkbox, shown with its size, decides
whether the live account's container store, `/home/kdos/.local/share/containers` (the images Podman
keeps), is copied to the disk. The medium ships no prebuilt applications, so the store holds only
what the live session has itself pulled or built, often nothing. Leaving it out does not stop you
installing applications later.

#### Services

Five services are a choice rather than part of the system:

| Service | Name | Default |
|---|---|---|
| NetworkManager (wired, Wi-Fi and VPN) | `networkmanager` | On |
| Bluetooth | `bluetooth` | On |
| ALSA state (restore mixer levels at boot) | `alsa` | On |
| CUPS (printing) | `cups` | Off |
| OpenSSH server | `sshd` | Off |

A service you turn off becomes an empty flag file `/etc/service.disabled/<name>` on the installed
machine, and the boot scripts skip it. Any of them can be changed later with `service enable` or
`service disable`; see [Administration](administration.md#services).

### 8. Applications

The application groups this medium's catalogue can build, with `essential` already ticked. The
catalogue is `/usr/share/kdos/appstore/catalogue`; the groups and their descriptions come from it,
and a group none of whose members is in the catalogue is not shown. The shipped catalogue has seven.
`essential` holds Firefox ESR, LibreOffice, GIMP, Zathura and KeePassXC.

| Group | Description | Applications |
|---|---|---|
| `essential` | What a new machine starts with | 5 |
| `office` | Documents, spreadsheets and reading | 8 |
| `creative` | Images, audio and video | 10 |
| `dev` | Programming and electronics | 8 |
| `science` | Computation, modelling and data | 9 |
| `make` | Three dimensions: model it, slice it, cut it | 8 |
| `games` | Games and emulators | 13 |

`Space`, `Enter` or a click toggles a group. The size total under the list updates as you go and is
labelled an estimate: each group counts its own members once and no shared runtime, and an
application in two ticked groups is counted in both (LibreOffice, Zathura and GIMP are each in
`essential` and in one other group). The Summary's application count is summed the same way. The
shared runtimes are not choices; each application brings the one it is built on.

The page also says how the applications will arrive, which matters more than the size:

| It says | Meaning |
|---|---|
| from the set on the stick | An exported application set (a `.ktar` file) was found on a mounted device. The install runs `kdos-appbox import` on it |
| built during the install, over the network | The machine has a default network route, so the install runs `kdos-appbox install` with the ticked groups |
| recorded; the first session offers them | Neither. The choice is saved to `/var/lib/kdos/apps-pending` on the installed disk |

A set on a stick takes precedence over everything else. Once any group is ticked, the install
imports the whole set (every application in it, not only the ticked groups) and never builds over
the network, even on a machine that has a network.

This is a known gap: only the recorded route is certain to reach the installed machine; see [Known
gaps](../06-reference/known-gaps.md#applications-imported-or-built-during-an-install-stay-in-the-live-session).
The import and the network build run the live session's own `kdos-appbox`, as root, after the
system has already been copied, and nothing points them at the new disk: what they install goes
into the live session's pack store (where installed [packs](../06-reference/glossary.md) live; see
[Packs and boxes](../03-architecture/packs-and-boxes.md)) and container storage, which the installed
machine never sees. The dependable way to get applications onto an installed machine is to install
with no network and no set on a stick, so that the choice is recorded, or to skip the groups and
install from the application store (`kdos app install`) after the first login.

On the recorded route, your first desktop login shows a notification offering to build the recorded
groups; `kdos app install --pending` builds them from a terminal. The offer is made once per user,
marked by `~/.config/kdos/apps-offered`; delete that file and the offer returns at the next login.
Building takes minutes per application, which is why this route waits for you rather than starting
a build unannounced at first boot. If an import or a build fails during the install, the install
still completes and the choice is recorded for the first login instead.

The installer looks for an exported set each time you arrive at the Applications page (an unattended
install looks once, when it starts): the first file ending in `.ktar` at the top level of a
directory directly under `/mnt`, `/media` or `/run/media`. A stick the desktop mounted is at
`/media/<user>/<label>`, one level deeper than the installer looks, so its set is not found. Mount
the stick directly under `/mnt` or `/media` before you reach the Applications page, or before an
unattended install starts, for example with `sudo mount /dev/sdX1 /mnt/stick`. Only that one set is
offered. See
[Applications](applications.md#carrying-a-set-to-another-machine) for making one.

On a medium with no catalogue the page says so and there is nothing to choose; install applications
later from the application store.

### 9. Summary

Your answers on one screen: keymap, time zone, hostname, user, root account, accent, whether the
alien app library is copied, the application groups and their route, the enabled services, the
target disk, the plan, the root and ESP on the reuse plan, the LVM layout when there is one, and
swap. The Summary shows encryption only as part of the erase plan's LVM line, so with encryption on
and LVM off, or on the reuse plan, check the Layout page before you begin. The destructive parts are
in the error colour, and a rule across the page reads `EVERYTHING ABOVE THIS LINE IS STILL
REVERSIBLE`. Two buttons sit under it:

- `BEGIN INSTALL`: starts writing to the disk. Under `--dry-run` it reads `REHEARSE INSTALL`.
- `Save answer file`: writes your answers to `/tmp/kinstall.conf` and installs nothing.

`BEGIN INSTALL` starts the install in the background and leaves the screen on the Summary page. An
interactive install does not move to the Install page: the screen shows no progress, no failed
step and none of the Install page's buttons, and `BEGIN INSTALL` stays live, so a second press
starts a second run on the same disk. This is a known gap; see [Known
gaps](../06-reference/known-gaps.md#an-interactive-install-stays-on-the-summary-page). Press it
once, then follow the install from another console:

```sh
sudo tail -f /var/log/kinstall.log
```

The log records every command as it runs and, when a step fails, the reason. A finished install
ends with the Finish step unmounting `/mnt`. `Ctrl+Q` still warns that the target would be left
half written while the install is running. The Install page below is what an unattended install
shows, and no install reaches the Done page; see [kinstall](../04-programs/kinstall.md#the-pages).

### 10. Install

The page an unattended install opens on (see [Installing unattended](#installing-unattended)):
the work, with an overall progress bar, one line per step and the tail of the log. The install runs
in a separate process, so the screen stays responsive during a multi-gigabyte copy. Each step is
marked as it starts, finishes, is skipped (`not needed`) or fails; the Copy step shows the amount
copied, the speed and the time remaining. `L` switches to the full log, where `PgUp` and `PgDn`
scroll.

| Step | What it does | Skipped when |
|---|---|---|
| Prepare | Checks the tools and the target, stops swap, unmounts the target and anything on the disk, and on the erase plan takes down volume groups and containers on it | |
| Partition | Wipes the disk's signatures and writes the GPT layout | The plan is not "erase" |
| Format | Encrypts and creates the volume group if asked, then makes the root filesystem, the ESP (when it is to be formatted) and the swap partition | |
| Mount | Mounts the root at `/mnt` and the ESP at `/mnt/boot/efi` | |
| Copy system | Copies the live system with `rsync` | |
| Packs | Imports, builds or records the applications you chose | Nothing was chosen, or the medium has no catalogue |
| Configure | `fstab`, hostname, `/etc/hosts`, time zone, Wi-Fi country, keymap, automatic login, DNS resolver, services and the swap file | |
| Accounts | Users, passwords, the administrator choice | |
| Theme | Regenerates the chosen accent in `/etc/skel` and the new home directory | The accent is the default, `bone` |
| Bootloader | Limine on the ESP, for BIOS and UEFI, and the firmware boot entry | |
| Finish | Flushes and unmounts | |

A failed step stops there and offers `Retry step`, `View log`, `Shell` and `Abort`. `Shell` opens a
login shell on the console; a fixable failure, such as a missing tool or a device that went away,
can be fixed there and the install continued from the failed step with `Retry step`. `Abort` leaves
the installer. When every step has finished, `Continue` appears, but it stays on the Install page;
an unattended run then ends by itself after five seconds.

### 11. Done

No install reaches this page, interactive or unattended, because `Continue` on the Install page
does not lead to it. The page is drawn to show what was installed, where, and how long it took,
with `Reboot now` (which asks first), `Back to the live desktop` and `View log`. After an
interactive install whose log ends with the Finish step, quit the installer with `Ctrl+Q`
and restart the machine yourself. Remove the boot medium before restarting, or the firmware may
start the live image again.

## The root filesystem

The installer offers four root filesystems. The one you pick sets the `mkfs` command, the mount
options, the `fstab` line and how the swap file is made, together, so they cannot disagree. The root
filesystem is always labelled `KDOS`.

| Filesystem | Mount options | `fstab` pass | Swap file made with | Suited to |
|---|---|---|---|---|
| `ext4` | `defaults,noatime` | 1 | `fallocate` | The default: journalled, well understood, built into the kernel |
| `btrfs` | `defaults,noatime,compress=zstd:3` | 0 | `btrfs filesystem mkswapfile` | Snapshots and transparent zstd compression |
| `xfs` | `defaults,noatime` | 0 | `dd` | Large files and parallel I/O. It cannot be shrunk |
| `f2fs` | `defaults,noatime` | 0 | `dd` | Flash storage: a stick, an SD card, a low-cost eMMC |

Only ext4 gets a non-zero `fstab` pass number, which asks for a filesystem check at boot; the other
three have no boot-time checker worth running. The swap file is made differently on each because
xfs and f2fs refuse to swap on a file created with `fallocate`, and btrfs needs a file that is not
copy-on-write and not compressed. The refusal would only show at the next boot, as a machine with no
swap and no message. Where `fallocate` or the `btrfs` tool is missing, the swap file is made with
`dd` instead.

ext4, btrfs and xfs are built into the kernel; f2fs is a module, and the initramfs carries it.

## Encrypting the root

Ticking encryption on the Layout page puts LUKS2 on the root partition. There is no recovery key and
no way into the machine without the passphrase, which is asked for at every boot.

The installer runs `cryptsetup luksFormat --type luks2` and then `cryptsetup open`, feeding the
passphrase on standard input both times. Passing it as an argument would expose it, through
`/proc/<pid>/cmdline`, to every process on the machine while `cryptsetup` runs. The opened container
is `/dev/mapper/kdosroot`, and everything after that (the `mkfs`, the mount, the `fstab` entry and
the copy) works on that one name.

The boot options then carry two different UUIDs, and it is easy to confuse them:

```text
cryptdevice=UUID=<the LUKS container>:kdosroot
root=UUID=<the filesystem inside it>
```

The second exists only once the first is unlocked. At boot, the passphrase prompt appears on the
splash screen and the keystrokes are read from `tty1`, where the keyboard is. You get three
attempts, each counted on screen, and then a shell rather than a reboot loop. Nothing is shown as
you type, not even asterisks.

An image without `cryptsetup` refuses encryption on the Layout page, before anything is written.

## A/B root slots

An installed KDOS machine is set up for two root filesystems, *slot A* and *slot B*, so that an
update can be installed into the slot you are not running and rolled back if it fails to boot.

The installer creates slot A only. It writes the initial boot state to the ESP
(`EFI/kdos/bootstate`): slot A is the filesystem it has made, slot B is empty, slot A's encrypted
container (if any) is recorded beside it, and `bootstate=UUID=<esp>` is added to the kernel options.
Slot A's kernel and initramfs go into their own ESP directory, `EFI/kdos/a/`, and each menu entry
names its slot with `kdos_slot=a`.

The installer does not create slot B; you create it by hand. Make a filesystem for it (on an LVM
install, in the space left free in the volume group, with `lvcreate -n root_b -l 100%FREE kdos`),
copy the system into it, and record it with `kdos-bootctl set-slot b <filesystem-uuid>
[<luks-uuid>]`. The second UUID is needed only when slot B is encrypted: it names the LUKS
container, and the first names the filesystem inside it. `blkid` gives both, for example `blkid -s
UUID -o value /dev/kdos/root_b`. `set-slot` with fewer arguments prints its usage and exits with
status 2. From then on, `kdos update apply` installs updates into the inactive slot, puts that
slot's kernel in its own ESP directory, and marks the slot to be tried. See [Keeping it
current](administration.md#keeping-it-current).

A trial works like this:

1. On UEFI, the candidate slot is booted once through the firmware's `BootNext` setting. Every boot
   after that starts the confirmed slot, so even a kernel that crashes before its initramfs is
   rolled back with no help. On BIOS, or where the firmware setting cannot be written, the boot
   menu leads the trial instead, for up to three boots.
2. The initramfs counts the attempt before anything is mounted.
3. The slot is promoted to active only when the boot scripts reach their end.

Recording a container per slot is what lets A/B work with encryption. The kernel command line can
name only one `cryptdevice=`, so the initramfs looks up the chosen slot's container after it has
picked the slot. A second *encrypted* slot has not been booted; see [Known
gaps](../06-reference/known-gaps.md#boot-and-updates). [Boot and
init](../03-architecture/boot-and-init.md#ab-slot-selection) describes the whole mechanism.

## Installing unattended

An answer file is plain `key = value` lines, with `#` starting a comment line. Write one, check it,
then install from it:

```sh
kinstall --save answers.conf                   # a file of the default answers; installs nothing
kinstall --config answers.conf                 # preload them, still interactive
kinstall --config answers.conf --dump plan     # check what they would do
sudo kinstall --unattended --config answers.conf    # install with no questions
```

`--save` on its own writes a template of the defaults. `--config` with `--save` writes the loaded
answers back out in the same form. To capture a real set of answers, walk the wizard to the Summary
page and press `Save answer file`, which writes `/tmp/kinstall.conf`; copy that off the machine and
hand it back with `--config`.

### The keys

Every key the installer reads is below. [kinstall](../04-programs/kinstall.md#answer-files) gives
the same keys with what each one writes and which plan honours it.

| Key | Value | Default |
|---|---|---|
| `keymap` | A console keymap name, as `/etc/keymap` holds it | `us` |
| `autologin` | `yes` or `1` for automatic login at `tty1`; anything else, or no key, means a password prompt | off |
| `timezone` | The `TZ` value. The wizard always writes `:/etc/localtime` | `:/etc/localtime` |
| `timezone_label` | The zone name `/etc/localtime` links to, such as `Europe/Berlin` | `UTC` |
| `disk` | The whole-disk device, such as `/dev/sda` | none |
| `plan` | `wipe`, `reuse` or `manual`; anything else reads as `wipe` | `wipe` |
| `esp`, `root` | On the `reuse` plan: the ESP partition, and the root as a partition or a logical volume (`/dev/<vg>/<lv>`) | none |
| `format_esp` | `1` to format the ESP as FAT32, `0` to keep it. It applies on every plan: `0` keeps an existing ESP on the `reuse` plan, and leaves the new ESP unformatted on the `wipe` plan, where the install then fails at the Mount step | `1` |
| `fstype` | `ext4`, `btrfs`, `xfs` or `f2fs`; anything else reads as `ext4` | `ext4` |
| `swap` | `file`, `partition` or `none`; any other value means `none` | `file` |
| `swap_mb` | Swap size in MiB | `4096` |
| `luks` | `1` for an encrypted root | `0` |
| `lvm` | `1` to put the root on LVM; honoured on the `wipe` plan only | `0` |
| `luks_passphrase` | The encryption passphrase, in plain text | none |
| `hostname` | The machine's name | `kdos` |
| `username`, `fullname` | The account | `kdos`, `KDOS User` |
| `password` | The user's password, in plain text | none |
| `root_locked` | `1` to lock root, `0` to set `root_password` | `1` |
| `root_password` | The root password, in plain text | none |
| `theme` | An accent name, such as `bone` | `bone` |
| `alien_apps` | `1` to copy the live container store, `0` to leave it out | `1` |
| `apps` | Space-separated group ids, such as `essential office` | `essential` |
| `services` | Space-separated names from `networkmanager bluetooth alsa cups sshd`; a service not named does not start | `networkmanager bluetooth alsa` |
| `reboot` | `1` to reboot when an unattended run finishes, `0` to exit | `1` |

Things to watch for:

- Automatic login is off unless you ask for it. The live medium logs in automatically, but an
  answer file without `autologin = yes` installs a machine that asks for a password at `tty1`. The
  install edits the target's `/etc/kdos/login.conf` in place: `autologin = <username>` for on, the
  same line commented out for off.
- `apps` takes groups, not applications. An application id such as `app.krita` matches no group
  and is dropped. If none of the names is a known group, the selection falls back to `essential`
  rather than failing the install over a spelling mistake.
- There is no key for the administrator choice. An account installed from an answer file is
  always in `wheel`.
- `luks = 1` is honoured on the `reuse` plan as well. The wizard draws the checkbox on the erase
  plan only, but an answer file that sets `luks = 1` with `plan = reuse` encrypts the chosen root,
  destroying what was on it.
- `swap = partition` installs no swap on the `reuse` plan; see [5. Layout](#5-layout).

### Passwords

No password is ever written to an answer file. `--save` and the Summary button both leave out
`password`, `root_password` and `luks_passphrase`, and the file's header says so. Adding them
yourself is how an unattended install gets credentials, and it puts a plain-text credential on
whatever medium the file lives on.

Set `password` for an unattended run. Nothing asks for one, and without it the account keeps the
live medium's password entry, under the old name if you also renamed the account.

### What an unattended run checks

An unattended run skips the wizard, and with it every check the pages make: it does not refuse the
boot medium, a disk that is too small, an existing `kdos` volume group on another disk, a missing
passphrase, or a locked root with no administrator. What it checks is what the Prepare step checks
on every install: a disk is named and exists, the installer runs as root, the required tools are
present (including `cryptsetup` for `luks = 1` and `lvm` for `lvm = 1`), and the chosen filesystem's
`mkfs` exists. Check an answer file with `--dump plan` and a `--dry-run` before you trust it with a
disk. `--dry-run` does not change what happens when an unattended run ends: with the default
`reboot = 1`, `kinstall --dry-run --unattended` reboots the live machine once the rehearsal
finishes, so rehearse with `reboot = 0` in the answer file.

Beyond that:

- An unknown filesystem falls back to ext4, and an unknown set of groups falls back to `essential`,
  so a typo in either does not stop the install.
- An unattended run always ends by itself, whether the install worked or failed. It shows the
  Install page for five seconds so a watcher can see what happened, then reboots or exits
  according to `reboot`, under `--dry-run` as well.
- With `reboot = 0` it exits with status 0 when the install worked and 1 when it failed. With
  `reboot = 1` it reboots either way, so set `reboot = 0` when a script needs the result.

## Looking without installing

```sh
kinstall --dry-run                    # log every command, execute none
kinstall --dump probe                 # the machine as the installer sees it
kinstall --dump plan                  # the steps that would run, including the skips
kinstall --dump plan --json           # the same, machine-readable
```

`--dump probe` is the output to paste into a bug report: firmware, processor, memory, the install
size, and every disk, partition and logical volume. Run as root, it first activates every LVM volume
group, so that logical volumes appear.

`--dump plan` uses the same planner as the wizard, so its step list and skips are the real ones:
choosing `reuse` skips the Partition step, the default accent skips the Theme step, and a medium
with no catalogue skips the Packs step. It also prints the `mkfs` command and the root's `fstab`
line the filesystem choice turns into, and the application groups, their route and their estimated
size. It does not probe the disks, so it does not check that the answers fit the target. No password
appears in either dump; the plan says only whether the root account is locked.

`--dry-run` runs the whole wizard and the whole install, logging every command and executing none;
the log is still written. The header shows `DRY RUN` and the Summary button reads
`REHEARSE INSTALL`, so the mode is visible on every page. The one thing a dry run does do is the
unattended ending: combined with `--unattended` and the default `reboot = 1`, it reboots the
machine when the rehearsal finishes.

## What the installer writes

| Where | What |
|---|---|
| The ESP | Formatted FAT32 with the label `KDOS_EFI` (on the reuse plan, only when you asked for it). Limine: both EFI loaders, `EFI/BOOT/BOOTX64.EFI` and `EFI/BOOT/BOOTIA32.EFI`, the BIOS second stage `limine-bios.sys`, and the generated `limine.conf`, all at fixed paths. Slot A's kernel and initramfs in `EFI/kdos/a/`, the A/B boot state `EFI/kdos/bootstate`, memtest86+ as `EFI/kdos/memtest.efi`, and the menu's font and wallpaper (`EFI/kdos/font.bin`, `EFI/kdos/wallpaper.png`) where the medium has them |
| The disk | Limine's BIOS boot code, written by `limine bios-install` |
| Firmware | A boot entry labelled `KDOS`, on a machine booted through UEFI |
| Root | The system, copied from the live medium with `rsync` |
| `/etc/fstab` | Prepended to, never replaced: the root, the ESP at `/boot/efi` (`umask=0077`), and the swap partition or `/swapfile`, above the shipped entries, which include the `/tmp` mount graphical applications depend on |
| `/etc/hostname`, `/etc/hosts` | The hostname, and the `127.0.1.1` line that resolves it locally |
| `/etc/keymap` | The console keymap |
| `/etc/localtime`, `/etc/profile.d/20-timezone.sh` | The time zone, as a link and as `TZ` |
| `/etc/modprobe.d/kdos-regdom.conf` | The zone's country, as the Wi-Fi regulatory domain |
| `/etc/kdos/login.conf` | The `autologin` line, edited in place |
| `/etc/passwd`, `/etc/group`, `/etc/shadow` | The account, renamed and with its password hashed (SHA-512 `crypt`); in `wheel` only if you chose administrator; root locked or with its password |
| `/etc/subuid`, `/etc/subgid` | The account's subordinate ID ranges, under its new name |
| `/etc/service.disabled/` | One flag file per service you turned off |
| `/etc/resolv.conf` | Copied from the live system, so the installed machine resolves names on first boot |
| `/swapfile` | The swap file, when you chose one |
| `/var/lib/kdos/packs` | The pack store, with the `staging` directory an import goes through, when the Packs step runs |
| `/var/lib/kdos/apps-pending` | The application groups your first login offers, on the "recorded" route |

### What is not copied

The copy leaves out the contents of `/dev`, `/proc`, `/sys`, `/tmp`, `/run`, `/mnt` and `/media`
(the directories themselves are created, `/tmp` with mode 1777), `/lost+found`, the installer's own
log, and the live container store when you unticked it. Two files are deliberately left out as well:
`/var/lib/dbus/machine-id` and the SSH host keys in `/etc/ssh/`. Each installed machine makes its
own: the machine ID on first boot, and the host keys the first time the SSH server starts; copied,
every machine installed from one medium would share the same identity and the same host keys.

### The ESP and the bootloader

The kernel and initramfs are copied onto the ESP, and `limine.conf` points at them there. Limine
reads only FAT and ISO9660, not xfs, f2fs or anything encrypted, all of which the installer offers
as a root, so keeping the kernel on the ESP is what keeps every choice bootable. Each root slot
boots its own kernel from its own directory; see [One kernel per
slot](../03-architecture/boot-and-init.md#one-kernel-per-slot). The log records how much space the
ESP has left against what a second slot's kernel needs. The 512 MiB ESP the erase plan creates holds
several; a small ESP kept on the reuse plan may not, and A/B updates are then refused.

`limine.conf` sits at the root of the ESP rather than beside `BOOTX64.EFI`, because that is the one
location Limine searches on both firmware types. Its colours come from the accent you chose.

Both EFI loaders are installed, and firmware runs only the one it can execute: `BOOTIA32.EFI` costs
about a hundred kilobytes and makes the disk start on 32-bit UEFI firmware. If the Limine build has
no 32-bit loader, the install continues without it. The firmware boot entry names one loader, chosen
from `/sys/firmware/efi/fw_platform_size`, and the ESP's partition number, read from the kernel.
Where that number cannot be read, no entry is written and the disk starts through the
removable-media path instead.

The BIOS boot code is written whichever way the installing machine booted. It costs one sector, and
it means a disk installed on a UEFI machine still starts when moved to a legacy one. A failure there
does not stop the install, because on a UEFI machine the EFI path is already complete.

On the reuse plan, the ESP's `EFI/BOOT/BOOTX64.EFI` and `EFI/BOOT/BOOTIA32.EFI` and the disk's BIOS
boot code are replaced with Limine's, and the Limine menu lists only KDOS. Another operating system
on the same disk is started through its own firmware boot entry.

### The boot menu

The installed boot menu counts down for ten seconds before booting `KDOS`. Press any key to stop the
countdown and choose another entry:

| Entry | Boots |
|---|---|
| `KDOS` | The normal system, with a quiet console |
| `KDOS (verbose)` | Every kernel message on the console |
| `KDOS (single user)` | The same as `KDOS (verbose)`. The entry passes `single`, but nothing in KDOS acts on it, so services and the desktop start as usual; KDOS has no single-user mode. For a console without the desktop, log in on `tty2` |
| `Memory Test (memtest86+)` | memtest86+. Shown on UEFI only |

When a second slot exists, `kdos-bootctl` rewrites the menu and adds an entry for it. A machine that
will not boot needs one of these entries, which is why the countdown is long enough to catch.

## See also

- [Getting started](getting-started.md): building and booting the image you are installing from
- [How KDOS is built](../05-developer/how-kdos-is-built.md): how the live image the installer
  copies is produced from source
- [kinstall](../04-programs/kinstall.md): how the installer works inside
- [Administration](administration.md): services, users and storage after the install
- [Boot and init](../03-architecture/boot-and-init.md): A/B slots, encryption and the boot path
- [Applications](applications.md): installing applications after the install
- [Known gaps](../06-reference/known-gaps.md): what has not been booted or is not supported
- [The ports catalogue](../06-reference/ports-catalogue.md): the ports that provide the tools the
  installer checks for, such as `rsync`, `cryptsetup`, `lvm2` and `limine`

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 6.* Previous: [5. Getting started](getting-started.md) · [Contents](../README.md) · Next: [7. The desktop](desktop.md)
