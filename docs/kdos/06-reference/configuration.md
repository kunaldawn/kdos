# Configuration

This chapter lists every configuration file on a KDOS system that a person may edit, every key in
each one, its default, and when a change takes effect. It also lists the defaults KDOS gives
Plasma and the other KDE programs, the files the image ships for third-party programs, the
environment a login sets, and the generated files that must not be edited. It is for anyone who
wants to change how the desktop looks or behaves and for whoever administers the machine. It is a
reference: the chapters on [the session](../03-architecture/session.md) and
[administration](../02-user-guide/administration.md) explain the same files in the context of the
jobs they belong to.

There is no settings database. KDOS's own files are read directly by the program that acts on
them, so the file *is* the setting. Plasma and the KDE applications keep their settings in KConfig
files, plain INI files under `~/.config`, and System Settings writes those same files; editing them
by hand, or with `kwriteconfig6`, gives the same result.

**How to use this page.** Find the file in the contents below, or search for the key. Each section
says who reads the file, what every key does, and when an edit starts to count. Read
[Where configuration lives](#where-configuration-lives) and
[When a change takes effect](#when-a-change-takes-effect) first: every table afterwards uses their
terms. For the paths, sockets and bus names these files mention, see
[Filesystem and IPC](filesystem-and-ipc.md). Terms such as *accent*, *desk*, *box* and *pack* are
defined in the [Glossary](glossary.md).

## Where configuration lives

| Tier | Path | Belongs to |
|---|---|---|
| Machine | `/etc/kdos/`, `/etc/`, `/etc/xdg/` | The administrator |
| Distribution defaults | `/usr/share/kdos/xdg/`, with the overlays `/usr/share/kdos/xdg-vm/` and `/usr/share/kdos/xdg-live/` | The image. An update replaces them, so change a default by writing the same key in `/etc/xdg` or in your own file |
| Per user | `~/.config/kdos/`, `~/.config/kdosrc`, and the KConfig files of Plasma and the KDE programs (`~/.config/kwinrc`, `~/.config/kdeglobals` and the rest) | You |
| Per user, generated | The terminal programs' colour files listed under [Generated files you should not edit](#generated-files-you-should-not-edit) | `kdos theme`. Edits are overwritten |
| State, not configuration | `~/.local/state/kdos/`, `~/.cache/kdos/`, `/var/lib/kdos/` | The programs that write them |

A new account is populated from `/etc/skel`. Editing a file there changes the defaults for accounts
created afterwards and leaves existing accounts alone.

**How a KDE setting is found.** A KDE program reads a KConfig file by its name from every
directory on `XDG_CONFIG_HOME` and `XDG_CONFIG_DIRS` and merges them key by key, the first
directory winning. A KDOS session's order is:

1. `~/.config` — what you, System Settings and `kdos` write
2. `/usr/share/kdos/xdg-live` — on the live medium only
3. `/usr/share/kdos/xdg-vm` — in a virtual machine only
4. `/etc/xdg` — the administrator's
5. `/usr/share/kdos/xdg` — KDOS's defaults

Plasma also puts `~/.config/kdedefaults`, which it writes when a global theme is applied, in front
of the system directories. `/etc/profile.d/10-wayland.sh` appends `/usr/share/kdos/xdg` to every
login, and `kdos_session_xdg_dirs` in `/usr/local/lib/kdos/session-common.sh` adds `/etc/xdg` when
missing and the two overlays where they apply, for the session SDDM starts. A key marked `[$i]` in a
system file is locked: nothing later in the order can change it.

Where a program honours `$XDG_CONFIG_HOME`, `~/.config` below means that directory. Two files are
always looked up under `$HOME/.config` whatever that variable says: box profiles and `kdos/a11y`.

## When a change takes effect

Where a key table has an *Applies* column, it uses one of these words. The other tables say in
prose when a change counts.

| Word | Meaning |
|---|---|
| immediate | At once: the program watches the file, or the command that writes it tells the program |
| next start | The next time that program starts |
| next login | Read once per session |
| create time | When a box is created. A running box keeps what it was created with |
| boot | Read once during startup |
| next launch | The next time an application in that box is launched |
| next collection | The next `kdos-box gc` pass |
| — | Recorded only; nothing acts on the key |

A KDE program reads its KConfig files when it starts, and most watch them while they run. KWin
and Plasma pick up a hand edit of their own files on a reconfigure: `qdbus6 org.kde.KWin /KWin
reconfigure` for KWin, and logging out and in for the rest. A change made through System Settings
or through `kdos` is announced to the running programs as it is made.

## File format

Most KDOS configuration files are `key = value`, one per line; the few that are not (`favorites`,
`timers.d`, and the files whose presence alone is the setting) show their format in their own
section. A line whose first non-blank character is `#` is a comment. The files are *parsed, never
sourced*: there is no shell, no command substitution and no variable expansion. A `#` after a value
is part of the value, not a comment, except in `backup.conf`, where a `#` anywhere on a line starts
a comment, so a path there cannot contain `#`.

Where a key takes yes or no, `yes`, `true`, `on` and `1` mean yes, and `no`, `false`, `off` and `0`
mean no, in lower case. Box profiles and sandbox profiles report a key they do not recognise, by
name, on standard error.

KDOS files that belong to a KDE program (`kdosrc`, `kdos/phosphor`, the files under
`/usr/share/kdos/xdg`) are KConfig files: `[Group]` headers, `Key=value` lines with no spaces round
the `=`, and `[Group][Subgroup]` for nested groups.

---

## The desktop

### `~/.config/kdos/accent`

One word, the accent in force: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect`
or `paper`. `kdos theme <accent>` writes it; `kdos theme` with no argument prints it; and the
`kded-kdos` module writes it through `kdos theme --from-plasma` when you pick a KDOS global theme in
System Settings. Ships absent, which means `bone`, the default accent.

Do not edit it by hand: the word is what `kdos theme` compares against when it decides which files
still name a KDOS default, and an accent changed here alone leaves Plasma, the boot menu and the
terminal programs in the old one. Run `kdos theme <accent>` instead. See
[`kdos theme`](../04-programs/kdos-command.md#kdos-theme) and [Theming](../02-user-guide/theming.md).

### `~/.config/kdos/phosphor`

Written by `kdos phosphor on`, and the record of what it changed. A KConfig file with two groups:
`[Saved]` holds the value each key it was about to change had in your own file, and `[Unset]` names
each key your file did not have. `kdos phosphor off` writes each one back and deletes this file, so
switching the mode off returns your files to what they were, key for key. While the file exists,
Phosphor mode is on.

The keys the mode changes are KWin's: `[Plugins]` `kdos_crtEnabled`, `kdos_unfoldEnabled`,
`kdos_loginEnabled` and `kdos_quitEnabled` (with `loginEnabled` and `sessionquitEnabled` off, so
the KDOS motion replaces KWin's own), and the effect's settings under `[Effect-kdos_crt]` in
`kwinrc`; the accent; and, with the Terminus option, the fixed-width font in `kdeglobals` and the
Konsole profile. Do not edit it while the mode is on. See
[`kdos phosphor`](../04-programs/kdos-command.md#kdos-phosphor).

### `~/.config/kdosrc`

KDOS's own KConfig file, for the settings that belong to the desktop rather than to one program.

| Group | Key | Means |
|---|---|---|
| `[Desk]` | `Layout` | The desk in force: `kdos`, `dock` or `tiling`. `kdos desk <name>` and the Welcome Center's desk page write it |
| `[Desk]` | `LaidOut` | When a desk was last laid out, in seconds since 1970, written by the desk's layout script. `kdos desk` compares it with the panels file to tell whether you have arranged the panels since |

`/usr/share/kdos/xdg/kdosrc` does not exist; a system default would go in `/etc/xdg/kdosrc`. Before
`kdos desk` replaces the panels it copies them to `~/.config/kdos/desk-undo/` (which `kdos desk
undo` restores) and to a dated directory under `~/.local/share/kdos/desk-backup/`, which nothing
ever removes. See [`kdos desk`](../04-programs/kdos-command.md#kdos-desk).

### `~/.config/kdos/favorites`

The boxes started ahead of time at login, and the two-letter codes of the KDOS KRunner runner. One
desktop-entry identifier per line — the entry's file name under `/usr/share/applications` or
`~/.local/share/applications`, without `.desktop` — or a box's name. Blank lines and lines starting
with `#` are skipped. It ships with four rows:

```
org.kde.konsole code=TE
org.kde.dolphin code=FM
firefox-esr code=FF
org.kde.kate
```

**Warm-up.** At every login, before Plasma starts, `kdos-appbox warmup` starts the box behind each
line, with its bus proxy, so the first launch of the session finds it running rather than paying
for a cold start. A boxed application's launcher id (`app.<id>`, or `app.<id>.<launcher>` for a
pack with several) names its box, and so does a box's name; a host application's id names no box
and is skipped. At most eight boxes are started, in the order listed, and a box that stays idle
past its profile's `autostop` is stopped again by `kdos-box gc`.

**Two-letter codes.** A line may carry `code=XX`, two letters or digits. Typing those two letters
alone in KRunner offers that entry first, through the KDOS runner. The runner reads your file, or,
when you have none, the first `kdos/favorites` along `XDG_CONFIG_DIRS`; it takes at most 64 lines,
and a line whose application is not installed is skipped silently.

Every reader takes the first word of a line as the identifier and ignores anything after it but a
`code=` word. The panel's pinned launchers are Plasma's own (right-click a task, then *Pin to Task
Manager*) and are not read from here. The desks pin the same applications this file ships with, so
a fresh account whose pinned application is boxed starts that box warm.

### `/etc/kdos/menu.conf`, merged under `~/.config/kdos/menu.conf`

A *route* is a stable name for a place in the system, such as `setup.network`, that a script, a
document or another program can refer to. `kdos menu summon setup.network` opens the network
settings, and keeps working when a shortcut is rebound or a settings page moves. Typing a route's
name in KRunner offers it through the KDOS runner.

One `route = command` per line:

```
setup.network      = systemsettings kcm_networkmanagement
style.font         = systemsettings kcm_fonts
toggle.night       = kdos toggle night-light
```

The name is `verb.noun`, and the verb describes what you are doing rather than which program does
it: someone looking for Wi-Fi is setting something up, whichever page owns it. The system file
ships 35 routes under eight verbs: `setup` (10), `system` (9), `style` (4), `capture` (4),
`toggle` (3), `learn` (2), `share` (2) and `about` (1):

| Route | Runs |
|---|---|
| `setup.network` | `systemsettings kcm_networkmanagement` |
| `setup.bluetooth` | `systemsettings kcm_bluetooth` |
| `setup.sound` | `systemsettings kcm_pulseaudio` |
| `setup.displays` | `systemsettings kcm_kscreen` |
| `setup.devices` | `kinfocenter` |
| `setup.printers` | `systemsettings kcm_printer_manager` |
| `setup.users` | `systemsettings kcm_users` |
| `setup.time` | `systemsettings kcm_clock` |
| `setup.tui` | `kdos app tui` |
| `setup.applications` | `plasma-discover` |
| `style.theme` | `systemsettings kcm_kdos_look` |
| `style.font` | `systemsettings kcm_fonts` |
| `style.background` | `systemsettings kcm_wallpaper` |
| `style.desktop` | `systemsettings kcm_lookandfeel` |
| `system.settings` | `systemsettings` |
| `system.monitor` | `plasma-systemmonitor` |
| `system.power` | `konsole --hold -e kdos-energy` |
| `system.disks` | `partitionmanager` |
| `system.burn` | `isoimagewriter` |
| `system.connect` | `kdeconnect-app` |
| `system.boxes` | `systemsettings kcm_kdos_boxes` |
| `system.slots` | `systemsettings kcm_kdos_system` |
| `system.notifications` | `systemsettings kcm_notifications` |
| `learn.keys` | `systemsettings kcm_keys` |
| `learn.docs` | `kdos-doc` |
| `capture.region` | `kdos-shot region` |
| `capture.screen` | `kdos-shot screen` |
| `capture.record` | `spectacle` |
| `capture.qr` | `kdos-shot qr` |
| `share.file` | `kdos-share` |
| `share.clipboard` | `kdos-share --clipboard` |
| `toggle.night` | `kdos toggle night-light` |
| `toggle.quiet` | `kdos toggle dnd` |
| `toggle.awake` | `kdos toggle stay-awake` |
| `about.system` | `kinfocenter` |

The command is split on spaces and run without a shell. There is no quoting, so a route cannot
become a way to run arbitrary shell code from a file you own.

The system file is read first and yours second. A route named in both takes your value; a route
only in yours is added. Routes cannot be deleted, so a name a script depends on keeps resolving. A
key beginning `@` is skipped.

### The switches `kdos toggle` sets

`kdos toggle` keeps no state of its own: each switch is the setting the Plasma control for it
writes, so the applet shows what `kdos toggle` set, and a click there undoes it.

| Toggle | Where the state is |
|---|---|
| `stay-awake` | An inhibition of sleep and of the screen locker, held by a `kdos` process for as long as the switch is on; its pid is in `$XDG_RUNTIME_DIR/kdos/stay-awake.pid`, and the battery applet lists it as blocking |
| `night-light` | KWin's Night Light, flipped through its own *Toggle Night Color* shortcut |
| `dnd` | `~/.config/plasmanotifyrc`, `[DoNotDisturb]` `Until`, set far ahead; deleted to switch it off |
| `reduced-motion` | `~/.config/kdeglobals`, `[KDE]` `AnimationDurationFactor=0`; deleted to switch it off, so the shipped factor applies again |

`kdos toggle` lists them, `kdos toggle <name>` flips one, and `kdos toggle <name> on|off` sets it.
The routes `toggle.night`, `toggle.quiet` and `toggle.awake` do the same. See
[`kdos toggle`](../04-programs/kdos-command.md#kdos-toggle).

### `~/.config/kdos/backup.conf`

What `kdos backup run` copies with [restic](https://restic.net), and where to. `include` and
`exclude` may repeat, up to eight of each; any other key repeated keeps its last value. The paths
become separate arguments, so a directory with a space in its name stays one path. An `exclude` is
a restic pattern and is passed as written: `*.iso` excludes every ISO image.

| Key | Default | Means |
|---|---|---|
| `repo` | — | The restic repository |
| `password-file` | `~/.config/kdos/backup.pass` | The file holding the repository's password |
| `include` | — | A directory to back up. Repeatable, eight at most |
| `exclude` | — | A pattern to leave out. Repeatable, eight at most |

The file ships with every line commented out, because the first run over a home directory can take
hours and should be a choice. Every snapshot it makes is tagged `kdos`.

To set it up:

1. Create the repository: `restic -r <repo> init`. It asks for a password twice.
2. Put that password alone in `~/.config/kdos/backup.pass` and make it mode 0600
   (`chmod 600 ~/.config/kdos/backup.pass`). `kdos backup run` refuses to run while the file is
   readable by group or others.
3. Uncomment `repo` and the `include`/`exclude` lines you want.
4. Run `kdos backup run`, or uncomment the nightly line described in the next section.

**Kup.** The Backups page of System Settings is Kup, and it may use the same repository. Its plans
are in `~/.config/kuprc`; a plan with `[Plan/N]` `Backup type=2` is a restic plan, with
`Restic use password file`, `Restic password file` and `Restic forget` beside it. Kup's own pruning
forgets only the snapshots it tagged `kup`, so neither ever removes the other's.

### `/etc/kdos/timers.d/` and `~/.config/kdos/timers.d/`

Periodic jobs. Each `*.timer` file holds one job per line:

```
NAME  TIMESPEC...  --  COMMAND...
```

`TIMESPEC` is the options of [`snooze`](https://github.com/leahneukirchen/snooze), unchanged:
`-H 9 -M 0` for nine in the morning, `-d /2` for every second day. `snooze(1)` is the full
reference. `--` marks where the timespec ends and the command begins.

The command is an argument vector: no pipe, no redirection, no `&&`. That is what lets the file be
parsed rather than run as a script. It also means an argument cannot contain a space: the line is
split into words and never unquoted, so `-- kdos notify "Good morning"` reaches the command as
`"Good` and `morning"`. A program that needs a sentence reads it from a file; `kdos remind` writes
its message as its timer file's first comment line for this reason.

**Missed runs.** `-s SLACK` lets a job run late: a machine asleep at the slot runs the job once when
it wakes, if that is within the slack. It does not cover a machine that was switched off, because a
`snooze` started at boot looks for the next slot from that moment. `-t FILE` makes it look from that
file's modification time instead, and the slack then covers the gap.

There are three tables, and they differ in when a job can fire:

| Table | Started by | Fires |
|---|---|---|
| `/etc/kdos/timers.d/` | The system supervisor, at boot (`/etc/init.d/18_timers.sh`), as root | Every occurrence, whether or not anybody is logged in |
| `/etc/kdos/user-services/timers.d/` | The `timers` user service, for every user | From a user's first login, on any console, over SSH or in Plasma, to the last logout |
| `~/.config/kdos/timers.d/` | The `timers` user service, for you | The same; `kdos remind` writes its reminders here |

`snooze` waits for its slot, runs the command once and exits, so something has to start it again.
For the system table the supervisor does that; each system job is a service named
`kdos-timer-NAME`. For the two user tables one supervised process,
`/usr/local/lib/kdos/user-timers`, runs a loop per row; `service --user restart timers` reads both
tables again after an edit, and the user services' stop at the last logout ends every loop. A job
of the user tables keeps running past a logout only for a user `loginctl enable-linger` names. A
job's standard output is discarded and its standard error kept, in the service's log under
`$XDG_RUNTIME_DIR/ksvc` and in syslog.

Before a row's loop starts, the command's first word must be on `PATH` and `snooze -n` must accept
the timespec. A row that fails either check is reported and not started.

Choose the table by who must be there: a job that must run on a machine nobody has logged into goes
in the system table; a job that writes into `$HOME` goes in yours.

Shipped jobs, at deliberately different times so that a laptop waking up is not running several
at once:

| File | Table | When | Runs |
|---|---|---|---|
| `/etc/kdos/timers.d/10-update-check.timer` | System | 04:17, slack 8 h | `kdos update check --out /var/lib/kdos/update.json`, which reads what the last fetch left and reaches no network |
| `/etc/kdos/timers.d/20-fstrim.timer` | System | 12:47, slack 20 h | `fstrim -a` |
| `/etc/kdos/user-services/timers.d/40-box-gc.timer` | Every user | every ten minutes | `nice -n 10 kdos-box gc` |
| `~/.config/kdos/timers.d/20-updatedb.timer` | Yours | 03:05, slack 8 h | `kdos-updatedb` |
| `~/.config/kdos/timers.d/10-backup.timer` | Yours, commented out | 03:40, slack 8 h | `kdos backup run` |

No shipped timer downloads anything: `kdos update fetch` and `fwupdmgr refresh` run when you ask.

### `~/.config/kdos/a11y`

Ships absent. Its existence — an empty file is enough — gives boxed applications the
accessibility bus whether or not a screen reader is running. Without it a box is given the bus only
when assistive technology is in use: the screen reader is on in System Settings, or the running
accessibility registry reports a tool attached. `KDOS_A11Y=1` or `0` decides one launch. The bus is
unfiltered, which is why it is not given by default: a client on it can read the text of every
other application on it. A box's bus policy is set when the box starts, so a screen reader switched
on later reaches a box from its next start. See [Accessibility](../02-user-guide/accessibility.md).

### `~/.config/kdos/checklist`

The first-run checklist's own state: the items you dismissed, written by `kdos checklist dismiss`
and the checklist widget, and cleared by `kdos checklist restore`. Whether an item is done is never
stored; it is measured each time the list is asked for. See
[`kdos checklist`](../04-programs/kdos-command.md#kdos-checklist).

### Window memory

The KDOS window memory script opens a program's window where its last one closed. Its settings are
in `kwinrc`, under `[Script-kdos_window_memory]`, and System Settings' KWin Scripts page edits them:

| Key | Default | Means |
|---|---|---|
| `Exclude` | — | Window classes the script leaves alone, comma-separated |
| `RememberDesktop` | `false` | Whether a window also returns to the virtual desktop it closed on |

The remembered places are kept in `$XDG_STATE_HOME/kdos/window-memory`.

---

## Plasma's files KDOS ships

KDOS's defaults for Plasma and the KDE programs are KConfig files in `/usr/share/kdos/xdg`, the
last directory on `XDG_CONFIG_DIRS`. Each sets only the keys KDOS decides; every other key keeps
upstream's default. A file of the same name in `/etc/xdg` is read before it, and your own in
`~/.config` before both, key by key.

| File | Read by | Sets |
|---|---|---|
| `kdeglobals` | Every KDE and Qt 6 program | The look-and-feel `org.kdos.bone.desktop` (dark) and `org.kdos.paper.desktop` (light), not switched automatically; the fonts, Noto Sans 10 for text and JetBrains Mono 10 fixed-width; Konsole as the terminal; the icon theme `KDOS` and the sound theme `kdos`; animations at half length (`AnimationDurationFactor=0.5`); double-click to open; and `ghns=false` under `[KDE Action Restrictions]`, locked, so no *Get New…* button downloads anything. Installed by `kdos-plasma-look`, with the greeter's colours generated into it |
| `kwinrc` | KWin | Four virtual desktops in one row, named `main`, `www`, `hack` and `misc`; one window switcher entry per application; auto-tiling (Krohnkite) off; and fcitx5 as the input method KWin starts |
| `kglobalshortcutsrc` | `kglobalacceld` | The KDOS chords beside KDE's defaults: `Meta+Q` closes a window, `Meta+F1` and `Meta+/` show the key card, `Meta+Return` and `Ctrl+Alt+T` open Konsole, `Alt+Space`, `Alt+F2` and `Meta+Space` open KRunner, `Print`, `Shift+Print` and `Meta+Shift+S` take screenshots, `Meta+R` and `Alt+Print` record. A shortcut you set in System Settings is yours and is kept |
| `kscreenlockerrc` | The screen locker | Lock after 10 minutes idle and on resume, with 5 seconds' grace; the clock and the media controls on the lock screen; the bone wallpaper behind it |
| `powerdevilrc` | PowerDevil | On mains power, dim after 5 minutes and switch the screen off after 15 |
| `plasmarc` | `plasmashell` | The Plasma style `kdos` |
| `breezerc` | The Breeze window decoration | Square corners with an outline, no title-bar gradient, the title on the left |
| `startkderc` | `startplasma` | `systemdBoot=false`: the session is started by `startplasma` itself, with no systemd user manager |
| `kaccessrc` | `kaccess` | `[ScreenReader] SystemdManaged=false`, locked, so turning the screen reader on starts Orca directly |
| `kcm-about-distrorc` | About this System | The KDOS logo, the project's address, the release from `/etc/os-release`, and the KDOS rows (slot, update, tree, accent, C library, boxes) from the `about-*` scripts in `/usr/libexec/kdos` |
| `baloofilerc` | The file indexer | Basic indexing only, file names and metadata, never contents; package caches, containers and KDOS's own data left out |
| `discoverrc` | Discover | The KDOS backend first; no ratings fetched from KDE's review server; no offline updates |
| `PlasmaDiscoverUpdates` | Discover's update notifier | No unattended updates; at most one notice a day |
| `freespacenotifierrc` | Plasma's free-space notifier | Warn when a volume has 10% of its size free, or 100000 MiB on a volume of more than about a terabyte: the notifier warns below the smaller of its two thresholds, so the fixed one is raised to its maximum |
| `systemmonitorrc` | System Monitor | The pages Overview, KDOS, Applications, History and Processes, in that order |
| `spectaclerc` | Spectacle | Starting it from the menu takes a full-screen shot |
| `knightsrc` | Knights | Stockfish as both engines |
| `plasma_workspace.notifyrc` | Plasma's notifications | The KDOS sounds for login and logout |
| `kde-mimeapps.list` | Every program that opens files, inside Plasma and out | The default application for each file type; see [Default handlers](#default-handlers) |
| `qt5ct/qt5ct.conf` | Qt 5 programs, through qt5ct | The bone palette, the KDOS icons, the Fusion style and the desktop's fonts, copied into `~/.config/qt5ct` by the first Qt 5 program you start; `kdos theme` keeps that copy in the accent |
| `plasma-workspace/env/kdos.sh`, `kdos-shortcuts.sh` | `startplasma`, before anything else starts | The session's environment, and the one-time seeding of the KDOS chords into your `kglobalshortcutsrc`, recorded in `$XDG_STATE_HOME/kdos/kglobalshortcutsrc.seeded` |
| `plasma-workspace/shutdown/kdos.sh`, `kdos-boxes.sh` | `plasma-shutdown`, at Log Out, Restart and Shut Down | The session's own cleanup, and `kdos-appbox session save` |
| `autostart/` | Plasma's autostart | The entries of `kdos-notice` and the firewall's question, `kdos-fwask` |

**The overlays.** Two directories go in front of `/etc/xdg`, each only where it applies; your own
`~/.config` still outranks them, so you can turn either setting back on.

| Directory | Applies | Sets |
|---|---|---|
| `/usr/share/kdos/xdg-vm` | In a virtual machine (`kdos in-vm`) | No idle dimming, no screen blanking and no timed lock, which from outside the guest look like a hung machine |
| `/usr/share/kdos/xdg-live` | On the live medium | No file index, no timed lock and no wallet for an account with no password; no update notices; and the Welcome Center on its live page, offering the installer |

**What `/etc/xdg` carries.** The image's own `/etc/xdg` holds the files every desktop must read, not
only Plasma:

| Path | Sets |
|---|---|
| `/etc/xdg/kdeglobals` | `LookAndFeelPackage=org.kdos.bone.desktop`, so a program run outside the session, under `sudo` or `pkexec`, still wears the KDOS look. The installer rewrites it with the accent you choose |
| `/etc/xdg/konsolerc` | Konsole's default profile, `KDOS Bone` when Plasma is dark and `KDOS Paper` when it is light |
| `/etc/xdg/mimeapps.list` | The handlers for the archive types no KDE program opens: `kdos-openarchive` |
| `/etc/xdg/fcitx5/config` | The keys that move between input-method groups, `Super+Alt+Space` and `Super+Alt+Shift+Space` |
| `/etc/xdg/autostart/kdos-boxes-restore.desktop` | `kdos-appbox session restore` at login: the boxes that were running when the last session ended |
| `/etc/xdg/autostart/kdos-session-good.desktop` | `kdos-notice --bless` at login: confirms the booted system version once Plasma has stayed up |
| `/etc/xdg/autostart/kdos-mpris.desktop` | `kdos-mpctl mpris`, the bridge that puts mpd on the session bus for the media keys and the Media Player widget |
| `/etc/xdg/autostart/kdos-speech-sync.desktop` | `kdos speech sync` at login: offers Klipper's *Read Aloud* action while a voice is installed, and takes it away when none is |
| `/etc/xdg/menus/applications-merged/kdos-boxes.menu` | A *Boxes* submenu in the application launcher for the boxed applications |

The installer writes `/etc/xdg/kdeglobals`, `/etc/xdg/kdosrc` and `/etc/xdg/kxkbrc` with the
accent, the desk and the keyboard chosen at install, so every account on the machine starts with
them; `/etc/xdg/kdeglobals` is the only one of the three the image ships.

---

## Programs

### `~/.config/kdos/boxes/<name>.conf`

One file per box — the container a boxed application runs in (see
[Packs and boxes](../03-architecture/packs-and-boxes.md#the-box)). Most keys map onto a
container-engine flag or onto something KDOS enforces itself; a few are only recorded, and the
*Applies* column below marks them `—`. `kdos-box profile <name>` prints what each key turns into,
and says so where a value cannot be enforced. Edit these with `kdos-box profile`, or on System
Settings' Boxes page, which runs that command rather than writing the file itself. A box with no profile
file takes every default below, which is what a plain `distrobox create` gives.

A box is made on one of two *lanes*, chosen by `base`: the *pack lane* (`pack:<id>`) creates it with
`podman create --rootfs` over a composed pack, and the *store lane* (`image:<ref>`) with
`distrobox create` over a container image. The lanes honour different subsets of the keys below;
[kdos-appbox](../04-programs/kdos-appbox.md#two-ways-an-application-reaches-a-box) has the full
flag-by-flag mapping.

Namespace keys (`network`, `ipc`, `processes`, `devices`, `home`) are yes-or-no in effect: `private`
(or any yes spelling) gives the box its own; anything else shares the host's.

| Key | Default | Applies | Means |
|---|---|---|---|
| `base` | — | create time | `pack:<id>`, `image:<ref>` or `box:<name>`. `kdos-box create` resolves `box:<name>` to that box's own `pack:` or `image:` base and saves the result in the profile, so the new box starts from the same software with an empty writable layer; a box with neither base is refused |
| `image` | — | create time | The registry reference of an image base. `kdos-box create` copies it from `base = image:<ref>`, replacing whatever the line held, so set `base` rather than this |
| `persistence` | `persistent` | — | `persistent`, `ephemeral` or `frozen`. Recorded only: no launch reads it, and a box's writes land wherever its runtime puts them |
| `network` | `host` | create time | `host`, `private`, or `none`. `none` is `--network none` on both lanes: a private namespace with no interface at all. A store-lane box with `private` gets `--unshare-netns`, its own namespace with an interface |
| `ipc` | shared | create time | IPC namespace |
| `processes` | shared | create time | PID namespace |
| `devices` | shared | create time | Whether the host's `/dev` and `/sys` are shared |
| `home` | shared | create time | `private` gives the box its own home under `$XDG_DATA_HOME/kdos/boxes/<name>` (by default `~/.local/share/kdos/boxes/<name>`) instead of your `$HOME` |
| `init` | `no` | create time | Store lane only: `yes` passes `--init` to `distrobox create`, which runs the image's own init system as the container's first process. A pack box always runs KDOS's `kdos-boxinit` first, whatever this says |
| `wayland` | `yes` | — | Recorded only. Every launch gives the box its own tagged Wayland socket through `kdos-boxsock`, whatever this says. The profile printer says so |
| `audio` | `yes` | — | Recorded only. Sound reaches every box through the PipeWire socket in `$XDG_RUNTIME_DIR`, which every box shares; see below |
| `gpu` | `yes` | create time | The graphics card's device nodes |
| `render` | `auto` | next launch | Which graphics the box's applications get: `auto`, `gpu`, or `software` |
| `memory` | — | create time | Memory limit such as `4G`, passed to the engine as `--memory` on both lanes, through `distrobox create --additional-flags` on the store lane. A store-lane value that is not letters, digits and dots is refused with a warning and not applied. The memory daemon also prefers a box over its limit when it has to kill something, since rootless containers often cannot enforce the flag |
| `cpus` | — | create time | CPU quota, as the engine's `--cpus` value, on both lanes, with the same store-lane check as `memory` |
| `pids` | — | create time | Process-count limit (`--pids-limit`), on both lanes |
| `autostop` | `0` | next collection | Running time, counted from when the box started, after which `kdos-box gc` stops it if none of its windows is open: `90s`, `30m`, `2h`, or bare seconds. `0` disables |
| `accent` | `session` | immediate | The box's colour, an accent name. Its windows' title bars take that accent's colour scheme through the KWin window rule `kdos-box-<name>` in `kwinrulesrc`, which `kdos-box profile` and `kdos-appbox genlaunchers` rewrite; `session` gives them the scheme `KDOSBox`. A window that draws its own title bar takes no colour |
| `grant` | — | next client | Desktop features the sandbox otherwise refuses, comma-separated (see below). A running client keeps what it bound |
| `export` | `manual` | — | Recorded only. `auto` does not create launchers by itself: `kdos-appbox genlaunchers` covers every installed pack and store box, and `kdos-box export` covers one application |
| `display` | `window` | — | Recorded only; nothing on this desktop reads it. It is kept so that rewriting a profile does not drop it |

**Namespace keys** apply when the box is created and cannot be changed on a running container, so
changing one tells you to recreate the box rather than silently doing nothing.

**`gpu` and `audio`** cannot subtract anything from a shared `/dev`: there is no engine flag that
grants a box a speaker and denies it a camera, so with `devices = shared` both follow `devices`.
With `devices = private`, `gpu = yes` binds `/dev/dri` back in by itself, and nothing binds the
sound devices back, which leaves the PipeWire socket as the box's way to sound. The profile printer
states each case as it is.

**`render`** resolves by *opening* a `/dev/dri/renderD*` node: hardware where one opens, software
where none does. `gpu` asks for the same, and `software` refuses the card whatever is present. For a
software box the launch sets `LIBGL_ALWAYS_SOFTWARE=1`, which is advisory — an application may
unset it. For a box that can see no device node (`devices = private` with `gpu = no`) the answer is
software without probing. `render` is separate from `gpu` because `gpu` is about device nodes and
`render` is about who draws.

**`grant`** names, one or more of these, in the order the Boxes page lists them:

| Grant | Lets the box |
|---|---|
| `window-list` | List and act on every window (`org_kde_plasma_window_management`) |
| `fake-input` | Type and click into other windows (`org_kde_kwin_fake_input`) |
| `screencast` | Record the screen without asking (`zkde_screencast_unstable_v1`) |
| `data-control` | Read and set the clipboard at any time (`ext_data_control_manager_v1`) |
| `layer-shell` | Draw panels and overlays (`zwlr_layer_shell_v1`) |
| `output-management` | Change the screens' modes and layout (`kde_output_management_v2`) |
| `dpms` | Switch the screens off and on (`org_kde_kwin_dpms_manager`) |
| `plasma-shell` | Place its windows as the shell does (`org_kde_plasma_shell`) |
| `input-method` | Pass KWin's filter for the input-method globals, which KWin still offers only to the input method it started |
| `secrets` | Reach the password store (`org.freedesktop.secrets` on the session bus) |

The first nine are Wayland globals KWin refuses a sandboxed client unless its box's profile names
them; `secrets` is a rule of the box's bus proxy. A name not in this list grants nothing, and
`kdos-box profile` says so. See [KWin](../04-programs/kwin.md#what-a-box-may-bind).

`base = image:` pulls from a registry, so it needs the network, and says so before doing anything.

### `~/.config/kdos/sandbox/<profile>.conf`

Profiles for `kdos sandbox`, which runs a *native* (non-boxed) program under a Landlock ruleset.
Landlock is the kernel's access-control interface that an unprivileged process applies to itself;
the kernel reports which version of it (its ABI) it supports, and rules about TCP need ABI 4. A
profile is named on the command line — `kdos sandbox <profile> -- <cmd>` — and combines with the
command-line options.

| Key | Repeatable | Means |
|---|---|---|
| `read` | yes | A path the program may read |
| `write` | yes | A path the program may read and write |
| `network` | no | `off` denies TCP bind and connect. Any other value leaves the network open. Needs Landlock ABI 4 |
| `tcp-connect` | yes | A TCP port the program may still connect to while `network = off` |

A run takes at most 64 `read` paths, 64 `write` paths and 64 ports, counting the profile's and the
command line's together: a 65th path stops `kdos sandbox` with an error, and ports past 64 are
dropped. A named path that cannot be added to the ruleset, such as one that does not exist, also
stops it. An unknown key is reported and skipped.
`kdos sandbox <profile> --explain` prints exactly what will be asked for, including what this
kernel cannot enforce. Nothing runs unconfined: with no Landlock at all, or with `network = off` on a
kernel below ABI 4, `kdos sandbox` refuses to start the program rather than run it with less
confinement than the profile asked for. See
[`kdos sandbox`](../04-programs/kdos-command.md#kdos-sandbox).

---

## The machine

### `/etc/kdos/packd.conf`

The pack daemon, `kdos-packd`, which installs and mounts application packs.

| Key | Default | Applies | Means |
|---|---|---|---|
| `retain` | `1` | next start | How many superseded versions of a pack the store keeps, at most 8 |

Retention is what makes rollback possible: `1` keeps the version you just replaced. `0` keeps none,
and a rollback then answers that no earlier version is kept. A negative value counts as `0`, and a
value above 8 keeps 8. The environment variable `KDOS_PACK_RETAIN`, when set, overrides the file.

Old versions are removed right after an install and at no other time, so a background job never
deletes a rollback while you are deciding whether to use it.

### `/etc/kdos/zram.conf`

Compressed swap in RAM.

| Key | Default | Range | Applies | Means |
|---|---|---|---|---|
| `size` | `50` | 1–90 | boot | Swap size as a percentage of RAM |
| `algorithm` | `zstd` | a compressor the kernel carries | boot | Compression algorithm |

The service is `/etc/init.d/12_zram.sh`. `size` is how much swap the device may claim to hold, not
how much memory it uses — the compressed pages live in that same memory. Only the leading run of
letters and digits in the `size` value is read: `12.5` is taken as 12, and a run that contains a
letter is ignored, leaving 50. A value outside 1–90 is reported and 50 is used. An algorithm
the kernel does not carry is reported and the kernel's default stands. The device is swap at
priority 100, ahead of any disk swap. While it is active, zswap is turned off (so no page is
compressed twice) and `vm.page-cluster` is 0 (read-ahead on zram is wasted decompression);
stopping the service turns zswap back on and sets `vm.page-cluster` to the kernel default of 3.

### `/etc/kdos/update.conf`

Not shipped. Tells `kdos update` where a newer system comes from, and what an armed slot keeps.

| Key | Default | Means |
|---|---|---|
| `tree` | `https://github.com/kunaldawn/kdos/releases/latest/download` | Where `kdos-ports.tar.xz` and `kdos-ports.tar.xz.sig` are: a URL, or a directory such as a USB stick or an NFS mount |
| `binhost` | the release `<version>` of a tree whose stamp names its release | Where the signed release index `kdos-update.idx` is through which the binary host is found, whose packages `kdos update apply` and `kdos update install` take: a URL, or a directory such as `build/binhost` copied to a stick |
| `image` | the latest KDOS release | Where the signed release index is through which the slot image and its manifest are found, for `kdos update apply --image`: a URL, or a directory such as the one `script/release-image.sh` wrote |
| `carry` | — | A path, relative to the root, that an armed slot keeps from the running one beyond what is always carried; repeatable. Never under `/etc`, which is carried file by file anyway |

Each of `tree`, `binhost` and `image` has an environment variable that overrides the file,
`KDOS_UPDATE_TREE`, `KDOS_UPDATE_BINHOST` and `KDOS_UPDATE_IMAGE`, and `kdos update fetch --from`
overrides both for the tree and the image. Whatever arrives is checked against
[`/etc/kdos/keys/`](#etckdoskeys) before it is used. Sources are not configured here: `kpkg`
fetches them from the KDOS source archive and then upstream, and `KDOS_SOURCES_BASE` moves the
archive as it does for `ports/fetch`. See [`kdos update`](../04-programs/kdos-command.md#kdos-update).

### `/etc/kdos/keys/`

The trusted keys. The directory *is* the policy: every public key (`*.pub`) here is one this machine
accepts a signature from. Trusting a key is copying a file in; removing trust is deleting it. There
is no revocation list and no online check.

| Directory | Trusted for |
|---|---|
| `/etc/kdos/keys/` | Host package indexes (a binhost's `PACKAGES.sig`) and package signatures |
| `/etc/kdos/keys/packs/` | Application packs and pack indexes |
| `/etc/kdos/keys/tree/` | The ports tree `kdos update fetch` installs as `/ports`, and the slot image's manifest |

Keys are read from each directory without descending into subdirectories, which keeps the three
policies separate: a pack key or a tree key cannot become a trusted source of host packages.

The host directory ships with no keys, only a `README` stating the policy. Packages are built from
source on the machine and checked against the `sha256` in their recipe, so nothing needs a
signature until you choose a binhost. To trust one:

```sh
kpkg keygen builder                 # on the machine that builds
cp builder.pub /etc/kdos/keys/      # on every machine that should trust it
```

The pack directory ships one key, `kdos-packs.pub`. A pack or pack index signed with its private
half would be accepted, but nothing the build makes is signed with it and the build does not hold
it, so the key verifies nothing on a fresh system. To trust packs you sign yourself, add your own
`.pub` from `kdos-pack keygen`, and delete `kdos-packs.pub` if you want only yours. See
[Security model](../03-architecture/security-model.md#signing-and-trust).

The tree directory holds `kdos-release.pub`, the public half of the release key of the build that
made the image. The image step copies it from `build/keys/kdos-release.pub`; the secret half,
`build/keys/kdos-release.key`, is what `ports/publish --release` signs each release's tree with. An
image built without `build/keys/kdos-release.pub` ships the directory with only its `README`, and
fetches no tree. Put the key of whoever publishes the trees you follow here.

### `/etc/sddm.conf.d/`

The login screen. SDDM reads every file in this directory in name order, a later file's key
winning.

| File | Sets |
|---|---|
| `00-kdos.conf` | The greeter on Wayland, under `kwin_wayland --no-global-shortcuts --no-lockscreen --inputmethod plasma-keyboard --locale1`; `kdos-session-wrapper` as every session's command; Wayland sessions only; accounts from UID 1000 to 60000; the last user and session remembered; the theme `kdos`, the Breeze cursor at 24 and Noto Sans 10 |
| `kde_settings.conf` | Autologin: `[Autologin]` `User`, `Session=plasma` and `Relogin=true`. Ships naming `kdos`, the live medium's account |

`kde_settings.conf` is the file System Settings' Login Screen page writes, and the installer
rewrites it. On the live medium `Relogin=true` logs the live account in again after it logs out.
An installed machine has an `[Autologin]` group only when it logs in by itself, which the installer
offers on a single-account machine with an encrypted root, with `Relogin=false`, so a person who
logs out meant to; everywhere else it shows the login screen. Applies at the next start of SDDM, which is the next boot or the next return to the greeter.
See [Boot and init](../03-architecture/boot-and-init.md).

### `/etc/elogind/logind.conf.d/50-kdos.conf` and `sleep.conf.d/50-kdos.conf`

elogind's settings, as drop-ins over its own `logind.conf` and `sleep.conf`.

| Key | Value | Means |
|---|---|---|
| `KillUserProcesses` | `no` | A user's processes survive their logout, so a lingering user's boxes and timers keep running |
| `RemoveIPC` | `no` | A user's shared memory and semaphores are kept at logout |
| `DelegateSessionCgroups` | `yes` | Each session gets a cgroup of its own that the user may manage |
| `HandleLidSwitch`, `HandleLidSwitchExternalPower` | `suspend` | Closing the lid suspends, on battery or on mains |
| `HandleLidSwitchDocked` | `ignore` | Closing the lid with an external screen does nothing |
| `HandlePowerKey` | `poweroff` | The power button shuts down |
| `InhibitDelayMaxSec` | `5` | How long a program holding a delay lock may hold up a suspend |
| `AllowHibernation`, `AllowSuspendThenHibernate`, `AllowHybridSleep` | `no` | Suspend to RAM only. KDOS swaps to zram, which is memory and holds nothing across a power-off, so there is no disk image to resume from; with these off, PowerDevil and the Leave menu offer no action that would lose the session |

Applies at the next start of elogind, which is the next boot. Inside a Plasma session PowerDevil
takes the lid and power-key inhibitors and its own settings decide; these keys apply at the login
screen and on a console login.

### `/etc/turnstile/turnstiled.conf` and `/etc/kdos/user-services/`

`turnstiled` starts a user's own services at that user's first login and stops them at the last.
The shipped configuration names `ksvc` as its backend (`/usr/lib/turnstile/ksvc`), makes the runtime
directory `/run/user/<uid>`, exports the session bus address into each login, and lingers only for
a user `loginctl enable-linger` names.

The services themselves are `/etc/kdos/user-services/*.sh`, run in name order by `ksvc` as the
user: `10_dbus` (the session bus at `$XDG_RUNTIME_DIR/bus`), `20_pipewire`, `21_wireplumber`,
`22_pipewire-pulse`, `25_mpd` (only when an mpd configuration file exists), `30_timers` (the user
timer tables above) and `40_boxes` (which stops your running boxes at the last logout).
`service --user list` shows them; `service --user disable NAME` keeps one from starting, recorded in
`~/.config/kdos/user-services.disabled`. See
[ksvc and service](../04-programs/kdos-command.md#ksvc-and-service).

### `/etc/nftables.conf`

The firewall. `/etc/init.d/25_nftables.sh` applies it at boot, before the network starts, after a
syntax check (`nft -c`) — a ruleset that does not load is never half-applied. When a firewall is
already loaded, it stays standing. When none is, as at boot, the script loads this file without
its `/etc/nftables.d` drop-ins, or, when that does not load either, a minimal drop policy written to
`/run/nftables-fallback.nft` (this machine's own connections, loopback, the ICMP and ICMPv6 that
IPv4 and IPv6 need, and the DHCPv6 client), and reports `[FAIL]` either way, so one broken drop-in
never boots a machine with no input filter. The file replaces only its own `inet filter` table, so podman's `inet netavark`
table and NetworkManager's `nm-shared-*` tables survive a reload, and changing the firewall does not
cut off running containers or hotspot clients.

The shipped policy:

- **Input** is dropped by default, and packets in the `invalid` connection state are dropped
  explicitly. Accepted: established and related traffic, loopback, the
  necessary ICMP and ICMPv6 types (including neighbour discovery and router advertisements),
  multicast DNS (UDP 5353), NetBIOS name service (UDP 137 to 137), DHCPv6 client replies (UDP 546),
  and DNS and DHCP from the NetworkManager hotspot range `10.42.0.0/16`.
- **Forwarding** is dropped by default, except established traffic and traffic to or from
  `10.42.0.0/16`, so a hotspot's clients reach the network.
- **Output** is accepted.

Anything else that should be reachable needs a rule; the file has commented examples. Every `*.nft`
in `/etc/nftables.d` is included last and adds to `table inet filter` (a separate table would keep
stale rules across a reload):

| File | Written by | Does |
|---|---|---|
| `40-podman.nft` | Shipped | Lets rootful podman's bridges forward and reach their DNS |
| `40-libvirt.nft` | The `libvirt` package | Lets libvirt's `virbr*` NAT bridges forward and reach their dnsmasq for DHCP and DNS |
| `50-kdos-services.nft` | Shipped empty; rewritten by System Settings' Firewall page and `kdos-power firewall`, through `kdos-powerd` | Opens the services you turned on |

The firewall is the one service `/etc/init.d/rcK` does not stop at shutdown, so the machine is
never unprotected while the other services wind down.

See [The firewall](../02-user-guide/administration.md#the-firewall) for opening a service.

### `/etc/fstab`

Applies at boot. The shipped entries mount the kernel's pseudo-filesystems (`/proc`, `/sys`,
`/dev`, tracefs, debugfs and bpf, the last three `nosuid,nodev,noexec`), the cgroup v2 hierarchy
with `nsdelegate`, and two temporary filesystems, both `nosuid,nodev`: `/tmp` at mode 1777 and
`/run` at mode 0755.

`/tmp` must be mode 1777, or no ordinary user can write there, and every graphical application
depends on it. A tmpfs that is already mounted ignores a mode change on remount, so
`/etc/init.d/rcS` also runs `chmod 1777 /tmp` itself. It creates `/run/lock` at mode 1777 for the
same reason: the serial-port tools take their locks there as the user.

`mount -a` ignores swap lines, so `rcS` turns on any swap entry with `swapon -a`; the installer
writes one for a swap partition or a swap file. The installer keeps every shipped
entry: it writes its own lines (root, the EFI partition, swap) first and the shipped file after
them.

### `/etc/inittab`

sysvinit's `init` reads it at boot, and again on `telinit q`. What each entry starts:

| Entry | Starts |
|---|---|
| `id`, `initdefault` | Runlevel 3, the running system. `S` is single-user, `0` powers off, `6` reboots |
| `si`, `sysinit` | `/etc/init.d/rcS`, which starts every enabled service script in order |
| `~~`, runlevel `S` | `/sbin/sulogin`: root's password, then a root shell on the console; leaving it continues to runlevel 3 |
| `1`, tty1 | `kdos-dm`, which runs SDDM, the graphical login; when SDDM is missing or exits three times in a minute it puts a password prompt on tty1 instead, never an autologin |
| `2`, tty2 | An ordinary `agetty` login through `kdos-getty`, never automatic: the recovery console |
| `S0`, serial line `ttyS0` | `/etc/init.d/serial-getty`, which runs `agetty --autologin root`: root with no password on the live medium. The installer removes `--autologin root` from an installed system's copy, which then asks for a login. With no UART at `ttyS0` (its sysfs `type` is `0`) the script sleeps instead of running `agetty`, which would fail at every respawn |
| `ca`, `ctrlaltdel` | `/sbin/shutdown -r now` |
| `k0` to `k4`, runlevels 0 and 6 | In order, each to completion: `rcK`, `killprocs`, `swapoff -a`, `umount -a -r`, `killpower` |
| `h0`, `h6` | `halt -d -f -p` in runlevel 0, `reboot -d -f` in runlevel 6 |

Both consoles and the serial line respawn when their program ends, and all three belong to
runlevels 2 to 5. On shutdown, init ends them, `/etc/init.d/rcK` runs every enabled service
script's `stop` in reverse order — except the firewall, which stays loaded to the end —
`/etc/init.d/killprocs` ends every process still running, then `swapoff -a` turns swap off,
`umount -a -r` unmounts every filesystem or remounts a busy one read-only, and
`/etc/init.d/killpower` tells a UPS to cut the power when NUT's `upsmon` began the shutdown on a low
battery (its flag file `/etc/killpower` is present); otherwise it does nothing.

### `/etc/service.disabled/<name>`

A marker file, not a setting. Creating one stops the matching init script from running at boot:

```sh
sudo touch /etc/service.disabled/cups
```

The name is the init script's with the order prefix and `.sh` removed: `42_networkmanager.sh` is
`networkmanager`. Delete the file to enable the service again. The image ships one marker, `sshd`:
the SSH server stays off until `kdos-power firewall ssh on`, the Firewall page's switch for it, or
the installer's SSH choice, removes it. `chrony`'s marker is also the state of *Set time
automatically*: `timedate1`'s `SetNTP` creates or removes it and starts or stops the service.

`networkmanager` has a second effect. `30_network` starts `dhcpcd` only when NetworkManager is
absent or disabled this way, so turning NetworkManager off hands DHCP back to `dhcpcd` rather than
leaving the machine with no DHCP client.

The marker is the only switch a supervised daemon needs. No daemon a service script supervises
(NetworkManager, ModemManager and `polkitd` among them) has a D-Bus activation file, so the system
bus never starts one of them itself: a call to the name of one that is stopped or disabled fails
with `org.freedesktop.DBus.Error.ServiceUnknown` and starts nothing. A stopped service therefore
stays stopped, and `service restart` leaves exactly one copy, the supervised one.

### `/etc/locale.conf`, `/etc/keymap` and `/etc/kdos/xkb.conf`

The language and the keyboard, written by `kdos-settingsd` for `locale1` (System Settings' Region
and Keyboard pages, `kdos locale`, `kdos keymap`) and by the installer.

| File | Holds | Read by |
|---|---|---|
| `/etc/locale.conf` | `NAME=value` lines with no quotes; ships `LANG=C.UTF-8` | `kdos-dm` and SDDM, for the login screen and every session |
| `/etc/keymap` | The console keymap name; not shipped | `kdos-getty`, on every console it starts |
| `/etc/kdos/xkb.conf` | `XKBMODEL`, `XKBLAYOUT`, `XKBVARIANT` and `XKBOPTIONS`, the graphical layout; ships with all four empty | `kdos-settingsd`, which answers `locale1`'s X11 properties from it; the login screen's KWin reads those. An empty layout follows `/etc/keymap` |

A person's own keyboard layout in Plasma is in `~/.config/kxkbrc`, and outranks these inside their
session.

### `/etc/hostname`, `/etc/machine-info` and the time zone

Written by `kdos-settingsd` for `hostname1` and `timedate1` (System Settings' About and Date & Time
pages, `kdos hostname`, `kdos timezone`) and by the installer. `/etc/hostname` is the static host
name, and the `127.0.1.1` line of `/etc/hosts` names it too, so the machine's own name resolves
without a network. `/etc/machine-info` holds the pretty name, icon and chassis. The time zone is
`/etc/localtime`, with `/etc/profile.d/20-timezone.sh` exporting `TZ=:/etc/localtime`.

### `/etc/modprobe.d/kdos-regdom.conf`

The Wi-Fi regulatory country, as `options cfg80211 ieee80211_regdom=<CC>`. Written from the time
zone's row in `zone.tab` whenever the time zone is set, by `kdos-settingsd` and by the installer.
For a zone with no country, `kdos-settingsd` removes the file; the installer writes nothing and
removes nothing, so a file already on the installed system stays. Applies when `cfg80211` loads,
which is the next boot; `iw reg set <CC>` changes the running radio.

### `/etc/kdos/autologin-xdg/`

Settings that apply to an automatic login and to nothing else. SDDM's `sddm-autologin` PAM stack
runs `pam_env` with `/etc/security/sddm-autologin.conf`, which puts this directory first on
`XDG_CONFIG_DIRS`; a password login never reads it. The image ships no such directory. The
installer creates it on a single-account machine with an encrypted root that logs in by itself,
holding a `kscreenlockerrc` of `[Daemon] LockOnStart=true`: the session comes up behind the lock
screen, which is that machine's login. Answering "no password after the disk passphrase" leaves the
directory out. Because the setting belongs to the automatic login rather than to the account,
turning autologin off in System Settings later does not leave every password login locked as well.

### `/etc/kpkg.conf`

Where the package manager `kpkg` keeps its trees. The file is written as shell assignments,
`NAME="${NAME:-default}"`. `kpkg` parses it itself, reading `NAME=value` with no space before the
`=` (a line spelt `NAME = value` is ignored) and unwrapping the `${...:-...}`. It is also sourced by
bash at the start of every port's build, so a shell expression in it runs there. An exported
environment variable of the same name always wins over the file, which is what the build phases
from `40_lang` to `60_kernel` rely on: each one's `phase.env` exports a `PORT_REPO` that names
`/ports/core` and then the checkout's own `src/<area>` directories as the build chroot sees them:
`/kdos/src/system` and `/kdos/src/art` in every one of them, and `/kdos/src/desktop` and
`/kdos/src/daemons` as well in `50_desktop`. `KPKG_CONF` names a different file. The variables
that change what `kpkg` does rather than where its trees are (`KPKG_ROOT`,
`KPKG_REQUIRE_SIG`, `KPKG_STRICT_RECIPE`, `KPKG_KEEP_CACHE`, `KPKG_SKIP_INDEX`, the package
store's `KPKG_STORE`, `KPKG_STORE_CHECK`, `KPKG_STORE_SALT` and `KPKG_STORE_BASE`, and the rest)
are environment only and are listed in [Packaging](../03-architecture/packaging.md#kpkg). The build
sets the store's from `make build KDOS_PKG_STORE=0|1|check` and `KDOS_PKG_STORE_MAX`; see
[Build](filesystem-and-ipc.md#build).

| Key | Default | Means |
|---|---|---|
| `PORT_REPO` | `/ports/core /ports/src/system /ports/src/art /ports/src/desktop /ports/src/daemons` | The port repositories — by default the ports tree, then the four areas of KDOS's own programs — separated by spaces and searched in order, at most eight; more warn and are ignored. A port is found as `<repo>/<name>/` or one shelf down as `<repo>/<shelf>/<name>/`, and a name at two paths in one repository is an error. See [Writing ports](../05-developer/writing-ports.md#shelves-and-how-a-port-is-found) |
| `SOURCE_DIR` | `/var/cache/kpkg/sources` | Downloaded source archives |
| `PACKAGE_DIR` | `/var/cache/kpkg/packages` | Built packages |
| `WORK_DIR` | `/var/cache/kpkg/work` | Where a port is unpacked and built |
| `PKGDB_DIR` | `/var/lib/kpkg/db` | The installed-package database |

See [Packaging](../03-architecture/packaging.md).

### Other machine files

Files the image ships under `/etc` that an administrator may need to find. Each is in the format
of the program that reads it.

| Path | Read by | Does |
|---|---|---|
| `/etc/ld-musl-x86_64.path` | musl's dynamic loader, at every program start | The shared-library search path, one directory per line: `/usr/lib`, then `/usr/lib64`. It replaces musl's built-in default, so `/usr/local/lib` is not searched. There is no cache to rebuild, and `ldconfig` (from the `musl` port) exits 0 without doing anything: a directory added here applies to the next program started |
| `/etc/sysctl.conf` | `/etc/init.d/10_sysctl.sh`, at boot | Unprivileged ICMP sockets for every group (`ping` needs no setuid bit), the `protected_*` link, FIFO and regular-file protections, fatal-signal logging and per-task delay accounting. On the network side: ICMP redirects neither accepted (IPv4 and IPv6) nor sent, and the loose reverse-path filter (`rp_filter = 2`). Kernel addresses in `/proc` are hidden from accounts other than root (`kernel.kptr_restrict = 1`). A file in `/etc/sysctl.d` is applied after it |
| `/etc/hosts` | The resolver | `localhost` for both address families, and `127.0.1.1 kdos`, which `kdos-settingsd` and the installer rewrite with the host name, so the machine's own name resolves without a network |
| `/etc/nsswitch.conf` | glibc programs | musl ignores this file. It is there for glibc tools inside boxes and anything else that consults it, and names `files`, then `dns` for hosts |
| `/etc/shells` | `chsh`, and programs that check a login shell | `sh`, `bash` and `zsh`, each under both `/bin` and `/usr/bin` |
| `/etc/issue` | `agetty`, before each console login | The pre-login banner: the system name, kernel release, architecture, host name and terminal line |
| `/etc/vtrgb` | `kdos-getty`, on every console it starts | The sixteen console colours, in `setvtrgb`'s format. `kdos theme` rewrites it in the new accent, through `kdos-powerd` and `kdos-bootctl theme`, and each console takes it at its next login prompt |
| `/etc/modules-load.d/*.conf` | `/etc/init.d/02_modules.sh`, at boot | Modules loaded unconditionally: `tun` and `overlay` for containers, the virtual-machine display drivers, the `nct6775`, `it87` and `drivetemp` sensor drivers, and `i2c-dev` |
| `/etc/modprobe.d/kdos-sdr.conf` | `modprobe` | Blacklists `dvb_usb_rtl28xxu`, so an RTL-SDR stick is free for the SDR tools rather than claimed as a TV tuner |
| `/etc/alsa/conf.d/99-kdos-pipewire.conf` | Every ALSA program | Defines two devices, `kdos_pipewire` (through PipeWire) and `kdos_card` (the sound card directly, with no sound server), and makes ALSA's default whichever the environment variable `KDOS_ALSA_DEFAULT` names, `kdos_pipewire` when it is unset: `KDOS_ALSA_DEFAULT=kdos_card aplay x.wav` plays straight to the card |
| `/etc/default/pcscd` | `/etc/init.d/47_pcscd.sh`, at each start | pcsc-lite's own keys, every one commented out: `PCSCD_ARGS` (extra `pcscd` arguments), `PCSCLITE_FILTER_IGNORE_READER_NAMES` (readers to hide, by a word of their name, `:` between words), `PCSCLITE_FILTER_EXTEND_READER_NAMES` (text added to every reader's name) and `PCSCLITE_HP_DROPDIR` (the USB driver directory). One `KEY=value` per line, parsed and not sourced: the quotes are removed, `$HOSTNAME` is the one expansion, and any other key is reported and skipped. `sudo service restart pcscd` applies an edit |
| `/etc/default/useradd` | `useradd`, at each new account | `GROUPS=audio`: every ordinary account joins `audio`, whose members get PipeWire's real-time limits, whether System Settings, AccountsService, Plasma Setup or a hand-run `useradd` makes it; `-G` replaces this list rather than adding to it, and `useradd -r` ignores it. `CREATE_MAIL_SPOOL=no`: no mailbox for a new account, as `/etc/login.defs` names no `MAIL_DIR` and the image has no mail spool. Every key it does not set keeps `useradd`'s compiled default, which `useradd -D` prints beside it; `useradd -D` with a flag (`-s` for the shell, `-b` for the home base) writes every key into the file |
| `/etc/pipewire/pipewire.conf.d/99-kdos-vm.conf` | PipeWire | In a virtual machine only, raises the audio quantum (the block of frames PipeWire processes in one cycle) and its floor to 4096, and its ceiling to 8192 |
| `/etc/security/limits.d/30-kdos.conf` | `pam_limits`, at every login | Real-time priority 1 for every account, so KWin may run the threads that read input and commit frames at the lowest real-time priority; a file capability on `kwin_wayland` does not survive kpkg's archive. PipeWire's `25-pw-rlimits.conf` gives the `audio` group priority 95 for the sound graph, and a group entry outranks this one |
| `/etc/samba/smb.conf` | Samba, for Dolphin's *Share* tab | A standalone server with user shares under `/var/lib/samba/usershares`, at most 100, guests allowed on a share that asks for them, and no printers. `/etc/init.d/83_samba.sh` starts `smbd` at boot only when a usershare exists or this file defines a share below `[global]`; the first folder shared from Dolphin starts it at once, through `kded-kdos` and `kdos-powerd`. Other machines reach a share only once the firewall's `smb` service is opened |
| `/etc/polkit-1/rules.d/50-kdos.rules` | `polkitd` | Grants members of `wheel` at the machine the named polkit actions the desktop calls, without a prompt; see the [security model](../03-architecture/security-model.md) |
| `/etc/udev/rules.d/70-kdos-*.rules` | `udevd` | Device access by group: serial, SDR, logic-analyser, debug-probe, instrument, scanner, camera, security-key and GPIO devices to `dialout`, gamepads to `input`, I²C buses and backlight brightness to `video`, and Brother P-touch label printers to `lpadmin` |

---

## Shipped configuration for third-party software

`/etc/skel` also carries configuration for third-party programs, so a new account gets a working,
consistently coloured setup rather than each program's own defaults. These files are yours to edit,
except the ones marked *Generated*, which `kdos theme` rewrites.

| Path | For | Notes |
|---|---|---|
| `~/.config/btop/btop.conf` | The system monitor `btop` | |
| `~/.config/btop/themes/kdos.theme` | btop's colours | Generated |
| `~/.config/kdos/ls-colors` | `LS_COLORS`, loaded by `~/.bashrc` from `${XDG_CONFIG_HOME:-~/.config}/kdos/ls-colors`, where `kdos theme` writes it | Generated |
| `~/.config/kdos/fzf-colors` | fzf's `--color` flags, sourced by `/etc/profile.d/30-kdos-colors.sh` | Generated. See the note on `FZF_DEFAULT_OPTS` under [Shell environment](#shell-environment) |
| `~/.config/bat/config` | The pager `bat` | One line: `--theme="kdos"` |
| `~/.config/bat/themes/kdos.tmTheme` | bat's theme, selected by file name | Generated |
| `~/.config/micro/settings.json` | The editor `micro` | Selects the generated colour scheme |
| `~/.config/micro/colorschemes/kdos.micro` | micro's colour scheme | Generated |
| `~/.config/helix/themes/kdos.toml` | helix's theme | Generated. Read by the host's helix and by a helix installed in a box, which shares this `$HOME`. Nothing selects it: write `theme = "kdos"` in helix's `config.toml` yourself |
| `~/.config/nvim/init.vim` | The editor `neovim` | Turns on `termguicolors` and selects the generated colour scheme |
| `~/.config/nvim/colors/kdos.vim` | neovim's colour scheme | Generated |
| `~/.config/git/config` | git | Sets `delta` as the pager and includes the generated colours |
| `~/.config/git/kdos-delta` | delta's colours | Generated |
| `~/.config/newsboat/config` | The feed reader `newsboat` | Includes the generated colours |
| `~/.config/newsboat/kdos-colors` | newsboat's colours, as 256-colour indices | Generated |
| `~/.config/aerc/stylesets/kdos` | aerc's style set | Generated |
| `~/.config/tmux/tmux.conf` | The terminal multiplexer `tmux` | |
| `~/.config/tmux/themes/kdos.conf` | tmux's colours | Generated |
| `~/.config/starship.toml` | The shell prompt `starship` | Only the palette block between its markers is generated; the rest is yours |
| `~/.config/yazi/theme.toml` | The file manager `yazi` | Generated; see [below](#configyazithemetoml) |
| `~/.config/fastfetch/config.jsonc` | The system-information tool `fastfetch` | The login banner runs it with its own logo turned off |
| `~/.config/lf/lfrc`, `~/.config/lf/preview` | The terminal file manager `lf` | |
| `~/.config/GIMP/3.0/gimprc` | GIMP | Selects the system theme; without it GIMP keeps its own |
| `~/.config/fcitx5/profile` | The input method `fcitx5` | One group, `Default`, with the four input methods the image carries: `keyboard-us`, `pinyin`, `anthy` and `hangul` |
| `~/.config/mimeapps.list` | Your own choices of which application opens which file type | Ships with an empty `[Default Applications]` section. This file outranks the system tables, and the *Open With* dialog's *Remember* writes to it |
| `~/.config/tealdeer/config.toml` | The `tldr` client `tealdeer` | Installed into `/etc/skel` by the `tealdeer` package. `cache_dir` is `/usr/share/tldr`, where the pages ship, so nothing is downloaded. `tldr --update` needs a `cache_dir` you can write |
| `~/.bashrc`, `~/.bash_profile` | bash | `.bashrc` reads `/etc/bash.bashrc` — through `/run/host` inside a box, since `$HOME` is shared with every box — and `kdos/ls-colors` under `$XDG_CONFIG_HOME` (`~/.config` when unset), the file `kdos theme` writes. `.bash_profile` reads `.bashrc` |
| `~/.hushlogin` | `login` | Empty. Its presence stops `login` printing `/etc/motd` and the last-login line, so the banner `/etc/bash.bashrc` draws is the only one. Deleting it makes each console login print both before that banner |

The standard user directories (Documents, Downloads and the rest) are made at the first login by
`xdg-user-dirs`, which writes `~/.config/user-dirs.dirs`.

### Default handlers

Which application opens a file type is decided the same way by Dolphin, every KDE program and
`kdos-appbox open` (which is also `xdg-open`), in freedesktop's order. Inside the session and out,
`XDG_CURRENT_DESKTOP` is `KDE`, so the desktop-named lists are `kde-mimeapps.list`:

| Path | Scope |
|---|---|
| `~/.config/kde-mimeapps.list`, then `~/.config/mimeapps.list` | This account |
| `/etc/xdg/kde-mimeapps.list`, then `/etc/xdg/mimeapps.list` | The administrator's, system-wide |
| `/usr/share/kdos/xdg/kde-mimeapps.list` | KDOS's defaults, one line per file type |

In each file the `[Default Applications]` section is read first; the account files'
`[Added Associations]` are read after the account defaults and before the system tables. The
generated `mimeinfo.cache` files beside the installed launchers are consulted last. An entry counts
only while its application is installed, so a list naming a removed application falls through to
the next one. `testing/mimecheck.py` fails the build of the image when the shipped list names an
application the image does not install. Which application each shipped line names is in
[Applications](../02-user-guide/applications.md).

### Midnight Commander

| Path | Holds |
|---|---|
| `~/.config/mc/ini` | How `mc` behaves: the KDOS skin, `F3` views internally, `F4` edits with `$EDITOR`, no exit confirmation, and the panel's directory in the window title |
| `~/.config/mc/mc.ext.ini` | What `Enter` does on a file |
| `~/.config/mc/menu` | `mc`'s `F2` user menu |

In `ini` the section is part of the key. `mc` reads its behaviour flags from `[Midnight-Commander]`
and its screen layout from `[Layout]`, and a key under the wrong header is silently ignored.
`kdos theme` merges `skin` into `ini` rather than replacing the file, and writes the skin itself to
`~/.local/share/mc/skins/kdos.ini` (`mc` looks for skins only under `<data>/mc/skins`,
`/etc/mc/skins` and `/usr/share/mc/skins`).

`mc.ext.ini` replaces the system file entirely, because `mc` does not merge the two. It carries only
the archive rows whose helper is on the image; every other file falls through to `kdos-appbox open`.
The `F2` menu has nine entries — open with the desktop's handler, show in Dolphin, edit, find in
this folder, terminal here, add to Places, share, move to trash, and git status — each naming a
program on the image.

### Shell environment

A login shell reads these files, and so does the session: `kdos-session-wrapper` starts Plasma from
a login shell, so every desktop program inherits them. `/etc/profile` runs first, and it runs every
`*.sh` file in `/etc/profile.d`.

| Path | Sets |
|---|---|
| `/etc/profile` | `PATH=/usr/local/bin:/usr/bin:/usr/sbin:/bin:/sbin`, replacing any inherited value; `PKG_CONFIG_PATH` to the `pkgconfig` directories under `/usr` and `/usr/local`, followed by any value already set; `LANG=C.UTF-8`; `TERM` only when it is empty or `dumb` (`linux` on a `/dev/ttyN` console, `vt100` elsewhere); `LINES` and `COLUMNS` when unset, to 24 and 80; and `USER`, `LOGNAME` and `HOME` from the account when the login did not set them. In bash it then reads `/etc/bash.bashrc` (below), *before* the `profile.d` files. In any other shell on a terminal it clears the screen and prints `/etc/motd` after them |
| `/etc/profile.d/00locale.sh` | `MUSL_LOCPATH` to `/usr/share/i18n/locales/musl`, where musl finds translated messages for a non-English `LANG`. From `musl-locales` |
| `/etc/profile.d/10-wayland.sh` | The session basics: `HOME` from `/etc/passwd` when it is empty; `XDG_RUNTIME_DIR` to `/run/user/<uid>` when unset and that directory exists; `XDG_CURRENT_DESKTOP=KDE` and `XDG_MENU_PREFIX=plasma-` unless already set, so a console or SSH shell opens files in the applications the desktop does; `DBUS_SESSION_BUS_ADDRESS` to `$XDG_RUNTIME_DIR/bus` when that bus exists; the `XDG_*_HOME` and `XDG_DATA_DIRS` defaults; `/usr/share/kdos/xdg` appended to `XDG_CONFIG_DIRS`; `~/.local/bin` and `/usr/games` on `PATH`; the toolkit back ends (`QT_QPA_PLATFORM=wayland;xcb`, `MOZ_ENABLE_WAYLAND=1`, `SDL_VIDEODRIVER=wayland`, `CLUTTER_BACKEND=wayland`, and no `GDK_BACKEND`, so GDK tries Wayland first and an application that asks for X11 gets Xwayland); `GTK_USE_PORTAL=1`, so GTK applications use the KDE file dialog and print portal; and `QT_QPA_PLATFORMTHEME=kde`, so Qt 6 applications read `kdeglobals` and Qt 5 applications read the qt5ct files `kdos theme` writes |
| `/etc/profile.d/20-lesspipe.sh` | `LESSOPEN` to `lesspipe.sh`, and `LESS=-R` |
| `/etc/profile.d/20-timezone.sh` | `TZ=:/etc/localtime`. Written by `kdos-settingsd` and by the installer; not shipped with the image |
| `/etc/profile.d/30-kdos-colors.sh` | Sources the generated `~/.config/kdos/fzf-colors` and appends its colours to `FZF_DEFAULT_OPTS` |
| `/etc/profile.d/30-open.sh` | `BROWSER` to `xdg-open`, which on this image is `kdos-appbox open`, so the variable and the handler tables above agree |
| `/etc/profile.d/40-plocate.sh` | `LOCATE_PATH` to this account's own file index |
| `/etc/profile.d/50-ssh-agent.sh` | `SSH_AUTH_SOCK` to `$XDG_RUNTIME_DIR/ssh-agent.socket`, starting `ssh-agent` there when nothing answers. One agent per account, shared by every login and everything the desktop starts, so `ssh-add` asks for a passphrase once. From `openssh` |
| `/etc/profile.d/50-sfeed.sh` | `SFEED_YANKER` to `wl-copy -n`, so `sfeed_curses`'s yank reaches the clipboard. From `sfeed` |
| `/etc/profile.d/50-opencl.sh` | `RUSTICL_ENABLE` to `iris,radeonsi`, the Intel and AMD drivers of Mesa's OpenCL implementation. Without it Rusticl offers no device and every OpenCL program finds none. `llvmpipe` is left out, so a machine with no supported GPU has no OpenCL device |
| `/etc/profile.d/bash_completion.sh` | No variables: in an interactive bash where bash-completion is not yet loaded, loads `~/.config/bash_completion` and then bash-completion. In a login bash `/etc/bash.bashrc` has already loaded it, so this file does nothing there. From `bash-completion` |
| `/etc/profile.d/gawk.sh` | No variables: the `gawkpath_*` and `gawklibpath_*` functions, which edit `AWKPATH` and `AWKLIBPATH`. From `gawk` |
| `/etc/profile.d/podman-docker.sh` | `DOCKER_HOST` to the rootless Podman API socket, `$XDG_RUNTIME_DIR/podman/podman.sock` (root's is `/run/podman/podman.sock`), so a Docker API client such as `lazydocker` finds `podman system service` while it runs — see [Containers](#containers) for what starts it. `/usr/bin/docker` is Podman's `docker` shim |
| `/etc/profile.d/vte.sh` | No variables: in a VTE-based terminal (GNOME Terminal and the like, which set `VTE_VERSION`), a prompt hook that reports the current directory, so a new tab opens where you are. Nothing in Konsole. From `vte3`, which also installs `vte.csh` for csh, a shell this image does not ship |
| `/etc/profile.d/Z97-byobu.sh` | Starts `byobu` at login, but only when asked: `LC_BYOBU=1`, an `LC_TERMTYPE` naming byobu, or the file `/etc/byobu/autolaunch`. Otherwise nothing. From `byobu` |

Some of these set their values unconditionally: `/etc/profile` (`PATH` and `LANG`), `00locale.sh`,
`10-wayland.sh` (the toolkit back ends and the platform theme) and `40-plocate.sh`
(`LOCATE_PATH`). `30-kdos-colors.sh` appends to whatever `FZF_DEFAULT_OPTS` holds. None of the
others overwrites a value you already exported. The
`less` filter decides what a file is with `file -L -s -b --mime` and nothing else, which is why the
image ships `file` with a full magic database. Inside Plasma, `kdos-session-wrapper` then sets
`XDG_CURRENT_DESKTOP=KDE` and the session's `XDG_CONFIG_DIRS`, and removes `TERM`, `LINES`,
`COLUMNS`, `QT_IM_MODULE` and `GTK_IM_MODULE`; a variable you want in every Plasma program goes in a
script in `~/.config/plasma-workspace/env/`, which Plasma runs after it.

What depends on the terminal is set in every interactive bash instead, in `/etc/bash.bashrc`, which
guards itself so it runs once per shell. The history settings, the shell options, the aliases that
need no other program, the `nano` fallback for `EDITOR` and `GPG_TTY` are set in every interactive
shell; every other entry first checks that its program is installed:

| Variable or hook | Is |
|---|---|
| History | `HISTSIZE=50000`, `HISTFILESIZE=100000`, `HISTCONTROL=ignoreboth:erasedups`, timestamps (`HISTTIMEFORMAT='%F %T  '`), and `history -a` after every command, so concurrent shells do not lose each other's history. `ls`, `ll`, `la`, `cd`, `pwd`, `exit`, `clear` and `history` typed alone are not recorded (`HISTIGNORE`) |
| Shell options | `histappend`, `checkwinsize` and `cmdhist`; `autocd` (a directory name typed alone changes to it), `cdspell` and `dirspell` (small spelling mistakes in a directory name are corrected) |
| Aliases | Colour for `grep` and `diff`; `-i` on `cp`, `mv` and `rm`; `-p` on `mkdir`; `-h` on `df`, `du` and `free`; `path` prints `PATH` one directory per line; `ports` counts the installed packages; `..`, `...` and `....` go up one, two and three directories; `kfetch`, `kstatic` and `khelp` for `kdos-fetch-app`, `kdos-fetch-static` and `kdos help`. When their programs are installed: `ls`, `ll`, `la`, `lt` and `l` through `eza` (plain `ls` with colour otherwise); `cat` through `bat`; `top` through `btop`; `lg` for lazygit; `dud`, `dff` and `psx` for `dust`, `duf` and `procs` |
| Prompt | `starship` when installed, else a plain prompt that names the box inside one. Every prompt emits the OSC 133 marks a terminal uses to find where each command's output starts and ends |
| `EDITOR`, `VISUAL` | `nvim` when neovim is installed, replacing any exported value (with `vi` and `vim` aliased to it). Otherwise an `EDITOR` you exported is kept, else `nano`. `VISUAL` always equals `EDITOR` |
| `MANPAGER` | Set only when `bat` is installed: `/usr/libexec/bat/man-pager`, from `bat`: it strips the page's overstrike and hands it to `bat -l man`. It is a script rather than a pipeline because `mandoc`'s `man` splits the variable on spaces and runs it without a shell |
| `BAT_THEME` | `ansi` |
| `FZF_DEFAULT_OPTS`, `FZF_DEFAULT_COMMAND`, fzf's keys | fzf's layout and a fixed phosphor-green colour set, and, when `fd` is installed, `fd --type f --hidden --follow --exclude .git` as the file source. fzf's own key bindings (`Ctrl+T`, `Alt+C`) and completion are loaded too. The login shell itself runs this file before the `profile.d` files, so there `30-kdos-colors.sh` appends the accent colours afterwards and they win; every shell started after it, such as each terminal window, runs this file again and the assignment replaces them |
| `GPG_TTY` | This shell's terminal, where `pinentry` asks for a passphrase when the session has no display to draw its dialog on |
| `lfcd` | A function from `lf`'s `lfcd.sh`: runs `lf` and leaves the shell in the directory `lf` quit in |
| `j` | `zoxide`'s jump command |
| `atuin init bash` | `Ctrl+R` searches atuin's history database, and every command is recorded there. The Up arrow stays readline's. atuin brings its own `bash-preexec`, which removes `ignorespace` from `HISTCONTROL`, so a command typed after a space is recorded too |
| Completion | bash-completion's programmable completion, outside POSIX mode |
| Banner | In the first shell of a login (`SHLVL` 1), on a terminal and outside a box: `kdos-banner`, or `fastfetch` where that is missing, or `/etc/motd` where both are |

System-wide files that packages install for the tools above:

| Path | From | Does |
|---|---|---|
| `/etc/gitconfig` | `git-lfs` | Registers the `lfs` filter, so cloning an LFS repository checks out content rather than pointer files, with no `git lfs install` |
| `/etc/nanorc` | `nano` | Includes every syntax definition under `/usr/share/nano` |
| `/usr/lib/NetworkManager/conf.d/10-kdos-dns.conf` | `networkmanager` | `dns=dnsmasq`: a local caching `dnsmasq` on `127.0.0.1`, with split DNS for VPNs. A file of the same name in `/etc/NetworkManager/conf.d` overrides it |
| `/etc/resolvconf.conf` | `openresolv` | `resolvconf`'s own settings. It is the one writer of `/etc/resolv.conf`, for NetworkManager, `dhcpcd` and `wg-quick` alike |
| `/etc/resolv.conf` | musl's resolver, every name lookup | Before a network is up it names only `nameserver 127.0.0.1`, where nothing answers until NetworkManager starts its `dnsmasq`, so an early lookup fails at once and reaches no outside resolver. `resolvconf` replaces it with the servers of the link that comes up. Its first line, `# Generated by resolvconf`, is openresolv's signature: `resolvconf` refuses to replace a file that does not start with it, and every `resolvconf -a` then fails |
| `/etc/sudoers.d/00-sudo` | `sudo` | Mode 0440. Members of `wheel` may run anything. `secure_path` is `/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin`, which is what lets `sudo kdos` find `kdos`. `EDITOR`, `VISUAL` and `SUDO_EDITOR` survive `env_reset`, so `sudoedit` and `visudo` open your editor; with none of the three set they open `nvim`, or `nano` where neovim is not installed |
| `/etc/zsh/zprofile`, `/etc/zsh/zshrc` | `zsh` | A login zsh reads `/etc/profile`; every interactive one runs `atuin init zsh` |

### Containers

`/etc/containers` belongs to one package, `containers-common`, which `podman`, `buildah` and
`skopeo` all depend on, because all three read all of it.

| Path | Sets |
|---|---|
| `/etc/containers/containers.conf` | `cgroupfs` as the cgroup manager (there is no systemd to delegate one); the file event logger; `crun` as the runtime; `netavark` with the `nftables` firewall driver; `pasta` for a rootless container's network |
| `/etc/containers/storage.conf` | The overlay driver, mounted through `fuse-overlayfs`, and no store path: root's store is the engine's default, `/var/lib/containers/storage`, and an ordinary account's is its own under `~/.local/share/containers/storage`. A `graphroot` or `runroot` here is read by rootless podman too, which cannot write root's store, so every rootless container would fail with `mkdir /var/lib/containers/storage: permission denied` |
| `/etc/containers/registries.conf` | `docker.io` as the one registry a short image name is looked up in |
| `/etc/containers/policy.json` | Upstream's default: any image is accepted and no signature is checked. Without this file every pull refuses to start |

A pod's infra container and `podman run --init` use `catatonit`, a static init at
`/usr/lib/podman/catatonit`. A box profile's `init = yes` is not that: it asks distrobox to boot
the image's own init system.

**The Podman API socket starts on demand.** Both *Containers* menu entries — `podman-tui` and
`lazydocker` — run through `kdos-podman-api`. It starts `podman system service` when nothing answers
on `$XDG_RUNTIME_DIR/podman/podman.sock` (`/run/podman/podman.sock` for root), sets `DOCKER_HOST` to
that socket, and gives `podman-tui` a default connection named `kdos-local` through
`CONTAINERS_CONF_OVERRIDE`. A default connection you set with `podman system connection default`
takes precedence, and a `CONTAINERS_CONF_OVERRIDE` you exported yourself is left alone. The service
exits after five minutes with no client.

Typed at a prompt, `lazydocker` reaches the socket only while a service is running, through the
`DOCKER_HOST` that `podman-docker.sh` exports; `kdos-podman-api lazydocker` starts one first. A bare
`podman-tui` has no connection at all, running service or not, unless it runs through
`kdos-podman-api` or you add one with `podman system connection add`.

### Mail, calendar and contacts

| Path | For | Notes |
|---|---|---|
| `~/.mbsyncrc` | Fetching mail with `mbsync` | Mode 600, because it holds a password. Blank lines separate sections, so a commented-out block must keep its blank lines or a `Channel` ends up inside the `Store` above it. Ships with no accounts |
| `~/.msmtprc` | Sending mail with `msmtp` | Mode 600: msmtp refuses a file with a `password` line that others can read (`passwordeval` avoids the question). `/usr/sbin/sendmail` and `/usr/bin/sendmail` are links to msmtp. Ships with no accounts |
| `~/.config/notmuch/default/config` | The mail index `notmuch` | `mail_root` is relative and expands against `$HOME`, so one shipped file suits every account. It names `~/Mail`, as do the other two files |
| `~/.config/notmuch/default/hooks/` | What `notmuch new` does around the scan | `pre-new` fetches with `mbsync -a`, `post-new` tags. A failing `pre-new` stops `notmuch new`, so a failed fetch is not reported as an empty inbox |
| `~/.config/khal/config` | The calendar `khal` | Active, not commented out: khal with no `[calendars]` section refuses to start, so it points at an empty store. It uses `type = discover` over a glob, because `type = calendar` *creates* the path it is given. The `[locale]` dates are ISO, which scripts that parse khal's output depend on |
| `~/.config/vdirsyncer/config` | Syncing a calendar or address book | Active but with no pair configured: `vdirsyncer sync` then exits 0 silently and creates nothing. `[general]` and `status_path` are the minimum that parses. `conflict_resolution` has no safe default, and the commented example says so |
| `~/.config/khard/khard.conf` | The address book `khard` | Active for the same reason: khard with no entry exits 3. `khard list` on an empty book prints `Found no contacts` and exits 1, which means "nothing matched", not a fault |
| `~/.config/aerc/aerc.conf` | The mail client `aerc`: what it shows and how it renders a message part | Only the keys that differ from aerc's built-in defaults, plus `[filters]`, which has no defaults behind it and so replaces the whole set |
| `~/.config/aerc/accounts.conf` | Your mail accounts | Not shipped. aerc refuses to start with one that group or others can read, and every shipped file is mode 644; with no file, aerc's setup wizard writes it at mode 600 |
| `~/.config/aerc/binds.conf` | aerc's keys | Not shipped. aerc's built-in bindings are empty and this file is the only source, so a partial file would unbind every key it did not name |

Every `[filters]` row except `colorize` goes through `kdos-part`, one script installed in aerc's
filter directory. Its error output becomes the message body, so a row naming a program that is not
installed shows the shell's `command not found` where the message should be. A type with no
`[filters]` row gets aerc's *No filter configured* card.

### `~/.config/yazi/theme.toml`

Generated by `kdos theme`; edits are overwritten. It is partial on purpose: yazi layers it *over*
its own `theme-dark.toml` preset key by key, so only what the palette decides is written, and the
preset's icons, separators and file-type rules stay. `[flavor]` is not written, because it is the
one part the preset splits between dark and light mode.

### Speech-to-text models

Dolphin's *Transcribe* action and `kdos speech transcribe` hear with a
[whisper.cpp](https://github.com/ggml-org/whisper.cpp) model. The image carries `base.en` in
`/usr/share/whisper.cpp/models/`, from the `whisper-model-base-en` port; with no model anywhere,
Dolphin offers no *Transcribe* entry. There is no configuration key; three locations are searched
in order and the first hit wins.

| Order | Location |
|---|---|
| 1 | `$KDOS_WHISPER_MODEL` — one file, named exactly |
| 2 | `$XDG_DATA_HOME/whisper.cpp/models/`, by default `~/.local/share/whisper.cpp/models/` |
| 3 | `whisper.cpp/models/` under each system data directory, `/usr/share/whisper.cpp/models/` among them |

When `$KDOS_WHISPER_MODEL` is set it is the only candidate. No directory is searched after it, and a
file that is missing or fails the check below leaves transcription unavailable rather than quietly
using a different model.

In a directory the pattern is `ggml-*.bin`, and the first in sorted name order wins (not the newest,
so the choice is reproducible).

A file counts only if its first four bytes are `lmgg`, the model format's magic, so a half-finished
download reads as *no model* rather than as a crash.

The directory name is upstream's own, so a model fetched by upstream's
`models/download-ggml-model.sh` and one fetched by KDOS land in the same place.

**Getting a model.** `kdos speech list` prints the catalogue of twelve, grouped by size from about
32 MB to about 3.1 GB: `tiny.en-q5_1`, `tiny.en`, `tiny`, `base.en-q5_1`, `base.en`, `base`,
`small.en-q5_1`, `small.en`, `small`, `medium.en`, `large-v3-turbo` and `large-v3`. A name
containing `.en` is English-only; a `-q5_1` suffix marks a quantised, smaller copy of the model
named without it. `kdos speech get <name>` downloads one into `$XDG_DATA_HOME/whisper.cpp/models`
(no privilege needed); with no name it gets `base.en`. The name is looked up in a table, never
pasted into a URL; the download is checked against a SHA-256 built into the tool, and the file is
moved into place only after both that and the `lmgg` check pass, so an interrupted download never
leaves a file behind. Upstream signs nothing; the checksum only says the bytes are the ones this
tree was written against (see the [security model](../03-architecture/security-model.md)). See
[`kdos speech`](../04-programs/kdos-command.md#kdos-speech).

`whisper-stream`, which transcribes a live microphone, uses the same model. It runs in a terminal
and opens no window.

### `~/.baresip/config`

The SIP phone `baresip` writes `~/.baresip/config` on its first run, and only when the file does not
exist, so the defaults below are what a new account gets and an edited file is never overwritten.

Five module lines that upstream leaves commented are uncommented, because this image builds those
modules: `opus.so`, `avcodec.so`, `vp8.so`, `vp9.so` and `sdl.so`. (A line naming a module that is
not installed is a startup error, which is why upstream comments them all.)

The audio module is `pipewire.so`; `alsa.so` is written commented out. The build sets baresip's
default audio device to `pipewire,default`, so `audio_player`, `audio_source` and `audio_alert` all
name the PipeWire default and a call goes straight to PipeWire. `sndfile.so` (call recording),
`snapshot.so` and `ctrl_dbus.so` are built and left commented as upstream writes them; the `aac.so`
codec is built and not mentioned in the generated file.

The display is on and the camera is off: which screen a picture goes to is a property of the build,
while sending your camera is a choice. Uncommenting `v4l2.so` turns the camera on; the generated
`video_source` line already names `v4l2,/dev/video0`. `avformat.so` is the other video source, for a
stream or file named in `video_source`. `x11.so` is not built. `fakevideo.so` (a null sink) and
`vidbridge.so` (a loopback) stay commented.

## Generated files you should not edit

`kdos theme` rewrites all of these whenever the accent or style changes, and seeds them into
`/etc/skel` when the image is built. Your edits are lost at the next change.

| Path | Read by |
|---|---|
| `~/.config/btop/themes/kdos.theme` | `btop` |
| `~/.config/tmux/themes/kdos.conf` | `tmux` |
| `~/.config/kdos/ls-colors`, `~/.config/kdos/fzf-colors` | `ls` and fzf |
| `~/.config/bat/themes/kdos.tmTheme`, `~/.config/micro/colorschemes/kdos.micro`, `~/.config/nvim/colors/kdos.vim`, `~/.config/helix/themes/kdos.toml` | The pager and editors |
| `~/.config/git/kdos-delta`, `~/.config/newsboat/kdos-colors`, `~/.config/aerc/stylesets/kdos`, `~/.config/yazi/theme.toml` | delta, newsboat, aerc and yazi |
| `~/.local/share/mc/skins/kdos.ini`, and the `skin` key in `~/.config/mc/ini` | Midnight Commander |
| The palette block in `~/.config/starship.toml` | The shell prompt, between its markers. The rest of the file is yours |
| `~/.config/qt5ct/qt5ct.conf` | Qt 5 programs. Merged: the theme owns `color_scheme_path` and `icon_theme` under `[Appearance]` while they still name a KDOS file, and your other settings in it survive |

Plasma's own half of an accent is not generated here. Every colour scheme, Konsole profile, qt5ct
palette, wallpaper and global theme was generated when the image was built, by `kdos-theme` and
`kdos-plasma-look`, and `kdos theme` chooses among them: it applies the global theme, the colour
scheme, the wallpaper and the Konsole profile through Plasma's own settings, and moves each only
while it still names a KDOS file, so a wallpaper or profile you chose is left as it is.

`kdos theme --audit` reports any of the generated files that differ from what this machine's
palette produces. See [`kdos theme`](../04-programs/kdos-command.md#kdos-theme) and
[Theming](../02-user-guide/theming.md).

## See also

- [Administration](../02-user-guide/administration.md) — using these files in the jobs they belong to
- [Theming](../02-user-guide/theming.md) — the accents, Phosphor mode and what an accent switch changes
- [KWin](../04-programs/kwin.md) — the KWin defaults in context
- [kdos-appbox](../04-programs/kdos-appbox.md) — box profiles in full
- [The `kdos` command](../04-programs/kdos-command.md) — `kdos theme`, `kdos phosphor`,
  `kdos toggle`, `kdos update`, `kdos sandbox`
- [Packaging](../03-architecture/packaging.md) — `kpkg`, the binhost and the ports tree behind
  `kpkg.conf` and `update.conf`
- [The ports catalogue](ports-catalogue.md) — every port, several of which install the
  third-party files listed here
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — how the `fs/` overlay that carries
  the shipped defaults in this chapter reaches the image
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#configuration) — configuration through
  plain files, compared with declarative systems and settings databases
- [Filesystem and IPC](filesystem-and-ipc.md) — the paths, sockets and bus names these files name

<!-- book-nav -->
---

*Part VI — Reference, chapter 40.* Previous: [39. Command index](command-index.md) · [Contents](../README.md) · Next: [41. Filesystem and IPC](filesystem-and-ipc.md)
