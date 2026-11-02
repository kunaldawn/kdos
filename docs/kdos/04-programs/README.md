# The programs

This part of the book documents the programs KDOS writes itself: what each one is, how it is
invoked, where it is installed and which chapter describes it in depth. It is for anyone who has
met a `kdos-*` command, a KDOS page in System Settings or a KDOS applet in the panel and wants to
know what it does, and for contributors looking for the program that owns a piece of behaviour.

The desktop is KDE Plasma, and almost everything on screen is KDE's: KWin, the Plasma shell,
System Settings, Discover, Konsole, Dolphin and the rest of KDE Gear. They are upstream programs,
documented by KDE and built from ports like any other;
[The ports catalogue](../06-reference/ports-catalogue.md) lists every port by shelf, with the phase
that builds it, and [Applications](../02-user-guide/applications.md) describes the applications.
This part covers what KDOS adds to them and the programs KDOS has of its own: four chapters on the
KDE programs KDOS extends most ([KWin](kwin.md), [the Plasma shell](plasma-shell.md),
[the KDOS settings pages](kdos-settings.md) and [Discover](discover.md)), then the box tools, the
root daemons, the installer, the `kdos` command and a demo. Read the
[Architecture overview](../03-architecture/overview.md) first if you want to see how these programs
fit together before meeting them one by one.

Several KDOS binaries dispatch on the name they were invoked as, in the manner of BusyBox, so the
number of commands is larger than the number of binaries.
[Binaries that answer to several names](#binaries-that-answer-to-several-names) lists every name
each of them answers to. The complete alphabetical list is the
[command index](../06-reference/command-index.md). On an installed machine every `kdos-*` command,
and each of ours without the prefix such as `kpkg`, `ksvc` and `kinstall`, also has a manual page,
generated at build time from the section of this book that documents it: see
[Manual pages and the book](kdos-command.md#manual-pages-and-the-book).

## Where the programs come from

The programs in this part are built from the repository's own `src/` tree. Each sits exactly two
levels down, in one of five directories chosen by what the program is:

| Directory | Holds | Built by |
|---|---|---|
| `src/desktop/` | KDOS's additions to the Plasma desktop: the System Settings pages, the tray applets, the KRunner runner, the notifier, the Discover backend, the KWin effects and scripts, the kded module, the System Monitor plugin, the `library:/` worker, the file-manager action, the welcome tour, the look-and-feel packages and the graphical installer | The `50_desktop` phase, the only phase whose [port search path](../05-developer/build-system.md#where-kpkg-looks-for-a-port) includes this directory, because these ports link KWin, libplasma or libdiscover, which that phase builds |
| `src/daemons/` | The six root daemons | The `50_desktop` phase, the only other phase that searches here |
| `src/system/` | The package manager, the `kdos` command and its services, the pack tool, the box manager, the box init and the per-box Wayland socket, the installer and its backend, and the two ports that render this book and *KDOS from Scratch* onto the image | The userland phases: `kdos-pack`, `kdos-boxinit`, `kdos-doc` and `kdos-kfs` in `41_system`, `kdos-tools` and `kdos-boxsock` in `42_graphics`, `kdos-appbox` in `43_toolkits`. `kdos-kpkg` (`kpkg`) and `kdos-installer` (`kinstall`) are compiled by the `120_kpkg.sh` and `130_kinstall.sh` steps of `10_bootstrap` and appear in no package list; `kinstall-core`, the installer's backend, is compiled into `kinstall`, which the graphical installer runs |
| `src/art/` | The identity generators, the icon themes, wallpapers and sounds, the boot splash, the login screen's theme, the curated add-ons and the demo | The `41_system` and `42_graphics` phases; `kdos-sddm-theme` and `kdos-addons`, which build on Plasma, in `50_desktop` |
| `src/devtools/` | The build orchestrator `kdosbuild` and the upstream version checker `kdos-portup`, neither installed on the target | `kdosbuild` by `script/kdosbuild.sh`, on the build host, at the start of every `make build`; `kdos-portup` by `ports/update`, on the build host, on demand |

`src/libs/`, the sixth directory, holds the nine `libk*` C libraries the C programs here compile
in, described in [The C libraries](../05-developer/c-libraries.md). The phases are described in the
order they run in [How KDOS is built](../05-developer/how-kdos-is-built.md); the phase that adds
`src/desktop/` and `src/daemons/` is
[The desktop: `50_desktop`](../05-developer/how-kdos-is-built.md#the-desktop-50_desktop).

A program under `src/` is an ordinary port: a directory holding a `kpkgbuild` and a `build.sh`, with
no `source =` to fetch because the code is in the tree. The ports under `src/desktop/` are Qt 6 and
KDE Frameworks code built with CMake and extra-cmake-modules, or Plasma and KWin packages in QML and
JavaScript, except `kdos-runner` and `kdos-notice`, which are C on basu with no Qt. Everything under
`src/daemons/` and `src/system/` is C and links no toolkit. The two directories with no recipe files
are `src/system/kdos-kpkg/`, because the package manager has to exist before anything can be built
with it, so `120_kpkg.sh` compiles it straight into the sysroot, and `src/system/kinstall-core/`,
whose sources `kinstall` compiles in. How to write a new desktop component is in
[Writing desktop software](../05-developer/writing-desktop-software.md).

The shell scripts in the tables (`kdos-dm`, `kdos-session-wrapper`, `kdos-openarchive`,
`kdos-updatedb` and `xdg-terminal-exec`) are not compiled: they are installed as they stand from
`fs/usr/local/`. The build-host scripts live under `ports/`, `script/hooks/` and `testing/`.

Three terms recur below; the [glossary](../06-reference/glossary.md) has the full definitions. A
*KCM* is a page of System Settings. A *pack* is one application, runtime or dataset as a single
signed filesystem image. A *box* is the rootless podman container an application runs in; a
*boxed* application is one that runs in a box.

## The desktop

| Port | What it adds | Documented in |
|---|---|---|
| `kdos-kwin-motion` | Phosphor mode's window, login and logout motion: three KWin effects in JavaScript | [KWin](kwin.md#window-motion) |
| `kwin-effect-kdos-keys` | The key card on Meta+F1 and Meta+/: every global shortcut, searchable | [KWin](kwin.md#the-key-card) |
| `kwin-script-kdos-runraise` | One key per terminal program, which raises its Konsole window or opens one | [KWin](kwin.md#run-or-raise) |
| `kwin-script-kdos-window-memory` | A program's window opens where its last one closed | [KWin](kwin.md#window-memory) |
| `kdos-plasma-look` | The eight colour schemes and global themes, the `kdos` Plasma style, the three desks, KSplash and the distribution's `kdeglobals` | [The Plasma shell](plasma-shell.md#the-desks), [Theming](../02-user-guide/theming.md) |
| `plasmoid-kdos-privacy` | The privacy lamps in the tray: microphone, camera and screen, naming the box | [The Plasma shell](plasma-shell.md#privacy-lamps) |
| `plasmoid-kdos-checklist` | The Getting Started checklist that ticks itself | [The Plasma shell](plasma-shell.md#getting-started) |
| `kdos-runner` | The KDOS command palette and the offline library in KRunner. `/usr/libexec/kdos/kdos-runner`, started by the session bus | [The Plasma shell](plasma-shell.md#krunner-and-the-kdos-runner) |
| `kdos-notice` | Notices of restarts, vulnerabilities, trials, rollbacks and memory kills, and the session's half of a confirmed boot. `/usr/bin/kdos-notice` | [The Plasma shell](plasma-shell.md#kdos-notice) |
| `kded-kdos` | The kded6 module that keeps the accent in step with Plasma, offers the sets on a stick and starts Samba for the first shared folder | [The Plasma shell](plasma-shell.md#kded-kdos) |
| `kdos-welcome` | The KDOS pages of the Welcome Center | [The Plasma shell](plasma-shell.md#the-welcome-tour) |
| `ksystemstats-kdos` | Sensors for boxes, energy, the slots and Phosphor mode; the KDOS Meters face and System Monitor's KDOS page | [The Plasma shell](plasma-shell.md#system-monitor) |
| `kio-library` | `library:/`, the offline library as a folder, its Places entry and the menu's Library category | [The Plasma shell](plasma-shell.md#the-library-as-a-folder) |
| `kdos-transcribe` | Transcribe in the file manager's menu, offline | [The Plasma shell](plasma-shell.md#transcribe) |
| `kcm-kdos-boxes` | The Boxes page | [KDOS settings pages](kdos-settings.md#boxes) |
| `kcm-kdos-firewall` | The Firewall page, and `kdos-fwask`, which offers to let a blocked program in | [KDOS settings pages](kdos-settings.md#firewall) |
| `kcm-kdos-health` | The Health page, and the KDOS rows of About this System | [KDOS settings pages](kdos-settings.md#health) |
| `kcm-kdos-look` | The KDOS Look page: one accent everywhere, and Phosphor mode | [KDOS settings pages](kdos-settings.md#kdos-look) |
| `kcm-kdos-system` | The System Versions page: the two root slots | [KDOS settings pages](kdos-settings.md#system-versions) |
| `kdos-discover` | Discover's backend for packs, boxes and ports, and the system-update notifier | [Discover](discover.md#the-kdos-backend) |
| `kdos-installer-gui` | Install KDOS, the graphical installer over `kinstall` | [kinstall](kinstall.md) |

KDOS also carries patches against KDE's own programs where no setting reaches what it needs. Each is
a `.patch` beside its port's recipe, opening with a `Subject:` line, and its port's note describes
it. The ones a person meets are KWin's ([KWin](kwin.md#what-the-port-builds)), Discover's ratings
switch ([Discover](discover.md#nothing-downloads-on-its-own)) and Kup's restic backend
([KDOS settings pages](kdos-settings.md#backups)).

## Root daemons

Six daemons run as root. Each is started by a script in `/etc/init.d/`, runs in the foreground
under `ksvc supervise`, which restarts it if it exits, and installs into `/usr/sbin`. Each
authorises a request by who is asking, as the kernel or the bus reports it, never by anything the
message claims. All six are documented in [The daemons](daemons.md).

| Command | Started by | Owns |
|---|---|---|
| `kdos-settingsd` | `43_settingsd.sh` | `org.freedesktop.hostname1`, `timedate1` and `locale1` on the system bus: the machine's name, clock, time zone, language and keyboards, for KDE's pages. See also [KDOS settings pages](kdos-settings.md#kdos-settingsd) |
| `kdos-sysupdated` | `46_sysupdated.sh` | `org.freedesktop.sysupdate1` and `io.kdos.Slots1` on the system bus: system updates for Discover, the slots for System Versions, and confirming a booted slot. See also [Discover](discover.md#kdos-sysupdated) |
| `kdos-powerd` | `55_powerd.sh` | The firewall's named services and the look of the machine before a login, on `/run/kdos-powerd.sock` and as `io.kdos.Power1` on the system bus |
| `kdos-energyd` | `56_energyd.sh` | The CPU energy counter (RAPL), sampled and attributed per application and box, on `/run/kdos-energyd.sock` |
| `kdos-oomd` | `57_oomd.sh` | Killing a process when memory pressure, as the kernel's pressure-stall information (PSI) reports it, would otherwise leave the desktop unresponsive; `/run/kdos-oomd.sock` |
| `kdos-packd` | `59_packd.sh` | Mounting, installing and composing application packs, and verifying their signatures; `/run/kdos-packd.sock` |

Two of the daemons have a command-line client: the daemon's own binary under a second name,
dispatched on its basename, so the socket path and the command words exist in one place.

| Command | Installed at | Talks to |
|---|---|---|
| `kdos-power` | `/usr/bin`, a link to `../sbin/kdos-powerd` | `kdos-powerd`; its `suspend`, `poweroff` and `reboot` run `loginctl` |
| `kdos-energy` | `/usr/bin`, a link to `../sbin/kdos-energyd` | `kdos-energyd` |

## Programs that run as root on request

KDOS installs no setuid program of its own. Two of its programs run as root when somebody asks, each
through `pkexec`, whose polkit action names the program and never its arguments, so each takes a
fixed, checked set of words. The image's setuid inventory, all of it upstream's, is in
[The security model](../03-architecture/security-model.md#setuid-binaries).

| Program | Run by | Does |
|---|---|---|
| `/usr/libexec/kdos-discover/kdos-ports-helper` | Discover, under `io.kdos.ports.install`, to install host software | `kdos update install <port>` from the signed binary host. See [Discover](discover.md#host-software) |
| `/usr/bin/kinstall` | The graphical installer | The install, as `kinstall --unattended --events json`. See [kinstall](kinstall.md) |

## Packaging and boxes

| Command | What it is | Documented in |
|---|---|---|
| `kpkg` | The package manager, under five names | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kdos-pack` | Builds, signs, verifies and indexes application packs, and makes and applies deltas between them | [Packs and boxes](../03-architecture/packs-and-boxes.md#building-a-pack) |
| `kdos-appbox` | Launches boxed applications, opens files and links, and generates launchers | [kdos-appbox](kdos-appbox.md) |
| `kdos-box` | Manages boxes. The same binary as `kdos-appbox` | [kdos-appbox](kdos-appbox.md#kdos-box) |
| `kdos-boxsock` | One process per box launcher, running as you, installed in `/usr/bin`. It binds a Wayland socket in the box's private runtime directory and registers it with KWin, tagged with the box, so KWin knows which box every window on it came from. `kdos-appbox` starts it | [The daemons](daemons.md#kdos-boxsock), [KWin](kwin.md#boxes-at-the-compositor) |
| `kdos-boxinit` | Process 1 inside a pack box, in place of `distrobox-init`. Statically linked, installed in `/usr/libexec/kdos` | [Packs and boxes](../03-architecture/packs-and-boxes.md#the-box) |

## System tools

| Command | Does | Documented in |
|---|---|---|
| `kdos` | The system's command-line entry point, and every subcommand | [The kdos command](kdos-command.md) |
| `kinstall` | The installer: the text front end, and the backend both front ends run | [kinstall](kinstall.md) |
| `kdos-installer-gui` | Install KDOS, the graphical installer on the live medium | [kinstall](kinstall.md) |
| `ksvc` | The service supervisor, for the system's services and each user's | [Boot and init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts) |
| `service` | The same binary under the conventional name | [Administration](../02-user-guide/administration.md#services) |
| `kdos-dm` | What `init` runs on tty1: starts SDDM, and falls back to a password prompt, never an automatic login, when SDDM cannot start. A shell script in `/usr/local/sbin` | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-session-wrapper` | SDDM's session command: reads the login shell's profile, checks the session bus, warms the boxes and starts Plasma. A shell script in `/usr/local/bin` | [The session](../03-architecture/session.md) |
| `kdos-getty` | Loads the virtual-terminal font and the accent's palette, then runs a getty | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-bootctl` | Chooses and confirms which of the two root filesystem slots, A or B, boots, and writes the console palette for the accent | [Boot and init](../03-architecture/boot-and-init.md#kdos-bootctl) |
| `kdos-splash` | The boot splash, drawn straight to the framebuffer | [Boot and init](../03-architecture/boot-and-init.md#the-splash) |
| `kdos-banner` | The login banner | [Boot and init](../03-architecture/boot-and-init.md#the-login-banner) |
| `kdos-doc` | Opens this book at a page, in the Help Center | [The kdos command](kdos-command.md#manual-pages-and-the-book) |
| `kdos-shot` | Screenshots, through Spectacle | [The kdos command](kdos-command.md#kdos-shot) |
| `kdos-theme` | The build-time generators of KDOS's identity: the KDE colour schemes, the Konsole schemes, the qt5ct palettes, the wallpapers and the login screen's palette, and the contrast check over them. The picker is `kdos theme` | [Theming](../02-user-guide/theming.md#how-the-theme-is-generated) |
| `kdos-mpctl` | Music player control, over mpd's protocol or MPRIS: what a media key runs | [The kdos command](kdos-command.md#kdos-mpctl) |
| `kdos-share` | Sends a file to another machine over `croc` | [The kdos command](kdos-command.md) |
| `kdos-sfx` | Plays the machine's sounds, and renders them into the `kdos` sound theme at build time | [The kdos command](kdos-command.md#kdos-sfx) |
| `kdos-fetch-app`, `kdos-fetch-static` | Install an [alien application](../06-reference/glossary.md#alien-app) (one not compiled by this repository) into a distrobox container from its distribution's packages, and export it to your desktop; fetch one upstream binary, checked against a SHA-256 you give it, into `/usr/local/bin` | [The kdos command](kdos-command.md#kdos-fetch-app-and-kdos-fetch-static) |
| `kdos-bb` | A hard fork of bb 1.3rc1, the AAlib ASCII-art demo | [kdos-bb](kdos-bb.md) |
| `kdos-openarchive` | Extracts an archive with `ouch` into the directory the archive is in: the default handler for 7z and rar. A shell script | [Applications](../02-user-guide/applications.md) |
| `kdos-updatedb` | Rebuilds your own `plocate` index, scoped to your home directory. A shell script | [Administration](../02-user-guide/administration.md#finding-a-file-by-name) |
| `xdg-terminal-exec` | The first terminal name GLib looks for when it wraps a `Terminal=true` entry. It drops a leading `-e` or `--` and runs `konsole -e` with the rest. A shell script | [kdos-appbox](kdos-appbox.md#terminal-entries) |

## Build and development tools

These run on a build host and never ship on the target.

| Command | Does | Documented in |
|---|---|---|
| `kdosbuild` | The build orchestrator. `script/kdosbuild.sh` compiles it into `build/.kdosbuild` when its sources, flags or compiler changed, and runs it | [The build system](../05-developer/build-system.md) |
| `kdos-portup` | Checks every port for a newer upstream release. Compiled on demand by `ports/update` | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/update` | The front end to the version checker | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/fetch` | Fetches every source a recipe names, from the cache, the source archive or upstream, and generates missing vendor bundles. `make fetch` runs it | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `ports/publish` | Uploads sources the archive lacks into the numbered volumes `ports/shelves` maps each shelf to, plans volumes for new shelves, moves files when a port changes shelf, lists and prunes files no recipe names, freezes a release's source list, and puts a system release on GitHub | [Writing ports](../05-developer/writing-ports.md#publishing-sources) |
| `script/hooks/pre-push` | Refuses a push that breaks the ports layout (every recipe at `ports/core/<shelf>/<name>/` on a listed shelf, every port name used once across `ports/core` and `src/`), that adds or changes a patch with no `Subject:` line, whose working copy fails `testing/docscheck.sh` or `testing/kfscheck.py`, or whose recipes name a source the archive does not hold. Enabled with `git config core.hooksPath script/hooks` | [Developing](../05-developer/developing.md#the-pre-push-check) |
| `testing/*` | The test harnesses: `selftest.sh` for the C, `plasmatest.sh` for the Qt and QML code in the development container, and the rig, which boots the ISO in QEMU and photographs it over VNC | [Testing](../05-developer/testing.md) |

## Binaries that answer to several names

Three binaries provide most of KDOS's own commands. Each dispatches on its own basename: it is
installed once, with a symbolic link for each further name. In `ksvc` and `kpkg` a table in the
source maps each name to an entry point, and the table and the installed links must agree: a name in
the table with no entry point fails the link, and a name with no link installed is a program nothing
can reach. Run under a name missing from its table, each takes its first argument as the name
instead, so the freshly compiled `kdos-tools` binary runs as `kdos-tools service list` before any
link exists; when that does not match either, `ksvc` prints the names it knows and `kpkg` reports
that it has no tool of that name. `kdos-appbox` has no such table: it treats any name it does not
recognise as the name of a boxed application to launch (an application shim, described below). Two
root daemons also answer to a second name; see [Root daemons](#root-daemons).

### `ksvc` — 12 names

The supervisor, and the system tools built from the same binary. Its source is
`src/system/kdos-tools/`; the binary is installed as `/usr/sbin/ksvc` and every other name is a
link to it.

| Name | Installed at | Is |
|---|---|---|
| `ksvc` | `/usr/sbin` | The service supervisor |
| `service` | `/usr/sbin` | The same, under the conventional name |
| `kdos` | `/usr/local/bin` | The `kdos` command and its subcommands |
| `kdos-getty` | `/usr/local/sbin` | The font-and-palette wrapper around a getty |
| `kdos-bootctl` | `/usr/bin` | A/B slot selection. The initramfs carries its own copy |
| `kdos-banner` | `/usr/local/bin` | The login banner |
| `kdos-shot` | `/usr/local/bin` | Screenshots, a front end to Spectacle |
| `kdos-sfx` | `/usr/local/bin` | Sound effects, and rendering the sound theme |
| `kdos-mpctl` | `/usr/local/bin` | Music player control, and the bridge that puts mpd on MPRIS |
| `kdos-share` | `/usr/local/bin` | A file to another machine, over `croc` |
| `kdos-fetch-app` | `/usr/local/bin` | Installs an alien application into a distrobox container and exports it |
| `kdos-fetch-static` | `/usr/local/bin` | Installs one upstream binary, checked against a given SHA-256 |

### `kpkg` — 5 names

All five are in `/usr/bin`. The source is `src/system/kdos-kpkg/`, compiled by
`script/phases/10_bootstrap/120_kpkg.sh`.

| Name | Is |
|---|---|
| `kpkg` | The front end |
| `kpkgadd` | Install a prebuilt package file |
| `kpkgbuild` | Build a port without installing it |
| `kpkgdel` | Remove a package |
| `kpkgdepends` | Print the resolved install order |

### `kdos-appbox` — 3 names, plus a shim per application

All three are in `/usr/local/bin`.

| Name | Is |
|---|---|
| `kdos-appbox` | Launching boxed applications, and generating their launchers |
| `kdos-box` | The box manager |
| `xdg-open` | Opening a file or a link with whatever this machine opens it with |

`/usr/local/bin` precedes `/usr/bin` on the `PATH` set by `/etc/profile`, so this binary answers
`xdg-open` before xdg-utils' script does. That script remains installed, and `kdos-appbox` falls
back to it by absolute path when nothing it knows claims the type.

Any other name the binary is invoked under is treated as an application shim: it launches the
application of that name in its box. The image build writes one such link in `/usr/local/bin` for
each application in a pack the medium carries, and the default medium carries none. Installing an
application writes its shim in `~/.local/bin`, so after `kdos app install app.hugin`, `hugin` on
your `PATH` is `kdos-appbox` dispatching on that name. `kdos-appbox genlaunchers` rewrites the
system set, and `kdos-box export` adds a `<app>@<box>` shim in `~/.local/bin` for an application in a
box other than the default one. See [kdos-appbox](kdos-appbox.md#launcher-generation).

## See also

- [Command index](../06-reference/command-index.md): every command, alphabetically
- [Architecture overview](../03-architecture/overview.md): how these programs fit together
- [How KDOS differs](../01-philosophy/how-kdos-differs.md): how KDOS ships Plasma compared with
  other distributions
- [How KDOS is built](../05-developer/how-kdos-is-built.md): the phases that compile these programs
- [The ports catalogue](../06-reference/ports-catalogue.md): the upstream software beside them
- [Repository layout](../06-reference/repository-layout.md): where each one's source lives
- [The C libraries](../05-developer/c-libraries.md): the libraries the C programs are built on
- [Writing desktop software](../05-developer/writing-desktop-software.md): writing a new KDOS
  component for the desktop
- [Configuration](../06-reference/configuration.md): every configuration key, with defaults

<!-- book-nav -->
---

*Part IV — Programs, chapter 20.* Previous: [19. The window model](../03-architecture/window-model.md) · [Contents](../README.md) · Next: [21. KWin](kwin.md)
