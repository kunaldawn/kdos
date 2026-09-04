# Command index

Every command KDOS itself installs, alphabetically, with one line and a link to the page that
documents it. Software from `ports/core/` is upstream's and is documented upstream.

This is an index, not a manual page. Many of these are the **same binary** under different names —
see [the program map](../04-programs/README.md#multi-name-binaries).

## Commands

| Command | What it does | Documented in |
|---|---|---|
| `kdos` | The front door: nineteen subcommands | [The kdos command](../04-programs/kdos-command.md) |
| `kdos-appbox` | Launch a boxed application; generate launchers | [kdos-appbox](../04-programs/kdos-appbox.md) |
| `kdos-ascii` | Render a picture as characters | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-audio` | Audio devices | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-banner` | The login banner | [Boot and init](../03-architecture/boot-and-init.md#the-login-banner) |
| `kdos-bb` | The ASCII-art demo | [kdos-bb](../04-programs/kdos-bb.md) |
| `kdos-bootctl` | A/B slot selection and confirmation | [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection) |
| `kdos-box` | Manage boxes | [kdos-appbox](../04-programs/kdos-appbox.md) |
| `kdos-boxinit` | Process 1 inside a box | [Packs and boxes](../03-architecture/packs-and-boxes.md#the-box) |
| `kdos-boxsock` | One tagged compositor socket per box | [The daemons](../04-programs/daemons.md#kdos-boxsock) |
| `kdos-bt` | Bluetooth | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-cal` | The calendar | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-checkpass` | Check the caller's own password. **setuid** | [The security model](../03-architecture/security-model.md#kdos-checkpass) |
| `kdos-clip` | Clipboard history | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-comp` | The compositor | [kdos-comp](../04-programs/kdos-comp.md) |
| `kdos-desk` | The desktop and its icons | [kdos-shell](../04-programs/kdos-shell.md#kdos-desk) |
| `kdos-desktop` | Start a session | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-desktop-start` | Bring up services, then the compositor | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-devices` | Cameras, microphones, removable media | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-display` | Screen configuration | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-doc` | The documentation viewer | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-energy` | Per-application energy report | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-energyd` | The energy daemon | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-fetch-app`, `kdos-fetch-static` | Fetch helpers | [The kdos command](../04-programs/kdos-command.md#the-other-names-on-this-binary) |
| `kdos-getty` | Load the console font and palette, then run a getty | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-keys` | The keybinding card | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-launcher` | Full-screen application search | [kdos-shell](../04-programs/kdos-shell.md#kdos-launcher) |
| `kdos-lock` | The lock screen | [The daemons](../04-programs/daemons.md#kdos-lock) |
| `kdos-menu` | Root, System and window menus | [kdos-shell](../04-programs/kdos-shell.md#kdos-menu) |
| `kdos-mountd` | The removable-media daemon | [The daemons](../04-programs/daemons.md#kdos-mountd) |
| `kdos-net` | Networking | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-notify` | The notification centre | [kdos-shell](../04-programs/kdos-shell.md#notifications) |
| `kdos-notifyd` | The notification daemon | [kdos-shell](../04-programs/kdos-shell.md#notifications) |
| `kdos-oomd` | The memory-pressure daemon | [The daemons](../04-programs/daemons.md#kdos-oomd) |
| `kdos-openwith` | Choose a handler for a file | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-osd` | Volume and brightness | [kdos-shell](../04-programs/kdos-shell.md#kdos-osd) |
| `kdos-pack` | Build, sign, index and diff packs | [Packs and boxes](../03-architecture/packs-and-boxes.md#building-a-pack) |
| `kdos-packd` | The pack daemon | [The daemons](../04-programs/daemons.md#kdos-packd) |
| `kdos-pick` | The file chooser and browser | [kdos-shell](../04-programs/kdos-shell.md#kdos-pick) |
| `kdos-power` | Client for the power daemon | [The daemons](../04-programs/daemons.md#kdos-powerd) |
| `kdos-powerd` | Suspend, poweroff, reboot | [The daemons](../04-programs/daemons.md#kdos-powerd) |
| `kdos-prompt` | Yes or no, by exit status | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-con` | The console session server | [kdos-con](../04-programs/kdos-con.md) |
| `kdos-con-login` | The tty1 login: greeter or autologin | [kdos-con](../04-programs/kdos-con.md#the-login) |
| `kdos-con-start` | Bring up the console session | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-grid` | A console session and a view, in one command | [kdos-con](../04-programs/kdos-con.md#four-names-one-binary) |
| `kdos-res` | The resource monitor | [kdos-res](../04-programs/kdos-res.md) |
| `kdos-term` | The terminal: `-e`, `--title`, `--font`, `-D DIR`, `--tty`, `--dump WxH` | [kdos-term](../04-programs/kdos-term.md) |
| `kdos-cage` | One application full screen, or embedded: `-d`, `-D`, `-m extend\|last`, `-s`, `-v`, `--embed WxH` | [kdos-cage](../04-programs/kdos-cage.md) |
| `kdos con run` | Run a graphical program on the console desktop. Prints the terminal it was given, or `0` for a window | [kdos-con](../04-programs/kdos-con.md#reaching-it) |
| `kdos-view --cast` | Rasterise the console session into a PipeWire stream. Prints the node id and the stream's pixel size | [kdos-con](../04-programs/kdos-con.md#recording-it) |
| `kdos-resctl` | Signal or renice a process. **setuid** | [The security model](../03-architecture/security-model.md#kdos-resctl) |
| `kdos-run` | The run box | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-saver` | Attract mode between idle and lock | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-settings` | Settings | [kdos-shell](../04-programs/kdos-shell.md#kdos-settings) |
| `kdos-sfx` | Sound effects | [The kdos command](../04-programs/kdos-command.md#the-other-names-on-this-binary) |
| `kdos-shell` | The panel | [kdos-shell](../04-programs/kdos-shell.md#the-panel) |
| `kdos-shot` | Screenshots. On the console, cells rather than an image | [The desktop](../02-user-guide/desktop.md) |
| `kdos-slit` | The dockapp column | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-view` | A display for a console session: `--kms`, `--kms-only`, `--tty`, `--dump`, `--cast` | [kdos-con](../04-programs/kdos-con.md#the-split-that-everything-else-falls-out-of) |
| `kdos-splash` | The boot splash | [Boot and init](../03-architecture/boot-and-init.md#the-splash) |
| `kdos-start` | The Start menu | [kdos-shell](../04-programs/kdos-shell.md#kdos-start) |
| `kdos-status` | The overflow popup | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-teams` | The window list | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-theme` | Generate the GTK, icon and cursor themes | [Theming](../02-user-guide/theming.md#how-the-theme-is-generated) |
| `kdos-tip` | Tooltips | [kdos-shell](../04-programs/kdos-shell.md#tooltips) |
| `kdos-ime` | The input-method candidate window, as cells | [kdos-shell](../04-programs/kdos-shell.md#the-candidate-window) |
| `kinstall` | The installer | [kinstall](../04-programs/kinstall.md) |
| `kpkg` | The package manager | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgadd` | Install a prebuilt package file | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgbuild` | Build a port without installing it | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgdel` | Remove a package | [Packaging](../03-architecture/packaging.md#kpkg) |
| `kpkgdepends` | Print the resolved install order | [Packaging](../03-architecture/packaging.md#kpkg) |
| `ksvc` | The service supervisor | [Administration](../02-user-guide/administration.md#services) |
| `service` | The same supervisor, conventional name | [Administration](../02-user-guide/administration.md#services) |
| `xdg-desktop-portal-kdos` | The portal backend | [The session](../03-architecture/session.md#the-kdos-backend) |

Plus **one shim per installed application**, named after the application and pointing at
`kdos-appbox`.

## `kdos` subcommands

| Subcommand | Does |
|---|---|
| `kdos help` | Every command, grouped by which question it answers |
| `kdos theme` | Switch accent, apply a style, audit the palette |
| `kdos status` | What this machine is and what it is running |
| `kdos doctor` | Check the things that actually break here |
| `kdos app` | Applications: list, search, show, install, launch, remove, rollback, update, sources |
| `kdos version` | Release, commit, and whether that tree was clean |
| `kdos why` / `kdos explain` | Why something is the way it is |
| `kdos sandbox` | What a box may do |
| `kdos appid` | Do launcher identifiers match what windows present |
| `kdos restarts` | Which supervised services have been restarting |
| `kdos stutter` | Why a frame was late, and who was busy |
| `kdos march` | Measure per-machine optimisation, and the ledger |
| `kdos rebuild` | Rebuild the image from the sources on the medium |
| `kdos clone` | Copy this medium to another device |
| `kdos cve` | Which pins carry known vulnerabilities, offline |
| `kdos trash` | The freedesktop trash |
| `kdos hey` | Ask the compositor about windows, outputs and boxes |
| `kdos con` | Console sessions: `ls`, `new`, `attach`, `detach`, `kill` (asks the session to end, and it drains), `forward` |
| `kdos oracle` | An aphorism |
| `kdos update` | Orchestrate a system update |

## Host-only tools

These run on a build machine and **never ship on the target**.

| Command | Does | Documented in |
|---|---|---|
| `kdosbuild` | The build orchestrator | [The build system](../05-developer/build-system.md#kdosbuild) |
| `kdos-portup` | Check ports for newer upstream releases | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/fetch` | Download and vendor sources | [Writing ports](../05-developer/writing-ports.md#vendoring) |
| `ports/update` | Front end to the version checker | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/sources` | Publish and fetch release assets | [Writing ports](../05-developer/writing-ports.md#publishing-sources) |
| `ports/appbox/bake` | Bake the application catalogue | [Packs and boxes](../03-architecture/packs-and-boxes.md#baking-the-catalogue) |
| `testing/preflight.sh` | Check the wiring | [Testing](../05-developer/testing.md#preflightsh) |
| `testing/selftest.sh` | The library and consumer suite | [Testing](../05-developer/testing.md#selftestsh) |
| `testing/vnc-shot.py` | Drive and photograph a real session | [Testing](../05-developer/testing.md#the-qemu-rig) |
| `testing/packlane.sh` | The application lane on a booted machine | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/install-to-disk.sh` | Run the installer into a disk image | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appsweep.sh`, `appreport.sh` | Launch every catalogue application and report | [Testing](../05-developer/testing.md#the-other-harnesses) |

## See also

- [The programs](../04-programs/README.md) — grouped by what they are, with the multi-name mapping
- [Configuration](configuration.md) — every setting these read
- [Filesystem and IPC](filesystem-and-ipc.md) — the paths and sockets they use
- [Glossary](glossary.md) — the vocabulary
