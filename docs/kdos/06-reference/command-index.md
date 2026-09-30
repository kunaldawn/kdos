# Command index

This chapter lists every command that the KDOS source tree itself installs on the target system,
with a one-line description, the directory it is installed in, and a link to the chapter that
documents it in full. It closes with the scripts and `make` targets a contributor runs on the
build machine. Use it to find out what a `kdos-*` name is, where it lives, or where to read more;
it is a lookup table, not an explanation, and assumes you have met the vocabulary in
[The programs](../04-programs/README.md) or the [glossary](glossary.md).

Software built from `ports/core/` (bash, foot, podman, mpv and the rest) is upstream's, and
upstream's manual pages describe it; [the ports catalogue](ports-catalogue.md) lists every port by
shelf, with its phase. This index lists only what this repository writes: the programs under `src/`,
the scripts under `fs/`, and the handful of wrapper scripts that a port recipe writes into its
package. Each entry below matches the source that installs it.

## Where the commands live

KDOS installs its own commands into six directories. Compiled programs from `src/` go in
`/usr/bin`, and the root daemons go in `/usr/sbin`. The session scripts from `fs/`, `kdos` and the
other session-side names of the `ksvc` binary, and the `kdos-appbox` names go in `/usr/local/bin`,
which precedes `/usr/bin` on the `PATH` that `/etc/profile` sets. `kdos-bootctl` is a name of the
same `ksvc` binary but is in `/usr/bin`, because the initramfs build copies it by path.

| Directory | What is there |
|---|---|
| `/usr/bin` | `kdos-shell` and its 54 other names, `kdos-comp`, `kdos-term`, `kdos-res`, `kdos-lock`, `kdos-record`, `kdos-boxsock`, `kdos-pack`, `kdos-splash`, `kdos-theme`, `kdos-bb`, `kinstall`, `kpkg` and its four other names, `kdos-bootctl`, the daemon clients, the two setuid helpers, and the wrapper scripts ports write |
| `/usr/sbin` | The five root daemons, and `ksvc` with its second name `service` |
| `/usr/local/bin` | `kdos` and the other session-side names of the `ksvc` binary, `kdos-appbox` with `kdos-box`, `xdg-open` and one shim per boxed application, and the session scripts from `fs/` |
| `/usr/local/sbin` | `kdos-getty`, which `/etc/inittab` runs on `tty1` and `tty2`, and `kdos-login`, which it runs on `tty1` |
| `/usr/lib`, `/usr/libexec/kdos` | `xdg-desktop-portal-kdos`, started by D-Bus; `kdos-boxinit`, which runs inside a box |

## Binaries with several names

Many of the commands below are one binary installed under several names. The binary reads the
name it was started by (the basename of `argv[0]`) and behaves as that command, so no shell
wrapper sits anywhere in the chain. `kdos-shell`, `ksvc` and `kpkg` also accept the command name
as the first argument when they are started under a name their table does not hold, such as the
build output's own name: `kdos-tools service list` behaves as `service list`. Under a listed name,
including `kdos-shell` and `kpkg`, the first argument is an ordinary argument.

| Binary | Installed as | Names it answers to | Where the name table lives |
|---|---|---|---|
| `kdos-shell` | `/usr/bin/kdos-shell` | 55 | `src/desktop/kdos-shell/main.c` |
| `ksvc` (the `kdos-tools` port) | `/usr/sbin/ksvc` | 12 | `src/system/kdos-tools/main.c` |
| `kpkg` | `/usr/bin/kpkg` | 5 | `src/system/kdos-kpkg/main.c` |
| `kdos-appbox` | `/usr/local/bin/kdos-appbox` | 3 fixed, plus one per application | `src/system/kdos-appbox/main.c` |
| `kdos-powerd` | `/usr/sbin/kdos-powerd` | 2: `kdos-powerd` and its client `kdos-power` | `src/daemons/kdos-powerd/build.sh` (the symlink) |
| `kdos-mountd` | `/usr/sbin/kdos-mountd` | 2: `kdos-mountd` and its client `kdos-mount` | `src/daemons/kdos-mountd/build.sh` (the symlink) |
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
[the program map](../04-programs/README.md#binaries-that-answer-to-several-names).

## The `kdos` command

`kdos` is the single entry point to the system's own tools. It has 30 subcommands, plus `help`,
which prints them grouped by the question each answers. It is installed as
`/usr/local/bin/kdos`, a name of the `ksvc` binary. Each subcommand below links to its section of
[The kdos command](../04-programs/kdos-command.md).

| Subcommand | Does |
|---|---|
| [`kdos help`](../04-programs/kdos-command.md#kdos-help) | Every command, grouped by the question it answers; also `-h` and `--help` |
| [`kdos version`](../04-programs/kdos-command.md#kdos-version) | Kernel, C library, userland, session and accent; also `-V` |
| [`kdos status`](../04-programs/kdos-command.md#kdos-status) | What this machine is and what it is running |
| [`kdos doctor`](../04-programs/kdos-command.md#kdos-doctor) | Check for the faults known to occur on KDOS, each reported as `ok`, `warn` or `skip`; `--json`, `--cve` |
| [`kdos theme [NAME]`](../04-programs/kdos-command.md#kdos-theme) | Switch the accent (`next`, `prev` and `list` too); `style FILE` applies a style file, `--audit` checks the generated colours, `--preview ACCENT` previews one |
| [`kdos settings [PAGE]`](../04-programs/kdos-command.md#kdos-settings) | The control centre, or one of its nine pages: `appearance`, `panel`, `desktop`, `hardware`, `session`, `input`, `apps`, `boxes`, `system` |
| [`kdos menu summon ROUTE`](../04-programs/kdos-command.md#kdos-menu) | Open the palette on a named place; `toggle [ROUTE]` opens or closes it |
| [`kdos panel toggle`](../04-programs/kdos-command.md#kdos-panel) | Put the panel away and bring it back |
| [`kdos toggle [NAME]`](../04-programs/kdos-command.md#kdos-toggle) | `stay-awake`, `night-light`, `dnd`: list, flip or set |
| [`kdos notify TEXT [BODY]`](../04-programs/kdos-command.md#kdos-notify) | Raise a toast; `--time`, `--battery`, `--dismiss`, `--dismiss-all`, `--raise`, `--dnd` |
| [`kdos remind TIME TEXT`](../04-programs/kdos-command.md#kdos-remind) | A toast later, delivered once (`in 20m`, `at 15:30`, `tomorrow 9`); also `ls`, `clear`, `--ask`, `fire ID` |
| [`kdos app`](../04-programs/kdos-command.md#kdos-app) | Applications: `list`, `search`, `info`, `groups`, `install`, `launch`, `remove`, `export`, `import`, and `tui add`/`rm`/`ls` for terminal programs |
| [`kdos trash FILE…`](../04-programs/kdos-command.md#kdos-trash) | The freedesktop trash from a prompt; `--list`, `--restore NAME`, `--rm NAME`, `--empty [-y]` |
| [`kdos places [add DIR [NAME]]`](../04-programs/kdos-command.md#kdos-places) | The places column the desktop shows |
| [`kdos thumb FILE…`](../04-programs/kdos-command.md#kdos-thumb) | A thumbnail in the shared freedesktop cache; `--path FILE`, `--ppm FILE OUT` |
| [`kdos speech`](../04-programs/kdos-command.md#kdos-speech) | The transcription models: `list`, `get NAME`, `where`, `remove NAME` |
| [`kdos share [FILE…]`](../04-programs/kdos-command.md#kdos-share) | Send files to another machine over `croc`; `--clipboard` sends the clipboard |
| [`kdos hey`](../04-programs/kdos-command.md#kdos-hey) | Ask the compositor: `list`, `outputs`, `boxes` (each with `--json`), and `run ACTION ID` |
| [`kdos why PATH\|PORT`](../04-programs/kdos-command.md#kdos-why-and-kdos-explain) | What provides this, and the recorded reason it is the way it is |
| [`kdos explain [TOPIC]`](../04-programs/kdos-command.md#kdos-why-and-kdos-explain) | The recorded reasons, browsable |
| [`kdos oracle`](../04-programs/kdos-command.md#kdos-oracle) | One recorded lesson, picked for today |
| [`kdos sandbox [PROFILE] -- CMD`](../04-programs/kdos-command.md#kdos-sandbox) | Run a native program under Landlock; `--read`, `--write`, `--no-network`, `--tcp`, `--explain` |
| [`kdos appid`](../04-programs/kdos-command.md#kdos-appid) | Whether launcher identifiers match the identifiers windows present |
| [`kdos restarts`](../04-programs/kdos-command.md#kdos-restarts) | Which running processes still use code an upgrade replaced or removed |
| [`kdos stutter`](../04-programs/kdos-command.md#kdos-stutter) | Why a frame was late, and which process was busy |
| [`kdos march`](../04-programs/kdos-command.md#kdos-march) | Measure per-machine optimisation (`probe`, `run PORT…`) and read the recorded verdicts (`report`) |
| [`kdos cve`](../04-programs/kdos-command.md#kdos-cve) | Which pinned versions carry known vulnerabilities, from an offline database |
| [`kdos update`](../04-programs/kdos-command.md#kdos-update) | `check` (`--json` writes the panel's badge), `apply`, and `theme` for the per-user artwork |
| [`kdos rebuild WORKDIR`](../04-programs/kdos-command.md#kdos-rebuild) | Rebuild the image from the sources on the medium; `--dry-run`, `--iso-only` |
| [`kdos clone [DEV]`](../04-programs/kdos-command.md#kdos-clone) | Copy this medium to another device, verified by read-back |
| [`kdos persist`](../04-programs/kdos-command.md#kdos-persist) | Report the live session's persistence store; `create [DEV]` makes one |

## Desktop surfaces

A **surface** is one window or popup of the desktop. All 55 names below are the `kdos-shell`
binary in `/usr/bin` (the binary itself and 54 symlinks to it), and they reach 54 distinct
programs: `kdos-launcher` and `kdos-palette` are one search program that shows only applications
when started as `kdos-launcher` or with `--apps`. All but two of the 54 open a surface of their
own; `kdos-mediad` raises its toasts through `kdos-notifyd`, and `kdos-ascii` is a filter that
writes text to standard output. Every surface is drawn as a grid of character
cells and handed to the compositor as an ordinary Wayland surface; the frame around a window is
drawn by the compositor.

| Command | What it does | Documented in |
|---|---|---|
| `kdos-shell` | The panel | [kdos-shell](../04-programs/kdos-shell.md#the-panel) |
| `kdos-start` | The Start menu | [kdos-shell](../04-programs/kdos-shell.md#kdos-start) |
| `kdos-launcher` | The palette showing applications only | [kdos-shell](../04-programs/kdos-shell.md#kdos-palette-and-kdos-launcher) |
| `kdos-palette` | One search over windows, applications, routes, settings pages, files and key chords (keyboard shortcuts) | [kdos-shell](../04-programs/kdos-shell.md#kdos-palette-and-kdos-launcher) |
| `kdos-menu` | The applications, places, system and window menus | [kdos-shell](../04-programs/kdos-shell.md#kdos-menu) |
| `kdos-desk` | The desktop and its icons | [kdos-shell](../04-programs/kdos-shell.md#kdos-desk) |
| `kdos-pick` | The file chooser and browser | [kdos-shell](../04-programs/kdos-shell.md#kdos-pick) |
| `kdos-peek` | What is in a file, without starting its application | [kdos-shell](../04-programs/kdos-shell.md#kdos-peek) |
| `kdos-find` | Files by name or contents, applications and recent files | [kdos-shell](../04-programs/kdos-shell.md#kdos-find) |
| `kdos-pix` | One picture, and the folder it is in | [kdos-shell](../04-programs/kdos-shell.md#kdos-pix) |
| `kdos-rec` | Record a microphone, watch the level, transcribe it | [kdos-shell](../04-programs/kdos-shell.md#kdos-rec) |
| `kdos-ascii` | Render a picture as characters | [kdos-shell](../04-programs/kdos-shell.md#the-small-dialogs) |
| `kdos-run` | The run box | [kdos-shell](../04-programs/kdos-shell.md#the-small-dialogs) |
| `kdos-prompt` | Yes or no, answered by exit status; `--input` asks for a line instead | [kdos-shell](../04-programs/kdos-shell.md#the-small-dialogs) |
| `kdos-notify` | The notification centre | [kdos-shell](../04-programs/kdos-shell.md#notifications) |
| `kdos-notifyd` | The notification daemon | [kdos-shell](../04-programs/kdos-shell.md#notifications) |
| `kdos-mediad` | Removable-media toasts, with Open and Eject | [kdos-shell](../04-programs/kdos-shell.md#background-services) |
| `kdos-netagent` | The NetworkManager secret agent | [kdos-shell](../04-programs/kdos-shell.md#kdos-netagent) |
| `kdos-osd` | The volume, microphone and brightness bezel | [kdos-shell](../04-programs/kdos-shell.md#kdos-osd) |
| `kdos-cal` | The calendar | [kdos-shell](../04-programs/kdos-shell.md#the-desk-accessories) |
| `kdos-display` | Screen configuration | [kdos-shell](../04-programs/kdos-shell.md#the-system-surfaces) |
| `kdos-keys` | The keybinding card | [kdos-shell](../04-programs/kdos-shell.md#kdos-keys) |
| `kdos-teams` | The window list | [kdos-shell](../04-programs/kdos-shell.md#the-desk-accessories) |
| `kdos-saver` | The screen saver, started by `Super+Shift+L` or by hand; the idle sequence does not start it | [kdos-shell](../04-programs/kdos-shell.md#kdos-saver) |
| `kdos-about` | What this machine is | [kdos-shell](../04-programs/kdos-shell.md#the-desk-accessories) |
| `kdos-style` | The accent on one page, the font on the other | [kdos-shell](../04-programs/kdos-shell.md#kdos-style) |
| `kdos-calc` | The calculator, through `qalc` | [kdos-shell](../04-programs/kdos-shell.md#the-desk-accessories) |
| `kdos-chars` | The character map | [kdos-shell](../04-programs/kdos-shell.md#the-desk-accessories) |
| `kdos-connect` | A folder on another machine, over SMB | [kdos-shell](../04-programs/kdos-shell.md#kdos-connect) |
| `kdos-traymenu` | A tray item's own `com.canonical.dbusmenu` menu, as cells | [kdos-shell](../04-programs/kdos-shell.md#the-tray) |
| `kdos-contacts` | The address book, through `khard` | [kdos-shell](../04-programs/kdos-shell.md#the-desk-accessories) |
| `kdos-disks` | Disks: mount, unlock, SMART, partition, write an image, erase | [kdos-shell](../04-programs/kdos-shell.md#kdos-disks) |
| `kdos-print` | Printers: what is set up, what is on the network | [kdos-shell](../04-programs/kdos-shell.md#kdos-print) |
| `kdos-time` | The time zone, the clock, and whether the clock is right | [kdos-shell](../04-programs/kdos-shell.md#the-system-surfaces) |
| `kdos-users` | The accounts, and which one `tty1` logs in automatically | [kdos-shell](../04-programs/kdos-shell.md#the-system-surfaces) |
| `kdos-update` | What is behind, what is vulnerable, which root slot is live | [kdos-shell](../04-programs/kdos-shell.md#the-system-surfaces) |
| `kdos-store` | What this machine can build, and one tick to build it | [kdos-shell](../04-programs/kdos-shell.md#kdos-store) |
| `kdos-firewall` | Which services answer the network | [kdos-shell](../04-programs/kdos-shell.md#the-system-surfaces) |
| `kdos-backup` | What is in the restic repository, one key to add to it, and a restore view | [kdos-shell](../04-programs/kdos-shell.md#the-system-surfaces) |
| `kdos-burn` | A folder or an image onto a CD, DVD or Blu-ray, and the disc checked | [kdos-shell](../04-programs/kdos-shell.md#the-system-surfaces) |
| `kdos-verify` | Files against a checksum list or a par2 set, and par2's repair | [kdos-shell](../04-programs/kdos-shell.md#the-system-surfaces) |
| `kdos-note` | The scratch pad | [kdos-shell](../04-programs/kdos-shell.md#the-desk-accessories) |
| `kdos-slit` | The slit: a narrow column of small status gadgets (dockapps) at the right edge of each screen, off unless `comp.conf` sets `slit = yes` | [kdos-shell](../04-programs/kdos-shell.md#the-small-dialogs) |
| `kdos-doc` | The documentation viewer | [kdos-shell](../04-programs/kdos-shell.md#kdos-doc) |
| `kdos-settings` | The control centre | [kdos-shell](../04-programs/kdos-shell.md#kdos-settings) |
| `kdos-openwith` | Choose a handler for a file | [kdos-shell](../04-programs/kdos-shell.md#the-small-dialogs) |
| `kdos-audio` | Audio devices | [kdos-shell](../04-programs/kdos-shell.md#kdos-audio) |
| `kdos-net` | Networking | [kdos-shell](../04-programs/kdos-shell.md#kdos-net) |
| `kdos-bt` | Bluetooth | [kdos-shell](../04-programs/kdos-shell.md#kdos-bt) |
| `kdos-devices` | Cameras, microphones, removable media | [kdos-shell](../04-programs/kdos-shell.md#kdos-devices) |
| `kdos-clip` | Clipboard history: with no argument the daemon, with `--pick` the picker | [kdos-shell](../04-programs/kdos-shell.md#the-desk-accessories) |
| `kdos-status` | The popup behind the panel's overflow chevron | [kdos-shell](../04-programs/kdos-shell.md#the-overflow-chevron) |
| `kdos-tip` | Tooltips | [kdos-shell](../04-programs/kdos-shell.md#tooltips) |
| `kdos-ime` | The input-method candidate window, as cells | [kdos-shell](../04-programs/kdos-shell.md#kdos-ime) |
| `kdos-trash` | What was deleted, and the way back | [kdos-shell](../04-programs/kdos-shell.md#kdos-trash) |

### Flags worth knowing

The full option list of every surface is in
[Command lines](../04-programs/kdos-shell.md#command-lines). The options most often typed by hand
are these:

| Command | Flags |
|---|---|
| `kdos-prompt` | `--message TEXT`, `--yes LABEL`, `--no LABEL`; exits 0 for yes, 1 for no and 254 when cancelled. `--input` makes it a one-row box: the typed line on standard output with exit 0, or exit 254 and nothing printed |
| `kdos-palette` | `--apps` shows applications only, as `kdos-launcher` does; `--route NAME` opens with a route already typed |
| `kdos-menu` | `applications`, `places` or `system`; `--windows APP_ID` and `--winmenu APP_ID` list one application's windows, the second with the window controls |
| `kdos-pick` | `--save`, `--directory`, `--multiple`, `--browse [DIR]`, `--dir DIR`, `--name NAME`, `--filter 'Label:*.png *.jpg'` |
| `kdos-settings` | `--page NAME`, one of the nine page names above |
| `kdos-style` | `--page accent` or `--page font` opens either page directly |
| `kdos-display` | `--list` prints the outputs and exits; `--apply` re-applies `~/.config/kdos/displays.conf` with no window |
| `kdos-rec` | `--input hw:C,D` picks the capture device; `--fixture DIR`, `--meter FILE` and `--write OUT` are test inputs |
| `kdos-traymenu` | `SERVICE PATH`; also `--name`, `--at X Y`, `--at-bottom X Y`, and the test inputs `--open ID` and `--pick ID` |

Every surface except `kdos-ascii`, `kdos-ime`, `kdos-mediad` and `kdos-netagent` also takes
`--dump` to print one frame as text with no display; see
[Rendering one frame with no display](../04-programs/kdos-shell.md#rendering-one-frame-with-no-display).

## The compositor and its session

| Command | Installed in | What it does | Documented in |
|---|---|---|---|
| `kdos-comp` | `/usr/bin` | The compositor, a fork of labwc | [kdos-comp](../04-programs/kdos-comp.md) |
| `kdos-desktop` | `/usr/local/bin` | Start a session from a tty | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-desktop-start` | `/usr/local/bin` | Bring up PipeWire, WirePlumber and the PulseAudio shim, run the compositor in the foreground, and start the portals once its socket exists | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-session-save` | `/usr/local/bin` | Write what is running to `$XDG_STATE_HOME/kdos/session`, so the next login can restore it | [The session](../03-architecture/session.md#starting-a-session) |
| `kdos-lock` | `/usr/bin` | The lock screen, through `ext-session-lock-v1` | [The daemons](../04-programs/daemons.md#kdos-lock) |
| `kdos-boxsock` | `/usr/bin` | One tagged compositor socket per box, so the compositor knows which box a window came from; `kdos-boxsock BOX [INSTANCE]` | [The daemons](../04-programs/daemons.md#kdos-boxsock) |
| `xdg-desktop-portal-kdos` | `/usr/lib` | The portal backend for the FileChooser, Settings, AppChooser and Access interfaces; started by D-Bus | [The session](../03-architecture/session.md#the-kdos-backend) |
| `kdos-record` | `/usr/bin` | Record the desktop through the ScreenCast portal; run it again to stop | [The session](../03-architecture/session.md#recording) |
| `kdos-shot` | `/usr/local/bin` | Screenshots through `grim` and `slurp`: `region` (the default), `screen` (also `full`), `qr` | [The kdos command](../04-programs/kdos-command.md#kdos-shot) |
| `kdos-updatedb` | `/usr/local/bin` | Rebuild this user's own `plocate` index, scoped to `$HOME` | [Configuration](configuration.md#etckdostimersd-and-configkdostimersd) |

While `kdos-record` runs, its process id is in `$XDG_RUNTIME_DIR/kdos/screencast.pid`; the next
invocation reads that file to stop the recording. The panel's `SCR` privacy lamp does not read the
file: it counts PipeWire ScreenCast nodes, so it lights for any screen share. The binding is
`Alt+Print` in the shipped `rc.xml`.

`kdos-shot` saves to `~/Pictures/Screenshots` (or under `$XDG_PICTURES_DIR`) and copies the
picture to the clipboard. `window` is accepted and behaves as `region`. `kdos-shot qr` captures to
the runtime directory, decodes a QR code in the picture onto the clipboard and deletes the
picture, so a pairing token never reaches the disk.

## Daemons

The daemons are installed in `/usr/sbin` and started from `/etc/init.d`. Three of them have a
client, which is the daemon's own binary under a second name, a symlink in `/usr/bin`, so a daemon
and its client are always the same version.

| Command | What it does | Documented in |
|---|---|---|
| `kdos-powerd` | Suspend, power-off, reboot, and the system settings a desktop user may change | [The daemons](../04-programs/daemons.md#kdos-powerd) |
| `kdos-power` | Its client | [The daemons](../04-programs/daemons.md#the-client) |
| `kdos-mountd` | The removable-media daemon: disks, LUKS, SMB shares, SMART | [The daemons](../04-programs/daemons.md#kdos-mountd) |
| `kdos-mount` | Its client | [The daemons](../04-programs/daemons.md#the-command-line-client) |
| `kdos-energyd` | Per-application energy attribution: which application is using the processor's power | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-energy` | The per-application energy report; `--json`, `ping` | [The daemons](../04-programs/daemons.md#kdos-energyd) |
| `kdos-oomd` | The memory-pressure daemon, which spares the desktop | [The daemons](../04-programs/daemons.md#kdos-oomd) |
| `kdos-packd` | The pack daemon, the only program that mounts a pack | [The daemons](../04-programs/daemons.md#kdos-packd) |

`kdos-power` takes these forms:

```sh
kdos-power [--no-lock] suspend|poweroff|reboot|ping
kdos-power timezone Area/City
kdos-power autologin USER|off
kdos-power firewall list|SERVICE on|off
kdos-power accent SCHEME
```

`kdos-mount` takes nine verbs:

| Verb | Does |
|---|---|
| `list` | The devices the daemon offers, one numbered row each |
| `mount N`, `unmount N`, `smart N` | Act on row `N` of `list` |
| `shares` | The network shares that are connected |
| `browse` | The hosts that answer an mDNS and a NetBIOS broadcast sent for this call, as `name<TAB>address` rows |
| `krb5 SERVER SHARE USER DOMAIN` | Mount a share with the Kerberos ticket `kinit` already obtained; `-` means no user or no domain |
| `ping` | Whether the daemon is answering |
| `subscribe` | Print `changed` whenever the list changes; never exits |

No form takes a device or a mount point: the daemon decides both. The daemon's socket answers
fifteen verbs, the nine above plus `eject`, `close`, `unlock`, `format`, `cifs` and `disconnect`,
which `kdos-disks` and `kdos-connect` send. The full protocol is in
[Filesystem and IPC](filesystem-and-ipc.md#sockets).

## Applications and boxes

A **box** is the container an application runs in; see the [glossary](glossary.md).

| Command | Installed in | What it does | Documented in |
|---|---|---|---|
| `kdos-appbox` | `/usr/local/bin` | Launch, install and export boxed applications; generate launchers | [kdos-appbox](../04-programs/kdos-appbox.md) |
| `kdos-box` | `/usr/local/bin` | Manage boxes: create, enter, freeze, export, snapshot, profile | [kdos-appbox](../04-programs/kdos-appbox.md#kdos-box) |
| `xdg-open` | `/usr/local/bin` | Open a file or a link with its handler | [kdos-appbox](../04-programs/kdos-appbox.md#the-open-path) |
| `xdg-terminal-exec` | `/usr/local/bin` | Run a command in a `foot` window, for `Terminal=true` handlers launched through GLib | [kdos-appbox](../04-programs/kdos-appbox.md#terminal-entries) |
| `kdos-boxinit` | `/usr/libexec/kdos` | Process 1 inside a pack box | [Packs and boxes](../03-architecture/packs-and-boxes.md#the-box) |
| `kdos-openarchive` | `/usr/local/bin` | Extract, with `ouch`, an archive `mc` cannot browse as a directory, beside the archive | [kdos-shell](../04-programs/kdos-shell.md#kdos-pick) |
| `kdos-pack` | `/usr/bin` | Build, sign, re-stamp, index and diff packs | [Packs and boxes](../03-architecture/packs-and-boxes.md#building-a-pack) |
| `kdos-podman-api` | `/usr/bin` | Run a container client against this account's Podman API socket, starting `podman system service` when nothing answers on it | [Configuration](configuration.md#containers) |

`kdos-appbox` takes `run`, `open`, `catalogue`, `install`, `uninstall`, `store`, `import`,
`export`, `warmup`, `status`, `list`, `apps` and `genlaunchers`, with the global options
`-b`/`--box NAME` and `-v`/`--verbose`. `kdos-box` takes `list`, `create`, `enter`, `run`, `apps`,
`export`, `unexport`, `freeze`, `import`, `clone`, `snapshot`, `snapshots`, `rollback`, `start`,
`stop`, `restart`, `remove`, `profile` and `gc`.

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
(with `--json`), `meta`, `verify` (and `verify --repro`), `keygen`, `sign FILE KEY`, `index` (with `--sign KEY`),
`verify-index`, `verify-pkg`, `delta`, `apply-delta`, `binhost`, `store gc DIR MAX-SIZE` and
`help`, and the options
`--root PATH`, `--keep-cache`, `-f`/`--force` and `--overwrite`. `install` also takes
`--build-only`, which builds exactly the named ports into the package cache and installs nothing,
and `--commit`, which installs what `--build-only` built; see
[Building apart from installing](../03-architecture/packaging.md#building-apart-from-installing).
`store gc` deletes [package store](../03-architecture/packaging.md#the-package-store) entries, least
recently used first, until the store fits `MAX-SIZE` (bytes, or a `K`, `M`, `G` or `T` suffix).
`kpkg help` describes each.

`kpkg help` also lists `update`, which `kpkg` does not implement: `kpkg update` prints the usage
text and exits 1. `kdos update` is the command that updates the system; see
[Updating a machine](../03-architecture/packaging.md#updating-a-machine).

## Boot, console and services

| Command | Installed in | What it does | Documented in |
|---|---|---|---|
| `kdos-splash` | `/usr/bin` | The boot splash | [Boot and init](../03-architecture/boot-and-init.md#the-splash) |
| `kdos-bootctl` | `/usr/bin` | A/B root slot selection and confirmation, each slot's kernel on the ESP, and the colours of the boot menu and text consoles | [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection) |
| `kdos-getty` | `/usr/local/sbin` | Load the console font and palette onto a VT, then run a getty: `kdos-getty TTY GETTY [ARGS…]` | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-login` | `/usr/local/sbin` | Hand `tty1` to `agetty`, logging in automatically when `/etc/kdos/login.conf` names an account | [Boot and init](../03-architecture/boot-and-init.md#the-console) |
| `kdos-banner` | `/usr/local/bin` | The login banner | [Boot and init](../03-architecture/boot-and-init.md#the-login-banner) |
| `ksvc` | `/usr/sbin` | The service supervisor | [Administration](../02-user-guide/administration.md#services) |
| `service` | `/usr/sbin` | The same supervisor, under the conventional name | [Administration](../02-user-guide/administration.md#services) |
| `kinstall` | `/usr/bin` | The installer | [kinstall](../04-programs/kinstall.md) |

`/etc/inittab` runs `kdos-getty tty1 kdos-login tty1` on the first console and
`kdos-getty tty2 /sbin/getty 38400 tty2` on the second.

`ksvc` (and `service`) takes `list`, `status NAME`, `start NAME`, `stop NAME`, `restart NAME`,
`enable NAME`, `disable NAME` and `help`, the commands a person types. The init scripts also call
`supervise`, `stop-supervised NAME` and `check NAME`, through `/etc/init.d/service_helper`. See
[ksvc and service](../04-programs/kdos-command.md#ksvc-and-service).

`kdos-bootctl` takes:

```sh
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

| Command | Installed in | What it does | Documented in |
|---|---|---|---|
| `kdos-res` | `/usr/bin` | The resource monitor | [kdos-res](../04-programs/kdos-res.md) |
| `kdos-term` | `/usr/bin` | The terminal | [kdos-term](../04-programs/kdos-term.md) |
| `kdos-bb` | `/usr/bin` | The ASCII-art demo | [kdos-bb](../04-programs/kdos-bb.md) |
| `kdos-theme` | `/usr/bin` | Generate the GTK, icon and cursor themes for an accent: `gtk`, `icons`, `cursors`, `accents` | [Theming](../02-user-guide/theming.md#how-the-theme-is-generated) |
| `kdos-share` | `/usr/local/bin` | Send files to another machine over `croc`; with no file, `kdos-pick` asks for one | [The kdos command](../04-programs/kdos-command.md#kdos-share) |
| `kdos-sfx` | `/usr/local/bin` | Sound effects: `login`, `notify`, `error`, `degauss`; `--route` prints the output it would use | [The kdos command](../04-programs/kdos-command.md#kdos-sfx) |
| `kdos-mpctl` | `/usr/local/bin` | Music control: `toggle`, `stop`, `next`, `prev` to mpd or an MPRIS player; `now` and `watch` from mpd | [The kdos command](../04-programs/kdos-command.md#kdos-mpctl) |
| `kdos-fetch-app` | `/usr/local/bin` | Install an application from another distribution into a distrobox; `--box`, `--image`, `--remove` | [The kdos command](../04-programs/kdos-command.md#kdos-fetch-app-and-kdos-fetch-static) |
| `kdos-fetch-static` | `/usr/local/bin` | Fetch one static binary and verify it: `kdos-fetch-static NAME URL SHA256` | [The kdos command](../04-programs/kdos-command.md#kdos-fetch-app-and-kdos-fetch-static) |
| `kdos-syncthing` | `/usr/bin` | Syncthing, given a LAN-only configuration the first time it runs | [Administration](../02-user-guide/administration.md#syncing-and-serving-on-the-network) |
| `update-ca-certificates` | `/usr/bin` | Rewrite `/etc/ssl/cert.pem` from the Mozilla bundle and the local roots in `/etc/ca-certificates/trust-source/anchors/`; `--root DIR` | [Security model](../03-architecture/security-model.md#tls-trust-anchors) |
| `sfeed-read` | `/usr/bin` | Open `sfeed_curses` on every feed under `~/.sfeed/feeds`, or say how to fetch some when there are none | The `sfeed` port's `build.sh` |

`kdos-syncthing`, `update-ca-certificates`, `sfeed-read` and `kdos-podman-api` are shell scripts
that a port recipe writes into its package (the `syncthing`, `ca-certificates`, `sfeed` and
`podman` ports), not programs under `src/`.

`kdos-term` takes `-e` (also `--exec`), `--title`, `--app-id`, `--font`, `--size WxH`, `--float`,
`-D DIR` (also `--working-directory`), `--tty` and `--dump WxH`. Everything after `--` is the
command to run. Its configuration is `~/.config/kdos/term.conf`.

`kdos-mpctl watch` writes `$XDG_RUNTIME_DIR/kdos/nowplaying` and waits in mpd's `idle`; it
reconnects when mpd restarts. The session starts one per login once an mpd configuration file
exists.

## Privileged helpers

Two of KDOS's own binaries are setuid root, each doing one thing an unprivileged process cannot.
Both are in `/usr/bin` and installed mode `4755`. The image's whole setuid inventory is in
[The security model](../03-architecture/security-model.md#setuid-binaries).

| Command | What it does | Documented in |
|---|---|---|
| `kdos-checkpass` | Check the caller's own password against `/etc/shadow`, for `kdos-lock` | [The security model](../03-architecture/security-model.md#kdos-checkpass) |
| `kdos-resctl` | For `kdos-res`: `signal PID TERM\|KILL\|STOP\|CONT`, `renice PID N`, and `dmi` to read the hardware table; the caller must be in `wheel` | [The security model](../03-architecture/security-model.md#kdos-resctl) |

## Host-only tools

These run on the machine KDOS is built on, from the top of the repository, and never ship on the
target. [How KDOS is built](../05-developer/how-kdos-is-built.md) follows them in the order a build
uses them, from `make fetch` to a bootable ISO.

### Make targets

| Target | What it does | Documented in |
|---|---|---|
| `make fetch` | Download and verify every recipe's sources; after it, `make build` needs no network | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `make fetch-check` | Check offline that every source is present and matches its hash | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `make build` | The whole build, in the container; `BUILD_ARGS` reaches the orchestrator | [Developing](../05-developer/developing.md#make-targets) |
| `make snapshots` | List the phase snapshots | [Developing](../05-developer/developing.md#make-targets) |
| `make updates` | Run `ports/update` over every port; `PORTUP_ARGS` narrows it | [Developing](../05-developer/developing.md#make-targets) |
| `make run`, `make rundisk` | Boot the ISO, or `build/kdos.qcow2`, in a virtual machine with plain graphics | [Developing](../05-developer/developing.md#running-the-result) |
| `make run-hw`, `make rundisk-hw` | The same with hardware-accelerated graphics, through `testing/qemu-hw/` | [Developing](../05-developer/developing.md#running-the-result) |
| `make debug-boot` | Boot the built kernel and initramfs directly, with the serial console on the terminal | [Developing](../05-developer/developing.md#running-the-result) |
| `make cleandisk`, `make cleanbuild`, `make clean` | Replace the test disk; empty `build/` except `snapshots`, `ccache`, `pkgstore` and `keys`; empty it except `keys` | [Developing](../05-developer/developing.md#make-targets) |
| `make help` | List every target with a one-line description | [Developing](../05-developer/developing.md#make-targets) |
| `make publish`, `publish-dry`, `publish-check`, `publish-describe`, `publish-rehome`, `publish-orphans` | `ports/publish` and its modes over the `src-<shelf>` source archive; `PORTS` and `PUBLISH_ARGS` narrow them | [Developing](../05-developer/developing.md#make-targets) |
| `make freeze`, `make release`, `make release-publish` | The system release for `TAG`: `sources.sha256`, a draft with the ISO and signed `SHA256SUMS`, then public and Latest | [Developing](../05-developer/developing.md#make-targets) |
| `make check`, `preflight`, `selftest`, `selftest-asan`, `docscheck`, `phaseclosure` | The checks that stand in for a build; `check` runs the four that need no built tree | [Developing](../05-developer/developing.md#make-targets) |
| `make depdrift`, `make debuginfo` | Audit `build/fs` for undeclared link dependencies, or for shipped DWARF | [Developing](../05-developer/developing.md#make-targets) |
| `make quick`, `make rig-image`, `make devdeps-image` | The fast loop, and the containers it and the rig run in | [Testing](../05-developer/testing.md#the-fast-loop) |

`KDOS_RES` (default `1920x1080`) sets the virtual screen for `make run`, `rundisk`,
`run-hw` and `rundisk-hw`.

### Scripts and programs

| Command | What it does | Documented in |
|---|---|---|
| `ports/fetch [--check] [--tree DIR] [PORT…]` | What `make fetch` runs: each source from the port directory, the cache, the sources archive or upstream, generating a vendor bundle where one must be made. A port is named by its bare name on any shelf, and a name that is no port fails the run | [Developing](../05-developer/developing.md#where-sources-come-from) |
| `ports/publish [--dry-run \| --check] [--history] [PORT…]` | Upload sources the archive lacks into the `src-<shelf>` pre-release of the shelf that owns them, under their own names (in parts past 1,900 MiB), and write their lines into `ports/sources.idx`. `--describe` rewrites the releases' notes; `--rehome [PORT…]` moves files to the release of the shelf their port now sits on and, once the index is pushed, deletes the old copies; `--orphans` lists index lines no recipe names, and `--orphans --prune=yes-delete` deletes the unprotected ones; `--freeze TAG` attaches a release's hash list. Ports are named as for `ports/fetch` | [Writing ports](../05-developer/writing-ports.md#portspublish) |
| `ports/publish --release TAG [--iso PATH] [--key PATH \| --unsigned] [--publish] [--dry-run]` | Put the system release `TAG` on `kunaldawn/kdos`: the ISO (in parts from 1,900 MiB), `sources.sha256`, a signed `SHA256SUMS` and the public key, as a draft until `--publish` makes it public and Latest | [Developing](../05-developer/developing.md#cutting-a-release) |
| `script/hooks/pre-push` | Refuse a push that breaks the ports layout, or names a source the archive lacks; enabled with `git config core.hooksPath script/hooks` | [Writing ports](../05-developer/writing-ports.md#the-pre-push-hook) |
| `kdosbuild [--fresh] [--restore PHASE] [--continue-from PHASE] [--phases LIST] [--steps LIST] [--rebuild LIST] [--plan] [--snapshot] [--no-snapshot] [--full-snapshots] [--port-jobs N] [--plain] [--json] [--list] [--delete PHASE]` | The build orchestrator, compiled from `src/devtools/kdosbuild/` by `script/kdosbuild.sh`, which `make build` runs with `BUILD_ARGS`. `--port-jobs N` builds up to `N` ports of a package phase at once, by dependency level | [The build system](../05-developer/build-system.md#flags) |
| `script/chroot/enter.sh` | A root shell, or one command, inside a built tree, for inspection by hand | [The build system](../05-developer/build-system.md#the-chroot) |
| `ports/update` | Check ports for newer upstream releases; compiles and runs `kdos-portup` | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `kdos-portup` | The version checker `ports/update` runs, from `src/devtools/kdos-portup/`, compiled to `ports/.portup` | [Writing ports](../05-developer/writing-ports.md#checking-for-new-versions) |
| `ports/hackage-vendor` | The Hackage downloader `ports/fetch` runs to make a Haskell port's vendor bundle | [Writing ports](../05-developer/writing-ports.md#the-haskell-bundle) |
| `testing/preflight.sh` | 55 checks over the wiring of the tree | [Testing](../05-developer/testing.md#preflightsh) |
| `testing/debuginfo.sh [ROOT]` | List the ELF files under a built root's `usr/` that carry DWARF, grouped by owning package | [Testing](../05-developer/testing.md#debug-information-in-the-built-tree) |
| `testing/phaseclosure.py` | Check that every package phase installs exactly the ports its list names; preflight runs it | [Writing ports](../05-developer/writing-ports.md#which-phase-lists-a-port) |
| `testing/selftest.sh` | The library and consumer suite, with the goldens | [Testing](../05-developer/testing.md#selftestsh) |
| `testing/docscheck.sh` | This book: dead links, historical phrasing, the page contract | [Testing](../05-developer/testing.md#docschecksh) |
| `testing/devdeps-image.sh` | Build the `kdos-devdeps` image and run the self-test in it, with nothing skipped | [Testing](../05-developer/testing.md#the-machine-where-nothing-is-skipped) |
| `testing/rig-image.sh` | Build the `kdos-qemu-py` rig image | [Testing](../05-developer/testing.md#the-host-may-not-have-an-emulator) |
| `testing/vnc-shot.py` | Boot, drive and photograph a real session | [Testing](../05-developer/testing.md#the-qemu-rig) |
| `testing/quick.sh`, `testing/quickpatch.sh` | Rebuild ports and patch them into a booted ISO's in-memory overlay | [Testing](../05-developer/testing.md#the-fast-loop) |
| `testing/qemu-audio.sh` | Pick a working QEMU audio backend for the run targets and the rig | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/usability.sh` | Drive the desktop with the pointer and chords, as a person would, and leave a numbered contact sheet to check against `testing/usability.md` | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/bios-boot.sh` | Boot the ISO with no UEFI firmware | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/packlane.sh` | The [pack lane](glossary.md) (installing signed packs into a box) end to end on a booted machine | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/install-to-disk.sh` | Run the installer into a disk image | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appsweep.sh` | Launch every catalogue application | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appreport.sh` | Report on what the sweep found | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/appbox-smoke.sh` | Check that every boxed application's launcher starts something | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/hostcheck.sh` | Check that the binary a caller resolves by name is the one that can do the job | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/oomd-fire.sh` | Put a booted machine under real memory pressure and read `kdos-oomd`'s kill count | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/prepare_base.py`, `testing/test_runner.py` | Build a minimal root filesystem image and build single ports against it; `report_gen.py` summarises the results | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/bootcheck/` | Scripted boots with no display: start QEMU, run a command on the serial console, type | [Testing](../05-developer/testing.md#the-other-harnesses) |
| `testing/qemu-hw/` | The containerised QEMU with accelerated graphics behind `make run-hw` | [Testing](../05-developer/testing.md#the-other-harnesses) |

## See also

- [The programs](../04-programs/README.md): the same commands grouped by what they are, with the
  multi-name mapping
- [The kdos command](../04-programs/kdos-command.md): every subcommand and every other name of the
  `ksvc` binary in full
- [The ports catalogue](ports-catalogue.md): the upstream software this index leaves out, by
  shelf, with the phase that builds each port
- [How KDOS is built](../05-developer/how-kdos-is-built.md): the host-only tools in the order a
  build runs them
- [Configuration](configuration.md): every setting these commands read
- [Filesystem and IPC](filesystem-and-ipc.md): the paths and sockets they use
- [Glossary](glossary.md): the vocabulary used here

<!-- book-nav -->
---

*Part VI — Reference, chapter 39.* Previous: [38. The ports catalogue](ports-catalogue.md) · [Contents](../README.md) · Next: [40. Configuration](configuration.md)
