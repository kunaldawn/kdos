# The kdos command

`kdos` is the command-line front door to a KDOS machine. It answers what the machine is and what
is wrong with it, sets the accent, manages applications, keeps a live session across reboots,
upgrades the system, and gives scripts a handle on the desktop. The same binary also answers to
eleven other names, among them the service supervisor `ksvc`, the boot-slot tool `kdos-bootctl`
and the screenshot tool `kdos-shot`, and this chapter is the reference for all of them.

It is written for anyone working at a KDOS prompt, whether diagnosing a fault, changing the look
of the desktop or scripting it. If you are new to KDOS, run `kdos help`, then `kdos status` and
`kdos doctor`: those three say what the machine is, what is running and what is wrong, and
everything else here is something you reach for with a specific job in mind. Terms such as
*box*, *pack*, *accent*, *surface* and *binhost* are defined in the
[glossary](../06-reference/glossary.md). A *toast* is the transient notification popup that
`kdos-notifyd` draws. The user-facing tour of the same commands, arranged by task, is
[Administration](../02-user-guide/administration.md).

## Synopsis

```sh
kdos <subcommand> [arguments...]
kdos help [--pager]
kdos version
```

`kdos` with no subcommand is `kdos help`. `-h` and `--help` are accepted as spellings of `help`,
and `-V` of `version`. An unknown subcommand prints `unknown command '<name>' — try: kdos help`
and exits 1. Colour is used only when standard output is a terminal, so output piped into another
program is plain text.

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
| [`doctor`](#kdos-doctor) | Check the machine for the faults KDOS is known to have |
| [`explain`](#kdos-why-and-kdos-explain) | Browse the recorded reasons behind the system's design |
| [`help`](#kdos-help) | The command list and the key-binding sheet |
| [`hey`](#kdos-hey) | Ask the compositor about windows, outputs and boxes |
| [`march`](#kdos-march) | Measure whether a processor tuning flag is worth keeping |
| [`menu`](#kdos-menu) | Open the palette on a named route |
| [`notify`](#kdos-notify) | Raise a toast, or drive the notification daemon |
| [`oracle`](#kdos-oracle) | One recorded lesson, picked for today |
| [`panel`](#kdos-panel) | Put the bar away and bring it back |
| [`persist`](#kdos-persist) | Keep a live session's writes across a reboot |
| [`places`](#kdos-places) | The places column, and the way to add to it |
| [`rebuild`](#kdos-rebuild) | Rebuild KDOS from the sources on this machine, offline |
| [`remind`](#kdos-remind) | Schedule a toast for later |
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

The other names on the binary (the service supervisor, the boot-slot tool, the screenshot tool
and the rest) are covered in [The other names on this binary](#the-other-names-on-this-binary).

Tab completion in bash comes from `/usr/share/bash-completion/completions/kdos`, and it covers
less than this table does, so a subcommand that Tab does not offer can still exist. For the first
word it offers ten subcommands: `help`, `theme`, `status`, `doctor`, `version`, `app`, `share`,
`remind`, `notify` and `toggle`. After the first word it completes these arguments only:

| After | Offers |
|---|---|
| `theme` | `phosphor`, `amber`, `ice`, `bone`, `next`, `prev`, `list`, `current`; the other four accents are not offered |
| `status` | `--bar` |
| `help` | `--pager` |
| `remind` | `in`, `at`, `tomorrow`, `ls`, `clear` |
| `notify` | `--time`, `--battery` |

The same file completes the first word of [`kdos-shot`](#kdos-shot) from `region`, `screen`,
`window`, `qr` and `colour` (it omits `full`, and `colour` is a mode `kdos-shot` refuses), and
completes `kdos-share` with file names plus its `--clipboard` flag. `bash-completion` loads a
completion file on demand, by the name of the command being completed, and there is no file named
`kdos-shot` or `kdos-share`: those two complete only after `kdos` has been completed once in the
same shell.

The shipped `~/.config/kdos-comp/rc.xml` binds these commands, and some of the other names on
this binary, to keys:

| Chord | Runs |
|---|---|
| `Super+Space` | `kdos-palette`, the program `kdos menu` opens |
| `Super+Ctrl+c` / `Super+Ctrl+h` | `kdos-palette --route capture` / `--route setup` |
| `Super+Shift+Space` | `kdos panel toggle` |
| `Super+Ctrl+Alt+t` / `Super+Ctrl+Alt+b` | `kdos notify --time` / `--battery` |
| `Super+x` / `Super+Shift+x` | `kdos notify --dismiss` / `--dismiss-all` |
| `Super+Ctrl+x` / `Super+Alt+x` | `kdos notify --dnd` / `--raise` |
| `Super+Ctrl+i` | `kdos toggle stay-awake` |
| `Super+Ctrl+Shift+n` | `kdos toggle night-light` |
| `Super+Ctrl+r` / `Super+Ctrl+Alt+r` / `Super+Ctrl+Shift+r` | `kdos remind --ask` / `ls` / `clear` |
| `Print` / `Shift+Print` / `Super+Shift+p` | [`kdos-shot`](#kdos-shot) `screen` / `region` / `region` |
| The play, stop, next and previous media keys | [`kdos-mpctl`](#kdos-mpctl) `toggle` / `stop` / `next` / `prev` |

---

## Orientation

### kdos help

```sh
kdos help            # print the sheet
kdos help --pager    # the same, through $PAGER (default less)
```

The sheet has three blocks:

- **WHERE THINGS LIVE** names the three places software lives: `kpkg` for the host system
  (compiled from source against musl), `kdos app` for applications, and `kdos-box` for
  environments. Most questions of the form "how do I install X" start with which of the three X
  belongs to.
- **COMMANDS** lists the everyday verbs, including those that belong to other programs
  (`kdos-display`, `kdos-power`, `kdos-energy`, `kdos-res`, `kdos-desktop`, `kinstall`).
- **KEYS** lists the default bindings. When `kdos-keys` is installed, a first line points at
  `Super+F1`, which shows the full card generated from your own `rc.xml`. The table itself is
  fixed text, so it answers on a machine with no desktop installed.

`--pager` renders the sheet into memory and feeds it to `$PAGER` on standard input, adding `-R`
only when the pager is `less`, because another pager may not accept the flag. If the pager is not
installed or fails, the sheet is printed to standard output instead. The **KDOS Help** menu entry
runs `kdos help --pager` in a terminal.

### kdos status

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
| `Alien apps` | Non-comment lines in `/usr/share/kdos/alien-apps`, the table baked at build time |
| `Session` | `tty` when `WAYLAND_DISPLAY` is unset, `kdos-comp` when the compositor is running, `wayland (not kdos-comp)` otherwise |
| CONTAINERS | `podman ps -a`: every container, its status and image |
| EXPORTED APPS | The exported entries by name |

The alien application count reads the baked table rather than asking the container engine, so it
answers on a machine where no box has ever been created.

`--bar` prints `N box · M app`: the number of running containers and of exported entries.

### kdos version

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

The session line reads `kdos-comp` whenever `WAYLAND_DISPLAY` is set and `none` otherwise,
followed by the accent in force. It does not check that the compositor is actually running;
`kdos status` does, and reports `wayland (not kdos-comp)` when it is not.

`kdos version` does not print the release number or the commit the image was built from. The
release is `VERSION` in `/etc/os-release`.

### kdos doctor

```sh
kdos doctor [--json]
kdos doctor --cve [...]      # hands the rest of the line to kdos cve
```

Checks the faults that are known to occur on KDOS rather than running a generic health sweep.
Each check reports one of three levels:

- `ok`: checked and fine;
- `warn`: checked and wrong, with the fix where there is one ("start with: service powerd
  start", "run: kdos theme phosphor");
- `skip`: could not be checked here, with the reason. Much of the hardware section cannot be
  answered in a virtual machine (no SOF audio device, no wireless, no NVIDIA GPU, no boot
  medium), and a green line for something never tested would be as misleading as a warning that
  made every virtual machine look broken.

| Section | Checks |
|---|---|
| Kernel | The Landlock ABI level (and whether Landlock is compiled out or merely disabled), and whether the initramfs carries microcode for this processor's vendor |
| Hardware | The wireless regulatory database, its signature and the regulatory domain; SOF audio firmware and topologies; NVIDIA GSP firmware when `nouveau` is bound; whether the calling user can open each attached serial, camera and instrument device (`/dev/ttyUSB*`, `/dev/ttyACM*`, `/dev/video*`, `/dev/usbtmc*`), naming the owning group of any it cannot; and, on a live session, whether the boot medium is reachable at `/mnt/iso` |
| Boxes | Whether erofs is available to the kernel, whether `kdos-packd` answers and by which mount route, whether the user is uid 1000 (packs are built for it), whether the home directory's filesystem can hold a container's writable layer, and whether every mounted pack still has a file behind it |
| Session | Whether the compositor's socket exists (not only whether `WAYLAND_DISPLAY` is set), `XDG_RUNTIME_DIR`, and whether `kdos-comp`, `kdos-shell` and `xdg-desktop-portal-wlr` are running |
| Containers | That the mount-namespace root is `/`, and the `/etc/subuid` and `/etc/subgid` entries rootless containers need; run as root, a warning instead |
| Desktop | The accent state file, the foot theme, the KDOS scheme in `~/.config/kdeglobals`, an Xwayland socket in `/tmp/.X11-unix`, five setuid helpers (below), `/etc/subuid` and `/etc/subgid`, `kdos-powerd`, `kdos-energyd` (or the absence of any RAPL energy counter), `fcitx5` when installed, `kdos-oomd`, the compositor's frame-timing and command sockets, `kdos-keys`, the screen-capture portal and its `kdos-portals.conf`, and `~/.local/bin` on `PATH` |
| Security | Whether the `kdos` password is still the shipped default. The section appears only when `/etc/shadow` is readable, so an ordinary user gets no section rather than a check that pretends it looked |

The session check looks for the socket because a session that died leaves `WAYLAND_DISPLAY` set in
every shell that inherited it. A doctor that reported that as fine would send you looking
everywhere but at the dead compositor. The Xwayland check looks for the socket for the opposite
reason: the compositor exports `DISPLAY` only to what it starts, so a shell reached over ssh has
none while Xwayland is running.

On a live session the box check reports that the home directory is on an overlay, so a box's
writes go to memory and end with the session. To keep them, create a persistence store with
[`kdos persist`](#kdos-persist).

Losing a setuid bit is silent, and each of the five fails differently:

| Helper | Without its bit |
|---|---|
| `kdos-checkpass` | Every password at the lock screen is refused |
| `kdos-resctl` | `kdos-res` cannot end or renice a process it does not own |
| `newuidmap`, `newgidmap` | The container engine exits 125 and no box starts |
| `dbus-daemon-launch-helper` (also needs group `messagebus`) | No D-Bus system service is ever activated: the Wi-Fi supplicant, fingerprints and firmware updates among them |

| Exit | Means |
|---|---|
| 0 | No warnings |
| 1 | At least one warning |
| 2 | Unknown option |

The exit status is the same in text and JSON mode. `--json` prints
`{"checks": [{"section", "level", "message"}, …], "warnings": N}`, one record per line of the
text report.

`--cve` hands the rest of the arguments to [`kdos cve`](#kdos-cve), and the exit status is
`kdos cve`'s. The vulnerability report is a table with its own exit code and database date, and
folding it into doctor's lines would flatten "17 packages are behind a recorded fix" into one
warning.

---

## The desktop

### kdos theme

```sh
kdos theme                    # print the accent in force (also: show, current)
kdos theme <name>             # phosphor amber ice bone norton borland perfect paper
kdos theme list | next | prev
kdos theme style <file>
kdos theme --audit [accent]   # also: audit
kdos theme --preview <accent>
```

KDOS has eight accents, compiled into `libkcolor`. `bone` is the default: a machine with no
accent state file uses it. `list` marks the one in force with `*` and prints each accent's theme
name beside it; `next` and `prev` step through the table and wrap around. The user-facing guide is
[Theming](../02-user-guide/theming.md).

#### What setting an accent does

The desktop's own programs carry the palette in `libkcolor` and need only the accent's name.
Everything else a theme switch writes is for software that is not KDOS's and cannot be told.
Setting an accent regenerates those files for your user and then repaints the running desktop, in
this order:

1. The generators rewrite the GTK stylesheet, icons, cursors, the KDE colour file and the
   palette, style, icon and font keys of `~/.config/kdeglobals`, the qt5ct and qt6ct palettes
   and settings, the window-frame theme (`~/.config/kdos-comp/themerc-override`), and the
   configuration of foot, `kdos-term`, bat, micro, helix, neovim, delta, newsboat, aerc, fzf,
   tmux, btop, mc, yazi, starship and `LS_COLORS`. The files applications also write into
   (`kdeglobals`, `qt5ct.conf`, `qt6ct.conf`, mc's `ini`, `starship.toml`) are merged, keeping
   every key the theme does not own;
   [Theming](../02-user-guide/theming.md#files-you-may-edit-and-files-you-may-not) lists which.
2. The shipped wallpaper is retinted into `~/.cache/kdos/wallpaper.png`.
3. The accent's name is written to `~/.cache/kdos/theme`, atomically.
4. `SIGHUP` goes to each long-lived KDOS surface by exact name: `kdos-shell`, `kdos-desk`,
   `kdos-notifyd`, `kdos-slit`, `kdos-res`, `kdos-comp` and `kdos-term`.
5. `kdos-power accent <name>` asks `kdos-powerd` to retint the boot menu and the splash. When the
   daemon does not answer (on the live medium, or inside a box) the command prints
   `boot menu and splash unchanged` and the accent is still applied everywhere else.
6. A running tmux server re-reads its configuration.

The order matters. The wallpaper and the state file are both inputs to the signal, so both are
written before it is sent; a surface that received the signal first would re-read the accent it
already had. The state file is written atomically because the surfaces signalled in step 4 read
it the moment the signal lands, and a truncating write would show them an empty file.

The match on names is exact for two reasons. The panel, the desktop icons and the notification
daemon are three names of one binary, so signalling only `kdos-shell` would leave the desktop and
any toast in the old accent. And `kdos-desk` is a substring of `kdos-desktop-start`, the shell
script that owns the session, which would die of a signal it does not handle. A program is on the
list only if it installs a `SIGHUP` handler, because the default action of the signal is to end
the process.

Programs that are not KDOS surfaces pick up the change when they next start: foot, btop and every
application in a box. Starship picks it up at the next prompt.

Because the retinted wallpaper in `~/.cache/kdos/wallpaper.png` takes precedence over the
`wallpaper =` key in `comp.conf`, an accent switch replaces a picture of your own with the
retinted default. `wallpaper = none` is never overridden.
[Theming](../02-user-guide/theming.md#wallpaper) explains how to keep your own picture.

#### Previews

`--preview <accent>` does only steps 3 and 4: it writes the state file and sends the signal. No GTK
stylesheet, icons, cursors or foreign configuration files are generated. Those take seconds and are
read by programs that are not running, so a preview repaints every KDOS surface at once and leaves
GTK and Qt applications, boxed or native, in the old accent. It is what the arrow keys in the
`kdos-theme` picker run, and it is why that picker restores the accent it opened on unless you tell
it to keep one.

#### Style files

`kdos theme style <file>` applies a shareable look: an accent plus the settings that make a
desktop someone's own, in one flat file. Each line is `key = value` or `key: value`; whichever
separator comes first on the line wins, so `chrome_font = Terminus:pixelsize=64` keeps its colon.
Blank lines and lines starting with `#` are skipped.

```ini
accent = amber
crt = 40
crt_scanlines = 50
chrome_font = Terminus (TTF):pixelsize=32
osd.bg.color: #1a1a1a
```

| Key | Goes to |
|---|---|
| `accent` | The accent; without one, the accent in force is kept |
| `crt`, `crt_scanlines`, `crt_curve`, `crt_fullscreen`, `chrome_font`, `clock_format` | Rewritten in `~/.config/kdos/comp.conf`, keeping every other line as it was; a key the file lacks is appended. The `crt*` keys take effect on the signal when the session started with the pass on; raising `crt` from 0 takes a new login, because the pass is created at startup or not at all. `chrome_font` and `clock_format` take effect at the next login |
| Any dotted key (`osd.bg.color`, …) | A window-frame theme line, kept in `~/.config/kdos/style-themerc` and appended after the generated block every time the theme is regenerated |
| Anything else | Ignored, with a warning naming the key |

A style with no dotted keys removes `style-themerc`, because a style is a whole look rather than a
patch on the last one. The accent is then applied exactly as `kdos theme <name>` applies it.

#### Auditing

`--audit` checks that every generated file still matches the palette. It runs the same generators
with `$HOME` and the XDG directories pointed at a temporary directory and compares the result with
your files byte for byte, symbolic links included (the icon theme is mostly links). Anything that
differs, differs from what this machine's palette produces now. It writes nothing outside its
temporary directory and signals nothing.

Give it an accent (`kdos theme --audit amber`) to see what would change if you switched to that
accent, before you switch.

| Exit | Means |
|---|---|
| 0 | Every generated file matches |
| 1 | Something has drifted |
| 2 | The audit could not run |

### kdos toggle

```sh
kdos toggle                  # list them and their state
kdos toggle <name>           # flip it
kdos toggle <name> on|off    # set it
```

Three switches, each a flag file under `~/.local/state/kdos/toggles/` (or
`$XDG_STATE_HOME/kdos/toggles/`) whose presence means on. There is no file format: a toggle is a
name and whether its file exists, which a shell script, a key binding and a surface can all read.

| Name | Does | Read by |
|---|---|---|
| `stay-awake` | Never dim, lock or blank on idle | `kdos-comp`'s idle policy, each time it re-arms its idle timer |
| `night-light` | Warm the palette | Every surface, on the retint signal |
| `dnd` | Hold notifications back | `kdos-notifyd` |

An unknown name, or a second argument other than `on` or `off`, exits 2. The files and their
readers are also listed in [Configuration](../06-reference/configuration.md#localstatekdostoggles).

`night-light` is read on the same retint signal `kdos theme` sends, so the toggle writes its file
first and then sends the signal. The other two are read by their consumers without a signal.
`dnd` is the notification daemon's only Do Not Disturb flag, so the notification centre's own
button and this command change the same state.

Typed at a prompt, the new state is printed. Run from a key binding, where standard output is not
a terminal, it is shown as a one-line toast instead, because two of the three switches change
nothing visible and a silent keystroke cannot be told from a broken one. `Super+Ctrl+i` is
`stay-awake` and `Super+Ctrl+Shift+n` is `night-light`; `Super+Ctrl+n` is taken by `kdos-note`.

### kdos notify

```sh
kdos notify <summary> [body]
kdos notify --time | --battery
kdos notify --dismiss | --dismiss-all | --raise | --dnd
```

With a summary, this raises a toast, which is useful at the end of a long job:

```sh
make && kdos notify "the build finished"
```

The toast is sent through `kb_notify()` in `libkbase`, the same call a terminal makes for a
program's OSC 9 notification, so the two look and behave the same.

`--time` and `--battery` compute their text, which a key binding cannot do: `rc.xml` binds a fixed
command string. They answer the two questions a bar answers by being on screen, for a desktop
whose bar can be put away.

- `--time` shows the time as `%H:%M`, with the full date as the body.
- `--battery` shows the charge and state, plus wear when the kernel reports it (for example
  `Battery 90%` / `Discharging, 70% of its original capacity`). A battery at 90% charge can hold
  only 70% of what it held new, and someone deciding whether to unplug wants both numbers. With
  no battery it says `On mains` or `No battery`. The figures come from the kernel through
  `libkproc`, the same reader the resource monitor uses; `kdos-energyd` estimates what programs
  cost and holds no battery state.

`--dismiss`, `--dismiss-all`, `--raise` and `--dnd` send one line (`dismiss`, `dismiss all`,
`raise`, `dnd toggle`) down `$XDG_RUNTIME_DIR/kdos-notify.sock`, the socket `kdos-notifyd`
serves. The daemon owns the toasts, the history and the Do Not Disturb flag; these flags are the
command a key binding needs to reach it. When no daemon is listening they print nothing and exit
1, because a chord has nowhere to show an error.

`--raise` moves the newest entry out of the history and back onto the screen rather than copying
it, so dismissing it again does not file a second copy. It comes back without its buttons: the
notification it came from is closed, and its actions belonged to the program that sent it.

### kdos remind

```sh
kdos remind in 20m tea
kdos remind at 15:30 call back
kdos remind tomorrow 9 stand-up
kdos remind ls | clear
kdos remind fire ID
kdos remind --ask            # the key binding's form
```

Schedules a toast for a later time. Everything after the time is the text; a `--` before it is
optional.

| Form | Means |
|---|---|
| `in N<s\|m\|h\|d>` | That long from now, at most 300 days |
| `at HH[:MM]` | The next time the clock reads that; a time already past today means tomorrow |
| `tomorrow HH[:MM]` | That time tomorrow |

`in` is capped because a timer pattern carries a month and a day but no year, so past a year the
same pattern would mean a different date. Any other form is refused rather than guessed at.

Each reminder is a row in your personal timer table,
`~/.config/kdos/timers.d/remind-<id>.timer` (see
[Configuration](../06-reference/configuration.md#etckdostimersd-and-configkdostimersd)), so it
survives a logout and comes back with the session. `snooze` does the waiting.

The session reads the timer table only at login, so a reminder set now would wait for the next
login. `kdos remind` therefore starts its own `snooze` as well, and the next login starts another
from the same file. Only one delivers, because `kdos remind fire` removes the file before it
returns and the other then finds nothing: the file is the reminder. With no `gdbus` or no session
bus when the time comes, nothing is delivered and the file stays.

The text is a comment line in the file, because both timer parsers split a row into words with no
quoting, so `"make tea"` would arrive as two broken words; `ls` reads the text back from there.
`snooze -t` counts from the file's modification time, the moment the reminder was set, with a day
of slack, so a reminder due while the machine was off fires when it comes back.

`clear` removes every reminder file. An armed `snooze` is left running; when it wakes it finds no
file and delivers nothing, by the same test `fire` applies.

`--ask` opens a one-line `kdos-prompt` and takes what you type (`in 20m tea`) as the whole
argument. It exists because a key binding runs one command with no shell, so
`kdos-prompt --input | kdos remind` cannot be a binding.

`ls` (when there is at least one reminder) and `clear` answer with a toast when standard output
is not a terminal, because the key bindings that run them have nowhere to print. With no
reminders, `ls` prints `no reminders` to standard output wherever it goes.

### kdos panel

```sh
kdos panel toggle
```

Hides the bar, or brings it back. It sends `SIGUSR1` to every process whose name (`comm`) is
exactly `kdos-shell`, which reaches the panel and not the desktop icons or the notification
daemon, the other names of the same binary. It is bound to `Super+Shift+Space`. The command walks
`/proc` itself rather than running `pkill -USR1`, because toybox's `pkill` would read `-U` as a
user option and refuse the signal, so the command works whichever `pkill` is installed.

A bar put away this way stays away while the pointer crosses the bottom row, even with autohide
on. Nothing is reported when no panel is running. Any other argument exits 2.

### kdos menu

```sh
kdos menu summon <route>     # open the palette on a named place
kdos menu toggle [<route>]   # close it if it is open, else open it
```

A route is a name for a place in the system, such as `setup.network`, defined in
`/etc/kdos/menu.conf` and your own `~/.config/kdos/menu.conf`; see
[Configuration](../06-reference/configuration.md#etckdosmenuconf-merged-under-configkdosmenuconf).
A script holds a route rather than a chord, because chords can be rebound and menu rows move.

Both verbs open `kdos-palette`, the program `Super+Space` opens, with `--route <route>`, so
summoning a route is a search with the name already typed. `toggle` first runs
`pkill -x kdos-palette`; if that found a palette, it is closed and the command returns, and
otherwise a new one is opened. The match is on the exact program name, so the palette must be
started as `kdos-palette` and not through a wrapper. Exit 2 for a missing or unknown verb, 127
when `kdos-palette` is not installed.

### kdos settings

```sh
kdos settings           # the grid
kdos settings hardware  # straight to a page
```

Replaces itself with `kdos-settings`, passing a page name through as `--page`. The page name is
not checked here: `kdos-settings` owns the list of pages, and a second copy would be a second list
to keep in step. When `kdos-settings` is not installed it says so and exits 127.

### kdos places

```sh
kdos places                  # the column, one per line
kdos places add DIR [NAME]   # keep one
```

Prints the places column the desktop shows, the same list `kdos-start`, `kdos-menu` and the file
chooser (`kdos-pick`, `Ctrl+P`) show, read through the same `kxdg_places()` call. Each line is the
name and the path.

`add` keeps a folder in that column, appending `NAME = PATH` to `~/.config/kdos/places`. The name
defaults to the folder's own name and may not contain `=` or a newline. The path is made absolute
before it is stored, because the row is read back by programs with a different working directory,
so `kdos places add .` means the directory you are in. A folder that is already a place is
reported and not added twice. The desktop can do the same from its context menu; `F2` in `mc`
runs this command, because `mc` has no other way to do it.

### kdos thumb

```sh
kdos thumb <file>...
kdos thumb --path <file>           # where its thumbnail would be
kdos thumb --ppm <file> <out.ppm>  # a small P6, for a caller with no image library
```

Makes a 128-pixel picture of a file in the shared freedesktop thumbnail cache, where file
managers, image viewers and the KDOS desktop all look.

A thumbnail lives at `$XDG_CACHE_HOME/thumbnails/normal/<md5>.png`, where the hash is taken over
the file's escaped `file://` URI. The escaping and hashing come from `libkbase`, shared by every
KDOS caller, because one character escaped differently makes a thumbnail nothing else can find.

Each thumbnail carries `Thumb::URI` and `Thumb::MTime`. A reader checks them before trusting the
picture: without the modification time an edited file's old thumbnail would be served forever,
and without the URI a hash collision would go unnoticed. The file is written with mode `0600`,
because a thumbnail can reveal what a protected file contains, and renamed into place so no
reader finds half a picture.

A PNG is its own source. Every other picture comes from a helper already on the image:
`ffmpegthumbnailer` for video, `pdftoppm` for PDF, and `magick` for every other still (JPEG, GIF,
WebP, camera raw). A helper must write to the file name it is given, which is why `exiv2` is not
used: its `-ep1` picks its own output name and extension.

`--ppm` writes a small binary PPM for a caller with no image library. `kdos-pick`'s preview pane
uses it.

### kdos trash

```sh
kdos trash <file>...          # move to the trash
kdos trash --list             # what is in it (also -l)
kdos trash --restore <name>   # put one back where it came from
kdos trash --rm <name>        # delete one for good
kdos trash --empty [-y]       # delete all of it
```

Manages the freedesktop trash at `~/.local/share/Trash` from a prompt. `--list` shows each entry
with its size, deletion time and original path, newest first. `--empty` is the only irreversible
verb that acts on everything, and it asks first unless given `-y`; with no terminal to ask on, it
refuses rather than assume yes. To browse the trash on the desktop, open
[`kdos-trash`](kdos-shell.md#kdos-trash), which the desktop's Trash icon opens.

This command, the desktop and `kdos-trash` share one implementation in `libkbase`, so deleting at
a prompt and deleting on the desktop are the same recoverable operation. That implementation
guarantees:

- The record is written before the file moves. A file in the trash with no record cannot be
  restored; a stale record with no file is harmless.
- A name is never overwritten. A second `notes.txt` becomes `notes.txt.1`, up to `.999`, then a
  name derived from the clock. If even that is taken the call fails with `EEXIST` rather than
  destroy the earlier file and its restore record.
- The recorded path is absolute, so a file trashed by a relative name can be put back.
- Restoring never overwrites. When something already exists at the original path, `--restore`
  says so and leaves both files where they are.
- A move across filesystems is refused by name. A rename cannot cross filesystems, and a copy
  followed by a delete would have no undo, so you are told which problem you have.

### kdos share

```sh
kdos share FILE...
kdos share --clipboard
kdos share                   # asks kdos-pick for a file
```

Sends files to another machine, paired by one code word. `croc` does the transfer; this command is
the desktop around it. There is no account and no server: the two ends derive a key from the code
word. The other machine receives with `croc <code>` in a terminal, and the sending window says so.
Up to 32 files go in one transfer; a `file://` URI is accepted wherever a path is.

On KDOS, `/usr/bin/croc` is a wrapper that always passes `--local`, so a transfer stays on the
local network and reaches no relay. `croc-relay` is the same binary with upstream's default, for
when you mean to use the public relay. The sending window therefore shows the code word and not
the `getcroc.com` link croc prints beside it. croc is built with upstream's `croc_no_tailcat` tag,
which leaves out the Tailscale transport and `croc ssh`, because they run over Tailscale's public
servers and take no `--local`.

Started without a terminal (from the desktop's icons, the file chooser or a menu), the command
opens a terminal window (`foot`, inside a Wayland session) and runs itself inside it, so croc runs
in the foreground where its own progress output is visible unchanged. At the end that window
waits for `Enter`, so the result stays on screen. Typed at a prompt, it runs where it was typed.
Outside a Wayland session there is no terminal emulator to open, so it also runs in place.

When croc prints the code word, the command also raises a toast naming it and draws the code as a
QR code, in full blocks, so a phone can scan it. The QR is drawn only when it fits the window
whole, because a partial QR cannot be read. The code is fed to `qrencode` on standard input rather
than as an argument, because it is the transfer's whole secret and a process's command line is
readable by every other process.

`--clipboard` sends the clipboard as `clipboard.txt`. croc sends files, so the text is written to
`$XDG_RUNTIME_DIR/kdos/clipboard.txt` with mode `0600` rather than beside your documents, and it
is read once, before any window opens.

The Share row on the desktop's icons and in the file chooser (`Shift+F10`) appears only when a
program called `kdos-share` is on `PATH`; `mc`'s `F2` menu is a text file and lists it
unconditionally. `kdos-share` is a link to this binary, and
`kdos share` is the same program under the spelling a person types.

### kdos hey

```sh
kdos hey list [--json]
kdos hey run <action> <id> [--json]
kdos hey outputs [--json]
kdos hey boxes [--json]
```

Asks the compositor about the desktop, so a script can find a window and act on it. It sends one
JSON request line down the compositor's
[command socket](kdos-comp.md#the-command-socket), `$XDG_RUNTIME_DIR/kdos-cmd.sock`, and reads one
line back.

| Verb | Prints or does |
|---|---|
| `list` | Every window: id, workspace, state letters (`F` focused, `m` minimised, `M` maximised, `X` fullscreen, `s` shaded), app_id, box and title (cut to 48 characters) |
| `run <action> <id>` | Runs a labwc action (the window-action vocabulary `kdos-comp` takes from labwc, the compositor it is derived from) on the window with that id: `Close`, `Focus`, `Iconify`, `ToggleMaximize`, `ToggleShade`, `ToggleFullscreen`, `Raise` and the rest |
| `outputs` | Each output's name, size, scale and position |
| `boxes` | The boxes that currently have a window on screen |

`--json` prints the compositor's reply as it arrived. For `list` that document also carries each
window's geometry, process id and instance, which the table leaves out.

```sh
kdos hey list                  # find the id
kdos hey run Close 42
```

Only actions that need no argument can be run this way; `Execute` and `SnapToEdge`, for example,
are refused by the compositor. A window's box comes from its Wayland security context, not from
walking its process tree, and a host window shows `-`. The box column is never truncated, because
a box is named after its pack and a cut name would read as a window with no box. `kdos-box gc`
uses `boxes` to ask whether a box still has a window before stopping it.

Exit 0 on success, 1 when the compositor is unreachable or refuses the request (for example, no
window with that id), and 2 for a bad verb or an id that is not a number. With `--json` a refusal
is in the printed document and the exit status is 0; only an unreachable compositor exits 1.

---

## Applications

### kdos app

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

The command-line half of the application store, over the same catalogue the graphical store
reads (`/usr/share/kdos/appstore/catalogue`, asked through `kdos-appbox catalogue`). The user's
guide is [Applications](../02-user-guide/applications.md), and the program that does the work is
[kdos-appbox](kdos-appbox.md).

| Verb | Does |
|---|---|
| `list` | What is installed. `--all` lists the whole catalogue: 75 entries, the 73 applications and 2 datasets, which would bury the handful you have |
| `search` | Entries whose id, name, category or tagline contain the text, ignoring case; exit 1 when none do |
| `info` | One entry: name, category, state, the row it is built on, and its size labelled an estimate |
| `groups` | The catalogue's named groups, which `install`, `remove` and `export` accept in place of an id |
| `install` | Builds the application on this machine, which needs a network. `--dry-run` shows what would be built. Installing an application that is already installed rebuilds nothing: each image in its chain prints `kdos/<id> is already here`, the box create is refused because the box exists, and the command reports `<id>: the image built and the box did not` and exits non-zero. To take a newer build, `remove` the application and `install` it again, which rebuilds its own image and any runtime image no remaining box uses. There is no update verb |
| `install --pending` | Installs what the installer was asked for and could not install itself, listed in `/var/lib/kdos/apps-pending`. The command deletes the file after a clean run, but the installer writes it as root and `kdos app` runs as the user, so the deletion fails and the file stays; a second run builds the whole list again |
| `launch` | Installs the application if needed, then runs its launcher from `~/.local/bin` |
| `remove` | Uninstalls |
| `export` | Writes the named applications into one `.ktar` set for another machine |
| `import` | Installs from a `.ktar` set, offline, verified by kdos-packd as it installs each pack; name ids to take only some |

Every verb that builds, removes, exports or imports runs `kdos-appbox` as a child, so the command
line and the store's buttons cannot disagree about what installing means.

Sizes are always labelled estimates. What apt resolves on the day depends on the snapshot, and
shared runtime layers are counted once on disk however many applications use them.

Of these verbs only `import` needs `kdos-packd`, because it is the only one that mounts anything;
the others work when the daemon is down. The path is never passed to the daemon: the set is
copied into the daemon's own staging directory and only a file name inside it is handed over, so
a member of `wheel` cannot use it to mount an arbitrary device on an arbitrary directory.

#### kdos app tui

`tui` turns a terminal program into a menu entry without editing a file by hand:

```sh
kdos app tui add "Disk usage" ncdu --float --size 100x30 --icon disk-usage-analyzer
kdos app tui ls
kdos app tui rm disk-usage
```

`add` writes `~/.local/share/applications/kdos-tui-<slug>.desktop`, where the slug is the name in
lower case with every run of other characters turned into one dash. The entry has
`Terminal=true`, `X-KDOS-TUI=true`, `Icon=` (default `system-run`), `Categories=` (default
`Utility`) and, when asked, `X-KDOS-Float=true` and `X-KDOS-Size=COLSxROWS`, the two KDOS keys
that say how the window opens. `--size` must be at least `4x2`. The `Exec` line is written one
quoted field at a time, so a path with a space in it survives. Every `%` in the command is written
doubled, because a single `%` starts a field code in a desktop entry. An entry added this way
opens its program; it is not a file handler.

`rm` accepts the slug with or without the `kdos-tui-` prefix, and deletes an entry only if it
carries `X-KDOS-TUI=true`, so it cannot delete an application the image shipped. The prefix keeps
new entries from shadowing shipped ones: a file in your data directory with the same name as one
in `/usr/share/applications` would take the shipped entry off the menu.

`tui` writes only in your own data directory and needs neither `kdos-packd` nor membership of
`wheel`.

### kdos sandbox

```sh
kdos sandbox [profile] [--read P] [--write P] [--no-network] [--tcp PORT] -- <cmd> [args...]
kdos sandbox [profile] [options] --explain
```

Runs a native (not boxed) program under a filesystem and network filter built with Landlock, the
kernel's unprivileged access-control interface. It needs no
container and no root. Every option maps onto a Landlock access right; confinement Landlock
cannot enforce is not offered as an option.

| Option | Allows |
|---|---|
| `--read P` | Reading under `P` |
| `--write P` | Reading and writing under `P` |
| `--no-network` | No TCP bind or connect at all (needs Landlock ABI 4) |
| `--tcp PORT` | With `--no-network`, still allow outgoing TCP connections to `PORT`; without it the network is not restricted and this has no effect |
| `--explain` | Print what would be allowed and what Landlock cannot police (UDP, unix sockets by path, processes and IPC), and run nothing |

Everything else is refused, except a fixed base: `/usr`, `/lib`, `/lib64`, `/bin`, `/sbin`,
`/etc` and `/proc` are readable, and `/dev/null`, `/dev/zero`, `/dev/full`, `/dev/random`,
`/dev/urandom`, `/dev/tty`, `/dev/ptmx` and `/dev/pts` are writable. The rest of `/dev` is not,
because that would hand over every disk and input device. A base path this system lacks is
skipped; a path you name that does not exist is an error. At most 64 paths of each kind are
accepted.

A profile is `~/.config/kdos/sandbox/<profile>.conf`, parsed and never executed, one
`key = value` per line; a key it does not know is reported and skipped. See
[Configuration](../06-reference/configuration.md#configkdossandboxprofileconf).

```ini
read = /home/kdos/Music
write = /home/kdos/.cache/cmus
network = off
tcp-connect = 6600
```

The command is executed directly, never through a shell.

It refuses to run rather than run unconfined: with no Landlock in the kernel it exits with
`no Landlock (<reason>) — refusing to run unconfined`, and `--no-network` on a kernel below ABI 4
exits with `--no-network needs Landlock ABI 4, kernel has <n> — refusing to run with the network
open`. `--explain` reports the same conditions instead of running: it exits 1 when Landlock is
unavailable, and marks the network `REQUESTED OFF BUT NOT ENFORCED` below ABI 4. A Landlock
ruleset cannot be inspected from outside the process, so `--explain` describes what will be asked
for, not what a running program holds.

### kdos appid

```sh
kdos appid [--quiet]
```

Checks that the identifier each window presents matches the file name of an installed launcher.
A dock matches a running window to its launcher by that identifier, so a mismatch shows a second,
generic icon beside the pinned one.

The window side comes from `~/.local/share/kdos/observed-app-ids`, a ledger the compositor appends
to the first time each application maps a window. When there is no ledger yet it falls back to
the windows open now, through `kdos hey`, and says so. The ledger covers every window this
machine has shown; the fallback covers only what is on screen. The launcher side is every
`.desktop` file in `/usr/share/applications`, `/usr/local/share/applications` and
`~/.local/share/applications`.

Each observed identifier is `ok` or `MISMATCH`; a launcher never yet observed is counted as not
yet observed rather than reported as fine. `--quiet` (`-q`) leaves out the `ok` lines. The fix
for a mismatch is to rename the `.desktop` file to the observed identifier: the file name is what
is matched, not `Name` and not `StartupWMClass`. Exit 1 when a mismatch is found, 2 for a bad
option.

---

## Diagnosis

### kdos why and kdos explain

```sh
kdos why <path|port>
kdos explain [topic]
```

Both read the reasons corpus: 78 short documents installed under `/usr/share/kdos/reasons`, each
stating a constraint in the system and its consequence. `$KDOS_REASONS` names another directory,
which is how the corpus is read from a checkout. Each document starts with a header of
`title:`, `cite:`, `path:` and `port:` lines.

`why` answers what provides a path or a port and why it is configured that way. For an absolute
path it first prints the owning package and version, from the package database's own file list,
and the recipe it was built from. A path no package owns is reported as `none — provided by fs/,
or not installed`: files from the base filesystem tree are copied into the image rather than
installed by a package, so that is a common answer. It then prints in full every reason whose
`path:` names the path or a directory above it, or whose `port:` names the port.

`explain` browses the corpus. With no topic it lists every reason's title. With a topic it prints
in full each reason that mentions the topic anywhere, ignoring case, so `kdos explain musl` finds
the reasons that mention musl in passing. Exit 1 when no reasons are installed.

`testing/preflight.sh` fails when a reason's `path:` or `port:` names something the tree does not
have, so a reason cannot describe a file or a port that has gone.

### kdos oracle

```sh
kdos oracle [--plain]
```

Prints one reason from the same corpus as `title · citation`. The choice is keyed on the day
combined with the boot's identifier, so the line stays the same all day and changes after a
reboot. On a terminal it is printed in the accent's secondary colour; `--plain` prints it without
colour, which is also what happens when the output is not a terminal. The login banner shows the
same line. Exit 1 when no reasons are installed.

### kdos restarts

```sh
kdos restarts [--quiet] [--json]
```

Lists running processes that still use code an upgrade has replaced or removed:

```
These are running code that has been replaced or removed.
They keep the old version until they restart:

  pipewire         (pid 2841)  pipewire, alsa-lib
  kdos-shell       (pid 3122)  kdos-shell

2 processes. Restarting them puts the new code in effect.
```

After an upgrade, a process that had the old shared library mapped keeps running it until it
restarts. That is what makes upgrading a running system safe, and it also means a security fix is
not in effect for that process yet. This command joins two exact sources: `/proc/<pid>/maps`,
where the kernel marks a mapping whose file was deleted, and `kpkg`'s file manifest, which says
which package owned it. Only mappings under `/usr`, `/lib*`, `/bin`, `/sbin` and `/opt` are
considered, because a deleted shared-memory mapping is normal. A mapping no installed package owns
is reported as `no installed package owns it` rather than guessed at, and a process whose maps
cannot be read (another user's, or one that exited during the scan) is skipped.

The list stops at 256 processes and says so. `--quiet` (`-q`) prints nothing. `--json` prints
`{"truncated", "processes": [{"pid", "comm", "exe", "packages"}], "count"}`.

| Exit | Means |
|---|---|
| 0 | Nothing is running replaced code |
| 1 | At least one process should be restarted, or `/proc` is not mounted |
| 2 | Bad option |

### kdos stutter

```sh
kdos stutter [--json] [--fixture DIR]
```

Watches the compositor's frame timing and, whenever a frame is late, says what the machine was
doing at that moment. It runs until you stop it.

```
14:02:31  7 frames dropped on eDP-1 (133 ms)
          the compositor's own render took 2.1 ms of a 16.7 ms frame, cpu pressure 12%, io 48%
          busiest just then: tracker-miner (waiting on the disk), hugin (appbox app.hugin) (92% of a core)
```

It joins three sources, none of which is an answer alone:

| Source | Knows | Does not know |
|---|---|---|
| The compositor's [frames socket](kdos-comp.md#the-frames-socket) | A frame was late, by how much, and what the compositor's own render cost | Who caused it |
| Pressure statistics (`/proc/pressure`, the 10-second average) | The machine was starved, and of what | By whom |
| A `/proc` sample taken about every half second | Who used processor time and who sat blocked on I/O | Whether it mattered |

The render cost separates the two explanations. When it is more than 60% of the frame budget, the
report says the desktop itself was late, which is the one causal claim it makes, because it has
both halves. Otherwise it reports what it measured and names up to three processes that were busy,
and never says "X caused this": a half-second sample cannot prove cause, and two busy programs at
once would make any such claim wrong. When nothing was measurably busy it says so, pointing at a
driver stall or something too short to sample.

Blocked processes are listed before busy ones. A process asleep in uninterruptible I/O shows
almost no processor time while it is the one holding the disk, so sorting by processor time alone
would hide it. Box names come from the container supervisor, found by walking the parent chain
through `libkproc`, not from control groups. Without cgroup delegation a rootless container
often sits in the root group, which names nothing. The walk costs a few file reads and no
container-engine call, which matters on a machine that is already struggling.

`--json` prints one NDJSON record per stutter, with the output, lateness, frames dropped, render
time, frame budget, the three pressure figures and the busiest processes. `--fixture DIR` replays
a recorded system state (two `/proc` snapshots in `DIR/t0` and `DIR/t1`, and `DIR/events.jsonl`)
instead of watching, which is how the attribution is tested against `testing/fixtures/stutter`.
Exit 2 when there is no frames socket to connect to.

### kdos cve

```sh
kdos cve [--json] [<port>]
```

Offline vulnerability tracking: every installed package is compared against a pruned copy of the
Alpine security database vendored into the image at `/usr/share/kdos/secdb.txt` (override with
`$KDOS_SECDB`). Alpine is the proxy because it also builds on musl from the same upstream
releases. The comparison uses `kpkg`'s version comparator: a package is behind when its version is
older than the one Alpine records as fixing a CVE. Nothing is scanned and nothing is fetched.
Details are in [Packaging](../03-architecture/packaging.md#vulnerability-tracking).

Name a port to check only that one. On a system with no package database, the recipes in the
ports tree are checked instead. The report gives the database's date and age, each package that
is behind a recorded fix with the first three CVEs and a count of the rest, and a summary line. A
database more than 180 days old is flagged.

A package is looked up under its own name, or under the name its recipe gives in `secdb =`. One
the database does not carry is **unknown**, never clean, and the summary says how many are in
that state. `--json` prints every finding with all its CVEs.

| Exit | Means |
|---|---|
| 0 | Nothing known-vulnerable |
| 1 | At least one package is behind a recorded fix |
| 2 | No database at the expected path |

---

## The system

### kdos update

```sh
kdos update [check [--json] [--out PATH]]
kdos update apply [--dry-run] [--binhost-only] [--root DIR] [--in-place]
kdos update theme
```

Upgrades the host system. It drives `kpkg` and the A/B root-slot tool rather than adding a trust path
of its own; the Ed25519 signature checks stay in `kpkg`.

An update means the ports tree on this machine pins a newer recipe than what is installed. A
prebuilt package from a binary host is only a way to avoid compiling that recipe, so a machine
with no ports tree cannot update: it cannot tell what is newer, and `kpkg` cannot match a binhost
package without the recipe. `check` and `apply` exit 2 in that case. The developer medium built
with `KDOS_ISO_SOURCES=1` carries a ports tree; elsewhere, `PORT_REPO` can point at a checkout.
Each directory `PORT_REPO` names is searched for a port at `<name>/` and one shelf down at
`<shelf>/<name>/`, and the first directory holding a name wins. A tree that files one name twice
inside one directory, or nests a port below its shelf, stops `check`, `apply` and `kdos cve` with
both paths named, because either recipe could be the one that counts.

| Verb | Does |
|---|---|
| `check` (the default when no argument is given; name it to pass `--json` or `--out`) | Lists each package whose installed version differs from the ports tree, counts packages with no recipe here, and reports whether a binhost is configured and whether its index is signed. Exit 0 up to date, 1 something behind. `--json` prints the same as a document; `--out PATH` writes that document atomically to `PATH` and implies `--json` |
| `apply` | Installs what `check` lists, from the binhost where it has the package and from source otherwise |
| `theme` | Re-runs the theme generators for `$HOME` with the accent in force, after an update changes the artwork |

The binhost is a directory, never a URL: set `binhost = /path/to/repo` in
`/etc/kdos/update.conf`, or `$KDOS_BINHOST`, which wins. A USB stick, an NFS mount or a local
share all work. `kpkg index <dir> --sign <key>` makes a directory into one. See
[Packaging](../03-architecture/packaging.md#the-binary-host).

`apply` options:

| Option | Does |
|---|---|
| `--dry-run` | Show what would be installed, and from where |
| `--binhost-only` | Install only what the binhost has; exit 2 when no binhost is configured |
| `--root DIR` | Install into `DIR` instead of choosing a root |
| `--in-place` | Update the running root even on an A/B machine, giving up the rollback |

For each package, `kpkg binhost` is tried first. When it refuses a package (no trusted signature,
or a checksum that does not match), the run stops: a refused signature is not worked around by
compiling instead. When the binhost does not have a matching package, the package is built from
source, unless `--binhost-only` was given.

On a machine with A/B root slots, `apply` installs into the inactive slot, which must already be
mounted. When every package installed, it runs `kdos-bootctl deploy` to put that slot's kernel
in its directory on the EFI system partition (ESP), then `kdos-bootctl try` to mark it as the
candidate for the next boot. A failed package or a failed deploy leaves the slot untried. When
the inactive slot is not mounted, `apply` refuses and says so, unless given `--in-place`. On a
machine without A/B slots it updates the running root. See
[A/B slot selection](../03-architecture/boot-and-init.md#ab-slot-selection).

### kdos march

```sh
kdos march [probe]           # which x86-64 levels this processor supports (the default verb)
kdos march run <port>...     # build each port twice and measure
kdos march report            # the ledger
kdos march decide <baseline> <optimised> <noise%>
```

```
$ kdos march run lz4
lz4                      x86-64-v3  baseline 0.321s  -march=x86-64-v3 0.302s  +5.9% (noise 16.7%) -> reverted
                           the win is inside the noise; that is not a win
                           this machine's noise floor is 16.7% — close things down or raise KDOS_MARCH_RUNS
```

Asks whether building a port for this processor's instruction-set level (`-march=x86-64-v2`,
`-v3` or `-v4`) makes it faster here. `probe` reads `/proc/cpuinfo` for the flags each level
needs. `run` builds the port twice on this machine with `kpkgbuild`, once with the default
`CFLAGS` and once with the flag added, unpacks both, runs the port's own benchmark against each,
and records the verdict `kept` only when the win is more than 3% plus the noise
measured on this machine (with 8% noise, a win must exceed 11%). Nothing is installed; the verdicts go to a ledger.

The rules:

- A port with no `bench =` line in its recipe is **unmeasurable**, never a winner. Six ports carry
  one (`bzip2`, `lz4`, `lzip`, `par2cmdline-turbo`, `rdfind` and `zip`), and assuming a win
  without a measurement is what this command exists to avoid.
- Each build is timed several times and the **median** is compared: not one run, which measures
  the scheduler, and not the mean, which the worst outlier owns.
- The **noise floor is measured**: it is the spread of the samples (slowest minus fastest, as a
  share of the median), the wider of the two builds'. A floor above 5% is reported with the advice
  to quieten the machine or raise the run count.
- A recipe's `bench_setup =` command runs once per build, untimed, to prepare fixtures.
- A build that fails with the flag is recorded as `unbuildable`, which is a result rather than an
  error.

A processor with no level above baseline x86-64 has nothing to measure and `run` exits 1.

| Variable | Default | Effect |
|---|---|---|
| `KDOS_MARCH_RUNS` | `5` | Timed runs per build, from 3 to 31, rounded up to an odd number |
| `KDOS_MARCH_LEVEL` | the highest level `probe` finds | The level to measure against |
| `KDOS_MARCH_LEDGER` | `/var/lib/kdos/march.ledger` | Where verdicts are recorded |

`report` prints the ledger (kept, reverted, unmeasurable and unbuildable) with a summary line, and
exits 1 when nothing has been measured. The reverts are listed as prominently as the wins, because
they are the evidence the measuring is real. `decide` applies the decision rule to three numbers
and exits 0 for `kept`, 1 otherwise; it exists for testing.

The argument for measuring per machine rather than choosing a tier is in
[Decisions](../01-philosophy/decisions.md).

### kdos rebuild

```sh
kdos rebuild [--dry-run] [--iso-only] <work-dir>
```

Runs the KDOS build from a booted KDOS medium, with no network at any point. It rests on three
properties of the tree: the repository builds offline once `make fetch` has placed every source
archive and vendor bundle in `ports/`, the shipped system carries the compilers and `kpkg`, and
packages are reproducible, so the result can be compared with what it was built from.

The sources are looked for in `$KDOS_SOURCES`, then `/mnt/iso/sources`, `/kdos` and the current
directory; a directory counts when it has `script/kdosbuild.sh`, `ports/core`, `fs/etc` and
`src/devtools/kdosbuild`. The work directory is the one argument, and before anything is copied the
command refuses when:

- a build tool is missing (`cc`, `make`, `bash`, `tar`, `xz`); a missing `mksquashfs`, `xorriso`
  or `mkfs.fat` is only a note, because packages still build but no ISO comes out;
- the work directory is on tmpfs, ramfs or an overlay. A live medium's root is an overlay in RAM,
  so a rebuild there reports gigabytes free, fills memory and dies hours in. A free-space check
  cannot see that;
- the work directory has less than 25 GB free.

It then copies the tree to `<work-dir>/kdos` (a second run with the same work directory reuses
the copy), compiles the build orchestrator `kdosbuild` from that copy into `<work-dir>/kdosbuild`,
and runs it with `--fresh`. That is the same orchestrator `make build` runs, reading the same
phase scripts. `--iso-only` runs only the `70_image` phase. `--dry-run` stops after the
checks and prints the plan. [How KDOS is built](../05-developer/how-kdos-is-built.md) follows
the same build from `git clone` to an ISO, and [The build system](../05-developer/build-system.md)
describes the orchestrator.

A developer medium, one that carries the sources, is made and used in four steps:

1. On a build machine, in a checkout of the repository, run `make fetch` and then
   `make build KDOS_ISO_SOURCES=1`.
2. Write the ISO to a USB stick and boot it.
3. Mount a real disk with at least 25 GB free (not tmpfs and not an overlay), for example at
   `/mnt/disk`.
4. Run `kdos rebuild /mnt/disk/work`.

`KDOS_ISO_SOURCES=1` makes `script/phases/70_image/110_iso.sh` copy `ports/`, `src/`, `script/`
and `fs/` onto the medium's ISO 9660 filesystem beside the system image, under `/sources`,
together with the `Makefile`, `Dockerfile` and `CLAUDE.md` when they are present, and with the
binary host at `/sources/binhost` when the same build wrote one (`KDOS_MAKE_BINHOST=1`). The sources
cost the installed system nothing and are readable at `/mnt/iso/sources` as soon as the live
system is up. The option is off by default because a medium that carried `ports/` would roughly
double in size: `ports/` is about 39 GB of archives that are already compressed (`du -sh
ports/core` with every source fetched). The fetch cache `ports/.srccache` is left off, because
each port directory already holds its own copy of the bytes. A `SOURCES` stamp records the port
count, size and build time, and `kdos rebuild` prints it before it starts; the port count is the
number of recipes in the medium's own `ports/core`, found at `<name>/` or one shelf down.

Inside the chroot, `/kdos` is a non-recursive bind of the build container's `/workspace`, and
`script/chroot/exec.sh` binds `script/`, `src/` and `fs/` back over it and binds `ports/` at
`/ports`; the step reads each from there. The build container mounts only `build/`, `src/`, `fs/`,
`script/` and `ports/`, so the three top-level files are normally not there to copy. `kdos
rebuild` names its copy of the tree in `KDOS_WORKSPACE`, and the host environment
(`script/env/host.env`) takes `WORKSPACE` from that variable, so the bootstrap phase's file-system
step copies the overlay from the copy's `fs/` rather than from `/workspace/fs`. No rebuild from a
medium has been run through every phase; see
[Known gaps](../06-reference/known-gaps.md#an-offline-kdos-rebuild-from-the-medium-has-not-been-run-to-the-end).

### kdos persist

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
partition with no reserved blocks, labelled `KDOS_PERSIST`, made in the free space after the last
partition of the boot medium or of the disk you name. It asks you to type `yes` unless given
`--yes`, and it must run as root with `sfdisk` and `mkfs.ext4` installed.

The label is the whole interface. At boot the initramfs asks `blkid` for a filesystem with that
label and uses whichever answers, so the store can live on the boot stick, on a second stick or on
an internal disk. No path is written into any configuration file, because device names depend on
enumeration order and USB does not keep it.

The store must be a filesystem with extended attributes, hard links and `d_type`, which is why
`create` makes ext4. The initramfs refuses a store that is vfat, exfat, ntfs, iso9660 or squashfs
by name and says so on the console, because overlayfs rejects an unsuitable upper layer with the
same `EINVAL` it gives for every bad mount. A store that is refused or will not mount costs
nothing but itself: the session comes up in RAM as it would with no store. The boot side is in
[Boot and init](../03-architecture/boot-and-init.md#the-live-medium-and-persistence).

Two behaviours of the store are easy to miss:

- A store is used from the next boot, not the one that created it. `kdos persist` then
  reports `present, NOT in use by this session` rather than claiming your work is being saved.
  Once it is in use, the report shows the free space.
- A change that stops the desktop coming up is still there at the next boot. Choose **KDOS
  Live (clean session)** in the boot menu: it passes `nopersist` on the kernel command line, which
  ignores the store for one boot without deleting it.

`create` appends a partition with `sfdisk --append` and never rewrites the partition table,
because the existing entries describe the image the machine is running from. It refuses when a
store already exists, and when no device is named and the system did not boot from a medium. The
new partition is registered with `partx -a`, because the kernel refuses to re-read the table of a
disk with a mounted partition, and the boot medium always has one.

### kdos clone

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
copy of the image, so the copy boots exactly what the original boots. The source is the device
mounted at `/mnt/iso`; an installed system has none, so give one with `--source`. The listing
offers only removable or USB disks that pass the refusals below, marking any that is too small.

| Flag | Does |
|---|---|
| `-y`, `--yes` | Skip the confirmation, which is otherwise typing the device name; with no terminal and no `-y`, the clone is refused |
| `--no-probe` | Skip the counterfeit-device probe, saying what was given up |
| `--no-verify` | Skip the read-back, saying what was given up |
| `--extent` | Print the image's exact byte count and stop |
| `--source <path>` | An image file or device to clone instead |

A small image written to a large stick leaves the device reporting the large size, so copying the
whole device would copy whatever was on it before. The length comes from two records inside the
image, and the larger wins:

| Record | Covers an appended partition? |
|---|---|
| The ISO 9660 primary volume descriptor's volume size | **No** |
| The GPT header's `alternate_lba`, the position of the backup header | Yes |

On an image with an appended partition the first record stops short by exactly the size of the
EFI system partition, and those bytes are what make the copy boot. An image with neither record
is refused as not a KDOS medium.

Four refusals stand before a byte is written. A clone never writes to the medium this system
booted from, to a disk with a filesystem mounted anywhere, to a disk named by device path in
`/etc/fstab`, or to anything smaller than the image. Each is checked against the whole disk,
because the target is a whole disk while everything identifying the running system names a
partition. These rules are wider than the removable-media daemon's: that daemon chooses something
to mount, and this chooses something to destroy.

A counterfeit stick reports a capacity it does not have and silently wraps writes around, so the
copy appears to succeed and the verify fails in the middle, which looks like a broken image. When
`f3probe` is installed it runs first in destructive mode (the device is about to be overwritten
anyway), and a counterfeit or damaged verdict refuses the clone.

Before the read-back, the block device's cache is dropped (`BLKFLSBUF`). Otherwise the read
would return the bytes this process just wrote rather than the bytes the flash stored, which is
exactly what a counterfeit stick would get away with. The written and read-back SHA-256 hashes are
compared, and a failed verify prints both.

### kdos speech

```sh
kdos speech list            # what there is, which are yours and which shipped (the default verb)
kdos speech get [NAME]      # fetch one; base.en when no name is given
kdos speech where           # both directories searched, and what is in each
kdos speech remove NAME
```

Manages the speech-to-text models that `whisper-cli` and `kdos-rec`'s Transcribe button use. The
image carries `whisper-cli` and one model, `base.en`, in `/usr/share/whisper.cpp/models` (the
`whisper-model-base-en` port). The others range from 32 MB to 3.1 GB, and language and size are a
personal choice, so they are fetched rather than shipped. `list` marks a model in your own
directory `installed` and the shipped one `shipped`; `where` prints your directory, then
`/usr/share/whisper.cpp/models`, each followed by the models in it.

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
The `q5_1` models are about 40% of the size of the full model for a difference most people cannot
hear.

Models go into `$XDG_DATA_HOME/whisper.cpp/models` as `ggml-<name>.bin`, which needs no
privilege. `kdos-rec` searches `$KDOS_WHISPER_MODEL`, then that directory, then
`/usr/share/whisper.cpp/models`.

Each download is fetched with `curl` from the upstream model repository and checked against a
SHA-256 recorded in the source and against the ggml magic number, and refused if either fails. A
model already present is not fetched again. Upstream serves the files over TLS and signs nothing,
so the checksum guarantees the bytes are the ones this release was written against; it does not
make the upstream trustworthy (see [the security model](../03-architecture/security-model.md)). A
name is looked up in the table and never pasted into a URL, so a name cannot point the download at
another host. An unknown verb exits 1.

---

## The other names on this binary

`kdos` is one of twelve names of a single program, built by the `kdos-tools` package from
`src/system/kdos-tools/` and installed as `/usr/sbin/ksvc` with symbolic links for the rest. It
chooses what to do from the name it was started under, the same technique `kdos-appbox` uses for
its application shims: one binary and no shell wrapper anywhere in the chain. Started under a name
it does not know (such as `kdos-tools`, the file the build produces), the first argument selects
the tool, so `kdos-tools service list` works before the links exist.

| Name | Installed at | Is |
|---|---|---|
| `ksvc` | `/usr/sbin` | The service supervisor |
| `service` | `/usr/sbin` | The same, under the conventional name |
| `kdos-getty` | `/usr/local/sbin` | Loads the console font and palette, then runs a getty |
| `kdos-bootctl` | `/usr/bin` | The A/B slot tool |
| `kdos` | `/usr/local/bin` | This chapter |
| `kdos-shot` | `/usr/local/bin` | Screenshots |
| `kdos-banner` | `/usr/local/bin` | The login banner |
| `kdos-fetch-app` | `/usr/local/bin` | Install an application into a distrobox with its package manager and export it to the host |
| `kdos-fetch-static` | `/usr/local/bin` | Fetch a single verified static binary |
| `kdos-sfx` | `/usr/local/bin` | The machine's four sounds |
| `kdos-mpctl` | `/usr/local/bin` | Music control: `toggle`, `stop`, `next`, `prev`, `now`, `watch` |
| `kdos-share` | `/usr/local/bin` | [`kdos share`](#kdos-share), under the name the desktop's Share row looks for |

The package also installs the **KDOS Help** menu entry, the reasons corpus under
`/usr/share/kdos/reasons`, and the vulnerability database at `/usr/share/kdos/secdb.txt`.

### ksvc and service

```sh
service list
service status|start|stop|restart <name>
service enable|disable <name>
```

Services are the executable scripts `/etc/init.d/NN_<name>.sh`, and a service's name is the
script's name without the number and the `.sh`. A name matches a script exactly first, then as a
substring, so `service start ssh` finds `70_sshd.sh`. `list` prints each service, whether it
starts at boot, and the script's own status line. `restart` runs the script's `stop`, waits a
second and runs its `start`.

`disable` creates `/etc/service.disabled/<name>` and `enable` removes it; the boot scripts skip a
service with that marker (see
[Configuration](../06-reference/configuration.md#etcservicedisabledname)). The marker is named
after the argument exactly as typed, while the boot scripts look for the script's own name, so
give `enable` and `disable` the full name `service list` prints. A name that is not a plain name
(letters, digits, `_`, `-` and `.`, at most 64 characters) is refused, because a name placed into
a file pattern could match anything.

The init scripts start their daemons through `ksvc supervise`:

```sh
ksvc supervise [--final-exit CODE]... <name> <command> [args...]
```

It forks a supervisor, writes the supervisor's process id to `/run/<name>.pid`, and runs the
daemon under it. The daemon's output and the supervisor's own start and exit lines go to the
system log under the service's name and to `/run/kdos-svc.<name>.log`, which holds at most 64 KiB
with one previous generation beside it as `.old`. The file is what is left before the system
logger is up and while it restarts.

The supervisor restarts the daemon five seconds after any exit. The exception is an exit status
named by `--final-exit` (up to eight): a status restarting cannot change, such as thermald on an
Intel model it does not know, smartd with no disk to watch, or mdadm with no array. The supervisor
then says so once, removes its pid file and exits, instead of restarting the daemon every five
seconds for as long as the machine is up. A death by signal is always restarted.

The supervisor runs in its own session and process group, so `ksvc stop-supervised` (what a
script's `stop` calls) signals the supervisor, the daemon and the log forwarder together, with
`SIGKILL` after a second if `SIGTERM` was not enough. A supervisor that did not lead its own
group would be killed alone, orphaning the daemon while reporting success. The forwarder ignores
the group's `SIGTERM` and leaves when the daemon closes its output, so a stopping daemon's last
lines still reach the log. `ksvc check <name>` prints `[ OK ]` or `[DOWN]` from the pid file.

### kdos-bootctl

`kdos-bootctl` manages the A/B root slots and each slot's kernel on the EFI system partition. Its
state is `/boot/efi/EFI/kdos/bootstate`, and it rewrites the `/KDOS` entries of `limine.conf` to
match.

| Verb | Does |
|---|---|
| `status [--json]` | The active slot, the candidate, attempts and the `BootNext` trial state (the default verb) |
| `select [<a\|b>]` | Run at boot: decide which slot's root to use, given the boot-menu entry that was picked, and roll back a candidate whose trial boot was spent |
| `try <a\|b> [n]` | Mark a slot as the candidate for the next boot. With UEFI variables it arms a one-shot `BootNext` trial; otherwise the menu leads with the candidate for `n` boots (default 3). Refused for the active slot, a slot with no root, or a slot with no kernel on the ESP |
| `mark-good` | Confirm the running slot after a successful boot, clearing the trial |
| `set-slot <a\|b> <uuid> [<luks-uuid>]` | Record a slot's root filesystem and, when encrypted, its LUKS container; without one the slot is recorded as unencrypted |
| `crypt <fs-uuid>` | Print the LUKS container recorded for the slot whose root filesystem has that UUID; asked by the initramfs after `select`, silent with exit 1 when there is none |
| `deploy <root> [<a\|b>]` | Copy the kernel from a slot's root into that slot's directory on the ESP |
| `theme [--print] [<accent>]` | Rewrite the theme lines of `limine.conf` for an accent, or print them |
| `palette [<accent>]` | Print the console palette (the `/etc/vtrgb` format) for an accent, the default one when none is named |

It is also copied into the initramfs. The full boot flow is in
[A/B slot selection](../03-architecture/boot-and-init.md#ab-slot-selection).

### kdos-shot

```sh
kdos-shot [region|screen|full|window|qr]
```

| Mode | Takes |
|---|---|
| `region` (the default) | A rectangle you drag out with the pointer, through `slurp`; `Escape` cancels and saves nothing |
| `screen`, `full` | The whole screen |
| `window` | The same as `region` |
| `qr` | A rectangle you drag out, read as a QR code by `zbarimg`, with the decoded text copied to the clipboard |

The capture is taken with `grim`. A screenshot is saved as
`~/Pictures/Screenshots/kdos-<date>-<time>.png` (`kdos-%Y%m%d-%H%M%S.png`), or under
`$XDG_PICTURES_DIR/Screenshots` when that variable is set, copied to the clipboard with `wl-copy`,
announced in a toast, and its path printed. `qr` writes its picture only to the runtime directory
and deletes it once read, so nothing appears under `~/Pictures`, and its toast says only that a
code was copied, never what it says. `--geom` is refused with an error: selection on this desktop
is done with the pointer. An unknown mode prints the usage and exits 1.

### kdos-banner and kdos-getty

`kdos-banner` draws the login banner: the KDOS logo beside the output of `fastfetch --logo none`,
revealed line by line like a CRT beam, followed by the [`kdos oracle`](#kdos-oracle) line when
it fits the terminal's width. `--plain` prints the banner with no animation and without the oracle
line, so its output is the same every time and can be compared. It does not animate when standard
output is not a terminal, when `KDOS_NO_ANIM` is set, when `TERM` is `dumb`, or when the terminal
is too short for the banner, and any key skips the rest.

`kdos-getty <ttyN> <getty...>` wraps each console getty in `/etc/inittab`. It waits for the
kernel's framebuffer console to take over the terminal, loads the `ter-kdos32n` font and the
accent's sixteen colours from `/etc/vtrgb`, then runs the getty. Loading earlier would be undone,
because the take-over resets every console to the kernel's built-in font. As the last root process
on the login path, it also sets up the delegated control group and the real-time limits the
session inherits.

### kdos-fetch-app and kdos-fetch-static

```sh
kdos-fetch-app [--box <name>] [--image <image>] <app>
kdos-fetch-app --remove [--box <name>] <app>
sudo kdos-fetch-static <name> <url> <sha256>
```

`kdos-fetch-app` installs an application into a distrobox container with the box's own package
manager (`apt`, `dnf` or `pacman`) and exports it to the host: its desktop entry as a launcher,
and its command as a wrapper in `~/.local/bin` when it has one. `--box` defaults to `kdos-debian`;
`--image` defaults to `debian:stable` and is used only when the box does not exist yet.
`--remove` deletes both exports and uninstalls the package. For example:

```sh
kdos-fetch-app firefox-esr
kdos-fetch-app --box arch --image archlinux:latest neovim
```

The application name reaches the box's package manager as a separate argument and is never
pasted into a command string, so a name containing a quote or other shell characters is looked
up as a name and cannot run anything as the box's root.

`kdos-fetch-static` downloads `<url>`, checks it against `<sha256>`, and installs the binary (or,
from a tarball, the file called `<name>`) as `/usr/local/bin/<name>`. It must run as root,
because it writes there. A `.tar`, `.tar.gz`, `.tgz`, `.tar.xz`, `.tar.bz2` or `.tar.zst` URL is
treated as a tarball; anything else as a plain binary.

### kdos-sfx

```sh
kdos-sfx login | notify | error | degauss
```

The machine's four sounds, synthesised from square and triangle waves in the program itself, so
there are no sample files and no sound theme. It plays through `pw-cat` when PipeWire is running
and through `aplay` otherwise (the console case), and returns at once while a child process
plays. With no audio device, no player or no session it exits 0 without a word, because a
decoration that prints an error at every login is one people remove. `$KDOS_NO_SFX` set to
anything mutes it.

### kdos-mpctl

`kdos-mpctl` is what the media keys run, and one player answers each key. mpd (over its socket at
`$XDG_RUNTIME_DIR/mpd/socket`) and every MPRIS player on the session bus are ranked by state
(playing, then paused, then anything else) and the highest takes the key, mpd winning a tie:

- a playing mpd always takes it;
- a paused mpd beats a paused MPRIS player;
- a stopped mpd yields to any MPRIS player that is playing or paused, but keeps the key over a
  stopped one;
- among MPRIS players, the first at the best rank wins.

`toggle` sends a stopped mpd `play`, because mpd's bare `pause` does nothing while stopped. MPRIS
reaches `mpv` (through the `mpv-mpris` plugin in `/etc/mpv/scripts`), `cmus` and every player in a
box, since boxes share the session bus. A player that does not answer within two seconds costs
that key press and nothing else. With no mpd and no player on the bus it prints
`no player on this login` and exits 1. `now` prints mpd's current track, and `watch` keeps a file
the panel reads up to date through mpd's `idle` command, outliving mpd restarts; both read mpd
only, because the panel reads MPRIS itself.

`libbasu`, the D-Bus library, is loaded at run time rather than linked, because this binary is
also `ksvc`, `kdos-getty` and the `kdos-bootctl` the initramfs copies with a hand-kept library
list.

## See also

- [Administration](../02-user-guide/administration.md): these commands in the jobs they belong to
- [Command index](../06-reference/command-index.md): every command on the system
- [kdos-comp](kdos-comp.md): the sockets `hey` and `stutter` read
- [The daemons](daemons.md): what `doctor` checks and `ksvc` supervises
- [Theming](../02-user-guide/theming.md): `kdos theme` from the user's side
- [Applications](../02-user-guide/applications.md): `kdos app` from the user's side
- [kdos-appbox](kdos-appbox.md): the program `kdos app` drives
- [Boot and init](../03-architecture/boot-and-init.md): A/B slots, persistence and service scripts
- [How KDOS is built](../05-developer/how-kdos-is-built.md): the build that `kdos rebuild` reruns
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#updates): how `kdos update` and
  `kdos rebuild` compare with the way other distributions update and self-host
- [The ports catalogue](../06-reference/ports-catalogue.md): every port `kdos update` and `kdos cve`
  compare against

<!-- book-nav -->
---

*Part IV — Programs, chapter 28.* Previous: [27. kinstall](kinstall.md) · [Contents](../README.md) · Next: [29. kdos-bb](kdos-bb.md)
