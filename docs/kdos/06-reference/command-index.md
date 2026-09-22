# Command index

Every command KDOS itself installs, with a one-line description and a link to the page that
documents it. Software from `ports/core/` is upstream's, and upstream's manual pages describe it.

Many of the commands below are the same binary under different names. Each binary dispatches on the
name it was invoked as, so there is no shell wrapper anywhere in the chain.

| Binary | Names it answers to | Where the table lives |
|---|---|---|
| `kdos-shell` | 53 | `src/desktop/kdos-shell/main.c` |
| `kdos-tools` | 12 | `src/packages/kdos-tools/main.c` |
| `kpkg` | 5 | `src/packages/kdos-kpkg/main.c` |
| `kdos-appbox` | 3 fixed, plus every shim | `src/packages/kdos-appbox/main.c` |

Beyond those, the system installs one shim per installed application, named after the application
and pointing at `kdos-appbox`. See [the program map](../04-programs/README.md#multi-name-binaries).

## The `kdos` command

`kdos` is the front door: one binary, 31 subcommands. `kdos help` groups them by the question they
answer.

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
| `kdos restarts` | Which supervised services have been restarting |
| `kdos stutter` | Why a frame was late, and who was busy |
| `kdos march` | Measure per-machine optimisation, and read the ledger |
| `kdos cve` | Which pins carry known vulnerabilities, offline |
| `kdos update` | Orchestrate a system update; `check --json` writes the panel's badge |
| `kdos rebuild` | Rebuild the image from the sources on the medium |
| `kdos clone [DEV]` | Copy this medium to another device, verified by read-back |
| `kdos persist` | Report the live session's persistence store; `create [DEV]` makes one |

Full descriptions are in [The kdos command](../04-programs/kdos-command.md).

## Desktop surfaces

All 53 names below are `kdos-shell`, and they reach 52 distinct surfaces: `kdos-launcher` and
`kdos-palette` are one search program that reads the name it was invoked by. Every one of them is
drawn as a grid of character cells and handed to the compositor as an ordinary Wayland surface; the
frame around that surface is the compositor's own, drawn with pango.

| Command | What it does | Documented in |
|---|---|---|
| `kdos-shell` | The panel | [kdos-shell](../04-programs/kdos-shell.md#the-panel) |
| `kdos-start` | The Start menu | [kdos-shell](../04-programs/kdos-shell.md#kdos-start) |
| `kdos-launcher` | Full-screen application search | [kdos-shell](../04-programs/kdos-shell.md#kdos-launcher) |
| `kdos-palette` | One search over everything the desktop can reach | [kdos-shell](../04-programs/kdos-shell.md#kdos-palette) |
| `kdos-menu` | Root, System and window menus | [kdos-shell](../04-programs/kdos-shell.md#kdos-menu) |
| `kdos-desk` | The desktop and its icons | [kdos-shell](../04-programs/kdos-shell.md#kdos-desk) |
| `kdos-pick` | The file chooser and browser | [kdos-shell](../04-programs/kdos-shell.md#kdos-pick) |
| `kdos-peek` | What is in a file, without starting its application | [kdos-shell](../04-programs/kdos-shell.md#kdos-peek) |
| `kdos-find` | Files by name or contents, applications and recents | [kdos-shell](../04-programs/kdos-shell.md#kdos-find) |
| `kdos-pix` | One picture, and the folder it is in | [kdos-shell](../04-programs/kdos-shell.md#kdos-pix) |
| `kdos-rec` | Record a microphone, watch the level, transcribe it | [kdos-shell](../04-programs/kdos-shell.md#kdos-rec) |
| `kdos-ascii` | Render a picture as characters | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-run` | The run box | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-prompt` | Yes or no, by exit status; `--input` asks for a line instead | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-notify` | The notification centre | [kdos-shell](../04-programs/kdos-shell.md#notifications) |
| `kdos-notifyd` | The notification daemon | [kdos-shell](../04-programs/kdos-shell.md#notifications) |
| `kdos-mediad` | Removable-media toasts, with Open and Eject | [The daemons](../04-programs/daemons.md#kdos-mountd) |
| `kdos-netagent` | The NetworkManager secret agent | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-osd` | Volume and brightness | [kdos-shell](../04-programs/kdos-shell.md#kdos-osd) |
| `kdos-cal` | The calendar | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-display` | Screen configuration | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-keys` | The keybinding card | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-teams` | The window list | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-saver` | Attract mode between idle and lock | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-about` | What this machine is | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-style` | The accent on one page, the font on the other | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-calc` | The calculator | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-chars` | The character map | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-connect` | A folder on another machine, over SMB | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-traymenu` | A tray item's own `com.canonical.dbusmenu` tree, as cells | [kdos-shell](../04-programs/kdos-shell.md#the-tray) |
| `kdos-contacts` | The address book | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-disks` | Disks: mount, unlock, SMART, partition, erase | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-print` | Printers: what is set up, what is on the network | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-time` | The zone, the clock, and whether the clock is right | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-users` | The accounts, and which one tty1 logs in | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-update` | What is behind, what is vulnerable, which slot is live | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-store` | What this machine can build, and one tick to build it | [kdos-shell](../04-programs/kdos-shell.md#kdos-store) |
| `kdos-firewall` | Which services answer the network | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-backup` | What is in the restic repository, and one key to add to it | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-note` | The scratch pad | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-slit` | The dockapp column | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-doc` | The documentation viewer | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-settings` | The control centre | [kdos-shell](../04-programs/kdos-shell.md#kdos-settings) |
| `kdos-openwith` | Choose a handler for a file | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-audio` | Audio devices | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-net` | Networking | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-bt` | Bluetooth | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-devices` | Cameras, microphones, removable media | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-clip` | Clipboard history | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-status` | The overflow popup | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-tip` | Tooltips | [kdos-shell](../04-programs/kdos-shell.md#tooltips) |
| `kdos-ime` | The input-method candidate window, as cells | [kdos-shell](../04-programs/kdos-shell.md#the-candidate-window) |
| `kdos-trash` | What was deleted, and the way back | [kdos-shell](../04-programs/kdos-shell.md#kdos-trash) |

### Flags worth knowing

| Command | Flags |
|---|---|
| `kdos-prompt` | `--input` makes it a one-row box: the typed line on stdout, exit 0 for an answer and 254 for none |
| `kdos-rec` | `--input hw:C,D`, `--font`, `--dump`, and the test seams `--fixture DIR`, `--meter FILE`, `--write OUT` |
| `kdos-style` | `--page accent` or `--page font` opens either page directly |
| `kdos-traymenu` | Takes `SERVICE PATH`; also `--name`, `--at`, `--at-bottom`, and the seams `--open ID`, `--pick ID`, `--dump` |

## The compositor and its session

| Command | What it does | Documented in |
|---|---|---|
| `kdos-comp` | The compositor | [kdos-comp](../04-programs/kdos-comp.md) |
| `kdos-desktop` | Start a session from a tty | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-desktop-start` | Bring the media stack up, then the compositor | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-session-save` | Write down what is running, so the next login can restore it | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-lock` | The lock screen | [The daemons](../04-programs/daemons.md#kdos-lock) |
| `kdos-boxsock` | One tagged compositor socket per box, per compositor | [The daemons](../04-programs/daemons.md#kdos-boxsock) |
| `xdg-desktop-portal-kdos` | The portal backend | [The session](../03-architecture/session.md#the-kdos-backend) |
| `kdos-record` | Record the desktop through the ScreenCast portal; run it again to stop | [The session](../03-architecture/session.md#the-kdos-backend) |
| `kdos-shot` | Screenshots, through `grim` and `slurp`: `region`, `screen`, `qr` | [The desktop](../02-user-guide/desktop.md) |
| `kdos-updatedb` | Rebuild this user's own `plocate` index from `$HOME` | [Configuration](configuration.md) |

While `kdos-record` runs, its pid is in `$XDG_RUNTIME_DIR/kdos/screencast.pid` and the panel draws a
`•REC` lamp. `kdos-shot qr` decodes a QR code in the picture onto the clipboard and removes the
picture, so a pairing token never reaches the disk.

## Daemons

| Command | What it does | Documented in |
|---|---|---|
| `kdos-powerd` | Suspend, poweroff, reboot | [The daemons](../04-programs/daemons.md#kdos-powerd) |
| `kdos-power` | Client for the power daemon | [The daemons](../04-programs/daemons.md#kdos-powerd) |
| `kdos-mountd` | The removable-media daemon | [The daemons](../04-programs/daemons.md#kdos-mountd) |
| `kdos-mount` | Its client half | [The daemons](../04-programs/daemons.md#kdos-mountd) |
| `kdos-energyd` | The energy daemon | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-energy` | Per-application energy report | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-oomd` | The memory-pressure daemon | [The daemons](../04-programs/daemons.md#kdos-oomd) |
| `kdos-packd` | The pack daemon — the only thing that mounts a pack | [The daemons](../04-programs/daemons.md#kdos-packd) |

`kdos-mount` takes fifteen verbs: `list`, `mount`, `unmount`, `eject`, `close`, `smart`, `unlock`,
`format`, `cifs`, `krb5`, `shares`, `browse`, `disconnect`, `subscribe`, `ping`. Every block verb
takes a row number out of `list`; `browse` and `krb5` name a server, because a share is not a row in
anything. The full protocol is in [Filesystem and IPC](filesystem-and-ipc.md#sockets).

## Applications and boxes

| Command | What it does | Documented in |
|---|---|---|
| `kdos-appbox` | Launch a boxed application; generate launchers | [kdos-appbox](../04-programs/kdos-appbox.md) |
| `kdos-box` | Manage boxes: create, enter, freeze, export, profile | [kdos-appbox](../04-programs/kdos-appbox.md) |
| `kdos-boxinit` | Process 1 inside a box | [Packs and boxes](../03-architecture/packs-and-boxes.md#the-box) |
| `xdg-open` | Open a file or a link with its handler | [kdos-appbox](../04-programs/kdos-appbox.md#the-open-path) |
| `xdg-terminal-exec` | Run a command in this desktop's terminal | [kdos-appbox](../04-programs/kdos-appbox.md#the-open-path) |
| `kdos-openarchive` | Extract an archive `mc` cannot browse as a directory, beside itself | [kdos-shell](../04-programs/kdos-shell.md) |
| `kdos-pack` | Build, sign, re-stamp, index and diff packs | [Packs and boxes](../03-architecture/packs-and-boxes.md#building-a-pack) |

## Packaging

| Command | What it does | Documented in |
|---|---|---|
| `kpkg` | The package manager | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgadd` | Install a prebuilt package file | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgdel` | Remove a package | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgbuild` | Build a port without installing it | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgdepends` | Print the resolved install order | [Packaging](../03-architecture/packaging.md#kpkg) |

## Boot, console and services

| Command | What it does | Documented in |
|---|---|---|
| `kdos-splash` | The boot splash | [Boot and init](../03-architecture/boot-and-init.md#the-splash) |
| `kdos-bootctl` | A/B slot selection and confirmation, and the colours of the boot menu and text consoles | [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection) |
| `kdos-login` | Hand tty1 to `agetty`, autologging in where `login.conf` names an account | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-getty` | Load the VT font and palette, then run a getty | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-banner` | The login banner | [Boot and init](../03-architecture/boot-and-init.md#the-login-banner) |
| `ksvc` | The service supervisor | [Administration](../02-user-guide/administration.md#services) |
| `service` | The same supervisor, under the conventional name | [Administration](../02-user-guide/administration.md#services) |
| `kinstall` | The installer | [kinstall](../04-programs/kinstall.md) |

`ksvc` takes `start`, `stop`, `restart`, `status`, `enable`, `disable`, `list`, `check`,
`supervise`, `stop-supervised` and `help`. The last three are what an init script calls; the rest
are what a person types. `kdos-bootctl` takes `status`, `select`, `try`, `set-slot`, `mark-good`,
`attempts`, `active`, `crypt`, `theme [--print] ACCENT` and `palette [ACCENT]`.

## Tools and helpers

| Command | What it does | Documented in |
|---|---|---|
| `kdos-res` | The resource monitor | [kdos-res](../04-programs/kdos-res.md) |
| `kdos-term` | The terminal | [kdos-term](../04-programs/kdos-term.md) |
| `kdos-bb` | The ASCII-art demo | [kdos-bb](../04-programs/kdos-bb.md) |
| `kdos-theme` | Generate the GTK, icon and cursor themes | [Theming](../02-user-guide/theming.md#how-the-theme-is-generated) |
| `kdos-share` | Send a file to another machine over `croc` | [The kdos command](../04-programs/kdos-command.md#share) |
| `kdos-sfx` | Sound effects: `login`, `notify`, `error`, `degauss` | [The kdos command](../04-programs/kdos-command.md#the-other-names-on-this-binary) |
| `kdos-mpctl` | The music, over mpd's unix socket: `toggle`, `stop`, `next`, `prev`, `now`, `watch` | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-fetch-app` | Install an alien application from a network | [The kdos command](../04-programs/kdos-command.md#the-other-names-on-this-binary) |
| `kdos-fetch-static` | Fetch a single verified static binary | [The kdos command](../04-programs/kdos-command.md#the-other-names-on-this-binary) |

`kdos-term` takes `-e` (also spelled `--exec`), `--title`, `--app-id`, `--font`, `--size WxH`,
`--float`, `-D DIR` (also `--working-directory`), `--tty` and `--dump WxH`. Everything after `--`
is the command. `kdos-mpctl watch`
writes `$XDG_RUNTIME_DIR/kdos/nowplaying` and sleeps in mpd's `idle`.

## Privileged helpers

Two setuid binaries ship, each doing one thing that an unprivileged process cannot.

| Command | What it does | Documented in |
|---|---|---|
| `kdos-checkpass` | Check the caller's own password against `/etc/shadow` | [The security model](../03-architecture/security-model.md#kdos-checkpass) |
| `kdos-resctl` | Signal or renice a process | [The security model](../03-architecture/security-model.md#kdos-resctl) |

## Host-only tools

These run on a build machine and never ship on the target.

| Command | What it does | Documented in |
|---|---|---|
| `kdosbuild` | The build orchestrator | [The build system](../05-developer/build-system.md#kdosbuild) |
| `kdos-portup` | Check ports for newer upstream releases | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/fetch` | Download and vendor sources | [Writing ports](../05-developer/writing-ports.md#vendoring) |
| `ports/update` | Front end to the version checker | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `testing/preflight.sh` | 47 checks over the wiring, in seconds | [Testing](../05-developer/testing.md#preflightsh) |
| `testing/selftest.sh` | The library and consumer suite | [Testing](../05-developer/testing.md#selftestsh) |
| `testing/docscheck.sh` | This book: dead links, historical phrasing, the page contract | [Testing](../05-developer/testing.md) |
| `testing/vnc-shot.py` | Drive and photograph a real session | [Testing](../05-developer/testing.md#the-qemu-rig) |
| `testing/quick.sh` | Rebuild one port and patch it into a booted ISO's RAM overlay | [Testing](../05-developer/testing.md) |
| `testing/packlane.sh` | The application lane on a booted machine | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/install-to-disk.sh` | Run the installer into a disk image | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appsweep.sh` | Launch every catalogue application | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appreport.sh` | Report on what the sweep found | [Testing](../05-developer/testing.md#the-other-harnesses) |

## See also

- [The programs](../04-programs/README.md) — grouped by what they are, with the multi-name mapping
- [Configuration](configuration.md) — every setting these read
- [Filesystem and IPC](filesystem-and-ipc.md) — the paths and sockets they use
- [Glossary](glossary.md) — the vocabulary
