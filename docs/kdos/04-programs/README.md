# The programs

This part of the book documents the programs KDOS writes itself: what each one is, how it is
invoked, where it is installed and which chapter describes it in depth. It is for anyone who has
met a `kdos-*` command and wants to know what it does, and for contributors looking for the program
that owns a piece of behaviour. Software in `ports/core/` comes from upstream projects and is
documented by them; nothing here repeats a manual page that already exists elsewhere, and
[The ports catalogue](../06-reference/ports-catalogue.md) lists every such port by phase and group.
That includes the graphical applications ported natively with GTK, Qt, KDE Frameworks, wxWidgets,
FLTK or Tk: they are upstream programs, and [Applications](../02-user-guide/applications.md)
describes how to use them. The programs in this part draw with KDOS's own cell libraries and link
no toolkit. Read the
[Architecture overview](../03-architecture/overview.md) first if you want to see how these programs
fit together before meeting them one by one.

One property of the system shapes this whole chapter. Several KDOS binaries dispatch on the name
they were invoked as, in the manner of BusyBox, so the number of commands on a KDOS machine is much
larger than the number of binaries: 53 commands come out of `kdos-shell` alone. If you have found a
command on your `PATH` and want to know where it is documented, the tables below resolve it, and
[Binaries that answer to several names](#binaries-that-answer-to-several-names) lists every name
each of the four binaries behind most commands answers to; the three daemon clients are under
[Root daemons](#root-daemons). The complete alphabetical list is the
[command index](../06-reference/command-index.md).

## Where the programs come from

The compiled programs in this chapter are built from the repository's own `src/` tree, in four
directories:

| Directory | Holds | Built by |
|---|---|---|
| `src/desktop/` | The compositor, the panel, the terminal, the resource monitor, the lock screen, the root daemons, the portal backend and the screen recorder | The `05_desktop` phase, the only phase whose port search path includes this directory |
| `src/packages/` | The system tools, the package manager, the installer, the box manager and its helpers, the splash, the theme generator and the demo | The `04_phase4` phase, with two exceptions: `kdos-kpkg` (`kpkg`) and `kdos-installer` (`kinstall`) are compiled by steps 12 and 13 of `01_phase1` and appear in no package list. `05_desktop` names `kdos-pack` too, but `04_phase4` has already built it as a dependency of `kdos-tools` |
| `src/build/kdosbuild/` | The build orchestrator | `script/kdosbuild.sh`, on the build host, at the start of every `make build` |
| `src/tools/kdos-portup/` | The upstream version checker | `ports/update`, on the build host, on demand |

The phases named here are described in the order they run, from the cross toolchain to the ISO,
in [How KDOS is built](../05-developer/how-kdos-is-built.md); the phase that adds `src/desktop/`
is [The desktop: `05_desktop`](../05-developer/how-kdos-is-built.md#the-desktop-05_desktop).

The shell scripts in the tables (`kdos-desktop`, `kdos-desktop-start`, `kdos-session-save`,
`kdos-login`, `kdos-openarchive`, `kdos-updatedb` and `xdg-terminal-exec`) are not compiled: they
are installed as they stand from `fs/usr/local/`. The build-host scripts live under `ports/`,
`script/hooks/` and `testing/`.

A program under `src/desktop/` or `src/packages/` is an ordinary port: a directory holding a
`kpkgbuild` and a `build.sh`, with no `source =` to fetch because the code is in the tree. Most of
them link the static `libk*` libraries under `src/libs/`, described in
[The C libraries](../05-developer/c-libraries.md). Three further ports under `src/packages/`,
`kdos-icons`, `kdos-cursors` and `kdos-gtk-theme`, carry no program: each runs `kdos-theme` at
build time to generate the icon, cursor or GTK theme it installs. They are described in
[Theming](../02-user-guide/theming.md#how-the-theme-is-generated).

Three terms recur in the tables below; the [glossary](../06-reference/glossary.md) has the full
definitions. A *surface* is one window or popup of the desktop drawn by KDOS, such as the panel or
the Start menu. A *pack* is one application, runtime or dataset as a single signed filesystem
image. A *box* is the rootless podman container an application runs in; a *boxed* application is
one that runs in a box.

## The desktop

| Command | What it is | Documented in |
|---|---|---|
| `kdos-comp` | The compositor: a frozen hard fork of labwc 0.20.0; its KDOS additions live mainly in eighteen `src/kdos-*.c` files | [kdos-comp](kdos-comp.md) |
| `kdos-shell` | The panel and 53 further surfaces: 55 names in all, since `kdos-launcher` and `kdos-palette` open the same surface. See [below](#kdos-shell--55-names) | [kdos-shell](kdos-shell.md) |
| `kdos-res` | The resource monitor. The same binary runs as a text program on a virtual terminal and as a window on the desktop | [kdos-res](kdos-res.md) |
| `kdos-term` | A terminal with three inline-picture protocols: sixel, iTerm2's and kitty's. It ships beside `foot`, which remains the default terminal | [kdos-term](kdos-term.md) |
| `kdos-lock` | The lock screen, an `ext-session-lock-v1` client: the compositor keeps the screen locked if the client dies | [The daemons](daemons.md#kdos-lock) |
| `kdos-record` | The screen recorder: the ScreenCast portal into a `gst-launch-1.0` pipeline. Running it a second time stops the recording | [The session](../03-architecture/session.md#recording) |
| `kdos-desktop` | Starts a session from a virtual terminal after login. A shell script in `/usr/local/bin` | [The session](../03-architecture/session.md#what-kdos-desktop-does) |
| `kdos-desktop-start` | Brings up the audio stack and the portals, then runs the compositor. After a crash it prints the log's tail and offers to restart the session; the third crash within 60 seconds returns to the text console without offering. A clean exit is a logout. A shell script in `/usr/local/bin` | [The session](../03-architecture/session.md#what-kdos-desktop-start-does) |
| `kdos-session-save` | Records the running applications when the session is ended, so the next login can relaunch the boxed ones. A shell script | [The session](../03-architecture/session.md#what-runs-once-after-the-display-exists) |
| `xdg-desktop-portal-kdos` | The FileChooser, Settings, AppChooser and Access portal backends, installed at `/usr/lib/xdg-desktop-portal-kdos` and started on demand over D-Bus in your session. It answers a file-chooser request by running `kdos-pick`, and an access question by running `kdos-prompt` | [The daemons](daemons.md#xdg-desktop-portal-kdos) |

## Root daemons

Five daemons run as root. Each is started by a script in `/etc/init.d/` (`55_powerd.sh` to
`59_packd.sh`), runs in the foreground under `ksvc supervise`, which restarts it if it exits, and
installs into `/usr/sbin`. Each owns one Unix socket in `/run`, named after the daemon (for example
`/run/kdos-powerd.sock`), and authorises a request by the credentials the kernel attaches to the
connection (`SO_PEERCRED`), never by anything the message claims. All five are documented in
[The daemons](daemons.md).

| Command | Owns |
|---|---|
| `kdos-powerd` | Suspend, power-off and reboot, for root and members of `seat` or `wheel`; the time zone, the [accent](../06-reference/glossary.md) (the colour palette), the autologin account and the firewall service list, for root and `wheel` only |
| `kdos-energyd` | The CPU energy counter (RAPL), sampled and attributed per application |
| `kdos-oomd` | Killing a process when memory pressure, as the kernel's pressure-stall information (PSI) reports it, would otherwise leave the desktop unresponsive |
| `kdos-mountd` | Mounting, unmounting, ejecting and formatting removable media; unlocking LUKS volumes; SMB shares; SMART queries |
| `kdos-packd` | Mounting, installing and composing application packs, and verifying their signatures |

Three of the daemons have a command-line client. Each client is the daemon's own binary under a
second name, dispatched on its basename, so the socket path and the command words exist in one
place. Run by an ordinary user, the client writes a request to the socket and prints the reply.

| Command | Installed at | Talks to |
|---|---|---|
| `kdos-power` | `/usr/bin`, a link to `../sbin/kdos-powerd` | `kdos-powerd` |
| `kdos-energy` | `/usr/bin`, a link to `../sbin/kdos-energyd` | `kdos-energyd` |
| `kdos-mount` | `/usr/bin`, a link to `../sbin/kdos-mountd` | `kdos-mountd` |

The same page also describes two programs that run as you rather than as root:
`xdg-desktop-portal-kdos`, listed under [The desktop](#the-desktop), and `kdos-boxsock`, listed
under [Packaging and boxes](#packaging-and-boxes).

## Privileged helpers

Two programs of KDOS's own are setuid root. Both are short (189 and 285 lines of C, counted with
`wc -l` on `checkpass.c` and `resctl.c`), and neither takes a path from its caller. The image's
whole setuid inventory is in
[The security model](../03-architecture/security-model.md#setuid-binaries).

| Command | Does | Takes |
|---|---|---|
| `kdos-checkpass` | Checks the caller's own password, by real user ID, against `/etc/shadow`, for `kdos-lock`. Exits 0 when it matches, 1 when it does not and 2 when it cannot tell | No arguments; the password on standard input |
| `kdos-resctl` | For `kdos-res`: sends `TERM`, `KILL`, `STOP` or `CONT` to a process, renices it, or reads the SMBIOS table from a compiled-in path. The caller must be in `wheel` | Three verbs, `dmi`, `signal` and `renice`; no paths, no options |

Both are installed in `/usr/bin` with mode 4755.

## Packaging and boxes

| Command | What it is | Documented in |
|---|---|---|
| `kpkg` | The package manager, under five names | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kdos-pack` | Builds, signs, verifies and indexes application packs, and makes and applies deltas between them | [Packs and boxes](../03-architecture/packs-and-boxes.md#building-a-pack) |
| `kdos-appbox` | Launches boxed applications, opens files and links, and generates launchers | [kdos-appbox](kdos-appbox.md) |
| `kdos-box` | Manages boxes. The same binary as `kdos-appbox` | [kdos-appbox](kdos-appbox.md#kdos-box) |
| `kdos-boxsock` | One process per box, running as you, installed in `/usr/bin`. It binds a Wayland socket in your `$XDG_RUNTIME_DIR` that the compositor tags with the box's name, so the compositor knows which box every window on it came from. `kdos-appbox` starts it | [The daemons](daemons.md#kdos-boxsock) |
| `kdos-boxinit` | Process 1 inside a pack box, in place of `distrobox-init`. Statically linked, installed in `/usr/libexec/kdos` | [Packs and boxes](../03-architecture/packs-and-boxes.md#the-box) |

## System tools

| Command | Does | Documented in |
|---|---|---|
| `kdos` | The system's command-line entry point: thirty subcommands, plus `help` | [The kdos command](kdos-command.md) |
| `kinstall` | The installer | [kinstall](kinstall.md) |
| `ksvc` | The service supervisor | [Boot and init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts) |
| `service` | The same binary under the conventional name | [Administration](../02-user-guide/administration.md#services) |
| `kdos-getty` | Loads the virtual-terminal font and the accent's palette, sets up the delegated cgroup and the real-time limits, then runs a getty | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-login` | Reads `/etc/kdos/login.conf` and hands tty1 to `agetty`, logging in automatically when the file names an account. A shell script in `/usr/local/sbin` | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-bootctl` | Chooses and confirms which of the two root filesystem slots, A or B, boots, and writes the console palette for the accent | [Boot and init](../03-architecture/boot-and-init.md#kdos-bootctl) |
| `kdos-splash` | The boot splash, drawn straight to the framebuffer | [Boot and init](../03-architecture/boot-and-init.md#the-splash) |
| `kdos-banner` | The login banner | [Boot and init](../03-architecture/boot-and-init.md#the-login-banner) |
| `kdos-shot` | Screenshots, to the clipboard and to `~/Pictures/Screenshots` | [The kdos command](kdos-command.md#kdos-shot) |
| `kdos-theme` | Generates the GTK, icon and cursor themes for an accent. The generator, not the picker: the picker is `kdos-style` | [Theming](../02-user-guide/theming.md#how-the-theme-is-generated) |
| `kdos-mpctl` | Music player control, over mpd's protocol or MPRIS: what a media key runs | [The kdos command](kdos-command.md#kdos-mpctl) |
| `kdos-share` | Sends a file to another machine over `croc` | [The kdos command](kdos-command.md#kdos-share) |
| `kdos-sfx` | Plays the machine's four sounds: `login`, `notify`, `error`, `degauss` | [The kdos command](kdos-command.md#kdos-sfx) |
| `kdos-fetch-app`, `kdos-fetch-static` | Install an [alien application](../06-reference/glossary.md) (one not compiled by this repository) from a network; fetch one verified static binary | [The kdos command](kdos-command.md#kdos-fetch-app-and-kdos-fetch-static) |
| `kdos-bb` | A hard fork of bb 1.3rc1, the AAlib ASCII-art demo | [kdos-bb](kdos-bb.md) |
| `kdos-openarchive` | Extracts an archive that `mc` cannot browse as a directory. A shell script | [kdos-shell](kdos-shell.md#kdos-pick) |
| `kdos-updatedb` | Rebuilds your own `plocate` index, scoped to your home directory. A shell script | [Administration](../02-user-guide/administration.md#finding-a-file-by-name) |
| `xdg-terminal-exec` | The first terminal name GLib looks for when it wraps a `Terminal=true` entry, which is how the portal opens a link for a boxed application. It drops a leading `-e` or `--` and runs `foot -e` with the rest, the same terminal `kdos-appbox` chooses. A shell script | [kdos-appbox](kdos-appbox.md#terminal-entries) |

## Build and development tools

These run on a build host and never ship on the target.

| Command | Does | Documented in |
|---|---|---|
| `kdosbuild` | The build orchestrator. `script/kdosbuild.sh` compiles it into `build/.kdosbuild` and runs it | [The build system](../05-developer/build-system.md) |
| `kdos-portup` | Checks every port for a newer upstream release. Compiled on demand by `ports/update` | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/update` | The front end to the version checker | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/fetch` | Fetches every source a recipe names, from the cache, the source archive or upstream, and generates missing vendor bundles. `make fetch` runs it | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `ports/publish` | Uploads sources the archive lacks, and freezes a release's source list | [Writing ports](../05-developer/writing-ports.md#publishing-sources) |
| `script/hooks/pre-push` | Refuses a push whose recipes name a source the archive does not hold. Enabled with `git config core.hooksPath script/hooks` | [Developing](../05-developer/developing.md#the-pre-push-check) |
| `testing/*` | The test harnesses, including the rig, which boots the ISO in QEMU and photographs it over VNC | [Testing](../05-developer/testing.md) |

## Binaries that answer to several names

Four binaries provide most of the commands on the system. Each dispatches on its own basename: it
is installed once, with a symbolic link for each further name. In `kdos-shell`, `ksvc` and `kpkg`
a table in `main.c` maps each name to an entry point, and the table and the installed links must
agree. A name in the table with no matching entry point fails the link; a name with no link
installed is a program nothing can reach. Run under a name missing from its table, `kdos-shell` or
`ksvc` prints the names it does know, and `kpkg` reports that it has no tool of that name.
`kdos-appbox` has no such table: it treats any name it does not recognise as the name of a boxed
application to launch (an application shim, described below). Three root daemons also answer to a
second name; see [Root daemons](#root-daemons).

### `kdos-shell` — 55 names

The panel and every surface reachable from it or from a key chord. One name, `kdos-launcher`,
opens the same surface as `kdos-palette`, so the 55 names resolve to 54 distinct programs. All are
installed in `/usr/bin`, and each is described in [kdos-shell](kdos-shell.md).

| | | | |
|---|---|---|---|
| `kdos-shell` | `kdos-start` | `kdos-launcher` | `kdos-palette` |
| `kdos-menu` | `kdos-desk` | `kdos-pick` | `kdos-ascii` |
| `kdos-run` | `kdos-prompt` | `kdos-notifyd` | `kdos-notify` |
| `kdos-mediad` | `kdos-netagent` | `kdos-osd` | `kdos-cal` |
| `kdos-display` | `kdos-keys` | `kdos-teams` | `kdos-saver` |
| `kdos-about` | `kdos-style` | `kdos-calc` | `kdos-chars` |
| `kdos-connect` | `kdos-traymenu` | `kdos-contacts` | `kdos-disks` |
| `kdos-print` | `kdos-time` | `kdos-users` | `kdos-update` |
| `kdos-store` | `kdos-firewall` | `kdos-backup` | `kdos-note` |
| `kdos-slit` | `kdos-doc` | `kdos-settings` | `kdos-openwith` |
| `kdos-audio` | `kdos-net` | `kdos-bt` | `kdos-devices` |
| `kdos-clip` | `kdos-status` | `kdos-tip` | `kdos-ime` |
| `kdos-trash` | `kdos-peek` | `kdos-find` | `kdos-pix` |
| `kdos-rec` | `kdos-burn` | `kdos-verify` | |

### `ksvc` — 12 names

The supervisor, and the system tools built from the same binary. Its source is
`src/packages/kdos-tools/`; the binary is installed as `/usr/sbin/ksvc` and every other name is a
link to it.

| Name | Installed at | Is |
|---|---|---|
| `ksvc` | `/usr/sbin` | The service supervisor |
| `service` | `/usr/sbin` | The same, under the conventional name |
| `kdos` | `/usr/local/bin` | The `kdos` command and its subcommands |
| `kdos-getty` | `/usr/local/sbin` | The font-and-palette wrapper around a getty |
| `kdos-bootctl` | `/usr/bin` | A/B slot selection. The initramfs carries its own copy |
| `kdos-banner` | `/usr/local/bin` | The login banner |
| `kdos-shot` | `/usr/local/bin` | Screenshots |
| `kdos-sfx` | `/usr/local/bin` | Sound effects |
| `kdos-mpctl` | `/usr/local/bin` | Music player control |
| `kdos-share` | `/usr/local/bin` | A file to another machine, over `croc`. An open-with list offers its Share row only when this name is on `PATH` |
| `kdos-fetch-app` | `/usr/local/bin` | Installs an alien application from a network |
| `kdos-fetch-static` | `/usr/local/bin` | Fetches one verified static binary |

### `kpkg` — 5 names

All five are in `/usr/bin`. The source is `src/packages/kdos-kpkg/`, compiled by
`script/01_phase1/12_kpkg.sh`.

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
box other than the default one. See
[kdos-appbox](kdos-appbox.md#launcher-generation).

## See also

- [Command index](../06-reference/command-index.md) — every command, alphabetically
- [Architecture overview](../03-architecture/overview.md) — how these programs fit together
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#the-desktop) — why the desktop is KDOS's own
  programs rather than an existing desktop environment
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the phases that compile these programs
- [The ports catalogue](../06-reference/ports-catalogue.md) — the upstream software beside them
- [Repository layout](../06-reference/repository-layout.md) — where each one's source lives
- [The C libraries](../05-developer/c-libraries.md) — the libraries these programs are built on
- [Configuration](../06-reference/configuration.md) — every configuration key, with defaults

<!-- book-nav -->
---

*Part IV — Programs, chapter 20.* Previous: [19. The window model](../03-architecture/window-model.md) · [Contents](../README.md) · Next: [21. kdos-comp](kdos-comp.md)
