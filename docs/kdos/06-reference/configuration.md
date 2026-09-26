# Configuration

This page lists every configuration file on a KDOS system that a person may edit, every key in each
one, its default, and when a change takes effect. It is for anyone who wants to change how the
desktop looks or behaves, and for whoever administers the machine.

There is no settings database and no abstraction layer. Each file below is read directly by the
program that acts on it, so the file *is* the setting. The Settings program (`kdos-settings`) writes
many of these same files; editing them by hand gives the same result.

**How to use this page.** Find the file in the contents below, or search for the key. Each section
says who reads the file, what every key does, and when an edit starts to count. Read
[Where configuration lives](#where-configuration-lives) and
[When a change takes effect](#when-a-change-takes-effect) first: every table afterwards uses their
terms. For the paths and sockets these files mention, see [Filesystem and IPC](filesystem-and-ipc.md).
Terms such as *surface*, *chrome*, *accent*, *box* and *pack* are defined in the
[Glossary](glossary.md).

## Where configuration lives

| Tier | Path | Belongs to |
|---|---|---|
| Machine | `/etc/kdos/`, `/etc/` | The administrator |
| Per user | `~/.config/kdos/`, `~/.config/kdos-comp/` | You |
| Per user, generated | `~/.themes/`, `~/.icons/`, `~/.config/gtk-*`, `~/.config/foot/themes/` and the others listed under [Generated files you should not edit](#generated-files-you-should-not-edit) | `kdos theme`. Edits are overwritten |
| State, not configuration | `~/.local/state/kdos/`, `~/.cache/kdos/`, `/var/lib/kdos/` | The programs that write them |

A new account is populated from `/etc/skel`. Editing a file there changes the defaults for accounts
created afterwards and leaves existing accounts alone.

Where a program honours `$XDG_CONFIG_HOME`, `~/.config` below means that directory.

## When a change takes effect

Every key table below uses one of these words.

| Word | Meaning |
|---|---|
| immediate | On the program's reload signal (`SIGHUP`). `kdos theme` and `kdos-settings` already send it; after a hand edit, send it yourself or wait for the next theme change |
| next start | The next time that program starts |
| next login | The value becomes part of a supervised program's command line, so it is read once per session |
| create time | When a box is created. A running box keeps what it was created with |
| boot | Read once during startup |

## File format

Most KDOS configuration files are `key = value`, one per line; the few that are not (`favorites`,
`slit.conf`, `displays.conf`, `timers.d`, and the files whose presence alone is the setting) show
their format in their own section. A line whose first non-blank
character is `#` is a comment. The files are *parsed, never sourced*: there is no shell, no command
substitution and no variable expansion, apart from the two path spellings noted under `comp.conf`.
A `#` after a value is part of the value, not a comment.

Where a key takes yes or no, `yes`, `true`, `on` and `1` mean yes, and `no`, `false`, `off` and `0`
mean no. `comp.conf` matches these in any case; the other files want them in lower case. In
`panel.conf`, `start_label` and `task_labels` take only `yes` or `on`, and `no` or `off`, and report
any other value.

`comp.conf`, `res.conf`, `term.conf`, box profiles and sandbox profiles report a key they do not
recognise, by name. `comp.conf` messages go to the compositor's log,
`$XDG_RUNTIME_DIR/kdos-comp.log`. The other programs print to standard error, which for a surface
the compositor started also lands in that log. `panel.conf` reports a widget or meter
name it does not recognise, and skips an unknown key without a message.

---

## The desktop

### `~/.config/kdos/comp.conf`

The compositor's KDOS settings: the phosphor effect, the idle timers, the lid, the wallpaper, and
the shape of the panel and other chrome the compositor starts. Keyboard bindings, mouse behaviour,
workspaces and window rules belong to [`rc.xml`](#configkdos-comprcxml); a line of that kind here
is reported by name and ignored.

The shipped file has every key commented out at its default, with an explanation beside each.

| Key | Default | Range | Applies | Means |
|---|---|---|---|---|
| `wallpaper` | `/usr/share/backgrounds/kdos/default-wallpaper.png` | path or `none` | immediate | The desktop background, scaled to cover the output and centred. See the note below about the retinted copy |
| `crt` | `55` | 0–100 | immediate | Strength of the phosphor effect, in per cent. `0` turns it off |
| `crt_scanlines` | `0` | 0–100 | immediate | Scanline depth. `60` is the strength the rest of the effect is tuned against |
| `crt_curve` | `0` | 0–100 | immediate | Screen curvature |
| `crt_fullscreen` | `on` | yes/no | immediate | Whether the effect covers a fullscreen window. `off` skips it on that output while the fullscreen window has focus |
| `idle_dim` | `300` | 0–86400 | immediate | Seconds of inactivity before the screen dims; `0` never |
| `idle_lock` | `600` | 0–86400 | immediate | Seconds of inactivity before the session locks; `0` never |
| `idle_off` | `900` | 0–86400 | immediate | Seconds of inactivity before the outputs power off; `0` never |
| `lid_close` | `suspend` | `suspend`, `lock`, `off` | immediate | What closing the lid does |
| `window_memory` | `yes` | yes/no | immediate | Whether an application opens where its window last was, per `app_id`, from `~/.local/state/kdos/winpos` |
| `panel` | `bottom` | `bottom`, `top`, `off` (or `none`) | next login | Which edge the panel sits on |
| `panel_cells` | `2` | 1–4 | next login | Panel height, in character cells |
| `panel_font` | `Terminus:pixelsize=20` | fontconfig pattern | next login | The panel's own font |
| `panel_margin` | `0` | 0–64 | next login | Gap between the panel and the screen edge, in pixels |
| `panel_opacity` | `80` | 20–100 | next login | Opacity of the panel's background, in per cent. Text and icons stay opaque |
| `panel_autohide` | `no` | yes/no | next login | Whether the panel retreats to a one-row strip until the pointer reaches it |
| `desktop_icons` | `yes` | yes/no | next login | Whether `~/Desktop` is drawn as icons on the background |
| `icons` | `yes` | yes/no | next login | Whether the desktop's surfaces draw pictures at all. `no` gives every surface its text-only form |
| `slit` | `no` | yes/no | next login | The column of small status gadgets, configured in [`slit.conf`](#configkdosslitconf) |
| `clipboard` | `yes` | yes/no | next login | The clipboard history, which also keeps a copied selection alive after the program it came from closes |
| `chrome_font` | `Terminus:pixelsize=32` | fontconfig pattern | next login | The font every KDOS surface draws with |
| `clock_format` | `%H:%M` | `strftime` format | next login | The panel clock. `%a %d %H:%M` adds the day and date |

**Values out of range are refused.** A number outside a key's range, or one that does not parse
(`idle_dim = 5m`), is reported and the default stands. A line with an empty value is reported and
ignored; to turn the wallpaper off, write `wallpaper = none`. `panel_opacity` stops at 20 because a
panel at zero opacity is not see-through, it is a row of invisible controls that still catch the
pointer.

**Paths.** A `wallpaper` value may begin `~/` or `$HOME/`; both are expanded. No other value is.

**The retinted wallpaper.** `kdos theme` writes a copy of the shipped wallpaper, recoloured to the
accent, to `~/.cache/kdos/wallpaper.png`. Whenever that file exists the compositor draws it instead
of the `wallpaper` path. A picture of your own is therefore shown only while that cached file is
absent; `wallpaper = none` always wins.

**In a virtual machine** the three idle timers default to `0` and `lid_close` defaults to `off`,
because a blanked screen over a remote display looks exactly like a crashed compositor, and a
virtual lid event is a stray ACPI report rather than a decision. Setting any one `idle_*` key switches
the virtual-machine default off for all three: the keys you set take your values and the others take
their normal defaults (300, 600 and 900). Write `idle_lock = 0` explicitly if you still want no
lock. Setting `lid_close` does the same for the lid. Only a line that parses counts: `idle_dim = 5m` is refused and does not
switch the timers back on.

**A startup-only key changed during a session** is reported by name when the compositor reloads,
and the running value is kept until the next login.

The two font keys have three writers besides your editor:

- `kdos-style`'s Font page writes the chosen family into both keys, keeping each key's own size and
  copying every other line of the file byte for byte, comments included.
- `kdos-settings` writes either pattern whole: `chrome_font` on its Appearance page, `panel_font` on
  its Panel page.
- `kdos theme style` writes `chrome_font` when a style file names it.

Whichever writes it, the effect is *next login*. Every surface is its own process and reads
`comp.conf` once as it starts, so a face chosen in the picker is live in that window at once and on
any other surface the next time that surface starts.

Terminus is a bitmap font with the sizes 12, 14, 16, 18, 20, 22, 24, 28 and 32. Name one of those,
or the nearest size it does have is used instead. The sizes are pixels and there is no automatic
HiDPI scaling: on a 4K screen, `Terminus:pixelsize=64` is the doubled cell.

### `~/.config/kdos/panel.conf`

What the panel shows. It is read when the panel starts and again on `SIGHUP`, so every key applies
immediately: `kdos-settings` sends the signal after writing, and after a hand edit
`pkill -x -HUP kdos-shell` does the same. The panel's edge, height and hiding are `comp.conf` keys,
not these.

| Key | Default | Means |
|---|---|---|
| `right` | `pager tray more media privacy mpris clipboard cpu stutter update restart net volume battery notify clock` | The notification-area widgets, left to right. A widget left out does not exist |
| `overflow` | `stutter restart clipboard` | Which of those widgets sit behind the `more` chevron rather than on the bar |
| `meters` | `cpu ram net` | The small charts on the panel's second row, most important first. A narrow bar drops them from the right |
| `task_labels` | `no` | `yes`, `no` or `auto`: whether a window button carries its name. `auto` shows full labels, then shortened ones, then icons only as the bar fills |
| `tray_hide` | `fcitx fcitx5 org.fcitx.fcitx5` | Tray item identifiers kept off the bar and listed in the chevron's popup instead |
| `start_label` | `yes` | Whether the Start button carries its word as well as its mark |

The sixteen widget names are `pager`, `tray`, `more`, `media`, `privacy`, `mpris`, `clipboard`,
`cpu`, `stutter`, `update`, `restart`, `net`, `volume`, `battery`, `notify` and `clock`. Every one
but `more` may go in `overflow`. The six meter names are `cpu`, `ram`, `disk`, `net`, `diskio` and
`temp`.

An empty value is meaningful for three keys: `overflow =` puts everything back on the bar,
`meters =` turns the charts off, and `tray_hide =` puts every tray item back on the bar.

`task_labels` defaults to `no` because the bar works as a dock: a two-cell bar gives each window a
square button with a picture in it, and the name is in the tooltip.

`tray_hide` starts with the input method's identifiers because its tray item opens a menu rather
than acting on a click; behind the chevron, its popup row at least says what the item is.

`temp` is the hottest sensor on the machine, on a fixed 0–100 °C scale. It is read every fourth
sample, since reading it walks `/sys/class/hwmon` and a temperature does not change meaningfully in
half a second. Where no sensor answers, the meter holds its last value rather than drawing zero.

The `update` widget shows how many installed packages are older than the version this system's
recipes describe. It reads the
`behind` field of `/var/lib/kdos/update.json`, which the system's
[update-check timer](#etckdostimersd-and-configkdostimersd) writes nightly with
`kdos update check --json`; `$XDG_STATE_HOME/kdos/update.json` is the fallback for a machine whose
timer is off and whose owner ran the check themselves. The file is re-read at most once a minute.
With no file the badge is not drawn.

Every default is restored before the file is parsed, on each reload, so deleting a line really does
return that key to its default.

### `~/.config/kdos/launcher.conf`

| Key | Default | Means |
|---|---|---|
| `files` | `no` | Meant to add this account's file index to what `Super+D` searches |

`kdos-settings` writes this key on its Desktop page, but **no surface reads it**: the searches are
fixed by which name the search surface was started under.

| Surface | Searches |
|---|---|
| `kdos-launcher` (`Super+D`) | Applications only |
| `kdos-palette` | Applications, windows, routes, settings, key chords, and files — always |
| `kdos-find` | Files by name, and file contents |

The palette's file search starts once three characters are typed. It runs `fd` over `$HOME` with
`--hidden`, `--fixed-strings` and a 200-result cap, one search at a time. It does not use
`~/.cache/kdos/plocate.db`; that index belongs to `kdos-updatedb`, which rebuilds it nightly from
`$HOME`, and `/etc/profile.d/40-plocate.sh` points the `locate` command at it through
`LOCATE_PATH`.

### `~/.config/kdos/favorites`

Your pinned applications: one desktop-entry identifier per line — the entry's file name under
`/usr/share/applications` or `~/.local/share/applications`, without `.desktop`. The panel and the
menus write it when you pin, unpin or reorder.

Two surfaces read this one file: the panel's quick-launch row and the Start menu's pinned column.

It ships with seven rows:

```
mc code=FM
btop code=MO
lazygit code=GI
foot code=TE
firefox-esr code=WW
org.xfce.mousepad code=ED
gimp code=IM
```

Delete every line for a bare desktop. Each surface shows the first eight identifiers; later lines
are read and never shown. An identifier with no installed entry is skipped silently, so a boxed
application that is not installed yet leaves no dead launcher.

**Two-letter codes.** A line may carry `code=XX`. The code is drawn at the right of the row, in the
Start menu and in `kdos-palette`, and typing those two letters (in either case) with nothing else
in the search field opens that row at once — no arrow keys and no `Enter`. This is the shorthand
DESQview's Open Window menu used. Letters are only taken as a code while a code could still match
them; with no coded line, or a first letter no code starts with, they are an ordinary search.
`kdos-launcher` does not act on codes.

Anything else after the identifier is ignored rather than refused, so a line written for a later
version still launches.

**The terminal.** A line naming `foot` or `kdos-term` opens whichever terminal emulator the session
uses — the same one every chord, menu row and `Terminal=true` entry gets. Every other line opens
exactly the entry it names.

### `~/.config/kdos/slit.conf`

The gadgets shown in the slit, a narrow column docked at the right edge of each screen. The slit
runs only when `comp.conf` has `slit = yes`, and exits when this file is absent (it ships absent).

One gadget per line:

```
<interval> <width> <command and its arguments>
```

`<interval>` is the seconds between runs and `<width>` the cells the gadget gets. The rest is the
command, split on spaces and tabs with no quoting and no shell, up to 16 words. The first line the
command prints is what is drawn, clipped to the width; a command that fails shows a dim `!` and
keeps its row.

```
60 8 date +%H:%M
5 14 cat /proc/loadavg
```

Up to 32 gadgets. Blank lines and `#` comments are skipped, an interval below one second is raised
to one, and a width is clamped to 1–64. The slit is as wide as its widest gadget and reserves that
space, so a maximised window stops short of it rather than covering it. `kdos-slit --help` prints
the syntax.

### `~/.config/kdos/session-restore`

Ships absent. Its existence is the setting; its contents are ignored. With it, the boxed
applications that were running when the session ended are started again at the next login, two
seconds apart, in the background.

The list is `$XDG_STATE_HOME/kdos/session`, written by `kdos-session-save` from the menu's Log Out,
Restart and Shut Down rows (and from `Super+Escape`), before the confirmation dialog. Each line is
`app <name>` for a boxed application or `native <app_id>` for a host program. Only the `app` lines
are relaunched. Where windows reappear on screen is decided by `comp.conf`'s `window_memory`, not by
this list.

To turn it on:

```sh
touch ~/.config/kdos/session-restore
```

### `~/.config/kdos/backup.conf`

What `kdos-backup` copies with [restic](https://restic.net), and where to. `include` and `exclude`
may repeat, up to eight of each; any other key repeated keeps its last value. The paths become
separate arguments, so a directory with a space in its name stays one path.

| Key | Default | Means |
|---|---|---|
| `repo` | — | The restic repository |
| `include` | — | A directory to back up. Repeatable, eight at most |
| `exclude` | — | A directory to leave out. Repeatable, eight at most |

The file ships with every line commented out, because the first run over a home directory can take
hours and should be a choice.

To set it up:

1. Create the repository: `restic -r <repo> init`. It asks for a password twice.
2. Put that password alone in `~/.config/kdos/backup.pass` and make it mode 0600
   (`chmod 600 ~/.config/kdos/backup.pass`). `kdos-backup` refuses to run while the file is
   readable by group or others.
3. Uncomment `repo` and the `include`/`exclude` lines you want.
4. Run a backup from `kdos-backup` (press `b`), or enable the nightly timer described in the next
   section.

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

The two tiers differ in when a job can fire:

| Tier | Started by | Fires |
|---|---|---|
| `/etc/kdos/timers.d/` | The system supervisor, at boot (`/etc/init.d/18_timers.sh`), as root | Every occurrence, whether or not anybody is logged in |
| `~/.config/kdos/timers.d/` | Your session, at login, and stopped when it ends | Every occurrence while that login lasts |

`snooze` waits for its slot, runs the command once and exits, so something has to start it again.
For the system tier the supervisor does that; each system job is a service named
`kdos-timer-NAME`. For the user tier the session runs a loop per row and records the loops' process
IDs in `$XDG_RUNTIME_DIR/kdos/timers.pid`, so the next login stops whatever the last one left
running. A loop whose command fails instantly waits at least one second before retrying.

Before a row's loop starts, the command's first word must be on `PATH` and `snooze -n` must accept
the timespec. A row that fails either check is not started.

Choose the tier by who must be there: a job that must run on a machine nobody has logged into goes
in the system tier; a job that writes into `$HOME` goes in the user tier.

Shipped jobs, at deliberately different times so that a laptop waking up is not running several
at once:

| File | Tier | When | Runs |
|---|---|---|---|
| `/etc/kdos/timers.d/10-update-check.timer` | System | 04:17, slack 8 h | `kdos update check --json --out /var/lib/kdos/update.json` |
| `/etc/kdos/timers.d/20-fstrim.timer` | System | 12:47, slack 20 h | `fstrim -a` |
| `/etc/kdos/timers.d/30-fwupd-refresh.timer` | System, from the `fwupd` package | 13:43, slack 20 h | `fwupdmgr refresh` |
| `~/.config/kdos/timers.d/20-updatedb.timer` | User | 03:05, slack 8 h | `kdos-updatedb` |
| `~/.config/kdos/timers.d/10-backup.timer` | User, commented out | 03:40, slack 8 h | `kdos-backup --once` |

### `~/.config/kdos/screensaver.txt`

A UTF-8 grid of characters, one line per row, drawn by the screensaver's `art` and `bounce`
effects. The other six effects need no file. Colour escape sequences in it are stripped; the effect
picks the colour.

Ships absent; `/usr/share/kdos/screensaver.txt` is drawn without it. A grid wider or taller than
the screen is pinned in place rather than bounced, and trailing blank lines are dropped.

This is separate from `/usr/share/kdos/logo.txt`, the login banner's picture, so replacing the
screensaver does not change the picture the machine boots with.

### `~/.config/kdos/a11y`

Ships absent. Its existence — an empty file is enough — opts boxed applications into the
accessibility stack. `KDOS_A11Y=1` does the same for one launch.

The host runs no accessibility registry, so the default skips a startup probe that would always
time out. A screen reader running inside a box reaches that box's own registry, which is what this
file enables. Nothing reads the desktop itself; see
[Accessibility](../02-user-guide/accessibility.md).

### `~/.config/kdos/displays.conf`

The screen layout `kdos-display` keeps. It is written when you keep a layout in `kdos-display`, and
replayed by `kdos-display --apply`, which the compositor runs at each login. One line per screen:

```
output <name> mode <W>x<H>@<mHz> scale <milli> transform <n> pos <x> <y>
output <name> off
```

Refresh is in millihertz and scale in thousandths (`scale 1500` is 1.5×). A screen switched off
has its own `off` line, so it stays off at the next login. A screen the file does not name is left
as the compositor brought it up.

### `~/.local/state/kdos/toggles/`

The on/off switches a desktop needs at hand. A file's presence means on; there is no format.

| Toggle | Means | Read by |
|---|---|---|
| `stay-awake` | Never dim, lock or blank on idle | The compositor's idle timers |
| `night-light` | Warm the colour palette | Every surface, on the retint signal |
| `dnd` | Do not disturb: hold notifications back | The notification daemon, `kdos-notifyd` |

`kdos toggle` lists them, `kdos toggle <name>` flips one, and `kdos toggle <name> on|off` sets it.
The menu routes `toggle.night`, `toggle.quiet` and `toggle.awake` do the same (see
[`menu.conf`](#etckdosmenuconf-merged-under-configkdosmenuconf)).

These are state files rather than configuration keys because another process sets them — a chord,
a menu row, a script before a long build — and their readers must not hold a copy. `stay-awake` and
`dnd` are checked on a timer their readers already run. `night-light` is applied on the retint
signal, which `kdos toggle` sends after writing the file.

`stay-awake` suppresses all three idle steps, not only the first: somebody who turned the
screensaver off for a presentation did not ask to be locked either.

Night light warms whatever accent is loaded: green is scaled to 93% and blue to 77%, red is left
alone, so the accent still reads as itself. Turning it off returns to the original palette.

Do Not Disturb in the notification centre writes this same `dnd` file through the daemon. A held
notification still reaches the history and the badge, so it is hidden rather than lost, and an
urgent one is shown anyway.

A program that reads or writes these files does it through `libkbase`; see
[C libraries](../05-developer/c-libraries.md).

### `/etc/kdos/menu.conf`, merged under `~/.config/kdos/menu.conf`

A *route* is a stable name for a place in the system, such as `setup.network`, that a script, a
document or another program can refer to. `kdos menu summon setup.network` opens the network
settings, and keeps working when a chord is rebound or a menu row moves.

One `route = command` per line:

```
setup.network      = kdos-net
style.font         = kdos-style --page font
toggle.night       = kdos toggle night-light
```

The name is `verb.noun`, and the verb describes what you are doing rather than which program does
it: someone looking for Wi-Fi is setting something up, whichever surface owns it. The shipped verbs
are `setup`, `style`, `system`, `learn`, `capture`, `share`, `toggle` and `about`.

The command is split on spaces and run without a shell. There is no quoting, so a route cannot
become a way to run arbitrary shell code from a file you own.

The system file is read first and yours second. A route named in both takes your value; a route
only in yours is added. Routes cannot be deleted, so a name a script depends on keeps resolving.

`kdos-start` searches routes alongside its own rows: typing `network` finds the row and
`setup.network` finds the same thing.

**Settings.** A key beginning `@` is a setting about the menu rather than a route. It is read by the
surface that asks for it and is never run.

| Setting | Default | What it does |
|---|---|---|
| `@toplevel` | `Network Sound Displays Terminal` | Which system rows `kdos-start` keeps visible when it is too narrow for three columns |

Below 100 columns the Start menu folds its system group behind one `Settings ▸` row, and the labels
named in `@toplevel` stay listed beside it. The value is labels as the menu draws them, separated
by spaces or commas, matched whole and case-insensitively. A label that matches no row is ignored
silently.

`kdos-settings` writes `@toplevel` on its Panel page into *your* copy, copying every other line of
the file through unchanged.

---

## Programs

### `~/.config/kdos/res.conf`

The resource monitor, `kdos-res`. The file is optional; an unknown key is reported by name.

| Key | Default | Means |
|---|---|---|
| `interval` | `1000` | Sampling interval in milliseconds, clamped to 200–60000. A monitor sampling faster than 200 ms mostly measures itself |
| `units` | `1024` | `1024` gives KiB/MiB/GiB; `1000` gives kB/MB/GB |
| `temperature` | `c` | `c` or `f`, everywhere a sensor is shown |
| `cpu_percent` | `core` | `core`: eight busy threads read 800%, as in `top`. `machine`: the same load reads 100% |
| `memory` | `rss` | `rss` counts a shared page against every process using it; `pss` divides it between them, so the numbers add up |
| `kernel_threads` | `no` | Show kernel threads in the process table. The footer says how many are hidden either way |
| `virtual_drives` | `no` | Show loop, zram and device-mapper devices on the Drives page |
| `virtual_net` | `no` | Show loopback, bridges and container interfaces on the Network page |
| `icons` | `yes` | Draw pictures beside rows; `no` is text only |
| `sort` | `cpu` | The column the Processes, Apps and Boxes pages sort on. Processes accepts `cpu`, `memory`, `pid`, `name`, `disk`; Apps and Boxes accept `name`, `cpu`, `memory`, `disk`, `procs`. A name a page does not have leaves that page on its own default |
| `columns` | empty | Parsed and stored, but no page applies it: every page draws all of its columns |

`kdos-settings` writes all of these on its Hardware page and signals `kdos-res`, so a change
applies immediately. After a hand edit, `pkill -HUP -x kdos-res` applies it. The `-x` matters:
without it the signal also reaches `kdos-resctl`, a separate helper whose name starts the same way.

### `~/.config/kdos/term.conf`

The terminal, `kdos-term`. Every key has a working default and the file need not exist. An unknown
key is reported by name. Numbers outside a key's range are clamped to it.

| Key | Default | Range | Means |
|---|---|---|---|
| `shell` | `$SHELL`, then `/bin/sh` | command | What `kdos-term` runs when given no command. Split like a desktop entry's `Exec`; no shell is involved |
| `font` | the toolkit's default | fontconfig name | The font and the size a window opens at. `Ctrl+=` and `Ctrl+-` step it for that window alone; `Ctrl+0` returns to this value |
| `columns` | `80` | 20–1000 | Columns asked for when the window first opens |
| `rows` | `24` | 4–1000 | Rows asked for when the window first opens |
| `scrollback` | `2000` | 0–200000 | Lines kept above the screen |
| `images` | `yes` | yes/no | Decode pictures. `no` turns the three image protocols off in the parser, not merely in the drawing |
| `image_max` | `1024` | 4–65536 | The largest single image payload accepted, in kilobytes |
| `image_cells` | `200` | 1–1000 | The widest and tallest a picture may be, in cells |
| `paste_guard` | `yes` | yes/no | The check behind the paste filter: hold back an unbracketed paste that still contains a control byte other than tab, and ask with a **Paste** / **Cancel** dialog. `no` turns the guard off; the filter stays on either way |
| `opacity` | `100` | 20–100 | How much of its own background the window keeps, in per cent |

`paste_guard` exists because of what an unbracketed paste does. With bracketed paste on, the
program in the terminal receives the text as text and decides what to do. With it off, the bytes go
straight to the terminal and a newline *runs* what came before it — at a shell prompt, at an `ssh`
password prompt, inside `read`. Every paste first passes through a filter that turns each newline
into a space and removes every other control byte, so no keyboard or mouse paste reaches the guard
with a control byte left in it, and in practice its dialog never appears. See
[Pasting, and the paste guard](../04-programs/kdos-term.md#pasting-and-the-paste-guard).

Below 100, `opacity` lets the desktop show through the terminal's background; text is never made
transparent.

`kdos-settings` writes all of these on its Desktop page and signals `kdos-term`, so a change reaches
every open terminal. The file is re-read on `SIGHUP`, which `kdos theme` also sends. A window whose
font size or transparency you stepped by hand keeps what you stepped: this file is where a window
*starts*.

### `~/.config/kdos/boxes/<name>.conf`

One file per box — the container a boxed application runs in (see
[Packs and boxes](../03-architecture/packs-and-boxes.md#the-box)). Each key maps onto a
container-engine flag or onto something KDOS enforces itself, and `kdos-box profile <name>` prints
which, including what the key cannot enforce. Edit these with `kdos-box profile`, or through
`kdos-settings`, which runs that command rather than writing the file itself.

Namespace keys (`network`, `ipc`, `processes`, `devices`, `home`) are yes-or-no in effect: `private`
(or any yes spelling) gives the box its own; anything else shares the host's.

| Key | Default | Applies | Means |
|---|---|---|---|
| `base` | — | create time | `pack:<id>`, `box:<name>`, or `image:<ref>` |
| `image` | — | create time | The registry reference, for an image base |
| `persistence` | `persistent` | — | `persistent`, `ephemeral` or `frozen`. Recorded only: no launch reads it, and a box's writes land wherever its runtime puts them |
| `network` | `host` | create time | `host`, `private`, or `none` — a private namespace with no interface at all |
| `ipc` | shared | create time | IPC namespace |
| `processes` | shared | create time | PID namespace |
| `devices` | shared | create time | Whether the host's `/dev` and `/sys` are shared |
| `home` | shared | create time | `private` gives the box its own home under `~/.local/share/kdos/boxes/<name>` instead of your `$HOME` |
| `init` | `no` | create time | Whether the engine runs an init (`catatonit`) inside the container |
| `wayland` | `yes` | next launch | Whether the box gets its own tagged compositor socket. `no` cannot be enforced — the box shares `$XDG_RUNTIME_DIR`, which holds the session's socket — and the profile printer says so |
| `audio` | `yes` | create time | Follows `devices`; see below |
| `gpu` | `yes` | create time | The graphics card's device nodes |
| `render` | `auto` | next launch | Which graphics the box's applications get: `auto`, `gpu`, or `software` |
| `memory` | — | create time | Memory limit such as `4G`, passed to the engine as `--memory`. The memory daemon also prefers a box over its limit when it has to kill something, since rootless containers often cannot enforce the flag |
| `cpus` | — | create time | CPU quota, as the engine's own `--cpus` value |
| `pids` | — | create time | Process-count limit (`--pids-limit`) |
| `autostop` | `0` | next collection | Idle time after which `kdos-box gc` stops the box: `90s`, `30m`, `2h`, or bare seconds. `0` disables |
| `accent` | — | immediate | The box's colour, drawn as a chip on its windows' title bars |
| `grant` | — | immediate | Compositor features the sandbox otherwise refuses, comma-separated (see below) |
| `export` | — | — | Recorded only. `auto` does not create launchers by itself: `kdos-appbox genlaunchers` covers every installed pack and store box, and `kdos-box export` covers one application |
| `display` | — | — | Recorded only; nothing on this desktop reads it. It is kept so that rewriting a profile does not drop it |

**Namespace keys** apply when the box is created and cannot be changed on a running container, so
changing one tells you to recreate the box rather than silently doing nothing.

**`gpu` and `audio`** cannot subtract anything from a shared `/dev`: there is no engine flag that
grants a box a speaker and denies it a camera. With `devices = private`, `gpu = yes` binds
`/dev/dri` back in by itself. The profile printer states both cases as they are.

**`render`** resolves by *opening* a `/dev/dri/renderD*` node: hardware where one opens, software
where none does. `gpu` asks for the same, and `software` refuses the card whatever is present. For a
software box the launch sets `LIBGL_ALWAYS_SOFTWARE=1`, which is advisory — an application may
unset it. For a box that can see no device node (`devices = private` with `gpu = no`) the answer is
software without probing. `render` is separate from `gpu` because `gpu` is about device nodes and
`render` is about who draws.

**`grant`** names, one or more of: `screencopy`, `toplevel-capture`, `export-dmabuf`,
`data-control`, `foreign-toplevel`, `layer-shell`, `input-method`, `output-power`. A name not in this
list grants nothing. Each opens the matching Wayland protocols to that box's windows only; see
[the session](../03-architecture/session.md#granting-a-box-more-than-the-allowlist).

`base = image:` pulls from a registry, so it needs the network, and says so before doing anything.

### `~/.config/kdos/sandbox/<profile>.conf`

Profiles for `kdos sandbox`, which runs a *native* (non-boxed) program under a Landlock ruleset. A
profile is named on the command line — `kdos sandbox <profile> -- <cmd>` — and combines with the
command-line options.

| Key | Repeatable | Means |
|---|---|---|
| `read` | yes | A path the program may read |
| `write` | yes | A path the program may read and write |
| `network` | no | `off` denies TCP bind and connect. Needs Landlock ABI 4; on an older kernel `--explain` says it cannot be enforced |
| `tcp-connect` | yes | A TCP port the program may still connect to while `network = off` |

An unknown key is reported. `kdos sandbox <profile> --explain` prints exactly what will be asked
for. See [`kdos sandbox`](../04-programs/kdos-command.md#kdos-sandbox).

---

## The compositor's own files

### `~/.config/kdos-comp/rc.xml`

The compositor's main configuration. The compositor is a fork of labwc, and this file is in labwc's
format, so labwc's documentation applies unchanged: key bindings, mouse behaviour, window rules,
workspaces, theme keys and fonts.

`<default />` must be the first child of both `<keyboard>` and `<mouse>`. The compositor loads its
built-in bindings only when your file defines none of that kind, so a file that binds one key
throws every default away — including click-to-focus, dragging by the title bar and the window
buttons. Put your own bindings *after* `<default />`; of two identical bindings, the later wins.

The title-bar font (`<theme><font>`) must name a scalable face, sized in points (24 pt is 32 px at
96 dpi). Naming the bitmap console font resolves, and then falls back silently to a generic sans.

Applies at next login, or on reload for the parts the compositor re-reads.

### `~/.config/kdos-comp/menu.xml`

The desktop's root and window menus. It lists no applications on purpose: those come from a program
that reads the same desktop entries everything else does, so the menu cannot go stale.

### `~/.config/kdos-comp/themerc-override`

Generated by `kdos theme`; do not edit. The dotted keys of a style file are kept in
`~/.config/kdos/style-themerc` and appended after the generated block, where they win.

---

## The machine

### `/etc/kdos/packd.conf`

The pack daemon, `kdos-packd`, which installs and mounts application packs.

| Key | Default | Applies | Means |
|---|---|---|---|
| `retain` | `1` | next start | How many superseded versions of a pack the store keeps |

Retention is what makes rollback possible: `1` keeps the version you just replaced. `0` keeps none,
and a rollback then answers that no earlier version is kept.

Old versions are removed right after an install and at no other time, so a background job never
deletes a rollback while you are deciding whether to use it.

### `/etc/kdos/zram.conf`

Compressed swap in RAM.

| Key | Default | Range | Applies | Means |
|---|---|---|---|---|
| `size` | `50` | 1–90 | boot | Swap size as a percentage of RAM |
| `algorithm` | `zstd` | a compressor the kernel carries | boot | Compression algorithm |

`size` is how much swap the device may claim to hold, not how much memory it uses — the compressed
pages live in that same memory. A value outside 1–90 is reported and 50 is used. An algorithm the
kernel does not carry is reported and the kernel's default stands. While the device is active,
zswap is turned off (so no page is compressed twice) and `vm.page-cluster` is 0 (read-ahead on zram
is wasted decompression); stopping the service restores both.

### `/etc/kdos/mountd.conf`

The removable-media daemon, `kdos-mountd`. Not shipped; create it to change the defaults. It is
read on each request, so a change applies to the next mount or format.

| Key | Default | Means |
|---|---|---|
| `exec` | `no` | Whether removable media are mounted with programs allowed to run |
| `format` | `no` | Whether `kdos-mountd` will write a new filesystem over a device at all |

**Spelling matters.** The daemon searches the file for the exact text `exec = yes` or `exec=yes`
(and likewise `format = yes` or `format=yes`). Other spellings such as `exec = true` are not
recognised, and the text counts even on a line that starts with `#`.

Everything removable is mounted without setuid and without device nodes whatever this file says; a
setuid program on someone else's USB stick is a local root hole. `exec = yes` is how you say you
accept programs on removable media.

`format = yes` exists for the same reason. Writing a filesystem cannot be undone, so the verb is off
unless you turn it on. Even then the daemon refuses the medium the system booted from and requires
the device's kernel name to be typed back.

### `/etc/kdos/update.conf`

Not shipped. Tells `kdos update` where to take binary packages from.

| Key | Default | Means |
|---|---|---|
| `binhost` | — | A directory holding a signed package index (made with `kpkg index <dir> --sign <key>`). A path, never a URL: a USB stick, an NFS mount or a local directory |

`KDOS_BINHOST` overrides it. Without either, `kdos update` has nothing to install from and says so.
The index's signature is checked against [`/etc/kdos/keys/`](#etckdoskeys). See
[`kdos update`](../04-programs/kdos-command.md#kdos-update).

### `/etc/kdos/keys/`

The trusted keys. The directory *is* the policy: every public key (`*.pub`) here is one this machine
accepts a signature from. Trusting a key is copying a file in; removing trust is deleting it. There
is no revocation list and no online check.

| Directory | Trusted for |
|---|---|
| `/etc/kdos/keys/` | Host package indexes (a binhost's `PACKAGES.sig`) and package signatures |
| `/etc/kdos/keys/packs/` | Application packs and pack indexes |

Keys are read from each directory without descending into subdirectories, which keeps the two
policies separate: a pack key cannot become a trusted source of host packages.

The host directory ships with no keys, only a `README` stating the policy. Packages are built from
source on the machine and checked against the `sha256` in their recipe, so nothing needs a
signature until you choose a binhost. To trust one:

```sh
kpkg keygen builder                 # on the machine that builds
cp builder.pub /etc/kdos/keys/      # on every machine that should trust it
```

The pack directory ships one key, `kdos-packs.pub`. A pack or pack index signed with its private
half is accepted. Replace it with your own key to trust packs you sign yourself.

### `/etc/kdos/login.conf`

Which account tty1 logs in without a password, and nothing else.

| Key | Default | Applies | Means |
|---|---|---|---|
| `autologin` | `kdos` | boot | The account tty1 logs in automatically |

With the key absent or commented out, tty1 shows a password prompt. Turning autologin off comments
the line out rather than emptying it — an empty value would name an account called `""`. That is
what `kdos-power autologin off` writes, and what the installer writes when you choose no autologin.

This is the only place the desktop account is named: `/etc/inittab` names none. **If you rename the
desktop user, update this key**, or tty1 fails to log in and the machine is reachable only from
tty2.

### `/etc/nftables.conf`

The firewall. It is applied at boot, before the network starts, after a syntax check — a ruleset
that does not load leaves the previous state standing rather than half-applying. The file replaces
only its own `inet filter` table, so podman's `inet netavark` table and NetworkManager's
`nm-shared-*` tables survive a reload, and changing the firewall does not cut off running
containers or hotspot clients.

The shipped policy:

- **Input** is dropped by default. Accepted: established and related traffic, loopback, the
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
| `50-kdos-services.nft` | `kdos-firewall`, through `kdos-powerd` | Opens the services you turned on |
| `40-podman.nft` | Shipped | Lets rootful podman's bridges forward and reach their DNS |

See [The firewall](../02-user-guide/administration.md#the-firewall) for opening a service.

### `/etc/fstab`

Applies at boot. The shipped entries mount the kernel's pseudo-filesystems (`/proc`, `/sys`,
`/dev`, tracefs, debugfs and bpf), the cgroup v2 hierarchy with `nsdelegate`, and two temporary
filesystems: `/tmp` at mode 1777 and `/run` at mode 0755.

`/tmp` must be mode 1777. With default options, the mount is root-only and *hides* the correctly
permissioned directory underneath, so no ordinary user can write there — and every graphical
application depends on it. A mounted filesystem ignores a mode change on remount, so the boot
sequence also sets the mode explicitly.

The installer appends to this file rather than replacing it, to keep those entries.

### `/etc/inittab`

Applies at boot. What each line starts:

| Line | Starts |
|---|---|
| tty1 | `kdos-login`, which hands the terminal to `agetty` and logs in the account `login.conf` names, if any |
| tty2 | An ordinary `getty` login: the recovery console |
| Serial line `ttyS0` | A root login shell (`bash -l`) after you press Enter, with no password |

tty1 and tty2 both start through `kdos-getty`, which loads the console font and colour palette
first. On shutdown, `/etc/init.d/rcK` runs every enabled service script's `stop` in reverse order —
except the firewall, which stays loaded to the end — then turns swap off and remounts every
filesystem read-only.

### `/etc/service.disabled/<name>`

A marker file, not a setting. Creating one stops the matching init script from running at boot:

```sh
sudo touch /etc/service.disabled/cups
```

The name is the init script's with the order prefix and `.sh` removed: `42_networkmanager.sh` is
`networkmanager`. Delete the file to enable the service again.

`networkmanager` has a second effect. `30_network` starts `dhcpcd` only when NetworkManager is
absent or disabled this way, so turning NetworkManager off hands DHCP back to `dhcpcd` rather than
leaving the machine with no DHCP client.

### `/etc/modprobe.d/kdos-regdom.conf`

The Wi-Fi regulatory country, as `options cfg80211 ieee80211_regdom=<CC>`. Written by the installer
and by `kdos-power timezone`, from the time zone's row in `zone.tab`, and removed for a zone with no
country. Applies when `cfg80211` loads, which is the next boot; `iw reg set <CC>` changes the
running radio.

### `/etc/keymap`

The console keymap name, written by the installer. `kdos-getty` loads it on every terminal, and the
session translates it into a graphical keyboard layout when it starts.

---

## Speech-to-text models

`kdos-rec` greys out *Transcribe* until a [whisper.cpp](https://github.com/ggml-org/whisper.cpp)
model is on the machine. There is no configuration key; three locations are searched in order and
the first hit wins.

| Order | Location |
|---|---|
| 1 | `$KDOS_WHISPER_MODEL` — one file, named exactly |
| 2 | `$XDG_DATA_HOME/whisper.cpp/models/`, by default `~/.local/share/whisper.cpp/models/` |
| 3 | `/usr/share/whisper.cpp/models/` |

When `$KDOS_WHISPER_MODEL` is set it is the only candidate. No directory is searched after it, and a
file that is missing or fails the check below leaves transcription unavailable rather than quietly
using a different model.

In a directory the pattern is `ggml-*.bin`, and the first in sorted name order wins (not the newest,
so the choice is reproducible).

A file counts only if its first four bytes are `lmgg`, the model format's magic, so a half-finished
download reads as *no model* rather than as a crash. The surface's header line names the model it
found, or the directory it searched.

The directory name is upstream's own, so a model fetched by upstream's
`models/download-ggml-model.sh` and one fetched by KDOS land in the same place.

**Getting a model.** No model ships. `kdos speech list` prints the catalogue, smallest first: the
English-only `tiny.en-q5_1`, `tiny.en`, `base.en-q5_1`, `base.en`, `small.en-q5_1`, `small.en` and
`medium.en`, the multilingual `tiny`, `base` and `small`, and `large-v3-turbo` and `large-v3`.
`kdos speech get <name>` downloads one into `$XDG_DATA_HOME/whisper.cpp/models` (no privilege
needed); with no name it gets `base.en`. The name is looked up in a table, never pasted into a URL;
the download is checked against a SHA-256 built into the tool, and the file is moved into place only
after both that and the `lmgg` check pass, so an interrupted download never leaves a file behind.
Upstream signs nothing; the checksum only says the bytes are the ones this tree was written against
(see the [security model](../03-architecture/security-model.md)). See
[`kdos speech`](../04-programs/kdos-command.md#kdos-speech).

While there is no model, `kdos-rec`'s third button reads *Get model* and opens a terminal running
that command. The window keeps checking, so the button becomes *Transcribe* by itself when the file
arrives.

`whisper-stream`, which transcribes a live microphone, uses the same model. It runs in a terminal
and opens no window; nothing on the desktop starts it. `kdos-rec`'s *Transcribe* runs `whisper-cli`
on the file it has just recorded.

## Video in a call

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

## Shipped configuration for software that is not ours

`/etc/skel` also carries configuration for third-party programs, so a new account gets a working,
consistently coloured setup rather than each program's own defaults. These files are yours to edit,
except the ones marked *Generated*, which `kdos theme` rewrites.

| Path | For | Notes |
|---|---|---|
| `~/.config/foot/foot.ini` | The terminal `foot` | Asks for server-side decorations, so windows wear the compositor's frame |
| `~/.config/foot/themes/kdos` | foot's colours | Generated |
| `~/.config/btop/btop.conf` | The system monitor `btop` | |
| `~/.config/btop/themes/kdos.theme` | btop's colours | Generated |
| `~/.config/kdos/term-colors.conf` | The sixteen terminal colours, as this desktop's terminals draw them | Generated |
| `~/.config/kdos/ls-colors` | `LS_COLORS`, loaded by `~/.bashrc` | Generated |
| `~/.config/kdos/fzf-colors` | fzf's `--color` flags, read by `/etc/profile.d/30-kdos-colors.sh` | Generated. See the note on `FZF_DEFAULT_OPTS` under [Shell environment](#shell-environment) |
| `~/.config/bat/config` | The pager `bat` | One line: `--theme="kdos"` |
| `~/.config/bat/themes/kdos.tmTheme` | bat's theme, selected by file name | Generated |
| `~/.config/micro/settings.json` | The editor `micro` | Selects the generated colour scheme |
| `~/.config/micro/colorschemes/kdos.micro` | micro's colour scheme | Generated |
| `~/.config/helix/themes/kdos.toml` | helix's theme | Generated. helix is not on the image; the file is for a helix installed in a box, which shares this `$HOME`. Nothing selects it: write `theme = "kdos"` in helix's `config.toml` yourself |
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
| `~/.config/gtk-3.0/settings.ini`, `~/.config/gtk-4.0/settings.ini` | GTK settings | Theme, cursor theme and size. Generated |
| `~/.config/user-dirs.dirs` | The standard user directories (Documents, Downloads, …) | Seeded from `/etc/skel`; there is no `xdg-user-dirs` program. `$HOME` is the only expansion understood |
| `~/.config/kdos/places` | Extra rows in the places column of file pickers and menus, one `Name = /path` per line | Not shipped; written by *Add to Places*. Merged over the user directories: a row whose path is already listed is dropped, and a row pointing at nothing is not shown |
| `~/.config/xdg-desktop-portal-wlr/config` | Screen capture for recording and sharing | Uses an output picker; the alternative captures the first output silently, which is wrong as soon as a second screen is plugged in |
| `~/.config/fcitx5/profile` | The input method `fcitx5` | One group, `Default`, with the four input methods the image carries: `keyboard-us`, `pinyin`, `anthy` and `hangul` |
| `~/.config/mimeapps.list` | Your own choices of which application opens which file type | Ships with an empty `[Default Applications]` section. This file outranks the system tables, and *Open With*'s **always** option writes to it |
| `~/.config/tealdeer/config.toml` | The `tldr` client `tealdeer` | Installed into `/etc/skel` by the `tealdeer` package. `cache_dir` is `/usr/share/tldr`, where the pages ship, so nothing is downloaded. `tldr --update` needs a `cache_dir` you can write |
| `~/.bashrc`, `~/.bash_profile` | bash | `.bashrc` reads `/etc/bash.bashrc` — through `/run/host` inside a box, since `$HOME` is shared with every box — and `~/.config/kdos/ls-colors`. `.bash_profile` starts the desktop on tty1 |
| `~/.zprofile` | zsh | Starts the desktop on tty1, as `.bash_profile` does. `/etc/shells` lists zsh, so `chsh -s /usr/bin/zsh` is accepted. `/etc/zsh/zprofile` reads `/etc/profile` and its drop-ins; `/etc/zsh/zshrc` sets up `atuin` and nothing from `/etc/bash.bashrc` |

### Default handlers

Which application opens a file type is decided by `kdos-appbox open` (which is also `xdg-open`).
It consults these tables in order:

| Path | Scope |
|---|---|
| `~/.config/kdos-mimeapps.list` | This desktop, this account |
| `~/.config/mimeapps.list` | Any desktop, this account |
| `/etc/xdg/kdos-mimeapps.list` | This desktop, system-wide |
| `/etc/xdg/mimeapps.list` | Any desktop, system-wide |

In each file the `[Default Applications]` section is read first. The account files'
`[Added Associations]` are read after the account defaults and before the system tables. The
generated `mimeinfo.cache` files beside the installed launchers are consulted last. A file type
belongs in exactly one of the two system-wide tables.

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
The `F2` menu has nine entries — open with the desktop's handler, peek, edit, find in this folder,
terminal here, add to Places, share, move to trash, and git status — each naming a program on the
image.

### Shell environment

A login shell reads these files; logging in is the only way into a session, so every desktop program
inherits them.

| Path | Sets |
|---|---|
| `/etc/profile.d/10-wayland.sh` | The session basics: `XDG_RUNTIME_DIR` (created at `/run/user/<uid>` if missing), `XDG_SESSION_TYPE=wayland`, `XDG_CURRENT_DESKTOP=KDOS` unless already set, `DBUS_SESSION_BUS_ADDRESS` when the session bus exists, the `XDG_*_HOME` and `XDG_DATA_DIRS` defaults, `~/.local/bin` and `/usr/games` on `PATH`, `XCURSOR_THEME=KDOS-cursors`, `XCURSOR_SIZE=24`, and Wayland back ends for Qt, GTK, Firefox, Java, SDL and Clutter |
| `/etc/profile.d/20-timezone.sh` | `TZ=:/etc/localtime`. Written by the installer and by `kdos-power timezone`; not shipped with the image |
| `/etc/profile.d/20-lesspipe.sh` | `LESSOPEN` to `lesspipe.sh`, and `LESS=-R` |
| `/etc/profile.d/30-kdos-colors.sh` | `FZF_DEFAULT_OPTS` from the generated `~/.config/kdos/fzf-colors` |
| `/etc/profile.d/30-open.sh` | `BROWSER` to `xdg-open`, which on this image is `kdos-appbox open`, so the variable and the handler tables above agree |
| `/etc/profile.d/40-plocate.sh` | `LOCATE_PATH` to this account's own file index |
| `/etc/profile.d/50-ssh-agent.sh` | `SSH_AUTH_SOCK` to `$XDG_RUNTIME_DIR/ssh-agent.socket`, starting `ssh-agent` there when nothing answers. One agent per account, shared by every login and everything the desktop starts, so `ssh-add` asks for a passphrase once. From `openssh` |
| `/etc/profile.d/50-sfeed.sh` | `SFEED_YANKER` to `wl-copy -n`, so `sfeed_curses`'s yank reaches the clipboard. From `sfeed` |
| `/etc/profile.d/gawk.sh` | No variables: the `gawkpath_*` and `gawklibpath_*` functions, which edit `AWKPATH` and `AWKLIBPATH`. From `gawk` |
| `/etc/profile.d/50-opencl.sh` | `RUSTICL_ENABLE` to `iris,radeonsi`, the Intel and AMD drivers of Mesa's OpenCL implementation. Without it Rusticl offers no device and every OpenCL program finds none. `llvmpipe` is left out, so a machine with no supported GPU has no OpenCL device |
| `/etc/profile.d/podman-docker.sh` | `DOCKER_HOST` to the rootless Podman API socket, `$XDG_RUNTIME_DIR/podman/podman.sock` (root's is `/run/podman/podman.sock`), so a Docker API client such as `lazydocker` finds `podman system service` while it runs — see [Containers](#containers) for what starts it. `/usr/bin/docker` is Podman's `docker` shim |

Apart from `10-wayland.sh` (session type, cursor and toolkit back ends) and `40-plocate.sh`
(`LOCATE_PATH`), which set their values unconditionally, none of these overwrites a value you
already exported. The `less` filter decides
what a file is with `file -L -s -b --mime` and nothing else, which is why the image ships the `file`
with a full magic database.

What depends on the terminal is set in every interactive bash instead, in `/etc/bash.bashrc`. Apart from
`EDITOR`, each line first checks that its program is installed:

| Variable or hook | Is |
|---|---|
| `EDITOR`, `VISUAL` | `nvim` when neovim is installed, replacing any exported value (with `vi` and `vim` aliased to it). Otherwise an `EDITOR` you exported is kept, else `nano`. `VISUAL` always equals `EDITOR` |
| `MANPAGER` | `/usr/libexec/bat/man-pager`, from `bat`: it strips the page's overstrike and hands it to `bat -l man`. It is a script rather than a pipeline because `mandoc`'s `man` splits the variable on spaces and runs it without a shell |
| `BAT_THEME` | `ansi` |
| `FZF_DEFAULT_OPTS`, `FZF_DEFAULT_COMMAND` | fzf's layout and a fixed phosphor-green colour set, and `fd --type f --hidden --follow --exclude .git` as the file source. This assignment replaces the accent colours `30-kdos-colors.sh` set at login |
| `GPG_TTY` | This shell's terminal, where the curses `pinentry` asks for a passphrase |
| `GNUTERM` | `sixelgd`, only in `kdos-term` and `foot`, which display sixel pictures. Elsewhere gnuplot's terminal is left unset, and `set term dumb` draws in characters |
| `lfcd` | A function from `lf`'s `lfcd.sh`: runs `lf` and leaves the shell in the directory `lf` quit in |
| `j` | `zoxide`'s jump command |
| `atuin init bash` | `Ctrl+R` searches atuin's history database, and every command is recorded there. The Up arrow stays readline's. atuin brings its own `bash-preexec`, which removes `ignorespace` from `HISTCONTROL`, so a command typed after a space is recorded too |

System-wide files that packages install for the tools above:

| Path | From | Does |
|---|---|---|
| `/etc/gitconfig` | `git-lfs` | Registers the `lfs` filter, so cloning an LFS repository checks out content rather than pointer files, with no `git lfs install` |
| `/etc/nanorc` | `nano` | Includes every syntax definition under `/usr/share/nano` |
| `/usr/lib/NetworkManager/conf.d/10-kdos-dns.conf` | `networkmanager` | `dns=dnsmasq`: a local caching `dnsmasq` on `127.0.0.1`, with split DNS for VPNs. A file of the same name in `/etc/NetworkManager/conf.d` overrides it |
| `/etc/resolvconf.conf` | `openresolv` | `resolvconf`'s own settings. It is the one writer of `/etc/resolv.conf`, for NetworkManager, `dhcpcd` and `wg-quick` alike |
| `/etc/sudoers.d/00-sudo` | `sudo` | Members of `wheel` may run anything. `secure_path` is `/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin`, which is what lets `sudo kdos` find `kdos` |
| `/etc/zsh/zprofile`, `/etc/zsh/zshrc` | `zsh` | A login zsh reads `/etc/profile`; every interactive one runs `atuin init zsh` |

### Containers

`/etc/containers` belongs to one package, `containers-common`, which `podman`, `buildah` and
`skopeo` all depend on, because all three read all of it.

| Path | Sets |
|---|---|
| `/etc/containers/containers.conf` | `cgroupfs` as the cgroup manager (there is no systemd to delegate one); the file event logger; `crun` as the runtime; `netavark` with the `nftables` firewall driver; `pasta` for a rootless container's network |
| `/etc/containers/storage.conf` | The overlay driver, mounted through `fuse-overlayfs`; root's store under `/var/lib/containers/storage` |
| `/etc/containers/registries.conf` | `docker.io` as the one registry a short image name is looked up in |
| `/etc/containers/policy.json` | Upstream's default: any image is accepted and no signature is checked. Without this file every pull refuses to start |

A pod's infra container and `--init` use `catatonit`, a static init at `/usr/lib/podman/catatonit`;
a box profile's `init = yes` uses the same binary.

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
| `~/.config/khal/config` | The calendar `khal` | Active, not commented out: khal with no `[calendars]` section refuses to start, so it points at an empty store. It uses `type = discover` over a glob, because `type = calendar` *creates* the path it is given. The `[locale]` dates are ISO, and `kdos-cal` depends on that |
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

## Generated files you should not edit

`kdos theme` rewrites all of these whenever the accent or style changes, and seeds them into
`/etc/skel` when the image is built. Your edits are lost at the next change.

| Path | Read by |
|---|---|
| `~/.themes/KDOS-<accent>/` | GTK applications in boxes. The accent is in the name because GTK reloads its styles only when the theme name changes |
| `~/.themes/KDOS` | A symlink to the above, for anything written against the fixed name |
| `~/.icons/KDOS/`, `~/.icons/KDOS-cursors/` | Every toolkit, host and box |
| `~/.config/gtk-3.0/settings.ini`, `~/.config/gtk-4.0/settings.ini` | The theme name, where a toolkit cannot reach the settings portal |
| `~/.config/gtk-4.0/gtk.css` | libadwaita, which ignores GTK themes. There is no GTK 3 copy: GTK 3 reads that file once at startup, so colours pinned there would stop every GTK 3 application following an accent change |
| `~/.config/kdeglobals` | Qt and KDE applications. Merged, so your own settings in it survive |
| `~/.local/share/color-schemes/KDOS.colors` | KDE applications' colour scheme |
| `~/.config/foot/themes/kdos` | The terminal `foot` |
| `~/.config/btop/themes/kdos.theme` | `btop` |
| `~/.config/tmux/themes/kdos.conf` | `tmux` |
| `~/.config/kdos/term-colors.conf`, `~/.config/kdos/ls-colors`, `~/.config/kdos/fzf-colors` | This desktop's terminals, `ls`, and fzf |
| `~/.config/bat/themes/kdos.tmTheme`, `~/.config/micro/colorschemes/kdos.micro`, `~/.config/nvim/colors/kdos.vim`, `~/.config/helix/themes/kdos.toml` | The pager and editors |
| `~/.config/git/kdos-delta`, `~/.config/newsboat/kdos-colors`, `~/.config/aerc/stylesets/kdos`, `~/.config/yazi/theme.toml` | delta, newsboat, aerc and yazi |
| `~/.local/share/mc/skins/kdos.ini`, and the `skin` key in `~/.config/mc/ini` | Midnight Commander |
| `~/.config/kdos-comp/themerc-override` | The compositor's window frames |
| The palette block in `~/.config/starship.toml` | The shell prompt, between its markers. The rest of the file is yours |
| `~/.cache/kdos/wallpaper.png` | The compositor, in preference to `comp.conf`'s `wallpaper` path |
| `~/.cache/kdos/theme` | Everything: one word, the accent's name |

`kdos theme --audit` reports any of these that differ from what this machine's palette produces. See
[`kdos theme`](../04-programs/kdos-command.md#kdos-theme) and [Theming](../02-user-guide/theming.md).

## See also

- [Administration](../02-user-guide/administration.md) — using these files in the jobs they belong to
- [Theming](../02-user-guide/theming.md) — the generated files and what regenerates them
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor keys in context
- [kdos-appbox](../04-programs/kdos-appbox.md) — box profiles in full
- [The `kdos` command](../04-programs/kdos-command.md) — `kdos theme`, `kdos toggle`, `kdos update`, `kdos sandbox`
- [Filesystem and IPC](filesystem-and-ipc.md) — the paths and sockets these files name
