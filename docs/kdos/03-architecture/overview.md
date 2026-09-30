# Architecture overview

This chapter is the map of a running KDOS machine: what is compiled from where, which programs
start which, where the line between the host and an application's container falls, which
processes are alive on a working desktop, and where each kind of state is kept on disk. It is
written for anyone who wants the whole system in view before reading about one part of it: a
person evaluating KDOS, an administrator tracing where something lives, or a contributor about to
change code. Read [Why KDOS](../01-philosophy/why-kdos.md) first for what the system is for; read
this chapter before the rest of Part III, because each of those chapters expands one region of
this map and assumes the words introduced here. The build side of the same picture, from a clone
of the repository to a bootable ISO, is told in
[How KDOS is built](../05-developer/how-kdos-is-built.md). Terms not defined on this page are in
the [Glossary](../06-reference/glossary.md).

## The three rings

Every piece of software on a KDOS machine belongs to exactly one of three **rings**. The ring
decides where the software's source comes from, how it is built and packaged, and who maintains
it.

| Ring | Lives in | Contains | Built by |
|---|---|---|---|
| Core | `ports/core/` | musl, toybox, the toolchain, the Linux kernel, Limine, wlroots, podman and distrobox, the libraries and tools of the base system, the GUI toolkits (GTK, Qt, KDE Frameworks, wxWidgets, FLTK, Tk) and the applications ported natively on them | Compiled on the build machine from upstream source archives, which `make fetch` downloads and verifies against each recipe's `sha256` |
| Desktop | `src/desktop/`, `src/daemons/`, `src/system/`, `src/art/`, `src/libs/` | The compositor, the panel, the terminal, the root daemons, the installer, the `kdos` command, the themes, and the `libk*` C libraries they share | Compiled on the build machine from this repository's own source |
| Outer | `src/system/kdos-appbox/catalogue` | Applications KDOS does not port natively, and alternatives to those it does: browsers, office suites, CAD, media tools, IDEs, games | Declared as Debian packages; built by podman on the machine that asks for them |

A **port** is one unit of the first two rings: a directory holding a recipe (`kpkgbuild`,
declarative metadata that is parsed and never sourced) and a `build.sh` beside it. `kpkg`, the
package manager, builds a port into a package and installs it. In the core ring, musl is the C
library, toybox is the single multi-call binary that provides the base system's command-line
tools, and wlroots is the library a Wayland compositor is built on.

The boundary between the rings follows build cost and ownership. Anything the desktop needs in
order to exist is compiled on the build machine. The compositor is one of these: `kdos-comp` is a
hard fork of labwc 0.20.0, a stacking Wayland compositor, built against the wlroots port in
`ports/core`. The desktop ring draws character cells and links no GUI toolkit.

Applications, the programs a person opens to do work unrelated to the operating system, come from
either of two places. A natively ported application is a core-ring recipe like any other: Firefox
ESR, Thunderbird, LibreOffice, GIMP, Inkscape, Krita, KiCad, FreeCAD and VLC are among them. The
toolkits and the applications build in two phases of their own: `43_toolkits` builds GTK 3 and 4, Qt
5 and 6 and the other toolkits with every library over them that something else links (VLC is one,
since other programs depend on it), and `44_apps` builds the applications nothing depends on. Each
toolkit is built with its Wayland back end as the default and its X11 back end compiled in, and
Xwayland serves a program that has only an X11 path. Anything not ported natively is in the outer
ring and runs in a container rather than on the host.

The sizes, counted as directories holding a `kpkgbuild` (and, for the catalogue, lines whose
first field is `app`):

| Where | Count |
|---|---|
| `ports/core` | 1,999 recipes, on 102 shelves |
| `src/system` | 5 recipes |
| `src/art` | 6 recipes |
| `src/desktop` | 8 recipes |
| `src/daemons` | 5 recipes |
| All port repositories | 2,023 recipes |
| Catalogue `app` rows (outer ring) | 73 |

`ports/core` files each port one level down, on a **shelf** named for its subject:
`ports/core/<shelf>/<name>/`, such as `ports/core/wl/wlroots/` or `ports/core/fonts/noto-fonts/`.
The closed list of 102 shelves, one line each with what belongs on it, is the file
`ports/shelves`. A shelf is only where a recipe is filed. It subdivides the core ring and is not a
ring of its own; a port is known everywhere by its bare name, and no dependency line, package or
database entry records its shelf.

The four `src/` areas that hold recipes are port repositories in their own right and use the same
two-file recipe format as `ports/core`, so building the desktop is not a special case anywhere in
the build system. Each area holds its ports directly, with no shelves. `kpkg` searches the
repositories named in `PORT_REPO`, which defaults to `/ports/core`; the environment file of each
later phase widens it:

| Phases (`script/phases/<phase>/phase.env`) | `PORT_REPO` |
|---|---|
| `20_selfhost`, `30_foundation`, `31_compilers` | `/ports/core` (the default) |
| `40_lang` to `44_apps`, `60_kernel` | `/ports/core /kdos/src/system /kdos/src/art` |
| `50_desktop` | `/ports/core /kdos/src/system /kdos/src/art /kdos/src/desktop /kdos/src/daemons` |

`src/libs/` holds no recipes: each desktop port compiles the library sources it needs into its
own binaries. `src/devtools/` holds none either: `kdosbuild` and `kdos-portup` run on the build
machine and are never installed. `src/system/kdos-kpkg`, the package manager itself, is the one
directory in a port area without a recipe: the bootstrap step
`script/phases/10_bootstrap/120_kpkg.sh` compiles it, because it has to exist before any recipe can
be read. How the phases follow one another, and how the package lists of `40_lang` to `44_apps`
are split into one file per shelf under `packages.d/`, is the subject of
[How KDOS is built](../05-developer/how-kdos-is-built.md); every port, listed by shelf, is in
[The ports catalogue](../06-reference/ports-catalogue.md).

## From power-on to a drawn window

A KDOS machine reaches a usable desktop through a chain of handoffs, each program starting or
executing the next. There is no service manager in the chain: PID 1 is toybox's `init`, which
runs one start-up script and keeps the login consoles alive, and everything after the login is
started by the login shell.

```
firmware
   │  loads Limine from the ESP (UEFI) or the boot sector (BIOS)
   ▼
Limine ─────────── one loader, one configuration, one menu for both firmwares
   │  loads the kernel and the initramfs
   ▼
kernel ─────────── applies CPU microcode from the early initramfs, then runs its init
   │
   ▼
initramfs init ─── splash, A/B slot selection, LUKS unlock, LVM, find and check the root
   │  switch_root (util-linux's)
   ▼
init (toybox, PID 1), reading /etc/inittab
   ├─ /etc/init.d/rcS ──── fsck and mount, then the numbered service scripts in order
   ├─ kdos-getty tty1 → kdos-login → agetty → login shell
   │       │  ~/.bash_profile, on tty1 only
   │       ▼
   │   kdos-desktop ──────── runtime directory, keymap, box warm-up, session bus
   │       │  exec
   │       ▼
   │   kdos-desktop-start ── audio, portals, input method, then the compositor
   │       │
   │       ▼
   │   kdos-comp ─────────── supervises the panel and the desktop's own daemons,
   │                         draws the window frames, runs the phosphor pass
   ├─ kdos-getty tty2 → getty ── the recovery console
   └─ ttyS0 ──────────────────── a root shell on the serial line, when a key is pressed
```

The terms in that chart, in order:

| Term | Meaning |
|---|---|
| ESP | The EFI system partition: the small FAT partition UEFI firmware loads boot programs from |
| Limine | The boot loader. One Limine configuration and menu serve both UEFI and BIOS machines — see [Limine and the kernel command line](boot-and-init.md#limine-and-the-kernel-command-line) |
| Microcode | The processor vendor's update for the CPU itself. The kernel is built without late loading, so the early initramfs is the only path — see [Microcode](boot-and-init.md#microcode) |
| initramfs | A small in-memory filesystem the kernel starts first; its `init` finds, unlocks, checks and mounts the real root — see [The initramfs](boot-and-init.md#the-initramfs) |
| A/B slots | Two root partitions on a machine set up for them: an update is installed into the other slot and tried, and the slot known to work stays the fallback — see [A/B slot selection](boot-and-init.md#ab-slot-selection) |
| LUKS | Linux disk encryption; an encrypted root is unlocked in the initramfs, before it can be mounted |
| `switch_root` | The call that makes the real root filesystem `/` and runs its `init`. KDOS uses util-linux's, because toybox's changes root in a way no container can later start under — see [`switch_root`](boot-and-init.md#switch_root) |
| Phosphor pass | The compositor's CRT-style shader over the finished frame; it runs only on the GLES2 renderer — see [The phosphor pass](../04-programs/kdos-comp.md#the-phosphor-pass) |

### The console and the splash

From the initramfs until `rcS` finishes, `kdos-splash` draws the boot progress on the
framebuffer, and `rcS` reports one step to it per enabled service script. The passphrase prompt
for an encrypted root is drawn through the splash. The last thing `rcS` does is confirm the booted
A/B slot as good (`kdos-bootctl mark-good`) and tell the splash to quit, so the login on `tty1`
and the compositor find a framebuffer nothing else is drawing on. Where kernel messages go, and
how the splash survives the change of root, are in [The splash](boot-and-init.md#the-splash) and
[Command-line parameters KDOS reads](boot-and-init.md#command-line-parameters-kdos-reads).

### The login

There is no display manager and no greeter. `/etc/inittab` runs `kdos-getty` on `tty1`, which
loads the console font and the accent's sixteen-colour palette onto that terminal, sets up the
delegated control group and the real-time resource limits every later process inherits (the
audio path depends on the latter), and then executes `kdos-login`. `kdos-login` reads
`/etc/kdos/login.conf` and executes `agetty`, with `--autologin` when the file names an account;
the image ships with `autologin = kdos`. The installer writes the line for the account it
creates, commented out unless an answer file asked for autologin, so an installed machine asks for
a password at `tty1`. Without an active line `agetty` asks for a name and password as usual.

The login shell's `~/.bash_profile` starts `kdos-desktop` when it is running on `tty1` and no
Wayland display is set. Any other login — `tty2`, SSH, the serial line — is an ordinary shell.
`tty2` is deliberately a plain getty: it is the recovery console to switch to when the desktop on
`tty1` does not come up. The serial port `ttyS0` offers a root shell for debugging once a key is
pressed on it.

### The session

`kdos-desktop` prepares the environment and then replaces itself with `kdos-desktop-start`. Most
of their steps are shell functions in `/usr/local/lib/kdos/session-common.sh`; the portal and
input-method steps are in `kdos-desktop-start` itself:

| Step | What it does |
|---|---|
| Runtime directory | `XDG_RUNTIME_DIR` defaults to `/run/user/<uid>` |
| Keymap | The VT keymap in `/etc/keymap` becomes `XKB_DEFAULT_LAYOUT` (and a variant), so the desktop and the lock screen use the same layout as the console |
| Boxes | `kdos-appbox warmup` starts the application container in the background, and every ten minutes `kdos-box gc` stops boxes that have run past their profile's `autostop` and have no window open |
| Session bus | One `dbus-daemon` per user at `unix:path=$XDG_RUNTIME_DIR/bus`, reused across session restarts |
| Audio | PipeWire, then WirePlumber, then `pipewire-pulse`, once per user |
| Portals | Once the compositor's socket exists: `xdg-desktop-portal-wlr`, then the main `xdg-desktop-portal` |
| Input method | `kdos-ime` and then `fcitx5 -d`, only when `fcitx5` is installed |
| Once per login | The login sound, the first-run keybinding card, `mpd` when a configuration file exists for it, the offer of applications left pending by the installer, one `snooze` loop per row of the per-user timers in `~/.config/kdos/timers.d/`, and the session restore |

`kdos-desktop-start` then runs `kdos-comp` in the foreground, with its standard error kept in
`$XDG_RUNTIME_DIR/kdos-comp.log`. A clean exit of the compositor is a logout. A crash prints the
log's last 20 lines on the terminal and offers to restart the session; after three crashes within
60 seconds it stops offering.

[Boot and init](boot-and-init.md) covers everything up to the login prompt;
[The session](session.md) covers everything after it.

## The host and a box

An outer-ring application runs in a **box**: a rootless podman container. A box's root
filesystem is either a container image built on the machine by podman from the catalogue (the
box itself is created from that image by `distrobox`), or a stack of mounted **packs** (signed
EROFS filesystem images, described in [Packs and boxes](packs-and-boxes.md)). The installation
medium carries no applications. Each application installed on the machine is also a command on the
host: installing it writes a symbolic link named after it in `~/.local/bin`, pointing at
`kdos-appbox`, and a row in the per-user `alien-apps` table. `kdos-appbox` looks up the name it was
called by in that table, which maps an application name to the command line that runs it inside its
box, and runs that command in the right box.

A box shares more with the host than a sandbox built for isolation would. Its defaults are those
of a plain `distrobox create`, and a per-box profile in `~/.config/kdos/boxes/<box>.conf` takes
individual namespaces away. A profile is applied when the box is created, so changing one means
recreating the box.

| Shared by default | Profile key that takes it away |
|---|---|
| `$HOME`, at the same path | `home = private` gives the box its own directory under `~/.local/share/kdos/boxes/<box>` instead: a catalogue box receives it as `distrobox --home`; a pack box has it bound at its own path, with the host home not bound and `HOME` still naming the host path |
| `$XDG_RUNTIME_DIR` (`/run/user/<uid>`): the session bus, the compositor socket, PipeWire | None |
| `/tmp` and `/dev/shm` | None |
| `/dev` and `/sys` | `devices = private`; `gpu = yes` then binds `/dev/dri` back alone |
| The host's network, IPC and process namespaces | `network = private` or `none`, `ipc = private`, `processes = private` |
| The print service's socket in `/run/cups`, when it exists at creation | None |

What a box does not share is the host's root filesystem at its own paths. The box's `/usr` is
Debian's, so the host's `/usr/share/themes`, `/usr/share/icons` and libraries are not where an
application looks for them. (A box built by `distrobox` does see the host's root mounted at
`/run/host`; KDOS offers no setting that would claim to hide it.)

Four consequences follow.

Themes are written into `$HOME`, because the system theme directories are invisible to a
boxed application at the paths it reads. See [Theming](../02-user-guide/theming.md).

The session bus listens at a fixed path in `$XDG_RUNTIME_DIR`, which a box sees at the same
path as the host, so one address is valid on both sides. See
[The session message bus](session.md#the-session-message-bus).

A box reaches the host's services through portals. Choosing a file, opening a link with
another application and capturing the screen go through `xdg-desktop-portal`, whose KDOS back end
(`xdg-desktop-portal-kdos`) answers the file chooser, settings, application chooser and access
requests, and whose wlroots back end answers screen capture.

A boxed Wayland client is tagged by the compositor. `kdos-boxsock` binds one Wayland socket per
box and registers it with `kdos-comp` through the security-context protocol (the Wayland protocol
that lets a compositor mark every client arriving on one socket as sandboxed); every client
connecting on it sees only an allowlist of protocols, so screen capture, clipboard managers, input
methods and panel surfaces are refused unless the box's `grant` key names them. The tag holds only
for clients that connect through that socket; the session's own socket is in the shared runtime
directory too. See [Sandboxed clients](security-model.md#sandboxed-clients).

`kdos-boxsock` runs as the desktop user, one process per box, started detached by `kdos-appbox`;
it stays alive holding the security context open for the box's lifetime, so the tag lasts exactly
as long as the box.

## A running system

This is the process tree of a working desktop. Scripts marked "skipped until configured" are
installed but start nothing until their configuration file, account or data directory exists;
those marked "skipped when there is nothing to manage" stand down on a machine without the
hardware or filesystem they look after.

```
init (PID 1, toybox)
 │
 ├─ /etc/init.d/rcS ──────────── the numbered service scripts, in order
 │   ├─ 01 udev   02 modules   03 lvm   05 hostname   10 sysctl   12 zram
 │   ├─ 15 userdirs   18 timers   20 dmesg   22 syslog
 │   ├─ 25 nftables ── loaded before the network comes up
 │   ├─ 30 network   35 chrony   40 dbus   41 polkitd
 │   ├─ 42 modemmanager   42 networkmanager   45 avahi   45 seatd
 │   ├─ 47 pcscd   50 alsa   52 smartd   55 tlp   60 bluetooth
 │   ├─ 51 lsmd   62 virtlogd   63 libvirtd
 │   ├─ 70 sshd   80 cups   81 cups-browsed   82 ipp-usb
 │   ├─ skipped when there is nothing to manage: 43 boltd   51 mdmonitor
 │   │    53 xfs_healer   54 thermald   64 lircd
 │   ├─ skipped until configured: 31 babeld   46 hostapd   56 nut
 │   │    63 gssd   65 brltty   71 snmpd   72 nfsd   73 mosquitto   74 prosody
 │   │    75 mumble-server   76 postgresql   77 radicale   78 maddy
 │   │    79 ngircd   83 samba   84 minidlna   85 gnuhealth
 │   │    86 kiwix-serve   87 kolibri   88 llama-server   89 step-ca
 │   └─ the KDOS root daemons:
 │        55 kdos-powerd    /run/kdos-powerd.sock    power, and system settings
 │        56 kdos-energyd   /run/kdos-energyd.sock   per-application energy
 │        57 kdos-oomd      /run/kdos-oomd.sock      memory-pressure protection
 │        58 kdos-mountd    /run/kdos-mountd.sock    removable media and disks
 │        59 kdos-packd     /run/kdos-packd.sock     mounting application packs
 │
 ├─ dbus-daemon --session   one per user, forked away from the session
 │   └─ xdg-desktop-portal-kdos   started by the bus on first request
 │
 ├─ kdos-boxsock <box>        one per running box, detached from the launch
 │
 ├─ kdos-getty tty1 → kdos-login → agetty → login
 │   └─ bash (login shell)
 │       └─ kdos-desktop → kdos-desktop-start        ← the session
 │           ├─ pipewire, wireplumber, pipewire-pulse
 │           ├─ xdg-desktop-portal-wlr, xdg-desktop-portal
 │           ├─ kdos-ime, fcitx5          if an input method is installed
 │           ├─ mpd, kdos-mpctl watch     if mpd is configured
 │           └─ kdos-comp                 the compositor
 │               ├─ kdos-shell  per output   the panel
 │               ├─ kdos-desk   per output   desktop icons
 │               ├─ kdos-slit   per output   dockapps, off by default
 │               ├─ kdos-notifyd    one      notifications
 │               ├─ kdos-netagent   one      NetworkManager secrets
 │               ├─ kdos-mediad     one      removable-media toasts
 │               ├─ kdos-clip       one      clipboard history
 │               ├─ Xwayland        started when the first X11 client connects
 │               └─ application clients, host and boxed
 │
 ├─ kdos-getty tty2 → getty ── an ordinary login: the recovery console
 │
 └─ bash -l on ttyS0          once a key is pressed on the serial line
```

`rcS` runs every executable `/etc/init.d/NN_name.sh` in the order the shell sorts them, skipping
any whose name has a marker file in `/etc/service.disabled/`; `rcK`, which `/etc/inittab` runs at
shutdown, stops the same set in reverse, except the firewall ruleset, which stays loaded through
shutdown. Forty of the scripts ship in the image's `fs/` tree, and twenty-two more are installed by
the ports they start, such as `43_boltd` by `bolt` and `83_samba` by `samba`. A script that
starts a long-running daemon runs it under `ksvc`, KDOS's service supervisor, which keeps the
daemon in the foreground and restarts it when it exits; the one-shot scripts (modules, sysctl,
zram, the firewall ruleset and the like) do their work and return. The full list is in
[rcS and the service scripts](boot-and-init.md#rcs-and-the-service-scripts); see also
[The daemons](../04-programs/daemons.md).

Three other names in the tree: `seatd` is the seat manager, which hands the display and input
devices to the logged-in session; `kdos-slit` draws the slit, an optional column of small gadgets
(dockapps); and Xwayland is an X11 server that runs as a Wayland client, so that a program that
speaks only X11 can still open a window.

### How the screen is drawn

Every surface KDOS paints is a grid of character cells, and the compositor puts those surfaces on
the screen. `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-lock` and the portal's file chooser each
paint their own cell buffer through `libkwl`, the toolkit's Wayland back end, and hand it over
as an ordinary Wayland surface. `libkdisp`, the display-selection library, is the one place a
program chooses its display server, and one implementation of it ships: Wayland, through
`libkwl`. The same interface is the seam the test suite renders through
with no compositor at all.

Two things on the screen are not cells:

- **The compositor's own chrome** — titlebars, the root menu, the window-switcher display — is
  drawn by `kdos-comp` with pango, as nodes of its scene graph rather than client surfaces. The
  face is `Terminus (TTF)` at 24 points (set in `~/.config/kdos-comp/rc.xml`), which is 32 pixels
  at 96 dpi and so exactly one cell high, and the corner radius is zero. The frames therefore sit
  on the grid even though they are not drawn on it.
- **An application**, native or inside a box, draws whatever its own toolkit draws.

### What the compositor supervises

`kdos-comp` starts and supervises the desktop's own chrome from a table of seven entries in
`src/desktop/kdos-comp/src/kdos-child.c`. It respawns an entry when it exits and stops a
per-output entry when its output goes away. Whether an optional entry runs is read from
`~/.config/kdos/comp.conf` once, at startup, so switching one on or off takes effect at the next
login.

| Program | Instances | Runs | Why that many |
|---|---|---|---|
| `kdos-shell` | One per output | Unless `panel = off` | A layer surface (a Wayland surface anchored to a screen edge, such as a panel) belongs to one screen, and the toolkit keeps one cell buffer per process, so a second monitor needs a second process |
| `kdos-desk` | One per output | When desktop icons are on (the default) | As above |
| `kdos-slit` | One per output | When the slit is on (off by default) | As above |
| `kdos-notifyd` | One | Always | Owns the bus name `org.freedesktop.Notifications` |
| `kdos-netagent` | One | Always | Registers one secret agent with NetworkManager |
| `kdos-mediad` | One | Always | Holds one subscription to `kdos-mountd` |
| `kdos-clip` | One | When clipboard history is on (the default) | Owns one socket and holds the history in memory |

A child that fails more than five times inside thirty seconds is not restarted again, so a crash
loop cannot bury the log line that explains it. The compositor raises a `kdos-prompt` dialog
naming what it gave up on. Any reload of the configuration (`kdos-comp -r`, `SIGHUP` to the
compositor, or `kdos theme`) resets the counters and gives each child one more run of attempts.
The compositor's root menu also has a Reload Configuration item, but a right click on the
wallpaper reaches that menu only with `desktop_icons = no`; with desktop icons on (the default)
`kdos-desk` owns the wallpaper and its menu has no reload row. See
[Supervised children](../04-programs/kdos-comp.md#supervised-children).

## The root daemons

Five KDOS daemons run as root. Each is supervised by `ksvc` as a foreground process and answers
one Unix socket in `/run`, one line per request. Who may ask is decided by the connecting
process's credentials as the kernel reports them (`SO_PEERCRED`: its user and groups), not by the
socket file's permissions.

| Daemon | Owns | Answers to |
|---|---|---|
| `kdos-powerd` | Suspend, poweroff and reboot; the time zone, the accent, autologin and the firewall's service list | root and `wheel`; also `seat` for `ping`, suspend, poweroff and reboot |
| `kdos-energyd` | Sampling the CPU's RAPL (running average power limit) energy counter and attributing it to applications | root and `wheel` |
| `kdos-oomd` | Killing the largest non-desktop process when the kernel's pressure-stall information (PSI) says the machine is stalling | root, `seat` and `wheel` |
| `kdos-mountd` | Removable media, encrypted (LUKS) volumes, formatting, SMB shares and disk health (SMART) | root, `seat` and `wheel` |
| `kdos-packd` | Mounting, installing, verifying and composing application packs | root and `wheel` |

`seat` is the group `seatd` gives the display to, so it is the person at the machine; `wheel` is
the administrators' group, the same one `sudo` and polkit treat as admin. A non-administrator can
therefore close the lid and shut the machine down, and cannot open a firewall port or change who
logs in.

No client of a root daemon names a path. Every request takes an identifier from a list the daemon
itself published, or from a list compiled into it; `kdos-packd`'s `install` takes a file name
inside a staging directory the daemon owns. There is nothing to aim, which is what stops a daemon
the desktop user can reach from becoming a way to mount a stick over `/etc`. See
[The daemons](../04-programs/daemons.md) and
[Root daemons](security-model.md#root-daemons).

Most administration goes through one command, `kdos`: `kdos update` updates the host packages,
`kdos app` installs and removes boxed applications, `kdos theme` sets the accent, and
`kdos march` builds a port twice to measure whether a per-machine optimisation pays. The
graphical settings program, `kdos-settings`, is another name for `kdos-shell`, and
`kdos settings` opens it. See [The kdos command](../04-programs/kdos-command.md).

## The two packaging systems

KDOS has two packaging systems because they answer different questions. Host packaging answers
*what is installed on this machine*. Application packaging answers *what software this machine
can run, and how to get a piece of it without installing anything into the system*.

| | Host packages | Applications |
|---|---|---|
| Unit | A compiled port | A container image, or a pack |
| Built by | `kpkg`, from a recipe | podman, from the catalogue, on this machine |
| Format | A reproducible `<name>-<version>-<release>.tar.xz` archive | Container image layers; EROFS with a signed footer when exported as packs |
| Installed by | `kpkg`, as root, into `/` | `kdos-appbox`, as a box; `kdos-packd` for an imported pack |
| Verified by | The recipe's `sha256` for the source, plus a signature when installing from a binary host (a server of prebuilt, signed packages) | Nothing KDOS controls, when built here; the payload hash and then the signature at mount time, when imported |
| Updated with | `kdos update`: a rebuild, or a signed binary host, installed into the other A/B slot where the machine has one | A rebuild against the catalogue's Debian snapshot |
| Trusted keyring | `/etc/kdos/keys` (empty as shipped) | `/etc/kdos/keys/packs` |

Building an application fetches from Debian's archive at a pinned snapshot, which apt verifies,
but nothing KDOS controls attests to the result. An exported set of applications is hashed and
signature-checked by `kdos-packd` at the point where it is mounted.

The two keyrings are separate by construction. A pack-signing key says who exported a set of
applications; placing it in the host keyring would make it a trusted publisher of host packages
as well. The keyring loader reads `*.pub` in one directory and does not descend into
subdirectories, so `/etc/kdos/keys/packs` is a separate policy. The image ships one key there,
which vouches only that the packs on a medium came from the same build, and ships no key in the
host keyring, because host packages are built from source on the machine and their integrity
comes from the recipe's `sha256`.

See [Packaging](packaging.md) and [Packs and boxes](packs-and-boxes.md).

## Where state lives

| Path | Holds | Written by |
|---|---|---|
| `/var/lib/kpkg/db` | The installed-package database and manifests | `kpkg`, as root |
| `/var/lib/kdos/packs` | Imported application packs | `kdos-packd`, as root |
| `/var/lib/kdos/packs/staging` | The one place an unprivileged write may land, mode 01777 | Whoever imports a pack |
| `/var/lib/kdos/packs/mnt` | Mount points for packs | `kdos-packd` |
| `/var/lib/kdos/pack-manifest` | Every graft made (a pack's files placed where a consumer looks for them), so removal is exact | `kdos-packd` |
| `/var/lib/kdos/fs-manifest` | Every path the build's `fs/` tree provided | The build |
| `/var/lib/kdos/apps-pending` | Applications chosen at install time and not yet built | `kinstall`; read by `kdos app install --pending` |
| `/var/lib/kdos/update.json` | The result of the last update check | The `update-check` timer, through `kdos update check` |
| `/var/lib/kdos/march.ledger` | Measured per-machine optimisation results | `kdos march` |
| `/etc/kdos/` | Machine configuration (`login.conf`, `packd.conf`, `zram.conf`, `timers.d/`, the system `accent`) and the trusted keys | The administrator, and `kdos-powerd` for the accent and autologin |
| `/usr/share/kdos/` | Shipped and generated data: icon art, the `alien-apps` table, the application catalogue, the security database, the documentation | The build |
| `~/.config/kdos/` | Desktop configuration, including `comp.conf` and the box profiles in `boxes/` | The user, and `kdos-settings` |
| `~/.config/kdos-comp/` | The compositor's `rc.xml` and root menu | The user |
| `~/.local/share/kdos/` | The per-user `alien-apps` table, pack grafts, and per-box homes and upper layers | `kdos-appbox`, `kdos-packd` |
| `$XDG_CACHE_HOME/kdos/` | The accent state file and the retinted wallpaper | `kdos theme` |
| `$XDG_RUNTIME_DIR/` | Sockets, the session bus, the compositor's log, launch traces | Everything, per session |

Two of those entries are interfaces rather than storage:

- `$XDG_CACHE_HOME/kdos/theme` holds a single word — the accent name — and that word is the whole
  of the theme state the desktop reads; an absent file means the default accent.
- `alien-apps` maps an application name to the command line that runs it inside its box, which is
  how a shim in `~/.local/bin` knows what to run. Installing an application writes its row to the
  per-user copy at `~/.local/share/kdos/alien-apps`, which the dispatcher reads first. The system
  copy, `/usr/share/kdos/alien-apps`, and its shims in `/usr/local/bin` are written by the build
  from the packs baked into the image; the shipped image bakes none, so that table has no rows.

The full list of paths and sockets is in [Filesystem and IPC](../06-reference/filesystem-and-ipc.md).

## See also

- [Why KDOS](../01-philosophy/why-kdos.md) — the properties this structure serves
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — the same choices compared with other distributions
- [Boot and init](boot-and-init.md) — how the machine gets to a login prompt
- [The session](session.md) — what happens when `kdos-desktop` runs
- [Packaging](packaging.md) — how host software is built and installed
- [Packs and boxes](packs-and-boxes.md) — how applications are packaged and run
- [The security model](security-model.md) — who is allowed to do what, and what is not protected
- [The design language](design-language.md) — why every surface looks the same
- [The window model](window-model.md) — where a window lands and how it tiles
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build side of this map, from clone to ISO
- [The ports catalogue](../06-reference/ports-catalogue.md) — every core-ring port, by shelf, with its phase
- [The kdos command](../04-programs/kdos-command.md) — the administrative command and its subcommands
- [Repository layout](../06-reference/repository-layout.md) — where each ring lives in the tree
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md) — every path and socket in full
- [The daemons](../04-programs/daemons.md) — each root daemon in detail
- [Glossary](../06-reference/glossary.md) — the terms this page uses

<!-- book-nav -->
---

*Part III — Architecture, chapter 12.* Previous: [11. Accessibility](../02-user-guide/accessibility.md) · [Contents](../README.md) · Next: [13. Boot and init](boot-and-init.md)
