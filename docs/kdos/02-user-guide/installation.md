# Installation

This chapter is for anyone putting KDOS onto a disk. The installer copies the running live system
onto a target disk, configures the copy for its new machine and makes it bootable on both UEFI and
legacy BIOS firmware. It has two faces over one engine. **Install KDOS** is a window on the live
desktop, and is what most people use. `kinstall` is the same installer full-screen on a bare text
console, a terminal window or a serial line, and it installs a machine unattended from an answer
file. Read [Getting started](getting-started.md) first if you do not yet have a booted live image.
Terms this chapter does not define are in the [Glossary](../06-reference/glossary.md).

The chapter follows the order in which you meet things. [Before you start](#before-you-start) lists
what to decide and what the installer needs; [Installing from the desktop](#installing-from-the-desktop)
walks through the window, and [The pages](#the-pages) through `kinstall`, screen by screen. The
sections after that are reference: the filesystem choices, encryption, the A/B root slots, answer
files for unattended installs, the inspection modes, and every file the installer writes. How the
installer is built inside is in [kinstall](../04-programs/kinstall.md).

## Before you start

Nothing reaches your disk until you confirm on the last page before the install, with one
exception: partitioning by hand. **Partition Manager…** in the window, and `Open cfdisk` in
`kinstall`, write a partition table the moment you apply it there. Under `kinstall --dry-run`
the `cfdisk` button is disabled and the page says why, so a rehearsal writes nothing to any disk.
Every other page only records an answer, so going back returns to a page unchanged and quitting
leaves the machine as you found it.

Two answers are worth settling before you begin, because changing either one later means installing
again:

- **The root filesystem**: ext4, btrfs, xfs or f2fs, offered by the window and by `kinstall`
  alike. See [The root filesystem](#the-root-filesystem).
- **Whether the system is encrypted.** Encryption is applied when the partition is created, so it
  cannot be added to or removed from an existing install. See
  [Encrypting the root](#encrypting-the-root).

You need:

- administrator rights: the live account is an administrator, and the window asks nothing more on
  the live medium; `kinstall` runs with `sudo`;
- a target disk that is not the medium you booted from;
- a disk large enough. The Welcome page reports the *install size*, how much the install copies;
  every package is on the medium and every one is copied. A disk smaller than the install plus
  768 MiB is refused. The default layout keeps two copies of the system, each 1.5 times the install
  rounded up to a whole GiB, beside the swap and a home of at least 4 GiB; for an install of 46.5 GiB
  that is about 149 GiB. A disk that holds only one copy is installed with one, and loses the
  ability to undo an update (see [A/B root slots](#ab-root-slots)); a 128 GB disk is one of those.
  The window's Disk page says which beside the disk, and `kinstall --dump probe` prints the
  figures for this medium.

The installer checks each of these and names the one that failed.

The natively built applications are part of the system and are always copied. Applications from
the catalogue are optional, and the installer builds none of them: it records the groups you chose
on the new disk, and your first login there offers to build them (see
[8. Applications](#8-applications)).

## Installing from the desktop

**Install KDOS** is on the live desktop as an icon, on the Welcome tour's live page and in Kickoff
under System. It is a Kirigami window and a front end only: it asks `kinstall` what this machine
holds (`kinstall --dump probe|look|apps|plan --json`), and when you confirm it runs
`pkexec /usr/bin/kinstall --config - --unattended --events json`, writing your answers, passwords
among them, to the installer's standard input and reading its progress back one event at a time.
So the window and `kinstall` cannot disagree about what an install does, and the Summary page shows
`kinstall`'s own plan for your answers. On the live medium a polkit rule lets the live session at
the machine start the installer and Partition Manager without a password; the rule is written onto
the medium's overlay at every live boot and never copied to an installed system.

The pages, in order, listed down the window's side; a page already answered can be returned to by
clicking it:

| Page | Asks |
|---|---|
| Welcome | The time zone, from tzdata's list of zones with UTC first; type part of a name to narrow it (a single match is chosen) and the local time there is shown. Beside it, what the installer measured about this machine: processor, memory, firmware, how many of the disks it found can take the install, and the install size |
| Keyboard | The keyboard layout, found with a search field, and its variant. The console keymap nearest it is chosen for the text consoles |
| Disk | The disk. One that is the boot medium, read-only or too small cannot be chosen and says why on its row; **Look Again** looks for disks again after you attach one. Then the layout: **Erase the disk and install KDOS**, with **Two system slots** ticked and a line saying whether this disk holds two; **Use partitions that already exist**, where you choose the EFI system partition, whether to format it, and the root, which is formatted; or **Partition Manager…**, which opens Partition Manager and, when it closes, comes back to the existing partitions with the disks looked at again. Last the filesystem (ext4, btrfs, xfs or f2fs) and the swap: none, a file or a partition, and its size; the existing-partitions layout offers none or a file |
| Encryption | Whether to encrypt, and the passphrase twice, with a strength hint. On an erased disk everything KDOS writes is inside the container; on partitions you made, the root is |
| Account | Either your full name, user name and password (twice, with a strength hint), whether you are an administrator, and, on an encrypted machine, whether to log in automatically after the disk passphrase; or **let the first person to start it set it up**, which makes no account (see [Preparing a machine for someone else](#preparing-a-machine-for-someone-else)). Then the computer's name, and whether root is locked or has a password of its own. A user name the system already uses, such as `root`, is refused here |
| Look | The accent and the desk, each shown as it will look |
| Applications | The catalogue's groups, recorded for the first login (see [8. Applications](#8-applications)), and whether to keep the live session's box images |
| Summary | Everything above as the installer plans to carry it out, with what is erased in the warning colour, and the button that starts the install. Answers the installer would refuse, a reused root too small for the install among them, are refused here with the reason |

Alt+Left and Alt+Right move back and on, as **Back** and **Next** do; neither starts the install,
which only the **Install…** button and its confirmation do. **Quit** closes the window. The install
page shows each step and its progress; the last page says how it ended, lists any warning and
offers to restart. When the install failed it shows `kinstall`'s reason and the end of the log when
`kinstall` sent one. Whenever `kinstall` ran, **Open the Log** opens `/var/log/kinstall.log` (the
live account, an administrator, may read it). Cancelling the permission prompt ends with "Permission to install
was not given." and no log, because nothing ran. Closing the window while the install waits for
permission cancels it; once the install has started, closing does not stop it, and the window asks
first. Only one installer window opens: starting it again brings the open one forward. Everything
the window does not ask for takes `kinstall`'s default, and the services of [Services](#services)
stay at their defaults. Use `kinstall` for any other choice.

## Running the installer

`kinstall` is the full-screen installer. From a terminal on the live system:

```sh
sudo kinstall
```

On the desktop that opens it inside Konsole. To give it a text console of its own, switch to one
with `Ctrl+Alt+F4` and log in there, or start it on one from Konsole:

```sh
sudo sh -c 'kinstall < /dev/tty4 > /dev/tty4 2>&1'
```

The redirections sit inside the root shell because a virtual console belongs to root: opened by your
own shell before `sudo` runs, they fail with `Permission denied` and the installer never starts.
Choose a console nothing else is on: `tty1` holds the login screen, `tty2` the recovery login, and
a desktop started from the login screen takes `tty3`.

Before it takes over the screen, the installer prints `kinstall: measuring the live system...` and
walks the whole live tree once to measure how much it will copy. On a large live session this takes
a few seconds.

The interface works with both keyboard and mouse. On a bare text console it reads the mouse directly
from `/dev/input`, so clicking works with no mouse daemon. It needs a terminal of at least
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
| `Ctrl+W`, `Ctrl+Backspace` | Delete the word before the caret; `Ctrl+←` / `Ctrl+→` move by word |
| `F1` | Show or hide the key card |
| `L` | Toggle the full log on the Install page; open the log in a pager on the Done page |
| `Ctrl+Q`, `Ctrl+C` | Quit. The installer always asks first, and during an install it warns that the target is left half written |

Pressing `Next` on the Summary page does not start the install; it tells you to press
`BEGIN INSTALL`, so a stray page-forward keystroke cannot erase a disk.

### Command-line options

| Option | Effect |
|---|---|
| `--config FILE` | Read answers from `FILE`; `-` reads them from standard input. See [Installing unattended](#installing-unattended) |
| `--save FILE` | Write the current answers to `FILE` and exit without installing |
| `--unattended` | Skip the wizard and install from the answers loaded with `--config` |
| `--events json` | With `--unattended`: draw nothing, and print one JSON event per line on standard output, as the window reads them |
| `--dry-run` | Run the wizard and the install, logging every install command and executing none. See [Looking without installing](#looking-without-installing) |
| `--dump probe` | Print the machine as the installer sees it, and exit |
| `--dump plan` | Print the steps these answers would run, and exit |
| `--dump apps` | Print the application groups, ticked as the answers have them, and exit |
| `--dump look` | Print the accents and desks the answers may name, and exit |
| `--json` | Print `--dump` output as JSON |
| `--theme NAME` | Draw the installer in this accent and preselect it for the installed system: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect` or `paper` |
| `--no-mouse` | Keyboard only |
| `--ascii` | Draw boxes with `-`, `\|` and `+`, for terminals that cannot show line-drawing characters |
| `--version`, `--help` | Print and exit |

An unknown option prints the usage and exits with status 1, as does a `--config` file that cannot be
read. `--save` and `--dump` act before the screen is taken over and before the root check, so
neither needs root. `--dump` still probes the machine; see
[Looking without installing](#looking-without-installing) for what it can and cannot read without
root.

Every install, including a `--dry-run` rehearsal and every install the window starts, appends to
the log `/var/log/kinstall.log`.

## The pages

`kinstall` has eleven pages, always in this order. The sidebar marks the page you are on and the
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

The first two stop you on this page; the other three only mark a warning. The disk line counts only
disks the installer can write, so a machine whose only disks are read-only stops here; the number of
disks shown above it counts every disk found, read-only ones included. The
install's first step checks `mount`, `umount`, `mkfs.vfat`, `rsync` and the chosen filesystem's
`mkfs` again, and refuses before anything is written; on a machine that booted in legacy BIOS mode
it also checks for the `limine` program. It does not check for the Limine files: if they are
missing, the install writes the disk and then fails at the Bootloader step, leaving a machine that
cannot boot. Do not start an install while that line is marked.

The disk count leaves out loop, RAM, optical, zram, device-mapper, software RAID, floppy and network
block devices.

The firmware line is information, never a refusal. A disk the installer partitions starts on both
firmwares whichever this machine booted as:

- `legacy BIOS`: this machine booted that way. The install writes the BIOS boot code as on any
  machine, and a failure to write it stops the install, because this machine has no other way to
  start the disk. Reusing partitions on a GPT disk needs a BIOS boot partition on it; see
  [The reuse plan](#the-reuse-plan).
- `UEFI  (32-bit firmware)`: a 64-bit processor with 32-bit UEFI firmware, as on some early Atom
  tablets. The firmware boot entry then names the 32-bit EFI loader.
- `(Secure Boot enabled)`: the install completes, but the installed machine will not start until
  you turn Secure Boot off in firmware setup. Limine's EFI loader is unsigned and KDOS enrols no
  keys; see [Known gaps](../06-reference/known-gaps.md#secure-boot-is-not-supported).

### 2. Keyboard

The console keymap, chosen from every map under `/usr/share/keymaps` and `/usr/share/kbd/keymaps`,
with a filter box because the list is long. Moving through the list applies each map with
`loadkeys`, so you can try it in the box beside the list. On an image without `loadkeys` the preview
is skipped, the page says so, and the choice is still saved. The install writes it to `/etc/keymap`,
which `kdos-getty`, the program that runs the login on each text console, loads on every console.

The login screen and the desktop use the same choice. The install leaves the graphical layout
unset, which the machine's locale service, `kdos-settingsd`, reads as "the layout nearest the
console keymap": `uk` becomes `gb`, `sg` becomes `ch`, `dvorak` becomes `us` with the `dvorak`
variant, and most other names are cut to their first two letters. The login screen takes its layout
from that service before anybody types, and every session starts with it. An answer file, or the
window's Keyboard page, can name the graphical layout itself (`xkb_layout`, `xkb_variant`); the
console keymap is then the one nearest it. The layout is changed later on System Settings'
Keyboard page.

### 3. Time

A time zone by name. The list is tzdata's `zone1970.tab`, with a filter box, and the page shows the
current local time in the zone under the cursor. An image with no zone data offers `UTC` alone.

The install records the zone in two places that always agree: `/etc/localtime` becomes a symbolic
link to the zone file, and `/etc/profile.d/20-timezone.sh` exports `TZ=':/etc/localtime'`. The
leading colon makes the C library read that same file, so the link and the variable cannot describe
different rules. The hardware clock is left as it is. If there is no zone file for the name you
picked, the install log says so and the machine keeps UTC. System Settings' Date & Time page, and
`kdos timezone`, change the zone later the same way.

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

How the chosen disk is used: the plan, the root filesystem, swap, the two system slots (on LVM,
Linux's logical volume manager, which divides a partition into resizable volumes) and encryption.

| Plan | What happens |
|---|---|
| Erase the whole disk | A new GPT (GUID partition table): a 512 MiB EFI System Partition (ESP), a 1 MiB BIOS boot partition, a swap partition if you asked for one without the two slots, then the rest |
| Use partitions that already exist | You choose the ESP and the root; every other partition on the disk is left alone and the Partition step is skipped. The root can be a partition or an existing LVM logical volume, and whichever you choose is always reformatted |
| Partition it myself | `Open cfdisk` hands the screen to `cfdisk`. Create an EFI System partition and a Linux partition, and a 1 MiB partition of type *BIOS boot* for the disk to start on legacy BIOS too; write the table and quit. The installer rereads the disk and switches to the reuse plan, where you assign the ESP and the root |

The root filesystem is chosen on both the erase and the reuse plan. A filesystem whose `mkfs` tool
is missing from the image is still listed, labelled `no mkfs on this image`, and the install refuses
it before writing anything.

#### The erase plan

On the erase plan the page offers swap, the two slots and encryption, and draws what will be
written: the partitions and volumes by name and size, as a bar and as a list. The ESP is always
partition 1, the BIOS boot partition 2, a swap partition 3 when there is one, and the rest the last.
The BIOS boot partition holds no filesystem: it is the raw space Limine's legacy-BIOS stage is
written into, and it is what lets the disk start on both firmwares.

#### Swap

Swap is a file on the root filesystem, a partition of its own, or none. The size starts at
4096 MiB, and the page shows how much RAM the machine has beside it. With the two slots, the swap
is a volume of its own that both slots use, whichever of file or partition you picked.

#### LVM

`Two system slots for safe updates (LVM)` is ticked by default. It makes the rest of the disk the
single physical volume of a volume group named `kdos` and lays it out for A/B updates:

| Volume | Holds |
|---|---|
| `kdos/root_a` | Slot A, the system you boot first |
| `kdos/root_b` | Slot B, a copy of slot A, where the first update is written |
| `kdos/swap` | The swap, used by both slots |
| `kdos/home` | Everything left: `/home` for both slots, with the pack store beside the homes |

Each slot is half as large again as the install, rounded up to a whole GiB and never under 8 GiB,
so a release that grows still fits. On a disk where two slots, the swap and a 4 GiB home (plus the
live container store, when it is copied) do not fit, `root_a` takes the whole group with the home
on it, there is no second slot, and the page and the Summary say so in the warning colour. See
[A/B root slots](#ab-root-slots) for what the layout does.

With encryption on as well, the volume group sits inside the encrypted container, so one passphrase
opens both slots, the home and the swap. The page refuses LVM on an image without the `lvm` tool,
and refuses when a volume group named `kdos` already has any part of itself on another disk, because
creating the new group would then fail after this disk had been erased. Rename the old group with
`vgrename` or untick the slots. Unticked, the erase plan makes one plain root partition. How the
installed machine finds a root on a logical volume at boot is in
[Activating volume groups](../03-architecture/boot-and-init.md#activating-volume-groups).

#### Encryption

`Encrypt the root filesystem (LUKS2)` puts LUKS2, the standard Linux disk-encryption format, on the
root (on the erase plan with the two slots, on the whole volume group). The checkbox is drawn on the
erase plan only, because encrypting a partition destroys what is on it and the reuse plan exists for
people who are keeping something. Tick it and two passphrase fields appear; they must match, and an
image without `cryptsetup` is refused on this page.

The tick is kept if you then switch to the reuse plan, or pick *Partition it myself*, which switches
to the reuse plan after `cfdisk`. The root you choose there is then encrypted, destroying what was
on it, although the reuse plan shows no checkbox; the Summary's `encryption` line says so. Untick
encryption on the erase plan before you switch.

#### The reuse plan

On the reuse plan the page shows two lists, the ESP and the root, and a checkbox to reformat the ESP
as FAT32. The checkbox starts ticked; untick it when another operating system boots from the same
ESP, or reformatting erases that system's boot files. The root list shows
the disk's partitions, then every LVM logical volume on the machine, named `vg/lv`. The installer
activates every volume group when it starts, so volumes on this disk appear. Thin and cached volumes
are offered as ordinary volumes, and the initramfs carries `thin_check` and `cache_check` for them;
their internal volumes are not listed. A plain LVM root has been booted in a virtual machine; an
encrypted, thin or cached one has not; see [Known
gaps](../06-reference/known-gaps.md#a-root-filesystem-on-lvm-has-been-booted-in-one-shape-only).

The reuse plan refuses as the root:

- the ESP you chose, or any partition typed as an EFI system partition;
- an LVM physical volume (choose one of its logical volumes instead);
- a partition or volume that something else holds open, such as an open encrypted container or an
  active volume group;
- a partition smaller than the install.

It refuses the disk's BIOS boot partition as either the ESP or the root, an ESP something holds
open, and an ESP you chose not to reformat that holds no FAT filesystem. The window's lists offer
only what these rules allow: an ESP is a partition typed as one or holding FAT, and the root list
leaves out ESPs, the BIOS boot partition and physical volumes. These rules are `kinstall`'s own,
asked by the page, by the window's Summary, by an unattended run and by the install's first step,
so no route takes a target another refuses.

Under the lists the page says which firmwares the disk will start on. A GPT disk with a partition of
type *BIOS boot* gets Limine's legacy-BIOS stage in it, and a disk with an MBR (dos) partition table
gets it after the boot sector; on either the disk starts on UEFI and on legacy BIOS. A GPT disk with
no BIOS boot partition starts on UEFI only, because this plan makes no partition: on a UEFI machine
the page says so, and on a machine that booted in legacy BIOS mode the page refuses the plan,
because the result could not start there. Add a 1 MiB BIOS boot partition with *Partition it
myself*, or erase the disk.

The reuse plan installs one slot. A software RAID array and an opened LUKS container are not offered
as the root. In `kinstall` the reuse plan shows no swap choice; the swap setting it installs with is
the one the erase plan last showed, which is a 4096 MiB swap file unless you changed it. A swap
*partition* is only created by the erase plan, so on the reuse plan that setting installs no swap at
all, and the Summary and `--dump plan` say `none`. The window's existing-partitions layout offers
only no swap or a file, and turns a swap partition chosen before into a file.

### 6. Accounts

- **Hostname**: letters, digits and dashes.
- **Who sets it up**: *Set up my account now*, the default, or *Let the first person to start it
  set it up (OEM)*, which makes no account and hides the fields below; see
  [Preparing a machine for someone else](#preparing-a-machine-for-someone-else).
- **Full name and user name**: the user name is lowercase letters, digits, `-` and `_`, and does
  not start with a digit. The full name may not hold a colon or a control character.
- **Password**: required, typed twice, with a strength meter.
- **Administrator**: ticked by default. Ticked, the account is in the `wheel` group and can use
  `sudo` and the administrator's actions in System Settings and Discover. Unticked, the account is
  taken out of `wheel`; it still has the desktop, and can suspend, power off, reboot, connect to
  networks and mount removable media, because those are granted to whoever is at the machine
  rather than to a group.
- **Login**: on an encrypted root, *Log in automatically after the disk passphrase*, and under it
  *Start the session locked (unlock with this password)*, ticked by default. On a plain root the
  page says the login screen asks for the password, and offers nothing.
- **Lock the root account**: ticked by default. Unticked, you set a root password, typed twice.

Root locked together with a non-administrator account would leave nobody able to gain privileges,
so `Next` refuses that combination.

The login screen asks for your password on every installed machine but one: an encrypted machine
whose account chose automatic login. The passphrase typed at boot already asks who is at the
machine, so the session then starts by itself, and with *Start the session locked* it starts behind
the lock screen, which takes the account's password. The choice is written to
`/etc/sddm.conf.d/kde_settings.conf`, the file System Settings' Login Screen page edits too, so
either can change it later. With automatic login the password store, KWallet, has no login
password to open with, and asks for its own the first time a program stores a secret.

The serial console `ttyS0` asks for a login too: the live image logs root in there with no
password, and the installer removes that from the installed copy of `/etc/inittab`. The live
image's root password is never carried over: the root entry in `/etc/shadow` is always rewritten,
locked or with the password you set, and the backup copies `/etc/passwd-`, `/etc/shadow-` and their
kin are not copied.

A user name other than `kdos` is a real rename. The installer rewrites `/etc/passwd` (name, full
name and home directory), `/etc/group` (the membership lists and the primary group's name),
`/etc/shadow`, and the owner of the `/etc/subuid` and `/etc/subgid` ranges that rootless containers
use. It moves `/home/kdos` to the new name.

### 7. System

Four settings: the accent, the desk, the live container store, and which services start at boot.

#### Accent

There are eight accents, and the installer repaints itself in the one you select, so you choose by
colour rather than by name. The installed machine's desktop, login screen, lock screen, splash,
boot menu and text consoles all start in it. See [Theming](theming.md).

#### Desk

How the first login lays out its panels: *KDOS*, one bottom panel with the menu, the tasks, the
meters, the tray and the clock; *Dock*, a top bar and a floating dock; or *Tiling*, a top bar, with
windows tiled as they open. See [The desks](desktop.md#the-desks); every account can change it
later.

#### The alien app library

The checkbox is `Install the alien app library`. An *alien app* is a graphical application that KDOS
does not compile itself and runs in a container instead; see
[Applications](applications.md#what-an-alien-app-is). This checkbox, shown with its size, decides
whether the live account's container store, `/home/kdos/.local/share/containers` (the images Podman
keeps), is copied to the disk. The medium ships no prebuilt container images, so the store holds
only what the live session has itself pulled or built, often nothing. Leaving it out does not stop
you installing applications later. The natively built applications are part of the system itself
and are copied with it whatever this checkbox says.

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

This page is about boxed applications only. The applications KDOS compiles natively are part of
the system the installer copies, so they are on the new disk whatever you choose here; see
[Applications](applications.md#native-applications).

The application groups this medium's catalogue can build, with `essential` already ticked. The
catalogue is `/usr/share/kdos/appstore/catalogue`, or the boot medium's
`/mnt/iso/appstore/catalogue` where the live root has none; the groups and their descriptions come
from it, and a group none of whose members is in the catalogue is not shown. The shipped catalogue
has seven. `essential` holds Zathura.

| Group | Description | Applications |
|---|---|---|
| `essential` | What a new machine starts with | 1 |
| `office` | Documents, spreadsheets and reading | 4 |
| `creative` | Images, audio and video | 1 |
| `dev` | Programming and electronics | 4 |
| `science` | Computation, modelling and data | 6 |
| `make` | Three dimensions: model it, slice it, cut it | 1 |
| `games` | Games and emulators | 1 |

`Space`, `Enter` or a click toggles a group. The size total under the list updates as you go and is
labelled an estimate: each group counts its own members once and no shared runtime, and an
application in two ticked groups is counted in both (Zathura is in `essential` and in `office`).
The Summary's application count is summed the same way. The shared runtimes are not choices; each
application brings the one it is built on.

The page also says how the applications arrive, which matters more than the size: the installer
builds none of them. The groups you tick are recorded in `/var/lib/kdos/apps-pending` on the new
disk. Building needs the container engine and a network on the installed machine, and the
installer's own `kdos-appbox` would write into the live session's pack store and container
storage, which end with the live medium.

Your first desktop login shows a notification offering to build the recorded groups;
`kdos app install --pending` builds them from a terminal. The offer is made once per user, marked
by `~/.config/kdos/apps-offered`; delete that file and the offer returns at the next login.
Building takes minutes per application, which is why this waits for you rather than starting a
build unannounced at first boot. An exported set (a `.ktar` file) is imported on the installed
machine with `kdos-appbox import`; see [Applications](applications.md#carrying-a-set-to-another-machine).

On a medium with no catalogue the page says so and there is nothing to choose; install applications
later from Discover.

### 9. Summary

Your answers on one screen: keymap, time zone, hostname, the user or the OEM choice, the login,
root account, accent, desk, whether the alien app library is copied, the application groups and
their route, the enabled services, the target disk, the plan, the root and ESP on the reuse plan,
the slot layout when there is one (`slots A and B of N each, /home shared`, or `one slot` in the
warning colour), which firmwares the disk will start on (`UEFI and legacy BIOS`, or `UEFI only` for
a reused GPT disk with no BIOS boot partition), and swap. Encryption is part of the LVM line when
there is one; otherwise, whenever encryption is on, on either plan, an `encryption` line reads
`root inside LUKS2`. The destructive parts are in the error colour, and a rule across the page reads
`EVERYTHING ABOVE THIS LINE IS STILL REVERSIBLE`. Two buttons sit under it:

- `BEGIN INSTALL`: starts writing to the disk. Under `--dry-run` it reads `REHEARSE INSTALL`.
- `Save answer file`: writes your answers to `/tmp/kinstall.conf` and installs nothing.

`BEGIN INSTALL` starts the install in the background and moves to the Install page, which has no
way back: the Summary and its button are not reachable again while the install runs. `Ctrl+Q`
warns that the target would be left half written while the install is running. The log,
`/var/log/kinstall.log`, records every command as it runs and, when a step fails, the reason; it can
be followed from another console with `sudo tail -f /var/log/kinstall.log`. It is root's and the
`wheel` group's (mode `0640`) and holds no password.

### 10. Install

The page an install runs on, interactive or unattended (see [Installing
unattended](#installing-unattended)): the work, with an overall progress bar, one line per step and
the tail of the log. The install runs in a separate process, so the screen stays responsive during a
multi-gigabyte copy. Each step is marked as it starts, finishes, is skipped (`not needed`) or fails;
the two copy steps show the amount copied out of the whole, the speed and the time remaining. `L` switches to the
full log, where `PgUp` and `PgDn` scroll. The window's install page shows the same steps.

| Step | What it does | Skipped when |
|---|---|---|
| Prepare | Checks the tools and the target, stops swap on disks (the compressed swap in memory stays on), unmounts the target and anything on the disk, and on the erase plan takes down volume groups and containers on it | |
| Partition | Wipes the disk's signatures, writes the GPT layout and zeroes the BIOS boot partition | The plan is not "erase" |
| Format | Encrypts and creates the volume group and its volumes if asked, then makes the root filesystem, slot B's and the home's, the ESP (when it is to be formatted) and the swap | |
| Mount | Mounts the root at `/mnt/kinstall`, the home at `/mnt/kinstall/home` with the pack store bound under it, and the ESP at `/mnt/kinstall/boot/efi`; the boot medium at `/mnt/iso` and any stick under `/mnt` stay reachable beside it | |
| Copy system | Copies the live system with `rsync`, leaving out the live session (see [What is not copied](#what-is-not-copied)). The installer and everything it starts run at `oom_score_adj` -900, so the memory guard, `kdos-oomd`, passes over the copy however long it stalls memory | |
| Packs | Records the applications you chose for the first login | Nothing was chosen, or the medium has no catalogue |
| Configure | `fstab`, hostname, `/etc/hosts`, time zone, Wi-Fi country, keymap and keyboard layout, the login screen's automatic login, the serial console's login, DNS resolver, services and the swap file | |
| Accounts | Users, passwords, the administrator choice, and the new home filled from `/etc/skel`; on an OEM install, the live account removed instead | |
| Theme | The accent and the desk the first login takes, the accent of the splash and the consoles, and the login screen's look | |
| Slot B | Copies slot A, as configured, into slot B, with slot B's own root in its `fstab` | The layout has one slot |
| Bootloader | Limine on the ESP, the BIOS boot code into the BIOS boot partition, and the firmware boot entry | |
| Finish | Flushes and unmounts | |

A failed step stops there and offers `Retry step`, `View log`, `Shell` and `Abort`. `Shell` opens a
login shell on the console; a fixable failure, such as a missing tool or a device that went away,
can be fixed there and the install continued from the failed step with `Retry step`. `Abort` leaves
the installer. When every step has finished, `Continue` appears and leads to the Done page; an
unattended run instead ends by itself after five seconds.

A warning is something the installed system will lack while the install carries on: no
`BOOTIA32.EFI` for 32-bit UEFI, no BIOS boot code (on a UEFI machine: a reused GPT disk with no
BIOS boot partition, no `limine` program, or a failed `limine bios-install`), no partition number
for the firmware boot entry, an ESP too small for a second slot's kernel, or a choice of
applications that could not be recorded. Each is listed under `WARNINGS` as it arrives and written to the
log with a `warning:` prefix.

### 11. Done

What was installed, where, and how long it took, the warnings again under `FINISHED WITH WARNINGS`
when there were any, and `Reboot now` (which asks first), `Back to the live desktop` and `View
log`; `L` opens the same log pager. Remove the boot medium before restarting, or the firmware may
start the live image again. An unattended run does not show this page.

## Preparing a machine for someone else

*Let the first person to start it set it up*, or `oem = 1` in an answer file, installs a machine
with no account. The live account is removed, root stays locked unless you gave it a password on
the Account page (or `root_password` in an answer file), and `/etc/plasma-setup-done` is
left absent. At every boot while that file is absent, the boot step `62_plasma-setup` arms Plasma's
first-start wizard, plasma-setup: the login screen logs its own account in, and the wizard asks the
person who switched the machine on for the language, keyboard, time zone and network and makes their
account, an administrator. It then writes `/etc/plasma-setup-done`, and from the next boot the
machine shows the ordinary login screen. Every install that makes an account writes the file, so the
wizard never appears there.

## The root filesystem

The installer offers four root filesystems. The one you pick sets the `mkfs` command, the mount
options, the `fstab` line and how the swap file is made, together, so they cannot disagree. The root
filesystem is always labelled `KDOS`, and the home volume of the two-slot layout takes the same
filesystem.

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

Ticking encryption puts LUKS2 on the root partition, or on the whole volume group of the two-slot
layout. There is no recovery key and no way into the machine without the passphrase, which is asked
for at every boot.

The installer runs `cryptsetup luksFormat --type luks2` and then `cryptsetup open`, feeding the
passphrase on standard input both times. Passing it as an argument would expose it, through
`/proc/<pid>/cmdline`, to every process on the machine while `cryptsetup` runs. The opened container
is `/dev/mapper/kdosroot`, and everything after that (the `mkfs`, the mount, the `fstab` entry and
the copy) works on that one name. With LVM on as well, the container is the volume group's
physical volume instead, and those steps work on the logical volumes inside it.

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

An installed KDOS machine keeps two root filesystems, *slot A* and *slot B*, so that an update is
written into the slot you are not running and a version that fails to start is rolled back.

On the default layout the installer makes both. Slot A is the system it copied and configured; the
Slot B step then copies slot A, as it stands after the Configure, Accounts and Theme steps, into
slot B, rewriting only the root line of slot B's `fstab`. Both slots therefore hold the same system
when the machine first boots, and the first update has a slot to go into. The installer writes the
initial boot state to the ESP (`EFI/kdos/bootstate`): both slots, each slot's encrypted container (if
any) recorded beside it, and `bootstate=UUID=<esp>` added to the kernel options. Each slot's kernel
and initramfs go into their own ESP directory, `EFI/kdos/a/` and `EFI/kdos/b/`, and each menu entry
names its slot with `kdos_slot=a` or `kdos_slot=b`.

What the slots share and what they do not:

- **`/home` is one volume both slots mount**, so your files are the same whichever slot is
  running. The pack store lives there too, as `/home/.kdos/packs`, bound onto `/var/lib/kdos/packs`
  by a line in each slot's `fstab`, so packs and the boxes built on them are never inside a slot and
  survive every switch.
- **`/etc` and `/var` belong to each slot.** Sharing them would share init scripts and package
  records across two versions and break the rollback. An update carries this machine's state into
  the new slot when it is armed: a configuration file you have not edited takes the new version's
  default, one you edited keeps your copy (the new default is left beside it as `<file>.kdos-new`),
  the account databases are merged by name, and machine state such as the network connections,
  Bluetooth pairings, the login screen's state and the crontabs follows.

Updates come through Discover or `kdos update`: the next release is written into the inactive slot,
its kernel put in that slot's ESP directory, and the slot marked to be tried; see
[Keeping it current](administration.md#keeping-it-current). A trial works like this:

1. On UEFI, the candidate slot is booted once through the firmware's `BootNext` setting. Every boot
   after that starts the confirmed slot, so even a kernel that crashes before its initramfs is
   rolled back with no help. On BIOS, or where the firmware setting cannot be written, the boot
   menu leads the trial instead, for up to three boots.
2. The initramfs counts the attempt before anything is mounted.
3. The slot is promoted to confirmed only when the boot scripts have reached their end *and* a
   Plasma session has run for a minute without the shell restarting, or, on a machine nobody logs
   in to, the login screen has been up for a minute.

System Settings' System Versions page lists both slots and starts the other one on the next boot,
which is how you go back to the previous version by hand.

Two slots need a disk of two slots (each 1.5 times the install, rounded up to a whole GiB, at least
8 GiB), the swap, a 4 GiB home, the ESP and the BIOS boot partition: about 149 GiB for an install of
46.5 GiB. `kinstall --dump probe` prints the figure for the medium in hand (`ab_need_bytes`), and the
window's Disk page says beside the disk whether it holds two. A disk too small for two slots, the
reuse plan, and an install with the slots unticked all get one slot. Such a machine has nowhere to write an update but over itself: `kdos update apply --in-place`
does that, and gives up the rollback. A second slot can be made by hand later: make a filesystem
for it, copy the system into it, and record it with `kdos-bootctl set-slot b <filesystem-uuid>
[<luks-uuid>]`. The second UUID is needed only when slot B is encrypted: it names the LUKS
container, and the first names the filesystem inside it. `blkid` gives both. `set-slot` with fewer
arguments prints its usage and exits with status 2.

Recording a container per slot is what lets A/B work with encryption. The kernel command line can
name only one `cryptdevice=`, so the initramfs looks up the chosen slot's container after it has
picked the slot. [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection) describes
the whole mechanism.

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
hand it back with `--config`. `--config -` reads the answers from standard input, which is how the
desktop's installer hands them over.

### The keys

Every key the installer reads is below. [kinstall](../04-programs/kinstall.md#answer-files) gives
the same keys with what each one writes and which plan honours it. A key not in this table is
named on standard error (`unknown key '…' ignored`) and has no effect, so a misspelt key installs
that setting's default.

| Key | Value | Default |
|---|---|---|
| `keymap` | A console keymap name, as `/etc/keymap` holds it | `us` |
| `xkb_layout`, `xkb_variant` | The graphical keyboard layout and variant. With a layout and no `keymap`, the console keymap is the one nearest it | none: the layout nearest the keymap |
| `timezone` | The `TZ` value. The wizard always writes `:/etc/localtime` | `:/etc/localtime` |
| `timezone_label` | The zone name `/etc/localtime` links to, such as `Europe/Berlin` | `UTC` |
| `disk` | The whole-disk device, such as `/dev/sda` | none |
| `plan` | `wipe`, `reuse` or `manual`; anything else reads as `wipe`. An unattended run has no `cfdisk` step, so `manual` installs exactly as `reuse` does | `wipe` |
| `esp`, `root` | On the `reuse` plan: the ESP partition, and the root as a partition or a logical volume (`/dev/<vg>/<lv>`) | none |
| `format_esp` | `1` to format the ESP as FAT32, `0` to keep it. It applies on every plan: `0` keeps an existing ESP on the `reuse` plan, and leaves the new ESP unformatted on the `wipe` plan, where the install then fails at the Mount step | `1` |
| `fstype` | `ext4`, `btrfs`, `xfs` or `f2fs`; anything else reads as `ext4` | `ext4` |
| `swap` | `file`, `partition` or `none`; any other value means `none` | `file` |
| `swap_mb` | Swap size in MiB | `4096` |
| `luks` | `1` for an encrypted root | `0` |
| `lvm` | `1` for the two-slot layout on LVM, `0` for one plain root; honoured on the `wipe` plan only | `1` |
| `luks_passphrase` | The encryption passphrase, in plain text | none |
| `hostname` | The machine's name | `kdos` |
| `oem` | `yes` or `1` to make no account and let the first person to start the machine set it up | off |
| `username`, `fullname` | The account | `kdos`, `KDOS User` |
| `password` | The user's password, in plain text | none |
| `admin` | `yes` or `1` for an administrator (in `wheel`); anything else for an account outside it | `yes` |
| `autologin` | `yes` or `1` for automatic login after the disk passphrase. Refused without `luks = 1`, and with `oem` | off |
| `autologin_lock` | With `autologin`: `yes` or `1` to start the session behind the lock screen | `yes` |
| `root_locked` | `1` to lock root, `0` to set `root_password`. `0` with no `root_password` is refused | `1` |
| `root_password` | The root password, in plain text | none |
| `theme` | An accent name, such as `bone` | `bone` |
| `desk` | `kdos`, `dock` or `tiling` | `kdos` |
| `alien_apps` | `1` to copy the live container store, `0` to leave it out | `1` |
| `apps` | Space-separated group ids, such as `essential office`, or `none` | `essential` |
| `services` | Space-separated names from `networkmanager bluetooth alsa cups sshd`; a service not named does not start | `networkmanager bluetooth alsa` |
| `reboot` | `1` to reboot when an unattended run finishes, `0` to exit | `1` |

Things to watch for:

- Automatic login is off unless you ask for it, and an answer file that asks for it without an
  encrypted root is refused. The live medium logs in automatically; an installed machine shows the
  login screen.
- `apps` takes groups, not applications. An application id such as `app.scribus` matches no group
  and is dropped. If none of the names is a known group, the selection falls back to `essential`
  rather than failing the install over a spelling mistake.
- `luks = 1` is honoured on the `reuse` plan as well. The wizard draws the checkbox on the erase
  plan only, but an answer file that sets `luks = 1` with `plan = reuse` encrypts the chosen root,
  destroying what was on it.
- `swap = partition` installs no swap on the `reuse` plan; see [5. Layout](#5-layout).

### Passwords

No password is ever written to an answer file. `--save` and the Summary button both leave out
`password`, `root_password` and `luks_passphrase`, and the file's header says so. Adding them
yourself is how an unattended install gets credentials, and it puts a plain-text credential on
whatever medium the file lives on. The desktop's installer never writes a file: it hands its
answers to `kinstall` on standard input.

Set `password` for an unattended run that makes an account. Nothing asks for one, and the answers
check refuses an account with none.

### What an unattended run checks

An unattended run skips the wizard, but not the two checks every route into an install shares, so
nothing is written when either refuses. The answers check refuses an accent or desk that does not
exist, a hostname, user name, full name or time zone the pages would reject, a user name the
system already uses, a missing or mismatched password or passphrase, a reuse plan with no ESP or
root, an unlocked root with no root password, a locked root with no administrator, and automatic
login without an encrypted root. The target check then holds the answers against the machine: it
refuses a disk that is not there, is the boot medium, is read-only or is smaller than the install
plus 768 MiB; encryption without `cryptsetup` and LVM without `lvm` on the image; a volume group
named `kdos` with any part of itself on another disk; on the reuse plan every ESP and root rule
of [The reuse plan](#the-reuse-plan); and on a machine booted in legacy BIOS mode a reused GPT disk
with no BIOS boot partition. The wizard's pages, the window's Summary, `--dump plan` and the
Prepare step of every install ask both. The Prepare step also checks that the installer runs as
root and that the required tools are present (`mount`, `umount`,
`mkfs.vfat`, `rsync`, the chosen filesystem's `mkfs`, and `limine` on a machine booted in legacy
BIOS mode). Check an answer file with `--dump plan` and a `--dry-run` before you trust it with a
disk. `--dry-run` does not change what happens when an unattended run ends: with the default
`reboot = 1`, `kinstall --dry-run --unattended` reboots the live machine once the rehearsal
finishes, so rehearse with `reboot = 0` in the answer file.

Beyond that:

- An unknown filesystem falls back to ext4, and an unknown set of groups falls back to `essential`,
  so a typo in either does not stop the install.
- An unattended run always ends by itself, whether the install worked or failed. It shows the
  Install page for five seconds so a watcher can see what happened, then reboots or exits
  according to `reboot`, under `--dry-run` as well. With `--events json` it draws nothing and
  prints its progress instead.
- With `reboot = 0` it exits with status 0 when the install worked and 1 when it failed. With
  `reboot = 1` it reboots either way, so set `reboot = 0` when a script needs the result.

## Looking without installing

```sh
kinstall --dry-run                    # log every command, execute none
kinstall --dump probe                 # the machine as the installer sees it
kinstall --dump plan                  # the steps that would run, including the skips
kinstall --dump plan --json           # the same, machine-readable
```

`--dump probe` is the output to paste into a bug report: firmware, processor, memory, whether this
is the live session, the install size, what a disk must hold for the install and for two slots,
and every disk, partition and logical volume, with the reason a disk would be refused and whether
it gets only one slot. Run as root, it first activates every LVM volume group, so that logical
volumes appear. Run without root, it reads what it may not open from udev's database, so the
filesystem types, labels and the ESP and BIOS boot marks are still shown.

`--dump plan` probes the machine as the install does (the firmware, the disks and the install
size) and uses the same planner as the wizard, so
its step list and skips are the real ones: choosing `reuse` skips the Partition and Slot B steps, a
disk too small for two slots skips Slot B, and a medium with no catalogue skips the Packs step. It
names the ESP, the root and the BIOS boot partition the install will use, or says the disk will
start on UEFI only, and on LVM whether there are two slots and how large each is. It also prints the
`mkfs` command and the root's `fstab` line the filesystem choice turns into, and the application
groups, their route and their estimated size. Its `refused` line is the first refusal of the
answers check or the target check, so it says whether the disk is large enough, is the boot medium
or is read-only, and whether a reused ESP and root are acceptable. No password appears in either
dump; the plan says only whether the root account is locked.

`--dry-run` runs the whole wizard and the whole install, logging every install command and
executing none; the log is still written. What only a real run learns, such as the new root's UUID,
is logged as not read, and what the copy would carry, such as the login screen's set for the
accent, is looked up on the live system. The header shows `DRY RUN` and the Summary button reads
`REHEARSE INSTALL`, so the mode is visible on every page. `Open cfdisk` on the Layout page is
disabled, since `cfdisk` writes its table itself; the page says so and points to the reuse plan,
which rehearses with the partition table as it is. The one thing a dry run does do is the
unattended ending: combined with `--unattended` and the default `reboot = 1`, it reboots the
machine when the rehearsal finishes.

## What the installer writes

| Where | What |
|---|---|
| The ESP | Formatted FAT32 with the label `KDOS_EFI` (on the reuse plan, only when you asked for it). Limine: both EFI loaders, `EFI/BOOT/BOOTX64.EFI` and `EFI/BOOT/BOOTIA32.EFI`, the BIOS second stage `limine-bios.sys`, and the generated `limine.conf`, all at fixed paths. Each slot's kernel and initramfs in `EFI/kdos/a/` and `EFI/kdos/b/`, the A/B boot state `EFI/kdos/bootstate`, memtest86+ as `EFI/kdos/memtest.efi`, and the menu's font and wallpaper (`EFI/kdos/font.bin`, `EFI/kdos/wallpaper.png`) where the medium has them |
| The disk | Limine's BIOS boot code: its boot sector in the disk's first sector, and its second stage in the BIOS boot partition (on a dos partition table, in the gap after the boot sector) |
| Firmware | A boot entry labelled `KDOS`, on a machine booted through UEFI |
| Root | The system, copied from the live medium with `rsync`; with two slots, slot B is a copy of slot A |
| `/etc/fstab` | Prepended to, never replaced: the root, the ESP at `/boot/efi` (`umask=0077`), the shared `/home` and the bind of `/home/.kdos/packs` onto `/var/lib/kdos/packs` on the two-slot layout, and the swap volume, partition or `/swapfile`, above the shipped entries, which include the `/tmp` mount graphical applications depend on |
| `/etc/hostname`, `/etc/hosts` | The hostname, and the `127.0.1.1` line that resolves it locally |
| `/etc/keymap`, `/etc/kdos/xkb.conf`, `/etc/xdg/kxkbrc` | The console keymap; the graphical layout, when one was named, for the login screen and System Settings |
| `/etc/localtime`, `/etc/profile.d/20-timezone.sh` | The time zone, as a link and as `TZ` |
| `/etc/modprobe.d/kdos-regdom.conf` | The zone's country, as the Wi-Fi regulatory domain |
| `/etc/sddm.conf.d/kde_settings.conf` | The `[Autologin]` group: the account, on an encrypted machine that asked for it; otherwise none, so the login screen is shown |
| `/etc/kdos/autologin-xdg/kscreenlockerrc` | With automatic login and *Start the session locked*: the setting that starts that session behind the lock screen, read by an automatic login alone |
| `/etc/plasma-setup-done` | Written by every install that made an account; left absent by an OEM install, which arms plasma-setup |
| `/etc/inittab` | The serial console's `S0` line, with its automatic root login removed; every other line as shipped |
| `/etc/xdg/kdeglobals` | The global theme of the chosen accent, `org.kdos.<accent>.desktop`, which every new account's first login applies |
| `/etc/xdg/kdosrc` | The desk the first login of every account lays out |
| `/etc/kdos/accent`, `/etc/vtrgb` | The accent of the boot splash and the sixteen colours of the text consoles |
| The `sddm` account's home | The login screen's colours, style and pointer for the chosen accent |
| `/etc/passwd`, `/etc/group`, `/etc/shadow` | The account, renamed and with its password hashed (SHA-512 `crypt`); in `wheel` only if you chose administrator; root locked or with its password. On an OEM install, the live account removed |
| `/etc/subuid`, `/etc/subgid` | The account's subordinate ID ranges, under its new name |
| The new home directory | Filled from `/etc/skel`, as a new account's is |
| `/etc/service.disabled/` | One flag file per service you turned off |
| `/etc/resolv.conf` | Copied from the live system, so the installed machine resolves names on first boot |
| `/swapfile` | The swap file, when you chose one on a one-slot layout |
| `/var/lib/kdos/packs` | The pack store, with the `staging` directory an import goes through and the `mnt` directory packs are mounted under; on the two-slot layout it is `/home/.kdos/packs`, bound there |
| `/var/lib/kdos/apps-pending` | The application groups you chose, which your first login offers to build |

### What is not copied

The copy leaves out the contents of `/dev`, `/proc`, `/sys`, `/tmp`, `/run`, `/mnt` and `/media`
(the directories themselves are created, `/tmp` with mode 1777), `/lost+found`, and the live
container store when you unticked it. The live session stays on the medium too: every file under
`/var/log` and `/var/cache` (the directories are kept), the installer's own log among them, so the
installed system's log and `last` start at its first boot; root's shell and atuin history and its
caches; and the whole live home, `/home/kdos`, apart from the container store when it is copied and
the `bat` cache the image ships. The live home holds the desktop that ran the install: its panels,
its wallet, its file index, the Welcome tour marked as seen. Carried over, the installed account
would find the desk already laid out and the tour already shown, so the Accounts step fills the new
home from `/etc/skel` instead. The **Install KDOS** entry and the live session's installer polkit
rule are left out, so an installed system never offers to install itself over its own disk. Two
files are deliberately left out as well: `/var/lib/dbus/machine-id` and the SSH host keys in
`/etc/ssh/`. Each installed machine makes its own: the machine ID on first boot, and the host keys
the first time the SSH server starts; copied, every machine installed from one medium would share
the same identity and the same host keys.

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

The BIOS boot code is written whichever way the installing machine booted, so one disk starts on
both firmwares: `limine bios-install <disk> 2` puts Limine's second stage into the erase plan's BIOS
boot partition, and Limine then finds `limine-bios.sys` and `limine.conf` at the root of the ESP.
On a machine that booted in legacy BIOS mode a failure here stops the install, because that machine
has no other way in; on UEFI it is a warning on the Install and Done pages, and Limine's own
`error:` lines are in `kinstall.log`. A disk laid out this way has started Limine and its kernel
on SeaBIOS in the rig; a whole installed system has not yet been booted on legacy BIOS.

On the reuse plan, the ESP's `EFI/BOOT/BOOTX64.EFI` and `EFI/BOOT/BOOTIA32.EFI` are replaced with
Limine's, as is the disk's BIOS boot code where the disk has room for it (a BIOS boot partition, or
a dos partition table), and the Limine menu lists only KDOS. Another operating system on the same
disk is started through its own firmware boot entry.

### The boot menu

The installed boot menu counts down for ten seconds before booting `KDOS`. Press Up or Down to stop
the countdown and choose another entry; Enter, Space or Right boots the highlighted entry at once:

| Entry | Boots |
|---|---|
| `KDOS` | The normal system, with a quiet console |
| `KDOS (verbose)` | Every kernel and boot message on the screen, no splash. On an encrypted root the passphrase prompt is printed on the screen rather than drawn by the splash |
| `KDOS (single user)` | Described in the menu as *A root shell for root's password, no desktop*. A verbose boot to single-user mode: the services start, then root's password is asked for on the console and a root shell opens, with no desktop and no other consoles. Leaving the shell continues to the normal system. See [Single-user and emergency mode](../03-architecture/boot-and-init.md#single-user-and-emergency-mode) |
| `Memory Test (memtest86+)` | memtest86+. Shown on UEFI only |

With two slots, `kdos-bootctl` writes an entry for the other slot as well, so the previous version
can be booted from the menu. A machine that will not boot needs one of these entries, which is why
the countdown is long enough to catch.

## See also

- [Getting started](getting-started.md): building and booting the image you are installing from
- [How KDOS is built](../05-developer/how-kdos-is-built.md): how the live image the installer
  copies is produced from source
- [kinstall](../04-programs/kinstall.md): how the installer works inside, and both of its front ends
- [Administration](administration.md): services, users, storage and updates after the install
- [Boot and init](../03-architecture/boot-and-init.md): A/B slots, encryption and the boot path
- [Applications](applications.md): installing applications after the install
- [Known gaps](../06-reference/known-gaps.md): what has not been booted or is not supported
- [The ports catalogue](../06-reference/ports-catalogue.md): the ports that provide the tools the
  installer checks for, such as `rsync`, `cryptsetup`, `lvm2` and `limine`

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 6.* Previous: [5. Getting started](getting-started.md) · [Contents](../README.md) · Next: [7. The desktop](desktop.md)
