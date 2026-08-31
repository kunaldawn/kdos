# Installation

Installing KDOS to a disk with `kinstall`, the text-mode installer that ships on every image.
This page covers the wizard page by page, the choices that are hard to change afterwards, and
running an install unattended from an answer file. For how the installer is built, see
[kinstall](../04-programs/kinstall.md).

## Before you start

**Nothing is written to your disk until you press Install on the Summary page.** Every page
before it fills in an answer and does nothing else, so `Back` genuinely goes back and abandoning
the wizard leaves the machine untouched. `Next` on the Summary page is deliberately refused: the
install starts from the button and only from the button.

Two decisions are worth making before you begin, because changing them later means reinstalling:
the **root filesystem** and whether the root is **encrypted**.

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

The installer draws a full-screen interface and works with either keyboard or mouse. On a bare
console it reads the mouse directly from the input devices, so pointing works on `tty1` with no
additional software.

| Key | Action |
|---|---|
| `Tab` / `Shift+Tab` | Move between controls |
| `↑` `↓` `←` `→` | Move within a control |
| `Enter` | Activate |
| `Esc` | Back |

## The pages

Eleven pages, in this order. The sidebar shows where you are, and a completed page can be
revisited by clicking it.

### 1. Welcome

What this installer is about to do, and the machine it has detected — processor, memory, storage
and firmware. If the firmware line does not say UEFI, stop: KDOS has no BIOS boot path.

### 2. Keyboard

The console keymap, with a filter box because the list is long. The choice is written to
`/etc/keymap` and is loaded by `kdos-getty` on every terminal.

It also reaches the graphical session: `kdos-desktop` translates the console keymap name into an
XKB layout, because console and XKB use different vocabularies for the same layouts. A keymap
whose XKB layout is not present is reported rather than silently defaulted.

### 3. Time

The time zone, as a **POSIX TZ string** rather than a link to a zone file. musl parses those
directly, DST rules included. The list carries the common zones with their rules; the cost of
this form is that a program wanting the zone *name* cannot read it back.

### 4. Disk

Which disk to install to, and what to do with it:

| Plan | What happens |
|---|---|
| `wipe` | The disk is repartitioned: an ESP and a root partition |
| `reuse` | Existing partitions are used as you assign them; the Partition step is skipped |
| `manual` | `cfdisk` is opened so you can partition by hand, then you assign |

This page also carries the **encryption** checkbox and its passphrase. See below.

### 5. Layout

The root filesystem, the swap size, and whether to format the ESP.

### 6. Accounts

Hostname, your user name and full name, your password, and whether the root account is locked.

Renaming the user is a real rename: it rewrites `passwd`, `group` (both the membership lists and
the primary group's own name), `shadow`, the home directory, and the autologin line in
`/etc/inittab`. Miss that last one and the installed system logs nobody in.

### 7. System

Which services start at boot — NetworkManager, Bluetooth, sshd, CUPS, ALSA — and the theme
accent.

### 8. Applications

The application packs on the medium, with the recommended set already ticked. Untick what you do
not want; the size total updates as you go.

**The base pack and the runtimes are not choices.** An application pack is a difference over a
runtime, and a runtime is a difference over the base, so leaving one out would install
applications that cannot start. They are drawn as facts and are always carried.

This page is skipped entirely on a medium with no pack index — a plain ISO built without
`make bootstrap-packs` or `make fetch-packs`.

### 9. Summary

Everything you chose, on one screen, with the destructive parts called out. This is the point of
no return.

### 10. Install

The work, with a progress bar and a live log. It runs in a separate process that reports back
over a pipe, so the interface stays responsive during a multi-gigabyte copy. Steps report as they
start, finish, or fail.

### 11. Done

What was installed and where. Reboot, or quit back to the live system.

## The root filesystem

Four choices. The row you pick is read by the menu, the `mkfs` invocation, the `fstab` line and
the swapfile step — they cannot disagree.

| Filesystem | `fstab` pass | Swapfile made with | Notes |
|---|---|---|---|
| `ext4` | 1 | `fallocate` | The default: journalled, boring, built into the kernel |
| `btrfs` | 0 | `btrfs filesystem mkswapfile` | Snapshots and transparent zstd compression |
| `xfs` | 0 | `dd` | Large files and parallel I/O; it cannot be shrunk |
| `f2fs` | 0 | `dd` | Log-structured, for flash: a stick, an SD card, a cheap eMMC |

Two details that are not arbitrary. Only ext4 gets a non-zero `fstab` pass number, because a
non-zero pass is an instruction to run a checker at boot and there is no checker worth running
for the other three. And the swapfile is made differently on each because `fallocate` leaves
unwritten extents, which xfs and f2fs refuse to swap on — a mistake here fails at the *next*
boot's `swapon -a`, with no swap and nothing saying why.

ext4 and btrfs are built into the kernel; xfs and f2fs are modules the initramfs carries. A
filesystem whose `mkfs` is missing from the image is still listed, with the row saying so, and is
refused before anything is written rather than failing partway through.

## Encryption

Ticking encryption on the Disk page puts LUKS2 on the root partition. The installer runs
`cryptsetup luksFormat` and then `open`, feeding the passphrase on **stdin** both times — an
argument would publish it through `/proc/<pid>/cmdline` to every process on the machine for as
long as `cryptsetup` runs.

Everything after that point talks to `/dev/mapper/kdosroot`, so one name means "where the root
filesystem is" for the `mkfs`, the mount, the `fstab` UUID and the copy.

**The boot options carry two UUIDs, and confusing them is the trap:**

```
cryptdevice=UUID=<the LUKS container>:kdosroot
root=UUID=<the filesystem inside it>
```

The second does not exist until the first is open. At boot the initramfs prompts through the
splash rather than on `/dev/console` — the kernel command line sends the console to a serial
port, so a plain prompt would be typed at a wire nobody is looking at — reads the keystrokes from
`/dev/tty1`, and feeds the passphrase to `cryptsetup` on stdin. Three attempts, then a shell
rather than a reboot loop.

There is no per-keystroke feedback while typing the passphrase. The splash owns the framebuffer
and the shell owns the terminal, so a masked field would mean moving the read into the splash.

**If the image has no `cryptsetup`, the passphrase is refused on the questionnaire page**, not at
the install step. Discovering it after the point of no return would be the wrong place.

## A/B root slots

The installer writes the initial boot state to the ESP: slot A active, slot B empty, and
`bootstate=UUID=<esp>` added to the kernel options.

That is the state machine's starting position, not a working dual-boot setup — filling slot B is
an updater's job. What it gives you now is the machinery: a candidate slot gets a fixed number of
attempts, counted down in the initramfs before anything is mounted, and promoted to active only
when `rcS` reaches its end. A root that boots into a wedged userland still spends an attempt, so
a bad update rolls itself back with no help from anything.

**A/B and LUKS are not wired together.** The slot selection yields a filesystem UUID, and an
encrypted slot's filesystem lives inside a container, so combining them needs a per-slot
`cryptdevice=`. That is not done — see [Known gaps](../06-reference/known-gaps.md).

## Answer files and unattended installs

Save your answers from an interactive run, then replay them:

```sh
kinstall --save answers.conf     # write the answers out
kinstall --config answers.conf   # preload them, still interactive
kinstall --unattended --config answers.conf
```

The file is flat `key = value`. The keys are the questions: `disk`, `plan`, `partition`, `root`,
`esp`, `format_esp`, `fstype`, `swap`, `swap_mb`, `luks`, `luks_passphrase`, `keymap`,
`timezone`, `hostname`, `username`, `fullname`, `password`, `root_locked`, `root_password`,
`services`, `theme`, `packs`, `reboot`.

Three behaviours that make an unattended run safe to script:

- **An answer file naming an unknown application falls back to the recommended set.** It is read
  before the point of no return, and refusing there would leave a machine with no operating
  system on it over the spelling of one application.
- **An answer file naming an unknown filesystem falls back to ext4** rather than failing at the
  `mkfs`.
- **An unattended run always terminates.** It shows the finished screen briefly, then reboots or
  exits according to `reboot`. It **exits non-zero when the install failed**, so a scripted
  install that returns 0 really did work.

## Inspecting without installing

```sh
kinstall --dry-run                    # log every command, execute none
kinstall --dump probe                 # the machine as the installer sees it
kinstall --dump plan                  # the steps that would run, including the skips
kinstall --dump plan --json           # the same, machine-readable
```

`--dump probe` is the right thing to paste into a bug report: firmware, processor, memory, and
every disk and partition. `--dump plan` calls the same planner the wizard does, so the step list
and its skips are the real ones — choosing `reuse` removes the Partition step, and a non-default
accent adds the Theme step.

**No password appears in either dump.** The configuration holds them in the clear because hashing
is the next thing that happens to them, and a dump is what ends up in a log.

## What the installer writes

| Path | What |
|---|---|
| The ESP | rEFInd, its generated configuration, the kernel and the initramfs |
| The ESP | The A/B boot state file |
| Root | The system, copied from the medium |
| `/etc/fstab` | **Appended to**, never replaced — the shipped file carries the `/tmp` entry that every graphical application depends on |
| `/etc/keymap` | The console keymap |
| `/etc/profile.d/20-timezone.sh` | The POSIX TZ string |
| `/var/lib/kdos/packs` | The base, the runtimes, and the applications you ticked |

The kernel and initramfs are copied **onto the ESP**, and the generated rEFInd configuration
points at those FAT paths. rEFInd can read an ext4 root only through a filesystem driver, and a
boot that depends on a driver load is a boot that fails silently after a kernel update.

## See also

- [Getting started](getting-started.md) — building the image you are installing from
- [kinstall](../04-programs/kinstall.md) — how the installer works inside
- [Administration](administration.md) — services, users and storage after the install
- [Boot and init](../03-architecture/boot-and-init.md) — A/B slots, encryption and the boot path
- [Applications](applications.md) — adding packs after the install
