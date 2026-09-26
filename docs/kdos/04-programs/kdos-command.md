# The kdos command

This page is the reference for `kdos`, the command-line front door to KDOS, and for the other
small tools built into the same binary (`ksvc`, `kdos-bootctl`, `kdos-shot` and the rest). It is
for anyone using a KDOS machine from a terminal: someone checking why something broke, changing
the accent, managing applications, keeping a live session, or scripting the desktop.

If you are new to KDOS, start with `kdos help`, then `kdos status` and `kdos doctor`. Those three
tell you what the machine is, what is running and what is wrong. Everything else on this page is
something you reach for when you have a specific job.

Words such as *box*, *pack*, *accent* and *binhost* are defined in the
[glossary](../06-reference/glossary.md).

## Synopsis

```
kdos <subcommand> [arguments...]
kdos help [--pager]
kdos version
```

`kdos` with no subcommand is `kdos help`. `-h` and `--help` are accepted as spellings of `help`, and
`-V` of `version`. An unknown subcommand prints `unknown command '<name>' — try: kdos help` and
exits 1.

Most subcommands follow one exit-status convention, so a script can test the answer without
parsing text:

| Exit | Means |
|---|---|
| 0 | Nothing to report: clean, up to date, done |
| 1 | Something was found (a warning, a stale process, a vulnerable package), or the action failed |
| 2 | The command was used wrongly, or could not run at all |
| 127 | The program this subcommand hands over to is not installed |

Where a subcommand differs, its section says so.

## Finding a subcommand

| Subcommand | Answers |
|---|---|
| [`app`](#kdos-app) | Install, launch, remove, export and import containerised applications |
| [`appid`](#kdos-appid) | Do launcher icons match the windows they open? |
| [`clone`](#kdos-clone) | Copy this boot medium onto another, verified by read-back |
| [`cve`](#kdos-cve) | Offline vulnerability tracking against a vendored database |
| [`doctor`](#kdos-doctor) | Check the machine for the breakage this system actually has |
| [`explain`](#kdos-why-and-kdos-explain) | Browse the recorded reasons behind decisions |
| [`hey`](#kdos-hey) | Ask the window manager about windows, outputs and boxes |
| [`help`](#kdos-help) | The command list and the keybinding sheet |
| [`march`](#kdos-march) | Measure whether a processor tuning flag is worth keeping |
| [`menu`](#kdos-menu) | Open the menu on a named route |
| [`notify`](#kdos-notify) | Raise a toast, or drive the notification daemon |
| [`oracle`](#kdos-oracle) | One recorded lesson, picked for today |
| [`panel`](#kdos-panel) | Put the bar away and bring it back |
| [`persist`](#kdos-persist) | Keep a live session's writes across a reboot |
| [`places`](#kdos-places) | The places column, and the way to add to it |
| [`rebuild`](#kdos-rebuild) | Rebuild KDOS from the sources on this machine, offline |
| [`remind`](#kdos-remind) | A toast, later |
| [`restarts`](#kdos-restarts) | Which running processes still use code an upgrade replaced |
| [`sandbox`](#kdos-sandbox) | Run a native program under a Landlock profile |
| [`settings`](#kdos-settings) | The control centre, or one of its pages |
| [`share`](#kdos-share) | Send a file to another machine over one code word |
| [`speech`](#kdos-speech) | Fetch and manage transcription models |
| [`status`](#kdos-status) | What this machine is and what it is running |
| [`stutter`](#kdos-stutter) | Why the desktop hiccuped, with the application's name |
| [`theme`](#kdos-theme) | Set, preview and audit the accent |
| [`thumb`](#kdos-thumb) | Put a thumbnail in the shared cache |
| [`toggle`](#kdos-toggle) | Stay-awake, night light and do-not-disturb |
| [`trash`](#kdos-trash) | The desktop's trash, from a prompt |
| [`update`](#kdos-update) | Check for and apply upgrades across the A/B slots |
| [`version`](#kdos-version) | Kernel, C library, userland, session and accent |
| [`why`](#kdos-why-and-kdos-explain) | What provides a path or a port, and why it is that way |

The same binary also answers to other names — the service supervisor, the boot-slot tool, the
screenshot tool and more. They are listed in
[The other names on this binary](#the-other-names-on-this-binary).

Several subcommands are bound to keys in the shipped `~/.config/kdos-comp/rc.xml`:

| Chord | Runs |
|---|---|
| `Super+Space` | `kdos-palette` (what `kdos menu` opens) |
| `Super+Shift+Space` | `kdos panel toggle` |
| `Super+Ctrl+Alt+t` / `Super+Ctrl+Alt+b` | `kdos notify --time` / `--battery` |
| `Super+x` / `Super+Shift+x` | `kdos notify --dismiss` / `--dismiss-all` |
| `Super+Ctrl+x` / `Super+Alt+x` | `kdos notify --dnd` / `--raise` |
| `Super+Ctrl+i` | `kdos toggle stay-awake` |
| `Super+Ctrl+Shift+n` | `kdos toggle night-light` |
| `Super+Ctrl+r` / `Super+Ctrl+Alt+r` / `Super+Ctrl+Shift+r` | `kdos remind --ask` / `ls` / `clear` |

---

## kdos help

```sh
kdos help            # print the sheet
kdos help --pager    # the same, through $PAGER (default less)
```

The sheet has three blocks:

- **WHERE THINGS LIVE** names the three places software lives: `kpkg` for the host system
  (compiled from source against musl), `kdos app` for applications (one signed file each), and
  `kdos-box` for environments. Most "how do I install X" questions start with which of these
  three X belongs to.
- **COMMANDS** lists the everyday verbs, including the ones that live in other programs
  (`kdos-power`, `kdos-energy`, `kdos-res`, `kinstall`).
- **KEYS** lists the default bindings. When `kdos-keys` is installed, a first line points at
  `Super+F1`, which shows the full card generated from your own `rc.xml`.

`--pager` feeds the sheet to `$PAGER`, adding `-R` only when the pager is `less`. If the pager is
missing or fails, the sheet is printed to standard output instead. The **KDOS Help** menu entry
runs `kdos help --pager` in a terminal.

## kdos theme

```sh
kdos theme                    # print the accent in force (also: show, current)
kdos theme <name>             # phosphor amber ice bone norton borland perfect paper
kdos theme list | next | prev
kdos theme style <file>
kdos theme --audit [accent]   # also: audit
kdos theme --preview <accent>
```

KDOS has eight accents, compiled into `libkcolor`. `bone` is the default: an unconfigured machine
uses it. `list` marks the one in force with `*`; `next` and `prev` step through the table and wrap
around. The user-facing guide is [Theming](../02-user-guide/theming.md).

### What setting an accent does

Setting an accent regenerates every themed file for your user and then repaints the running
desktop, in this order:

1. The generators rewrite the GTK stylesheet, icons, cursors, the KDE colour file, the window-frame
   theme (`~/.config/kdos-comp/themerc-override`), and the configuration of foot, `kdos-term`,
   bat, micro, helix, neovim, delta, newsboat, aerc, fzf, tmux, btop, mc, yazi, starship and
   `LS_COLORS`.
2. The wallpaper is retinted into `~/.cache/kdos/wallpaper.png`.
3. The accent is written to `~/.cache/kdos/theme`, atomically.
4. `SIGHUP` goes to each long-lived KDOS surface by exact name: `kdos-shell`, `kdos-desk`,
   `kdos-notifyd`, `kdos-slit`, `kdos-res`, `kdos-comp` and `kdos-term`.
5. `kdos-power accent <name>` asks `kdos-powerd` to retint the boot menu and the splash. When the
   daemon does not answer — on the live medium, or inside a box — the command prints
   `boot menu and splash unchanged` and the accent is still applied everywhere else.
6. A running tmux server re-reads its configuration.

The order matters. The wallpaper and the state file are both inputs to the signal, so both are
written before it is sent; a surface that received the signal first would re-read the accent it
already had.

The match on names is exact for two reasons. The panel, the desktop icons and the notification
daemon are three names of one binary, so signalling only `kdos-shell` would leave the desktop and
any toast in the old accent. And `kdos-comp` is a substring of `kdos-desktop-start`, the shell
script that owns the session, which would die of a signal it does not handle.

Programs that are not KDOS surfaces pick up the change when they next start: foot, btop, and
every application in a box.

Because the retinted wallpaper in `~/.cache/kdos/wallpaper.png` takes precedence over the
`wallpaper =` key in `comp.conf`, an accent switch replaces a picture of your own with the retinted
default. [Theming](../02-user-guide/theming.md#wallpaper) explains how to keep your own.

### Previews

`--preview <accent>` does only steps 3 and 4: it writes the state file and sends the signal. No
GTK stylesheet, icons, cursors or foreign configuration files are generated. Those take seconds and
are read by programs that are not running, so a preview repaints every KDOS surface at once and
leaves boxed applications in the old accent. It is what the arrow keys in the `kdos-theme` picker
run, and it is why that picker restores the accent it opened on unless you tell it to keep one.

### Style files

`kdos theme style <file>` applies a shareable look: an accent plus the knobs that make a desktop
someone's own, in one flat file. Each line is `key = value` or `key: value`; the first separator
on the line wins, so `chrome_font = Terminus:pixelsize=64` keeps its colon.

```
accent = amber
crt = 40
crt_scanlines = 50
chrome_font = Terminus (TTF):pixelsize=32
osd.bg.color: #1a1a1a
```

| Key | Goes to |
|---|---|
| `accent` | The accent; without one, the accent in force is kept |
| `crt`, `crt_scanlines`, `crt_curve`, `crt_fullscreen`, `chrome_font`, `clock_format` | Rewritten in `~/.config/kdos/comp.conf`, keeping every other line as it was; a key the file lacks is appended. The `crt*` keys take effect immediately; `chrome_font` and `clock_format` at the next login |
| Any dotted key (`osd.bg.color`, …) | A window-frame theme line, kept in `~/.config/kdos/style-themerc` and appended after the generated block every time the theme is regenerated |
| Anything else | Ignored, with a warning naming the key |

A style with no dotted keys removes `style-themerc`, because a style is a whole look rather than a
patch on the last one. The accent is then applied exactly as `kdos theme <name>` applies it.

### Auditing

`--audit` checks that every generated file still matches the palette. It runs the same generators
with the home and cache directories pointed at a scratch directory and compares the results with
your files byte for byte, symlinks included. Anything that differs, differs from what this
machine's palette produces right now. It writes nothing outside its scratch directory and signals
nothing.

Give it an accent — `kdos theme --audit amber` — to see what would change if you switched to that
accent before you switch.

| Exit | Means |
|---|---|
| 0 | Every generated file matches |
| 1 | Something has drifted |
| 2 | The audit could not run |

## kdos notify

```sh
kdos notify <summary> [body]
kdos notify --time | --battery
kdos notify --dismiss | --dismiss-all | --raise | --dnd
```

With a summary, this raises a toast. It is handy at the end of a long job:

```sh
make && kdos notify "the build finished"
```

The toast looks exactly like one a terminal program raises with an OSC 9 notification, because
both go through the same code.

`--time` and `--battery` compute their text, which a key binding cannot do: `rc.xml` binds a fixed
command string. They answer the two questions a bar answers by being on screen, for a desktop
whose bar can be put away.

- `--time` shows the time as `%H:%M`, with the full date as the body.
- `--battery` shows the charge and state, plus wear when the kernel reports it (for example
  `Battery 90%` / `Discharging, 70% of its original capacity`). A battery at 90% charge can hold
  only 70% of what it held new, and someone deciding whether to unplug wants both numbers. With no
  battery it says `On mains` or `No battery`. The figures come from the kernel through `libkproc`,
  the same reader the resource monitor uses. `kdos-energyd` estimates what programs cost and holds
  no battery state.

`--dismiss`, `--dismiss-all`, `--raise` and `--dnd` send one line (`dismiss`, `dismiss all`,
`raise`, `dnd toggle`) down `$XDG_RUNTIME_DIR/kdos-notify.sock`, the socket `kdos-notifyd`
already serves. The daemon owns the toasts, the history and the Do Not Disturb flag; these flags
are only the command a key binding needs. When no daemon is listening they do nothing and print
nothing, because a chord has nowhere to show an error.

`--raise` moves the newest entry out of the history and back onto the screen rather than copying
it, so dismissing it again does not file a second copy. It comes back without its buttons: the
notification it came from is closed, and its actions belonged to the program that sent it.

## kdos toggle

```sh
kdos toggle                  # list them and their state
kdos toggle <name>           # flip it
kdos toggle <name> on|off    # set it
```

Three switches, each a flag file under `~/.local/state/kdos/toggles/` whose presence means on:

| Name | Does | Read by |
|---|---|---|
| `stay-awake` | Never dim, lock or blank on idle | `kdos-comp`'s idle policy |
| `night-light` | Warm the palette | Every surface, on the retint signal |
| `dnd` | Hold notifications back | `kdos-notifyd` |

An unknown name is refused with the list. The files and their readers are also listed in
[Configuration](../06-reference/configuration.md#localstatekdostoggles).

`night-light` is read on the same retint signal `kdos theme` sends, so the toggle writes its file
first and then sends the signal. The other two are checked on a timer their readers already run
and need no signal.

Typed at a prompt, the new state is printed. Run from a key binding, it is shown as a one-line
toast instead, because two of the three switches change nothing visible and a silent keystroke
cannot be told from a broken one. `Super+Ctrl+i` is `stay-awake` and `Super+Ctrl+Shift+n` is
`night-light`; `Super+Ctrl+n` is taken by `kdos-note`.

## kdos panel

```sh
kdos panel toggle
```

Hides the bar, or brings it back. It sends `SIGUSR1` to every process whose name (`comm`) is
exactly `kdos-shell`, which reaches the panel and not the desktop icons or the notification
daemon, the other names of the same binary. It is bound to `Super+Shift+Space`. The command finds
the panel itself, so it works whichever `pkill` is installed.

A bar put away this way stays away while the pointer crosses the bottom row, even with autohide
on. Nothing is reported when no panel is running.

## kdos menu

```sh
kdos menu summon <route>     # open the menu on a named place
kdos menu toggle [<route>]   # close it if it is open, else open it
```

A route is a name for a place in the system, such as `setup.network`, defined in
`/etc/kdos/menu.conf` and your own `~/.config/kdos/menu.conf` — see
[Configuration](../06-reference/configuration.md#etckdosmenuconf-merged-under-configkdosmenuconf).
A script holds a route rather than a chord, because chords can be rebound and menu rows move.

Both verbs open `kdos-palette` — the program `Super+Space` opens — with `--route <route>`, so
summoning a route is a search with the name already typed. `toggle` first runs
`pkill -x kdos-palette`; if that found a palette, it is closed and the command returns, and
otherwise a new one is opened. The match is on the exact program name, so the palette must be
started as `kdos-palette` and not through a wrapper. Exit 2 for a missing or unknown verb, 127 when
`kdos-palette` is not installed.

## kdos status

```sh
kdos status
kdos status --bar            # one line: running boxes and exported applications
```

Prints what this machine is and what it is running:

| Line | From |
|---|---|
| `KDOS  Linux <release>` | The running kernel |
| `Theme` | The accent in force |
| `Packages` | Entries in `/var/lib/kpkg/db` |
| `Exported apps` | `.desktop` files in `~/.local/share/applications` |
| `Alien apps` | Lines in `/usr/share/kdos/alien-apps`, the table baked at build time |
| `Session` | `tty` outside a desktop, `kdos-comp` when the compositor is running, or `wayland (not kdos-comp)` |
| CONTAINERS | `podman ps -a`: every box, its status and image |
| EXPORTED APPS | The exported entries by name |

The alien application count reads the baked table rather than asking the container engine, so it
answers on a machine where no box has ever been created.

`--bar` prints `N box · M app`, counting running containers and exported entries.

## kdos doctor

```sh
kdos doctor [--json]
kdos doctor --cve [...]      # hands the rest of the line to kdos cve
```

Checks the things that actually break on KDOS rather than running a generic health sweep. Each
check reports one of three levels:

- `ok` — checked and fine;
- `warn` — checked and wrong, with the fix where there is one ("add yourself to the `video`
  group", "run: kdos theme phosphor");
- `skip` — could not be checked here, with the reason. Much of the hardware section cannot be
  answered in a virtual machine (no energy counter, no wireless, no discrete graphics, no boot
  medium), and a green line for something never tested would be as misleading as a warning that
  made every virtual machine look broken.

| Section | Checks include |
|---|---|
| Kernel | The Landlock ABI level, and whether the microcode in the boot image matches the revision the processor is running |
| Hardware | The wireless regulatory database, audio (SOF) firmware, graphics firmware, the boot medium, and **device present but unopenable**: every attached device the calling user cannot open, with the group that owns it |
| Boxes | Whether the pack filesystem (erofs) is loadable, whether `kdos-packd` answers and by which mount route, whether the home directory's filesystem can hold a container layer, and whether every mounted pack still has a file behind it |
| Session | Whether the compositor's **socket** exists (not only whether `WAYLAND_DISPLAY` is set), `XDG_RUNTIME_DIR`, and whether `kdos-comp`, `kdos-shell` and the wlr portal are running |
| Containers | The mount-namespace root, and the `/etc/subuid` and `/etc/subgid` mappings rootless containers need; a warning when run as root |
| Desktop | The accent state file, the foot theme, the KDE colour file, Xwayland's socket, five setuid bits (see below), `kdos-powerd`, `kdos-energyd` (or the absence of an energy counter), fcitx5, `kdos-oomd`, the compositor's frame-timing and command sockets, `kdos-keys`, the screen-capture portal and its configuration, and `~/.local/bin` on `PATH` |
| Security | The default password. Shown to root only; an ordinary user gets no section rather than a check that pretends it looked |

The session check looks for the socket because a session that died leaves `WAYLAND_DISPLAY` set in
every shell that inherited it. A doctor that reported that as fine would send you looking
everywhere but at the dead compositor.

On a live session, the box check reports that the home directory is on an overlay and so a
persistent box cannot exist. That is a rule of the live medium, stated to the person it affects.

Losing a setuid bit is silent, and each of the five fails differently:

| Helper | Without its bit |
|---|---|
| `kdos-checkpass` | Every password at the lock screen is wrong |
| `kdos-resctl` | `kdos-res` cannot end or renice a process |
| `newuidmap`, `newgidmap` | The container engine exits 125 and no box starts |
| `dbus-daemon-launch-helper` (group `messagebus`) | No D-Bus system service is ever activated: Wi-Fi's supplicant, fingerprints and firmware updates among them |

| Exit | Means |
|---|---|
| 0 | No warnings |
| 1 | At least one warning |
| 2 | Unknown option |

The exit status is the same in text and JSON mode. `--json` prints
`{"checks": [{"section", "level", "message"}, …], "warnings": N}`, one record per line of the
text report, and `warnings` the number of warnings.

`--cve` is handed to [`kdos cve`](#kdos-cve) with the rest of the arguments, and its exit status is
`kdos cve`'s. The vulnerability report is a table with its own exit code and database date, and
folding it into doctor's lines would flatten "17 packages are behind a recorded fix" into one
warning.

## kdos version

```sh
kdos version
kdos -V
```

```
KDOS — KD's Homebrew Linux Distro
  kernel   7.2.7
  libc     musl
  userland toybox
  session  kdos-comp (bone)
```

The session line reads `kdos-comp` whenever `WAYLAND_DISPLAY` is set and `none` otherwise, followed
by the accent in force. `kdos status` also checks that kdos-comp is actually running: its session
line reads `tty` outside a Wayland session and `wayland (not kdos-comp)` when the compositor is not
running.

`kdos version` does not print the release number or the commit the image was built from. The
release is `VERSION` in `/etc/os-release`.

## kdos app

```sh
kdos app list [--all]
kdos app search <text>
kdos app info <id>                # also: show
kdos app groups
kdos app install <id|group>... [--dry-run]
kdos app install --pending
kdos app launch <id>
kdos app remove <id|group>...
kdos app export <file.ktar> <id|group>...
kdos app import <file.ktar> [<id>...]
kdos app tui add <name> <command> [--float] [--size COLSxROWS] [--icon NAME] [--category X]
kdos app tui rm <slug>
kdos app tui ls
```

The command-line half of the application store, over the same catalogue the graphical store reads.
The user's guide is [Applications](../02-user-guide/applications.md), and the program that does the
work is [kdos-appbox](kdos-appbox.md).

| Verb | Does |
|---|---|
| `list` | What is installed. `--all` lists the whole catalogue (180 applications), which would bury the handful you have |
| `search` | Catalogue entries matching the text |
| `info` | One entry, with its size labelled an estimate |
| `groups` | The catalogue's named groups, which `install`, `remove` and `export` accept in place of an id |
| `install` | Builds the application on this machine, which needs a network. `--dry-run` shows what would be built. Installing over an existing application rebuilds it, which is how an application is updated — there is no separate update verb |
| `install --pending` | Installs what the installer was asked for and could not install itself, listed in `/var/lib/kdos/apps-pending`. The file is removed only after a clean run, so a partial run leaves it for a second attempt |
| `launch` | Installs the application if needed, then runs it |
| `remove` | Uninstalls |
| `export` | Writes the named applications into one `.ktar` set for another machine |
| `import` | Installs from a `.ktar` set, offline, verified at the mount; name ids to take only some |

Every verb that builds, removes, exports or imports runs `kdos-appbox` as a child, so the command
line and the store's buttons cannot disagree about what installing means.

Sizes are always labelled estimates. What apt resolves on the day depends on the snapshot, and
shared runtime layers are counted once on disk however many applications use them.

`import` is the only verb that needs `kdos-packd`, because it is the only one that mounts
anything. It never passes a path to the daemon: it copies the set into the daemon's own staging
directory and hands over a file name inside it, so a member of `wheel` cannot use it to mount an
arbitrary device on an arbitrary directory.

### kdos app tui

`tui` turns a terminal program into a menu entry without editing a file by hand:

```sh
kdos app tui add "Disk usage" ncdu --float --size 100x30 --icon disk-usage-analyzer
kdos app tui ls
kdos app tui rm disk-usage
```

`add` writes `~/.local/share/applications/kdos-tui-<slug>.desktop`. The entry has
`Terminal=true`, `X-KDOS-TUI=true`, and — when asked — `X-KDOS-Float=true` and
`X-KDOS-Size=COLSxROWS`, the two KDOS keys that say how the window opens. The `Exec` line is
written one quoted field at a time, so a path with a space in it survives. Every `%` in the
command is written doubled, because a single `%` starts a field code in a desktop entry. An entry
added this way opens its program; it is not a file handler.

`rm` deletes an entry only if it carries `X-KDOS-TUI=true`, so it cannot delete an application
the image shipped. The `kdos-tui-` prefix keeps new entries from shadowing shipped ones: a file in
your data directory with the same name as one in `/usr/share/applications` would take the shipped
entry off the menu.

`tui` writes only in your own data directory and needs neither `kdos-packd` nor membership of
`wheel`.

## kdos why and kdos explain

```sh
kdos why <path|port>
kdos explain [topic]
```

`why` answers what provides a path or a port and why it is configured that way. For an absolute
path it first prints the owning package and version, from the package database's own file list,
and the recipe it was built from. A path no package owns is reported as `none — provided by fs/,
or not installed`: files from the base filesystem tree are copied into the image rather than
installed by a package, so that is a common answer. It then prints every recorded reason that names the path or the port.

`explain` browses the recorded reasons: 78 short documents installed under
`/usr/share/kdos/reasons`, each stating a constraint and its consequence. With no topic it prints
them all. A topic matches anywhere in a document, ignoring case, not only in its file name, so
`kdos explain musl` finds the reasons that mention musl in passing. Exit 1 when no reasons are installed.

Each reason names the paths and ports it is about in `path:` and `port:` lines, and
`testing/preflight.sh` fails the build when one of those does not exist, so a reason cannot
describe something the tree does not have.

## kdos sandbox

```sh
kdos sandbox [profile] [--read P] [--write P] [--no-network] [--tcp PORT] -- <cmd> [args...]
kdos sandbox [profile] [options] --explain
```

Runs a native (not boxed) program under a Landlock filesystem and network filter. It needs no
container and no root.

| Option | Allows |
|---|---|
| `--read P` | Reading under `P` |
| `--write P` | Reading and writing under `P` |
| `--no-network` | No TCP at all (needs Landlock ABI 4) |
| `--tcp PORT` | With `--no-network`, still allow outgoing TCP connections to `PORT`; without it the network is not restricted and this has no effect |
| `--explain` | Print what would be allowed and what Landlock cannot police (UDP, unix sockets by path, processes and IPC), and run nothing |

Everything else is refused, except a fixed base: `/usr`, `/lib`, `/lib64`, `/bin`, `/sbin`,
`/etc` and `/proc` are readable, and `/dev/null`, `/dev/zero`, `/dev/full`, `/dev/random`,
`/dev/urandom`, `/dev/tty`, `/dev/ptmx` and `/dev/pts` are writable. The rest of `/dev` is not,
because that would hand over every disk and input device.

A profile is `~/.config/kdos/sandbox/<profile>.conf`, parsed and never executed, one `key = value`
per line:

```
read = /home/kdos/Music
write = /home/kdos/.cache/cmus
network = off
tcp-connect = 6600
```

The command is executed directly, never through a shell.

The command refuses to run rather than run unconfined: with no Landlock in the kernel it exits with
`no Landlock (<reason>) — refusing to run unconfined`, and `--no-network` on a kernel below ABI 4
exits with `--no-network needs Landlock ABI 4, kernel has <n> — refusing to run with the network
open`. `--explain` says the same thing instead of running. A Landlock
ruleset cannot be inspected from outside the process, so `--explain` describes what will be asked
for, not what a running program has.

## kdos appid

```sh
kdos appid [--quiet]
```

Checks that each launcher's desktop-file identifier matches the identifier its window actually
presents. A dock matches a running window to its launcher by that identifier, so a mismatch shows
a second, generic icon beside the pinned one.

The window side comes from `~/.local/share/kdos/observed-app-ids`, a ledger the compositor appends
to the first time each application maps a window. When there is no ledger yet it falls back to the
windows open right now and says so. The ledger covers every window this machine has shown; the
fallback covers only what is on screen. Exit 1 when a mismatch is found, 2 for a bad option.

## kdos restarts

```sh
kdos restarts [--quiet] [--json]
```

Lists running processes that still use code an upgrade has replaced or removed:

```
These are running code that has been replaced or removed.
They keep the old version until they restart:

  firefox-esr      (pid 2841)  nss, firefox-esr
  gimp             (pid 3122)  babl

2 processes. Restarting them puts the new code in effect.
```

After an upgrade, a process that had the old shared library mapped keeps running it until it
restarts. That is what makes upgrading a running system safe, but it also means a security fix is
not in effect for that process yet. This command joins two exact sources: `/proc/<pid>/maps`, where
the kernel marks a mapping whose file was deleted, and `kpkg`'s file manifest, which says which
package owned it. A mapping no installed package owns is reported as `no installed package owns
it` rather than guessed at, and a process whose maps cannot be read (another user's, or one that
exited during the scan) is skipped.

The list stops at 256 processes and says so. `--quiet` prints nothing. `--json` prints
`{"truncated", "processes": [{"pid", "comm", "exe", "packages"}], "count"}`.

| Exit | Means |
|---|---|
| 0 | Nothing is running replaced code |
| 1 | At least one process should be restarted, or `/proc` is not mounted |
| 2 | Bad option |

## kdos stutter

```sh
kdos stutter [--json] [--fixture DIR]
```

Watches the compositor's frame timing and, whenever a frame is late, says what the machine was
doing at that moment. It runs until you stop it.

```
7 frames dropped on eDP-1 (133 ms) — the busiest just then: an indexer waiting on the disk, and
an application at 92% of a core.
```

It joins three sources, none of which is an answer alone:

| Source | Knows | Does not know |
|---|---|---|
| The compositor's [frames socket](kdos-comp.md#the-frames-socket) | A frame was late, by how much, and what the compositor's own render cost | Who caused it |
| Pressure statistics | The machine was starved, and of what | By whom |
| A 500 ms `/proc` sample | Who burned processor time and who sat blocked on I/O | Whether it mattered |

The render cost separates the two explanations. When it is a large fraction of the frame budget,
the report says the desktop itself was late — the one causal claim it makes, because it has both
halves. Otherwise it reports what it measured and names who was busy, and never says "X caused
this": a half-second sample cannot prove cause, and two busy programs at once would make any such
claim wrong.

Two details:

- **Blocked comes before busy.** A process asleep in uninterruptible I/O shows almost no processor
  time while it is the one holding the disk, so sorting by processor time alone would hide it.
- **Box names come from the container supervisor's command line**, found by walking the parent
  chain, not from control groups. Without cgroup delegation a rootless container often sits in the
  root group, which names nothing. The walk costs a few file reads and no container-engine call,
  which matters on a machine that is already struggling.

`--json` prints one NDJSON record per stutter. `--fixture DIR` replays a recorded system state (two
`/proc` snapshots and an event stream) instead of watching, which is how the attribution is tested.

## kdos march

```sh
kdos march probe             # which x86-64 levels this processor supports (the default verb)
kdos march run <port>...     # build each port twice and measure
kdos march report            # the ledger
kdos march decide <baseline> <optimised> <noise%>
```

```
$ kdos march run lz4
lz4  x86-64-v3  baseline 0.321s  -march=x86-64-v3 0.302s  +5.9% (noise 16.7%) -> reverted
       the win is inside the noise; that is not a win
```

Asks whether building a port for this processor's instruction-set level (`-march=x86-64-v2`,
`-v3`, …) actually makes it faster here. `run` builds the port twice on this machine — once
normally, once with the flag — runs the port's own benchmark against both, and keeps the flag
only when the win clears both a fixed 3% floor and the noise measured on this machine.

The rules:

- A port with no `bench =` line in its recipe is **unmeasurable**, never a winner. Most ports have
  no meaningful benchmark, and assuming a win without a measurement is exactly what this command
  exists not to do.
- Each build is timed several times and the **median** is compared — not one run, which measures
  the scheduler, and not the mean, which the worst outlier owns.
- The **noise floor is measured**: it is the spread of the samples themselves.
- A recipe's `bench_setup =` command runs once per build, untimed, to prepare fixtures.

A processor with no level above baseline x86-64 has nothing to measure and `run` exits 1.

| Variable | Default | Effect |
|---|---|---|
| `KDOS_MARCH_RUNS` | `5` | Timed runs per build, at most 31 |
| `KDOS_MARCH_LEVEL` | the highest level `probe` finds | The level to measure against |
| `KDOS_MARCH_LEDGER` | `/var/lib/kdos/march.ledger` | Where verdicts are recorded |

`report` prints the ledger — kept, reverted, unmeasurable and unbuildable — with a summary line.
The reverts are listed as prominently as the wins, because they are the evidence the measuring is
real. `decide` applies the decision rule to three numbers and exits 0 for `kept`, 1 otherwise;
it exists for testing.

The argument for measuring per machine rather than choosing a tier is in
[Decisions](../01-philosophy/decisions.md).

## kdos rebuild

```sh
kdos rebuild [--dry-run] [--iso-only] <work-dir>
```

1. On a build machine, in a checkout of the repository, run `make fetch` and then
   `make build KDOS_ISO_SOURCES=1`. This makes a developer medium that carries the sources.
2. Write the ISO to a USB stick and boot it.
3. Mount a real disk with at least 25 GB free — not tmpfs and not an overlay — for example at
   `/mnt/disk`.
4. Run `kdos rebuild /mnt/disk/work`.

A second run with the same work directory reuses the tree already copied there.

Rebuilds KDOS from a booted KDOS medium, with no network at any point. It works because the
repository builds offline once `make fetch` has placed every source archive and vendor bundle in
`ports/`, because the shipped system carries the compilers and `kpkg`, and because packages are
reproducible, so the result can be compared with what it was built from.

`KDOS_ISO_SOURCES=1` copies `ports/`, `src/` and `script/` onto the medium's outer filesystem
beside the system image, under `/sources`. `make build` does not mount `fs/`, the `Makefile` or the
`Dockerfile` into the build, so none of them reaches the medium. They
cost the installed system nothing and are readable at `/mnt/iso/sources` as soon as the live system
is up. It is opt-in because it roughly doubles the image: `ports/` is about 11 GB of archives that
are already compressed. The fetch cache `ports/.srccache` is left off the medium, because each port
directory already holds its own copy of the bytes. A `SOURCES` stamp records the port count, size
and build time, and `kdos rebuild` prints it before it starts.

The sources are looked for in `$KDOS_SOURCES`, then `/mnt/iso/sources`, `/kdos` and the current
directory. The work directory is the one argument, and before anything is copied the command
refuses when:

- a build tool is missing (`cc`, `make`, `bash`, `tar`, `xz`); a missing `mksquashfs`, `xorriso` or
  `mkfs.fat` is only a note, because packages still build but no ISO comes out;
- the work directory is on tmpfs or an overlay. A live medium's root is an overlay in RAM, so a
  rebuild there reports gigabytes free, fills memory and dies hours in. A free-space check cannot
  see that;
- the work directory has less than 25 GB free.

It then copies the tree into the work directory, compiles the build orchestrator `kdosbuild` from
that copy, and runs it with `--fresh` — the same orchestrator `make build` runs, reading the same
phase scripts. `--iso-only` runs only the `06_packaging` phase. `--dry-run` stops after the checks.
See [The build system](../05-developer/build-system.md).

## kdos persist

```sh
kdos persist                                   # report
sudo kdos persist create [<device>] [--yes]    # -y also works
```

```
$ kdos persist
no persistence store

  This session's writes are in RAM and go when it is
  powered off. `kdos persist create` makes a store in
  the free space after the image on the boot medium.

$ sudo kdos persist create
  disk       /dev/sda  (32G)
  partitions 3 now; the store becomes number 4
  filesystem ext4, labelled KDOS_PERSIST
```

On the live medium, everything you write goes into the upper layer of an overlay held in RAM, and
is lost at power-off. `create` gives that upper layer a real filesystem instead: one ext4
partition, labelled `KDOS_PERSIST`, made in the free space after the last partition of the boot
medium or of the disk you name. It asks you to type `yes` unless given `--yes`, and it must run as
root.

The label is the whole interface. At boot the initramfs asks `blkid` for a filesystem with that
label and uses whichever answers, so the store can live on the boot stick, on a second stick or on
an internal disk. No path is written into any configuration file, because device names depend on
enumeration order and USB does not keep it.

The store must be a filesystem with extended attributes, hard links and `d_type`, which is why
`create` makes ext4 and why the initramfs refuses vfat, exfat and ntfs by name. overlayfs rejects an
unsuitable upper layer with the same `EINVAL` it gives for every bad mount, so without the check a
boot would quietly fall back to RAM.

Two things surprise people:

- **A store is used from the next boot**, not the one that created it. `kdos persist` then reports
  `present, NOT in use by this session` rather than claiming your work is being saved. Once it is in
  use, the report shows the free space.
- **A change that stops the desktop coming up is still there at the next boot.** Choose **KDOS Live
  (clean session)** in the boot menu: it passes `nopersist` on the kernel command line, which
  ignores the store for one boot without deleting it.

`create` appends a partition and never rewrites the partition table, because the existing entries
describe the image the machine is running from. It refuses when a store already exists or when the
system did not boot from a medium. The new partition is registered with `partx -a`, because the
kernel refuses to re-read the table of a disk with a mounted partition, and the boot medium always
has one.

## kdos clone

```sh
kdos clone                          # list what may be written to
sudo kdos clone /dev/sdb
```

```
$ kdos clone
source  /dev/sda  9.5G

  DEVICE            SIZE  MODEL
  /dev/sdb          32G   Ultra Fit
  /dev/sdc         8.0G   DataTraveler   (too small)
```

Copies the medium this system booted from onto another device and verifies the copy. It is a raw
copy of the image, so the copy boots exactly what the original boots. On an installed system there
is no boot medium mounted at `/mnt/iso`, so give a source with `--source`.

| Flag | Does |
|---|---|
| `-y`, `--yes` | Skip the confirmation, which is otherwise typing the device name |
| `--no-probe` | Skip the counterfeit-device probe, saying what was given up |
| `--no-verify` | Skip the read-back, saying what was given up |
| `--extent` | Print the image's exact byte count and stop |
| `--source <path>` | An image file or device to clone instead |

**How long the image is.** A small image written to a large stick leaves the device reporting the
large size, so copying the whole device would copy whatever was on it before. The length comes
from two records inside the image, and the larger wins:

| Record | Covers an appended partition? |
|---|---|
| The filesystem's own volume size | **No** |
| The partition table's alternate header | Yes |

On an image with an appended partition the first record stops short by exactly the size of the
boot partition, and those bytes are what make the copy boot.

**Four refusals** stand before a byte is written. A clone never writes to the medium this system
booted from, to a disk with a filesystem mounted anywhere, to anything named in `fstab`, or to
anything smaller than the image. Each is checked against the whole disk, because the target is a
whole disk while everything identifying the running system names a partition. These are wider
than the removable-media daemon's rules on purpose: that daemon chooses something to mount, and
this chooses something to destroy.

**The counterfeit probe.** A counterfeit stick reports a capacity it does not have and silently
wraps writes around, so the copy appears to succeed and the verify fails in the middle, which looks
like a broken image. When `f3probe` is installed it runs first in destructive mode (the device is
about to be overwritten anyway) and a counterfeit or damaged verdict refuses the clone.

**The verify.** The block device's cache is dropped before reading back. Otherwise the read would
return the bytes this process just wrote rather than the bytes the flash stored, which is exactly
what a counterfeit stick would get away with. A failed verify prints both hashes.

## kdos cve

```sh
kdos cve [--json] [<port>]
```

Offline vulnerability tracking: every installed package is compared against the Alpine security
database vendored into the image at `/usr/share/kdos/secdb.txt` (override with `$KDOS_SECDB`).
Name a port to check only that one. The report gives the database's date and age, each package
that is behind a recorded fix, and a summary line. Details are in
[Packaging](../03-architecture/packaging.md#vulnerability-tracking).

A package the database does not carry is **unknown**, never clean, and the summary says how many
are in that state.

| Exit | Means |
|---|---|
| 0 | Nothing known-vulnerable |
| 1 | At least one package is behind a recorded fix |
| 2 | No database at the expected path |

## kdos thumb

```sh
kdos thumb <file>...
kdos thumb --path <file>           # where its thumbnail would be
kdos thumb --ppm <file> <out.ppm>  # a small P6, for a caller with no image library
```

Makes a small picture of a file in the shared freedesktop thumbnail cache, where file managers,
image viewers and the KDOS desktop all look.

A thumbnail lives at `$XDG_CACHE_HOME/thumbnails/normal/<md5>.png`, where the hash is taken over
the file's escaped `file://` URI. The escaping and hashing come from `libkbase`, shared by every
KDOS caller, because one character escaped differently makes a thumbnail nothing else can find.

Each thumbnail carries `Thumb::URI` and `Thumb::MTime`. A reader checks them before trusting the
picture: without the modification time an edited file's old thumbnail would be served forever, and
without the URI a hash collision would go unnoticed. The file is written with mode `0600`, because
a thumbnail can reveal what a protected file contains, and renamed into place so no reader finds
half a picture.

The picture comes from a helper already on the image: `ffmpegthumbnailer` for video, `pdftoppm`
for PDF, and `magick` for every other still (JPEG, GIF, WebP, camera raw). A helper must write to
the file name it is given, which is why `exiv2` is not used: its `-ep1` picks its own output name
and extension.

`--ppm` writes a small binary PPM for a caller with no image library. `kdos-pick`'s preview pane
uses it.

## kdos places

```sh
kdos places                  # the column, one per line
kdos places add DIR [NAME]   # keep one
```

Prints the places column the desktop shows — the same list `kdos-start`, `kdos-menu` and the file
chooser (`kdos-pick`, `Ctrl+P`) show, read through the same `kxdg_places()` call.

`add` keeps a folder in that column, with an optional display name. The desktop can do this from
its context menu, but `mc` cannot, so `F2` in the file manager runs this. The path is made absolute
before it is stored, because the row is read back by programs running elsewhere.

## kdos speech

```sh
kdos speech list            # what there is, and which are here (the default verb)
kdos speech get [NAME]      # fetch one; base.en when no name is given
kdos speech where           # the directory searched, and what is in it
kdos speech remove NAME
```

Manages the speech-to-text model that `whisper-cli` and `kdos-rec`'s Transcribe button need. The
image carries `whisper-cli` but no model: models range from 32 MB to 3 GB, and language and size are
a personal choice.

| Model | Size |
|---|---|
| `tiny.en-q5_1` | 32 MB |
| `base.en-q5_1` | 60 MB |
| `tiny.en`, `tiny` | 78 MB |
| `base.en` (the default), `base` | 148 MB |
| `small.en-q5_1` | 190 MB |
| `small.en`, `small` | 488 MB |
| `medium.en` | 1.5 GB |
| `large-v3-turbo` | 1.6 GB |
| `large-v3` | 3.1 GB |

A `.en` model is English-only and better at English than the multilingual model of the same size.
The `q5_1` models are about 40% of the size for a difference most people cannot hear.

Models go into `$XDG_DATA_HOME/whisper.cpp/models`, which needs no privilege. `kdos-rec` searches
`$KDOS_WHISPER_MODEL`, then that directory, then `/usr/share/whisper.cpp/models`.

Each download is checked against a SHA-256 recorded in the source and against the model file's
magic number, and refused if either fails. Upstream serves the files over TLS and signs nothing,
so the checksum guarantees the bytes are the ones this release was written against — it does not
make the upstream trustworthy; see [the security model](../03-architecture/security-model.md). A
name is looked up in the table and never pasted into a URL, so a name cannot point the download at
another host.

## kdos remind

```sh
kdos remind in 20m tea
kdos remind at 15:30 call back
kdos remind tomorrow 9 stand-up
kdos remind ls | clear
kdos remind fire ID
kdos remind --ask            # the key binding's form
```

A toast, later. Everything after the time is the text; a `--` before it is optional.

| Form | Means |
|---|---|
| `in N<s|m|h|d>` | That long from now, at most 300 days |
| `at HH[:MM]` | The next time the clock reads that; a time already past today means tomorrow |
| `tomorrow HH[:MM]` | That time tomorrow |

`in` is capped because a timer pattern carries a month and a day but no year, so past a year the
same pattern would mean a different date.

Each reminder is a row in your personal timer table, `~/.config/kdos/timers.d/remind-<id>.timer`,
so it survives a logout and comes back with the session. `snooze` does the waiting.

- **Armed twice, delivered once.** The timer table is read only at login, so a reminder set now
  would wait for the next login. `kdos remind` therefore starts its own `snooze` as well, and the
  next login starts another from the same file. Only one delivers, because `kdos remind fire`
  removes the file before it returns and the other then finds nothing: the file is the reminder.
- **Not lost when there is nowhere to show it.** With no session and no `gdbus` when the time
  comes, nothing is delivered and the file stays.
- **The text is a comment line in the file**, because both timer parsers split a row into words
  with no quoting, so `"make tea"` would arrive as two broken words. `ls` reads the text back from
  there.
- **A missed reminder still fires.** `snooze -t` counts from the file's modification time — the
  moment the reminder was set — with a day of slack, so a reminder due while the machine was off
  fires when it comes back.

`--ask` opens a one-line prompt and takes what you type — `in 20m tea` — as the whole argument. It
exists because a key binding runs one command with no shell, so `kdos-prompt --input | kdos remind`
cannot be a binding.

`ls` and `clear` answer with a toast when there is no terminal, because the key bindings that run
them have nowhere to print.

<a id="share"></a>

## kdos share

```sh
kdos share FILE...
kdos share --clipboard
kdos share                   # asks kdos-pick for a file
```

Sends files to another machine, paired by one code word. `croc` does the transfer; this is the
desktop around it. There is no account and no server: the two ends derive a key from the code
word. The other machine receives with `croc <code>` in a terminal, and the sending window says so.

On KDOS, `/usr/bin/croc` is a wrapper that always passes `--local`, so a transfer stays on the
local network and reaches no relay. `croc-relay` is the same binary with upstream's default, for
when you mean to use the public relay. The sending window therefore shows the code word and not the
`getcroc.com` link croc prints beside it. croc is built relay-only (upstream's `croc_no_tailcat`
tag): the Tailscale transport and `croc ssh` are left out, because they run over Tailscale's public
servers and take no `--local`. `croc ssh` answers that it is not supported in this build.

Started from the desktop, `kdos-share` opens a `kdos-term` window and runs itself inside it, so croc
runs in the foreground where you can see its own progress output unchanged. The window stays open
at the end, because `kdos-term` has no `--hold` and closing would take the result with it. Typed at
a prompt, it runs where it was typed.

The code word is also drawn as a QR code, in full blocks, so a phone can scan it. It is drawn only
when the whole code fits the window, because a partial QR cannot be read. The code is fed to
`qrencode` on standard input rather than as an argument, because it is the transfer's whole secret
and a process's command line is readable by every other process.

`--clipboard` sends the clipboard as `clipboard.txt`. croc sends files, so the text is written into
your runtime directory (mode 0700, yours alone) rather than beside your documents, and it is read
once, before the window opens.

The Share row on the desktop's icons, in the file chooser (`Shift+F10`) and in `mc` (`F2`) appears
only when a program called `kdos-share` is on `PATH`. `kdos-share` is a link to this binary, and
`kdos share` is the same program under the spelling a person types.

<a id="trash"></a>

## kdos trash

```sh
kdos trash <file>...          # move to the trash
kdos trash --list             # what is in it (also -l)
kdos trash --restore <name>   # put one back where it came from
kdos trash --rm <name>        # delete one for good
kdos trash --empty [-y]       # delete all of it
```

The freedesktop trash at `~/.local/share/Trash`, from a prompt. `--empty` asks first unless given
`-y`. To browse the trash with each file's origin and deletion date, open
[`kdos-trash`](kdos-shell.md#kdos-trash), which the desktop's Trash icon opens.

This command, the desktop's `Delete` key and `kdos-trash` share one implementation in `libkbase`,
so deleting at a prompt and deleting on the desktop are the same recoverable operation. That
implementation guarantees:

- **The record is written before the file moves.** A file in the trash with no record cannot be
  restored; a stale record with no file is harmless.
- **A name is never overwritten.** A second `notes.txt` becomes `notes.txt.1`, up to `.999`, then a
  name derived from the clock. If even that is taken the call fails with `EEXIST` rather than
  destroy the earlier file and its restore record.
- **The recorded path is absolute**, so a file trashed by a relative name can be put back.
- **Paths are escaped both ways**, and a truncated escape is copied through rather than dropping a
  character.
- **A move across filesystems is refused by name.** A rename cannot cross filesystems, and a copy
  followed by a delete would have no undo, so you are told which filesystem problem you have.

## kdos hey

```sh
kdos hey list [--json]
kdos hey run <action> <id>
kdos hey outputs [--json]
kdos hey boxes [--json]
```

Asks the compositor about the desktop, so a script can find a window and act on it. It talks to
the compositor's [command socket](kdos-comp.md#the-command-socket).

| Verb | Prints or does |
|---|---|
| `list` | Every window: id, workspace, state letters, app_id, box and title (cut to 48 characters); `--json` adds geometry, pid, instance and the state flags |
| `run <action> <id>` | Runs a labwc action on the window with that id: `Close`, `Focus`, `Iconify`, `ToggleMaximize`, `ToggleShade`, `ToggleFullscreen`, `Raise` and the rest |
| `outputs` | The outputs and their scales |
| `boxes` | The boxes that currently have a window on screen |

```sh
kdos hey list                  # find the id
kdos hey run Close 42
```

Only actions that need no argument can be run this way; `Execute` and `SnapToEdge`, for example,
are refused with a reason. The box column is never truncated, because a box is named after its
pack and a cut name would read as a window with no box. The box collector uses `boxes` to ask
whether a box still has a window before stopping it.

Exit 0 on success, 1 when the compositor is unreachable or refuses the request (for example, no
window with that id), and 2 for a bad verb or an id that is not a number.

## kdos oracle

```sh
kdos oracle [--plain]
```

Prints one line from the recorded reasons that `kdos explain` reads. The choice is keyed on the day
combined with the boot, so the line stays the same all day and changes after a reboot. On a terminal
it is printed in the accent's secondary colour; `--plain` prints it without colour, which is also
what happens when the output is not a terminal. Exit 1 when no reasons are installed.

## kdos update

```sh
kdos update check [--json] [--out PATH]
kdos update apply [--dry-run] [--binhost-only] [--root DIR] [--in-place]
kdos update theme
```

Upgrades the host system. It drives `kpkg` and the A/B slot tool rather than adding a trust path of
its own; the Ed25519 signature checks stay in `kpkg`.

An update means the ports tree on this machine pins a newer recipe than what is installed. A
prebuilt package from a binary host is only a way to avoid compiling that recipe, so a machine
with no ports tree cannot update: it cannot tell what is newer, and `kpkg` cannot match a binhost
package without the recipe. `check` and `apply` exit 2 in that case. The developer medium built
with `KDOS_ISO_SOURCES=1` carries a ports tree.

| Verb | Does |
|---|---|
| `check` | Lists each package whose installed version differs from the ports tree, counts packages with no recipe here, and reports whether a binhost is configured and signed. Exit 0 up to date, 1 something behind. `--json` prints the same as a document; `--out PATH` writes that document atomically to `PATH` |
| `apply` | Installs what `check` lists, from the binhost where it has the package and from source otherwise |
| `theme` | Re-runs the theme generators for `$HOME` after an update changes the artwork |

The binhost is a directory, never a URL: set `binhost = /path/to/repo` in `/etc/kdos/update.conf`,
or `$KDOS_BINHOST`, which wins. A USB stick, an NFS mount or a local share all work. `kpkg index
<dir> --sign <key>` makes a directory into one. See
[Packaging](../03-architecture/packaging.md#the-binary-host).

`apply` options:

| Option | Does |
|---|---|
| `--dry-run` | Show what would be installed |
| `--binhost-only` | Install only what the binhost has; exit 2 when no binhost is configured |
| `--root DIR` | Install into `DIR` instead of choosing a root |
| `--in-place` | Update the running root even on an A/B machine, giving up the rollback |

On a machine with A/B root slots, `apply` installs into the inactive slot, which must be mounted.
When all packages are installed it runs `kdos-bootctl deploy` to put that slot's kernel in its
directory on the EFI system partition, then `kdos-bootctl try` to mark it as the candidate for the
next boot. A deploy that fails leaves the slot untried. When the inactive slot is not mounted,
`apply` refuses and says so, unless given `--in-place`. On a machine without A/B slots it updates
the running root. See [A/B slot selection](../03-architecture/boot-and-init.md#ab-slot-selection).

## kdos settings

```sh
kdos settings           # the grid
kdos settings hardware  # straight to a page
```

Opens `kdos-settings`, passing a page name through as `--page`. The page name is not checked here:
`kdos-settings` owns the list of pages, and a second copy would be a second list to keep in step.
When `kdos-settings` is not installed it says so and exits 127.

## The other names on this binary

`kdos` is one of twelve names of a single program, installed as `/usr/sbin/ksvc` with symbolic links
for the rest. It chooses what to do from the name it was started under, the same technique
`kdos-appbox` uses for its application shims: one binary and no shell wrapper anywhere in the
chain. Started under a name it does not know (such as `kdos-tools`, the file the build produces),
the first argument selects the tool, so `kdos-tools service list` works before the links exist.

| Name | Installed at | Is |
|---|---|---|
| `ksvc` | `/usr/sbin` | The service supervisor |
| `service` | `/usr/sbin` | The same, under the conventional name |
| `kdos-getty` | `/usr/local/sbin` | Loads the console font and palette, then runs a getty |
| `kdos-bootctl` | `/usr/bin` | The A/B slot tool — see below |
| `kdos` | `/usr/local/bin` | This page |
| `kdos-shot` | `/usr/local/bin` | Screenshots — see [kdos-shot](#kdos-shot) |
| `kdos-banner` | `/usr/local/bin` | The login banner |
| `kdos-fetch-app` | `/usr/local/bin` | Install an alien application from a network |
| `kdos-fetch-static` | `/usr/local/bin` | Fetch a single verified static binary |
| `kdos-sfx` | `/usr/local/bin` | The machine's four sounds: `login`, `notify`, `error`, `degauss` |
| `kdos-mpctl` | `/usr/local/bin` | Music control: `toggle`, `stop`, `next`, `prev`, `now`, `watch` |
| `kdos-share` | `/usr/local/bin` | [`kdos share`](#kdos-share), under the name the desktop's Share row looks for |

The package also installs the **KDOS Help** menu entry, the recorded reasons under
`/usr/share/kdos/reasons`, and the vulnerability database at `/usr/share/kdos/secdb.txt`.

### ksvc and service

```sh
service list
service status|start|stop|restart <name>
service enable|disable <name>
```

Services are the scripts in `/etc/init.d`. A name matches a script exactly first, then as a
substring, so `service start ssh` finds `20_sshd.sh`. `disable` creates
`/etc/service.disabled/<name>` and `enable` removes it. A name that is not a plain name is refused,
because a name placed into a file pattern could match anything.

The init scripts start their daemons through `ksvc supervise`:

```sh
ksvc supervise [--final-exit CODE]... <name> <command> [args...]
```

It writes `/run/<name>.pid`, forwards the daemon's output to the system log and to
`/run/kdos-svc.<name>.log`, and restarts the daemon five seconds after any exit. The exception is
an exit status named by `--final-exit` (up to eight): a status restarting cannot change, such as
thermald on an Intel model it does not know, smartd with no disk to watch, or mdadm with no array.
The supervisor then says so once, removes its pid file and exits, instead of restarting the daemon
every five seconds for as long as the machine is up. A death by signal is always restarted.

The supervisor runs in its own session and process group, so `service stop` signals the supervisor,
the daemon and the log forwarder together. A supervisor that did not lead its own group would be
killed alone, orphaning the daemon while reporting success.

### kdos-bootctl

`kdos-bootctl` manages the A/B root slots and each slot's kernel on the EFI system partition. It
rewrites the `/KDOS` entries of `limine.conf`.

| Verb | Does |
|---|---|
| `status [--json]` | The active slot, the candidate, attempts and the `BootNext` trial state (the default verb) |
| `select [<a|b>]` | Run at boot: decide which slot's root to use, given the boot-menu entry that was picked, and roll back a candidate whose trial boot was spent |
| `try <a|b> [n]` | Mark a slot as the candidate for the next boot, arming a UEFI `BootNext` trial |
| `mark-good` | Confirm the running slot after a successful boot, clearing the trial |
| `set-slot <a|b> <uuid> [<luks-uuid>]` | Record a slot's root filesystem and, when encrypted, its LUKS container |
| `crypt <fs-uuid>` | Print the LUKS container recorded for the slot whose root filesystem has that UUID; asked by the initramfs after `select`, silent with exit 1 when there is none |
| `deploy` | Copy a slot's kernel into its directory on the ESP |
| `theme` | Rewrite the theme lines of `limine.conf` |
| `palette [accent]` | Print the console palette (the `/etc/vtrgb` format) for an accent, the default one when none is named |

It is also copied into the initramfs. The full boot flow is in
[A/B slot selection](../03-architecture/boot-and-init.md#ab-slot-selection).

### kdos-shot

```sh
kdos-shot [region|screen|full|window|qr]
```

| Mode | Takes |
|---|---|
| `region` (the default) | A rectangle you drag out with the pointer; `Escape` cancels and saves nothing |
| `screen`, `full` | The whole screen |
| `window` | The same as `region` |
| `qr` | A rectangle you drag out, read as a QR code, with the decoded text copied to the clipboard |

A screenshot is saved as `~/Pictures/Screenshots/kdos-<date>-<time>.png`
(`kdos-%Y%m%d-%H%M%S.png`), or under `$XDG_PICTURES_DIR/Screenshots` when that variable is set.
`qr` writes its picture only to the runtime directory and deletes it once read, so nothing appears
under `~/Pictures`. `--geom` is refused with an error: selection on this desktop is done with the
pointer. An unknown mode prints the usage and exits 1.

### kdos-fetch-app and kdos-fetch-static

```sh
kdos-fetch-app [--box <name>] [--image <image>] <app>
kdos-fetch-app --remove [--box <name>] <app>
sudo kdos-fetch-static <name> <url> <sha256>
```

`kdos-fetch-app` installs an application into a distrobox container and exports its launcher to the
host. `--box` defaults to `kdos-debian`; `--image` defaults to `debian:stable` and is used only when
the box does not exist yet. For example:

```sh
kdos-fetch-app firefox
kdos-fetch-app --box arch --image archlinux:latest yay
```

The application name reaches the box's package manager as a separate argument and is never pasted
into a command string, so a name containing a quote or other shell characters is looked up as a
name and cannot run anything as the box's root.

`kdos-fetch-static` downloads `<url>`, checks it against `<sha256>`, and installs the binary (or,
from a tarball, the file called `<name>`) into `/usr/local/bin/<name>`. It must run as root, because it writes there. A `.tar`, `.tar.gz`,
`.tgz`, `.tar.xz`, `.tar.bz2` or `.tar.zst` URL is treated as a tarball; anything else as a plain
binary.

### kdos-mpctl

`kdos-mpctl` is what the media keys run, and one player answers each key. mpd and every MPRIS player
on the session bus are ranked by state — playing, then paused, then anything else — and the highest
takes the key, mpd winning a tie:

- a playing mpd always takes it;
- a paused mpd beats a paused MPRIS player;
- a stopped mpd yields to any MPRIS player that is playing or paused, but keeps the key over a
  stopped one;
- among MPRIS players, the first at the best rank wins.

`toggle` sends a stopped mpd `play`, because mpd's bare `pause` does nothing while stopped. MPRIS
reaches `mpv` (through the `mpv-mpris` plugin in `/etc/mpv/scripts`), `cmus` and every player in a
box, since boxes share the session bus. A player that does not answer within two seconds costs that
key press and nothing else. With no mpd and no player on the bus it prints `no player on this
login` and exits 1. `now` and `watch` read mpd only; the panel reads MPRIS itself.

`libbasu`, the D-Bus library, is loaded at run time rather than linked, because this binary is also
`ksvc`, `kdos-getty` and the `kdos-bootctl` the initramfs copies with a hand-kept library list.

## See also

- [Administration](../02-user-guide/administration.md) — these commands in the jobs they belong to
- [Command index](../06-reference/command-index.md) — every command on the system
- [kdos-comp](kdos-comp.md) — the sockets `hey` and `stutter` read
- [The daemons](daemons.md) — what `doctor` checks and `ksvc` supervises
- [Theming](../02-user-guide/theming.md) — `kdos theme` in full
- [Applications](../02-user-guide/applications.md) — `kdos app` from the user's side
