# Command index

Every command KDOS itself installs, with a one-line description and a link to the page that
documents it — plus the scripts a contributor runs on a build machine. Use it to find out what a
`kdos-*` name is, or where its full description lives.

Software from `ports/core/` (bash, foot, podman, mpv and the rest) is upstream's, and upstream's
manual pages describe it. This page lists only what this repository writes.

## Binaries with several names

Many of the commands below are one binary installed under several names. Each binary looks at the
name it was started by and behaves as that command, so there is no shell wrapper anywhere in the
chain.

| Binary | Installed as | Names it answers to | Where the name table lives |
|---|---|---|---|
| `kdos-shell` | `/usr/bin/kdos-shell` | 53 | `src/desktop/kdos-shell/main.c` |
| `ksvc` (the `kdos-tools` port) | `/usr/sbin/ksvc` | 12 | `src/packages/kdos-tools/main.c` |
| `kpkg` | `/usr/bin/kpkg` | 5 | `src/packages/kdos-kpkg/main.c` |
| `kdos-appbox` | `/usr/local/bin/kdos-appbox` | 3 fixed, plus one per application | `src/packages/kdos-appbox/main.c` |
| `kdos-powerd` | `/usr/sbin/kdos-powerd` | 2: `kdos-powerd` and its client `kdos-power` | `src/desktop/kdos-powerd/build.sh` (the symlink) |
| `kdos-mountd` | `/usr/sbin/kdos-mountd` | 2: `kdos-mountd` and its client `kdos-mount` | `src/desktop/kdos-mountd/build.sh` (the symlink) |
| `kdos-energyd` | `/usr/sbin/kdos-energyd` | 2: `kdos-energyd` and its client `kdos-energy` | `src/desktop/kdos-energyd/build.sh` (the symlink) |

The three fixed `kdos-appbox` names are `kdos-appbox`, `kdos-box` and `xdg-open`. Beyond those,
every application has a **shim**: a symlink in `/usr/local/bin` named after the application and
pointing at `kdos-appbox`, which is what makes `gimp` an ordinary command. See
[the program map](../04-programs/README.md#binaries-that-answer-to-several-names).

## The `kdos` command

`kdos` is the front door: one command, 31 subcommands (30 plus `help`). `kdos help` groups them by
the question they answer.

| Subcommand | Does |
|---|---|
| `kdos help` | Every command, grouped by which question it answers |
| `kdos version` | Kernel, C library, userland, session and accent |
| `kdos status` | What this machine is and what it is running |
| `kdos doctor` | Check the things that actually break here |
| `kdos theme [name]` | Switch accent, apply a style file, audit the palette |
| `kdos settings [page]` | The control centre, or one of its nine pages |
| `kdos menu summon ROUTE` | Open a menu on a named place; `toggle` is the other verb |
| `kdos panel toggle` | Put the panel away and bring it back |
| `kdos toggle [name]` | `stay-awake`, `night-light`, `dnd` — list, flip or set |
| `kdos notify TEXT` | Raise a toast; `--time`, `--battery`, `--dismiss`, `--dismiss-all`, `--raise`, `--dnd` |
| `kdos remind TIME TEXT` | A toast later, delivered once; also `ls`, `clear`, `--ask`, `fire ID` |
| `kdos app` | Applications: list, search, info, groups, install, launch, remove, export, import |
| `kdos trash FILE` | The freedesktop trash from a prompt; `--restore` is the way back |
| `kdos places [add DIR]` | The places column the desktop shows |
| `kdos thumb FILE` | A thumbnail in the shared freedesktop cache; `--path` and `--ppm` |
| `kdos speech` | The transcription model: `list`, `get NAME`, `where`, `remove NAME` |
| `kdos share [FILE…]` | Send a file to another machine over `croc`; `--clipboard` for the clipboard |
| `kdos hey` | Ask the compositor about windows, outputs and boxes |
| `kdos why PATH` | What provides this, and why it is the way it is |
| `kdos explain [topic]` | The recorded reasons, browsable |
| `kdos oracle` | One recorded lesson, picked for today |
| `kdos sandbox PROFILE -- CMD` | Run a native application under Landlock |
| `kdos appid` | Do launcher identifiers match what windows present |
| `kdos restarts` | Which running processes still use code an upgrade replaced or removed |
| `kdos stutter` | Why a frame was late, and who was busy |
| `kdos march` | Measure per-machine optimisation, and read the ledger |
| `kdos cve` | Which pinned versions carry known vulnerabilities, offline |
| `kdos update` | Orchestrate a system update; `check --json` writes the panel's badge |
| `kdos rebuild` | Rebuild the image from the sources on the medium |
| `kdos clone [DEV]` | Copy this medium to another device, verified by read-back |
| `kdos persist` | Report the live session's persistence store; `create [DEV]` makes one |

Full descriptions are in [The kdos command](../04-programs/kdos-command.md).

## Desktop surfaces

A **surface** is one window or popup of the desktop. All 53 names below are `kdos-shell`, and they
reach 52 distinct surfaces: `kdos-launcher` and `kdos-palette` are one search program that reads
the name it was started by. Every surface is drawn as a grid of character cells and handed to the
compositor as an ordinary Wayland surface; the frame around it is the compositor's own, drawn with
pango.

| Command | What it does | Documented in |
|---|---|---|
| `kdos-shell` | The panel | [kdos-shell](../04-programs/kdos-shell.md#the-panel) |
| `kdos-start` | The Start menu | [kdos-shell](../04-programs/kdos-shell.md#kdos-start) |
| `kdos-launcher` | Full-screen application search | [kdos-shell](../04-programs/kdos-shell.md#kdos-palette-and-kdos-launcher) |
| `kdos-palette` | One search over everything the desktop can reach | [kdos-shell](../04-programs/kdos-shell.md#kdos-palette-and-kdos-launcher) |
| `kdos-menu` | Root, System and window menus | [kdos-shell](../04-programs/kdos-shell.md#kdos-menu) |
| `kdos-desk` | The desktop and its icons | [kdos-shell](../04-programs/kdos-shell.md#kdos-desk) |
| `kdos-pick` | The file chooser and browser | [kdos-shell](../04-programs/kdos-shell.md#kdos-pick) |
| `kdos-peek` | What is in a file, without starting its application | [kdos-shell](../04-programs/kdos-shell.md#kdos-peek) |
| `kdos-find` | Files by name or contents, applications and recent files | [kdos-shell](../04-programs/kdos-shell.md#kdos-find) |
| `kdos-pix` | One picture, and the folder it is in | [kdos-shell](../04-programs/kdos-shell.md#kdos-pix) |
| `kdos-rec` | Record a microphone, watch the level, transcribe it | [kdos-shell](../04-programs/kdos-shell.md#kdos-rec) |
| `kdos-ascii` | Render a picture as characters | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-run` | The run box | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-prompt` | Yes or no, by exit status; `--input` asks for a line instead | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-notify` | The notification centre | [kdos-shell](../04-programs/kdos-shell.md#notifications) |
| `kdos-notifyd` | The notification daemon | [kdos-shell](../04-programs/kdos-shell.md#notifications) |
| `kdos-mediad` | Removable-media toasts, with Open and Eject | [The daemons](../04-programs/daemons.md#kdos-mountd) |
| `kdos-netagent` | The NetworkManager secret agent | [kdos-shell](../04-programs/kdos-shell.md#kdos-netagent) |
| `kdos-osd` | Volume and brightness | [kdos-shell](../04-programs/kdos-shell.md#kdos-osd) |
| `kdos-cal` | The calendar | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-display` | Screen configuration | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-keys` | The keybinding card | [kdos-shell](../04-programs/kdos-shell.md#kdos-keys) |
| `kdos-teams` | The window list | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-saver` | Attract mode between idle and lock | [kdos-shell](../04-programs/kdos-shell.md#kdos-saver) |
| `kdos-about` | What this machine is | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-style` | The accent on one page, the font on the other | [kdos-shell](../04-programs/kdos-shell.md#kdos-style) |
| `kdos-calc` | The calculator | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-chars` | The character map | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-connect` | A folder on another machine, over SMB | [kdos-shell](../04-programs/kdos-shell.md#kdos-connect) |
| `kdos-traymenu` | A tray item's own `com.canonical.dbusmenu` menu, as cells | [kdos-shell](../04-programs/kdos-shell.md#the-tray) |
| `kdos-contacts` | The address book | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-disks` | Disks: mount, unlock, SMART, partition, erase | [kdos-shell](../04-programs/kdos-shell.md#kdos-disks) |
| `kdos-print` | Printers: what is set up, what is on the network | [kdos-shell](../04-programs/kdos-shell.md#kdos-print) |
| `kdos-time` | The time zone, the clock, and whether the clock is right | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-users` | The accounts, and which one `tty1` logs in | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-update` | What is behind, what is vulnerable, which root slot is live | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-store` | What this machine can build, and one tick to build it | [kdos-shell](../04-programs/kdos-shell.md#kdos-store) |
| `kdos-firewall` | Which services answer the network | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-backup` | What is in the restic repository, and one key to add to it | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-note` | The scratch pad | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-slit` | The dockapp column | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-doc` | The documentation viewer | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-settings` | The control centre | [kdos-shell](../04-programs/kdos-shell.md#kdos-settings) |
| `kdos-openwith` | Choose a handler for a file | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-audio` | Audio devices | [kdos-shell](../04-programs/kdos-shell.md#kdos-audio) |
| `kdos-net` | Networking | [kdos-shell](../04-programs/kdos-shell.md#kdos-net) |
| `kdos-bt` | Bluetooth | [kdos-shell](../04-programs/kdos-shell.md#kdos-bt) |
| `kdos-devices` | Cameras, microphones, removable media | [kdos-shell](../04-programs/kdos-shell.md#kdos-devices) |
| `kdos-clip` | Clipboard history | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-status` | The overflow popup | [kdos-shell](../04-programs/kdos-shell.md#the-overflow-chevron) |
| `kdos-tip` | Tooltips | [kdos-shell](../04-programs/kdos-shell.md#tooltips) |
| `kdos-ime` | The input-method candidate window, as cells | [kdos-shell](../04-programs/kdos-shell.md#kdos-ime) |
| `kdos-trash` | What was deleted, and the way back | [kdos-shell](../04-programs/kdos-shell.md#kdos-trash) |

### Flags worth knowing

| Command | Flags |
|---|---|
| `kdos-prompt` | `--input` makes it a one-row box: the typed line on standard output, exit 0 for an answer and 254 for none |
| `kdos-rec` | `--input hw:C,D`, `--font`, `--dump`, and the test inputs `--fixture DIR`, `--meter FILE`, `--write OUT` |
| `kdos-style` | `--page accent` or `--page font` opens either page directly |
| `kdos-traymenu` | Takes `SERVICE PATH`; also `--name`, `--at`, `--at-bottom`, and the test inputs `--open ID`, `--pick ID`, `--dump` |

Every surface except `kdos-ascii`, `kdos-ime`, `kdos-mediad` and `kdos-netagent` also takes
`--dump` to print one frame as text with no display; see
[Rendering one frame with no display](../04-programs/kdos-shell.md#rendering-one-frame-with-no-display).

## The compositor and its session

| Command | What it does | Documented in |
|---|---|---|
| `kdos-comp` | The compositor | [kdos-comp](../04-programs/kdos-comp.md) |
| `kdos-desktop` | Start a session from a tty | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-desktop-start` | Bring the media stack up, then the compositor | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-session-save` | Write down what is running, so the next login can restore it | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-lock` | The lock screen | [The daemons](../04-programs/daemons.md#kdos-lock) |
| `kdos-boxsock` | One tagged compositor socket per box, so the compositor knows which box a window came from | [The daemons](../04-programs/daemons.md#kdos-boxsock) |
| `xdg-desktop-portal-kdos` | The portal backend: file chooser, settings, app chooser, access. Installed in `/usr/lib` and started by D-Bus | [The session](../03-architecture/session.md#the-kdos-backend) |
| `kdos-record` | Record the desktop through the ScreenCast portal; run it again to stop | [The session](../03-architecture/session.md#the-kdos-backend) |
| `kdos-shot` | Screenshots, through `grim` and `slurp`: `region` (the default; `window` is accepted and behaves as `region`), `screen` (also `full`), `qr` | [The desktop](../02-user-guide/desktop.md) |
| `kdos-updatedb` | Rebuild this user's own `plocate` index from `$HOME` | [Configuration](configuration.md) |

While `kdos-record` runs, its process id is in `$XDG_RUNTIME_DIR/kdos/screencast.pid` and the
panel draws a `•REC` lamp. `kdos-shot qr` decodes a QR code in the picture onto the clipboard and
deletes the picture, so a pairing token never reaches the disk.

## Daemons

| Command | What it does | Documented in |
|---|---|---|
| `kdos-powerd` | Suspend, poweroff, reboot, and the system settings a desktop user may change | [The daemons](../04-programs/daemons.md#kdos-powerd) |
| `kdos-power` | Client for the power daemon | [The daemons](../04-programs/daemons.md#kdos-powerd) |
| `kdos-mountd` | The removable-media daemon: disks, LUKS, SMB shares, SMART | [The daemons](../04-programs/daemons.md#kdos-mountd) |
| `kdos-mount` | Its command-line client | [The daemons](../04-programs/daemons.md#kdos-mountd) |
| `kdos-energyd` | The energy daemon | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-energy` | Per-application energy report | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-oomd` | The memory-pressure daemon, which spares the desktop | [The daemons](../04-programs/daemons.md#kdos-oomd) |
| `kdos-packd` | The pack daemon — the only thing that mounts a pack | [The daemons](../04-programs/daemons.md#kdos-packd) |

The daemons are installed in `/usr/sbin` and their clients in `/usr/bin`. Each client is its
daemon's own binary under a second name — a symlink in `/usr/bin` — so the daemon and its client
are always the same version.

`kdos-mount` takes nine verbs:

| Verb | Does |
|---|---|
| `list` | The devices the daemon offers, one numbered row each |
| `mount N`, `unmount N`, `smart N` | Act on row `N` of `list` |
| `shares` | The network shares that are connected |
| `browse` | What answered an mDNS and a NetBIOS broadcast just now, as `name<TAB>address` rows |
| `krb5 SERVER SHARE USER DOMAIN` | Mount a share with the Kerberos ticket `kinit` already obtained; `-` means no user or no domain |
| `ping` | Is the daemon answering |
| `subscribe` | Print `changed` whenever the list changes; never exits |

There is no form that takes a device or a mount point: the daemon decides both. The daemon's socket
answers fifteen verbs — the nine above plus `eject`, `close`, `unlock`, `format`, `cifs` and
`disconnect`, which `kdos-disks` and `kdos-connect` send. The full protocol is in
[Filesystem and IPC](filesystem-and-ipc.md#sockets).

## Applications and boxes

A **box** is the container an application runs in; see the [glossary](glossary.md).

| Command | What it does | Documented in |
|---|---|---|
| `kdos-appbox` | Install, launch and export boxed applications; generate launchers | [kdos-appbox](../04-programs/kdos-appbox.md) |
| `kdos-box` | Manage boxes: create, enter, freeze, export, profile | [kdos-appbox](../04-programs/kdos-appbox.md) |
| `kdos-boxinit` | Process 1 inside a pack box (`/usr/libexec/kdos/kdos-boxinit`) | [Packs and boxes](../03-architecture/packs-and-boxes.md#the-box) |
| `xdg-open` | Open a file or a link with its handler | [kdos-appbox](../04-programs/kdos-appbox.md#the-open-path) |
| `xdg-terminal-exec` | Run a command in a terminal window (`foot`), for handlers that need one | [kdos-appbox](../04-programs/kdos-appbox.md#the-open-path) |
| `kdos-openarchive` | Extract an archive `mc` cannot browse as a directory, beside itself | [kdos-shell](../04-programs/kdos-shell.md) |
| `kdos-pack` | Build, sign, re-stamp, index and diff packs | [Packs and boxes](../03-architecture/packs-and-boxes.md#building-a-pack) |
| `kdos-podman-api` | Run a container client against this account's Podman API socket, starting `podman system service` when nothing answers on it | [Configuration](configuration.md#containers) |

## Packaging

| Command | What it does | Documented in |
|---|---|---|
| `kpkg` | The package manager | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgadd` | Install a package file | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgdel` | Remove a package | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgbuild` | Build the port in the current directory into a package, without installing it | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgdepends` | Print the resolved install order | [Packaging](../03-architecture/packaging.md#kpkg) |

`kpkg` takes `install`, `remove`, `list`, `info`, `meta`, `verify` (and
`verify --repro`), `keygen`, `index` (with `--sign KEY`), `verify-index`, `verify-pkg`, `delta`,
`apply-delta`, `binhost` and `help`, and the options `--root PATH`, `--keep-cache`, `-f`/`--force`
and `--overwrite`. `kpkg help` describes each. `kpkg help` also lists `update`, which `kpkg` does not
implement: `kpkg update` prints the usage text and exits 1. `kdos update` is the command that
updates the system; see [Updating a machine](../03-architecture/packaging.md#updating-a-machine).

## Boot, console and services

| Command | What it does | Documented in |
|---|---|---|
| `kdos-splash` | The boot splash | [Boot and init](../03-architecture/boot-and-init.md#the-splash) |
| `kdos-bootctl` | A/B root slot selection and confirmation, each slot's kernel on the ESP, and the colours of the boot menu and text consoles | [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection) |
| `kdos-login` | Hand `tty1` to `agetty`, logging in automatically when `/etc/kdos/login.conf` names an account | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-getty` | Load the console font and palette, then run a getty | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-banner` | The login banner | [Boot and init](../03-architecture/boot-and-init.md#the-login-banner) |
| `ksvc` | The service supervisor | [Administration](../02-user-guide/administration.md#services) |
| `service` | The same supervisor, under the conventional name | [Administration](../02-user-guide/administration.md#services) |
| `kinstall` | The installer | [kinstall](../04-programs/kinstall.md) |

`ksvc` (and `service`) takes `list`, `status NAME`, `start NAME`, `stop NAME`, `restart NAME`,
`enable NAME`, `disable NAME` and `help` — the commands a person types. Init scripts also call
`supervise`, `stop-supervised NAME` and `check NAME`.

`kdos-bootctl` takes:

```
kdos-bootctl status [--json]
kdos-bootctl select [a|b]
kdos-bootctl mark-good
kdos-bootctl crypt FS-UUID
kdos-bootctl set-slot a|b UUID [LUKS-UUID]
kdos-bootctl try a|b [N]
kdos-bootctl deploy ROOT [a|b]
kdos-bootctl palette [ACCENT]
kdos-bootctl theme [--print] ACCENT
```

## Tools and helpers

| Command | What it does | Documented in |
|---|---|---|
| `kdos-res` | The resource monitor | [kdos-res](../04-programs/kdos-res.md) |
| `kdos-term` | The terminal | [kdos-term](../04-programs/kdos-term.md) |
| `kdos-bb` | The ASCII-art demo | [kdos-bb](../04-programs/kdos-bb.md) |
| `kdos-theme` | Generate the GTK, icon and cursor themes | [Theming](../02-user-guide/theming.md#how-the-theme-is-generated) |
| `kdos-share` | Send a file to another machine over `croc` | [The kdos command](../04-programs/kdos-command.md#kdos-share) |
| `kdos-sfx` | Sound effects: `login`, `notify`, `error`, `degauss` | [The kdos command](../04-programs/kdos-command.md#the-other-names-on-this-binary) |
| `kdos-mpctl` | Music control: `toggle`, `stop`, `next`, `prev` to mpd or an MPRIS player; `now` and `watch` from mpd | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-fetch-app` | Install an alien application from a network | [The kdos command](../04-programs/kdos-command.md#the-other-names-on-this-binary) |
| `kdos-fetch-static` | Fetch a single verified static binary | [The kdos command](../04-programs/kdos-command.md#the-other-names-on-this-binary) |
| `kdos-syncthing` | Syncthing, given a LAN-only configuration the first time it runs | [Administration](../02-user-guide/administration.md#syncing-and-serving-on-the-network) |
| `update-ca-certificates` | Rewrite `/etc/ssl/cert.pem` from the Mozilla bundle and the local roots in `/etc/ca-certificates/trust-source/anchors/` | [Security model](../03-architecture/security-model.md#tls-trust-anchors) |

`kdos-term` takes `-e` (also `--exec`), `--title`, `--app-id`, `--font`, `--size WxH`, `--float`,
`-D DIR` (also `--working-directory`), `--tty` and `--dump WxH`. Everything after `--` is the
command to run.

`kdos-mpctl watch` writes `$XDG_RUNTIME_DIR/kdos/nowplaying` and waits in mpd's `idle`; it
reconnects when mpd restarts, and the session starts it once an mpd configuration exists.

## Privileged helpers

Two of KDOS's own binaries are setuid root, each doing one thing an unprivileged process cannot.
The image's whole setuid inventory — these two and seventeen upstream ones — is in
[The security model](../03-architecture/security-model.md#setuid-binaries).

| Command | What it does | Documented in |
|---|---|---|
| `kdos-checkpass` | Check the caller's own password against `/etc/shadow` | [The security model](../03-architecture/security-model.md#kdos-checkpass) |
| `kdos-resctl` | Signal or renice a process, and read the hardware table | [The security model](../03-architecture/security-model.md#kdos-resctl) |

## Host-only tools

These run on the machine you build KDOS on, from the top of the repository, and never ship on the
target.

| Command | What it does | Documented in |
|---|---|---|
| `make fetch` | Download and verify every recipe's sources; the only step that uses the network | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `make fetch-check` | Check offline that every source is present and matches its hash | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `ports/fetch [--check] [--tree DIR] [PORT…]` | What `make fetch` runs: each source from the port directory, the cache, the sources archive or upstream, generating a vendor bundle where one must be made | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `ports/publish [--dry-run] [--check] [--history] [--freeze TAG] [PORT…]` | Upload sources to the sources archive; `--freeze` attaches a release's hash list | [Writing ports](../05-developer/writing-ports.md#publishing-sources) |
| `script/hooks/pre-push` | Refuse a push naming a source the archive lacks; enabled with `git config core.hooksPath script/hooks` | [Writing ports](../05-developer/writing-ports.md#the-pre-push-hook) |
| `make build` | The whole build, in the container; `BUILD_ARGS` reaches the orchestrator | [Developing](../05-developer/developing.md#make-targets) |
| `make run`, `make run-hw` | Boot the ISO in a virtual machine, with plain or hardware-accelerated graphics | [Developing](../05-developer/developing.md#running-the-result) |
| `make updates` | Run `ports/update` over every port; `PORTUP_ARGS` narrows it | [Developing](../05-developer/developing.md#make-targets) |
| `kdosbuild` | The build orchestrator, run by `make build` | [The build system](../05-developer/build-system.md#kdosbuild) |
| `ports/update` | Check ports for newer upstream releases; compiles and runs `kdos-portup` | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/hackage-vendor` | The Hackage downloader `ports/fetch` runs to make a Haskell port's vendor bundle | [Writing ports](../05-developer/writing-ports.md#the-haskell-bundle) |
| `kdos-portup` | The version checker `ports/update` runs, cached at `ports/.portup` | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `testing/preflight.sh` | 48 checks over the wiring of the tree | [Testing](../05-developer/testing.md#preflightsh) |
| `testing/selftest.sh` | The library and consumer suite, with the goldens | [Testing](../05-developer/testing.md#selftestsh) |
| `testing/docscheck.sh` | This book: dead links, historical phrasing, the page contract | [Testing](../05-developer/testing.md#docschecksh) |
| `testing/devdeps-image.sh` | Build the `kdos-devdeps` image and run the self-test in it, with nothing skipped | [Testing](../05-developer/testing.md#the-machine-where-nothing-is-skipped) |
| `testing/rig-image.sh` | Build the `kdos-qemu-py` rig image | [Testing](../05-developer/testing.md#the-host-may-not-have-an-emulator) |
| `testing/vnc-shot.py` | Boot, drive and photograph a real session | [Testing](../05-developer/testing.md#the-qemu-rig) |
| `testing/quick.sh` | Rebuild ports and patch them into a booted ISO's in-memory overlay | [Testing](../05-developer/testing.md#the-fast-loop) |
| `testing/usability.sh` | Drive the desktop by hand and leave a contact sheet | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/bios-boot.sh` | Boot the ISO with no UEFI firmware | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/packlane.sh` | The application lane on a booted machine | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/install-to-disk.sh` | Run the installer into a disk image | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appsweep.sh` | Launch every catalogue application | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appreport.sh` | Report on what the sweep found | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appbox-smoke.sh` | Check that every boxed application's launcher starts something | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/hostcheck.sh` | Check that the binary a caller resolves by name is the one that can do the job | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/oomd-fire.sh` | Put a booted machine under real memory pressure and read `kdos-oomd`'s kill count | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/bootcheck/` | Scripted boots with no display: start QEMU, run a command on the serial console, type | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/qemu-hw/` | The containerised QEMU with accelerated graphics behind `make run-hw` | [Testing](../05-developer/testing.md#the-other-harnesses) |

## See also

- [The programs](../04-programs/README.md) — grouped by what they are, with the multi-name mapping
- [Configuration](configuration.md) — every setting these read
- [Filesystem and IPC](filesystem-and-ipc.md) — the paths and sockets they use
- [Glossary](glossary.md) — the vocabulary
