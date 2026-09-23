# Configuration

Every configuration file a KDOS user or administrator may edit, every key in it, and when a change
takes effect. There is no configuration abstraction layer here: each file below is read directly by
the program that acts on it.

## Where configuration lives

| Tier | Path | Belongs to |
|---|---|---|
| Machine | `/etc/kdos/`, `/etc/` | The administrator |
| Per user | `~/.config/kdos/`, `~/.config/kdos-comp/` | You |
| Per user, generated | `~/.themes/`, `~/.icons/`, `~/.config/gtk-*`, `~/.config/foot/themes/` | `kdos theme` — do not edit |
| State, not configuration | `~/.local/state/kdos/`, `~/.cache/kdos/`, `/var/lib/kdos/` | Programs |

New accounts are populated from `/etc/skel`, so editing a file there changes the defaults for
accounts created afterwards.

## When a change takes effect

Every key table below carries one of these four words.

| | |
|---|---|
| immediate | On signal. `kdos theme` already sends one |
| next start | When that program next runs |
| next login | The setting is a supervised program's command line |
| boot | Read once during startup |

## File format

Every KDOS configuration file is `key = value`, one per line, with `#` starting a comment. The
files are parsed, never sourced: there is no shell, no command substitution and no variable
expansion, apart from the two path spellings noted under `comp.conf`.

A key a parser does not recognise is reported by name rather than ignored. A typo that produces
silence is indistinguishable from a setting that does nothing, so every parser here says which it
was.

---

## The desktop

### `~/.config/kdos/comp.conf`

The compositor's KDOS keys, and only these. Bindings, mouse behaviour, workspaces and window rules
belong to `rc.xml`; a line of that kind here is reported by name and ignored.

Every key ships commented out at its default.

| Key | Default | Range | Applies | Means |
|---|---|---|---|---|
| `wallpaper` | the shipped image | path or `none` | immediate | The desktop background |
| `crt` | `55` | 0–100 | immediate | Phosphor pass strength, per cent. `0` is off |
| `crt_scanlines` | `0` | 0–100 | immediate | Scanline depth |
| `crt_curve` | `0` | 0–100 | immediate | Barrel distortion |
| `crt_fullscreen` | `on` | boolean | immediate | Whether the pass runs over a fullscreen window |
| `idle_dim` | `300` | 0–86400 | immediate | Seconds of inactivity before dimming; `0` never |
| `idle_lock` | `600` | 0–86400 | immediate | Seconds before locking; `0` never |
| `idle_off` | `900` | 0–86400 | immediate | Seconds before powering outputs off; `0` never |
| `lid_close` | `suspend` | `suspend`, `lock`, `off` | immediate | What closing the lid does |
| `icons` | `yes` | boolean | immediate | Whether chrome draws pictures at all |
| `panel_opacity` | `80` | 20–100 | immediate | Panel opacity, per cent |
| `panel_margin` | `0` | 0–64 | immediate | Panel margin in pixels |
| `window_memory` | `yes` | boolean | immediate | Whether an application opens where its window last was, per `app_id`, from `~/.local/state/kdos/winpos` |
| `panel` | `bottom` | `bottom`, `top`, `off` | next login | Which edge the panel sits on |
| `panel_cells` | `2` | 1–4 | next login | Panel height in cells |
| `panel_font` | `Terminus:pixelsize=20` | fontconfig pattern | next login | The bar's own font; empty follows `chrome_font` |
| `panel_autohide` | `no` | boolean | next login | |
| `desktop_icons` | `yes` | boolean | next login | Whether desktop icons run |
| `slit` | `no` | boolean | next login | The dockapp column |
| `clipboard` | `yes` | boolean | next login | The clipboard history daemon |
| `chrome_font` | `Terminus:pixelsize=32` | fontconfig pattern | next login | The font every KDOS surface draws with |
| `clock_format` | `%H:%M` | `strftime` format | next login | |

A value outside a key's range is clamped rather than refused, except that a value which does not
parse as a number at all is reported and the default stands. `panel_opacity` is floored at 20
rather than 0: a bar at zero opacity is not a see-through bar, it is a bar whose every control is
invisible with the pointer still hitting it, and the only way back is editing this file from a tty.

A `wallpaper` value may begin `~/` or `$HOME/`; both are expanded. No other value is.

The three idle timers default to zero in a virtual machine unless any `idle_*` key is set. A
blanked screen over a remote display is indistinguishable from a crashed compositor, and only a
line that parses counts as an opinion — `idle_dim = 5m` is refused, and does not re-arm the timers
the virtual-machine gate turned off.

A startup-only key that changed is reported by name on reload, and the running value is kept. A
configuration structure that disagreed with the running chrome is how a later reader concludes the
setting works.

The two font keys have three writers besides your editor:

- `kdos-style`'s Font page writes the family it was given into both keys, keeping each key's own
  size and copying every other line of the file — comments, blank lines, keys it knows nothing
  about — byte for byte.
- `kdos-settings` writes either pattern whole: `chrome_font` on Appearance, `panel_font` on Panel.
- `kdos theme style` writes `chrome_font` when a style file names it.

Whichever writes it, the effect is *next login*. Every surface is its own process and reads
`comp.conf` once as it starts, so a face chosen in the picker is live in that window immediately
and on any other surface the next time that surface starts.

### `~/.config/kdos/panel.conf`

Read by the panel and re-read on the same signal a theme change sends, so every key here applies
immediately.

| Key | Default | Means |
|---|---|---|
| `right` | `pager tray more media privacy mpris clipboard cpu stutter update restart net volume battery notify clock` | The notification-area widgets, in order |
| `overflow` | `stutter restart clipboard` | Which of them live behind the chevron |
| `meters` | `cpu ram net` | Which meters, in order of importance — a narrow bar drops them from the right |
| `task_labels` | `no` | `auto`, `yes` or `no`: the ladder, always, or never |
| `tray_hide` | `fcitx fcitx5 org.fcitx.fcitx5` | Tray item identifiers not drawn on the bar; they go in the chevron's popup |
| `start_label` | `yes` | Whether the Start button carries its word |

The sixteen widget names are `pager`, `tray`, `more`, `media`, `privacy`, `mpris`, `clipboard`,
`cpu`, `stutter`, `update`, `restart`, `net`, `volume`, `battery`, `notify` and `clock`. The six
meter names are `cpu`, `ram`, `disk`, `net`, `diskio` and `temp`.

`task_labels` defaults to `no` because the bar is a dock: a two-cell bar gives a square button with
a picture in it, and the name is what the tooltip carries. `auto` is the adaptive ladder — full
labels, then squeezed ones, then icon mode.

`tray_hide` is the one key here whose default is not empty. fcitx5 publishes `ItemIsMenu`, so its
Activate means *show my menu*; the three identifiers it may register under start behind the chevron,
where the popup can at least say what the item is. Writing `tray_hide =` with nothing after it puts
every tray item back on the bar.

`temp` is the hottest sensor on the machine, on a fixed 0–100 °C band. It is read every fourth
sample rather than every one: the read walks `/sys/class/hwmon`, and a die's temperature does not
move meaningfully in half a second. Where nothing answers, the meter holds rather than drawing
zero — a machine with no sensor is not one running cold.

The `update` widget reads a file and never computes. `kdos update check` walks the ports tree
against the package database, which is hundreds of file reads and nothing the panel may do on a
tick. The count is the `behind` field of `/var/lib/kdos/update.json`, which the system timer writes
with `kdos update check --json`; `$XDG_STATE_HOME/kdos/update.json` is the fallback, for somebody
who ran the check themselves on a machine whose timer is off. What the ports tree pins against what
is installed is a fact about the machine, so one walk serves every session. An absent file is zero
and the badge is not drawn at all, which is the honest picture of a machine nobody has checked. It
is re-read at most once a minute.

The loader restores every default before parsing, because it runs again on reload. A reload that
only ever *added* would leave a widget hidden after the line hiding it was deleted.

### `~/.config/kdos/launcher.conf`

| Key | Default | Means |
|---|---|---|
| `files` | `no` | Intended to add this account's file index to what `Super+D` searches |

The shipped file has the key commented out, and `kdos-settings` writes it on its Desktop page. No
surface reads it. What each search surface does today is fixed by which name it was invoked under:

| Surface | Searches |
|---|---|
| `kdos-launcher` (`Super+D`) | Applications only |
| `kdos-palette` | Applications, windows, routes, settings, chords, and files — always |
| `kdos-find` | Files by name, and file contents |

The palette's file source starts at three typed characters and runs `fd` over `$HOME` with
`--hidden`, `--fixed-strings` and a 200-result cap, one child at a time. It does not read
`~/.cache/kdos/plocate.db`; that index is `kdos-updatedb`'s, rebuilt nightly from `$HOME`, and
`/etc/profile.d/40-plocate.sh` points the `locate` command at it through `LOCATE_PATH`.

### `~/.config/kdos/favorites`

One desktop-entry identifier per line — the file name under the applications directory, without the
extension. Written by the panel and the menus when you pin, unpin or reorder.

Two surfaces read this one file: the panel's quick-launch row and the Start menu's pinned column.
Two lists of favourites would be two things to keep in agreement, and one always loses.

It ships populated, with seven rows. An empty list makes both surfaces look broken on a freshly
booted machine; delete every line if you want one. Each surface draws the first eight identifiers
and stops, so a ninth line is parsed and never shown. An identifier with no matching entry is
skipped in silence, so an application this image's catalogue does not carry leaves no launcher that
opens nothing.

A line may carry a two-letter code:

```
mc code=FM
```

The code is drawn right-aligned on the row in both the Start menu and the palette, and typing both
letters with nothing else in the search field opens that row — no arrows, no `Enter`, and no
waiting to see whether the search narrowed to one. It is DESQview's Open Window shorthand, and it
is a property of the pinned row rather than of the program, because the codes are yours and this
file is where you say so.

A letter is part of a code only while a code could still match it. With no coded line in this file,
or a first letter no code begins with, the letters are a search like any other — a rule that ate
two letters whatever the file said would be a search box that lost the first two characters of
every query. Anything else after the identifier is ignored rather than refused: this file is edited
by hand, and a line a later version understands must not stop this one launching it.

The terminal is the session's, not the row's. A line naming `foot` or `kdos-term` resolves to
whichever emulator the session runs — the same answer every chord, menu row and `Terminal=true`
entry gets. Only those two identifiers are resolved; every other line is the entry it names and is
launched as written, because a favourites line is a desktop-entry identifier and not a command.

### `~/.config/kdos/slit.conf`

One gadget per line, three fields:

```
<interval> <width> <command and its arguments>
```

`<interval>` is seconds between runs, `<width>` the cells the gadget is given, and the rest is the
command, split the way a desktop entry is, so no shell is involved. The first line of the command's
output is what is drawn, clipped to the width.

```
60 8 date +%H:%M
10 8 kdos-res --line cpu
```

Up to 32 gadgets. Blank lines and `#` comments are skipped, an interval below one second is raised
to one, and a width is clamped to 1–64.

The file ships absent, and its absence is why `kdos-slit` exits: a column of empty marks says less
than no column. The slit's own width is the widest gadget's, and it docks with an exclusive zone,
so a maximised window stops short of it rather than covering it.

### `~/.config/kdos/session-restore`

Ships absent. Its existence is the setting, and the contents are ignored. With it, the boxed
applications that were running when the session ended are started again at the next login, two
seconds apart in the background.

The list is `$XDG_STATE_HOME/kdos/session`, written by `kdos-session-save` from the menu's Log Out,
Restart and Shut Down rows — before the confirmation dialog, because after the answer there is no
session left to ask. Each line is `app <name>` for a boxed application or `native <app_id>` for a
host toplevel, and only the `app` lines are relaunched: a host program costs no container start,
and where a window comes *back* is `comp.conf`'s `window_memory` rather than this list.

### `~/.config/kdos/backup.conf`

What `kdos-backup` copies, and where to. `include` and `exclude` may repeat, up to eight of each;
every other key is last-one-wins. The paths become an argument vector, so a directory with a space
in its name is one argument rather than two.

| Key | Default | Means |
|---|---|---|
| `repo` | — | The restic repository |
| `include` | — | A directory to back up. Repeatable, eight maximum |
| `exclude` | — | A directory to leave out. Repeatable, eight maximum |

Ships with every line commented out, because a backup nobody asked for is a surprise and the first
run over a home directory can be hours.

The repository is not made for you: `restic -r <repo> init` asks for a password twice and writes it
down nowhere. Put that password in `~/.config/kdos/backup.pass` at mode 0600, with nothing else in
the file. `kdos-backup` checks the mode before it uses the file and refuses to run while it is group
or world readable — a backup that succeeded is not the moment to learn the key was readable.

### `/etc/kdos/timers.d/` and `~/.config/kdos/timers.d/`

Periodic jobs, one per file, one job per line:

```
NAME  TIMESPEC...  --  COMMAND...
```

`TIMESPEC` is `snooze`'s own options — `-H 9 -M 0` for nine in the morning, `-d /2` for every second
day. `snooze(1)` is the whole reference; nothing is invented here.

The command is an argument vector: no pipe, no redirection, no `&&`. That is what lets this table be
parsed instead of sourced, and it is why a line here cannot run something you did not write. It also
means a command here cannot take an argument with a space in it — the row is split into words and
never quoted, so `-- kdos notify "Good morning"` reaches the command as `"Good` and `morning"`. A
program that needs a sentence reads it from a file of its own, which is why `kdos remind` writes its
text as its timer file's first comment line.

`-s SLACK` is not enough on its own for a missed run. A `snooze` that has just started begins
looking one second from now, so a slot already past is the same slot next time round whatever the
slack says. `-t FILE` starts the search from that file's modification time, and the slack then
covers the gap.

The two tiers differ in when a job can fire at all:

| Tier | Started by | Fires |
|---|---|---|
| `/etc/kdos/timers.d/` | The system supervisor | Every occurrence, whether or not anybody is logged in |
| `~/.config/kdos/timers.d/` | The session, at login, reaped with it | Every occurrence while that login lasts |

`snooze` waits for its slot, runs the command once and exits, so something has to start it again.
The system table's supervisor does that; the user table gets a loop of its own per row, whose pid
goes in `$XDG_RUNTIME_DIR/kdos/timers.pid` so the next login stops what the last one left. A loop
whose command fails instantly is floored at one second, which is far below any schedule and far
above a spin.

A row is checked before its loop starts: the command's first word must be on `PATH`, and `snooze
-n` must accept the timespec. Both tiers reject a row on either count rather than starting a loop
that can never fire.

The tier decides who is there to run the job. A job that must run on a machine nobody has logged
into belongs in the system tier; a job that writes into `$HOME` belongs in the user tier, where it
dies with the login that started it.

Two timers ship enabled: `/etc/kdos/timers.d/10-update-check.timer` at 04:17 and
`~/.config/kdos/timers.d/20-updatedb.timer` at 03:05, with `10-backup.timer` beside the latter
commented out. The three times are deliberately different: two jobs at the same second on a laptop
that has just woken are two jobs competing for one disk.

### `~/.config/kdos/screensaver.txt`

A UTF-8 grid of characters, one line per row, read by the `art` and `bounce` effects. The other six
effects need no file at all. SGR colour in it is stripped, because a surface paints slots and the
effect picks one.

Ships absent; `/usr/share/kdos/screensaver.txt` is what is drawn without it. A grid wider or taller
than the screen is pinned rather than bounced, and trailing blank lines are dropped, or the art
would bounce off an edge nobody can see.

This is not `logo.txt`. That file is the login banner's, generated from the mascot, and somebody
replacing their screensaver must not be replacing the picture the machine boots with.

### `~/.config/kdos/a11y`

Ships absent. Its existence — an empty file is enough — opts boxed applications into the
accessibility stack. `KDOS_A11Y=1` does the same for one launch.

The host runs no accessibility registry, so the default avoids a startup probe that always times
out. A screen reader running *inside* a box reaches that box's own registry, which is what this
file enables. Nothing reads the desktop itself; see
[Accessibility](../02-user-guide/accessibility.md).

### `~/.local/state/kdos/toggles/`

The switches a desktop needs at hand. A file's presence means on. There is no format and nothing to
parse, which is the smallest thing a shell script, a chord and a surface can all read without
agreeing on a syntax first.

| Toggle | Means | Read by |
|---|---|---|
| `stay-awake` | Never save, lock or blank on idle | `kdos-comp`'s idle policy |
| `night-light` | Warm the palette | Every surface, on the retint signal |
| `dnd` | Hold notifications back | `kdos-notifyd` |

`kdos toggle` lists them, `kdos toggle <name>` flips one, and `kdos toggle <name> on|off` sets it.

These are state rather than configuration keys because a configuration file is read when the
session starts, so a runtime writer would make half its answers come from before an edit and half
from after. A toggle is set by another process — a chord, a menu row, a script before a long build
— so its reader must not hold a copy. `stay-awake` and `dnd` are stat'd on a tick their reader
already runs; `night-light` is read on the retint signal, which `kdos toggle` sends after writing
the file, because a palette is applied once and not consulted per frame.

Night light is a transform over the eight slots rather than a scheme of its own. Eight accents
times a warm copy would be sixteen palettes to keep in step. `ktui_theme_night()` warms whatever
scheme is loaded — green to 93%, blue to 77%, red untouched, so the accent still reads as itself —
and turning it off returns to the table rather than undoing the arithmetic, which eight bits cannot
do.

`kb_toggle_on()` and `kb_toggle_set()` are the one reader and the one writer in the tree; a program
that spells the path itself is a program looking where nothing wrote. The notification centre's Do
Not Disturb button writes this file through the daemon rather than keeping a flag of its own: a
second flag OR'd with this one is a state that button cannot clear, so it would silence the toasts
and say it had not. A held notification still reaches the history and the badge, so Do Not Disturb
hides a toast rather than losing it, and an urgent one is shown anyway.

`stay-awake` is consulted before all three idle steps, not the first only. Somebody who suppressed
the saver did not ask to be locked either, and a machine that locked during the presentation they
turned the saver off for is the failure the toggle exists to prevent.

### `/etc/kdos/menu.conf`, merged under `~/.config/kdos/menu.conf`

A route is a name a script can hold. One `route = argv` per line.

A chord opens a surface and a person clicks a row; neither is something a shell script, a
documentation page or another program can refer to. `kdos menu summon setup.network` is, and it
keeps resolving when the chord is rebound or the row moves.

The name is `verb.noun`, and the verb is the shape of what is being done rather than the program
that does it: somebody looking for the wifi is looking to *set something up*, and does not know
which of eleven surfaces owns it.

The value is an argument vector, split on spaces and run without a shell. There is no quoting and
there will not be: a route that needed a shell would be a route a menu file could run anything
with, and this file merges a copy you own over the system's.

The system file is read first and yours second. A route named in both is yours; one named only in
yours is added. There is no delete, which is the point — a name a script may hold has to keep
resolving.

`kdos-start` searches the routes beside its fixed rows, so the names are not a second vocabulary:
`network` finds the row and `setup.network` finds the same thing. `testing/preflight.sh` fails on a
route whose first word is a command the image does not carry.

A key beginning `@` is a setting about the menu rather than a route. Its value is read by whichever
surface asks for it and is never run. `@` cannot begin a route name, so that is the whole of the
distinction; a setting that fell through to the route table would be a launchable row running the
first word of its own value, and `preflight.sh` skips `@` lines for exactly that reason.

| Setting | Default | What it does |
|---|---|---|
| `@toplevel` | `Network Sound Displays Terminal` | Which system rows `kdos-start` keeps outside the fold when it is too narrow for three columns |

`@toplevel` takes labels as the menu draws them, separated by spaces or commas, whole entries and
case-insensitive. `kdos-settings` writes it on its Panel page into *your* copy; every other line —
every route — is copied through byte for byte, which is what makes writing one `@` key into a route
table safe.

Below a hundred columns the Start menu's system group folds behind one `Settings ▸` row, and the
labels named here stay listed beside it. A label that names no row promotes nothing and reports
nothing — a preference file is not a wiring diagram — so `testing/selftest.sh` is what fails on the
typo.

---

## Programs

### `~/.config/kdos/res.conf`

The resource monitor. Sort keys use the page identifiers from its own registry, so there is one
spelling. An unknown key is reported by name.

| Key | Default | Means |
|---|---|---|
| `interval` | `1000` | Sampling interval in milliseconds, clamped to 200–60000: a monitor sampling faster than 200 ms is mostly measuring itself |
| `units` | `1024` | `1024` gives KiB/MiB/GiB; `1000` gives kB/MB/GB |
| `temperature` | `c` | `c` or `f`, everywhere a sensor is shown |
| `cpu_percent` | `core` | `core` — eight busy threads read 800%, which is `top`'s convention — or `machine`, where the same load reads 100% |
| `memory` | `rss` | `rss` counts a shared page against every process holding it; `pss` divides it between them, which is the number that adds up |
| `kernel_threads` | `no` | Show kernel threads in the process table. The footer says how many are hidden either way |
| `virtual_drives` | `no` | Show loop, zram and device-mapper devices on the Drives page |
| `virtual_net` | `no` | Show loopback, bridges and container interfaces on the Network page |
| `icons` | `yes` | Draw pictures beside the rows; `no` is the glyph tier |
| `sort` | `cpu` | Which column each page sorts on, by that page's own identifier. A name a page has no column for leaves that page on its own default |
| `columns` | all | Which columns to show |

`kdos-settings` writes all of these but `columns`, on its Hardware page, and signals `kdos-res`
exactly. `kdos-resctl` is a longer name with this one as its prefix and is setuid, so a substring
match would kill a privileged helper that handles no signals.

### `~/.config/kdos/term.conf`

The terminal. Every key has a working default and the file need not exist. An unknown key is
reported by name.

| Key | Default | Means |
|---|---|---|
| `shell` | `$SHELL`, then `/bin/sh` | What an argument-less `kdos-term` runs. Split as a desktop entry's `Exec` is; there is no shell |
| `font` | the toolkit's | A fontconfig name, and the size a window opens at. `Ctrl+=` and `Ctrl+-` step it for that window alone, `Ctrl+0` comes back here |
| `columns` | `80` | Columns asked for on the first configure |
| `rows` | `24` | Rows asked for on the first configure |
| `scrollback` | `2000` | Lines kept above the screen |
| `images` | `yes` | Decode pictures. `no` turns the three image protocols off in the parser, not merely in the drawing |
| `image_max` | `1024` | The cap on one image payload, in kilobytes |
| `image_cells` | `200` | The widest and tallest a picture may be, in cells |
| `paste_guard` | `yes` | Ask before an unbracketed paste carrying a newline |
| `opacity` | `100` | How much of the window's own background it keeps, per cent, 20–100 |

`paste_guard` matters because of what an unbracketed paste is. With bracketed paste on, the child
sees the text as text and decides for itself; with it off, the bytes go straight to the pty and a
newline *executes* — at a shell, at an `ssh` password prompt, inside `read`. A second attempt
within five seconds means it.

Below 100, `opacity` lets the desktop show through the cells the terminal has not drawn on; the ink
is never mixed. It applies only where a compositor is underneath, since on a bare terminal there is
nothing behind the window at all.

`kdos-settings` writes all of it, on its Desktop page, and signals `kdos-term`, so a change reaches
every terminal already open. A window that has stepped its own font or its own transparency keeps
what it stepped: this file is where a window *starts*. The file is re-read on `SIGHUP`, which is
what `kdos theme` sends.

### `~/.config/kdos/boxes/<name>.conf`

One file per box. Every key maps onto a container-engine flag or onto something KDOS enforces
itself, and `kdos-box profile` names which.

| Key | Default | Applies | Means |
|---|---|---|---|
| `base` | — | create time | `pack:<id>`, `box:<name>`, or `image:<ref>` |
| `image` | — | create time | The reference, for a registry base |
| `persistence` | `persistent` | never | `persistent`, `ephemeral` or `frozen`. Recorded only: no launch reads it, and a box's writes land where its runtime puts them |
| `network` | shared | create time | `host`, `private`, or `none` — private *plus* no interface at all |
| `ipc` | shared | create time | IPC namespace |
| `processes` | shared | create time | PID namespace |
| `devices` | shared | create time | Whether `/dev` and the runtime directory are shared |
| `home` | shared | create time | Whether the box gets a private home directory |
| `init` | `no` | create time | Whether the engine runs an init inside the container |
| `wayland` | `yes` | | Whether the compositor socket is bound in |
| `audio` | `yes` | | Rides on `devices` |
| `gpu` | `yes` | create time | The card's device nodes |
| `render` | `auto` | next launch | Which graphics this box's applications get |
| `memory` | — | immediate | Budget, enforced by the memory daemon rather than the engine |
| `cpus` | — | | CPU quota, as the engine's own `--cpus` value |
| `pids` | — | | Process-count limit |
| `autostop` | `0` | | Idle timeout for the collector: `90s`, `30m`, `2h`, or bare seconds. `0` disables |
| `accent` | — | on reload | The box's colour, which draws a title-bar chip |
| `grant` | — | on reload | Compositor globals the sandbox allowlist otherwise refuses |
| `export` | — | | `auto` gives its applications host launchers |
| `display` | — | next launch | Carried and not interpreted |

A namespace key applies at create time and cannot be re-flagged on a live container, so changing
one says to recreate the box rather than silently doing nothing.

`gpu` subtracts nothing from a shared `/dev`. With `devices = private` it is the `--volume
/dev/dri` that binds the card back. `gpu` and `audio` are not independently enforceable — there is
no flag that grants a box a speaker and denies it a camera — and the profile printer says so rather
than pretending.

`render` resolves by *opening* a `/dev/dri/renderD*` node: hardware where one opens, software where
none does. `gpu` asks for the same thing and `software` refuses the card whatever is plugged in.
The resolved answer reaches the guest's own Mesa and nothing else — `LIBGL_ALWAYS_SOFTWARE=1` in a
software box's launch environment, and nothing at all for a hardware one, because Mesa asks the
machine the same question by itself. It is advisory: an application may unset the variable. For a
box that can see no node at all (`devices = private` with `gpu = no`), the resolve refuses the card
without opening anything, because this process's `/dev` is not that box's. It is its own key rather
than part of `gpu`, which is about device nodes rather than about who draws.

`display` is carried and not interpreted: it means nothing to a container flag and nothing on this
desktop reads it. It is kept so that a profile rewrite does not silently drop somebody else's key.

`base = image:` is an online operation, and announces itself before doing anything.

Edit these with `kdos-box profile`, or through the Settings program, which runs that command rather
than writing the file itself.

---

## The compositor's own files

### `~/.config/kdos-comp/rc.xml`

The compositor's configuration, in the upstream format, so upstream's documentation applies
verbatim: bindings, mouse behaviour, window rules, workspaces, theme keys and fonts.

`<default />` must be the first child of both `<keyboard>` and `<mouse>`. The compositor loads its
built-in bindings only when your file defines none of that kind, so a file that binds one key throws
every default away — including click-to-focus, the title-bar drag and the window buttons. Put your
own bindings *after* it; the later of a duplicate pair wins.

The title-bar font must name a scalable face. Naming the bitmap console font resolves, and then
falls back silently to a generic sans.

Applies at next login, or on reload for the parts the compositor re-reads.

### `~/.config/kdos-comp/menu.xml`

The root and client menus. It deliberately lists no applications: those come from a program that
reads the same desktop entries everything else does, because a menu built at compositor startup
would be the one that went stale.

### `~/.config/kdos-comp/themerc-override`

Generated by `kdos theme`. Do not edit. A style file's dotted keys are appended after the generated
block and win.

---

## The machine

### `/etc/kdos/packd.conf`

| Key | Default | Applies | Means |
|---|---|---|---|
| `retain` | `1` | next start | How many superseded versions of a pack the store keeps |

Retention is what makes rollback possible. A store keeping none could not roll anything back, and
one keeping every version would fill a disk. `0` is an honest off: rollback then answers that no
earlier version is kept, rather than failing at a rename.

The sweep runs after an install and at no other time. A sweep on a timer would be a background job
deleting somebody's rollback while they were deciding whether to use it.

### `/etc/kdos/zram.conf`

| Key | Default | Range | Applies | Means |
|---|---|---|---|---|
| `size` | `50` | 1–90 | boot | A percentage of RAM |
| `algorithm` | `zstd` | a compressor the kernel carries | boot | Compression algorithm |

`size` is how much swap the device may claim to hold, never how much memory it will occupy — the
compressed pages live in that same memory. A value outside 1–90 is reported and 50 is used. An
algorithm the kernel does not carry is reported and the kernel's own default stands.

### `/etc/kdos/mountd.conf`

Not shipped; create it to change the defaults.

| Key | Default | Means |
|---|---|---|
| `exec` | `no` | Whether removable media are mounted executable |
| `format` | `no` | Whether `kdos-mountd` will write a filesystem over a device at all |

Everything removable is mounted without setuid and without device nodes regardless. `exec = yes` is
how somebody says they meant it: a setuid binary on somebody else's stick is a local root hole.

`format = yes` is the same argument. Writing a filesystem is not undoable, and a desktop that
offers it by default on every machine it is installed on is one where a mis-click costs somebody
their photographs. The daemon still refuses the boot medium and still demands the device's own
kernel name typed, whatever this says. The key decides whether the verb exists, not whether it is
careful.

### `/etc/kdos/keys/`

The trusted key directory, and the directory *is* the policy. Every public key here is one this
machine accepts a signature from. Adding a key is copying a file in; removing trust is deleting one.
There is no revocation list and no online check.

| Directory | Trusted for |
|---|---|
| `/etc/kdos/keys/` | Host package indexes and package signatures |
| `/etc/kdos/keys/packs/` | The application pack index |

The loader does not descend into subdirectories, which is what keeps those two genuinely separate
policies. A pack key in the upper directory would silently become a trusted publisher of host
packages.

The pack directory ships one key, `kdos-packs.pub`, which attests that the application images beside
it came from one bake. Replace it and re-sign to make the index attest something about you. `README`
in the upper directory states the policy on the machine itself.

The host key directory ships empty: packages built here are not signed and need no signature, since
a port's integrity is the checksum in its recipe.

### `/etc/kdos/login.conf`

Who tty1 logs in, and nothing else. One key, because there is one decision on this path.

| Key | Default | Applies | Means |
|---|---|---|---|
| `autologin` | `kdos` | boot | The account tty1 logs in without asking |

Absent or commented is a password prompt rather than a default account: `kdos-login` reads the key
and asks when it finds none. `autologin =` with nothing after it would be an account called `""`,
which `agetty` would be handed — so turning it off comments the line rather than emptying it. That
is what `kdos-power autologin off` writes, and what the installer writes for an unattended
`autologin = no`.

This is the only place the desktop's account is named. `/etc/inittab` carries no account at all, so
a rename that misses this key leaves the machine reachable only from terminal two.

### `/etc/nftables.conf`

The firewall. Applies at boot, before the network starts, after a syntax check — so an unloadable
ruleset leaves the previous state standing rather than half-applying a flush.

The shipped policy drops input and forwarding, accepts established traffic and loopback, answers the
necessary ICMP and ICMPv6 types, and opens multicast DNS and DHCPv6. Anything that should be
reachable needs a rule; the file carries commented examples.

### `/etc/fstab`

Applies at boot. The shipped entries are the pseudo-filesystems, the control-group hierarchy, and
two temporary filesystems.

The temporary filesystem for scratch space must be mode 1777. Default options give a root-owned
filesystem that *hides* the correct one baked into the image, so no ordinary user can write there at
all — and every graphical application depends on it. Because a mounted filesystem ignores a mode
change on remount, the boot sequence also applies the mode explicitly.

The installer appends to this file rather than replacing it, precisely because of that entry.

### `/etc/inittab`

Applies at boot. Terminal one runs `kdos-login`, which hands the tty to `agetty` and autologins
where `login.conf` names an account. Terminal two is an ordinary login and is the recovery console.
The serial line gives a login on demand. Both terminals are wrapped by the VT-font loader. On
shutdown, `/etc/init.d/rcK` runs every enabled service script's `stop` in reverse order — all but
`25_nftables`, so the firewall stays loaded to the end — then swap is
turned off and every filesystem remounted read-only.

Renaming the desktop user must rewrite `login.conf`'s `autologin`, which names the account tty1 logs
in. A name that matches nothing leaves the machine reachable only from terminal two.

### `/etc/service.disabled/<name>`

A marker file, not a setting. Creating one stops the matching init script running at boot:

```sh
sudo touch /etc/service.disabled/cups
```

The name is the init script's, with the order prefix and the `.sh` stripped — `42_networkmanager.sh`
is `networkmanager`.

`networkmanager` carries a second effect. `30_network` starts dhcpcd only when NetworkManager is
absent or carries this marker, so turning NetworkManager off hands DHCP back to dhcpcd instead of
leaving the machine with no client.

### `/etc/keymap`

The console keymap, written by the installer, loaded on every terminal, and translated into a
graphical keyboard layout when a session starts.

---

## Speech-to-text models

`kdos-rec` greys *Transcribe* until a whisper.cpp model is on the machine. There is no configuration
key; three locations are searched in order and the first hit wins.

| Order | Location |
|---|---|
| 1 | `$KDOS_WHISPER_MODEL` — one file, named exactly |
| 2 | `$XDG_DATA_HOME/whisper.cpp/models/`, default `~/.local/share/whisper.cpp/models/` |
| 3 | `/usr/share/whisper.cpp/models/` |

`$KDOS_WHISPER_MODEL` is the whole answer when it is set. No directory is searched behind it, and a
file that is missing or fails the test below leaves transcription unavailable rather than falling
through. Somebody who named a model and got a different one has been lied to.

Inside a directory the pattern is `ggml-*.bin` and the winner is the first in sorted name order —
not the newest, because an mtime is not reproducible and a name is.

The gate is the file's magic, not its name. A candidate counts only if its first four bytes are
`lmgg`, so a half-finished download reads as *no model* rather than as a crash behind an enabled
button. The surface's header line names the model that was found, or the directory that was
searched.

Upstream's own directory name is used deliberately. The model is whisper's data, and naming where
`models/download-ggml-model.sh` writes means a model fetched by that script and one fetched here
land in the same place.

Nothing ships a model, and `kdos speech` is how one arrives. `kdos speech list` prints the catalogue
— every English-only size, the three smallest multilingual ones and the two large ones worth the
disk, smallest first — and `kdos speech get <name>` downloads it into
`$XDG_DATA_HOME/whisper.cpp/models`, which needs no privilege because a model is a per-user choice.
The name is looked up in a table and never interpolated into a URL, the download is verified against
a sha256 compiled into the tool, and the file is renamed into place only after both that and the
`lmgg` magic pass. An interrupted download is therefore never a file the gate above has to reject.
The checksum says the bytes are the bytes this tree was written against; upstream signs nothing, and
that is the [unsigned content](../03-architecture/security-model.md) rule applying here as
everywhere.

`kdos-rec`'s third button is *Get model* while there is none, and it opens a terminal on that same
command rather than downloading inside the panel. A progress bar, a cancel, a disk-full and a
network that went away all already exist in the command, and a second implementation of the four in
the process that draws the taskbar is not worth the button. The window keeps looking while there is
no model, so the button becomes *Transcribe* on its own when the file lands.

`whisper-stream` wants the same model and is not a desktop verb. It transcribes a live microphone
rather than a closed file, through SDL's audio device, which opens no window — so it runs in a
terminal with no window above it, and nothing starts it. `kdos-rec`'s *Transcribe* is `whisper-cli`
over the file it has just recorded.

## Video in a call

`baresip` writes `~/.baresip/config` on a first run and only when there is none, so the default
below is what a fresh account gets and an edited file is never overwritten.

Five module lines are uncommented there that upstream leaves commented, and they are the codec
and display modules this image builds: `opus.so`, `avcodec.so`, `vp8.so`, `vp9.so` and `sdl.so`. An
uncommented line naming a module that is not installed is a start-up error, which is why upstream's
default comments them all.

The audio line stays `alsa.so`, which reaches PipeWire through the ALSA default. `pipewire.so` is
built beside it for a config that names it, as are `sndfile.so` (call recording), `snapshot.so`
and `ctrl_dbus.so`, all left commented as upstream writes them, and the `aac.so` codec, which the
generated config does not mention at all.

The display is uncommented and the camera is not, and the asymmetry is the point: which screen a
picture goes on is a property of the build, and which camera it comes from is a choice. Turning on
`v4l2.so` is what sends yours: the generated `video_source` line already names `v4l2,/dev/video0`.
`avformat.so` is the other video source, for a stream or file named in `video_source` instead.
`x11.so` is not built, by rule. `fakevideo.so` is a null sink and `vidbridge.so` a
loopback, and both stay commented because each is something a person chooses deliberately.

## Shipped configuration for software that is not ours

`/etc/skel` also carries configuration for the third-party programs the system ships, so a new
account gets a working setup rather than each program's own defaults. These are yours to edit.

| Path | For | Notes |
|---|---|---|
| `~/.config/foot/foot.ini` | The terminal | Sets server-side decorations, so windows wear the compositor's frame rather than the terminal's own |
| `~/.config/foot/themes/kdos` | The terminal's colours | Generated |
| `~/.config/btop/btop.conf` | The system monitor | |
| `~/.config/btop/themes/kdos.theme` | Its colours | Generated |
| `~/.config/kdos/term-colors.conf` | The sixteen colours a program asks for, in this desktop's terminals | Generated |
| `~/.config/kdos/fzf-colors` | fzf's `--color` flags, sourced by `/etc/profile.d/30-kdos-colors.sh` | Generated |
| `~/.config/bat/config` | The pager | One line: `--theme="kdos"` |
| `~/.config/bat/themes/kdos.tmTheme` | bat's theme, selected by file stem | Generated |
| `~/.config/micro/settings.json` | The editor | Selects the generated colorscheme |
| `~/.config/micro/colorschemes/kdos.micro` | micro's colorscheme | Generated |
| `~/.config/helix/themes/kdos.toml` | helix's theme | Generated. helix itself is in no `packages.txt` and is not on the image; the file is for a helix installed in a box, which sees this `$HOME`. Nothing ships a `config.toml` to select it — write `theme = "kdos"` yourself |
| `~/.config/nvim/init.vim` | The editor | `termguicolors` and the generated colorscheme |
| `~/.config/nvim/colors/kdos.vim` | neovim's colorscheme | Generated |
| `~/.config/git/config` | Version control | Names delta as the pager and `[include]`s the generated colours |
| `~/.config/git/kdos-delta` | delta's colours, `[include]`d from the shipped gitconfig | Generated |
| `~/.config/newsboat/config` | The feed reader | `include`s the generated colours |
| `~/.config/newsboat/kdos-colors` | newsboat's colours, as 256 indices | Generated |
| `~/.config/aerc/stylesets/kdos` | aerc's styleset | Generated |
| `~/.config/tmux/tmux.conf` | The terminal multiplexer | |
| `~/.config/starship.toml` | The shell prompt | Only the palette block between its markers is generated |
| `~/.config/fastfetch/config.jsonc` | The system-information tool | The login banner runs it with its own logo disabled |
| `~/.config/lf/lfrc`, `~/.config/lf/preview` | The terminal file manager | |
| `~/.config/GIMP/3.0/gimprc` | The image editor | Selects the system theme, or it keeps its own |
| `~/.config/gtk-3.0/settings.ini`, `~/.config/gtk-4.0/settings.ini` | Toolkit settings | Cursor theme and size |
| `~/.config/user-dirs.dirs` | The standard user directories | Seeded from `/etc/skel`; there is no `xdg-user-dirs` here. `$HOME` is the only expansion read |
| `~/.config/kdos/places` | Extra rows on the places column, `Name = /path` one per line | Merged over the user directories; a row whose path is already listed is dropped, and one pointing at nothing is never shown. Written by *Add to Places* on the desktop |
| `~/.config/xdg-desktop-portal-wlr/config` | The screen-capture backend | Uses an output picker; the alternative captures the first output silently, which is wrong the moment a second screen is plugged in |
| `~/.config/fcitx5/profile` | The input method | One group, `Default`, holding the four engines the image carries: `keyboard-us`, `pinyin`, `anthy` and `hangul` |
| `~/.config/mimeapps.list` | This account's own handler choices | Ships with an empty `[Default Applications]` section, deliberately: this file outranks every system table, and *Open With*'s **always** tick is what writes to it |
| `~/.bashrc`, `~/.bash_profile` | The shell | `.bashrc` reads `/etc/bash.bashrc` — through `/run/host` inside a box, because `$HOME` is bind-mounted into every one. `.bash_profile` is what starts the desktop on `tty1` |

### Default handlers

Four tables decide which application opens a file type, consulted in this order:

| Path | Scope |
|---|---|
| `~/.config/<desktop>-mimeapps.list` | This desktop, this account |
| `~/.config/mimeapps.list` | Any desktop, this account |
| `/etc/xdg/kdos-mimeapps.list` | This desktop, system-wide |
| `/etc/xdg/mimeapps.list` | Any desktop, system-wide |

The generated caches are consulted last. A type belongs in exactly one of the two system-wide
tables.

### Midnight Commander

| Path | Holds |
|---|---|
| `~/.config/mc/ini` | How `mc` behaves: the KDOS skin, `F3` internal, `F4` to `$EDITOR`, no exit confirmation, the panel's directory in the window title |
| `~/.config/mc/mc.ext.ini` | What `Enter` does on a file |
| `~/.config/mc/menu` | `mc`'s `F2` user menu |

The section a key is in is part of the key. `mc` reads its behaviour flags out of
`[Midnight-Commander]` and its screen layout out of `[Layout]`, and a key under the wrong header is
silently never read. `kdos theme` merges `skin` into `ini` rather than replacing it, and generates
the skin itself into `~/.local/share/mc/skins/kdos.ini` — `mc` looks for skins under
`<data>/mc/skins`, `/etc/mc/skins` and `/usr/share/mc/skins` and nowhere else.

`mc.ext.ini` replaces the system file wholesale, because `mc` does not merge them. Only the archive
rows whose VFS helper is on this image are carried; everything else falls to the catch-all, which is
`kdos-appbox open`. The `F2` menu carries nine verbs — open with the desktop's handler, peek, edit,
find in this folder, terminal here, add to Places, share, move to trash, and git status — each
naming a program on the image, and `testing/preflight.sh` refuses one that is not.

### Shell environment

| Path | Sets |
|---|---|
| `/etc/profile.d/30-open.sh` | `$BROWSER` to `xdg-open`, which on this image is `kdos-appbox open` — so the variable and the mimeapps table are one road rather than two that drift |
| `/etc/profile.d/20-lesspipe.sh` | `LESSOPEN` to `lesspipe.sh` and `LESS=-R` |
| `/etc/profile.d/40-plocate.sh` | `LOCATE_PATH` to this account's own index |
| `/etc/profile.d/podman-docker.sh` | `DOCKER_HOST` to the rootless Podman API socket, `$XDG_RUNTIME_DIR/podman/podman.sock` (root's is `/run/podman/podman.sock`), so a Docker API client such as `lazydocker` finds `podman system service` once it is running. `/usr/bin/docker` is Podman's shim over `podman` |

None of them writes over a value you already exported. A login shell reads them, which is the
only way into a session here. The `less` filter is driven by `file -L -s -b --mime` and nothing
else, which is why `file` on this image is the one with a magic database.

### Mail, calendar and contacts

| Path | For | Notes |
|---|---|---|
| `~/.mbsyncrc` | Fetching mail | Mode 600 — it carries a password. Empty lines delimit sections, so a commented block must keep its blank lines or a `Channel` lands inside the `Store` above it. Ships with no accounts |
| `~/.msmtprc` | Sending mail | Mode 600; msmtp refuses a file carrying a `password` line that others can read, and `passwordeval` avoids the question. `/usr/sbin/sendmail` and `/usr/bin/sendmail` are links to this program. Ships with no accounts |
| `~/.config/notmuch/default/config` | The mail index | `mail_root` is relative and expands against `$HOME`, which is what makes one shipped file right for every account. It names `~/Mail`, and so do the other two |
| `~/.config/notmuch/default/hooks/` | What `notmuch new` does around the scan | `pre-new` fetches with `mbsync -a`, `post-new` tags. A non-zero `pre-new` aborts `notmuch new`, deliberately: indexing after a fetch that did not happen reports an empty inbox |
| `~/.config/khal/config` | The calendar | Live, not commented out — khal with no `[calendars]` section refuses to start, so it points at an empty store instead. `type = discover` over a glob, because `type = calendar` *creates* the path it is given. The `[locale]` dates are ISO and `kdos-cal` depends on it |
| `~/.config/vdirsyncer/config` | Syncing a calendar or address book | Live but with no pair configured: `vdirsyncer sync` then exits 0, silently, and creates nothing. `[general]` and `status_path` are the minimum that parses. `conflict_resolution` has no safe default and the commented example says so |
| `~/.config/khard/khard.conf` | The address book | Live for the same reason; khard with no entry exits 3. `khard list` on an empty book prints `Found no contacts` and exits 1 — that is "nothing matched", not a fault |
| `~/.config/aerc/aerc.conf` | What a mail client shows and how it renders a part | Only the keys that differ from aerc's compiled-in defaults, plus `[filters]` — which is not a struct with defaults behind it and therefore replaces the whole set |
| `~/.config/aerc/accounts.conf` | Your mail accounts | Not shipped, and must not be. aerc refuses to start on one that group or other can read, and everything the build ships is 644; with no file, aerc's wizard writes it at 600 |
| `~/.config/aerc/binds.conf` | aerc's keys | Not shipped either. aerc's compiled-in bindings are empty and the file is the only source, so a partial one would unbind every key it did not name |

Every `[filters]` row but `colorize` goes through `kdos-part`, one script installed in aerc's own
filter directory. Its stderr is the message body, so a row naming a program that is not on the image
shows the shell's `command not found` where the message should be. A type absent from `[filters]`
gets aerc's *No filter configured* card rather than a rendering.

### `~/.config/yazi/theme.toml`

Generated by `kdos theme`; edits are overwritten. It is partial on purpose: yazi deserialises it
*over* its own `theme-dark.toml` preset key by key, so only what the palette decides is written and
the preset's icons, separators and file-type rules stand. `[flavor]` is not written — it is the one
part the preset splits by dark and light mode.

## Generated files you should not edit

All of these are rewritten by `kdos theme`.

| Path | Read by |
|---|---|
| `~/.themes/KDOS-<accent>/` | GTK applications in boxes. The accent is in the name because GTK rebuilds its style cascade when the theme name moves, and never when one file under a fixed name is rewritten |
| `~/.themes/KDOS` | A symlink to the above, for everything written against the fixed spelling |
| `~/.icons/KDOS/`, `~/.icons/KDOS-cursors/` | Every toolkit, host and box |
| `~/.config/gtk-3.0/settings.ini`, `~/.config/gtk-4.0/settings.ini` | The theme name, where a toolkit cannot reach the portal |
| `~/.config/gtk-4.0/gtk.css` | libadwaita, which ignores GTK themes entirely. There is no GTK3 copy: GTK loads that file once at startup and never again, so a palette pinned there would outrank the theme for the life of the process and stop every GTK3 application following an accent switch |
| `~/.config/kdeglobals` | Qt applications — merged, so your own settings survive |
| `~/.config/foot/themes/kdos` | The terminal |
| `~/.config/btop/themes/kdos.theme` | The system monitor |
| `~/.config/tmux/themes/kdos` | The terminal multiplexer |
| `~/.config/kdos-comp/themerc-override` | The compositor's frames |
| The palette block in `~/.config/starship.toml` | The shell prompt, between markers — the rest of the file is yours |
| `~/.cache/kdos/wallpaper.png` | The compositor |
| `~/.cache/kdos/theme` | Everything: one word, the accent name |

`kdos theme --audit` reports anything that differs from what this machine's palette produces.

## See also

- [Administration](../02-user-guide/administration.md) — using these in the jobs they belong to
- [Theming](../02-user-guide/theming.md) — the generated files and what regenerates them
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor keys in context
- [kdos-appbox](../04-programs/kdos-appbox.md) — box profiles in full
- [Filesystem and IPC](filesystem-and-ipc.md) — the paths and sockets these name
