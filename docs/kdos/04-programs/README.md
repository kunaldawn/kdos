# Program reference

This section documents every program KDOS itself ships: what it is, how it is invoked, and the
internals worth knowing. It is for anyone who has met a `kdos-*` command and wants to know what it
does and where it is described, and for contributors looking for the program that owns a piece of
behaviour. Software in `ports/core/` comes from upstream projects and is documented by those
projects; nothing here duplicates a manual page that already exists elsewhere.

One property of the system shapes the whole section. Several KDOS binaries dispatch on the name
they were invoked as, so the number of commands on a KDOS machine is far larger than the number of
binaries — 53 commands come out of `kdos-shell` alone. If you have found a command on your `PATH`
and want to know where it is documented, the tables below resolve it. The complete alphabetical
list is the [command index](../06-reference/command-index.md).

## The desktop

| Command | What it is | Documented in |
|---|---|---|
| `kdos-comp` | The compositor: a frozen fork of labwc 0.20.0 with sixteen KDOS grafts | [kdos-comp](kdos-comp.md) |
| `kdos-shell` | The panel, and 51 further surfaces under 52 further names | [kdos-shell](kdos-shell.md) |
| `kdos-res` | The resource monitor | [kdos-res](kdos-res.md) |
| `kdos-term` | The terminal, with three inline-picture protocols | [kdos-term](kdos-term.md) |
| `kdos-lock` | The lock screen | [The daemons](daemons.md) |
| `kdos-record` | The screen recorder: the ScreenCast portal into a GStreamer pipeline. Run it again to stop | [The session](../03-architecture/session.md#recording) |
| `kdos-desktop` | Starts a session from a virtual terminal. A shell script | [The session](../03-architecture/session.md) |
| `kdos-desktop-start` | Brings up audio and the portals, then the compositor. A shell script | [The session](../03-architecture/session.md) |
| `kdos-bb` | The forked ASCII-art demo | [kdos-bb](kdos-bb.md) |

## Root daemons

Five daemons run as root. Each runs in the foreground under `ksvc`, owns one socket in `/run`,
authorises requests by the calling process's credentials, and installs into `/usr/sbin`. All are
documented in [The daemons](daemons.md).

| Command | Owns |
|---|---|
| `kdos-powerd` | Suspend, poweroff, reboot, the time zone, the accent, the autologin account and the firewall service list |
| `kdos-energyd` | The CPU energy counter, attributed per application |
| `kdos-oomd` | Killing something before memory pressure wedges the desktop |
| `kdos-mountd` | Mounting removable media, LUKS volumes, SMB shares and SMART queries |
| `kdos-packd` | Mounting, installing and composing application packs, and verifying their signatures |

Two further programs on the same page are not root daemons and own no socket in `/run`:

| Command | Installed at | Runs as |
|---|---|---|
| `kdos-boxsock` | `/usr/bin` | One process per box, giving that box its own tagged Wayland socket so the compositor knows which box a window came from |
| `xdg-desktop-portal-kdos` | `/usr/lib/xdg-desktop-portal-kdos` | Activated over D-Bus in your session: the file-chooser, settings, application-chooser and access portal backends |

Their clients are ordinary unprivileged commands:

| Command | Talks to |
|---|---|
| `kdos-power` | `kdos-powerd` |
| `kdos-energy` | `kdos-energyd` |
| `kdos-mount` | `kdos-mountd` |

## Privileged helpers

Two programs of KDOS's own are setuid root. Both are deliberately tiny, and both refuse to take a
path from their caller. The image's whole setuid inventory is in
[The security model](../03-architecture/security-model.md#setuid-binaries).

| Command | Does | Takes |
|---|---|---|
| `kdos-checkpass` | Checks the caller's own password against `/etc/shadow` | No arguments; the password on standard input |
| `kdos-resctl` | Signals or renices a process, and reads the hardware table | Three verbs, no paths, no options |

## Packaging and boxes

| Command | What it is | Documented in |
|---|---|---|
| `kpkg` | The package manager, under five names | [Packaging](../03-architecture/packaging.md) |
| `kdos-pack` | Builds, signs, indexes and diffs application packs | [Packs and boxes](../03-architecture/packs-and-boxes.md) |
| `kdos-appbox` | Launches boxed applications and generates their launchers | [kdos-appbox](kdos-appbox.md) |
| `kdos-box` | Manages boxes. The same binary as `kdos-appbox` | [kdos-appbox](kdos-appbox.md) |
| `kdos-boxinit` | Process 1 inside a box. Statically linked, in `/usr/libexec/kdos` | [Packs and boxes](../03-architecture/packs-and-boxes.md) |

## System tools

| Command | Does | Documented in |
|---|---|---|
| `kdos` | The front door: thirty subcommands, plus `help` | [The kdos command](kdos-command.md) |
| `kinstall` | The installer | [kinstall](kinstall.md) |
| `ksvc` | The service supervisor | [Boot and init](../03-architecture/boot-and-init.md) |
| `service` | The same binary under the conventional name | [Administration](../02-user-guide/administration.md) |
| `kdos-getty` | Loads the virtual-terminal font and palette, then runs a getty | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-login` | Reads `login.conf` and hands tty1 to agetty | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-bootctl` | Chooses and confirms the A/B root slot | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-splash` | The boot splash | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-banner` | The login banner | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-shot` | Screenshots | [The kdos command](kdos-command.md#kdos-shot) |
| `kdos-theme` | Generates the GTK, icon and cursor themes. The generator, not the picker — the picker is `kdos-style` | [Theming](../02-user-guide/theming.md) |
| `kdos-mpctl` | Music player control: which player a media key reaches | [The kdos command](kdos-command.md#kdos-mpctl) |
| `kdos-share` | Sends a file to another machine over `croc` | [The kdos command](kdos-command.md#kdos-share) |
| `kdos-sfx` | Plays the machine's four sounds: `login`, `notify`, `error`, `degauss` | [The kdos command](kdos-command.md#the-other-names-on-this-binary) |
| `kdos-fetch-app`, `kdos-fetch-static` | Install an alien application from a network; fetch one verified static binary | [The kdos command](kdos-command.md#kdos-fetch-app-and-kdos-fetch-static) |

## Build and development tools

These run on a build host and never ship on the target.

| Command | Does | Documented in |
|---|---|---|
| `kdosbuild` | The build orchestrator | [The build system](../05-developer/build-system.md) |
| `kdos-portup` | Checks every port for a newer upstream release. Compiled on demand by `ports/update` | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/fetch` | Fetches every source a recipe names, from the cache, the source archive or upstream, and generates missing vendor bundles. `make fetch` runs it | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `ports/publish` | Uploads sources the archive lacks, and freezes a release's source list | [Writing ports](../05-developer/writing-ports.md#publishing-sources) |
| `script/hooks/pre-push` | Refuses a push whose recipes name a source the archive does not hold. Enabled with `git config core.hooksPath script/hooks` | [Developing](../05-developer/developing.md#getting-the-source) |
| `ports/update` | The front end to the version checker | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `testing/*` | The test and rig harnesses | [Testing](../05-developer/testing.md) |

## Binaries that answer to several names

Four binaries provide most of the commands on the system. Each dispatches on its own basename and
is installed once, with a symbolic link per further name. A name in the dispatch table with no
matching entry point fails the link; a name with no link installed is a program nothing can reach.

### `kdos-shell` — 53 names

The panel and every surface reachable from it or from a chord. One name, `kdos-launcher`, is a
second entry to the surface `kdos-palette` also opens, so the 53 names resolve to 52 distinct
programs. Each is described in [kdos-shell](kdos-shell.md).

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
| `kdos-rec` | | | |

### `ksvc` — 12 names

The supervisor, and the system tools built from the same binary.

| Name | Is |
|---|---|
| `ksvc` | The service supervisor |
| `service` | The same, under the conventional name |
| `kdos` | The front door and its subcommands |
| `kdos-getty` | The font-and-palette wrapper around a getty |
| `kdos-bootctl` | A/B slot selection. Also copied into the initramfs |
| `kdos-banner` | The login banner |
| `kdos-shot` | Screenshots |
| `kdos-sfx` | Sound effects |
| `kdos-mpctl` | Music player control |
| `kdos-share` | A file to another machine, over `croc`. The name the Share verb resolves |
| `kdos-fetch-app` | Installs an alien application from a network |
| `kdos-fetch-static` | Fetches one verified static binary |

### `kpkg` — 5 names

| Name | Is |
|---|---|
| `kpkg` | The front end |
| `kpkgadd` | Install a prebuilt package file |
| `kpkgbuild` | Build a port without installing it |
| `kpkgdel` | Remove a package |
| `kpkgdepends` | Print the resolved install order |

### `kdos-appbox` — 3 names, plus a shim per application

| Name | Is |
|---|---|
| `kdos-appbox` | Launching boxed applications, and generating their launchers |
| `kdos-box` | The box manager |
| `xdg-open` | Opening a file or a link with whatever this machine opens it with |

`/usr/local/bin` precedes `/usr/bin` on `PATH`, so this binary answers `xdg-open` before
xdg-utils' script does. That script remains installed and is where an unclaimed type ends up.

The same binary is also invoked through a shim named after each installed application, so `gimp`
on your `PATH` is `kdos-appbox` dispatching on that name.

## See also

- [Command index](../06-reference/command-index.md) — every command, alphabetically
- [Architecture overview](../03-architecture/overview.md) — how these programs fit together
- [Repository layout](../06-reference/repository-layout.md) — where each one's source lives
- [The C libraries](../05-developer/c-libraries.md) — what they are built on
- [Configuration](../06-reference/configuration.md) — every configuration key, with defaults
