# Command index

This chapter lists every command that the KDOS source tree itself installs on the target system,
with a one-line description, the directory it is installed in, and a link to the chapter that
documents it in full. It closes with the scripts and `make` targets a contributor runs on the
build machine. Use it to find out what a `kdos-*` name is, where it lives, or where to read more;
it is a lookup table, not an explanation, and assumes you have met the vocabulary in
[The programs](../04-programs/README.md) or the [glossary](glossary.md).

Software built from `ports/core/` (bash, Konsole, Plasma, podman, mpv and the rest) is upstream's,
and upstream's manual pages and handbooks describe it; [the ports catalogue](ports-catalogue.md)
lists every port by shelf, with its phase. This index lists what this repository writes: the
programs under `src/`, the scripts under `fs/`, and the few commands a port recipe adds to its
package under a name of its own. A recipe that installs a wrapper under the upstream program's own
name, such as `chromium`, is left out, because upstream's documentation still describes that
command. Each entry below matches the source that installs it. The two exceptions are
[the upstream commands the desktop and the book name](#upstream-commands-the-desktop-and-the-book-name)
and [the upstream commands the boot path and the book name](#upstream-commands-the-boot-path-and-the-book-name),
which init, the service scripts and this book's procedures run by name.

Every `kdos-*` row in the tables before [Host-only tools](#host-only-tools), and the rows of
`kpkg`, its other names, `ksvc`, `service`, `kinstall` and `xdg-terminal-exec`, becomes a manual
page on the image: the `kdos-doc` port renders the book section the row links to as that command's
`man` page. See [Manual pages and the book](../04-programs/kdos-command.md#manual-pages-and-the-book).

## Where the commands live

KDOS installs its own commands into seven directories. Compiled programs from `src/` go in
`/usr/bin`, and the root daemons go in `/usr/sbin`. The session scripts from `fs/`, `kdos` and the
other session-side names of the `ksvc` binary, and the `kdos-appbox` names go in `/usr/local/bin`,
which precedes `/usr/bin` on the `PATH` that `/etc/profile` sets. `kdos-bootctl` is a name of the
same `ksvc` binary but is in `/usr/bin`, because the initramfs build copies it by path. Helpers
that only another program starts go in `/usr/libexec`.

| Directory | What is there |
|---|---|
| `/usr/bin` | `kdos-boxsock`, `kdos-pack`, `kdos-splash`, `kdos-theme`, `kdos-bb`, `kdos-doc`, `kdos-notice`, `kdos-installer-gui`, `kdos-discover-catalog`, `kinstall`, `kpkg` and its four other names, `kdos-bootctl`, the daemon clients `kdos-power` and `kdos-energy`, and the wrapper scripts ports write |
| `/usr/sbin` | The six root daemons, and `ksvc` with its second name `service` |
| `/usr/local/bin` | `kdos` and the other session-side names of the `ksvc` binary, `kdos-appbox` with `kdos-box`, `xdg-open` and one shim per boxed application, and the session scripts from `fs/` |
| `/usr/local/sbin` | `kdos-dm`, which `/etc/inittab` runs on `tty1` and which runs SDDM, and `kdos-getty`, which `/etc/inittab` runs on `tty2` |
| `/usr/libexec/kdos` | `kdos-boxinit`, which runs inside a box; `kdos-runner`, started by D-Bus for KRunner; `kdos-fwask` and `kdos-library-place`, started by the session's autostart; and the `about-*` scripts the Health page and About this System run |
| `/usr/libexec/kdos-discover` | `kdos-ports-helper`, which Discover runs through polkit to install a port |
| `/usr/lib/turnstile` | `ksvc`, the per-user service backend `turnstiled` runs at a user's first login |

## Binaries with several names

Many of the commands below are one binary installed under several names. The binary reads the
name it was started by (the basename of `argv[0]`) and behaves as that command, so no shell
wrapper sits anywhere in the chain. `ksvc` and `kpkg` also accept the command name as the first
argument when they are started under a name their table does not hold, such as the build output's
own name: `kdos-tools service list` behaves as `service list`. Under a listed name, including
`kpkg`, the first argument is an ordinary argument.

| Binary | Installed as | Names it answers to | Where the name table lives |
|---|---|---|---|
| `ksvc` (the `kdos-tools` port) | `/usr/sbin/ksvc` | 12 | `src/system/kdos-tools/main.c` |
| `kpkg` | `/usr/bin/kpkg` | 5 | `src/system/kdos-kpkg/main.c` |
| `kdos-appbox` | `/usr/local/bin/kdos-appbox` | 3 fixed, plus one per application | `src/system/kdos-appbox/main.c` |
| `kdos-powerd` | `/usr/sbin/kdos-powerd` | 2: `kdos-powerd` and its client `kdos-power` | `src/daemons/kdos-powerd/build.sh` (the symlink) |
| `kdos-energyd` | `/usr/sbin/kdos-energyd` | 2: `kdos-energyd` and its client `kdos-energy` | `src/daemons/kdos-energyd/build.sh` (the symlink) |

The twelve names of the `ksvc` binary are `kdos`, `ksvc`, `service`, `kdos-getty`,
`kdos-bootctl`, `kdos-shot`, `kdos-banner`, `kdos-fetch-app`, `kdos-fetch-static`, `kdos-sfx`,
`kdos-mpctl` and `kdos-share`. The five names of `kpkg` are `kpkg`, `kpkgadd`, `kpkgdel`,
`kpkgbuild` and `kpkgdepends`; `kpkg` itself is built and installed by
`script/phases/10_bootstrap/120_kpkg.sh` rather than by a port.

The three fixed `kdos-appbox` names are `kdos-appbox`, `kdos-box` and `xdg-open`. Any other name
the binary is started under is taken as an application **shim**: a symlink in `/usr/local/bin`
named after the application and pointing at `kdos-appbox`, which is what makes `scribus` an ordinary
command. The build writes one such link per application in a pack the medium carries
(`script/phases/70_image/030_launchers.sh`, through `kdos-appbox genlaunchers`); the default medium
carries no packs, so it ships none. An application you install gets its shim in `~/.local/bin` instead. See
[the program map](../04-programs/README.md).

## The `kdos` command

`kdos` is the single entry point to the system's own tools. It has 41 subcommands, plus `help`,
which prints a cheat sheet of where software lives, the everyday commands and the default
keys. It is installed as `/usr/local/bin/kdos`, a name of the `ksvc` binary. Each subcommand below
links to its section of [The kdos command](../04-programs/kdos-command.md).

| Subcommand | Does |
|---|---|
| [`kdos help`](../04-programs/kdos-command.md#kdos-help) | The cheat sheet: where software lives, the everyday commands and the default keys; `--keys` prints the keys alone, `--pager` pages it; also `-h` and `--help` |
| [`kdos version`](../04-programs/kdos-command.md#kdos-version) | Kernel, C library, userland, session and accent; also `-V` |
| [`kdos status`](../04-programs/kdos-command.md#kdos-status) | Packages, boxes, exported applications and the session |
| [`kdos doctor`](../04-programs/kdos-command.md#kdos-doctor) | Check for the faults known to occur on KDOS, each reported as `ok`, `warn` or `skip`; `--json`, `--cve` |
| [`kdos theme [NAME]`](../04-programs/kdos-command.md#kdos-theme) | Set the accent everywhere at once (`next`, `prev` and `list` too); `style FILE` applies a style file, `--audit` checks the generated colours |
| [`kdos phosphor on\|off`](../04-programs/kdos-command.md#kdos-phosphor) | Phosphor mode: the CRT pass, the motion and the retro accent; also `status`, `preset`, `terminus`, `collapse`, `unfold`, `degauss` |
| [`kdos desk kdos\|dock\|tiling`](../04-programs/kdos-command.md#kdos-desk) | Lay the panels out as one of the three desks; `undo` puts the previous layout back |
| [`kdos tiling on\|off`](../04-programs/kdos-command.md#kdos-tiling) | Auto-tiling, through the bundled Krohnkite script; also `toggle` and `--only DESK` |
| [`kdos settings [PAGE]`](../04-programs/kdos-command.md#kdos-settings) | System Settings, at a page |
| [`kdos menu summon ROUTE`](../04-programs/kdos-command.md#kdos-menu) | Open KRunner on the KDOS runner's named place; `toggle [ROUTE]` opens or closes KRunner |
| [`kdos panel toggle`](../04-programs/kdos-command.md#kdos-panel) | Switch the first panel between always shown and auto-hide |
| [`kdos toggle [NAME]`](../04-programs/kdos-command.md#kdos-toggle) | `stay-awake`, `night-light`, `dnd`, `reduced-motion`: list, flip or set |
| [`kdos notify TEXT [BODY]`](../04-programs/kdos-command.md#kdos-notify) | Raise a desktop notification: `make && kdos notify done` |
| [`kdos remind TIME TEXT`](../04-programs/kdos-command.md#kdos-remind) | A notification later, delivered once (`in 20m`, `at 15:30`, `tomorrow 9`); also `ls` |
| [`kdos privacy`](../04-programs/kdos-command.md#kdos-privacy) | Which programs, and which boxes, hold the microphone, the camera and the screen; `--json`, `--watch` |
| [`kdos checklist`](../04-programs/kdos-command.md#kdos-checklist) | The first-run checklist: `--json` prints its items and which are done, `dismiss ID` and `restore` hide and show items |
| [`kdos backup`](../04-programs/kdos-command.md#kdos-backup) | `run` takes one restic snapshot of what `~/.config/kdos/backup.conf` names |
| [`kdos timezone [AREA/CITY]`](../04-programs/kdos-command.md) | Print, or set, the time zone, through `timedate1` |
| [`kdos hostname [NAME]`](../04-programs/kdos-command.md) | Print, or set, the machine's static host name, through `hostname1` |
| [`kdos locale [NAME=VALUE…]`](../04-programs/kdos-command.md) | Print, or replace, the system locale settings, through `locale1` |
| [`kdos keymap [MAP]`](../04-programs/kdos-command.md) | Print, or set, the console keymap, through `locale1` |
| [`kdos in-vm`](../04-programs/kdos-command.md) | Exit 0 inside a virtual machine |
| [`kdos app`](../04-programs/kdos-command.md#kdos-app) | Applications: `list`, `search`, `info`, `groups`, `install`, `launch`, `remove`, `export`, `import`, and `tui add`/`rm`/`ls` for terminal programs |
| [`kdos trash FILE…`](../04-programs/kdos-command.md#kdos-trash) | The freedesktop trash from a prompt; `--list`, `--restore NAME`, `--rm NAME`, `--empty [-y]` |
| [`kdos places [add DIR [NAME]]`](../04-programs/kdos-command.md#kdos-places) | Dolphin's Places |
| [`kdos thumb FILE…`](../04-programs/kdos-command.md#kdos-thumb) | A thumbnail in the shared freedesktop cache; `--path FILE` |
| [`kdos verify FILE…`](../04-programs/kdos-command.md#kdos-verify) | A file against its `.sha256`, `.sha512`, `SHA256SUMS` or `SHA512SUMS` and its KDOS signature; `--notify` posts the verdict |
| [`kdos speech`](../04-programs/kdos-command.md#kdos-speech) | The transcription models: `list`, `get NAME`, `where`, `remove NAME` |
| [`kdos share [FILE…]`](../04-programs/kdos-command.md#kdos-share) | Send files to another machine over `croc`; `--clipboard` sends the clipboard |
| [`kdos hey`](../04-programs/kdos-command.md#kdos-hey) | Ask KWin from a prompt: `list` the windows, also `boxes`, `outputs` and `run` |
| [`kdos why PATH\|PORT`](../04-programs/kdos-command.md#kdos-why-and-kdos-explain) | What provides this, and the recorded reason it is the way it is |
| [`kdos explain [TOPIC]`](../04-programs/kdos-command.md#kdos-why-and-kdos-explain) | The recorded reasons, browsable |
| [`kdos oracle`](../04-programs/kdos-command.md#kdos-oracle) | One recorded lesson, picked for today |
| [`kdos sandbox [PROFILE] -- CMD`](../04-programs/kdos-command.md#kdos-sandbox) | Run a native program under Landlock; `--read`, `--write`, `--no-network`, `--tcp`, `--explain` |
| [`kdos appid`](../04-programs/kdos-command.md#kdos-appid) | Whether launcher identifiers match the identifiers windows present |
| [`kdos restarts`](../04-programs/kdos-command.md#kdos-restarts) | Which running processes still use code an upgrade replaced or removed |
| [`kdos march`](../04-programs/kdos-command.md#kdos-march) | Measure per-machine optimisation (`probe`, `run PORT…`) and read the recorded verdicts (`report`) |
| [`kdos cve`](../04-programs/kdos-command.md#kdos-cve) | Which pinned versions carry known vulnerabilities, from an offline database |
| [`kdos update`](../04-programs/kdos-command.md#kdos-update) | `check` (what is behind the signed tree, what the binhost holds and whether a newer slot image exists; `--json`), `fetch` (the newest signed ports tree, and the release index through which the binhost index and slot image manifest are taken), `install PORT` (one port from the signed binhost), `apply` (the newer slot image, or the binhost's packages, into the inactive slot; `--source` builds them here instead), `notes` and `theme` |
| [`kdos image`](../04-programs/kdos-command.md#kdos-image) | Signed whole-slot images: `make`, `verify`, `unpack` into a root, and `carry` what a slot keeps from the running one |
| [`kdos rebuild WORKDIR`](../04-programs/kdos-command.md#kdos-rebuild) | Rebuild the image from the sources on the medium; `--dry-run`, `--iso-only` |
| [`kdos clone [DEV]`](../04-programs/kdos-command.md#kdos-clone) | Copy this medium to another device, verified by read-back; with no device, list the devices it may write to |
| [`kdos persist`](../04-programs/kdos-command.md#kdos-persist) | Report the live session's persistence store; `create [DEV]` makes one |

## The desktop

The desktop is KDE Plasma. These are the commands KDOS writes for it; the parts of Plasma KDOS adds
that are plugins rather than commands (System Settings pages, widgets, KWin effects and scripts,
the Discover backend, the KRunner runner, the KIO worker) are listed in
[The programs](../04-programs/README.md).

| Command | Installed in | What it does | Documented in |
|---|---|---|---|
| `kdos-dm` | `/usr/local/sbin` | Run SDDM on `tty1` from `/etc/inittab`, and give a password prompt on that console when SDDM is missing or exits three times in a minute | [Boot and init](../03-architecture/boot-and-init.md) |
| `kdos-session-wrapper` | `/usr/local/bin` | SDDM's session command: start Plasma from a login shell, on the one session bus at `$XDG_RUNTIME_DIR/bus`, with the desktop's environment set | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-notice` | `/usr/bin` | The system's notices as Plasma notifications: restarts after an update, CVEs, the trial slot, a rollback, memory kills; `--scan` looks once, `--bless` confirms this boot once Plasma has stayed up | [The Plasma shell](../04-programs/plasma-shell.md#kdos-notice) |
| `kdos-installer-gui` | `/usr/bin` | Install KDOS: the graphical installer, which runs `kinstall` through `pkexec` | [kinstall](../04-programs/kinstall.md) |
| `kdos-discover-catalog` | `/usr/bin` | Write the AppStream catalogue of the ports a root has installed, or a binary host can add, for Discover; `--root DIR`, `--out FILE`, `--origin NAME`, `--package NAME` | [Discover](../04-programs/discover.md) |
| `kdos-shot` | `/usr/local/bin` | Screenshots through Spectacle, to the clipboard and `~/Pictures/Screenshots`: `region` (the default), `screen` (also `full`), `window`, `qr` | [The kdos command](../04-programs/kdos-command.md#kdos-shot) |
| `kdos-updatedb` | `/usr/local/bin` | Rebuild this user's own `plocate` index, scoped to `$HOME` | [Configuration](configuration.md#etckdostimersd-and-configkdostimersd) |
| `kdos-runner` | `/usr/libexec/kdos` | The KDOS rows of KRunner and the offline library, on the session bus as `io.kdos.Runner`; `--query TEXT` and `--library TEXT` answer once on standard output | [The Plasma shell](../04-programs/plasma-shell.md#krunner-and-the-kdos-runner) |
| `kdos-fwask` | `/usr/libexec/kdos` | Ask once, per network, whether to let in a service a session program serves when the firewall turns its connections away | [KDOS's settings pages](../04-programs/kdos-settings.md#allow-on-this-network) |
| `kdos-library-place` | `/usr/libexec/kdos` | Add the `library:/` entry to Dolphin's Places once per account | [The Plasma shell](../04-programs/plasma-shell.md#the-library-as-a-folder) |

`kdos-shot qr` captures to the runtime directory, decodes a QR code in the picture onto the
clipboard and deletes the picture, so a pairing token never reaches the disk. `kdos-shot` saves to
`~/Pictures/Screenshots` (`$XDG_PICTURES_DIR/Screenshots` when that variable is set).

### Upstream commands the desktop and the book name

These come from ports, and upstream's manual pages and handbooks describe them. They are listed
because the session, the `kdos` command and this book's procedures call them by name.

| Command | Installed in | Port | What it does | Documented in |
|---|---|---|---|---|
| `sddm` | `/usr/bin` | `sddm` | The display manager: the login screen, autologin and the start of each session | [Boot and init](../03-architecture/boot-and-init.md) |
| `loginctl` | `/usr/bin` | `elogind` | Sessions, seats, users and the power actions, through `login1` | [The session](../03-architecture/session.md) |
| `startplasma-wayland` | `/usr/bin` | `plasma-workspace` | Start a Plasma session: KWin, the session manager and `plasmashell` | [The session](../03-architecture/session.md#starting-a-session) |
| `kwin_wayland` | `/usr/bin` | `kwin` | The compositor and window manager | [KWin](../04-programs/kwin.md) |
| `plasmashell` | `/usr/bin` | `plasma-workspace` | The panels, the desktop and the widgets | [The Plasma shell](../04-programs/plasma-shell.md) |
| `krunner` | `/usr/bin` | `plasma-workspace` | The search and command line on `Alt+Space` | [The Plasma shell](../04-programs/plasma-shell.md) |
| `systemsettings` | `/usr/bin` | `systemsettings` | System Settings, every page of it, KDOS's own included | [KDOS's settings pages](../04-programs/kdos-settings.md) |
| `plasma-discover` | `/usr/bin` | `discover` | Software and system updates | [Discover](../04-programs/discover.md) |
| `konsole` | `/usr/bin` | `konsole` | The terminal; the program `Terminal=true` entries open in | [Applications](../02-user-guide/applications.md) |
| `spectacle` | `/usr/bin` | `spectacle` | Screenshots and screen recordings | [The desktop](../02-user-guide/desktop.md) |
| `kwriteconfig6`, `kreadconfig6` | `/usr/bin` | `kconfig` | Write or read one key of a KDE configuration file | [Configuration](configuration.md) |
| `kbuildsycoca6` | `/usr/bin` | `kservice` | Rebuild the cache of desktop entries and plugins that Kickoff and KRunner read | [Packs and boxes](../03-architecture/packs-and-boxes.md) |

## Daemons

The daemons are installed in `/usr/sbin` and started from `/etc/init.d`. Two of them have a
client, which is the daemon's own binary under a second name, a symlink in `/usr/bin`, so a daemon
and its client are always the same version.

| Command | What it does | Documented in |
|---|---|---|
| `kdos-powerd` | The firewall's named services and the look of the machine before a login, for a desktop that is not root: on a socket for `kdos` and as `io.kdos.Power1` on the system bus | [The daemons](../04-programs/daemons.md#kdos-powerd) |
| `kdos-power` | `kdos-powerd`'s client; `suspend`, `poweroff` and `reboot` go to `loginctl` and never reach the daemon | [The daemons](../04-programs/daemons.md#the-client) |
| `kdos-energyd` | Per-application energy attribution: which application is using the processor's power | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-energy` | The per-application energy report; `--json`, `ping` | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-oomd` | The memory-pressure daemon, which spares the desktop | [The daemons](../04-programs/daemons.md#kdos-oomd) |
| `kdos-packd` | The pack daemon, the only program that mounts a pack | [The daemons](../04-programs/daemons.md#kdos-packd) |
| `kdos-settingsd` | `org.freedesktop.hostname1`, `timedate1` and `locale1` on the system bus, over the files KDOS keeps | [KDOS's settings pages](../04-programs/kdos-settings.md#kdos-settingsd) |
| `kdos-sysupdated` | `org.freedesktop.sysupdate1` and `io.kdos.Slots1` on the system bus: system updates into the inactive A/B slot, for Discover and System Settings | [Discover](../04-programs/discover.md#kdos-sysupdated) |

`kdos-power` takes these forms:

```sh
kdos-power suspend|poweroff|reboot
kdos-power ping
kdos-power firewall list|knocks|SERVICE on|off
kdos-power accent SCHEME
kdos-power splash phosphor|modern
```

## Applications and boxes

A **box** is the container an application runs in; see the [glossary](glossary.md).

| Command | Installed in | What it does | Documented in |
|---|---|---|---|
| `kdos-appbox` | `/usr/local/bin` | Launch, install and export boxed applications; open a file in a throwaway box; generate launchers | [kdos-appbox](../04-programs/kdos-appbox.md) |
| `kdos-box` | `/usr/local/bin` | Manage boxes: create, enter, freeze, export, snapshot, profile | [kdos-appbox](../04-programs/kdos-appbox.md#kdos-box) |
| `xdg-open` | `/usr/local/bin` | Open a file or a link with its handler | [kdos-appbox](../04-programs/kdos-appbox.md#the-open-path) |
| `xdg-terminal-exec` | `/usr/local/bin` | Run a command in a Konsole window, for `Terminal=true` handlers launched through GLib | [kdos-appbox](../04-programs/kdos-appbox.md#terminal-entries) |
| `kdos-boxsock` | `/usr/bin` | Open a launcher's tagged Wayland socket in a box's private runtime directory, so KWin knows which box a window came from; `kdos-boxsock [-s confined\|tagged] BOX [LAUNCHER-ID]` | [The daemons](../04-programs/daemons.md#kdos-boxsock) |
| `kdos-boxinit` | `/usr/libexec/kdos` | Process 1 inside a pack box | [Packs and boxes](../03-architecture/packs-and-boxes.md#the-box) |
| `kdos-openarchive` | `/usr/local/bin` | Extract, with `ouch`, an archive beside itself; the default handler for the archive types | [Applications](../02-user-guide/applications.md) |
| `kdos-pack` | `/usr/bin` | Build, sign, re-stamp, index and diff packs | [Packs and boxes](../03-architecture/packs-and-boxes.md#building-a-pack) |
| `kdos-podman-api` | `/usr/bin` | Run a container client against this account's Podman API socket, starting `podman system service` when nothing answers on it | [Configuration](configuration.md#containers) |
| `kdos-ports-helper` | `/usr/libexec/kdos-discover` | Install one port and what it lacks from the signed binary host, as root through polkit, for Discover: `kdos-ports-helper install PORT` | [Discover](../04-programs/discover.md) |

`kdos-appbox` takes `run`, `open`, `throwaway`, `catalogue`, `install`, `uninstall`, `store`,
`import`, `export`, `warmup`, `status`, `session save` and `session restore`, `list`, `apps`,
`keeper` and `genlaunchers`, with the global options `-b`/`--box NAME`, `-l`/`--launcher ID` and
`-v`/`--verbose`. `kdos-box` takes `list`, `create`, `enter`, `run`, `apps`, `export`, `unexport`,
`freeze`, `import`, `clone`, `snapshot`, `snapshots`, `rollback`, `start`, `stop`, `restart`,
`remove`, `profile` and `gc`.

`kdos-pack` takes `build`, `assemble`, `info`, `imagehash`, `uuid`, `image`, `extract-meta`,
`verify`, `sign`, `restamp`, `index`, `delta`, `apply` and `keygen`.

## Packaging

All five names are one binary in `/usr/bin`.

| Command | What it does | Documented in |
|---|---|---|
| `kpkg` | The package manager | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgadd` | Install a package file | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgdel` | Remove a package | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgbuild` | Build the port in the current directory into a package, without installing it | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgdepends` | Print the resolved install order | [Packaging](../03-architecture/packaging.md#kpkg) |

`kpkg` takes `install` (also `i`), `remove` (also `r`), `list` (also `l`, with `--json`), `info`
(with `--json`), `check` (packages by name, or `--all`), `meta`, `fetch` (the sources a port lacks, from the archive and then upstream), `verify` (and `verify --repro`), `keygen`, `sign FILE KEY`, `index` (with `--sign KEY`),
`verify-index`, `verify-pkg`, `delta`, `apply-delta` (with `--expect SHA256`, which deletes the
reconstructed package unless its checksum matches; see [Deltas](../03-architecture/packaging.md#deltas)),
`binhost`, `store gc DIR MAX-SIZE` and `help`, and the options
`--root PATH`, `--keep-cache` and `-f`/`--force`. `install` also takes
`--build-only`, which builds exactly the named ports into the package cache and installs nothing,
and `--commit`, which installs what `--build-only` built; see
[Building apart from installing](../03-architecture/packaging.md#building-apart-from-installing).
`store gc` deletes [package store](../03-architecture/packaging.md#the-package-store) entries, least
recently used first, until the store fits `MAX-SIZE` (bytes, or a `K`, `M`, `G` or `T` suffix).
`kpkg help` describes each.

The other four names take few options. `kpkgadd [-f|--force] [--root PATH] FILE` installs one
package file, `kpkgdel [--root PATH] PACKAGE…` removes packages, and
`kpkgdepends [--root PATH] PORT…` prints the install order on one line. `kpkgbuild` takes no
arguments: it builds the port in the current directory.

`kpkg` has no `update` subcommand: `kpkg update` prints the usage text and exits 1. `kdos update`
is the command that updates the system; see
[Updating a machine](../03-architecture/packaging.md#updating-a-machine).

## Boot, console and services

| Command | Installed in | What it does | Documented in |
|---|---|---|---|
| `kdos-splash` | `/usr/bin` | The boot splash: a fade from black, or Phosphor mode's CRT power-on; `handoff` leaves the screen to SDDM, `preview WxH T OUT [modern\|phosphor]` writes one frame to a file | [Boot and init](../03-architecture/boot-and-init.md#the-splash) |
| `kdos-bootctl` | `/usr/bin` | A/B root slot selection, trial and confirmation, each slot's kernel on the ESP, the boot entries, the splash's motion, and the colours of the boot menu and text consoles | [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection) |
| `kdos-getty` | `/usr/local/sbin` | Load the console font, palette and keymap onto a VT, then run a getty: `kdos-getty TTY GETTY [ARGS…]` | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-banner` | `/usr/local/bin` | The login banner | [Boot and init](../03-architecture/boot-and-init.md#the-login-banner) |
| `ksvc` | `/usr/sbin` | The service supervisor, for the system's services and, with `--user`, each user's own | [ksvc and service](../04-programs/kdos-command.md#ksvc-and-service) |
| `service` | `/usr/sbin` | The same supervisor, under the conventional name | [ksvc and service](../04-programs/kdos-command.md#ksvc-and-service) |
| `kinstall` | `/usr/bin` | The installer: the terminal wizard, and the backend the graphical installer drives through `--events` | [kinstall](../04-programs/kinstall.md) |

`/etc/inittab` runs `kdos-dm` on the first console and
`kdos-getty tty2 /sbin/agetty --noclear tty2 38400 linux` on the second, the recovery console.

`ksvc` (and `service`) takes `list`, `status NAME`, `start NAME`, `stop NAME`, `restart NAME`,
`enable NAME`, `disable NAME` and `help`, the commands a person types; `--user` before the command
works on the user services in `/etc/kdos/user-services` instead, which start at a user's first
login and stop at the last. The init scripts also call `supervise`, `stop-supervised NAME` and
`check NAME`, through `/etc/init.d/service_helper`. See
[ksvc and service](../04-programs/kdos-command.md#ksvc-and-service).

`kdos-bootctl` takes:

```sh
kdos-bootctl status [--json]
kdos-bootctl list [--json[=short|pretty]]
kdos-bootctl select [a|b]
kdos-bootctl mark-good
kdos-bootctl crypt FS-UUID
kdos-bootctl set-slot a|b UUID [LUKS-UUID]
kdos-bootctl try a|b [N]
kdos-bootctl cancel
kdos-bootctl deploy ROOT [a|b]
kdos-bootctl splash phosphor|modern
kdos-bootctl palette [ACCENT]
kdos-bootctl theme [--print] ACCENT
```

### Upstream commands the boot path and the book name

These come from ports, not from this repository, and upstream's manual pages describe them. They
are listed because `/etc/inittab`, the service scripts, the initramfs build and this book's
procedures call them by name. `/sbin` is a link to `/usr/sbin`.

| Command | Installed in | Port | What it does | Documented in |
|---|---|---|---|---|
| `init` | `/sbin` | `sysvinit` | PID 1: reads `/etc/inittab`, runs `rcS`, keeps the consoles and changes runlevel | [Boot and init](../03-architecture/boot-and-init.md#rcs-and-the-service-scripts) |
| `telinit` | `/sbin` | `sysvinit` | Ask init for another runlevel: `0` powers off, `6` reboots, `S` is single-user | [Boot and init](../03-architecture/boot-and-init.md#shutdown) |
| `runlevel` | `/sbin` | `sysvinit` | Print the previous and the current runlevel | [Boot and init](../03-architecture/boot-and-init.md#login-records) |
| `shutdown` | `/sbin` | `sysvinit` | Power off or reboot, now or at a time, through init | [Boot and init](../03-architecture/boot-and-init.md#shutdown) |
| `halt`, `poweroff`, `reboot` | `/sbin` | `sysvinit` | Change to runlevel 0 or 6, or with `-f` stop the machine at once | [Boot and init](../03-architecture/boot-and-init.md#shutdown) |
| `killall5` | `/sbin` | `sysvinit` | Signal every process outside the caller's session | [Boot and init](../03-architecture/boot-and-init.md#shutdown) |
| `sulogin` | `/sbin` | `util-linux` | Ask for root's password on the console and give a shell, for single-user mode | [Boot and init](../03-architecture/boot-and-init.md#single-user-and-emergency-mode) |
| `getent` | `/usr/bin` | `musl-utils` | Look an entry up in `passwd`, `group`, `hosts` and the other name databases | [Packaging](../03-architecture/packaging.md#the-base-userland) |
| `getconf` | `/usr/bin` | `musl-utils` | Print a system configuration value, such as `_NPROCESSORS_ONLN` | [Packaging](../03-architecture/packaging.md#the-base-userland) |
| `iconv` | `/usr/bin` | `musl-utils` | Convert text between character encodings | [Packaging](../03-architecture/packaging.md#the-base-userland) |
| `cpio` | `/usr/bin` | `cpio` | Write and read cpio archives; the initramfs is one | [Boot and init](../03-architecture/boot-and-init.md#the-initramfs) |
| `which` | `/usr/bin` | `which` | Print the full path of a command | [Packaging](../03-architecture/packaging.md#the-base-userland) |
| `xxd` | `/usr/bin` | `tinyxxd` | Hex dump a file, or turn a dump back into bytes with `-r` | [The ports catalogue](ports-catalogue.md#devtools) |
| `ping` | `/usr/bin` | `iputils` | Send ICMP echo requests, as any user and with no setuid bit, through the ICMP sockets `net.ipv4.ping_group_range` opens | [The security model](../03-architecture/security-model.md#setuid-binaries) |
| `arping` | `/usr/bin` | `iputils` | Send ARP requests on a link; needs root | [The security model](../03-architecture/security-model.md#setuid-binaries) |
| `mkpasswd` | `/usr/bin` | `whois` | Hash a password for `/etc/shadow` or `useradd -p`; name the method, `-m sha512crypt`, because with musl's `crypt` the default is DES | [Packaging](../03-architecture/packaging.md#the-base-userland) |
| `tracepath` | `/usr/bin` | `iputils` | Trace the path to a host and the MTU along it, with no privilege | [Packaging](../03-architecture/packaging.md#the-base-userland) |

## Tools and helpers

| Command | Installed in | What it does | Documented in |
|---|---|---|---|
| `kdos-bb` | `/usr/bin` | The ASCII-art demo | [kdos-bb](../04-programs/kdos-bb.md) |
| `kdos-doc` | `/usr/bin` | Open the KDOS book in the Help Center, at a page: `kdos-doc [CHAPTER/PAGE[#ANCHOR]]` | [The kdos command](../04-programs/kdos-command.md#manual-pages-and-the-book) |
| `kdos-theme` | `/usr/bin` | Generate the identity kit for an accent at build time: `kde-colors`, `konsole`, `qt5ct-colors`, `wallpaper`, `greeter-globals`, `contrast`, `accents` | [Theming](../02-user-guide/theming.md#how-the-theme-is-generated) |
| `kdos-share` | `/usr/local/bin` | Send files to another machine over `croc`; `--clipboard` sends the clipboard | [The kdos command](../04-programs/kdos-command.md#kdos-share) |
| `kdos-sfx` | `/usr/local/bin` | The machine's sounds: `login`, `notify`, `error`, `degauss`, `logout`, `warning`, `complete`, `added`, `removed`; `--route` prints the output it would use; `render DIR [--format oga\|wav]` writes the sound theme | [The kdos command](../04-programs/kdos-command.md#kdos-sfx) |
| `kdos-mpctl` | `/usr/local/bin` | Music control: `toggle`, `stop`, `next`, `prev` to mpd or an MPRIS player; `mpris` serves mpd on the session bus as `org.mpris.MediaPlayer2.mpd` | [The kdos command](../04-programs/kdos-command.md#kdos-mpctl) |
| `kdos-fetch-app` | `/usr/local/bin` | Install an application from another distribution into a distrobox; `--box`, `--image`, `--remove` | [The kdos command](../04-programs/kdos-command.md#kdos-fetch-app-and-kdos-fetch-static) |
| `kdos-fetch-static` | `/usr/local/bin` | Fetch one static binary and verify it: `kdos-fetch-static NAME URL SHA256` | [The kdos command](../04-programs/kdos-command.md#kdos-fetch-app-and-kdos-fetch-static) |
| `kdos-syncthing` | `/usr/bin` | Syncthing, given a LAN-only configuration the first time it runs | [Administration](../02-user-guide/administration.md#syncing-and-serving-on-the-network) |
| `update-ca-certificates` | `/usr/bin` | Rewrite `/etc/ssl/cert.pem` from the Mozilla bundle and the local roots in `/etc/ca-certificates/trust-source/anchors/`; `--root DIR` | [Security model](../03-architecture/security-model.md#tls-trust-anchors) |
| `sfeed-read` | `/usr/bin` | Open `sfeed_curses` on every feed under `~/.sfeed/feeds`, or say how to fetch some when there are none | [The `sfeed` port's notes](../../../ports/core/mail/sfeed/kfs.md) |
| `rnodeconf-local` | `/usr/bin` | Run `rnodeconf` against the RNode firmware images the `rnode-firmware` package carries, so flashing and updating a LoRa radio reach no network | [The `rnode-firmware` port's notes](../../../ports/core/hamradio/rnode-firmware/kfs.md) |
| `ldconfig` | `/usr/sbin` | Exits 0 and changes nothing: musl's loader keeps no cache, so there is nothing to rebuild; it answers makefiles and install scripts that run it | [Configuration](configuration.md#other-machine-files) |

`kdos-syncthing`, `update-ca-certificates`, `sfeed-read`, `rnodeconf-local`, `ldconfig` and
`kdos-podman-api` are shell scripts that a port puts in its package (the `syncthing`,
`ca-certificates`, `sfeed`, `rnode-firmware`, `musl` and `podman` ports), not programs under
`src/`. Each recipe writes its script from `build.sh`, except `musl`'s, which installs the
`ldconfig` file kept beside the recipe.

`kdos-mpctl mpris` is started at login by `/etc/xdg/autostart/kdos-mpris.desktop`. It owns
`org.mpris.MediaPlayer2.mpd` while mpd is running and follows mpd's `idle`, so the media keys, the
Media Player widget and the lock screen's controls reach mpd.

## Host-only tools

These run on the machine KDOS is built on, from the top of the repository, and never ship on the
target. [How KDOS is built](../05-developer/how-kdos-is-built.md) follows them in the order a build
uses them, from `make fetch` to a bootable ISO.

### Make targets

| Target | What it does | Documented in |
|---|---|---|
| `make fetch` | Download and verify every recipe's sources; after it, `make build` needs no network. `FETCH_JOBS=N` works on `N` ports at once | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `make fetch-check` | Check offline that every source is present and matches its hash | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `make build` | The whole build, in the container; `BUILD_ARGS` reaches the orchestrator | [Developing](../05-developer/developing.md#make-targets) |
| `make snapshots` | List the phase snapshots | [Developing](../05-developer/developing.md#make-targets) |
| `make updates` | Run `ports/update` over every port; `PORTUP_ARGS` narrows it | [Developing](../05-developer/developing.md#make-targets) |
| `make run`, `make rundisk` | Boot the ISO, or `build/kdos.qcow2`, in a virtual machine with plain graphics | [Developing](../05-developer/developing.md#running-the-result) |
| `make run-hw`, `make rundisk-hw` | The same with hardware-accelerated graphics, through `testing/qemu-hw/` | [Developing](../05-developer/developing.md#running-the-result) |
| `make debug-boot` | Boot the built kernel and initramfs directly, with the serial console on the terminal | [Developing](../05-developer/developing.md#running-the-result) |
| `make cleandisk`, `make cleanbuild`, `make clean` | Replace the test disk; empty `build/` except `snapshots`, `ccache`, `pkgstore`, `keys` and `binhost-key`; empty it except `keys` and `binhost-key` | [Developing](../05-developer/developing.md#make-targets) |
| `make help` | List every target with a one-line description | [Developing](../05-developer/developing.md#make-targets) |
| `make publish`, `publish-dry`, `publish-check`, `publish-plan`, `publish-describe`, `publish-rehome`, `publish-retire`, `publish-orphans` | `ports/publish` and its modes over the source archive's `sources-<N>` volumes; `PORTS` and `PUBLISH_ARGS` narrow them | [Developing](../05-developer/developing.md#make-targets) |
| `make freeze`, `make release`, `make release-publish` | The system release for `TAG`: `sources.sha256`, a draft with the ISO and signed `SHA256SUMS`, then public and Latest | [Developing](../05-developer/developing.md#make-targets) |
| `make check`, `preflight`, `selftest`, `selftest-asan`, `docscheck`, `anchorcheck`, `booknav`, `booknav-write`, `kfscheck`, `phaseclosure`, `defaultscheck`, `xdgcheck`, `lnfcheck`, `aboutcheck`, `atspi-selftest` | The checks that stand in for a build; `check` runs the twelve that need no built tree: every one listed but `selftest-asan` and `booknav-write`, which rewrites the footers `booknav` checks. `defaultscheck` reads every KDOS default for a KDE program back, `xdgcheck` keeps KDOS's defaults out of `/etc/xdg`, `lnfcheck` holds the global themes and desks to what Plasma reads, `aboutcheck` proves the About-page check's verdicts can fail, and `atspi-selftest` checks the AT-SPI dump format on fixtures | [Developing](../05-developer/developing.md#make-targets) |
| `make plasmatest` | The Qt suite: QtTest, plugins, packages and `qmllint` over KDOS's Qt components, in the development image only | [Testing](../05-developer/testing.md) |
| `make depdrift`, `make debuginfo`, `make unowned`, `make linkcheck`, `make entrycheck` | Audit `build/fs` for undeclared same-phase link dependencies, shipped DWARF, unowned files, run-time references that do not resolve, or dangling links and dead entries | [Developing](../05-developer/developing.md#make-targets) |
| `make book`, `make book-data`, `make book-stamp` | Render *KDOS from Scratch* into `build/kfs` (HTML, `kfs.pdf`, `kfs-html.tar.zst`); rewrite its committed snapshot `docs/kfs/data` from a built tree; rewrite its recipe stamp alone, from the checkout. `BUILDFS` names the tree, `KFS_PANDOC` and `KFS_TYPST` the renderers | [Developing](../05-developer/developing.md#make-targets) |
| `make session-probe`, `make shots`, `make atspi` | Boot the ISO in the rig: time to the panel and idle memory, then the Plasma session probe; photograph the desktop, Kickoff, KRunner and the Overview into `build/shots`; check every KDOS surface's AT-SPI tree against its golden | [Testing](../05-developer/testing.md) |
| `make quick`, `make rig-image`, `make devdeps-image` | Rebuild the ports `QUICK` names and patch them into a booted ISO (`QUICK_ARGS` reaches the rig); build the rig's container, `kdos-qemu-py`; build the development image, `kdos-devdeps`, and run the self-test in it | [Testing](../05-developer/testing.md#the-fast-loop) |

`KDOS_RES` (default `1920x1080`) sets the virtual screen for `make run`, `rundisk`,
`run-hw` and `rundisk-hw`. `KDOS_DISK` (default `96G`) is the size of the sparse
`build/kdos.qcow2` that `make run` creates when it is missing and `make cleandisk` creates afresh.

### Scripts and programs

| Command | What it does | Documented in |
|---|---|---|
| `ports/fetch [--check] [--tree DIR] [PORT…]` | What `make fetch` runs: each source from the port directory, the cache, the sources archive or upstream, generating a vendor bundle where one must be made. A port is named by its bare name on any shelf, and a name that is no port fails the run | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `ports/publish [--dry-run \| --check] [--history] [--all \| PORT…]` | Upload sources the archive lacks (with no port named, those new against the upstream branch or unindexed; `--all`, every recipe's) into the volume `sources-<N>` that `ports/shelves` gives the shelf that owns them, as `<shelf>--<file>` (in parts past 1,900 MiB), spilling into another volume when that one is full, and write their lines into `ports/sources.idx`. `--plan` gives every shelf without a volume one; `--describe` rewrites the releases' titles and notes; `--rehome [PORT…]` moves files into the volume and name of the shelf their port sits on and, once the index is pushed, deletes the old copies; `--retire` deletes every `src-<shelf>` release that holds no asset, and its tag; `--orphans` lists index lines no recipe names, and `--orphans --prune=yes-delete` deletes the unprotected ones; `--freeze TAG` attaches a release's hash list. Ports are named as for `ports/fetch` | [Writing ports](../05-developer/writing-ports.md#portspublish) |
| `ports/publish --release TAG [--iso PATH] [--key PATH \| --unsigned] [--binhost DIR] [--image DIR] [--publish] [--dry-run]` | Put the system release `TAG` on `kunaldawn/kdos`: the ISO (in parts from 1,900 MiB), `sources.sha256`, the recipe tree `kdos-ports.tar.xz` with its signature, a signed `SHA256SUMS` and the public key, as a draft until `--publish` makes it public and Latest; and its updates, the binhost in pre-releases `binhost-TAG-NN` of at most 1,000 assets and the slot image in `image-TAG`, with the signed `kdos-update.idx` that says where each file is | [Developing](../05-developer/developing.md#cutting-a-release) |
| `script/hooks/pre-push` | Refuse a push that breaks the ports layout, adds or changes a patch with no `Subject:` line, fails `docscheck.sh` or `kfscheck.py`, or names a source the archive lacks; enabled with `git config core.hooksPath script/hooks` | [Writing ports](../05-developer/writing-ports.md#the-pre-push-hook) |
| `kdosbuild [--fresh] [--restore PHASE] [--continue-from PHASE] [--phases LIST] [--steps LIST] [--rebuild LIST] [--plan] [--snapshot] [--full-snapshots] [--port-jobs N] [--plain] [--json] [--list] [--delete PHASE]` | The build orchestrator, compiled from `src/devtools/kdosbuild/` by `script/kdosbuild.sh`, which `make build` runs with `BUILD_ARGS`. `--port-jobs N` builds up to `N` ports of a package phase at once, by dependency level (1 to 8; the default, 1, builds one at a time) | [The build system](../05-developer/build-system.md#flags) |
| `script/chroot/enter.sh` | A root shell, or one command, inside a built tree, for inspection by hand | [The build system](../05-developer/build-system.md#the-chroot) |
| `ports/update` | Check ports for newer upstream releases; compiles and runs `kdos-portup` | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `kdos-portup` | The version checker `ports/update` runs, from `src/devtools/kdos-portup/`, compiled to `ports/.portup` | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/hackage-vendor` | The Hackage downloader `ports/fetch` runs to make a Haskell port's vendor bundle | [Writing ports](../05-developer/writing-ports.md#the-haskell-bundle) |
| `testing/preflight.sh` | 74 checks over the wiring of the tree | [Testing](../05-developer/testing.md#preflightsh) |
| `testing/depdrift.py [--root ROOT] [--repo REPO] [--strict]` | Name every package of a built root that links against, or whose `.pc` file requires, a package outside its `depends`; a `DRIFT` line is an owner from the package's own phase, which a build with `--port-jobs` may not have installed yet | [Testing](../05-developer/testing.md#undeclared-link-dependencies) |
| `testing/debuginfo.py [--root ROOT] [--threshold BYTES] [--files]` | Sum the `.debug_*` and `.zdebug*` sections of every ELF file under a built root's `usr/` by owning package, and fail above the threshold (1 MiB) outside the ghc `-debug` runtimes | [Testing](../05-developer/testing.md#debug-information-in-the-built-tree) |
| `testing/unowned.py [--root ROOT] [--fs DIR] [--all] [--fail-under DIR]…` | List the files of a built root no package and no `fs/` path provides; `--fail-under` fails only on a finding under that directory | [Testing](../05-developer/testing.md#files-nothing-owns) |
| `testing/linkcheck.py [--root ROOT] [--all]` | Fail on a `DT_NEEDED` owned outside the consumer's `depends`, an interpreter that does not resolve, or OpenSSL 3 and 4 in one process; warn on a bare `python` shebang | [Testing](../05-developer/testing.md#run-time-references) |
| `testing/entrycheck.py [--root ROOT]` | Fail on a dangling symlink, a desktop or D-Bus `Exec=` that does not resolve, a visible `Icon=` with no icon, or a udev program that does not exist | [Testing](../05-developer/testing.md#links-launchers-and-rules) |
| `testing/mimecheck.py` | Fail when a default application in `/usr/share/kdos/xdg/kde-mimeapps.list` names a desktop entry the image does not install | [Testing](../05-developer/testing.md) |
| `testing/plasmatest.sh [--selftest] [--keep] [PORT…]` | The Qt half of the regression net: build KDOS's Qt ports with their QtTest suites, load their plugins, install their packages and lint their QML; `make plasmatest` runs it in the development image | [Testing](../05-developer/testing.md) |
| `testing/defaultscheck.py`, `testing/xdgcheck.sh` | Read every KDOS default for a KDE program back against `testing/fixtures/defaultscheck`; keep KDOS's defaults in `/usr/share/kdos/xdg` and out of `/etc/xdg` | [Testing](../05-developer/testing.md) |
| `testing/lnfcheck.py`, `testing/chordcheck.py`, `testing/monofont.py` | The KDOS global themes and desks hold only what Plasma reads, and pin what the favourites name; no chord KDOS ships is bound twice; what the KDOS monospace face covers and the cell it makes | [Testing](../05-developer/testing.md) |
| `testing/plasma-probe.sh`, `testing/session-cost.sh`, `testing/aboutcheck.sh` | On a booted image: whether the Plasma session is whole; time to the panel and memory at idle; whether every About-page row program prints its two lines | [Testing](../05-developer/testing.md) |
| `testing/atspi/` | Dump each KDOS surface's accessibility tree through AT-SPI and compare it with a committed golden; `make atspi` boots the ISO for it, `make atspi-selftest` checks the format on fixtures | [Testing](../05-developer/testing.md) |
| `testing/bookcounts.py` | Recompute the port counts on the three pages that state them and check every ports-catalogue row against its recipe; `docscheck.sh` runs it | [Testing](../05-developer/testing.md#docschecksh) |
| `testing/recipemeta.py`, `testing/optdeps.py` | Check every recipe's `description`, `homepage`, `license` and first source, and its catalogue row; list every build switch that turns off a feature whose library is a port. Preflight runs both | [Testing](../05-developer/testing.md#recipe-metadata-and-switched-off-features) |
| `testing/phaseclosure.py` | Check that every package phase installs exactly the ports its list names, and that each port of `41_system` to `44_apps` is in the phase the phase table gives its closure; preflight runs it | [Writing ports](../05-developer/writing-ports.md#which-phase-lists-a-port) |
| `testing/selftest.sh` | The library and consumer suite | [Testing](../05-developer/testing.md#selftestsh) |
| `testing/docscheck.sh [--book docs/kdos\|docs/kfs]… [PAGE…]` | Both books: dead links, historical phrasing, each book's page contract and *KDOS from Scratch*'s wiki skeleton, and through `testing/bookcounts.py` the port counts | [Testing](../05-developer/testing.md#docschecksh) |
| `testing/anchorcheck.py [--book DIR]… [PAGE…]` | Every `#fragment` link in both books and `README.md` lands on a heading, by GitHub's id rule | [Testing](../05-developer/testing.md#anchorcheckpy) |
| `testing/booknav.py [--book DIR]… [--write]` | Check, or with `--write` rewrite, every chapter footer of both books and the contents of *KDOS from Scratch* | [Testing](../05-developer/testing.md#booknavpy) |
| `testing/kfscheck.py [--base REF] [--strict] [--only CHECK,…]`, `kfscheck.py --repin` | *KDOS from Scratch* against the tree: sidecar drift, notes and patch headers on new or changed ports, the snapshot and recipe stamp, pinned includes, the kernel option tables against the `linux` port's `kdos.config`, and a full render; `--repin` moves include ranges whose lines moved and pins unpinned ones | [Testing](../05-developer/testing.md#kfscheckpy) |
| `script/book/kfsgen.py`, `kfsrender.py`, `make_data.py` | *KDOS from Scratch*'s renderer: generate the book's Markdown from the tree into `build/kfs/src`; render it to HTML, PDF and a tarball with pandoc and typst; write the `docs/kfs/data` snapshot, or its recipe stamp alone with `--stamp-only`. `make book`, `make book-data` and `make book-stamp` run them | [Developing](../05-developer/developing.md#make-targets) |
| `testing/devdeps-image.sh` | Build the `kdos-devdeps` image and run the self-test in it, with nothing skipped | [Testing](../05-developer/testing.md#the-machine-where-nothing-is-skipped) |
| `testing/rig-image.sh` | Build the `kdos-qemu-py` rig image | [Testing](../05-developer/testing.md#the-host-may-not-have-an-emulator) |
| `testing/vnc-shot.py` | Boot, drive and photograph a real session | [Testing](../05-developer/testing.md#the-qemu-rig) |
| `testing/quick.sh`, `testing/quickpatch.sh` | Rebuild ports and patch them into a booted ISO's in-memory overlay | [Testing](../05-developer/testing.md#the-fast-loop) |
| `testing/qemu-audio.sh` | Pick a working QEMU audio backend for the run targets and the rig | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/usability.sh` | Drive the desktop with the pointer and chords, as a person would, and leave a numbered contact sheet to check against `testing/usability.md` | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/bios-boot.sh` | Boot the ISO with no UEFI firmware | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/packlane.sh` | The [pack lane](glossary.md#lane) (installing signed packs into a box) end to end on a booted machine | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/install-to-disk.sh` | Run the installer into a disk image | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appsweep.sh` | Launch every catalogue application | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appreport.sh` | Report on what the sweep found | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appbox-smoke.sh` | Check that every boxed application's launcher starts something | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/hostcheck.sh` | Check that the binary a caller resolves by name is the one that can do the job | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/oomd-fire.sh` | Put a booted machine under real memory pressure and read `kdos-oomd`'s kill count | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/prepare_base.py`, `testing/test_runner.py` | Build a minimal base image, through `mini_build.py`, and build single ports against it, each in a fresh container; `report_gen.py` summarises the results | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/bootcheck/` | Scripted boots with no display: start QEMU, run a command on the serial console, type | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/qemu-hw/` | The containerised QEMU with accelerated graphics behind `make run-hw` | [Testing](../05-developer/testing.md#the-other-harnesses) |

## See also

- [The programs](../04-programs/README.md): the same commands grouped by what they are, with the
  multi-name mapping and the Plasma plugins KDOS adds
- [The kdos command](../04-programs/kdos-command.md): every subcommand and every other name of the
  `ksvc` binary in full
- [The ports catalogue](ports-catalogue.md): the upstream software this index leaves out, by
  shelf, with the phase that builds each port
- [How KDOS is built](../05-developer/how-kdos-is-built.md): the host-only tools in the order a
  build runs them
- [Configuration](configuration.md): every setting these commands read
- [Filesystem and IPC](filesystem-and-ipc.md): the paths, sockets and bus names they use
- [Glossary](glossary.md): the vocabulary used here

<!-- book-nav -->
---

*Part VI — Reference, chapter 39.* Previous: [38. The ports catalogue](ports-catalogue.md) · [Contents](../README.md) · Next: [40. Configuration](configuration.md)
