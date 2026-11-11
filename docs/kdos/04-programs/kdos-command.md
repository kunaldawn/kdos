# The kdos command

`kdos` is the command-line front door to a KDOS machine. It answers what the machine is and what
is wrong with it, sets the accent and Phosphor mode, lays out the panels, manages applications and
backups, keeps a live session across reboots, updates the system, and gives scripts a handle on the
desktop. The same binary also answers to eleven other names, among them the service supervisor
`ksvc`, the boot-slot tool `kdos-bootctl` and the screenshot tool `kdos-shot`, and this chapter is
the reference for all of them.

It is written for anyone working at a KDOS prompt, whether diagnosing a fault, changing the look
of the desktop or scripting it. If you are new to KDOS, run `kdos help`, then `kdos status` and
`kdos doctor`: those three say what the machine is, what is running and what is wrong, and
everything else here is something you reach for with a specific job in mind. Terms such as
*box*, *pack*, *accent*, *desk* and *binhost* are defined in the
[glossary](../06-reference/glossary.md). The user-facing tour of the same commands, arranged by
task, is [Administration](../02-user-guide/administration.md).

## Synopsis

```sh
kdos <subcommand> [arguments...]
kdos help [--pager | --keys]
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

Every subcommand that changes the desktop does it through the program that owns the setting:
Plasma's own command-line tools and `kwriteconfig6` for configuration, KWin, plasmashell, KRunner
and kglobalaccel over the session bus, and `kdos-settingsd`, `kdos-sysupdated` and `kdos-powerd`
for what is root's. A command that wrote those files itself would be a second way in with its own
rules, and the settings page that shows the value would show something else.

## Finding a subcommand

| Subcommand | Answers |
|---|---|
| [`app`](#kdos-app) | Install, launch, remove, export and import containerised applications |
| [`appid`](#kdos-appid) | Does every open window have the launcher it claims? |
| [`backup`](#kdos-backup) | One restic snapshot, as `~/.config/kdos/backup.conf` says |
| [`checklist`](#kdos-checklist) | The first-run checklist, which ticks itself |
| [`clone`](#kdos-clone) | Copy this boot medium onto another, verified by read-back |
| [`cve`](#kdos-cve) | Offline vulnerability tracking against a vendored database |
| [`desk`](#kdos-desk) | Lay the panels out as one of the three desks, or put them back |
| [`doctor`](#kdos-doctor) | Check the machine for the faults KDOS is known to have |
| [`explain`](#kdos-why-and-kdos-explain) | Browse the recorded reasons behind the system's design |
| [`help`](#kdos-help) | The command list and the global shortcuts |
| [`hey`](#kdos-hey) | Ask KWin about windows, screens and boxes |
| [`hostname`](#kdos-timezone-hostname-locale-keymap-and-in-vm) | Print or set the machine's name |
| [`image`](#kdos-image) | Make, verify and write signed slot images |
| [`in-vm`](#kdos-timezone-hostname-locale-keymap-and-in-vm) | Is this a virtual machine? |
| [`keymap`](#kdos-timezone-hostname-locale-keymap-and-in-vm) | Print or set the console keymap |
| [`locale`](#kdos-timezone-hostname-locale-keymap-and-in-vm) | Print or set the locale |
| [`march`](#kdos-march) | Measure whether a processor tuning flag is worth keeping |
| [`menu`](#kdos-menu) | Open KRunner on a named route |
| [`notify`](#kdos-notify) | Raise a notification |
| [`oracle`](#kdos-oracle) | One recorded lesson, picked for today |
| [`panel`](#kdos-panel) | Hide the panel and bring it back |
| [`persist`](#kdos-persist) | Keep a live session's writes across a reboot |
| [`phosphor`](#kdos-phosphor) | Phosphor mode: the CRT pass, its motion and its accent |
| [`places`](#kdos-places) | Dolphin's Places, and the way to add to them |
| [`privacy`](#kdos-privacy) | Who has the microphone, the camera and the screen |
| [`rebuild`](#kdos-rebuild) | Rebuild KDOS from the sources on this machine, offline |
| [`remind`](#kdos-remind) | Schedule a notification for later |
| [`restarts`](#kdos-restarts) | Which running processes still use code an upgrade replaced |
| [`sandbox`](#kdos-sandbox) | Run a native program under a Landlock profile |
| [`settings`](#kdos-settings) | System Settings, at a page |
| [`share`](#kdos-share) | Send a file to another machine over one code word |
| [`speech`](#kdos-speech) | Transcribe, read aloud, and manage the models |
| [`status`](#kdos-status) | What this machine is and what it is running |
| [`theme`](#kdos-theme) | Set and audit the accent |
| [`thumb`](#kdos-thumb) | A file's thumbnail in the shared cache |
| [`tiling`](#kdos-tiling) | Auto-tiling on or off |
| [`timezone`](#kdos-timezone-hostname-locale-keymap-and-in-vm) | Print or set the time zone |
| [`toggle`](#kdos-toggle) | Stay-awake, night light, do-not-disturb and reduced motion |
| [`trash`](#kdos-trash) | The desktop's trash, from a prompt |
| [`update`](#kdos-update) | Check for and apply system updates across the A/B slots |
| [`verify`](#kdos-verify) | Check a file against its checksum files and its KDOS signature |
| [`version`](#kdos-version) | Release, kernel, C library, userland, session and accent |
| [`why`](#kdos-why-and-kdos-explain) | What provides a path or a port, and why it is that way |

The other names on the binary (the service supervisor, the boot-slot tool, the screenshot tool
and the rest) are covered in [The other names on this binary](#the-other-names-on-this-binary).

Tab completion in bash comes from `/usr/share/bash-completion/completions/kdos`. For the first word
it offers `help`, `theme`, `phosphor`, `desk`, `tiling`, `status`, `doctor`, `version`, `app`,
`share`, `remind`, `notify`, `toggle`, `settings`, `menu`, `panel`, `places`, `trash`, `thumb`,
`verify`, `hey`, `appid`, `privacy`, `checklist`, `backup`, `speech`, `timezone`, `hostname`, `locale`,
`keymap`, `in-vm`, `update`, `cve`, `why`, `explain`, `persist` and `restarts`, and after the first
word it completes each one's verbs and options: the accents after `theme`, the presets after
`phosphor preset`, the desks after `desk`, the toggles, the window actions after `hey run`, the
checklist items after `checklist dismiss`, the zone names under `/usr/share/zoneinfo` after
`timezone`, and the page words after `settings`. The same file completes the first word of
[`kdos-shot`](#kdos-shot) from its five modes and completes `kdos-share` with file names plus its
`--clipboard` flag. `bash-completion` loads a completion file on demand, by the name of the command
being completed, and there is no file named `kdos-shot` or `kdos-share`: those two complete only
after `kdos` has been completed once in the same shell.

Keys reach these commands through kglobalaccel like every other shortcut on the desktop. KDOS's own
is `Meta+Shift+T`, which runs `kdos tiling toggle` from
`/usr/share/kglobalaccel/org.kdos.tiling.desktop`; the screenshot keys belong to Spectacle, and
the media keys to Plasma's media controls, which reach mpd through
[`kdos-mpctl mpris`](#kdos-mpctl). Every chord, and how to change it, is in
[KWin](kwin.md#shortcuts).

---

## Orientation

### kdos help

```sh
kdos help            # print the sheet
kdos help --pager    # the same, through $PAGER (default less)
kdos help --keys     # the global shortcuts alone
```

The sheet has three blocks:

- **WHERE THINGS LIVE** names the three places software lives: `kpkg` for the host system
  (compiled from source against musl), `kdos app` for applications (a box over a pack, offline,
  or built on this machine over a network), and `kdos-box` for environments. Most questions of the form "how do I install X" start with which of the three X
  belongs to.
- **COMMANDS** lists the everyday verbs, each as it is typed, including those that belong to
  other programs (`kdos-power`, `kdos-energy`, `kdos-doc`, `kinstall`). Every line fits 80
  columns.
- **KEYS** lists the global shortcuts, read from the files kglobalaccel reads:
  `kglobalshortcutsrc` along the configuration cascade, where every component that ever
  registered left its actions with the chord in force, and the desktop entries under
  `kglobalaccel/` along `$XDG_DATA_DIRS`, which declare a program's launch key before anything has
  run. A shortcut you rebound in System Settings is in your own file and wins, so the sheet says
  what the keyboard does on this login. An action's several keys print joined by ` / `, and a
  `$`-named subgroup such as `[kwin][$InverseAction]`, which pairs an action with its opposite and
  holds no keys, is skipped. The keys are grouped by the key card's own table,
  `/usr/share/kdos/keycard-groups.conf`, so `Meta+F1` and this sheet put each key under the same
  heading. On a machine where no Plasma session has run yet, the block says that kglobalaccel
  writes the shortcuts at the first login.

`--keys` prints the KEYS block alone. Nothing in it needs a session: the sheet is most useful where
none is running, at a console or over ssh.

`--pager` renders the sheet into memory and feeds it to `$PAGER` on standard input, adding `-R`
only when the pager is `less`, because another pager may not accept the flag. If the pager is not
installed or fails, the sheet is printed to standard output instead. The **KDOS Help** menu entry
runs `kdos help --pager` in a terminal.

### Manual pages and the book

This book is on every image, as HTML under `/usr/share/doc/kdos`, and it is the first entry in the
Help Center. `kdos-doc` opens it at a page:

```sh
kdos-doc                                    # the title page
kdos-doc 04-programs/kdos-command           # a chapter
kdos-doc 04-programs/kdos-command#kdos-theme
```

A page is named as the book's own links name it, with or without `.md` or `.html`. `kdos-doc`
hands KHelpCenter that page's file URL; on a machine without KHelpCenter it opens the desktop's
browser through `xdg-open`, and on a console with neither it prints the file's path, which `lynx`
or `w3m` can open. The name is checked before it is used, letters, digits and `._-/` for the page
and the anchor and no `..`, so it can only name a page under the book's directory. It exits 1 when
the page is not in the book, 2 on bad usage, and otherwise with the viewer's status.

`man 7 kdos` says where the book is and lists its chapters. Every `kdos-*` command has a manual
page, `man kdos-appbox` for example, and so do the commands of ours without the prefix: `kpkg` and
its four other names, `ksvc` and `service` (section 8), `kinstall` and `xdg-terminal-exec`. Each
page's description is the section of the book that documents the command, so the page and the book
cannot disagree, and its one-line summary is the command's row in the
[command index](../06-reference/command-index.md); `apropos kdos` lists the `kdos-*` pages. The
`kdos-doc` port writes all of it from `docs/kdos` at build time; see
[Developing](../05-developer/developing.md#rebuilding-one-thing).

The second book, *KDOS from Scratch*, which builds KDOS phase by phase and package by package, is
on the image too: it is the Help Center's second entry, `/usr/share/doc/kfs/index.html`, or the
whole of it as `/usr/share/doc/kfs/kfs.pdf`. The `kdos-kfs` port writes both from `docs/kfs` and
the recipes.

### kdos status

```sh
kdos status
kdos status --bar            # one line: running boxes and exported applications
```

Prints what this machine is and what it is running:

| Line | From |
|---|---|
| `KDOS  Linux <release>` | The running kernel |
| `Accent` | The accent in force |
| `Packages` | Entries in `/var/lib/kpkg/db` |
| `Exported apps` | `.desktop` files in `~/.local/share/applications` |
| `Alien apps` | Non-comment lines in `/usr/share/kdos/alien-apps`, the table baked at build time |
| `Session` | elogind's answer for this process: `Plasma <version> (Wayland)` for a Plasma session, with a note when it is not the active one on its seat; the session type (`tty` on tty2); or `none (no elogind session)` for an ssh login or a shell outside any login |
| BOXES | `podman ps -a`: every container, its status and image |
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
  release  0.2.14
  kernel   7.2.11
  libc     musl
  userland GNU
  session  Plasma 6.8.2 (Wayland)
  accent   bone
```

The release is `VERSION_ID` in `/etc/os-release`, the version the update machinery compares, and
the line is left out when there is none. The session line is the same answer `kdos status` gives.
The figures above are an example of the shape.

### kdos doctor

```sh
kdos doctor [--json]
kdos doctor --cve [...]      # hands the rest of the line to kdos cve
```

Checks the faults that are known to occur on KDOS rather than running a generic health sweep.
Each check reports one of three levels:

- `ok`: checked and fine;
- `warn`: checked and wrong, with the fix where there is one ("start with: service start
  powerd");
- `skip`: could not be checked here, with the reason. Much of the hardware section cannot be
  answered in a virtual machine (no SOF audio device, no wireless, no NVIDIA GPU, no boot
  medium), and a green line for something never tested would be as misleading as a warning that
  made every virtual machine look broken.

| Section | Checks |
|---|---|
| Kernel | The Landlock ABI level (and whether Landlock is compiled out or merely disabled), and whether the initramfs carries microcode for this processor's vendor |
| Hardware | The wireless regulatory database, its signature and the regulatory domain; SOF audio firmware and topologies; NVIDIA GSP firmware when `nouveau` is bound; whether the calling user can open each attached serial, camera and instrument device (`/dev/ttyUSB*`, `/dev/ttyACM*`, `/dev/video*`, `/dev/usbtmc*`), naming the owning group of any it cannot; and, on a live session, whether the boot medium is reachable at `/mnt/iso` |
| Boxes | Whether erofs is available to the kernel, whether `kdos-packd` answers and by which mount route, whether the user is uid 1000 (packs are built for it), whether the home directory's filesystem can hold a container's writable layer, whether every mounted pack still has a file behind it; then the box plumbing (`kdos-boxsock`, `kdos-boxinit`, `xdg-dbus-proxy`) and each box's confinement as its last launch recorded it |
| Boot | Whether `rcS` finished, whether this boot is confirmed or still a trial nobody blessed, and whether SDDM has been restarting (three restarts after the first start and `kdos-dm` gives tty1 to a console login instead) |
| Session | Whether the Wayland socket exists (not only whether `WAYLAND_DISPLAY` is set), `XDG_RUNTIME_DIR`, turnstile and the supervised session bus, elogind and whether this session is the active one on its seat, and whether `kwin_wayland`, `plasmashell` and the polkit agent are running and the KDE portal, the session manager (`ksmserver`) and the secret service own their bus names; a wallet switched off (`kwalletrc` `[Wallet] Enabled=false`, as the live medium ships it) makes the secret-service line a skip |
| Services | `kdos-powerd`'s socket and `io.kdos.Power1`, `kdos-settingsd`, udisks2, `tlp-pd` when it is installed, `kdos-sysupdated` and the last update check's problem, `kdos-energyd` (a `skip` on a machine with no RAPL energy counter, most virtual machines among them), `kdos-oomd` and the protected names it lists that are not running |
| Containers | That the mount-namespace root is `/`, the `/etc/subuid` and `/etc/subgid` entries rootless containers need, and, when podman is installed, that `podman info` opens this account's store: `ok` with the store's path, or a warning with podman's own first line of complaint; run as root, a warning instead |
| Desktop | The accent and its global theme `org.kdos.<accent>.desktop`, Xwayland (its socket when `DISPLAY` is set; otherwise the binary, and `/tmp/.X11-unix` root-owned and mode 1777, without which KWin cannot start it), the lock screen's PAM stack and setuid `unix_chkpwd`, the setuid helpers (below), `fcitx5` when installed, and `~/.local/bin` on `PATH` |
| Security | Whether the `kdos` password is still the shipped default, and, when it is, whether `sshd` is enabled (`/etc/init.d/70_sshd.sh` executable and no `/etc/service.disabled/sshd`), which turns the warning into one that anyone who can reach the machine can log in. The section appears only when `/etc/shadow` is readable, so an ordinary user gets no section rather than a check that pretends it looked |

The session check looks for the socket because a session that died leaves `WAYLAND_DISPLAY` set in
every shell that inherited it. A doctor that reported that as fine would send you looking
everywhere but at the dead compositor. A name on a bus is asked, not a process listed, wherever
there is a name: a daemon that is running and owns nothing answers nobody.

On a live session the box check reports that the home directory is on an overlay, so a box's
writes go to memory and end with the session. To keep them, create a persistence store with
[`kdos persist`](#kdos-persist).

Losing a setuid bit is silent, and each of these fails differently:

| Helper | Without its bit |
|---|---|
| `unix_chkpwd` | Every password at the lock screen is refused, and the session locks you out |
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
kdos theme --from-plasma <accent>
```

KDOS has eight accents, compiled into `libkcolor`. `bone` is the default: a machine with no
accent file uses it. `list` marks the one in force with `*` and prints each accent's theme name
beside it; `next` and `prev` step through the table and wrap around. The user-facing guide is
[Theming](../02-user-guide/theming.md).

#### What setting an accent does

Everything Plasma wears was generated from `libkcolor`'s table when the image was built: a colour
scheme, a Konsole profile, a qt5ct palette, a wallpaper and a global theme, `org.kdos.<accent>.desktop`,
per accent. A switch is therefore a choice between files that exist. What is generated here is only
what no package can ship: the configuration of the terminal programs in your home. The steps, in
this order:

1. **The accent**, written first and atomically to `~/.config/kdos/accent`. `kded-kdos` compares the
   Global Theme page against exactly this file, and a half-written word would read as a foreign
   scheme.
2. **The global theme**, applied with `plasma-apply-lookandfeel -a org.kdos.<accent>.desktop -k`:
   colours, Plasma style, icons, cursor, window decoration, splash and fonts in one call. `-k` keeps
   the automatic light and dark switch, and no layout reset is asked for, so the panels are not
   touched. The light and dark pairing is moved with it: a dark accent pairs with Paper, but only
   while the dark half is still a KDOS theme.
3. **The wallpaper** of the desktop and the lock screen, over the session bus, because a global
   theme does not change a running desktop's picture. Only a desktop showing a KDOS wallpaper, or
   the theme's default, is moved.
4. **Konsole**: the profile a new tab opens in, and every open tab whose profile is one of KDOS's.
   A tab on a profile you made keeps it. QMLKonsole's colour scheme (`colorScheme` in
   `~/.config/qmlkonsolerc`) moves to the accent's `KDOS-<Title>` scheme while it names a KDOS one.
5. **The terminal programs**: bat, micro, helix, neovim, delta, newsboat, aerc, fzf, tmux, btop, mc,
   yazi, starship and `LS_COLORS`; a running tmux server re-reads its configuration.
6. **qt5ct**, for Qt 5 applications.
7. **Phosphor mode's tint**, when the effect is loaded: KWin re-reads it, and the screen degausses
   once.
8. **The machine's half**: `kdos-power accent <name>` asks [`kdos-powerd`](daemons.md#accent) to
   recolour the boot menu, the splash, the text console and the login screen, which are root's
   files. The daemon accepts the request from root and `wheel`, and from the active session where
   polkit grants it, which the shipped policy does. When it is not running or refuses, or the boot
   menu cannot be written (the live medium), the last line says what stayed as it was, and the
   accent is still applied everywhere else.

Every step leaves alone what you chose yourself: a wallpaper, a Konsole profile, a dark-theme
pairing or a qt5ct palette is moved only while it still names a KDOS file. An accent switch that
took your own wallpaper with it would be a switch nobody could use twice. Outside a Plasma session
(at a console, or over ssh) steps 2 and 3 become one write of `LookAndFeelPackage` to
`~/.config/kdeglobals`, which startplasma applies at the next login, and the command says so.
KConfig writes no key whose value `/etc/xdg/kdeglobals` already gives, so the accent the machine
already names leaves that file without the key, and startplasma reads the machine's.

The command prints `<accent> (<theme name>)` and, from step 8, a line when the machine's half was
not reached. It exits 0 when the session wears the accent, even when the machine's half was refused,
and 1 when a session step failed, with the reason as the last line.

`--from-plasma <accent>` is what `kded-kdos` runs after the Global Theme page applied a KDOS theme:
every step but the second, which Plasma has just done.

#### Style files

`kdos theme style <file>` applies a shareable look: an accent and Phosphor mode's tuning, in one
flat `key = value` file.

```ini
accent = amber
crt = 55
crt_scanlines = 60
crt_curve = 30
crt_fullscreen = true
```

| Key | Goes to |
|---|---|
| `accent` | The accent, applied as `kdos theme <name>` applies it; without one, the accent in force is kept |
| `crt`, `crt_scanlines`, `crt_curve` | `kwinrc [Effect-kdos_crt] Strength`, `Scanlines` and `Curve`, numbers |
| `crt_fullscreen` | `kwinrc [Effect-kdos_crt] Fullscreen`, `true` or `false` |
| Anything else | Ignored, with a warning naming the key |

The tuning is written whether Phosphor mode is on or not, and the effect re-reads it at once when it
is loaded. A key that silently did nothing would be a look that differs on the second machine with
nobody told.

#### Auditing

`--audit` checks three things and writes nothing outside a temporary directory:

- **The installed schemes still meet the contrast floors.** `kdos-theme contrast` is the gate the
  build ran; run again here it catches a scheme file edited or replaced after the image was made.
- **The accent's installed files are all there**, found the way KDE finds them, along
  `$XDG_DATA_DIRS`. A global theme whose colour scheme is missing applies Breeze's colours under the
  KDOS name, which looks like a broken accent rather than a missing file.
- **Your generated files are what the palette makes.** It runs the same generators with `$HOME` and
  the XDG directories pointed at a temporary directory and compares the result with your files
  byte for byte. Anything that differs, differs from what this machine's palette produces now.

Give it an accent (`kdos theme --audit amber`) to see what would change if you switched to that
accent, before you switch.

| Exit | Means |
|---|---|
| 0 | Everything matches |
| 1 | Something has drifted |
| 2 | The audit could not run |

### kdos phosphor

```sh
kdos phosphor on [--preset subtle|retro] [--terminus | --no-terminus]
kdos phosphor off
kdos phosphor status [--json]
kdos phosphor preset subtle|retro
kdos phosphor terminus on|off
kdos phosphor fullscreen on|off
kdos phosphor collapse | unfold | degauss
```

The one switch for Phosphor mode. It turns on KWin's `kdos_crt` effect, the CRT pass over the
whole screen, and the three motion effects that replace KWin's own login and log-out animations;
puts the `phosphor` accent in place of `bone` or `paper`, the two accents that do not read as a
CRT; with the Terminus option, sets the fixed font and a Terminus Konsole profile; and sets the boot
splash's motion through [`kdos-powerd`](daemons.md#splash). It is off by default for every new
user, and no key is bound to it. What the effect draws is in [KWin](kwin.md#phosphor-mode), and
what it costs in [Theming](../02-user-guide/theming.md#phosphor-mode).

**Off puts back what on found, key for key.** Before it changes anything, `on` writes
`~/.config/kdos/phosphor`: the raw value of every key it is about to change as your own file had
it, under `[Saved]`, or the key's name under `[Unset]` when the file had none. `off` writes each
back through `kwriteconfig6`, so a file KConfig last wrote comes back byte for byte. When you picked
another accent while the mode was on, `off` keeps your accent and drops only the pairing's records,
and the Konsole keys follow the accent in force.

**Software rendering is refused before anything is written.** The effect is unsupported on a
software renderer, where a full-screen pass per frame would be the desktop's whole budget, so KWin
is asked first, and a refusal of `on` leaves no state file and no key behind. The option verbs
(`preset`, `terminus`, `fullscreen`) refuse nothing: each records its option, which takes effect at
once while the mode is on and at the next `on` while it is off.

| Verb | Does |
|---|---|
| `on` | Turns the mode on with the preset (default `subtle`) and the Terminus option as asked; on a mode already on, only the options move. Prints `phosphor mode on (<preset>)` |
| `off` | Puts every key back and prints `phosphor mode off`, or `phosphor mode is already off` |
| `status` | `phosphor`, `supported` (with the reason when not), `preset` with its strength, scanlines and curve, `fullscreen`, `terminus`, `accent` (and the one before the mode), and the effect's phase |
| `preset` | `subtle` (strength 55, no scanlines, no curve) or `retro` (strength 55, scanlines 60, curve 30); a preset whose values were edited by hand reports as `custom`. Applied at once while the mode is on, recorded for the next `on` while it is off |
| `terminus` | The Terminus option alone: the font changes at once while the mode is on, and is recorded for the next `on` while it is off |
| `fullscreen` | Whether a fullscreen window is drawn through the pass (`drawn`) or exempt from it; recorded in `kwinrc` whether the mode is on or off |
| `collapse`, `unfold`, `degauss` | Plays one of the effect's motions now, through `io.kdos.Crt1` at `/io/kdos/Crt1` on KWin |

`status --json` prints one object: `state`, `on`, `supported`, `reason` (`null` when supported),
`refusal` (`"session"` or `"gpu"`, the refusals of exit statuses 2 and 3, or `null`), `preset`, `strength`, `scanlines`, `curve`, `fullscreen`, `terminus`, `accent`,
`previous_accent`, `loaded` (whether each of `kdos_crt`, `kdos_unfold`, `kdos_login` and
`kdos_quit` is loaded) and `phase`. The Look settings page and the welcome tour read it; `reason`
is the command's own sentence, and `refusal` is for a front end that says it in its own words.

| Exit | Means |
|---|---|
| 0 | Done |
| 1 | A key could not be written or put back, or the state file could not be written |
| 2 | Bad usage, or no Plasma session to ask (`phosphor mode needs a running Plasma session`) |
| 3 | The session renders in software (`phosphor mode needs GPU rendering (this session renders in software)`) |
| 4 | A motion was asked for while the mode is off |

When `kdos-powerd` does not answer, the mode still turns on and a line says
`boot splash unchanged (kdos-powerd did not answer)`.

### kdos desk

```sh
kdos desk                          # kdos, dock or tiling
kdos desk status --json
kdos desk kdos | dock | tiling     # lay the panels out again
kdos desk undo                     # put the last panels back
```

A desk is a Plasma layout template, `org.kdos.desk.<name>`, applied by plasmashell's own scripting:
every panel goes and the template lays the new ones out. The three desks are described in
[Plasma's shell](plasma-shell.md#the-desks). Because a desk throws away whatever you arranged, the
panels file is copied to two places first: the undo set in `~/.config/kdos/desk-undo`, with
`plasmashellrc`, the auto-tiling key and the desk it replaced, which `kdos desk undo` puts back
whole; and a dated copy under `~/.local/share/kdos/desk-backup/<UTC time>/`, which nothing removes.
The backup's path is the last line printed. The desk's name is written to `~/.config/kdosrc` as
`[Desk] Layout`.

A desk is the whole arrangement: the tiling desk turns auto-tiling on and the other two turn it
off. Outside a session the name is recorded and the command prints
`desk <name> is laid out at the next login`. `undo` with nothing recorded prints
`nothing to undo`, and after a restore the undo set is emptied.

`status --json` prints `{"desk", "panels_edited", "undo"}`: the desk in force, whether the panels
were changed by hand since it was laid out (a panel, its applets or their order differ from the
digest in `[Desk] LaidOutPanels`; always `false` outside a session), and the undo set's path or
`null`. `undo` puts back the digest of the desk it restores. The welcome tour
asks it before offering a desk.

### kdos tiling

```sh
kdos tiling [status] [--json]
kdos tiling on | off | toggle [--only <desktop>[,<desktop>...]]
```

Auto-tiling is Krohnkite, a KWin script, switched by its `kwinrc [Plugins] krohnkiteEnabled` key and
KWin's reconfigure, which loads or unloads scripts to match. It is off by default.
`Meta+Shift+T` runs `kdos tiling toggle`. `toggle` prints the new state; `status --json` prints
`{"tiling", "loaded"}`, the key and whether KWin has the script loaded. Outside a session the key is
written for the next login and the command exits 0.

`--only` names the virtual desktops to tile, by the names KWin gives them, and leaves the rest
floating. Krohnkite tiles every desktop except those in its `ignoreVDesktop` key, so the list
written there is every other desktop's name, which is why `--only` needs a running session to read
the names from.

### kdos toggle

```sh
kdos toggle                  # list them and their state
kdos toggle <name>           # flip it
kdos toggle <name> on|off    # set it
```

Four switches, each the Plasma surface's own state, so the applet that shows it shows what this
command set, and a click there undoes it here:

| Name | Is |
|---|---|
| `stay-awake` | An inhibition of sleep and of the screen locker, held by a process of ours for as long as the switch is on; the battery applet lists it as blocking. PowerDevil drops an inhibition the moment its holder leaves the bus, so the holder is the state, and its pid file says where it is |
| `night-light` | `kwinrc [NightColor] Active`, the key System Settings' Night Light page writes and KWin watches. On also writes `Mode=Constant`, so the screen warms at once and stays warm all day, in place of the page's schedule; off writes `Active=false`. It reads as on while KWin reports Night Light enabled and not suspended; on lifts a suspension made with KWin's own *Suspend/Resume Night Light* action, and fails, putting the suspension back, when another program is holding Night Light off. `Meta+Ctrl+Shift+N` runs `kdos toggle night-light` |
| `dnd` | `plasmanotifyrc [DoNotDisturb] Until`, the key the notification applet's own switch writes, set far ahead; deleted to switch it off |
| `reduced-motion` | `kdeglobals [KDE] AnimationDurationFactor=0` in your own file, which stops every animation KWin, Plasma and Phosphor mode draw; off deletes the key, so the shipped factor applies again |

The list prints each name, `on` or `off`, and what it is. An unknown name, or a second argument
other than `on` or `off`, exits 2. Typed at a prompt, the new state is printed. Run from a key
binding or a menu, where standard output is not a terminal, it is shown as a notification instead,
because a silent keystroke cannot be told from a broken one.

### kdos notify

```sh
kdos notify <summary> [body]
kdos notify --time | --battery | --dnd
```

With a summary, this raises a notification, which is useful at the end of a long job:

```sh
make && kdos notify "the build finished"
```

It is a `Notify` call on the session bus, sent and answered, so a command typed at a prompt waits
the moment it takes and says so when no notification server answered. It carries the
`sound-name` hint `message-new-instant`, so Plasma plays the sound theme's new-message voice with
it, unless Do Not Disturb is on or the Notifications page has sounds off. A program that must
never wait (a terminal's OSC 9 notification, a screenshot's) uses the same call fire-and-forget
through `kb_notify()` in `libkbase`, with no sound.

`--time` and `--battery` compute their text, which a shortcut cannot do: a shortcut runs a fixed
command.

- `--time` shows the time as `%H:%M`, with the full date as the body.
- `--battery` shows the charge and state, plus wear when the kernel reports it (for example
  `Battery 90%` / `Discharging, 70% of its original capacity`). A battery at 90% charge can hold
  only 70% of what it held new, and someone deciding whether to unplug wants both numbers. With
  no battery it says `On mains` or `No battery`. The figures come from the kernel through
  `libkproc`.

`--dnd` is `kdos toggle dnd`. The notification centre's own verbs (dismissing, raising, counting and
listing) belong to Plasma's notification applet, and asking for one is a usage error that says so.

### kdos remind

```sh
kdos remind in 20m tea
kdos remind at 15:30 call back
kdos remind tomorrow 9 stand-up
kdos remind ls | clear
kdos remind fire ID
kdos remind --ask            # a dialog asks for the whole line
```

Schedules a notification for a later time. Everything after the time is the text; a `--` before it
is optional.

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
survives a logout and comes back with your user services. `snooze` does the waiting.

Your user services read the timer table when they start, so a reminder set now would wait for the
next start. `kdos remind` therefore starts its own `snooze` as well, and the next start runs another
from the same file. Only one delivers, because `kdos remind fire` removes the file before it returns
and the other then finds nothing: the file is the reminder. With no session bus when the time comes,
nothing is delivered and the file stays.

The text is a comment line in the file, because both timer parsers split a row into words with no
quoting, so `"make tea"` would arrive as two broken words; `ls` reads the text back from there,
and prints each reminder's id, the time it was set (the file's modification time, not the time it
is due) and its text.
`snooze -t` counts from the file's modification time, the moment the reminder was set, with a day
of slack, so a reminder due while the machine was off fires when it comes back.

`clear` removes every reminder file. An armed `snooze` is left running; when it wakes it finds no
file and delivers nothing, by the same test `fire` applies.

`--ask` opens a one-line `kdialog` input and takes what you type (`in 20m tea`) as the whole
argument, because a shortcut runs one command with no shell.

`ls` (when there is at least one reminder) and `clear` answer with a notification when standard
output is not a terminal, because the shortcuts that run them have nowhere to print. With no
reminders, `ls` prints `no reminders` to standard output wherever it goes.

### kdos panel

```sh
kdos panel toggle
```

Hides the first panel, or brings it back, through plasmashell's own scripting, so the panel's
settings dialog shows the same state afterwards. When plasmashell does not answer it says so and
exits 1. Any other argument exits 2.

### kdos menu

```sh
kdos menu summon <route>     # KRunner on a named place
kdos menu toggle [<route>]   # the same, or KRunner's own toggle with no route
```

A route is a name for a place in the system, such as `setup.network`, defined in
`/etc/kdos/menu.conf` and your own `~/.config/kdos/menu.conf`; see
[Configuration](../06-reference/configuration.md#etckdosmenuconf-merged-under-configkdosmenuconf).
A script holds a route rather than a chord, because chords can be rebound and menu rows move.

Both verbs open [KRunner](plasma-shell.md#krunner-and-the-kdos-runner) showing the KDOS runner alone
with `route:<route>` as its query, so the one row it answers is the place and Enter goes there.
`toggle` with no route is KRunner's own show-or-hide. When KRunner does not answer the command says
so and exits 1; a missing or unknown verb exits 2.

### kdos settings

```sh
kdos settings              # System Settings
kdos settings network      # straight to a page
kdos settings kcm_kscreen  # or a module by its id
```

Replaces itself with `systemsettings`, given the module the word names. The words are a fixed map:

| Words | Module |
|---|---|
| `appearance` | `kcm_lookandfeel` |
| `look`, `accent` | `kcm_kdos_look` |
| `colors`, `fonts` | `kcm_colors`, `kcm_fonts` |
| `network`, `wifi` | `kcm_networkmanagement` |
| `bluetooth` | `kcm_bluetooth` |
| `sound`, `audio` | `kcm_pulseaudio` |
| `display`, `screens` | `kcm_kscreen` |
| `nightlight` | `kcm_nightlight` |
| `power` | `kcm_powerdevilprofilesconfig` |
| `keyboard` | `kcm_keyboard` |
| `shortcuts`, `keys` | `kcm_keys` |
| `mouse`, `touchpad` | `kcm_mouse`, `kcm_touchpad` |
| `time`, `date` | `kcm_clock` |
| `region` | `kcm_regionandlang` |
| `users` | `kcm_users` |
| `printers` | `kcm_printer_manager` |
| `notifications` | `kcm_notifications` |
| `accessibility` | `kcm_access` |
| `firewall` | `kcm_kdos_firewall` |
| `boxes` | `kcm_kdos_boxes` |
| `health`, `about` | `kcm_kdos_health` |
| `system`, `updates` | `kcm_kdos_system` |
| `backup` | `kcm_kup` |
| `autostart` | `kcm_autostart` |
| `defaults` | `kcm_componentchooser` |

A word with no row opens System Settings at its start, which is a better answer to a typo than an
error; a word that is itself a module id (`kcm_` and lower-case letters, digits and `_`) is passed
through. KDOS's own pages are described in [KDOS's settings pages](kdos-settings.md). When
`systemsettings` is not installed the command says so and exits 127.

### kdos places

```sh
kdos places                  # the places, one per line
kdos places add DIR [NAME]   # keep one
```

Prints Dolphin's Places: `$XDG_DATA_HOME/user-places.xbel`, which Dolphin's Places panel, every KDE
file dialog and Kickoff's Places tab show. Each line is the name and the path.

`add` keeps a folder there, inserting a bookmark before the file's closing tag in the shape KDE
writes its own and touching nothing else in the file. The name defaults to the folder's own name.
The path is made absolute before it is stored, so `kdos places add .` means the directory you are
in. A folder that is already a place is reported and not added twice. A home with no
`user-places.xbel` yet is refused rather than given one: KIO lays out the default places (Home,
Downloads, Trash and the rest) only into a file that does not exist, so a file created here holding
one folder would be a Places panel without them for good. `F2` in `mc` runs this command, because
`mc` cannot drag a folder into Dolphin's panel.

### kdos thumb

```sh
kdos thumb <file>...          # the cached thumbnail of each
kdos thumb --path <file>      # where its thumbnail is or would be
```

Reads the shared freedesktop thumbnail cache, where Dolphin, Gwenview, a GTK file chooser and a
script all find each other's work. A thumbnail lives at
`$XDG_CACHE_HOME/thumbnails/<size>/<md5>.png`, where the hash is taken over the file's escaped
`file://` URI. The escaping and hashing come from `libkbase`, because one character escaped
differently makes a thumbnail nothing else can find.

It writes nothing: KDE's thumbnailers fill the cache the first time a file manager or a viewer
shows a file. A thumbnail carries the file's modification time in a `Thumb::MTime` text chunk, and
one whose time differs from the file's is reported as missing rather than handed out. Exit 0 when every file
had a current thumbnail, 1 when any had none, 2 on bad usage.

### kdos trash

```sh
kdos trash <file>...          # move to the trash
kdos trash --list             # what is in it (also -l)
kdos trash --restore <name>   # put one back where it came from
kdos trash --rm <name>        # delete one for good
kdos trash --empty [-y]       # delete all of it
```

Manages the freedesktop trash at `~/.local/share/Trash` from a prompt, the same trash Dolphin shows
at `trash:/`. `--list` shows each entry with its size, deletion time and original path, newest
first. `--empty` is the only irreversible verb that acts on everything, and it asks first unless
given `-y`; with no terminal to ask on, it refuses rather than assume yes.

The implementation is `libkbase`'s, and it guarantees:

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

### kdos verify

```sh
kdos verify [--notify] [--] FILE...
```

Checks each file against what is beside it, with nothing fetched:

| Check | Where it looks | Against |
|---|---|---|
| Checksum | `<file>.sha512`, `<file>.sha256`, then `SHA512SUMS` and `SHA256SUMS` in the file's directory | The file's SHA-512 or SHA-256. A per-file sidecar's first hash line is the file's whatever name it carries, since a download renamed afterwards keeps its sidecar; a list's line must name the file. GNU (`<hash>  name`, `<hash> *name`), BSD (`SHA256 (name) = <hash>`) and a bare hash are read |
| A pack's signature | A `.kpack`'s own signature block | `/etc/kdos/keys/packs` (`$KDOS_KEYS`), after the pack's bytes are shown to hash to its own digest |
| A signature sidecar | `<file>.sig` in KDOS's `kdos-sig-1` form, which kpkg writes beside a package and a release's `SHA256SUMS` | `/etc/kdos/keys` (`$KPKG_KEYRING`). A checksum list with such a `.sig` is verified with it |

A `.sig` in any other form, such as a GPG signature, is named and not checked. Each file gets one
line: `verified` and every check that passed, `FAILED` and the check that did not, or
`not verified` and why (no sidecar, an unsigned pack, a signature this machine has no key for).
`--notify` also posts the verdict as a notification, which is how Dolphin's **Verify** action
(`/usr/share/kio/servicemenus/kdos-verify.desktop`, on every file) reports.

| Exit | Means |
|---|---|
| 0 | Every file passed at least one check and failed none |
| 1 | A check failed for at least one file |
| 2 | A file could not be read, or the command was used wrongly |
| 3 | Nothing vouches for at least one file, and none failed |

Several files answer with the worst of them, in the order 1, 2, 3, 0. A file that nothing vouches
for exits 3, never 0: unverifiable is not verified.

### kdos share

```sh
kdos share FILE...
kdos share --clipboard
kdos share                   # a file dialog asks for a file
```

Sends files to another machine, paired by one code word. `croc` does the transfer; this command is
the desktop around it. There is no account and no server: the two ends derive a key from the code
word. The other machine receives with `croc <code>` in a terminal, and the sending window says so.
Up to 32 files go in one transfer; a `file://` URI is accepted wherever a path is. With no file,
KDE's own file dialog (`kdialog`) asks for one.

On KDOS, `/usr/bin/croc` is a wrapper that always passes `--local`, so a transfer stays on the
local network and reaches no relay. `croc-relay` is the same binary with upstream's default, for
when you mean to use the public relay. The sending window therefore shows the code word and not
the `getcroc.com` link croc prints beside it. croc is built with upstream's `croc_no_tailcat` tag,
which leaves out the Tailscale transport and `croc ssh`, because they run over Tailscale's public
servers and take no `--local`. croc is also patched (`disable-update-check.patch`) so that a send
or a receive makes no request to `api.github.com` for the latest release, which `--local` alone
would not stop: a transfer makes no connection but its own. `croc update` answers, with no
request, that the system package manager updates croc, and only `croc update --check` asks GitHub.

Started without a terminal (from KRunner's `share.file` route, Dolphin's **Send with a Code
Word** action or a
menu), the command opens a
Konsole window titled **Share** and runs itself inside it, so croc runs in the foreground where its
own progress output is visible unchanged. At the end that window waits for `Enter`, so the result
stays on screen. Typed at a prompt, it runs where it was typed. Outside a Plasma session there is
no terminal emulator to open, so it also runs in place.

When croc prints the code word, the command also raises a notification naming it and draws the
code as a QR code, in full blocks, so a phone can scan it. The QR is drawn only when it fits the
window whole, because a partial QR cannot be read. The code is fed to `qrencode` on standard input
rather than as an argument, because it is the transfer's whole secret and a process's command line
is readable by every other process.

`--clipboard` sends the clipboard as `clipboard.txt`. croc sends files, so the text is written to
`$XDG_RUNTIME_DIR/kdos/clipboard.txt` with mode `0600` rather than beside your documents, and it
is read once, before any window opens.

`kdos-share` is a link to this binary, and `kdos share` is the same program under the spelling a
person types; KRunner's `share.file` and `share.clipboard` routes and `mc`'s `F2` menu run
`kdos-share`.

### kdos hey

```sh
kdos hey list [--json]
kdos hey boxes
kdos hey run <action> <id>
kdos hey outputs [--json]
```

Asks KWin about the desktop, so a script can find a window and act on it. The window list is KWin's
`org_kde_plasma_window_management` Wayland global, which names every window by a uuid and reports
its title, application id, process id, state and resource name; `kdos-tools` speaks that protocol
itself rather than through `libwayland-client`. KWin offers it to clients outside a sandbox and
refuses it to a box, so `kdos hey` works on the host and never from inside a box. The Wayland socket
is `$WAYLAND_DISPLAY`, else the first of `$XDG_RUNTIME_DIR/wayland-0` to `wayland-32` that accepts a
connection, so it answers from a timer service with no `WAYLAND_DISPLAY` too; KWin's socket is
`wayland-1`. A box's own sockets are in subdirectories of the runtime directory and are never tried.

| Verb | Prints or does |
|---|---|
| `list` | Every window: a row number (the id), `ws` (always `-`, kept as a column), state letters (`F` active, `m` minimised, `M` maximised, `X` fullscreen, `a` kept above, `-` none), app id, box and title (cut to 60 characters) |
| `boxes` | The boxes that have a window on screen, one a line |
| `run <action> <id>` | `close`, `activate`, `minimize`, `unminimize`, `maximize`, `unmaximize`, `fullscreen`, `unfullscreen`, `keep-above` or `no-keep-above` on the window with that row number or uuid |
| `outputs` | The screens, from `kscreen-doctor -o` (or `-j` with `--json`); into a pipe or a file, without the colour codes `kscreen-doctor` writes regardless |

`list --json` prints every window with its `uuid`, `pid`, `app_id`, `resource`, `box` (`null` for
a host window), `title` and the four state flags.

A window's box comes from its process: a box's cgroup scope (`app-kdos-<box>-<n>.scope`) names
the box where it has one, and otherwise the `KDOS_BOX=<box>` in the process's environment does,
when the process runs in a mount namespace other than the caller's; `kdos-appbox` sets the
variable in every box's container and podman passes it to every launch. Failing both, a window
whose app id is a running container's name, or begins with it and a dot, is that box's; the
longest such name wins, so `app.x` does not claim `app.xyz`'s windows. The box column is never truncated, because a box is
named after its pack and a cut name would read as a window with no box. `kdos-box gc` uses `boxes`
to ask whether a box still has a window before stopping it.

```sh
kdos hey list                  # find the id
kdos hey run close 3
```

Exit 0 on success, an empty answer included; 1 when there is no Plasma session to ask
(`kdos hey: there is no Plasma session to ask`), no such window, or the window closed before it
was reached; 2 for an unknown action or bad usage.

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
kdos app tui add <name> <command> [--icon NAME] [--category X]
kdos app tui rm <slug>
kdos app tui ls
```

The command-line half of the application catalogue, over the same catalogue Discover's KDOS backend
reads (`/usr/share/kdos/appstore/catalogue`, asked through `kdos-appbox catalogue`). The user's
guide is [Applications](../02-user-guide/applications.md), the program that does the work is
[kdos-appbox](kdos-appbox.md), and Discover's side is [Discover](discover.md).

| Verb | Does |
|---|---|
| `list` | What is installed: the catalogue's installed rows, then each `deb.<package>` box Discover's Debian lane built (category `Debian`, from `kdos-box list --json`; a box described and not yet created is left out). `--all` lists the whole catalogue: 72 entries, the 70 applications and 2 datasets, which would bury the handful you have, and the Debian boxes after it |
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
line and Discover cannot disagree about what installing means.

Sizes are always labelled estimates. What apt resolves on the day depends on the snapshot, and
shared runtime layers are counted once on disk however many applications use them.

Of these verbs only `import` needs `kdos-packd`, because it is the only one that mounts anything;
the others work when the daemon is down. The path is never passed to the daemon: the set is
copied into the daemon's own staging directory and only a file name inside it is handed over, so
a member of `wheel` cannot use it to mount an arbitrary device on an arbitrary directory.

#### kdos app tui

`tui` turns a terminal program into a menu entry without editing a file by hand:

```sh
kdos app tui add "Disk usage" ncdu --icon disk-usage-analyzer
kdos app tui ls
kdos app tui rm disk-usage
```

`add` writes `~/.local/share/applications/kdos-tui-<slug>.desktop`, where the slug is the name in
lower case with every run of other characters turned into one dash. The entry has
`Terminal=true`, `X-KDOS-TUI=true`, `Icon=` (default `system-run`) and `Categories=` (default
`Utility`), so the menu opens it in Konsole. The `Exec` line is written one quoted field at a time,
so a path with a space in it survives. Every `%` in the command is written doubled, because a single
`%` starts a field code in a desktop entry. An entry added this way opens its program; it is not a
file handler.

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
kdos appid
```

Checks that every open window has the launcher it claims. The Task Manager matches a window to its
launcher by file id: the window's app id, which KWin reports as the desktop file name the
application set or else its resource class, must be the basename of an installed `.desktop` entry.
A window whose app id names no entry shows as a second, unnamed icon beside the launcher it came
from, with no pin and no name, and nothing reports it, because the window works.

It asks KWin for every window open now, as [`kdos hey`](#kdos-hey) does, and looks for
`<app id>.desktop` in the applications directory of `$XDG_DATA_HOME` and of each `$XDG_DATA_DIRS`
entry, printing one line per window:

```text
<app id>	OK|MISMATCH	<title>
```

`NoDisplay=true` is a match: it keeps an entry out of the menu, not out of the Task Manager, which
resolves a window to it all the same. `Hidden=true` is the specification's "this entry is deleted"
and matches nothing. A boxed window's app id is its box's launcher id, which `kdos-appbox` names
its launchers by, so a box's windows match by construction; see
[Naming rules](kdos-appbox.md#naming-rules). The fix for a host mismatch is to name the `.desktop`
file after the app id: the file name is what is matched, not `Name` and not `StartupWMClass`.

Exit 0 with no mismatch, 1 with any, 2 when there is no session to ask.

---

## Diagnosis

### kdos why and kdos explain

```sh
kdos why <path|port>
kdos explain [topic]
```

Both read the reasons corpus: 52 short documents installed under `/usr/share/kdos/reasons`, each
stating a constraint in the system and its consequence. `$KDOS_REASONS` names another directory,
which is how the corpus is read from a checkout. Each document starts with a header of
`title:`, `cite:`, `path:` and `port:` lines.

`why` answers what provides a path or a port and why it is configured that way. For an absolute
path it first prints the owning package and version, from the package database's own file list,
and the recipe it was built from. A path no package owns is reported as `none — provided by fs/
or 10_bootstrap, or not installed`: files from the base filesystem tree are copied into the image
rather than installed by a package, and `10_bootstrap` compiles `kpkg` (with its four other names
and `/etc/kpkg.conf`) and `kinstall` straight into the root, so that is a common answer. It then
prints in full every reason whose `path:` names the path or a directory above it, or whose `port:`
names the port.

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
colour, which is also what happens when the output is not a terminal. The animated
[login banner](#kdos-banner-and-kdos-getty) shows the same line when the terminal has room for
it. Exit 1 when no reasons are installed.

### kdos restarts

```sh
kdos restarts [--quiet] [--json]
```

Lists running processes that still use code an upgrade has replaced or removed:

```
These are running code that has been replaced or removed.
They keep the old version until they restart:

  pipewire         (pid 2841)  pipewire, alsa-lib
  plasmashell      (pid 3122)  plasma-workspace

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

### kdos privacy

```sh
kdos privacy                    # who has what, for a person
kdos privacy --json             # one line, now
kdos privacy --watch --json     # that line, and again at every change
kdos privacy mute|unmute <id>   # a microphone stream
kdos privacy stop <id>          # a camera or screen capture
```

Says who has the microphone, the camera and the screen. The answer is PipeWire's, plus the
kernel's for a camera opened without it:

| Device | In use when |
|---|---|
| Microphone | A running `Stream/Input/Audio` node exists, named by the process that owns it |
| Camera | A camera source (`v4l2` or `libcamera`) has an active link, named by the stream at the other end; or a process holds a `/dev/video*` open itself, which PipeWire never sees |
| Screen | A KWin screencast stream (a `Stream/Output/Video` node owned by `kwin_wayland`), whether Spectacle or the desktop portal asked for it, or any other non-camera video source, has an active link, named by the stream that reads it. Another program's video output stream is not listed |

Each user is named with the box it runs in, from its cgroup, because "Firefox has the microphone"
is a different sentence when Firefox is a box kept for one website. `--watch` follows
`pw-dump --monitor` rather than polling it, and runs the camera's file-descriptor scan on inotify:
an open or close of a `/dev/video*` node, or one appearing in `/dev`, rescans, and the scan runs
again a quarter second later for an opener whose descriptor was not yet listed. Where inotify
cannot be set up the scan runs every half second instead. With no `/dev/video*` node the scan
returns at once, so a machine with no camera never walks `/proc`. The tray's privacy lamps read it
(see [Privacy lamps](plasma-shell.md#privacy-lamps)).

`stop` ends a screen capture by destroying KWin's stream node for it: KWin closes the screencast,
so the program that asked is told — Spectacle stops and reports that the stream closed, and a
program sharing the screen through the portal sees its session end. A camera, or any other video
source, is shared, so `stop` destroys only the link that feeds the one stream; its frames stop, and
the program is not told. A camera opened without PipeWire cannot be stopped from here.

Exit 0; 1 when PipeWire cannot be read or an action failed, with the reason on standard error; 2
on bad usage, which the tray takes as "stop retrying".

### kdos cve

```sh
kdos cve [--json] [<port>]
```

Offline vulnerability tracking: every installed package is compared against a pruned copy of the
Alpine security database vendored into the image at `/usr/share/kdos/secdb.txt` (override with
`$KDOS_SECDB`). Alpine is the proxy because it also builds on musl from the same upstream
releases. The comparison uses `kpkg`'s version comparator: a package is behind when its version is
older than the one Alpine records as fixing a CVE on the version's own branch, or, for a CVE that
branch has no row for, older than every recorded fix. A CVE that a later branch also fixed is not
counted against a version whose own release already closed it. Nothing is scanned and nothing is
fetched.
Details are in [Packaging](../03-architecture/packaging.md#vulnerability-tracking).

Name a port to check only that one. One name at most: a second, or an option other than
`--json`, prints the usage line and exits 2 rather than checking only the last name given. On a
system with no package database, the recipes in the
ports tree are checked instead. The report gives the database's date and age, each package that
is behind a recorded fix with the first three CVEs and a count of the rest, and a summary line. A
database more than 180 days old is flagged, with the reminder that a newer one arrives with
`kdos-tools` (through `kdos update`) or that `KDOS_SECDB` can name another.

A package is looked up under its own name, or under the name its recipe gives in `secdb =`. One
the database does not carry is **unknown**, never clean, and the summary says how many are in
that state. `--json` prints every finding with all its CVEs.

A library a port compiles into its own binary is in no package database, so a second file,
`/usr/share/kdos/bundled.txt` (override with `$KDOS_BUNDLED`), lists each one as `<port> <crate>
<component> <version>`. Every row whose port is checked is looked up under the component's name: a
version behind a recorded fix is a finding named `<port>/<component>`, and every row is listed under
*bundled copies*, with the ones the database does not carry marked. `--json` gives them as the
`bundled` array.

| Exit | Means |
|---|---|
| 0 | Nothing known-vulnerable |
| 1 | At least one package is behind a recorded fix |
| 2 | No database at the expected path, a second port name, or an unknown option |

---

## The system

### kdos update

```sh
kdos update [check [--json] [--out PATH] [--fetch]]
kdos update fetch [--from URL|DIR]
kdos update apply [--image | --binhost | --source] [--acquire | --arm] [--mount]
                  [--dry-run] [--root DIR] [--in-place] [--progress FD]
kdos update install [--dry-run] <port>
kdos update notes [--json]
kdos update theme
```

Updates the system. A recipe pins its version, so what is newer arrives with the **ports tree** and
nowhere else: every image carries the recipe-only tree at `/ports` (each recipe, patch and program
of our own, `shelves`, `sources.idx`, the two books under `docs/` and their renderer under
`script/book`, and no source tarball). An update is two steps:

- **`fetch`**: a newer tree arrives as one signed tarball, is verified against `/etc/kdos/keys/tree`
  and replaces `/ports` whole. The release's signed
  [release index](../03-architecture/packaging.md#the-release-index), `kdos-update.idx`, says where
  every file of its updates is; through it alone the signed binary-host index (`PACKAGES`,
  `PACKAGES.sig`) is fetched into `/var/lib/kdos/binhost`, and the slot image manifest into
  `/var/lib/kdos/image/offer`, each kept only with the size and hash the index gives and once its
  own signature verifies against `/etc/kdos/keys`, as the index's must.
- **`apply`**: the inactive A/B slot receives the newer release, in one of three lanes. The **image**
  lane writes the signed [slot image](../03-architecture/packaging.md#slot-images) of it, when one
  newer than the running system is published; the **binhost** lane installs the prebuilt packages
  the [binary host](../03-architecture/packaging.md#the-binary-host) index names for every package
  whose recipe now pins something newer; the **source** lane, only with `--source`, builds them on
  this machine. The image lane comes first, because a slot image is the release as it was built and
  tested, written whole; `--image`, `--binhost` and `--source` pick a lane by name.

Discover drives the same two steps through [`kdos-sysupdated`](daemons.md#kdos-sysupdated), and
[Discover](discover.md#system-updates) is the graphical side.

`/etc/kpkg.conf`'s default `PORT_REPO` names `/ports/core` and the four areas of `/ports/src`;
`PORT_REPO` can point at a checkout instead. Each directory `PORT_REPO` names is searched for a port
at `<name>/` and one shelf down at `<shelf>/<name>/`, and the first directory holding a name wins.
A tree that files one name twice inside one directory, or nests a port below its shelf, stops
`check`, `apply` and `kdos cve` with both paths named, because either recipe could be the one that
counts.

| Verb | Does |
|---|---|
| `check` (the default when no argument is given; name it to pass an option) | Lists each installed package whose version, then release, is older than its recipe's and whether the binary host covers it, counts packages with no recipe here, names the tree's commit, says what is offered and from which lane, and reports the recorded problem. A package newer than its recipe, such as a local rebuild with a higher release, is not listed. `--json` prints the same as one document; `--out PATH` writes that document atomically to `PATH` and implies `--json`; `--fetch` runs `fetch` first. Exit 0 up to date, 1 something behind or an image offered, 2 nothing could be surveyed |
| `fetch` | Downloads the newest published tree, checks its signature, installs it as `/ports`, and fetches the release index and, through it, the binary-host index and the slot image manifest. Each part checks its key before the network: with no key in `/etc/kdos/keys/tree` the tree is not downloaded and `fetch` records `no-key`, and with none in `/etc/kdos/keys` neither the binary-host index nor the image manifest is, and each records `no-key` on its own line. Exit 0 installed or already current, 1 refused or failed, 2 not attempted (not root, another update running, or `/ports` inside a git checkout) |
| `apply` | Fills the inactive slot (the acquire) and makes it the next boot's candidate (the arm). See below |
| `install <port>` | One port and every package it lacks into the running root, from the release binary host `fetch` keeps. Nothing is ever built: a package the signed index does not cover is refused, and so is a dependency already installed and older than its recipe, because the new package was built against the newer one and the system update comes first. Each package is fetched and checked against the index before any is installed, under `KPKG_REQUIRE_SIG`. The port joins the world set only after everything installed. It prints one `==> installing <pkg>` line per package; `--dry-run` prints `<pkg> <file>` for each and touches nothing, and needs no root. Discover's ports helper runs it through `pkexec`. Exit 0 installed or already was, 1 refused or failed, 2 usage |
| `notes` | The release notes of the slot this boot runs. `--json` prints `{"version", "previous", "notes"}`: the version running, the version the other slot holds (`""` when none is recorded), and the `RELEASE` file of the tree this slot carries, as Markdown. Exit 1 with no document when the slot carries no notes. The welcome tour's What's New page reads it |
| `theme` | Applies the accent in force again, exactly as `kdos theme <accent>` does, so the copies under `$HOME` are regenerated from artwork an update replaced |

**Where a tree comes from.** `fetch` reads `kdos-ports.tar.xz` and `kdos-ports.tar.xz.sig` from
`--from`, else `$KDOS_UPDATE_TREE`, else `tree =` in `/etc/kdos/update.conf`, else the latest KDOS
release on GitHub (`https://github.com/kunaldawn/kdos/releases/latest/download`). A value with no
scheme is a directory holding the two files, such as a USB stick. Only HTTPS or a local path is
read, and no redirect to plain HTTP is followed. `ports/publish --release` makes both files and signs
the tree with the release key; the image trusts that key because the build put its public half in
`/etc/kdos/keys/tree`. An image built without a release key has none there, and on it `fetch` stops
before the network with `no trusted tree key in /etc/kdos/keys/tree: this image can verify no
published tree; `kdos update apply --source` builds from /ports`: the tree the image carries is
the only one it can use. An image with no key in `/etc/kdos/keys` fetches no release index either,
so `fetch` there reaches no network at all. Nothing is unpacked before the signature verifies. A tree whose `TREE` stamp is older
than the installed tree's is refused, and the same commit is reported as current. The signature is
downloaded first, and one byte for byte the installed tree's (kept as
`/var/lib/kdos/update.state.sig`) is reported as current without downloading the tree. The old tree
and the new one change places in one rename. On the live ISO `/ports` lies in the overlay's
read-only lower layer, which cannot be renamed, so there the old entries are removed and the new
ones moved in one at a time.

**The update document.** `check --json` prints, and `check --out` writes, one line of JSON: the
document `kdos-sysupdated` serves Discover from, `kdos-notice` watches and the System Versions page
reads.

| Keys | Hold |
|---|---|
| `behind`, `orphans`, `packages` | What is older than its recipe, each with `binary`, whether the binary host covers it; installed packages with no recipe |
| `tree`, `version`, `date` | The tree in place, the version it is, and the day (UTC) its commit was made, or the offered image's when the lane is `image`; Discover shows it as the release date |
| `installed` | The version this system says it is |
| `covered`, `uncovered`, `size` | How much of `behind` the binary host covers, and the bytes still to download |
| `offer`, `lane` | The version an update would make this system, and from where: `image` or `binhost` |
| `signed_by`, `verified`, `binhost`, `image` | The key that signed what is offered, when it was verified, and where the index and the image come from |
| `channel`, `notes`, `checked` | The tree source in effect, the tree's release notes, and the time of this check |
| `problem`, `detail` | The worst recorded problem |

It answers in JSON even when it cannot answer: no tree is `"behind":0` with
`"problem":"no-tree"`. A prebuilt package is offered only when the whole set is prebuilt: `kpkg`'s
three equality tests (architecture, build-configuration hash, recipe hash) decide each package, and
one that fails them refuses the binhost lane with `N packages need a source build`, because a slot
half from the binary host and half from a compiler is a slot nobody tested. `check` runs the same
tests against the cached index, so what it offers is exactly what `apply` will take.

**What went wrong is recorded.** `fetch` and `apply` write their outcome to
`/var/lib/kdos/update.state`, and `check` reports the first problem of: `no-tree`, then the last
fetch's `no-key`, `no-network` or `fetch-failed`, a release index, binary-host index or image
manifest that no key in `/etc/kdos/keys` could verify (`no-key`), that did not verify, or a file
other than the one the release index names, then the last apply's `no-network`, `fetch-failed` or `build-failed`; a machine with a tree
and no package database reports `no-database` and exits 2. The nightly timer
(`/etc/kdos/timers.d/10-update-check.timer`) runs `kdos update check --out
/var/lib/kdos/update.json`, which reads only what is already on the machine and fetches nothing;
`kdos-sysupdated`'s `CheckNew` fetches at most once an hour, and `fetch` and `apply` rewrite the
document when they finish.

#### Applying

`apply` is two halves, because Discover shows progress for the first and not the second:

- **The acquire** (`--acquire`) does all the long work: it downloads, writes or installs the update
  into the inactive slot, runs [`kpkg check --all --root`](../03-architecture/packaging.md#checking-an-installed-root)
  over it, and records the slot as acquired. A slot with a missing file, or a changed one outside
  `/etc`, is not acquired; edited configuration under `/etc` is reported and does not stop it. A
  slot an earlier `try` armed is disarmed before its first byte is rewritten.
- **The arm** (`--arm`) takes only a slot an acquire of this tree or image filled. It carries this
  machine's configuration and state into the slot (a password or a network changed after the
  download is in the slot that boots), copies `/ports` into it unless an image brought its own,
  records its version, runs `kdos-bootctl deploy` to put that slot's kernel on the EFI system
  partition (ESP) every time, and `kdos-bootctl try` to make it the candidate for the next boot.

`apply` with neither runs both, which is what a person types. `--mount` mounts the inactive slot
itself (`kdos-sysupdated` passes it); without it the slot must already be mounted. On a trial boot
the inactive slot is the confirmed one, the way back, and nothing is written into it. On the live
medium, which `kdos-bootctl status --json` reports as `"live": true`, `apply` is refused with
`this is the live medium: what an update installs here is gone at the next restart unless a
persistence store keeps it`, because its root is an overlay in RAM; `--in-place` updates the
session anyway.

| Lane | What `apply` does |
|---|---|
| Image | Writes the signed slot image into the slot with [`kdos image unpack`](#kdos-image)'s synchroniser, seeded from the running root, so only what changed is downloaded |
| Binhost | Fetches every package's file and delta, checks each against the signed index, and installs them inside a chroot of the slot, dependencies first, under `KPKG_REQUIRE_SIG` |
| Source | Fetches every source with [`kpkg fetch`](../03-architecture/packaging.md#fetching-sources-on-a-machine), from the `sources-<N>` archive first, then upstream, before it builds anything, then builds and installs each package with `kpkg install -f` in a chroot of the slot (`/proc`, `/sys`, `/dev`, the ports tree, the source cache and `/etc/resolv.conf` bound in), so every package compiles against the slot's own libraries. Hours on a large set |

| Option | Does |
|---|---|
| `--image`, `--binhost`, `--source` | Pick the lane |
| `--acquire`, `--arm` | Run one half |
| `--mount` | Mount the inactive slot itself |
| `--dry-run` | List what would be fetched and installed; needs no root |
| `--root DIR` | Install into `DIR` instead of choosing a slot. Nothing after the installs runs: no `kpkg check`, no copy of `/ports`, no `deploy` or `try` |
| `--in-place` | Update the running root even on an A/B machine, giving up the rollback, or on the live medium, into the overlay |
| `--progress FD` | Write one line per event to that descriptor, for a program |

`--progress` writes these lines, each as it happens:

| Line | Means |
|---|---|
| `refused <reason>` | Nothing was written; the reason is the sentence a person is shown |
| `verified <key-id> <version> <items> <bytes>` | The index or manifest verified and covers every item; for `--arm`, `verified - <version> 0 0` |
| `item <done> <total> <name>` | One more step of the long work |
| `error <reason>` | The run failed; the slot stays untried |
| `acquired <slot> <version>` | The slot is filled and checked |
| `armed <slot> <version>` | The slot is the next boot's candidate |

`SIGTERM` stops an apply between packages, which is what `kdos-sysupdated`'s `Job.Cancel` sends.
`fetch` and `apply` need root and take one lock, so a tree is never replaced under an install. On an
installed machine without A/B slots `apply` updates the running root, and a package that fails leaves the
ones that installed in place. See
[Updating a machine](../03-architecture/packaging.md#updating-a-machine) and
[A/B slot selection](../03-architecture/boot-and-init.md#ab-slot-selection).

| Exit | Means |
|---|---|
| 0 | Done, or nothing was behind |
| 1 | A download, a verification, an install or build, the slot's check, the carry, the deploy or `try` failed |
| 2 | Not attempted: no tree, no package database, not root (without `--dry-run`), another update holds the lock, a bad option, a trial boot, or the inactive slot is not mounted and neither `--mount` nor `--in-place` was given |

### kdos image

```sh
kdos image make ROOT OUTDIR --version V [--tree C] [--time T] [--key FILE]
                [--prev DIR [--prev-root DIR]]... [--exclude PATH]... [--level N] [--part-mb N]
kdos image verify DIR [--keys DIR]
kdos image unpack DIR|URL TARGET [--seed ROOT] [--keys DIR]
kdos image carry RUNNING SLOT [--root-dev UUID=…]
```

A whole root slot as one signed release, and its deltas from earlier ones. One file of
`kdos-tools` holds both halves, the release step that makes an image and the machine that writes
one into its inactive slot, so the format has one reader and one writer. The format and why a slot
is not one patch are in [Slot images](../03-architecture/packaging.md#slot-images).

| Verb | Does |
|---|---|
| `make` | Writes a signed image of `ROOT` into `OUTDIR`: the manifest (every directory, file, link and hard link with its mode, owner, time, size and SHA-256), the full bundle of objects cut into parts small enough for a release asset, for each `--prev` image a delta bundle holding what that version lacks, whole or as a `zstd --patch-from` difference, plus every object under `etc/` and `var/`, and `kdos-update.idx`, the release index naming each of those files at `image/<name>`, signed with the same key |
| `verify` | Checks the manifest's signature against `/etc/kdos/keys` (or `--keys`) and every object the manifest names, in a local directory; a URL is a usage error |
| `unpack` | Reads the release index at `DIR` or `URL` first and takes the manifest and every bundle where it says; writes an image into `TARGET`, synchronising rather than formatting: a file the target already holds with the right hash stays, one the seed root (`--seed`, the running root) holds is copied, and only the rest comes from a bundle. Entries the manifest does not name are removed, except `lost+found` and anything on another filesystem. The slot keeps its filesystem and its UUID |
| `carry` | Copies a running root's configuration and state into a slot, which is what `kdos update apply --arm` does before a slot is tried; `--root-dev` names the slot's own root device for its `fstab` |

`make` stamps the image with the commit, time and version in `ROOT/ports/TREE` unless `--tree`,
`--time` or `--version` names them, and refuses a version it cannot use. `--level` is the zstd
level of the bundles, 1 to 19 (default 12), and `--part-mb` the size a bundle is cut at (default
1900 MiB); `--exclude` leaves a path out of the image.

Nothing in a bundle is trusted: every object, whole or patched, is hashed after it is written and
kept only when the hash is the one the signed manifest gives, so a tampered bundle costs a download
and never a file.

| Exit | Means |
|---|---|
| 0 | Done |
| 1 | A write, a copy or a carry failed |
| 2 | Bad usage |
| 3 | The release index or the manifest did not verify, the manifest is not the file or the version the index names, or the image is for another architecture |

### kdos timezone, hostname, locale, keymap and in-vm

```sh
kdos timezone [Area/City]       # print, or set, the time zone
kdos hostname [name]            # print, or set, the static host name
kdos locale [NAME=value...]     # print, or replace, the locale settings
kdos keymap [map]               # print, or set, the console keymap
kdos in-vm                      # exit 0 inside a virtual machine
```

The machine's name, clock and language, through the same services System Settings uses:
`timedate1`, `hostname1` and `locale1`, which [`kdos-settingsd`](daemons.md#kdos-settingsd) serves
and writes to the files KDOS reads. With no argument each prints the value in force. Setting one asks
polkit with interaction allowed, so a person outside `wheel` is asked for a password by the polkit
agent rather than refused outright, and the time zone needs none in the active session. `locale`
replaces the whole list: every `NAME=value` given, and nothing else. `in-vm` prints nothing and
exits 0 inside a virtual machine and 1 otherwise.

Exit 0 done, 1 when the service refused or did not answer (its message is printed:
`InvalidArgs` means the zone, name or map is not one), 2 on bad usage.

### kdos backup

```sh
kdos backup run
```

One restic snapshot, as `~/.config/kdos/backup.conf` says: what the nightly timer runs, and what a
person runs at a prompt. Kup, in System Settings, is the backup a person sets up with the mouse;
this is the one written in a file, and both may share one repository. Every snapshot made here is
tagged `kdos`, and Kup's own prune forgets only the snapshots tagged `kup`, so neither ever removes
the other's.

The file is parsed, never sourced: `key = value` a line, `#` to the end of a line a comment.

| Key | Means |
|---|---|
| `repo` | The restic repository; the last one wins |
| `password-file` | Where the repository's password is; default `~/.config/kdos/backup.pass`; the last one wins |
| `include` | A path to back up; up to eight |
| `exclude` | A restic pattern to leave out, passed as written, globs and all; up to eight |

The password is a file and never an argument, and a file anyone else can read is refused before
restic is started: group or world readable is the same disclosure as an argument, by a slower
route. Each include and exclude is one argument, so a folder with a space in its name is one
folder.

The timer row is `~/.config/kdos/timers.d/10-backup.timer`, shipped commented out, because a
backup nobody asked for is a surprise and the first run over a home can take hours. Uncomment it
once the configuration names a repository and the password file is mode 0600; it runs at 03:40
under your own user services, and once on waking when the machine slept through it, within eight
hours.

The exit status is restic's (3 an incomplete snapshot, 10 no repository, 11 locked, 12 a wrong
password), or 1 for a refusal here, 2 for bad usage, 127 with no restic.

### kdos checklist

```sh
kdos checklist --json        # {"version":1,"items":[{id,done,dismissed}…]}
kdos checklist dismiss <id>  # hide one item for good
kdos checklist restore       # bring every hidden one back
```

The first-run checklist the tray's Getting Started applet shows (see
[Getting Started](plasma-shell.md#getting-started)). Each item is done when the machine says so,
never when a box was ticked:

| Item | Done when |
|---|---|
| `backup` | `~/.config/kdos/backup.conf` names a repository and something to put in it, or Kup is on with a restic plan that has a destination |
| `phone` | KDE Connect has a paired device |
| `language` | Formats were set, or a second keyboard layout added. A `[Formats]` `LANG` equal to `/etc/locale.conf`'s (a C locale when that names none) is not a choice, since Plasma writes the session's `LANG` there at every account's first login; any other Formats key, or a `LANG` that differs, is |
| `box` | A box exists |
| `firmware` | fwupd has no update waiting |
| `encryption` | The root filesystem sits on LUKS |

An item whose component is absent is left out rather than shown undone: a phone item on a machine
with no KDE Connect is a task nobody can finish, and so is encryption on a root that is not a block
device, such as the live medium's overlay. The tray asks every five seconds while anything is
open, so fwupd's answer is remembered for six hours and the root device's for the boot. Exit 0; 1 a
failure, with one line a person can read; 2 on bad usage.

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
needs. `run` builds the port twice on this machine with `kpkgbuild`, run directly in the port's
directory with `CFLAGS`, `CXXFLAGS` and `PACKAGE_DIR` in its environment and no shell between, once with the `CFLAGS` of
its own environment (`-O2 -pipe` when `CFLAGS` is unset; `CXXFLAGS` gets the same value) and
once with `-march=<level>` appended, unpacks both, runs the port's own benchmark against each,
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

`run` with no port exits 2 (`name at least one port`) and does no work. A processor with no
level above baseline x86-64 has nothing to measure and `run` exits 1. A verdict the ledger cannot
take is said on standard error, `<port> not recorded — cannot write <path>`, because `report`
reads only the ledger.

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
[Decisions](../01-philosophy/decisions.md#-march-measured-per-machine-not-chosen-for-a-population).

### kdos rebuild

```sh
sudo kdos rebuild [--dry-run] [--iso-only] <work-dir>
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
phase scripts. The job count is `KDOS_JOBS` when the caller sets it; otherwise the build
environment computes it from this machine, one job per thread but no more than one per 2 GiB of
memory, so a laptop with little memory builds slower rather than having its compilers killed.
`--iso-only` runs only the `70_image` phase. `--dry-run` stops after the
checks and prints the plan.

The build runs as root. It sets owners on the files of the tree it assembles, and most phases
run their steps inside a chroot whose entry script, `script/chroot/exec.sh`, exits at once for
any other user. `kdos rebuild` therefore refuses to run (exit 1) unless it is root, before it
copies anything; `--dry-run` is the exception, and checks and prints the plan as any user. `sudo` resets the environment, so `KDOS_JOBS` and
`KDOS_SOURCES` reach the build only when they are passed through it, as in
`sudo env KDOS_JOBS=4 kdos rebuild /mnt/disk/work`. [How KDOS is built](../05-developer/how-kdos-is-built.md) follows
the same build from `git clone` to an ISO, and [The build system](../05-developer/build-system.md)
describes the orchestrator.

A developer medium, one that carries the sources, is made and used in four steps:

1. On a build machine, in a checkout of the repository, run `make fetch` and then
   `make build KDOS_ISO_SOURCES=1`.
2. Write the ISO to a USB stick and boot it.
3. Mount a real disk with at least 25 GB free (not tmpfs and not an overlay), for example at
   `/mnt/disk`.
4. Run `sudo kdos rebuild /mnt/disk/work`.

`KDOS_ISO_SOURCES=1` makes `script/phases/70_image/110_iso.sh` copy `ports/`, `src/`, `script/`
and `fs/` onto the medium's ISO 9660 filesystem beside the system image, under `/sources`,
together with the `Makefile`, `Dockerfile` and `CLAUDE.md` when they are present, and with the
binary host at `/sources/binhost` when the same build wrote one (`KDOS_MAKE_BINHOST=1`). The sources
cost the installed system nothing and are readable at `/mnt/iso/sources` as soon as the live
system is up. The option is off by default because a medium that carried `ports/` would roughly
double in size: `ports/` is about 40 GiB of archives that are already compressed (`du -sh
ports/core` with every source fetched). The fetch cache `ports/.srccache` is left off, because
each port directory already holds its own copy of the bytes. A `SOURCES` stamp records the port
count, size and build time, and `kdos rebuild` prints it before it starts; the port count is the
number of recipes in the medium's own `ports/core`, found at `<name>/` or one shelf down.

Inside the chroot, `/kdos` is a non-recursive bind of the build container's `/workspace`, and
`script/chroot/exec.sh` binds `script/`, `src/`, `fs/`, the `Makefile` and the `Dockerfile` back
over it and binds `ports/` at `/ports`; the step reads each from there. The build container mounts
the `Makefile` and the `Dockerfile` beside the directories, and not `CLAUDE.md`, so a medium made by
`make build` carries the first two and not the third. `kdos
rebuild` names its copy of the tree in `KDOS_WORKSPACE`, and the host environment
(`script/env/host.env`) takes `WORKSPACE` from that variable, so the bootstrap phase's file-system
step copies the overlay from the copy's `fs/` rather than from `/workspace/fs`. No rebuild from a
medium has been run through every phase; see
[Known gaps](../06-reference/known-gaps.md#an-offline-kdos-rebuild-from-the-medium-has-not-been-run-to-the-end).

### kdos persist

```sh
kdos persist [--json]                                  # report
sudo kdos persist create [<device>] [--yes]            # -y also works
sudo kdos persist reset [--home | --system] [--yes]    # from the clean session
```

```
$ kdos persist
no persistence store

  This session's writes are in RAM and go when it is
  powered off. `kdos persist create` makes a store in
  the free space after the image on the boot medium.

$ sudo kdos persist create
  disk       /dev/sda  (32G)
  partitions 3 now; the store is added after the last
  filesystem ext4, labelled KDOS_PERSIST
```

On the live medium, everything you write goes into the upper layer of an overlay held in RAM, and
is lost at power-off. `create` gives that upper layer a real filesystem instead: one ext4
partition with no reserved blocks, labelled `KDOS_PERSIST`, made in the free space after the last
partition of the boot medium or of the disk you name. A disk with no partition table is given a
GPT holding only the store. It asks you to type `yes` unless given `--yes`, and it must run as root
with `sfdisk`, `mkfs.ext4`, `blkid` and `losetup` installed.

`create` refuses, before it writes anything, when a store already exists, when no device is named
and the system did not boot from a medium, when the device is a partition rather than a disk (it
names the disk), when the device is read-only (an optical boot's `/dev/sr0`: name a USB stick
instead), and when the whole disk is one filesystem with no partition table (it names the type,
because there is no free space to take). When `sfdisk` will not add the partition, its own error
is printed.

`--json` prints the report as one line for a settings page:

| Key | Holds |
|---|---|
| `live` | Whether a boot medium is mounted at `/mnt/iso` |
| `store` | `none`, `present`, `refused` or `in-use` |
| `device` | The store's partition, or `null` |
| `type` | The store's filesystem type, or `null` |
| `medium` | The boot medium's disk, or `null` |
| `writable` | Whether the boot medium can hold a store; `false` for a disc |
| `candidates` | The disks a store could be made on: the medium when it is writable, then each removable or USB disk that is writable and has nothing mounted |

The welcome tour's live page reads it, offers the candidates, and runs
`pkexec kdos persist create --yes` for the medium or `pkexec kdos persist create /dev/<name> --yes`
for another disk, which the live medium's polkit rule grants the session without a password.

The label is the whole interface. At boot the initramfs asks `blkid` for a filesystem with that
label and uses whichever answers, so the store can live on the boot stick, on a second stick or on
an internal disk. No path is written into any configuration file, because device names depend on
enumeration order and USB does not keep it. When more than one filesystem carries the label, the
report lists them all: the boot takes the one `blkid` answers first, which can change between
boots.

The store must be a filesystem with extended attributes, hard links and `d_type`, which is why
`create` makes ext4. FAT and exFAT labels hold at most 11 characters, so neither can carry the
12-character `KDOS_PERSIST` and such a store is never found. The initramfs refuses an ntfs,
iso9660 or squashfs store by name and says so on the console, because overlayfs rejects an
unsuitable upper layer with the same `EINVAL` it gives for every bad mount; `kdos persist` reports
such a store as `refused` with its type, and `--json` gives `"store": "refused"`. A store that is
refused or will not mount costs nothing but itself: the session comes up in RAM as it would with
no store. The boot side is in
[Boot and init](../03-architecture/boot-and-init.md#the-live-medium-and-persistence).

Three behaviours of the store are easy to miss:

- A store is used from the next boot, not the one that created it. `kdos persist` then
  reports `present, NOT in use by this session` rather than claiming your work is being saved.
  Once it is in use, the report names the device mounted at `/mnt/persist` and shows the free
  space.
- A change that stops the desktop coming up is still there at the next boot. Choose **KDOS
  Live (clean session)** in the boot menu: it passes `nopersist` on the kernel command line, which
  ignores the store for one boot without deleting it.
- From that clean session, `reset` deletes what the store holds: everything in its `upper/`
  directory, only `upper/home` with `--home`, or everything but `upper/home` with `--system`. The
  overlay's scratch directory, `work/`, is emptied each time. A deleted entry in `upper/` is a
  change undone, and a deleted whiteout brings back the image's file it hid. `reset` refuses while
  the store is in use, because the upper layer of a mounted overlay must not change beneath it. It
  asks for `yes` unless given `--yes`.

`create` appends a partition with `sfdisk --append` and never rewrites the partition table,
because the existing entries describe the image the machine is running from; `--wipe never` keeps
`sfdisk` off the ISO 9660 signature at the start of the disk. The medium is mounted as the whole
disk, because ISO 9660 starts at its first sector, and a mounted disk is held exclusively: the
kernel refuses to re-read its table and refuses to open any partition on it for `mkfs` or `mount`.
So `create` passes `--no-reread`, registers the new partition with `partx -a`, and makes the
filesystem through a loop device over the disk at the partition's offset and length, which opens
the disk without that claim. `reset` mounts a store on the boot disk the same way, and so does the
initramfs; a store on any other disk is opened directly.

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

| Record | Covers a partition appended after the ISO 9660 volume? |
|---|---|
| The ISO 9660 primary volume descriptor's volume size | **No** |
| The GPT header's `alternate_lba`, the position of the backup header | Yes |

The KDOS image needs only the first. Its ISO 9660 volume spans the whole file: the EFI system
partition is an El Torito boot image inside the volume, which the MBR also names as partition 2,
and the image carries no GPT. The second record is for a hybrid image made with an appended EFI
system partition, where the volume descriptor stops short by the size of that partition and those
bytes are what make the copy boot. An image with neither record is refused as not a KDOS medium.

Four refusals stand before a byte is written. A clone never writes to the medium this system
booted from, to a disk with a filesystem mounted anywhere, to a disk named by device path in
`/etc/fstab`, or to anything smaller than the image. Each is checked against the whole disk,
because the target is a whole disk while everything identifying the running system names a
partition. These rules are wider than a mounter needs: a mounter chooses something to mount, and
this chooses something to destroy.

A counterfeit stick reports a capacity it does not have and silently wraps writes around, so the
copy appears to succeed and the verify fails in the middle, which looks like a broken image. When
`f3probe` is installed it runs first in destructive mode (the device is about to be overwritten
anyway), and a counterfeit or damaged verdict refuses the clone. The probe writes and reads back
across the whole device and takes minutes; its output is shown as it arrives, with an elapsed-time
line between its lines on a terminal, and `--no-probe` skips it.

Before the read-back, the block device's cache is dropped (`BLKFLSBUF`). Otherwise the read
would return the bytes this process just wrote rather than the bytes the flash stored, which is
exactly what a counterfeit stick would get away with. The written and read-back SHA-256 hashes are
compared, and a failed verify prints both.

### kdos speech

```sh
kdos speech transcribe [--notify] [--] FILE...   # a .txt beside each file
kdos speech say [--voice NAME] [--] [TEXT...]    # read TEXT, or standard input, aloud
kdos speech sync                                 # Klipper's Read Aloud action
kdos speech list              # the transcription models, and which are here
kdos speech get [NAME]        # fetch one; base.en when no name is given
kdos speech where             # the directories searched, and the models in each
kdos speech remove NAME
kdos speech voice list        # the Piper voices, and which are here
kdos speech voice get [NAME]  # fetch one; en_US-lessac-medium when no name is given
kdos speech voice remove NAME
```

Transcription and reading aloud, on this machine: whisper.cpp hears and Piper speaks, each from a
model file on the disk, and nothing is sent anywhere. Dolphin's **Transcribe** action runs
`transcribe` (see [Transcribe](plasma-shell.md#transcribe)), and Klipper's **Read Aloud** action
runs `say`.

`transcribe` decodes each file with `ffmpeg` to the 16 kHz mono audio whisper reads, into a private
file in the runtime directory removed on every path out, and writes the text beside the file as
`FILE.txt`, or `FILE.2.txt` and onward, never over a text you already have. The model is
`$KDOS_WHISPER_MODEL`, else the largest catalogue model in your directory, else the largest in the
image's. `--notify` says when each is done.

`say` speaks with a Piper voice, `<name>.onnx` with its `<name>.onnx.json` beside it, from
`~/.local/share/piper/voices` and then `/usr/share/piper/voices`; `--voice` names one. `sync`
keeps Klipper's **Read Aloud** action in step with the voices: added while a voice is installed and
removed, exactly that action, when the last one goes, through `kwriteconfig6` on `klipperrc`. The
autostart entry `kdos-speech-sync.desktop` runs it at every login, and `voice get` and
`voice remove` run it when they finish.

The image carries Piper and no voice: a voice is one language and one speaker, so it is fetched
like a transcription model. `voice get` fetches the `.onnx` and its `.onnx.json` from Piper's
voice repository at its `v1.0.0` tag into `~/.local/share/piper/voices`, checks each against a
SHA-256 recorded in the source, and renames both into place only when both match. `say` and Read
Aloud find a voice only while its `.onnx.json` is beside it, so a half-fetched voice is never
used. `voice list` marks a voice in your directory `installed`.

| Voice | Size |
|---|---|
| `en_US-lessac-medium` (the default) | 63 MB |
| `en_GB-alan-medium` | 63 MB |
| `de_DE-thorsten-medium` | 63 MB |
| `es_ES-davefx-medium` | 63 MB |
| `fr_FR-siwis-medium` | 63 MB |

The image carries `whisper-cli` and one model, `base.en`, in `/usr/share/whisper.cpp/models` (the
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
privilege. Each download is fetched with `curl` from the upstream model repository and checked
against a SHA-256 recorded in the source and against the ggml magic number, and refused if either
fails. A model already present is not fetched again. Upstream serves the files over TLS and signs
nothing, so the checksum guarantees the bytes are the ones this release was written against; it
does not make the upstream trustworthy (see [the security model](../03-architecture/security-model.md)).
A name is looked up in the table and never pasted into a URL, so a name cannot point the download
at another host. An unknown verb exits 2, as every usage error does; a failed download, a
checksum that does not match or a name not in the table exits 1.

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
| `kdos-shot` | `/usr/local/bin` | Screenshots through Spectacle |
| `kdos-banner` | `/usr/local/bin` | The login banner |
| `kdos-fetch-app` | `/usr/local/bin` | Install an application into a distrobox with its package manager and export it to the host |
| `kdos-fetch-static` | `/usr/local/bin` | Fetch a single verified static binary |
| `kdos-sfx` | `/usr/local/bin` | The machine's sounds |
| `kdos-mpctl` | `/usr/local/bin` | Music control, and the bridge that puts mpd on MPRIS |
| `kdos-share` | `/usr/local/bin` | [`kdos share`](#kdos-share), under the name KRunner's routes and `mc` run |

The package also installs the **KDOS Help** menu entry, the reasons corpus under
`/usr/share/kdos/reasons`, the vulnerability database at `/usr/share/kdos/secdb.txt` with
`bundled.txt` beside it, the `Meta+Shift+T` tiling shortcut under `/usr/share/kglobalaccel`, and
two autostart entries, `kdos-mpris.desktop` and `kdos-speech-sync.desktop`. `kdos-doc` is its own
port, `src/system/kdos-doc`.

### ksvc and service

```sh
service [--user] list
service [--user] status|start|stop|restart <name>
service [--user] enable|disable <name>
```

Services are the executable scripts `/etc/init.d/NN_<name>.sh`, and a service's name is the
script's name without the number and the `.sh`. A name matches a script exactly first, then as a
substring, so `service start ssh` finds `70_sshd.sh`. `list` prints one line per service: its
name, whether it starts at boot, and the first line the script's `status` printed. A script that
printed more lines gets ` (+N more)` after it, and `service status <name>` prints them all; one that
printed nothing gets `—`. On a terminal a line too long for the width is cut at its last space that
keeps at least half of it, or at a character when there is none, and ends in `…`, so that each
service is one row. `restart` runs the script's `stop`, waits a second and
runs its `start`.

`disable` creates `/etc/service.disabled/<name>` and `enable` removes it; the boot scripts skip a
service with that marker (see
[Configuration](../06-reference/configuration.md#etcservicedisabledname)). The marker is named
after the argument exactly as typed, while the boot scripts look for the script's own name, so
give `enable` and `disable` the full name `service list` prints. A name that is not a plain name
(letters, digits, `_`, `-` and `.`, at most 64 characters) is refused, because a name placed into
a file pattern could match anything.

**Your own services.** `--user` works on a second table: the scripts in `/etc/kdos/user-services`
(the session bus, PipeWire, WirePlumber, pipewire-pulse, mpd, your timers and your boxes), which
turnstile's `ksvc` backend starts at your first login and stops at your last, with their pid,
state and log files in `$XDG_RUNTIME_DIR/ksvc` instead of `/run`. `enable` and `disable` then
decide whether one starts. Root always gets the system table and refuses `--user`, because a path
a root tool took from its caller's environment would let that caller name the pid file the next
`stop` kills. Anyone else reads the system table (`service list`, `ksvc check sshd`) but supervises
only into their own, whose run directory must be theirs and writable by nobody else.

The init scripts start their daemons through `ksvc supervise`:

```sh
ksvc supervise [--final-exit CODE]... [--facility NAME] [--stop-grace SECS] [--restart-with NAME] <name> <command> [args...]
```

It forks a supervisor, writes the supervisor's process id to `/run/kdos-svc.<name>.pid`, and runs
the daemon under it. The `kdos-svc.` prefix is what keeps the file the supervisor's: `/run/<name>.pid`
is where most daemons write their own pid, and one that overwrote the supervisor's id would make
`status` say `[DOWN]`, `stop` drop the file as stale and stop nothing, and `start` run a second
copy. When `/run/kdos-svc.<name>.pid` already names a live supervisor of that name, it starts
nothing, prints `[SKIP] <name>: already supervised (pid N)` and exits 0, so `service start` on a
running service changes nothing. A second supervisor would overwrite the pid file, and the first,
with its daemon, could then never be stopped; a second system bus would also take the socket from
the clients already connected to the first. A daemon whose supervisor is gone (one ended by
`SIGKILL`) counts as running too: `supervise` waits for it to exit for at most the stop grace,
then prints `[FAIL] <name>: daemon pid N of a supervisor that is gone is still running` and exits
1, and `ksvc stop-supervised <name>` ends it. A live pid alone does not make a service running:
the supervisor must also run as the table's owner, because another account's supervisor of a
service with the same name (every user has a `pipewire`) is not this one. The daemon's output and
the supervisor's own start and exit lines go to the system log under the service's name, one
`info` message per line, and to `/run/kdos-svc.<name>.log`, which holds at most 64 KiB with one
previous generation beside it as `.old`. The file is what is left before the system logger is up
and while it restarts.

The messages go under the `daemon` facility, so to `/var/log/messages`, unless `--facility` names
another: `auth`, `authpriv`, `daemon`, `user` or `local0` to `local7`. `41_polkitd.sh` passes
`--facility authpriv`, so that what polkitd prints lands in `/var/log/auth.log` with what it sends
to syslog itself.

The supervisor restarts the daemon five seconds after any exit. The exception is an exit status
named by `--final-exit` (up to eight): a status restarting cannot change, such as thermald on an
Intel model it does not know, smartd with no disk to watch, or a bus daemon whose name another
process owns. The supervisor then says so once, removes its pid file and exits, instead of
restarting the daemon every five seconds for as long as the machine is up. A death by signal is
always restarted.

`--restart-with NAME` ties the daemon to the daemon of another supervised service. The supervisor
looks at that service's state file four times a second, and when its daemon differs from the one
that was running when this daemon started (it exited, was restarted, or the service was stopped),
prints `[KDOS] (<name>) NAME restarted or stopped: restarting <name>`, sends this daemon `SIGTERM`
(`SIGKILL` after the stop grace) and starts it again: at once when the other service has no
supervisor, otherwise when its daemon is back, and one second later, the time a bus that has just
started needs to open its socket. Before every start, a supervisor of NAME that is between two
runs of its daemon is waited for. Such a restart is not a failure and does not wait the five
seconds. `--restart-with dbus` is passed by the scripts of the daemons that own a name on the
system bus and do not exit when the bus goes: elogind, NetworkManager, `kdos-settingsd`,
accounts-daemon, `kdos-sysupdated`, iio-sensor-proxy and tlp-pd. Without it, a restarted bus
leaves such a daemon running with no name on the new bus while `check` reports `[ OK ]`.

The supervisor runs in its own session and process group, so `ksvc stop-supervised` (what a
script's `stop` calls) signals the supervisor, the daemon and the log forwarder together. A
`SIGTERM` sent to the supervisor alone is passed on to the group, and to a daemon that left the
group by its pid, before the supervisor exits, so the daemon goes with it. `stop-supervised` creates `/run/kdos-svc.<name>.stopping` for as long as
it runs, and a supervisor that finds it passes nothing on, so the daemon gets one `SIGTERM`, not
two: llama-server, for one, takes a second as "exit at once". `stop-supervised` sends
`SIGTERM`, then waits for every process in the group to exit, checking every 50 ms, for at most the
stop grace: five seconds, or what `--stop-grace` (1 to 600) named. Whatever is still running then
gets `SIGKILL`, after the line `[KDOS] (<name>) Still running after Ns, killing`. A daemon that
exits at once is stopped at once, and one whose clean shutdown takes longer names a longer grace:
`76_postgresql` passes 30, `83_samba` 15 and `65_turnstiled` 60, every user's services within it. A
daemon that moved itself into a process group of its own is signalled by its pid as well. A
supervisor that did not lead its own group would be killed alone, orphaning the daemon while
reporting success. The forwarder ignores the group's `SIGTERM` and leaves when the daemon closes
its output, so a stopping daemon's last lines still reach the log.

The pid file names the supervisor, which outlives every exit of its daemon, so the supervisor
also keeps `/run/kdos-svc.<name>.state`: whether the daemon is running or waiting to be restarted,
its pid, since when, how many failing exits it has had in a row, the last exit status and the stop
grace. A non-zero exit
is a failure, and starts the count again at one after a run of 60 seconds or more. An exit with
status 0 is a failure only after a run shorter than five seconds, a daemon giving up at once
(ModemManager exits 0 when its bus name is taken); after a longer run it is a clean exit and
clears the count, because a periodic job's `snooze` exits 0 after every job it runs.
`ksvc check <name>` prints one of these lines, and exits 0 for `[ OK ]` and 1 for the rest:

| Line | When |
|---|---|
| `[ OK ] <name>  (pid N)` | The supervisor is alive, and its daemon is running, exited 0, or is being restarted with the service `--restart-with` names |
| `[FAIL] <name>  (exited with code N; restarting)` | The supervisor is waiting five seconds to start the daemon again after a non-zero exit, or after its second failing exit in a row of any status; `M failed starts in a row` is added once there are two or more |
| `[FAIL] <name>  (crash loop: M failed starts in a row, last exit code N; pid P)` | The daemon is running again after two or more failing exits in a row, and has not yet run 60 seconds |
| `[FAIL] <name>  (supervisor gone; daemon pid N still running)` | The supervisor named by the pid file is gone, and the daemon it last started still runs in its process group |
| `[DOWN] <name>` | No pid file, or its supervisor and its daemon are gone |

Stopping the service, and a final exit, remove the state file with the pid file.

### kdos-bootctl

`kdos-bootctl` manages the A/B root slots and each slot's kernel on the EFI system partition. Its
state is `/boot/efi/EFI/kdos/bootstate`, and it rewrites the `/KDOS` entries of `limine.conf` to
match. The state file lives on FAT, which has no journal, so every write is a temporary file, a
`fsync` of the file and of the directory, then a rename; a file that cannot be parsed is treated as
absent, never as partial.

| Verb | Does |
|---|---|
| `status [--json]` | The active slot, the candidate, attempts and the `BootNext` trial state, and which version each slot holds (the default verb) |
| `list [--json[=short\|pretty\|off]]` | The boot entries, one per slot, in the shape `bootctl list --json` prints |
| `select [<a\|b>]` | Run at boot by the initramfs: decide which slot's root to use, given the boot-menu entry that was picked, and roll back a candidate whose trial boot was spent |
| `try <a\|b> [n]` | Mark a slot as the candidate for the next boot. With UEFI variables it arms a one-shot `BootNext` trial; otherwise the menu leads with the candidate for `n` boots (default 3). Refused for the active slot, a slot with no root, or a slot with no kernel on the ESP |
| `cancel` | Forget a trial that has not booted: the firmware's one-shot first, then the state, so a power cut between the two comes up on the confirmed slot. `no trial is armed` when there is none |
| `mark-good` | Confirm the running slot after a successful boot, clearing the trial. `kdos-sysupdated` runs it once both bless marks exist; a trial armed earlier in the same boot is left as it is |
| `set-slot <a\|b> <uuid> [<luks-uuid>]` | Record a slot's root filesystem and, when encrypted, its LUKS container; without one the slot is recorded as unencrypted |
| `crypt <fs-uuid>` | Print the LUKS container recorded for the slot whose root filesystem has that UUID; asked by the initramfs after `select`, silent with exit 1 when there is none |
| `deploy <root> [<a\|b>]` | Copy the kernel and initramfs from a slot's root into that slot's directory on the ESP, with the root's identity as `EFI/kdos/<slot>/release` |
| `theme [--print] [<accent>]` | Write the accent's console palette to `/etc/vtrgb` and rewrite the theme lines of `limine.conf` (the default accent when none is named); a machine with no `limine.conf` keeps its boot menu and still exits 0. `--print` prints the `limine.conf` lines and writes nothing |
| `splash phosphor\|modern` | The boot splash's motion, as `kdos.splash=` on every KDOS entry |
| `palette [<accent>]` | Print the console palette (the `/etc/vtrgb` format) for an accent, the default one when none is named |

`status --json` prints one line. `configured`, `live`, `active`, `trying`, `slot_a`, `slot_b`, `crypt_a`,
`crypt_b` and `running` are read by `kdos update` with exact string matches, each spelled
`"key": value` with the space, so their spelling is an interface; `attempts`, `bootnext`, `next`,
`failed`, `entry` and `slots` (each slot's state, title, version, build, date, tree and kernel)
are for readers that parse JSON: the System Versions page, the logout screen, the notices and
`kdos-sysupdated`.

With no boot state `status` and `list` exit 1. On the live medium, where `/` is an overlay in
`/proc/mounts`, they say `this is the live medium — it has no boot state and no slots; install KDOS
for A/B updates and rollback`, and `status --json` prints `{"configured": false, "live": true}`;
on an installed machine with one root they say it has one root and no rollback, and `status --json`
prints `{"configured": false}`.

The ESP is root's alone, so every run as root against this machine's own state also writes
world-readable copies a session can read: `/run/kdos/bootstate.json` (`status --json`),
`/run/kdos/boot-entries.json` (`list --json=short`) and `/run/kdos/slot.json`. A `--json` read that
cannot open the state file prints those copies instead, so the logout screen, the welcome tour and
About this System answer without root. `kdos-sysupdated` refreshes them at start, every half minute
and after each change it makes, and `try`, `cancel`, `mark-good`, `set-slot` and `deploy` write
them as they change the state.

It is also copied into the initramfs. The full boot flow is in
[A/B slot selection](../03-architecture/boot-and-init.md#ab-slot-selection).

### kdos-shot

```sh
kdos-shot [region|screen|full|window|qr]
kdos-shot --help
```

Spectacle takes the picture, in its background mode without its own notification
(`spectacle -b -n -o <file>`): it is the one program KWin hands a screen to, through its
ScreenShot2 interface. What `kdos-shot` adds is the naming, the clipboard, one notification and
`qr`.

| Mode | Takes |
|---|---|
| `region` (the default) | A rectangle you drag out (`spectacle -r`); `Escape` cancels and saves nothing |
| `screen` | The screen under the pointer (`-m`) |
| `full` | Every screen (`-f`) |
| `window` | The active window (`-a`) |
| `qr` | A rectangle you drag out, read as a QR code by `zbarimg`, with the decoded text copied to the clipboard |

A screenshot is saved as `~/Pictures/Screenshots/kdos-<date>-<time>.png`
(`kdos-%Y%m%d-%H%M%S.png`), or under `$XDG_PICTURES_DIR/Screenshots` when that variable is set,
copied to the clipboard with `wl-copy`, announced in a notification, and its path printed. A second
shot in the same second is `kdos-<date>-<time>-2.png`, then `-3` and on: the name is reserved
before Spectacle runs, so no shot replaces another. A notification the server refuses (Plasma
limits a burst) is not reported. `qr`
writes its picture only to the runtime directory and deletes it on every path out, so nothing
appears under `~/Pictures`. A QR code is a secret more often than not, a Wi-Fi password or a
pairing token, so its text goes to the clipboard and nowhere else, without the trailing newline,
and the notification says only that a code was copied, never what it says.

Every argument is read before anything is done. `-h` and `--help` print the usage to standard
output and exit 0. `--geom` is refused with an error and exit 1, before the screenshot directory is
made: selection on this desktop is done with the pointer. Any other option, a second mode or an
unknown mode prints the usage to standard error and exits 1, so a mistyped argument never leaves
the command waiting for a drag. Without Spectacle it says so and exits 1. The `Print` keys run
Spectacle itself; `kdos-shot` is what KRunner's `capture.*` routes run.

### kdos-banner and kdos-getty

`kdos-banner` draws the login banner: the KDOS logo beside the output of `fastfetch --logo none`,
revealed line by line like a CRT beam, followed by the [`kdos oracle`](#kdos-oracle) line when
the terminal is wide enough for it and has two rows to spare below the banner. `--plain` prints the
banner with no animation and without the oracle line, so its output is the same every time and can
be compared. fastfetch takes its parent process for the shell, and that parent is `kdos-banner`, so
the banner runs it with `FFTS_IGNORE_PARENT=1`: the **Shell** row names the login shell and the
**Terminal** row the terminal fastfetch detects past it (`konsole` in Konsole). The same plain banner is printed when standard output is not a terminal, when
`KDOS_NO_ANIM` is set, when `TERM` is unset or `dumb`, or when the terminal is too short for the
banner. While it animates, any key skips the rest.

`kdos-getty <ttyN> <getty...>` wraps each console getty in `/etc/inittab`. It puts the console
back into text mode first, because the splash leaves tty1 in graphics mode for the display manager,
and a getty on a console in graphics mode would wait at a prompt nobody can see. It then waits for
the kernel's framebuffer console to take over the terminal, loads the `ter-kdos32n` font and the
accent's sixteen colours from `/etc/vtrgb`, then runs the getty. Loading earlier would be undone,
because the take-over resets every console to the kernel's built-in font. Limits, control groups
and the session itself are not its job: every login below it is a PAM login, and `pam_limits` and
`pam_elogind` do all three.

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
kdos-sfx login | notify | error | degauss | logout | warning | complete | added | removed
kdos-sfx --route
kdos-sfx render <dir> [--format oga|wav]
```

The machine's sounds, synthesised from square and triangle waves in the program itself, so there
are no sample files. `render` writes them out once, at build time, as the KDOS sound theme's files
(the `kdos-sounds` port runs it): the freedesktop names Plasma plays, `desktop-login`,
`desktop-logout`, `message-new-instant`, `dialog-error`, `dialog-warning`, `complete`,
`device-added` and `device-removed`, so the session and this command make the same noise from the
same numbers. Played by name, a sound goes through `pw-cat` when PipeWire is running and through
`aplay` otherwise (the console case), and the command returns at once while a child process plays;
`--route` prints which it would use. With no audio device, no player or no session it exits 0
without a word, because a decoration that prints an error at every login is one people remove.
`$KDOS_NO_SFX` set to anything mutes it.

### kdos-mpctl

```sh
kdos-mpctl toggle | stop | next | prev
kdos-mpctl mpris
```

mpd speaks its own protocol and no MPRIS, and Plasma reaches a player only through MPRIS: the media
keys, the Media Player applet, the lock screen's controls and KDE Connect all ask the session bus.
`mpris` is the bridge: a small service, started at login by the autostart entry
`kdos-mpris.desktop` (only where mpd is installed), that owns `org.mpris.MediaPlayer2.mpd` while mpd
is running and answers for it, following mpd's own `idle` so the applet's title and play state
change when mpd's do, without polling. With no mpd it holds no name and looks again every five
seconds, so mpd started, stopped and restarted under it keeps its media keys; it ends with
`SIGTERM` or with the session bus.

The transport verbs are what a script or a KRunner line runs. mpd (over its socket at
`$XDG_RUNTIME_DIR/mpd/socket`, never TCP) and every MPRIS player on the session bus are ranked on
one scale, playing 3, paused 2, anything else 1, and the highest takes the key, mpd winning a tie:

- a playing mpd always takes it;
- a paused mpd beats a paused MPRIS player;
- a stopped mpd yields to any MPRIS player that is playing or paused, but keeps the key over a
  stopped one, because its queue is the music this login set up;
- among MPRIS players, the first at the best rank wins.

One player answers each key, never two, or a toggle would start one while it paused the other.
`toggle` sends a stopped mpd `play`, because mpd's bare `pause` does nothing while stopped. A player
that does not answer within two seconds costs that key press and nothing else. With no mpd and no
player on the bus it prints `no player on this login` and exits 1.

The image ships no `mpd.conf`, and writing one is what starts mpd at the next login (or at once,
with `service --user start mpd`). The user service makes `$XDG_RUNTIME_DIR/mpd` before it starts
mpd, and `/etc/profile.d/45-mpd.sh` sets `MPD_HOST` to the socket in it, so `mpc`, `rmpc` and
`ncmpcpp` find the same daemon. A configuration that serves that socket and plays through
PipeWire, as `~/.config/mpd/mpd.conf`:

```text
music_directory    "~/Music"
playlist_directory "~/.local/share/mpd/playlists"
db_file            "~/.local/share/mpd/database"
state_file         "~/.local/share/mpd/state"
bind_to_address    "$XDG_RUNTIME_DIR/mpd/socket"
audio_output {
    type "pipewire"
    name "PipeWire"
}
```

mpd expands `~` and `$XDG_RUNTIME_DIR` itself. The directories under `~/.local/share/mpd` must
exist before the first start.

`libbasu`, the D-Bus library, is loaded at run time rather than linked, because this binary is
also `ksvc`, `kdos-getty` and the `kdos-bootctl` the initramfs copies with a hand-kept library
list.

## See also

- [Administration](../02-user-guide/administration.md): these commands in the jobs they belong to
- [Command index](../06-reference/command-index.md): every command on the system
- [KWin](kwin.md): the compositor `hey` asks for its windows, Phosphor mode and the shortcuts
- [Plasma's shell](plasma-shell.md): the desks, KRunner and the tray applets these commands feed
- [KDOS's settings pages](kdos-settings.md): the pages that read `phosphor`, `desk` and the boot state
- [Discover](discover.md): the graphical side of `kdos update` and `kdos app`
- [The daemons](daemons.md): what `doctor` checks and `ksvc` supervises
- [Theming](../02-user-guide/theming.md): `kdos theme` and `kdos phosphor` from the user's side
- [Applications](../02-user-guide/applications.md): `kdos app` from the user's side
- [kdos-appbox](kdos-appbox.md): the program `kdos app` drives
- [Boot and init](../03-architecture/boot-and-init.md): A/B slots, persistence and service scripts
- [Packaging](../03-architecture/packaging.md#updating-a-machine): what an update is, in full
- [How KDOS is built](../05-developer/how-kdos-is-built.md): the build that `kdos rebuild` reruns
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#updates): how `kdos update` and
  `kdos rebuild` compare with the way other distributions update and self-host
- [The ports catalogue](../06-reference/ports-catalogue.md): every port `kdos update` and `kdos cve`
  compare against

<!-- book-nav -->
---

*Part IV — Programs, chapter 28.* Previous: [27. kinstall](kinstall.md) · [Contents](../README.md) · Next: [29. kdos-bb](kdos-bb.md)
