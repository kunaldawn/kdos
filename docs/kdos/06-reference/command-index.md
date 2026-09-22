# Command index

Every command KDOS itself installs, alphabetically, with one line and a link to the page that
documents it. Software from `ports/core/` is upstream's and is documented upstream.

This is an index, not a manual page. Many of these are the **same binary** under different names —
see [the program map](../04-programs/README.md#multi-name-binaries).

## Commands

| Command | What it does | Documented in |
|---|---|---|
| `kdos` | The front door: thirty subcommands | [The kdos command](../04-programs/kdos-command.md) |
| `kdos-appbox` | Launch a boxed application; generate launchers | [kdos-appbox](../04-programs/kdos-appbox.md) |
| `kdos-ascii` | Render a picture as characters | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-audio` | Audio devices | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-banner` | The login banner | [Boot and init](../03-architecture/boot-and-init.md#the-login-banner) |
| `kdos-bb` | The ASCII-art demo | [kdos-bb](../04-programs/kdos-bb.md) |
| `kdos-bootctl` | A/B slot selection and confirmation, and the colours of the two surfaces drawn before a session — the boot menu and the text consoles (`theme [--print] <accent>`, `palette [<accent>]`) | [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection) |
| `kdos-backup` | What is in the restic repository, and one key to add to it | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-box` | Manage boxes | [kdos-appbox](../04-programs/kdos-appbox.md) |
| `kdos-boxinit` | Process 1 inside a box | [Packs and boxes](../03-architecture/packs-and-boxes.md#the-box) |
| `kdos-boxsock` | One tagged compositor socket per box, per compositor | [The daemons](../04-programs/daemons.md#kdos-boxsock) |
| `kdos-bt` | Bluetooth | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-cal` | The calendar | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-checkpass` | Check the caller's own password. **setuid** | [The security model](../03-architecture/security-model.md#kdos-checkpass) |
| `kdos-clip` | Clipboard history | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-trash` | What was deleted, and the way back; the Trash icon opens it | [kdos-shell](../04-programs/kdos-shell.md#kdos-trash) |
| `kdos-comp` | The compositor | [kdos-comp](../04-programs/kdos-comp.md) |
| `kdos-desk` | The desktop and its icons | [kdos-shell](../04-programs/kdos-shell.md#kdos-desk) |
| `kdos-peek` | What is in a file, without starting its application | [kdos-shell](../04-programs/kdos-shell.md#kdos-peek) |
| `kdos-find` | Files by name or contents, applications and recents | [kdos-shell](../04-programs/kdos-shell.md#kdos-find) |
| `kdos-pix` | One picture, and the folder it is in | [kdos-shell](../04-programs/kdos-shell.md#kdos-pix) |
| `kdos-rec` | Record a microphone, watch the level, transcribe it. `--input hw:C,D`, `--font`, `--dump`, and the test seams `--fixture DIR`, `--meter FILE`, `--write OUT` | [kdos-shell](../04-programs/kdos-shell.md#kdos-rec) |
| `kdos-desktop` | Start a session | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-desktop-start` | Bring up services, then the compositor | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-devices` | Cameras, microphones, removable media | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-connect` | A folder on another machine, over SMB. Six fields to `kdos-mountd`'s `cifs` or `krb5` verb — `Sign in` chooses between a password and the ticket `kinit` left in the credential cache — and `Browse` asks who answered a broadcast. The password never in an argument | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-display` | Screen configuration | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-doc` | The documentation viewer | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-energy` | Per-application energy report | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-energyd` | The energy daemon | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-fetch-app`, `kdos-fetch-static` | Fetch helpers | [The kdos command](../04-programs/kdos-command.md#the-other-names-on-this-binary) |
| `kdos-getty` | Load the VT font and palette, then run a getty | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-login` | Hand tty1 to `agetty`, autologging in where `login.conf` names an account | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-keys` | The keybinding card | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-launcher` | Full-screen application search | [kdos-shell](../04-programs/kdos-shell.md#kdos-launcher) |
| `kdos-lock` | The lock screen | [The daemons](../04-programs/daemons.md#kdos-lock) |
| `kdos-menu` | Root, System and window menus | [kdos-shell](../04-programs/kdos-shell.md#kdos-menu) |
| `kdos-mount list\|mount\|unmount\|smart\|shares\|browse\|krb5\|ping\|subscribe` | The client half of the removable-media daemon. Every block verb takes a row number out of `list`; `browse` and `krb5` name a server, because a share is not a row in anything | [The daemons](../04-programs/daemons.md#kdos-mountd) |
| `kdos-mountd` | The removable-media daemon | [The daemons](../04-programs/daemons.md#kdos-mountd) |
| `kdos-net` | Networking | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-netagent` | The NetworkManager secret agent: the passphrase box the service asks for | [kdos-shell](../04-programs/kdos-shell.md#the-device-managers) |
| `kdos-notify` | The notification centre | [kdos-shell](../04-programs/kdos-shell.md#notifications) |
| `kdos-notifyd` | The notification daemon | [kdos-shell](../04-programs/kdos-shell.md#notifications) |
| `kdos-oomd` | The memory-pressure daemon | [The daemons](../04-programs/daemons.md#kdos-oomd) |
| `kdos-openwith` | Choose a handler for a file | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-osd` | Volume and brightness | [kdos-shell](../04-programs/kdos-shell.md#kdos-osd) |
| `kdos-mpctl toggle\|stop\|next\|prev\|now\|watch` | The music, over mpd's unix socket. `watch` writes `$XDG_RUNTIME_DIR/kdos/nowplaying` and sleeps in mpd's `idle` | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-pack` | Build, sign, re-stamp, index and diff packs | [Packs and boxes](../03-architecture/packs-and-boxes.md#building-a-pack) |
| `kdos-packd` | The pack daemon | [The daemons](../04-programs/daemons.md#kdos-packd) |
| `kdos-pick` | The file chooser and browser | [kdos-shell](../04-programs/kdos-shell.md#kdos-pick) |
| `kdos-power` | Client for the power daemon | [The daemons](../04-programs/daemons.md#kdos-powerd) |
| `kdos-powerd` | Suspend, poweroff, reboot | [The daemons](../04-programs/daemons.md#kdos-powerd) |
| `kdos-prompt [--input]` | Yes or no, by exit status. `--input` is a one-row box instead: the typed line on stdout, 0 for an answer and 254 for none | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-record [FILE.mkv]` | Record the desktop to a file, through the ScreenCast portal. **Run it again to stop**; while it runs, its pid is in `$XDG_RUNTIME_DIR/kdos/screencast.pid` and the panel draws a `•REC` lamp | [The session](../03-architecture/session.md#the-kdos-backend) |
| `kdos-res` | The resource monitor | [kdos-res](../04-programs/kdos-res.md) |
| `kdos-term` | The terminal: `-e`, `--title`, `--font`, `-D DIR`, `--tty`, `--dump WxH` | [kdos-term](../04-programs/kdos-term.md) |
| `kdos-resctl` | Signal or renice a process. **setuid** | [The security model](../03-architecture/security-model.md#kdos-resctl) |
| `kdos-run` | The run box | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-saver` | Attract mode between idle and lock | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-settings` | Settings | [kdos-shell](../04-programs/kdos-shell.md#kdos-settings) |
| `kdos-share [--clipboard] [FILE\|file://URI]...` | A file to another machine over `croc`: one code word, **this network only**, no account. With nothing named, `kdos-pick` asks. What the Share verb resolves | [The kdos command](../04-programs/kdos-command.md#share) |
| `kdos-sfx` | Sound effects | [The kdos command](../04-programs/kdos-command.md#the-other-names-on-this-binary) |
| `kdos-shell` | The panel | [kdos-shell](../04-programs/kdos-shell.md#the-panel) |
| `kdos-shot [region\|screen\|qr]` | Screenshots, through `grim` and `slurp`. `qr` decodes a QR code in the picture through `zbarimg` onto the clipboard and removes the picture, so a pairing token never reaches the disk | [The desktop](../02-user-guide/desktop.md) |
| `kdos-slit` | The dockapp column | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-splash` | The boot splash | [Boot and init](../03-architecture/boot-and-init.md#the-splash) |
| `kdos-start` | The Start menu | [kdos-shell](../04-programs/kdos-shell.md#kdos-start) |
| `kdos-status` | The overflow popup | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-teams` | The window list | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos panel toggle` | Put the panel away and bring it back — what `Super+Shift+space` runs | [kdos-shell](../04-programs/kdos-shell.md#autohide) |
| `kdos-contacts` | The address book, `Super+Ctrl+b`. Type a name, `Enter` copies the address or the number | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-style` | How the screen looks: the accent on one page, the screen's font on the other. `--page accent\|font` opens either | [kdos-shell](../04-programs/kdos-shell.md#the-small-surfaces) |
| `kdos-theme` | Generate the GTK, icon and cursor themes | [Theming](../02-user-guide/theming.md#how-the-theme-is-generated) |
| `kdos-tip` | Tooltips | [kdos-shell](../04-programs/kdos-shell.md#tooltips) |
| `kdos-traymenu SERVICE PATH` | A tray item's own `com.canonical.dbusmenu` tree, drawn as cells. `--name`, `--at`, `--at-bottom`, and the test seams `--open ID`, `--pick ID`, `--dump` | [kdos-shell](../04-programs/kdos-shell.md#the-tray) |
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
| `xdg-open` | Open a file or a link with its handler — a name on `kdos-appbox` | [kdos-appbox](../04-programs/kdos-appbox.md#the-open-path) |
| `xdg-terminal-exec` | Run a command in this desktop's terminal, for a caller that only knows the standard name | [kdos-appbox](../04-programs/kdos-appbox.md#the-open-path) |

Plus **one shim per installed application**, named after the application and pointing at
`kdos-appbox`.

## `kdos` subcommands

| Subcommand | Does |
|---|---|
| `kdos help` | Every command, grouped by which question it answers |
| `kdos theme` | Switch accent, apply a style, audit the palette |
| `kdos settings [page]` | The control centre, or one of its nine pages |
| `kdos menu summon\|toggle [route]` | Open the menu on a named place; a route is what a script holds instead of a chord |
| `kdos status` | What this machine is and what it is running |
| `kdos doctor` | Check the things that actually break here |
| `kdos app` | Applications: list, search, info, groups, install, launch, remove, export, import. `install --pending` builds what the installer chose. `kdos app tui add\|rm\|ls` makes a terminal program an application — a `.desktop` in `~/.local/share/applications` with `Terminal=true`, `X-KDOS-TUI` as the marker `rm` checks, and optional `X-KDOS-Float` and `X-KDOS-Size` | [kdos-command](../04-programs/kdos-command.md#app) |
| `kdos version` | Release, commit, and whether that tree was clean |
| `kdos why` / `kdos explain` | Why something is the way it is |
| `kdos sandbox` | What a box may do |
| `kdos appid` | Do launcher identifiers match what windows present |
| `kdos restarts` | Which supervised services have been restarting |
| `kdos stutter` | Why a frame was late, and who was busy |
| `kdos march` | Measure per-machine optimisation, and the ledger |
| `kdos rebuild` | Rebuild the image from the sources on the medium |
| `kdos clone` | Copy this medium to another device |
| `kdos persist` | Report the live session's persistence store |
| `kdos persist create [<dev>]` | Make one in the free space after the image |
| `kdos cve` | Which pins carry known vulnerabilities, offline |
| `kdos trash` | The freedesktop trash |
| `kdos places [add DIR]` | The places column the desktop shows, and the way to keep one from a prompt | [kdos-command](../04-programs/kdos-command.md#places) |
| `kdos thumb <file>` | A thumbnail in the shared freedesktop cache — also `--path` and `--ppm` | [kdos-command](../04-programs/kdos-command.md#thumb) |
| `kdos speech list\|get NAME\|where\|remove NAME` | The transcription model, which the image does not carry. `get` checks a sha256 and writes to `$XDG_DATA_HOME/whisper.cpp/models`, where `kdos-rec` looks | [configuration](configuration.md) |
| `kdos share [--clipboard] [FILE…]` | The same program as `kdos-share`, under the spelling a person types | [kdos-command](../04-programs/kdos-command.md#share) |
| `kdos remind in 20m\|at 15:30\|tomorrow 9 TEXT` | A toast, later — a row in J.13's per-user table, delivered **once**. Also `ls`, `clear`, `--ask` (the one-row prompt a chord opens) and `fire ID` (what the timer runs) | [kdos-command](../04-programs/kdos-command.md#remind) |
| `kdos notify <summary> [body]` | Raise a toast. `--time` and `--battery` compute their own; `--dismiss`, `--dismiss-all`, `--raise` and `--dnd` are one line down `kdos-notifyd`'s socket | [kdos-command](../04-programs/kdos-command.md#notify) |
| `kdos-openarchive ARCHIVE` | Extract an archive `mc` cannot browse as a directory, beside itself | [kdos-shell](../04-programs/kdos-shell.md) |
| `kdos hey` | Ask the compositor about windows, outputs and boxes |
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
