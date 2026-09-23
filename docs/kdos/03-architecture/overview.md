# Architecture overview

This page is the map for the rest of Part III. It describes how a KDOS machine
is assembled — the division of the source tree into three rings, the boundary
between the host system and a containerised application, the sequence from
power-on to a drawn window, the processes running once that window is up, and
where each kind of state is kept.

Each of the other architecture pages expands one region of this map. Read this
one first; the others assume its vocabulary.

## From power-on to a drawn window

A KDOS system reaches a usable desktop through six handoffs. Nothing in the
chain is optional and nothing in it is a service manager.

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
   ├─ /etc/init.d/rcS ──── 28 numbered service scripts, in order
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

Two properties of that chain are worth stating at the top, because they explain
a great deal of what follows.

The kernel command line ends with a serial `console=`, so kernel and initramfs
messages go to the serial line and the display stays blank. That is what makes
a graphical splash possible at all, and it is why the encrypted-root passphrase
prompt has to be drawn through the splash and read from `/dev/tty1` by name.

There is no display manager and no greeter. A login on `tty1` reaches the
desktop because the login shell's profile starts it, along the same path any
other login takes. `tty2` stays a plain getty and is the recovery console.

[Boot and init](boot-and-init.md) covers everything up to the login prompt;
[the session](session.md) covers everything after it.

## The three rings

Every piece of software on a KDOS machine belongs to exactly one of three
rings. Which ring it is in determines how it is built, how it is packaged and
who is responsible for it.

| Ring | Lives in | Contains | Built by |
|---|---|---|---|
| Core | `ports/core/` | musl, toybox, the toolchain, libraries, the kernel, init | Compiled here from upstream tarballs |
| Desktop | `src/desktop/`, `src/libs/`, `src/packages/` | The compositor, the shell, the daemons, the C libraries, the tools | Compiled here from this repository's own source |
| Outer | `src/packages/kdos-appbox/catalogue` | Applications: browsers, office suites, CAD, media tools, IDEs, games | Declared as Debian packages; built by podman on the machine that asks for them |

Build cost decides the boundary. Anything the desktop needs in order to exist
is compiled here. Anything that is an *application* — something a person opens
to do work unrelated to the operating system — is in the outer ring, and no
amount of desirability moves it inward.

Measured against the tree: 879 recipes in `ports/core`, 11 in `src/packages`,
13 in `src/desktop`, for 903 ports in total, and 183 `app` rows in the
catalogue.

`src/packages/` and `src/desktop/` are port repositories in their own right,
using exactly the same two-file recipe format as `ports/core`. Building the
desktop is therefore not a special case anywhere in the build system. The phase
environment lists more repositories as the build progresses:

```
script/phase4.env.sh    PORT_REPO="/ports/core /kdos/src/packages"
script/phase5.env.sh    PORT_REPO="/ports/core /kdos/src/packages"
script/desktop.env.sh   PORT_REPO="/ports/core /kdos/src/packages /kdos/src/desktop"
```

## The host and a box

An outer-ring application runs in a **box**: a rootless container over a stack
of mounted pack images. The boundary between the host and a box is narrow and
deliberate, and knowing exactly what crosses it explains most of the desktop's
design.

| Shared with a box | Not shared |
|---|---|
| `$HOME` — the same path, the same files | `/usr/share/themes`, `/usr/share/icons` |
| `$XDG_RUNTIME_DIR` (`/run/user/1000`) | The system message bus |
| `/tmp` | The host's `/usr` generally |
| `/dev`, `/sys`, when the box profile allows devices | Host processes |

Three consequences follow, and each shows up elsewhere in this documentation.

Themes are written into `$HOME`, because the system theme directories are
invisible inside a box. See [Theming](../02-user-guide/theming.md).

The session message bus listens at a fixed path under `$XDG_RUNTIME_DIR`. That
directory is shared; a `/tmp`-based bus address would resolve to a different
`/tmp` on the two sides of the boundary. See [The session](session.md).

A box reaches the host through a portal rather than directly. Opening a link,
choosing a file and capturing the screen all go through a service on the host
that decides what to allow. See [The security model](security-model.md).

## A running system

```
init (PID 1, toybox)
 │
 ├─ /etc/init.d/rcS ──────────── the numbered service scripts, in order
 │   ├─ 01 udev   02 modules   05 hostname   10 sysctl   12 zram
 │   ├─ 15 userdirs   18 timers   20 dmesg   22 syslog
 │   ├─ 25 nftables ── loaded before the network comes up
 │   ├─ 30 network   35 chrony   40 dbus   41 polkitd
 │   ├─ 42 NetworkManager   45 avahi   45 seatd   50 alsa
 │   ├─ 54 thermald   55 tlp   60 bluetooth   70 sshd   80 cups
 │   └─ the KDOS root daemons:
 │        55 kdos-powerd    /run/kdos-powerd.sock    suspend, poweroff, reboot
 │        56 kdos-energyd   /run/kdos-energyd.sock   per-application energy
 │        57 kdos-oomd      /run/kdos-oomd.sock      memory-pressure protection
 │        58 kdos-mountd    /run/kdos-mountd.sock    removable media
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

Every surface KDOS paints is a grid of character cells, and the compositor is
what puts them on a screen. `kdos-shell`, `kdos-res`, `kdos-term` and
`kdos-lock` each paint their own cell buffer through `libkwl` and hand it over
as an ordinary Wayland surface. `libkdisp` is the one place that picks a
display server, and there is one to pick.

Two things on that screen are not cells, and knowing which saves a reader the
surprise of their first titlebar. The compositor draws its own chrome —
titlebars, the root menu, the window-switcher OSD — with pango: those are
scene-graph nodes the server renders itself, not client surfaces with a cell
buffer behind them. The face is `Terminus (TTF)` at 24 points, which is 32
pixels at 96dpi and so exactly one cell high, and the corner radius is zero, so
the frames sit on the grid even though they are not drawn on it. And an
application inside a box draws whatever its own toolkit draws.

The compositor supervises the desktop's own chrome from a table in its source —
seven entries, respawned when they die and stopped when their output goes away.
Three are per-output, because a layer surface belongs to one screen and the
toolkit keeps one cell buffer per process: a second monitor needs a second
*process*, not a second surface. Four are single-instance, each owning
something there can only be one of — a bus name, one agent registered with
NetworkManager, one subscription to the mount daemon, and a socket holding a
clipboard history in memory.

A child that dies more than five times inside thirty seconds is not restarted
again, so a crash loop cannot bury the log line that explains it. The
compositor raises a prompt naming what it gave up on, and the Reconfigure verb
in the window-manager menu re-arms supervision.

## The root daemons

Five daemons run as root, each supervised by `ksvc` as a foreground process,
each answering one socket in `/run` with one line per request. Authorisation is
by the connecting process's credentials rather than by the socket's mode.

| Daemon | Owns | Answers to |
|---|---|---|
| `kdos-powerd` | Suspend, poweroff, reboot, timezone, the accent | root and `wheel` |
| `kdos-energyd` | Reading the CPU energy counter, and attributing it | root and `wheel` |
| `kdos-oomd` | Killing something before memory pressure wedges the desktop | root and `wheel` |
| `kdos-mountd` | Mounting removable media | root and `wheel` |
| `kdos-packd` | Mounting, installing and composing application packs | root and `wheel` |

`kdos-boxsock` is frequently listed beside them and is not one of them: it runs
as the desktop user, binds one tagged Wayland socket per box, and stays alive
holding the security context open for that box's lifetime.

No client of a root daemon ever names a path. Every verb takes an identifier
out of a list the daemon itself published, or out of a list compiled into it.
There is nothing to aim, which is what keeps a daemon reachable from `wheel`
from becoming a way to mount a stick over `/etc`. See
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
| Updated with | A rebuild, or a signed binary host | A rebuild against the catalogue's snapshot |
| Trusted keyring | `/etc/kdos/keys` | `/etc/kdos/keys/packs` |

The verification row deserves emphasis rather than a footnote. Building an
application fetches from Debian's archive over a pinned snapshot, which apt
verifies — but nothing this system controls attests to the result. An exported
set is hashed and signature-checked by `kdos-packd` at the point where it is
mounted.

The two keyrings are separate by construction, not by convention. A
pack-signing key attests to who exported a set of applications; placing it in
the host keyring would silently make it a trusted publisher of *host* packages
as well. The keyring loader reads `*.pub` in one directory and does not descend
into subdirectories, so `/etc/kdos/keys/packs` is genuinely a second policy.

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
| `/var/lib/kdos/march.ledger` | Measured per-machine optimisation results | `kdos march` |
| `/etc/kdos/` | Machine configuration and the trusted keys | You |
| `/usr/share/kdos/` | Generated and shipped data: the icon atlas, the alien-app table, the security database, the documentation | The build |
| `~/.config/kdos/` | Your desktop configuration | You, and `kdos-settings` |
| `~/.local/share/kdos/` | Per-user launcher tables and pack grafts | `kdos-appbox` |
| `$XDG_CACHE_HOME/kdos/` | The accent state file and the retinted wallpaper | `kdos theme` |
| `$XDG_RUNTIME_DIR/` | Sockets, the session bus, launch traces | Everything, per session |

Two of those entries are interfaces rather than storage.
`$XDG_CACHE_HOME/kdos/theme` holds a single word — the accent name — and that
word is the entire theme state the desktop reads. `/usr/share/kdos/alien-apps`
maps an application name to the command line that runs it inside its box, which
is how a shim on your `PATH` knows what to execute.

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
