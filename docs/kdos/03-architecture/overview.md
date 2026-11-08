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
| Core | `ports/core/` | musl, the GNU userland, sysvinit, elogind, the toolchain, the Linux kernel, Limine, podman and distrobox, the libraries and tools of the base system, the GUI toolkits (GTK, Qt, KDE Frameworks, wxWidgets, FLTK, Tk), KDE Plasma and its compositor KWin, the SDDM login screen, every KDE Gear application, and the other applications ported natively | Compiled on the build machine from upstream source archives, which `make fetch` downloads and verifies against each recipe's `sha256` |
| Desktop | `src/desktop/`, `src/daemons/`, `src/system/`, `src/art/`, `src/libs/` | KDOS's own code: the settings pages, panel widgets, KWin effects and scripts, the Discover backend and the graphical installer that make Plasma a KDOS desktop; the root daemons; the `kdos` command, the package manager, the box runtime and the installer; the colour, icon, wallpaper, sound, splash and login-screen art; and the `libk*` C libraries they share | Compiled on the build machine from this repository's own source |
| Outer | `src/system/kdos-appbox/catalogue` | Applications KDOS does not port natively, and alternatives to those it does: browsers, office suites, CAD, media tools, IDEs, games | Declared as Debian packages; built by podman on the machine that asks for them |

A **port** is one unit of the first two rings: a directory holding a recipe (`kpkgbuild`,
declarative metadata that is parsed and never sourced) and a `build.sh` beside it. `kpkg`, the
package manager, builds a port into a package and installs it. In the core ring, musl is the C
library, the GNU tools (coreutils, grep, sed, gawk, findutils), each a separate program, are the
base system's command line, and sysvinit's `init` is PID 1.

The boundary between the rings follows ownership. The desktop is KDE Plasma 6.8, built from
upstream's release tarballs like any other port: KWin is the Wayland compositor and window manager,
`plasmashell` draws the panels and the desktop, System Settings and Discover are the settings and
software centres, and the KDE Gear applications (Dolphin, Konsole, Kate, Okular and the rest) are
the everyday programs. KDOS does not fork any of it. What makes it KDOS is in the desktop ring: data
packages Plasma already knows how to load (colour schemes, global themes, a Plasma style, desk
layouts, an SDDM theme), Qt and QML code written against Plasma's own plug-in interfaces (settings
modules, widgets, a KRunner runner, KWin effects and scripts, a Discover backend), a handful of
patches carried against KDE modules where no setting exists, and the root daemons those pages talk
to. [Principles](../01-philosophy/principles.md) gives the reasons.

Applications, the programs a person opens to do work unrelated to the operating system, come from
either of two places. A natively ported application is a core-ring recipe like any other: Firefox
ESR, Thunderbird, LibreOffice, GIMP, Inkscape, Krita, KiCad, FreeCAD and VLC are among them, beside
all of KDE Gear. The toolkits and the applications build in two phases of their own: `43_toolkits`
builds GTK 3 and 4, Qt 5 and 6, KDE Frameworks, wxWidgets, FLTK and Motif with every library over
them that something else links (VLC is one, since other programs depend on it, and so are the
Plasma libraries the Gear applications link), and `44_apps` builds the applications nothing
depends on. Tk needs nothing past the X11 libraries and is built earlier, in `42_graphics`. A
toolkit with a Wayland back end is built with it as the default and its X11 back end compiled in;
Tk and Motif have only an X11 back end, and Xwayland serves them and any other program that has only
an X11 path. Anything not ported natively is in the outer ring and runs in a container rather than
on the host.

The sizes, counted as directories holding a `kpkgbuild` (and, for the catalogue, lines whose
first field is `app`):

| Where | Count |
|---|---|
| `ports/core` | about 2,400 recipes, on 102 shelves |
| `src/system`, `src/art`, `src/desktop`, `src/daemons` | about forty recipes |
| All port repositories | about 2,400 recipes; the exact counts are in [Status](../06-reference/status.md#scale) |
| Catalogue `app` rows (outer ring) | 73 |

`ports/core` files each port one level down, on a **shelf** named for its subject:
`ports/core/<shelf>/<name>/`, such as `ports/core/kde/kwin/` or `ports/core/fonts/noto-fonts/`.
The closed list of 102 shelves, one line each with its source-archive volume and what belongs on
it, is the file `ports/shelves`. A shelf is only where a recipe is filed. It subdivides the core
ring and is not a ring of its own; a port is known everywhere by its bare name, and no dependency
line, package or database entry records its shelf.

The four `src/` areas that hold recipes are port repositories in their own right and use the same
two-file recipe format as `ports/core`, so building the desktop is not a special case anywhere in
the build system. Each area holds its ports directly, with no shelves. `kpkg` searches the
repositories named in `PORT_REPO`, an environment variable that overrides the same key in
`/etc/kpkg.conf`. The shipped file names `/ports/core` and the four areas under `/ports/src`, which
is where an installed machine keeps its recipe tree. Inside the build's chroot `/ports` is the
repository's own `ports/` directory, which has no `src/`, so the early phases see only
`/ports/core`, and the environment file of each later phase sets the variable to add the areas
from the repository mounted at `/kdos`:

| Phases (`script/phases/<phase>/phase.env`) | `PORT_REPO` |
|---|---|
| `20_selfhost`, `30_foundation`, `31_compilers` | Not set: `/etc/kpkg.conf`'s list, of which only `/ports/core` exists in the chroot |
| `40_lang` to `44_apps`, `60_kernel` | `/ports/core /kdos/src/system /kdos/src/art` |
| `50_desktop` | `/ports/core /kdos/src/system /kdos/src/art /kdos/src/desktop /kdos/src/daemons` |

`src/desktop` and `src/daemons` join the search only in `50_desktop` because their code links
KWin, `libplasma`, Discover's library and the Plasma session, which that phase installs, and the
daemons serve that session. `src/libs/` holds no recipes: each port of KDOS's own compiles the
library sources it needs into its own binaries. `src/devtools/` holds none either: `kdosbuild` and
`kdos-portup` run on the build machine and are never installed. `src/system/kdos-kpkg`, the package
manager itself, is the one directory in a port area without a recipe: the bootstrap step
`script/phases/10_bootstrap/120_kpkg.sh` compiles it, because it has to exist before any recipe can
be read. How the phases follow one another is the subject of
[How KDOS is built](../05-developer/how-kdos-is-built.md), and how the package lists of `40_lang`
to `44_apps` are split into one file per shelf under `packages.d/` is in its section
[The userland](../05-developer/how-kdos-is-built.md#the-userland-40_lang-to-44_apps); every port,
listed by shelf, is in [The ports catalogue](../06-reference/ports-catalogue.md).

## From power-on to a drawn window

A KDOS machine reaches a usable desktop through a chain of handoffs, each program starting or
executing the next. There is no service manager in the chain: PID 1 is sysvinit's `init`, which
runs one start-up script and keeps the display manager and the consoles alive. elogind tracks who
is logged in and hands the screen and the input devices to the session at the front; it starts
nothing.

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
init (sysvinit, PID 1), reading /etc/inittab
   ├─ /etc/init.d/rcS ── fsck and mount, the numbered service scripts, then the
   │                     splash fades to black and lets go of the screen
   ├─ kdos-dm tty1 → sddm ─────────────── the display manager
   │     ├─ the login screen: sddm-greeter-qt6 on a kwin_wayland of its own
   │     └─ after a login, through PAM (elogind, turnstile, the wallet):
   │        kdos-session-wrapper → startplasma-wayland
   │           ├─ kwin_wayland_wrapper → kwin_wayland ── the compositor
   │           └─ plasma_session → ksmserver, kded6, plasmashell, …
   ├─ kdos-getty tty2 → agetty ── the recovery console
   └─ serial-getty ttyS0 ──────── the serial line: root on the live medium, a login when installed
```

The terms in that chart, in order:

| Term | Meaning |
|---|---|
| ESP | The EFI system partition: the small FAT partition UEFI firmware loads boot programs from |
| Limine | The boot loader. One Limine configuration and menu serve both UEFI and BIOS machines — see [Limine and the kernel command line](boot-and-init.md#limine-and-the-kernel-command-line) |
| Microcode | The processor vendor's update for the CPU itself. The kernel is built without late loading, so the early initramfs is the only path — see [Microcode](boot-and-init.md#microcode) |
| initramfs | A small in-memory filesystem the kernel starts first; its `init` finds, unlocks, checks and mounts the real root — see [The initramfs](boot-and-init.md#the-initramfs) |
| A/B slots | Two root partitions on a machine set up for them: an update is written into the other slot and tried, and the slot known to work stays the fallback — see [A/B slot selection](boot-and-init.md#ab-slot-selection) |
| LUKS | Linux disk encryption; an encrypted root is unlocked in the initramfs, before it can be mounted — see [Unlocking an encrypted root](boot-and-init.md#unlocking-an-encrypted-root) |
| LVM | The Linux logical volume manager; a root on a logical volume needs its volume group activated before it can be found — see [Activating volume groups](boot-and-init.md#activating-volume-groups) |
| `switch_root` | The call that makes the real root filesystem `/` and runs its `init`. KDOS uses util-linux's, which moves the new root onto `/`; one that only `chroot()`s leaves a root no container can later start under — see [`switch_root`](boot-and-init.md#switch_root) |
| SDDM | The display manager: the daemon that shows the login screen, checks the password through PAM and starts the chosen session as that user |
| elogind | The login and seat tracker, `org.freedesktop.login1` on the system bus: it records each session, hands the active one its display and input devices, and carries suspend, power-off and the lid. It is the one systemd-derived program on the host, run standalone |
| turnstile | The per-user service manager: at a user's first login it makes `/run/user/<uid>` and starts that user's services (the session bus and the audio server) under `ksvc`, and stops them at the last logout |

### The console and the splash

From the initramfs until `rcS` finishes, `kdos-splash` draws the boot progress on the
framebuffer, and `rcS` reports one step to it per enabled service script. The passphrase prompt
for an encrypted root is drawn through the splash. At its end `rcS` leaves the first of the two
marks that confirm a tried A/B slot, then tells the splash to hand the screen over: the splash
fades to black and lets go of the framebuffer before `rcS` returns, so the login screen's
compositor, which `init` starts only after that, never competes with it for the display. The
second mark is written once a desktop session has answered for a minute, and `kdos-sysupdated`
confirms the slot only when both exist. Where kernel messages go, how the splash survives the
change of root, and how a slot is confirmed are in [The splash](boot-and-init.md#the-splash),
[Command-line parameters KDOS reads](boot-and-init.md#command-line-parameters-kdos-reads) and
[Counting and confirming](boot-and-init.md#counting-and-confirming).

### The login

`/etc/inittab` runs `kdos-dm` on `tty1`, and `kdos-dm` executes SDDM. SDDM's login screen is a QML
greeter in the KDOS theme, running on a `kwin_wayland` of its own with no X server anywhere on the
path. The password is checked through PAM: `pam_elogind` registers the session with elogind,
`pam_turnstile` makes the user's runtime directory and, at the first login, starts the user's
services, and `pam_kwallet5` keeps the password to unlock the wallet. SDDM then runs the session
as that user through `kdos-session-wrapper`.

Autologin is SDDM's alone, set in `/etc/sddm.conf.d/kde_settings.conf`: the live medium logs the
account `kdos` straight in, and an installed machine shows the login screen. The one exception is
a machine with an encrypted root and a single account whose installer was asked for autologin:
the disk passphrase has already been typed, so SDDM logs the account in and the session starts
behind the lock screen. If SDDM is missing, cannot run its greeter, or exits three times inside a
minute, `kdos-dm` puts a password prompt on `tty1` instead, never an autologin. `tty2` is a plain
`agetty` and stays one: it is the recovery console to switch to when the desktop on `tty1` does not
come up, which includes a greeter that SDDM keeps restarting without ever exiting. The serial port
`ttyS0` logs root in with no password on the live medium, for debugging, and asks for a login on an
installed system. See [The console](boot-and-init.md#the-console).

### The session

`kdos-session-wrapper` is SDDM's session command. It runs the user's login profile, so
`/etc/profile.d` reaches Plasma as it reaches a console login; it insists that the user's session
bus answers at the fixed address `$XDG_RUNTIME_DIR/bus`, the one every box is given, and refuses to
start the session when it does not; it sets `XDG_CURRENT_DESKTOP=KDE` and puts KDOS's defaults on
`XDG_CONFIG_DIRS`; it starts the boxes behind the pinned applications warming up; and then it runs
`startplasma-wayland`. Plasma starts the classic way, without systemd (`startkderc`
`systemdBoot=false`): `startplasma-wayland` runs KWin under `kwin_wayland_wrapper`, which holds the
Wayland socket and restarts KWin after a crash so clients reconnect, and `plasma_session` starts
the session manager `ksmserver`, the module host `kded6`, `plasmashell` and everything in the
autostart directories.

The user's own services are not started by the session but by turnstile, before the session
begins: the scripts in `/etc/kdos/user-services` (the session bus, PipeWire, WirePlumber,
`pipewire-pulse`, the user's timers, `mpd` when it has a configuration, and one that starts nothing
and stops the user's containers at the end) run under `ksvc` exactly as
the system's do under `rcS`, live as long as the user has any login, and stop at the last logout
unless the user lingers. [The session](session.md#starting-a-session) covers the session from the
PAM stack to the logout, and [Boot and init](boot-and-init.md) everything up to the login screen.

## The host and a box

An outer-ring application runs in a **box**: a rootless podman container. A box's root
filesystem is either a container image built on the machine by podman from the catalogue, or a
stack of mounted **packs** (signed EROFS filesystem images, described in
[Packs and boxes](packs-and-boxes.md)); either way `kdos-appbox` creates the container itself with
`podman create`. A box made with `kdos-box create` from any other container image is created by
`distrobox` instead. The installation medium carries no applications. Each application installed on the machine is also a command on the
host and an entry in the application menu: installing it writes a desktop entry, a symbolic link
named after it in `~/.local/bin` pointing at `kdos-appbox`, and a row in the per-user `alien-apps`
table. `kdos-appbox` looks up the name it was called by in that table, which maps an application
name to the command line that runs it inside its box, and runs that command in the right box.

A box shares more with the host than a sandbox built for isolation would. Its defaults are the
flags a plain `distrobox create` passes, and a per-box profile in
`~/.config/kdos/boxes/<box>.conf` takes individual namespaces away. The namespace keys are applied when the box is created, so changing
one means recreating the box; the profile's `grant` key, described below, is read by KWin instead.

| Shared by default | Profile key that takes it away |
|---|---|
| `$HOME`, at the same path | `home = private` gives the box its own home: a directory of the box's own, bound at the home's path |
| A runtime directory at `/run/user/<uid>`: the box's own, `$XDG_RUNTIME_DIR/kdos/box/<box>`, holding its tagged Wayland socket, its filtered session bus and links to the audio sockets, and never the session's own socket or bus | `wayland`, `audio` and `x11` decide which sockets are in it. A box distrobox made from another image sees the session's own directory, and no key changes that |
| `/tmp` and `/dev/shm` | None |
| `/dev` and `/sys` | `devices = private`; `gpu = yes` then binds `/dev/dri` back alone |
| The host's network, IPC and process namespaces | `network = private` or `none`, `ipc = private`, `processes = private` |
| The print service's socket in `/run/cups`, when it exists at creation | None |

What a box does not share is the host's root filesystem at its own paths. The box's `/usr` is
Debian's, so the host's `/usr/share/themes`, `/usr/share/icons` and libraries are not where an
application looks for them. (A box built by `distrobox` does see the host's root mounted at
`/run/host`; KDOS offers no setting that would claim to hide it.)

Four consequences follow.

A boxed application takes the desktop's look from what it can reach: the colour scheme and the
light or dark preference through the portal's Settings interface, and the KDE runtime row of the
catalogue installs Plasma's own platform theme into boxes that want it. See
[Theming](../02-user-guide/theming.md).

A box reaches the session bus at the fixed path in its runtime directory, and what answers there
is a proxy, `xdg-dbus-proxy`, that passes the portals, notifications and the names the box's own
applications own, and hides the rest of the desktop's bus (KWin's scripting and screenshot
interfaces, the global shortcuts, the clipboard history, the panel's scripting) by name.

A box reaches the host's services through portals. Choosing a file, opening a link with another
application, capturing the screen and asking for access go through `xdg-desktop-portal`, whose KDE
back end, `xdg-desktop-portal-kde`, draws the dialogs in Plasma's own look. See
[Portals](session.md#portals).

A boxed Wayland client is tagged by KWin. `kdos-boxsock` binds one Wayland socket per box launcher
and registers it with KWin through the security-context protocol (the Wayland protocol that lets a
compositor mark every client arriving on one socket as sandboxed). Every client connecting on it
is refused the window list, fake input, screen capture, clipboard managers, the panel and overlay
surfaces and the rest of KWin's privileged globals unless the box's `grant` key names them, its
windows carry the box's launcher id as their application id, and their title bar shows a box glyph
in the box's colour. The tag holds only for clients that connect through that socket, which is why
a box's own runtime directory holds no other. See
[Sandboxed clients](security-model.md#sandboxed-clients).

`kdos-boxsock` runs as the desktop user, one process per box launcher, started detached by
`kdos-appbox` at the box's first launch in a session. It holds the security context open until
KWin exits, so the tagged socket outlives a stopped box and ends with the session that served it;
the next launch after a restart binds a new one.

## A running system

This is the process tree of a working desktop, as the live medium runs it. Scripts marked "skipped
until configured" are installed but start nothing until their configuration file, account or data
directory exists; those marked "skipped when there is nothing to manage" stand down on a machine
without the hardware or filesystem they look after.

```
init (PID 1, sysvinit)
 │
 ├─ /etc/init.d/rcS ──────────── the numbered service scripts, in order
 │   ├─ 01 udev   02 modules   03 lvm   05 hostname   06 utmps   10 sysctl
 │   ├─ 12 zram   15 cgroups   18 timers   20 dmesg   22 syslog
 │   ├─ 25 nftables ── loaded before the network comes up
 │   ├─ 35 chrony   40 dbus   40 elogind   41 polkitd   44 accounts
 │   ├─ 42 modemmanager   42 networkmanager   45 avahi
 │   ├─ 47 pcscd   50 alsa   52 smartd   55 tlp   56 tlppd   60 bluetooth
 │   ├─ 51 lsmd   61 spacenavd   62 virtlogd   63 libvirtd
 │   ├─ 65 turnstiled   80 cups   81 cups-browsed   82 ipp-usb   85 crond
 │   ├─ standing down while NetworkManager runs: 30 network (dhcpcd)
 │   ├─ disabled as shipped: 70 sshd
 │   ├─ on the live medium only: 63 live-installer; on an OEM install only: 62 plasma-setup
 │   ├─ skipped when there is nothing to manage: 43 boltd
 │   │    44 switcheroo-control   48 iio-sensor-proxy   51 mdmonitor
 │   │    53 xfs_healer   54 thermald   64 lircd
 │   ├─ skipped until configured: 31 babeld   46 hostapd   56 nut
 │   │    63 gssd   65 brltty   71 snmpd   72 nfsd   73 mosquitto   74 prosody
 │   │    75 mumble-server   76 postgresql   77 radicale   78 maddy
 │   │    79 ngircd   83 samba   84 minidlna   85 gnuhealth
 │   │    86 kiwix-serve   87 kolibri   88 llama-server   89 step-ca
 │   └─ the KDOS root daemons:
 │        43 kdos-settingsd    hostname1, timedate1, locale1 on the system bus
 │        46 kdos-sysupdated  sysupdate1 and io.kdos.Slots1: updates and slots
 │        55 kdos-powerd      /run/kdos-powerd.sock and io.kdos.Power1
 │        56 kdos-energyd     /run/kdos-energyd.sock   per-application energy
 │        57 kdos-oomd        /run/kdos-oomd.sock      memory-pressure protection
 │        59 kdos-packd       /run/kdos-packd.sock     mounting application packs
 │
 ├─ turnstiled ─── per user, from the first login to the last logout:
 │   └─ the ksvc backend → dbus-daemon --session, pipewire, wireplumber,
 │                          pipewire-pulse, the user's timers, mpd if configured
 │
 ├─ kdos-boxsock <box>        one per box launcher used this session, detached from the launch
 │
 ├─ kdos-dm → sddm             on tty1
 │   └─ sddm-helper → kdos-session-wrapper → startplasma-wayland     ← the session
 │       ├─ kwin_wayland_wrapper → kwin_wayland   the compositor
 │       │   │                    (global shortcuts are served inside it)
 │       │   ├─ Xwayland          started when the first X11 client connects
 │       │   └─ fcitx5            the input method, when one is installed
 │       └─ plasma_session
 │           ├─ ksmserver, kded6, kaccess
 │           ├─ plasmashell       the panels, the desktop, notifications
 │           ├─ polkit-kde-authentication-agent-1, org_kde_powerdevil, …
 │           └─ application clients, host and boxed
 │   (xdg-desktop-portal and xdg-desktop-portal-kde are started by the session bus on first request)
 │
 ├─ kdos-getty tty2 → agetty ── an ordinary login: the recovery console
 │
 └─ serial-getty ttyS0        root on the live medium, a login prompt when installed
```

`rcS` runs every executable `/etc/init.d/NN_name.sh` in the order the shell sorts them, skipping
any whose name has a marker file in `/etc/service.disabled/`; `rcK`, which `/etc/inittab` runs at
shutdown, stops the same set in reverse, except the firewall ruleset, which stays loaded through
shutdown. Forty-nine of the scripts ship in the image's `fs/` tree, and twenty-two ports install
twenty-four more, such as `43_boltd` by `bolt` and `83_samba` by `samba`. The image ships `sshd`
with its marker in place; the installer offers NetworkManager, Bluetooth, the mixer state, CUPS
and `sshd` as choices, and leaves CUPS and `sshd` off unless they are switched on
([Answer files](../04-programs/kinstall.md#answer-files) lists them). A script that starts a
long-running daemon runs it under `ksvc`, KDOS's service supervisor, which keeps the daemon in the
foreground and restarts it when it exits; the one-shot scripts (modules, sysctl, zram, the
firewall ruleset and the like) do their work and return. A daemon that owns a name on the system
bus is started by its script and never by the bus, so there is one supervised copy of it. The full
list is in [rcS and the service scripts](boot-and-init.md#rcs-and-the-service-scripts); see also
[The daemons](../04-programs/daemons.md).

`cronie`, at `85_crond`, runs only users' own crontabs; the timers the system ships are `snooze`
loops under `18_timers`, read from `/etc/kdos/timers.d`, so the two schedulers never share a job.

### How the screen is drawn

KWin draws the screen. It takes the display through elogind, composites every window with OpenGL
over EGL on the kernel's DRM device, and puts the result on each output; a fullscreen window that
can be shown as it is goes to the display directly. On a machine or virtual machine with no GPU
rendering, Mesa's software renderer, llvmpipe, does the same work on the processor, and KWin turns
off the effects that would cost too much there.

Everything else on the screen is a Wayland client of KWin. `plasmashell` draws the panels, the
desktop, the notifications and the widgets in QML with Qt Quick, in the `kdos` Plasma style and the
accent's colour scheme; System Settings, Discover, the KDE applications and KDOS's own settings
pages draw with Qt and Kirigami in the same scheme; a GTK application draws in the Breeze GTK theme
KDE keeps in step with the scheme; an X11-only program draws through Xwayland, and a boxed
application draws whatever its own toolkit draws. Window title bars are KWin's decoration, Breeze,
configured square and coloured from the scheme. The whole of that look is in
[The design language](design-language.md), and where a window goes in
[The window model](window-model.md).

**Phosphor mode** is the one exception to "KWin draws what the clients drew". It is an opt-in
full-screen effect built into KWin, `kdos_crt`, that draws every output through a CRT shader, with
three window-motion effects beside it. It is off by default, needs GPU rendering, and is switched by
`kdos phosphor`; see [Phosphor mode](design-language.md#phosphor-mode).

## The root daemons

Six KDOS daemons run as root, each supervised by `ksvc` as a foreground process. Four answer a Unix
socket in `/run`, one line per request, and decide who may ask by the connecting process's
credentials as the kernel reports them (`SO_PEERCRED`: its user and groups), not by the socket
file's permissions. Three serve the system bus, where every method is put to polkit for the calling
process; `kdos-powerd` answers both ways.

| Daemon | Owns | Answers to |
|---|---|---|
| `kdos-powerd` | The firewall's named services; the accent of the boot menu, the splash, the text console and the login screen; the splash's motion | On its socket, root and `wheel`, and anyone else as polkit decides without a prompt; on `io.kdos.Power1`, polkit for every caller. The shipped policy lets the person at the machine set the accent and the splash, and nobody outside `wheel` open a port |
| `kdos-settingsd` | The machine's name, clock, time zone, locale and keyboard layouts, as `org.freedesktop.hostname1`, `timedate1` and `locale1` | polkit, per method |
| `kdos-sysupdated` | The operating-system update Discover shows, as `org.freedesktop.sysupdate1`, and the A/B slots, as `io.kdos.Slots1` | polkit, per method; the confirmation of a tried slot only for the active session, never with a prompt |
| `kdos-energyd` | Sampling the CPU's RAPL (running average power limit) energy counter and attributing it to applications | root and `wheel` |
| `kdos-oomd` | Killing the largest process outside the desktop when the kernel's pressure-stall information (PSI) says the machine is stalling and at most a tenth of memory is available ([how it decides](../04-programs/daemons.md#how-it-decides)) | root, `wheel`, and the user of the active session, as elogind reports it |
| `kdos-packd` | Mounting, installing, verifying and composing application packs | root and `wheel` |

`wheel` is the administrators' group, the same one `sudo` and polkit treat as admin. Suspend,
power-off, reboot and the lid are elogind's, asked through `org.freedesktop.login1`, where polkit
lets the person at the machine use them; none of the KDOS daemons owns the power state.

No client of a root daemon names a path. Every request takes an identifier from a list the daemon
itself published, or from a list compiled into it; `kdos-packd`'s `install` takes a file name
inside a staging directory the daemon owns. There is nothing to aim, which is what stops a daemon
the desktop user can reach from becoming a way to mount a stick over `/etc`. See
[The daemons](../04-programs/daemons.md) and
[Root daemons](security-model.md#root-daemons).

Most administration goes through one command, `kdos`: `kdos update` updates the system, `kdos app`
installs and removes boxed applications, `kdos theme` sets the accent, `kdos desk` and `kdos
tiling` arrange the panels and the windows, and `kdos march` builds a port twice to measure
whether a per-machine optimisation pays. The same operations have a graphical face in Plasma's
own centres: System Settings carries KDOS's pages (Boxes, Firewall, Look, Health and the system
pages; see [KDOS's settings pages](../04-programs/kdos-settings.md)), and Discover installs ports,
packs and boxes and applies system updates (see [Discover](../04-programs/discover.md)). See
[The kdos command](../04-programs/kdos-command.md).

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
| Verified by | The recipe's `sha256` for the source, and the release key's signature for a fetched recipe tree; the signed index for a package taken from a binary host (a server of prebuilt, signed packages); the signed manifest for every file of a slot image | Nothing KDOS controls, when built here; the payload hash and then the signature at mount time, when imported |
| Updated with | `kdos update`, or Discover through `kdos-sysupdated`: the newer release written into the other A/B slot, as a signed slot image, or as prebuilt packages from the release's binary host, or built from source on the machine | A rebuild against the catalogue's Debian snapshot |
| Trusted keyring | `/etc/kdos/keys` for binary-host indexes and slot images (empty as shipped); `/etc/kdos/keys/tree` for the recipe tree (the build's release key, when the build had one) | `/etc/kdos/keys/packs` |

Building an application fetches from Debian's archive at a pinned snapshot, which apt verifies,
but nothing KDOS controls attests to the result. An exported set of applications is hashed and
signature-checked by `kdos-packd` at the point where it is mounted.

The three keyrings are separate by construction. A pack-signing key says who exported a set of
applications; placing it in the host keyring would make it a trusted publisher of host packages
and whole system images as well. The keyring loader reads `*.pub` in one directory and does not
descend into subdirectories, so `/etc/kdos/keys/packs` and `/etc/kdos/keys/tree` are separate
policies. The image ships one key in `packs/`, which vouches only that the packs on a medium came
from the same build, and the build's release key in `tree/` when the build was given one, which
`kdos update` checks a fetched recipe tree against. It ships no key in the host keyring: a machine
takes prebuilt packages or a slot image only from a publisher whose key an administrator put there.

See [Packaging](packaging.md) and [Packs and boxes](packs-and-boxes.md).

## Where state lives

| Path | Holds | Written by |
|---|---|---|
| `/var/lib/kpkg/db` | The installed-package database and manifests | `kpkg`, as root |
| `/ports` | The recipe tree the installed packages were built from: every recipe and patch, and KDOS's own programs under `src/`, with no source archives | The build; replaced whole by `kdos update fetch` |
| `/var/lib/kdos/packs` | Imported application packs; on an A/B machine a bind of `/home/.kdos/packs`, shared by both slots | `kdos-packd`, as root |
| `/var/lib/kdos/packs/staging` | The one place an unprivileged write may land, mode 01777 | Whoever imports a pack |
| `/var/lib/kdos/packs/mnt` | Mount points for packs | `kdos-packd` |
| `/var/lib/kdos/packs/pack-manifest` | Every graft made (a pack's files placed where a consumer looks for them), so removal is exact; in the store, so both slots read one record | `kdos-packd` |
| `/var/lib/kdos/fs-manifest` | Every path the build's `fs/` tree provided | The build |
| `/var/lib/kdos/apps-pending` | Applications chosen at install time and not yet built | `kinstall`; read by `kdos app install --pending` |
| `/var/lib/kdos/update.json` | The result of the last update check: what is offered, by which lane | The `update-check` timer and `kdos-sysupdated`, through `kdos update check` |
| `/var/lib/kdos/update.state` | The outcome of the last update fetch and apply | `kdos update` |
| `/var/lib/kdos/binhost`, `/var/lib/kdos/image` | The verified binary-host index and the packages downloaded for an update; the slot-image manifest on offer, and the record of the image this root was written from | `kdos update` |
| `/var/lib/kdos/march.ledger` | Measured per-machine optimisation results | `kdos march` |
| `/etc/kdos/` | Machine configuration (`menu.conf`, `packd.conf`, `zram.conf`, `update.conf`, `timers.d/`, `user-services/`, the system `accent`) and the trusted keys | The administrator, and `kdos-powerd` for the accent |
| `/etc/sddm.conf.d/` | The login screen's configuration and autologin | The installer, and System Settings' Login Screen page |
| `/etc/xdg/` | The administrator's overrides of KDE's settings for every account; the installer writes the chosen accent and desk here | The administrator and the installer |
| `/usr/share/kdos/xdg/` | KDOS's defaults for KDE's settings, read after `/etc/xdg` | The build |
| `/usr/share/kdos/` | Shipped and generated data besides: the login screen's files per accent, the `alien-apps` table, the application catalogue, the security database, the documentation | The build |
| `~/.config/` | KDE's own settings files (`kdeglobals`, `kwinrc`, `plasma-org.kde.plasma.desktop-appletsrc` and the rest), each written by the program or settings page it belongs to | The user, through Plasma |
| `~/.config/kdos/` | KDOS's per-user files: the accent in force, the box profiles in `boxes/`, the pre-started boxes in `favorites`, the per-user timers | The user, `kdos`, and the KDOS settings pages |
| `~/.local/share/kdos/` | The per-user `alien-apps` table, pack grafts, and per-box homes and upper layers | `kdos-appbox`, `kdos-packd` |
| `$XDG_RUNTIME_DIR/` | The session bus, the Wayland socket, the user services' state under `ksvc/`, each box's private runtime directory under `kdos/box/` | Everything, per login |

Two of those entries are interfaces rather than storage:

- `~/.config/kdos/accent` holds a single word, the accent name. `kdos theme` writes it first, and
  the KDOS module in `kded6` compares the global theme picked in System Settings against it, so
  the command and the settings page cannot disagree about which accent is in force; an absent
  file means the colour scheme in use names it, or the default.
- `alien-apps` maps an application name to the command line that runs it inside its box, which is
  how a shim in `~/.local/bin` knows what to run. Installing an application writes its row to the
  per-user copy at `~/.local/share/kdos/alien-apps`, which the dispatcher reads first. The system
  copy, `/usr/share/kdos/alien-apps`, and its shims in `/usr/local/bin` are written by the build
  from the packs baked into the image; the shipped image bakes none, so that table has no rows.

The full list of paths and sockets is in [Filesystem and IPC](../06-reference/filesystem-and-ipc.md).

## See also

- [Why KDOS](../01-philosophy/why-kdos.md) — the properties this structure serves
- [How KDOS differs](../01-philosophy/how-kdos-differs.md) — the same choices compared with other distributions
- [Boot and init](boot-and-init.md) — how the machine gets to the login screen
- [The session](session.md) — what happens from the login to the logout
- [Packaging](packaging.md) — how host software is built, installed and updated
- [Packs and boxes](packs-and-boxes.md) — how applications are packaged and run
- [The security model](security-model.md) — who is allowed to do what, and what is not protected
- [The design language](design-language.md) — how KDOS looks on Plasma, from the boot menu to the lock screen
- [The window model](window-model.md) — where a window lands, how it snaps and tiles
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build side of this map, from clone to ISO
- [The ports catalogue](../06-reference/ports-catalogue.md) — every core-ring port, by shelf, with its phase
- [The kdos command](../04-programs/kdos-command.md) — the administrative command and its subcommands
- [Repository layout](../06-reference/repository-layout.md) — where each ring lives in the tree
- [Filesystem and IPC](../06-reference/filesystem-and-ipc.md) — every path and socket in full
- [KWin](../04-programs/kwin.md) and [The Plasma shell](../04-programs/plasma-shell.md) — the compositor and the shell as KDOS configures them
- [The daemons](../04-programs/daemons.md) — each root daemon in detail
- [Glossary](../06-reference/glossary.md) — the terms this page uses

<!-- book-nav -->
---

*Part III — Architecture, chapter 12.* Previous: [11. Accessibility](../02-user-guide/accessibility.md) · [Contents](../README.md) · Next: [13. Boot and init](boot-and-init.md)
