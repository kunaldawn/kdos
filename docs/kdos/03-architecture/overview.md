# Architecture overview

This page is the map of how a KDOS machine is put together. It is for anyone
who wants to understand the system as a whole before reading about one part of
it: a person evaluating KDOS, an administrator tracing where something lives,
or a contributor about to change code.

It covers five things, each in its own section:

- the chain of programs from power-on to a window on the screen;
- the three **rings** the software is divided into, and how each is built;
- the boundary between the host system and a **box** (the container an
  application runs in);
- the processes running on a working desktop, including the root daemons;
- where each kind of state is kept on disk.

Read this page first. The other pages in Part III each expand one region of
this map and assume the words it introduces. Unfamiliar terms are defined in
the [Glossary](../06-reference/glossary.md).

## From power-on to a drawn window

A KDOS system reaches a usable desktop through six handoffs. Each step starts
the next directly; there is no service manager in the chain.

```
firmware
   │  loads Limine from the ESP (UEFI) or the MBR (BIOS)
   ▼
Limine ─────────── one loader, one config, one menu for both firmwares
   │  loads the kernel and the initramfs from FAT or ISO9660
   ▼
kernel  ────────── early microcode load, then the initramfs init
   │
   ▼
initramfs init ─── splash, A/B slot selection, LUKS unlock, find the root
   │  switch_root (util-linux's)
   ▼
init (toybox, PID 1)
   ├─ /etc/init.d/rcS ──── the numbered service scripts, in order
   └─ kdos-getty on tty1 and tty2
          │
          ▼
      kdos-login → agetty → login shell → ~/.bash_profile
          │
          ▼
      kdos-desktop ──── environment, keymap, bus, box warmup
          │
          ▼
      kdos-desktop-start ──── audio, portals, input method, the compositor
          │
          ▼
      kdos-comp ──── supervises the panel and the desktop's own daemons,
                     draws the window frames, runs the phosphor pass
```

The terms in that chart, in order:

| Term | Meaning |
|---|---|
| ESP | The EFI system partition: the small FAT partition UEFI firmware loads boot programs from |
| Limine | The boot loader. One Limine configuration and menu serve both UEFI and BIOS machines — see [Limine and the kernel command line](boot-and-init.md#limine-and-the-kernel-command-line) |
| Microcode | The processor vendor's firmware update for the CPU itself, loaded before anything else runs — see [Microcode](boot-and-init.md#microcode) |
| initramfs | A small in-memory filesystem the kernel starts first; its `init` finds and mounts the real root — see [The initramfs](boot-and-init.md#the-initramfs) |
| A/B slots | Two root partitions on a machine set up for them: an update is installed into the other slot and tried, and the slot known to work stays the fallback — see [A/B slot selection](boot-and-init.md#ab-slot-selection) |
| LUKS | Linux disk encryption; an encrypted root is unlocked here, before it can be mounted |
| Phosphor pass | The compositor's CRT-style shader over the finished frame — see [The phosphor pass](../04-programs/kdos-comp.md#the-phosphor-pass) |

Two facts about that chain explain much of what follows.

**Kernel messages go to the serial port, not the screen.** The kernel command
line lists `console=tty0 console=ttyS0`, and the last `console=` wins, so kernel
and initramfs messages go to the serial line and the display stays blank. That
blank display is what lets a graphical splash be drawn at all. It is also why
the passphrase prompt for an encrypted root is drawn through the splash and
read from `/dev/tty1` by name: `/dev/console` is the serial port.

**There is no display manager and no greeter.** `tty1` runs `kdos-getty`, which
loads the console font and palette and hands the terminal to `kdos-login`.
`kdos-login` runs `agetty`, logging in automatically when
`/etc/kdos/login.conf` names an account. The login shell's `~/.bash_profile`
then starts `kdos-desktop`, the same path any other login takes. `tty2` stays a
plain getty: it is the recovery console you switch to when the desktop on
`tty1` does not come up. The serial port `ttyS0` offers a root shell for
debugging.

[Boot and init](boot-and-init.md) covers everything up to the login prompt;
[the session](session.md) covers everything after it.

## The three rings

Every piece of software on a KDOS machine belongs to exactly one of three
**rings**. The ring decides how the software is built, how it is packaged and
who maintains it.

| Ring | Lives in | Contains | Built by |
|---|---|---|---|
| Core | `ports/core/` | musl, toybox, the toolchain, libraries, the kernel, init | Compiled here from upstream source archives, which `make fetch` downloads and verifies by `sha256` |
| Desktop | `src/desktop/`, `src/libs/`, `src/packages/` | The compositor, the shell, the daemons, the C libraries, the tools | Compiled here from this repository's own source |
| Outer | `src/packages/kdos-appbox/catalogue` | Applications: browsers, office suites, CAD, media tools, IDEs, games | Declared as Debian packages; built by podman on the machine that asks for them |

The boundary follows build cost. Anything the desktop needs in order to exist
is compiled here. Anything that is an *application* — something a person opens
to do work unrelated to the operating system — is in the outer ring, however
much it is wanted.

The sizes, measured against the tree:

| Where | Recipes |
|---|---|
| `ports/core` | 1,014 |
| `src/packages` | 11 |
| `src/desktop` | 13 |
| **All port repositories** | **1,038** |
| Catalogue `app` rows (outer ring) | 180 |

`src/packages/` and `src/desktop/` are port repositories in their own right and
use exactly the same two-file recipe format as `ports/core`. Building the
desktop is therefore not a special case anywhere in the build system. Each
phase's environment file lists the repositories that phase may draw from, and
the list grows as the build progresses:

```
script/phase4.env.sh    PORT_REPO="/ports/core /kdos/src/packages"
script/phase5.env.sh    PORT_REPO="/ports/core /kdos/src/packages"
script/desktop.env.sh   PORT_REPO="/ports/core /kdos/src/packages /kdos/src/desktop"
```

`src/packages/kdos-kpkg`, the package manager itself, is the one directory
without a recipe: it is compiled by the phase 1 script `12_kpkg.sh`, because it
has to exist before any recipe can be read.

## The host and a box

An outer-ring application runs in a **box**: a rootless container whose root
filesystem is either a container image built on this machine or a stack of
mounted **packs** (signed filesystem images, described in
[Packs and boxes](packs-and-boxes.md)). The boundary between the host and a box
is narrow on purpose, and knowing what crosses it explains most of the
desktop's design.

| Shared with a box | Not shared |
|---|---|
| `$HOME` — the same path, the same files | `/usr/share/themes`, `/usr/share/icons` |
| `$XDG_RUNTIME_DIR` (`/run/user/1000`) | The system message bus |
| `/tmp` | The host's `/usr` generally |
| `/dev`, `/sys`, when the box profile allows devices | Host processes |

Three consequences follow, and each shows up elsewhere in this book.

**Themes are written into `$HOME`**, because the system theme directories are
invisible inside a box. See [Theming](../02-user-guide/theming.md).

**The session message bus listens at a fixed path under `$XDG_RUNTIME_DIR`.**
That directory is shared; a bus address under `/tmp` would resolve to a
different `/tmp` on each side of the boundary. See [The session](session.md).

**A box reaches the host through a portal, not directly.** Opening a link,
choosing a file and capturing the screen all go through a service on the host
that decides what to allow. See [The security model](security-model.md).

## A running system

This is the process tree of a working desktop. Scripts marked "skipped until
configured" are installed but start nothing until you write their configuration
file.

```
init (PID 1, toybox)
 │
 ├─ /etc/init.d/rcS ──────────── the numbered service scripts, in order
 │   ├─ 01 udev   02 modules   03 lvm   05 hostname   10 sysctl   12 zram
 │   ├─ 15 userdirs   18 timers   20 dmesg   22 syslog
 │   ├─ 25 nftables ── loaded before the network comes up
 │   ├─ 30 network   35 chrony   40 dbus   41 polkitd
 │   ├─ 42 NetworkManager   42 modemmanager   43 boltd   45 avahi
 │   ├─ 45 seatd   47 pcscd   50 alsa
 │   ├─ 51 mdmonitor   52 smartd   53 xfs_healer
 │   ├─ 54 thermald   55 tlp   60 bluetooth   70 sshd   80 cups
 │   ├─ 81 cups-browsed   82 ipp-usb
 │   ├─ skipped until configured: 46 hostapd   63 gssd   65 brltty
 │   │    72 nfsd   73 mosquitto   74 prosody   76 postgresql   83 samba
 │   └─ the KDOS root daemons:
 │        55 kdos-powerd    /run/kdos-powerd.sock    power, and system settings
 │        56 kdos-energyd   /run/kdos-energyd.sock   per-application energy
 │        57 kdos-oomd      /run/kdos-oomd.sock      memory-pressure protection
 │        58 kdos-mountd    /run/kdos-mountd.sock    removable media and disks
 │        59 kdos-packd     /run/kdos-packd.sock     mounting application packs
 │
 ├─ kdos-getty tty1 ── loads the VT font and palette, then kdos-login
 │   └─ agetty, autologging in the account login.conf names
 │       └─ login shell
 │           └─ kdos-desktop        ← THE SESSION, from ~/.bash_profile
 │               ├─ session-common.sh:  runtime dir, keymap, boxes, bus
 │               └─ kdos-desktop-start
 │                   ├─ xdg-desktop-portal-wlr, then the main portal
 │                   ├─ fcitx5 and kdos-ime, if an input method is installed
 │                   └─ kdos-comp                 the compositor
 │                       ├─ kdos-shell  per output   the panel
 │                       ├─ kdos-desk   per output   desktop icons
 │                       ├─ kdos-slit   per output   dockapps, off by default
 │                       ├─ kdos-notifyd    one      notifications
 │                       ├─ kdos-netagent   one      NetworkManager secrets
 │                       ├─ kdos-mediad     one      removable-media toasts
 │                       ├─ kdos-clip       one      clipboard history
 │                       ├─ Xwayland        rootless
 │                       └─ application clients, host and boxed
 │
 └─ kdos-getty tty2 ── an ordinary login: the RECOVERY CONSOLE
```

The service scripts under `fs/etc/init.d/` ship with the image; `43_boltd`,
`63_gssd`, `65_brltty`, `72_nfsd`, `73_mosquitto`, `74_prosody`,
`76_postgresql` and `83_samba` are installed by their ports. Each script starts
its daemon under `ksvc`, KDOS's service supervisor, which runs the daemon in
the foreground and restarts it when it exits — see
[The daemons](../04-programs/daemons.md).

### How the screen is drawn

Every surface KDOS paints is a grid of character cells, and the compositor puts
them on the screen. `kdos-shell`, `kdos-res`, `kdos-term` and `kdos-lock` each
paint their own cell buffer through `libkwl` and hand it over as an ordinary
Wayland surface. `libkdisp` is the one place that chooses a display server, and
one implementation ships: Wayland, through `libkwl`.

Two things on the screen are not cells, and knowing which saves surprise at the
first titlebar you look at closely:

- **The compositor's own chrome** — titlebars, the root menu, the
  window-switcher OSD — is drawn by `kdos-comp` with pango, as nodes of its
  scene graph rather than client surfaces with a cell buffer behind them. The
  face is `Terminus (TTF)` at 24 points (set in `~/.config/kdos-comp/rc.xml`),
  which is 32 pixels at 96 dpi and so exactly one cell high, and the corner
  radius is zero. The frames therefore sit on the grid even though they are not
  drawn on it.
- **An application inside a box** draws whatever its own toolkit draws.

### What the compositor supervises

`kdos-comp` starts and supervises the desktop's own chrome from a table of
seven entries in `src/desktop/kdos-comp/src/kdos-child.c`. It respawns an entry
when it dies and stops it when its output goes away.

| Program | Instances | Why that many |
|---|---|---|
| `kdos-shell` | One per output | A layer surface (a Wayland surface anchored to a screen edge, such as a panel) belongs to one screen, and the toolkit keeps one cell buffer per process, so a second monitor needs a second process |
| `kdos-desk` | One per output | As above; runs when desktop icons are on |
| `kdos-slit` | One per output | As above; off by default |
| `kdos-notifyd` | One | Owns the bus name `org.freedesktop.Notifications` |
| `kdos-netagent` | One | Registers one secret agent with NetworkManager |
| `kdos-mediad` | One | Holds one subscription to `kdos-mountd` |
| `kdos-clip` | One | Owns one socket and holds the clipboard history in memory; runs when the clipboard history is on |

A child that fails more than five times inside thirty seconds is not restarted
again, so a crash loop cannot bury the log line that explains it. The
compositor raises a prompt naming what it gave up on, and the Reconfigure item
in the window-manager menu re-arms supervision.

## The root daemons

Five daemons run as root. Each is supervised by `ksvc` as a foreground process
and answers one socket in `/run`, one line per request. Who may ask is decided
by the connecting process's credentials (its user and groups), not by the
socket file's permissions.

| Daemon | Owns | Answers to |
|---|---|---|
| `kdos-powerd` | Suspend, poweroff and reboot; the time zone, the accent, autologin and the firewall's service list | root and `wheel`; also `seat` for suspend, poweroff and reboot |
| `kdos-energyd` | Reading the CPU energy counter, and attributing it to applications | root and `wheel` |
| `kdos-oomd` | Killing something before memory pressure wedges the desktop | root, `seat` and `wheel` |
| `kdos-mountd` | Removable media, encrypted (LUKS) volumes, SMB shares and disk health (SMART) | root, `seat` and `wheel` |
| `kdos-packd` | Mounting, installing, verifying and composing application packs | root and `wheel` |

`seat` is the group seatd gives the display to, so it is exactly the person at
the machine; `wheel` is the administrators' group, the same one `sudo` and
polkit treat as admin.

`kdos-boxsock` is often listed beside these and is not one of them. It runs as
the desktop user, one process per box, binds one tagged Wayland socket for that
box, and stays alive holding the security context open for the box's lifetime.

No client of a root daemon ever names a path. Every request takes an identifier
from a list the daemon itself published, or from a list compiled into it. There
is nothing to aim, which is what stops a daemon that the desktop user can reach
from becoming a way to, say, mount a stick over `/etc`. See
[The daemons](../04-programs/daemons.md) and
[The security model](security-model.md).

## The two packaging systems

KDOS has two packaging systems because they answer different questions. Host
packaging answers *what is installed on this machine*. Application packaging
answers *what software can this machine run, and how do I get a piece of it
without installing anything into the system*.

| | Host packages | Applications |
|---|---|---|
| Unit | A compiled port | A container image, or a pack |
| Built by | `kpkg`, from a recipe | podman, from the catalogue, on this machine |
| Format | A reproducible `tar.xz` archive | OCI layers; EROFS with a signed footer when exported |
| Installed by | `kpkg`, as root, into `/` | `kdos-appbox`, as a box; `kdos-packd` for an import |
| Verified by | `sha256` in the recipe, plus an optional signature | Nothing, when built here; payload hash then signature at mount time, when imported |
| Updated with | A rebuild, or a signed binary host, through `kdos update` | A rebuild against the catalogue's snapshot |
| Trusted keyring | `/etc/kdos/keys` | `/etc/kdos/keys/packs` |

The verification row matters. Building an application fetches from Debian's
archive over a pinned snapshot, which apt verifies — but nothing KDOS controls
attests to the result. An exported set of applications is hashed and
signature-checked by `kdos-packd` at the point where it is mounted.

The two keyrings are separate by construction. A pack-signing key says who
exported a set of applications; placing it in the host keyring would silently
make it a trusted publisher of *host* packages as well. The keyring loader
reads `*.pub` in one directory and does not descend into subdirectories, so
`/etc/kdos/keys/packs` is a genuinely separate policy.

See [Packaging](packaging.md) and [Packs and boxes](packs-and-boxes.md).

## Where state lives

| Path | Holds | Written by |
|---|---|---|
| `/var/lib/kpkg/db` | The installed-package database and manifests | `kpkg`, as root |
| `/var/lib/kdos/packs` | Imported application packs | `kdos-packd`, as root |
| `/var/lib/kdos/packs/staging` | The one place an unprivileged write may land | You, mode 01777 |
| `/var/lib/kdos/packs/mnt` | Mount points for packs | `kdos-packd` |
| `/var/lib/kdos/pack-manifest` | Every graft made, so removal is exact | `kdos-packd` |
| `/var/lib/kdos/fs-manifest` | Every path the build's `fs/` tree provided | The build |
| `/var/lib/kdos/apps-pending` | Applications chosen at install time and not yet built | `kinstall`; read by `kdos app install --pending` |
| `/var/lib/kdos/march.ledger` | Measured per-machine optimisation results | `kdos march` |
| `/etc/kdos/` | Machine configuration and the trusted keys | You |
| `/usr/share/kdos/` | Generated and shipped data: the icon atlas, the application table, the security database, the documentation | The build |
| `~/.config/kdos/` | Your desktop configuration | You, and `kdos-settings` |
| `~/.local/share/kdos/` | Per-user launcher tables, pack grafts and box upper layers | `kdos-appbox`, `kdos-packd` |
| `$XDG_CACHE_HOME/kdos/` | The accent state file and the retinted wallpaper | `kdos theme` |
| `$XDG_RUNTIME_DIR/` | Sockets, the session bus, launch traces | Everything, per session |

Two of those entries are interfaces rather than storage:

- `$XDG_CACHE_HOME/kdos/theme` holds a single word — the accent name — and that
  word is the entire theme state the desktop reads.
- `alien-apps` maps an application name to the command line that runs it inside
  its box, which is how a shim on your `PATH` knows what to run. The system copy
  is `/usr/share/kdos/alien-apps`; applications you install add a per-user copy
  at `~/.local/share/kdos/alien-apps`.

The full list of paths and sockets is in
[Filesystem and IPC](../06-reference/filesystem-and-ipc.md).

## Reading further

| To understand | Read |
|---|---|
| How the machine gets to a login prompt | [Boot and init](boot-and-init.md) |
| What happens when `kdos-desktop` runs | [The session](session.md) |
| How host software is built and installed | [Packaging](packaging.md) |
| How applications are packaged and run | [Packs and boxes](packs-and-boxes.md) |
| Who is allowed to do what, and what is not protected | [The security model](security-model.md) |
| Why every surface looks the same | [The design language](design-language.md) |
| Where a window lands and how it tiles | [The window model](window-model.md) |

## See also

- [Why KDOS](../01-philosophy/why-kdos.md) — the properties this structure serves
- [Repository layout](../06-reference/repository-layout.md) — where each ring lives in the tree
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md) — every path and socket in full
- [The daemons](../04-programs/daemons.md) — each root daemon in detail
- [The build system](../05-developer/build-system.md) — how the rings are built in order
- [Glossary](../06-reference/glossary.md) — the terms this page uses
