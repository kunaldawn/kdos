# Architecture overview

How KDOS is put together: the three rings, the boundary between the host and a containerised
application, what is running on a live system, and where every kind of state lives. Read this
before the other architecture pages — each of them is one region of the map drawn here.

## The three rings

Everything in KDOS belongs to exactly one of three rings, and which ring a thing is in determines
how it is built, how it is packaged and who is responsible for it.

| Ring | Lives in | What it is | Built by |
|---|---|---|---|
| **Core** | `ports/core/` | musl, toybox, the toolchain, libraries, the kernel, init | Compiled here from upstream tarballs |
| **Desktop** | `src/desktop/`, `src/libs/`, `src/packages/` | The compositor, the shell, the daemons, the libraries, the tools | Compiled here from our own source |
| **Outer** | `ports/appbox/` | Applications: browsers, office, CAD, media, IDEs, games | Assembled from Debian packages into signed images |

The rule that decides is the build cost. Anything the desktop needs to exist is compiled here.
Anything that is an *application* — something a user opens to do work unrelated to the operating
system — is in the outer ring, and no amount of desirability moves it inward.

**`src/packages/` and `src/desktop/` are both port repositories**, alongside `ports/core`. They
hold what is *ours* rather than an upstream tarball, and they use exactly the same recipe format,
so building the desktop is not a special case in the build system. The phase environment names
them:

```
PORT_REPO="/ports/core /kdos/src/packages /kdos/src/desktop"
```

## The host and a box

An application in the outer ring runs in a **box**: a rootless container over a stack of mounted
pack images. The boundary is narrow and deliberate, and knowing exactly what crosses it explains
most of the desktop's design.

| Shared with a box | Not shared |
|---|---|
| `$HOME` — the same path, the same files | `/usr/share/themes`, `/usr/share/icons` |
| `$XDG_RUNTIME_DIR` (`/run/user/1000`) | The system message bus |
| `/tmp` | The host's `/usr` generally |
| `/dev`, `/sys` | Host processes |

Three consequences fall out of that table, and each shows up elsewhere in this documentation:

- **Themes are written into `$HOME`**, because the system theme directories are invisible inside a
  box. See [Theming](../02-user-guide/theming.md).
- **The session message bus is at a fixed path under `$XDG_RUNTIME_DIR`**, because that directory
  is shared and `/tmp`-based bus addresses would be a different `/tmp`. See
  [The session](session.md).
- **A box reaches the host through the portal**, not directly. Opening a link, choosing a file or
  capturing the screen all go through a service on the host that decides what to allow. See
  [The security model](security-model.md).

## A running system

```
init (PID 1, toybox)
 │
 ├─ /etc/init.d/rcS ──────────── runs the numbered service scripts in order
 │   ├─ udev, modules, sysctl, zram, userdirs, logging
 │   ├─ nftables ─── loaded before the network comes up
 │   ├─ NetworkManager, chrony, avahi, dbus (system bus), seatd
 │   └─ the KDOS root daemons:
 │        kdos-powerd    /run/kdos-powerd.sock    suspend, poweroff, reboot
 │        kdos-energyd   /run/kdos-energyd.sock   per-application energy
 │        kdos-oomd      /run/kdos-oomd.sock      memory-pressure protection
 │        kdos-mountd    /run/kdos-mountd.sock    removable media
 │        kdos-packd     /run/kdos-packd.sock     mounting application packs
 │
 ├─ kdos-getty tty1 ── loads the console font, then agetty --autologin kdos
 │   └─ login shell
 │       └─ kdos-desktop            ← started by hand
 │           ├─ dbus-daemon --session   at $XDG_RUNTIME_DIR/bus
 │           ├─ pipewire, pipewire-pulse
 │           ├─ kdos-appbox warmup      pinned applications, at nice 10
 │           └─ kdos-desktop-start
 │               ├─ xdg-desktop-portal-wlr, then the main portal
 │               └─ kdos-comp                     the compositor
 │                   ├─ kdos-shell      per output   the panel
 │                   ├─ kdos-desk       per output   desktop icons
 │                   ├─ kdos-slit       per output   dockapps, off by default
 │                   ├─ kdos-notifyd    one          notifications
 │                   ├─ kdos-clip       one          clipboard history
 │                   ├─ Xwayland        rootless
 │                   └─ application clients, host and boxed
 │
 └─ kdos-getty tty2 ── an ordinary login
```

**The compositor supervises its own chrome.** Five programs are started from a table inside it,
respawned if they die, and stopped when their output goes away. Three are per-output, because a
layer surface belongs to one screen and our toolkit has one cell buffer per process — so a second
monitor needs a second *process*, not a second surface. Two are single-instance, because each owns
something there can only be one of: a bus name, and a socket holding a history in memory.

Five deaths in thirty seconds stops the respawn, so a crash loop does not bury the log line that
explains it.

## The root daemons

Every one takes the same shape: a foreground process under `ksvc`, one socket in `/run`, one line
per connection, and authorisation by the peer's credentials rather than by the socket's mode.

| Daemon | Owns | Answers to |
|---|---|---|
| `kdos-powerd` | Suspend, poweroff, reboot | root and `wheel` |
| `kdos-energyd` | Reading the CPU energy counter, and attributing it | root and `wheel` |
| `kdos-oomd` | Killing something before memory pressure wedges the desktop | root and `wheel` |
| `kdos-mountd` | Mounting removable media | root and `wheel` |
| `kdos-packd` | Mounting, installing and composing application packs | root and `wheel` |
| `kdos-boxsock` | Tagging box clients so the compositor can identify them | the compositor |

**The client never names a path.** Every verb takes an identifier out of a list the daemon itself
published. There is nothing to aim, which is what keeps a daemon reachable from `wheel` from being
a way to mount a stick over `/etc`. See [The daemons](../04-programs/daemons.md) and
[The security model](security-model.md).

## The two package systems

KDOS has two, and they are not one system because they answer different questions.

| | Host packages | Application packs |
|---|---|---|
| Unit | A compiled port | A filesystem image |
| Built by | `kpkg` from a recipe | `bake` from Debian packages |
| Format | A reproducible tar archive | EROFS with a signed footer |
| Installed by | `kpkg`, as root, into `/` | `kdos-packd`, as a mount |
| Verified by | `sha256` in the recipe, and an optional signature | Payload hash, then signature, at mount time |
| Updated with | A rebuild, or a signed binary host | A delta or a whole pack, from a directory |
| Trusted keyring | `/etc/kdos/keys` | `/etc/kdos/keys/packs` |

**The keyrings are separate on purpose.** A pack-signing key attests that some application images
came off one medium. Putting it in the host keyring would silently make it a trusted publisher of
*host* packages too, which is a widening nobody asked for. The keyring loader does not descend
into subdirectories, so the separation is structural rather than a convention.

See [Packaging](packaging.md) and [Packs and boxes](packs-and-boxes.md).

## Where state lives

| Path | What | Who writes it |
|---|---|---|
| `/var/lib/kpkg/db` | The installed-package database and manifests | `kpkg`, as root |
| `/var/lib/kdos/packs` | Installed application packs | `kdos-packd`, as root |
| `/var/lib/kdos/packs/staging` | Where an unprivileged download may land | You, mode 01777 |
| `/var/lib/kdos/packs/mnt` | Mount points for packs | `kdos-packd` |
| `/var/lib/kdos/pack-manifest` | Every graft made, so removal is exact | `kdos-packd` |
| `/var/lib/kdos/fs-manifest` | Every path the build's `fs/` tree provided | The build |
| `/var/lib/kdos/march.ledger` | Measured per-machine optimisation results | `kdos march` |
| `/etc/kdos/` | Machine configuration and the trusted keys | You |
| `/usr/share/kdos/` | Generated and shipped data: the icon atlas, the alien-app table, the security database, documentation | The build |
| `~/.config/kdos/` | Your desktop configuration | You, and `kdos-settings` |
| `~/.local/share/kdos/` | Per-user launcher tables and pack grafts | `kdos-appbox` |
| `$XDG_CACHE_HOME/kdos/` | The accent state file and the retinted wallpaper | `kdos theme` |
| `$XDG_RUNTIME_DIR/` | Sockets, the session bus, launch traces | Everything, per session |

Two of those are interfaces rather than storage. `$XDG_CACHE_HOME/kdos/theme` holds a single word
— the accent name — which is the entire theme state the desktop reads; and
`/usr/share/kdos/alien-apps` maps an application name to the command line that runs it inside its
box, which is how a shim on your `PATH` knows what to do.

## Reading further

| To understand | Read |
|---|---|
| How the machine gets to a login prompt | [Boot and init](boot-and-init.md) |
| What happens when you type `kdos-desktop` | [The session](session.md) |
| How host software is built and installed | [Packaging](packaging.md) |
| How applications are packaged and run | [Packs and boxes](packs-and-boxes.md) |
| Who is allowed to do what, and what is not protected | [The security model](security-model.md) |
| Why every surface looks the same | [The design language](design-language.md) |

## See also

- [Why KDOS](../01-philosophy/why-kdos.md) — the properties this structure serves
- [Repository layout](../06-reference/repository-layout.md) — where each ring lives in the tree
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md) — every path and socket in full
- [The daemons](../04-programs/daemons.md) — each root daemon in detail
- [The build system](../05-developer/build-system.md) — how the rings are built in order
