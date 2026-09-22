# Configuration

Every configuration file a KDOS user or administrator may edit, every key in it, and **when a
change takes effect**. There is no configuration abstraction layer here: each of these is a file
that takes effect.

## Where configuration lives

| Tier | Path | Belongs to |
|---|---|---|
| Machine | `/etc/kdos/`, `/etc/` | The administrator |
| Per user | `~/.config/kdos/`, `~/.config/kdos-comp/` | You |
| Per user, generated | `~/.themes/`, `~/.icons/`, `~/.config/gtk-*`, `~/.config/foot/themes/` | `kdos theme` — do not edit |
| State, not configuration | `~/.local/state/kdos/`, `~/.cache/kdos/`, `/var/lib/kdos/` | Programs |

New users are populated from `/etc/skel`, so editing a file there changes the defaults for
accounts created afterwards.

**When a change applies** is one of:

| | |
|---|---|
| **immediate** | On signal — `kdos theme` already sends one |
| **next start** | When that program next runs |
| **next login** | The setting is a supervised program's command line |
| **boot** | Read once at boot |

---

## `~/.config/kdos/launcher.conf`

`Super+d`'s own key, and only one.

| Key | Default | Means |
|---|---|---|
| `files` | `no` | Also list files whose name matches, from this user's index |

Written by **`kdos-settings`**, on its Desktop page. No signal is sent: the launcher is started by
its chord and reads this as it comes up, so there is no long-lived process holding a stale answer.

**Off by default, and not for speed.** A launcher that searched the disk unasked puts a person's
filenames on screen the moment they press a key — in front of whoever is standing behind them, on
a machine they may have opened to start a browser. Turning it on is a decision about who can see
the screen.

With it on, a query of three characters or more is also put to `~/.cache/kdos/plocate.db`, which
`kdos-updatedb` rebuilds nightly from `$HOME` only. Files are listed under the applications with
their directory on the right, and Enter opens one by its handler rather than executing it.

## `~/.config/kdos/comp.conf`

The compositor's KDOS keys. **Only** these — bindings, mouse behaviour, workspaces and window
rules are `rc.xml`'s, and an old-style line here is reported by name rather than ignored.

Every key ships commented out at its default.

| Key | Default | Applies | Means |
|---|---|---|---|
| `wallpaper` | the shipped image | immediate | Path, or `none` |
| `crt` | `55` | immediate | Phosphor pass strength, per cent. `0` is off |
| `crt_scanlines` | `0` | immediate | Scanline depth |
| `crt_curve` | `0` | immediate | Barrel distortion |
| `crt_fullscreen` | `on` | immediate | Whether the pass runs over a fullscreen window |
| `idle_dim` | `300` | immediate | Seconds of inactivity before dimming |
| `idle_lock` | `600` | immediate | Seconds before locking |
| `idle_off` | `900` | immediate | Seconds before powering outputs off |
| `lid_close` | `suspend` | immediate | `suspend`, `off`, or ignore |
| `icons` | `yes` | immediate | Whether chrome draws pictures at all |
| `panel_opacity` | `80` | immediate | Panel opacity, per cent |
| `panel_margin` | `0` | immediate | Panel margin |
| `window_memory` | `yes` | immediate | Whether an application opens where its window last was, per `app_id`, from `~/.local/state/kdos/winpos` |
| `panel` | `bottom` | **next login** | `bottom`, `top` or `off` |
| `panel_cells` | `2` | **next login** | Panel height in cells |
| `panel_font` | `Terminus:pixelsize=20` | **next login** | The panel's font pattern |
| `panel_autohide` | `no` | **next login** | |
| `desktop_icons` | `yes` | **next login** | Whether desktop icons run |
| `slit` | `no` | **next login** | The dockapp column |
| `clipboard` | | **next login** | The clipboard history daemon |
| `chrome_font` | `Terminus:pixelsize=32` | **next login** | The font every KDOS surface draws with |
| `clock_format` | `%H:%M` | **next login** | |

**The three idle timers default to zero in a virtual machine** unless any `idle_*` key is set,
because a blanked screen over a remote display is indistinguishable from a crashed compositor.

**A startup-only key that changed is reported by name** on reload, and the running value is kept —
a configuration structure that disagreed with the running chrome is how a later reader concludes
the setting works.

## `~/.config/kdos/panel.conf`

Read by the panel, and re-read on the same signal a theme change sends — so these apply
**immediately**.

| Key | Default | Means |
|---|---|---|
| `right` | `pager tray more media privacy mpris clipboard cpu stutter update restart net volume battery notify clock` | The notification-area widgets, **in order** |
| `overflow` | `stutter restart clipboard` | Which of them live behind the chevron |
| `meters` | `cpu ram net` | Which meters, **in order of importance** — a narrow bar drops them from the right |
| `task_labels` | `auto` | `auto`, `yes` or `no`: the ladder, always, or never |
| `tray_hide` | empty | Tray item identifiers not drawn on the bar; they go in the chevron's popup |
| `start_label` | `yes` | Whether the Start button carries its word |
| `icons` | `yes` | Whether pictures are drawn |

Available meters: `cpu`, `ram`, `disk`, `net`, `diskio`, `temp`.

**`temp` is the hottest sensor on the machine**, on a fixed 0–100 °C band. It is read every fourth
sample rather than every one: the read walks `/sys/class/hwmon`, and a die's temperature does not
move meaningfully in half a second. Where nothing answers, the meter holds rather than drawing
zero — a machine with no sensor is not one running cold.

**The `update` widget reads a FILE and never computes.** `kdos update check` walks the ports tree
against the package database — hundreds of file reads, and nothing the panel may do on a tick,
where the rule is that nothing blocks the frame. The count comes from
`$XDG_STATE_HOME/kdos/update.json`, which `kdos update check --json` writes; **absent is zero** and
the badge is simply not there, which is the honest picture of a machine nobody has checked. It is
re-read at most once a minute, because it changes at most as often as whatever refreshes it.

**An unknown widget name is reported, not ignored.** The loader restores every default before
parsing, because it runs again on reload and a reload that only ever *added* would leave a widget
hidden after the line hiding it was deleted.

## `~/.local/state/kdos/toggles/`

The switches a desktop needs at hand. **A file's presence means on**; there is no format and nothing
to parse, which is the smallest thing a shell script, a chord and a surface can all read without
agreeing on a syntax first.

| Toggle | Means | Read by |
|---|---|---|
| `stay-awake` | never save, lock or blank on idle | `kdos-comp`'s idle policy |
| `night-light` | warm the palette | every surface, on the retint signal |
| `dnd` | hold notifications back | `kdos-notifyd` |

`kdos toggle` lists them, `kdos toggle <name>` flips one, `kdos toggle <name> on|off` sets it.

**They are state rather than configuration keys** because a configuration file is read when the
session starts, so a runtime writer would make half its answers come from before an edit and half
from after. A toggle is set by another process — a chord, a menu row, a script before a long
build — so its reader must not hold a copy: `stay-awake` and `dnd` are stat'd on a tick their
reader already runs, and `night-light` is read on the retint signal, which `kdos toggle` sends
after writing the file because a palette is applied once and not consulted per frame.

**Night light is a transform over the eight slots, not a scheme of its own.** Seven accents times a
warm copy would be fourteen palettes to keep in step. `ktui_theme_night()` warms whatever scheme is
loaded — green to 93%, blue to 77%, red untouched, so the accent still reads as itself — and
turning it off returns to the table rather than undoing the arithmetic, which eight bits cannot do.

**`kb_toggle_on()` and `kb_toggle_set()` are the one reader and the one writer in the tree**, and a
program that spells the path itself is a program looking where nothing wrote. The notification
centre's Do Not Disturb button writes this file through the daemon rather than keeping a flag of
its own: a second flag OR'd with this one is a state that button cannot clear, so it would silence
the toasts and say it had not. A held notification still reaches the history and the badge,
so Do Not Disturb hides a toast rather than losing it; an urgent one is shown anyway.

**`stay-awake` is consulted before all three idle steps, not the first only.** Somebody who
suppressed the saver did not ask to be locked either, and a machine that locked during the
presentation they turned the saver off for is the failure the toggle exists to prevent.

## `~/.config/kdos/favorites`

One desktop-entry identifier per line — the file name under the applications directory, without
the extension.

**Two surfaces read this one file**: the panel's quick-launch row and the Start menu's pinned
column. Two lists of favourites would be two things to keep in agreement, and one always loses.

It **ships populated**. An empty list makes both surfaces look broken on a freshly booted machine;
delete every line for an empty one. **An identifier with no matching entry is skipped in silence**,
so an application this image's catalogue does not carry leaves no launcher that opens nothing.

**A line may carry a two-letter code**: `mc code=FM`. The code is drawn right-aligned on the row in
both the Start menu and the palette, and typing both letters with nothing else in the search field
opens that row — no arrows, no `Enter`, and no waiting to see whether the search narrowed to one. It
is DESQview's Open Window shorthand, and it is a property of the **pinned row** rather than of the
program, because the codes are the person's own and this file is where they say so. **A letter is
part of a code only while a code could still match it**: with no coded line in this file, or a first
letter no code begins with, the letters are a search like any other — a rule that ate two letters
whatever the file said would be a search box that lost the first two characters of every query. Anything else after the id is
ignored rather than refused: this file is edited by hand, and a line a later version understands
must not stop this one launching it.

**The terminal follows the desktop.** A line naming `foot` or `kdos-term` resolves to whichever of
the two the session that is reading it runs — `kdos-term` on the console, `foot` on the compositor
— exactly as every chord, menu row and `Terminal=true` entry does. A shell no session started has
neither, and the entry runs where it already is. `foot` is a Wayland client and
the console has no compositor to run it on, so without this rule a pinned terminal on the console
was a row that launched nothing; two favourites files would be two things to keep in agreement, and
the one nobody is looking at is the one that goes stale.

Written by the panel and the menus when you pin, unpin or reorder.

## `~/.config/kdos/slit.conf`

One gadget per line, three fields:

```
<interval> <width> <command and its arguments>
```

`<interval>` is seconds between runs, `<width>` the cells the gadget is given, and the rest is the
command — split the way a desktop entry is, so no shell is involved. **The first line of its output
is what is drawn**, clipped to the width. Up to 32 gadgets; blank lines and `#` comments are
skipped, an interval below one second is raised to one, and a width is clamped to 1-64.

```
60 8 date +%H:%M
10 8 kdos-res --line cpu
```

**Ships absent, and its absence is why `kdos-slit` exits.** A column of empty marks says less than
no column. The slit's own width is the widest gadget's, and it docks with an **exclusive zone**: a
maximised window stops short of it rather than covering it.

## `~/.config/kdos/session-restore`

**Ships absent.** Its existence is the setting: with it, the windows open when the session ended
are reopened. The list itself is written before the confirmation dialog, because after the answer
there is no session left to ask.

## `/etc/kdos/menu.conf`, merged under `~/.config/kdos/menu.conf`

**A route is a name a script can hold.** One `route = argv` per line. A chord opens a surface and
a person clicks a row; neither is something a shell script, a documentation page or another program
can refer to. `kdos menu summon setup.network` is, and it keeps resolving when the chord is rebound
or the row moves.

The name is `verb.noun` and the verb is the shape of what is being done rather than the program
that does it: somebody looking for the wifi is looking to **set something up**, and does not know
which of eleven surfaces owns it.

**The value is an argument vector, split on spaces and run without a shell.** There is no quoting
and there will not be: a route that needed a shell would be a route a menu file could run anything
with, and this file merges a copy the user owns over the system's.

**The system file is read first and the user's second.** A route named in both is the user's; one
named only in theirs is added. **There is no delete**, which is the point — a name a script may
hold has to keep resolving.

`kdos-start` searches the routes beside its fixed rows, so the names are not a second vocabulary:
`network` finds the row and `setup.network` finds the same thing. `preflight.sh` fails on a route
whose first word is a command the image does not carry.

**A key beginning `@` is a setting about the menu, not a route.** Its value is read by whichever
surface asks for it and is never run; `@` cannot begin a route name, so that is the whole of the
distinction. A setting that fell through to the route table would be a launchable row running the
first word of its own value, and `preflight.sh` skips `@` lines for exactly that reason.

| Setting | Default | What it does |
|---|---|---|
| `@toplevel` | `Network Sound Displays Terminal` | Which system rows `kdos-start` keeps outside the fold when it is too narrow for three columns. Labels as the menu draws them, separated by spaces or commas, whole entries and case-insensitive. Written by **`kdos-settings`**, on its Panel page, into the **user's** copy: every other line — every route — is copied through byte for byte, which is what makes writing one `@` key into a route table safe. |

Below a hundred columns the Start menu's system group folds behind one `Settings ▸` row; the labels
named here stay listed beside it. A label that names no row promotes nothing and reports nothing —
a preference file is not a wiring diagram — so `selftest.sh` is what fails on the typo.

## `~/.config/kdos/screensaver.txt`

**Read by the `art` and `bounce` effects; the other six need no file at all.** Ships absent, and
`/usr/share/kdos/screensaver.txt` is what is drawn without it. A UTF-8 grid
of characters, one line per row; SGR colour in it is stripped, because a surface paints slots and
the effect picks one. The screensaver's art mode is a transform over this grid, so replacing the
file replaces the picture without touching the program.

**It is not `logo.txt`.** That file is the login banner's, generated from the mascot, and a person
replacing their screensaver must not be replacing the picture the machine boots with. A grid wider
or taller than the screen is pinned rather than bounced; trailing blank lines are dropped, or the
art would bounce off an edge nobody can see.

## `~/.config/kdos/a11y`

**Ships absent.** Its existence — an empty file is enough — opts boxed applications into the
accessibility stack. `KDOS_A11Y=1` does the same for one launch.

The host runs no accessibility **registry**, so the default avoids a startup probe that always
times out, and a screen reader running **inside** a box reaches that box's own registry — which is
what this enables.

**Nothing reads the desktop itself.** See [Accessibility](../02-user-guide/accessibility.md) for
what that means and what is left.

## `~/.config/kdos/boxes/<name>.conf`

One per box. Every key maps onto a container-engine flag or onto something KDOS enforces itself,
and the profile printer names which.

| Key | Means | Applies |
|---|---|---|
| `base` | `pack:<id>`, `box:<name>`, or `image:<ref>` | **create time** |
| `persistence` | Whether the writable layer survives | |
| `export` | Whether its applications get host launchers | |
| `network` | Network namespace | **create time** |
| `ipc` | IPC namespace | **create time** |
| `devices` | Whether `/dev` and the runtime directory are shared | **create time** |
| `audio` | Rides on `devices` | |
| `gpu` | The card's device nodes. Subtracts nothing from a shared `/dev`; with `devices = private` it is the `--volume /dev/dri` that binds the card back | **create time** |
| `memory` | Budget, enforced by **the memory daemon**, not the engine | immediate |
| `accent` | The box's colour, which draws a title-bar chip | on reload |
| `autostop` | Idle timeout for the collector | |
| `grant` | Compositor globals the sandbox allowlist otherwise refuses | on reload |
| `image` | The reference, for a registry base | **create time** |
| `display` | Carried and not interpreted: it means nothing to a container flag and nothing on this desktop reads it. Kept so that a profile rewrite does not silently drop somebody else's key | next launch |
| `render` | Which graphics this box's applications get. `auto` (the default, and what an absent key means) resolves by **opening** a `/dev/dri/renderD*` node: hardware where one opens, software where none does. `gpu` asks for the same thing and `software` refuses the card whatever is plugged in. The resolved answer reaches the guest's own Mesa and nothing else: `LIBGL_ALWAYS_SOFTWARE=1` in a software box's launch environment, and nothing at all for the hardware one, because Mesa asks the machine the same question by itself. It is **advisory** — an application may unset the variable. The resolve refuses the card without opening anything for a box that can see no node — `devices = private` with `gpu = no` — because this process's `/dev` is not that box's. Its own key and not `gpu`, which is about device nodes rather than about who draws | next launch |

**A namespace key applies at create time** and cannot be re-flagged on a live container, so
changing one says to recreate the box rather than silently doing nothing.

**`gpu` and `audio` are not independently enforceable** — there is no flag that grants a box a
speaker and denies it a camera. The profile says so rather than pretending.

**`base = image:` is an online operation** and announces itself before doing anything.

Edited by `kdos-box profile`, or by the Settings program, which **runs that command** rather than
writing the file itself.

## `~/.config/kdos/res.conf`

The resource monitor. Sort keys use the page identifiers from its own registry, so there is one
spelling. An unknown key is reported by name rather than ignored. **`kdos-settings` writes all of
these but `columns`**, on its Hardware page, and signals `kdos-res` exactly — `kdos-resctl` is a
longer name with this one as its prefix and is setuid, so a substring match would kill a privileged
helper that handles no signals.

| Key | Default | Means |
|---|---|---|
| `interval` | `1000` | Sampling interval in milliseconds. Floored at 200: a monitor sampling faster than that is mostly measuring itself |
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

## `~/.config/kdos/term.conf`

The terminal. Every key has a working default and the file need not exist; an unknown key is
reported by name rather than ignored. **`kdos-settings` writes all of it**, on its Desktop page,
and signals `kdos-term` — so a change reaches every terminal already open. A window that has
stepped its own font or its own transparency keeps what it stepped: this file is where a window
STARTS.

| Key | Default | Means |
|---|---|---|
| `shell` | `$SHELL`, then `/bin/sh` | What an argument-less `kdos-term` runs. Split as a desktop entry's `Exec` is — there is no shell |
| `font` | the toolkit's | fontconfig name. The size a window opens at: `Ctrl+=` and `Ctrl+-` step it for that window alone and `Ctrl+0` comes back here |
| `columns` | 80 | Columns asked for on the first configure |
| `rows` | 24 | Rows asked for on the first configure |
| `scrollback` | 2000 | Lines kept above the screen |
| `images` | `yes` | Decode pictures. `no` turns the three image protocols off in the parser, not merely in the drawing |
| `image_max` | 1024 | The cap on one image payload, in kilobytes |
| `image_cells` | 200 | The widest and tallest a picture may be, in cells |
| `paste_guard` | `yes` | Ask before an **unbracketed** paste carrying a newline. With bracketed paste on the child sees the text as text and decides for itself; with it off the bytes go straight to the pty and a newline **executes** — at a shell, at an `ssh` password prompt, inside `read`. A second attempt within five seconds means it |
| `opacity` | 100 | How much of the window's own background it keeps, per cent, 20–100. Below 100 the desktop shows through the cells the terminal has not drawn on; the ink is never mixed. **Only where a compositor is under it**: on a bare terminal there is nothing behind the window at all |

Re-read on `SIGHUP`, which is what `kdos theme` sends.

## `~/.config/kdos-comp/rc.xml`

**The compositor's own configuration**, in the upstream format — so upstream's documentation
applies verbatim. Bindings, mouse behaviour, window rules, workspaces, theme keys and fonts.

**`<default />` must be the first child of both `<keyboard>` and `<mouse>`.** The compositor loads
its built-in bindings only when your file defines none of that kind, so a file that binds one key
throws every default away — including click-to-focus, the title-bar drag and the window buttons.
Put your own bindings **after** it; the later of a duplicate pair wins.

**The title-bar font must name a scalable face.** Naming the bitmap console font resolves and then
silently falls back to a generic sans.

Applies at **next login**, or on reload for the parts the compositor re-reads.

## `~/.config/kdos-comp/menu.xml`

The root and client menus. **It deliberately lists no applications** — those are a program that
reads the same desktop entries everything else does, because a menu built at compositor startup
would be the one that went stale.

## `~/.config/kdos-comp/themerc-override`

**Generated by `kdos theme`.** Do not edit; a style file's dotted keys are appended after the
generated block and win.

## `/etc/kdos/packd.conf`

| Key | Default | Means |
|---|---|---|
| `retain` | `1` | How many **superseded** versions of a pack the store keeps |

Retention is what makes rollback possible: a store keeping none could not roll anything back, and
one keeping every version would fill a disk. **`0` is an honest off** — rollback then answers that
no earlier version is kept rather than failing at a rename.

**The sweep runs after an install and at no other time.** A sweep on a timer would be a background
job deleting somebody's rollback while they were deciding whether to use it.

Applies at **next start** of the daemon.

## `/etc/kdos/zram.conf`

| Key | Default | Means |
|---|---|---|
| `size` | `50` | **A percentage of RAM** |
| `algorithm` | `zstd` | Compression algorithm |

`size` is how much swap the device may claim to hold, **never** how much memory it will occupy —
the compressed pages live in that same memory. Applies at **boot**.

## `/etc/kdos/mountd.conf`

**Not shipped**; create it to change the defaults.

| Key | Default | Means |
|---|---|---|
| `exec` | `no` | Whether removable media are mounted executable |
| `format` | `no` | Whether `kdos-mountd` will write a filesystem over a device at all |

Everything removable is mounted without setuid and without device nodes regardless. `exec = yes`
is how somebody says they meant it: a setuid binary on somebody else's stick is a local root hole.

`format = yes` is the same argument. Writing a filesystem is not undoable, and a desktop that
offers it by default on every machine it is installed on is one where a mis-click costs somebody
their photographs. The daemon still refuses the boot medium and still demands the device's own
kernel name typed, whatever this says — the key decides whether the verb exists, not whether it is
careful.

## `/etc/kdos/keys/`

**The trusted key directory, and the directory *is* the policy.** Every public key here is one this
machine accepts a signature from. Adding a key is copying a file in; removing trust is deleting
one. There is no revocation list and no online check.

| Directory | Trusted for |
|---|---|
| `/etc/kdos/keys/` | Host package indexes and package signatures |
| `/etc/kdos/keys/packs/` | The application pack index |

**The loader does not descend into subdirectories**, which is what keeps those two genuinely
separate policies — a pack key in the upper directory would silently become a trusted publisher of
host packages.

The pack directory ships one key, `kdos-packs.pub`, which attests that the application images
beside it came from one bake. Replace it and re-sign to make the index attest something about you.
`README` in the upper directory states the policy on the machine itself.

The host key directory ships **empty**: packages built here are not signed and need no signature,
since a port's integrity is the checksum in its recipe.

## `/etc/nftables.conf`

The firewall. Applies at **boot**, before the network starts, after a syntax check — so an
unloadable ruleset leaves the previous state standing rather than half-applying a flush.

The shipped policy drops input and forwarding, accepts established traffic and loopback, answers
the necessary ICMP and **ICMPv6** types, and opens multicast DNS and DHCPv6. Anything that should
be reachable needs a rule; the file carries commented examples.

## `/etc/fstab`

Applies at **boot**. The shipped entries are the pseudo-filesystems, the control-group hierarchy,
and two temporary filesystems.

**The temporary filesystem for scratch space must be mode 1777.** Default options give a
root-owned filesystem that *hides* the correct one baked into the image, so no ordinary user can
write there at all — and every graphical application depends on it. Because a mounted filesystem
ignores a mode change on remount, the boot sequence also applies the mode explicitly.

**The installer appends to this file rather than replacing it**, precisely because of that entry.

## `/etc/inittab`

Applies at **boot**. Terminal one runs `kdos-login`, which hands the tty to `agetty` and
autologins where `login.conf` names an account; terminal two is an ordinary login and is the
**recovery console**; the serial line gives a login on demand. Both terminals are wrapped by the
VT-font loader.

Renaming the desktop user must rewrite `login.conf`'s `autologin`. It names the account tty1 logs
in, so a name that matches nothing leaves the machine reachable only from terminal two.

## `/etc/kdos/login.conf`

Who tty1 logs in, and nothing else. One key, because there is one decision on this path.

| Key | Default | Applies | Means |
|---|---|---|---|
| `autologin` | `kdos` | **boot** | The account tty1 logs in without asking |

**Absent or commented is a password prompt**, not a default account: `kdos-login` reads the key and
asks when it finds none. `autologin =` with nothing after it would be an account called `""`, which
`agetty` would be handed — so turning it off COMMENTS the line rather than emptying it, which is
what `kdos-power autologin off` writes and what the installer writes for an unattended `autologin =
no`.

It is the **only** place the desktop's account is named: `/etc/inittab` carries no account at all.
A rename that misses this key leaves the machine reachable only from terminal two.

## `/etc/service.disabled/<name>`

**A marker file, not a setting.** Creating one stops the matching init script running at boot:

```sh
sudo touch /etc/service.disabled/cups
```

## `/etc/keymap`

The console keymap, written by the installer, loaded on every terminal — and translated into a
graphical keyboard layout when a session starts.

## Speech-to-text models

`kdos-rec` greys *Transcribe* until a whisper.cpp model is on the machine. There is no
configuration key: three locations are looked at in order and **the first hit wins**.

| Order | Location |
|---|---|
| 1 | `$KDOS_WHISPER_MODEL` — one file, named exactly |
| 2 | `$XDG_DATA_HOME/whisper.cpp/models/`, default `~/.local/share/whisper.cpp/models/` |
| 3 | `/usr/share/whisper.cpp/models/` |

**`$KDOS_WHISPER_MODEL` is the whole answer when it is set.** No directory is searched behind it,
and a file that is missing or fails the test below leaves transcription unavailable rather than
falling through. Somebody who named a model and got a different one has been lied to.

Inside a directory the pattern is `ggml-*.bin` and the winner is the **first in sorted name
order** — not the newest, because an mtime is not reproducible and a name is.

**The gate is the file's magic, not its name.** A candidate counts only if its first four bytes are
`lmgg`, so a half-finished download reads as *no model* rather than as a crash behind an enabled
button. The surface's header line names the model that was found, or the directory that was
searched.

Upstream's own directory name is used deliberately: the model is whisper's data, and naming where
`models/download-ggml-model.sh` writes means a model fetched by that script and one fetched here
land in the same place.

**Nothing ships a model, and `kdos speech` is how one arrives.** `kdos speech list` prints the
catalogue — every English-only size, the three smallest multilingual ones and the two large ones
worth the disk, smallest first — and `kdos speech get <name>` downloads it into
`$XDG_DATA_HOME/whisper.cpp/models`, which needs no privilege because a model is a per-user
choice. The name is looked up in a table and never interpolated into a URL, the download is
verified against a sha256 compiled into the tool, and the file is renamed into place only after
both that and the `lmgg` magic pass — so an interrupted download is never a file the gate above
has to reject. The checksum says the bytes are the bytes this tree was written against; upstream
signs nothing, and that is the [unsigned content](../03-architecture/security-model.md) rule
applying here as everywhere.

**`kdos-rec`'s third button is *Get model* while there is none**, and it opens a terminal on that
same command rather than downloading inside the panel: a progress bar, a cancel, a disk-full and a
network that went away all already exist in the command, and a second implementation of the four
in the process that draws the taskbar is not worth the button. The window keeps looking while
there is no model, so the button becomes *Transcribe* on its own when the file lands.

**`whisper-stream` wants the same model and is not a desktop verb.** It transcribes a live
microphone rather than a closed file — through SDL's audio device, which opens no window, so it
runs in a terminal with no window above it — and nothing starts it. `kdos-rec`'s *Transcribe* is `whisper-cli` over the file it has just recorded.

## Video in a call

`baresip` writes `~/.baresip/config` on a first run and only when there is none, so the default
below is what a fresh account gets and an edited file is never overwritten. Five module lines are
uncommented there that upstream leaves commented, and they are exactly the five this image builds:
`opus.so`, `avcodec.so`, `vp8.so`, `vp9.so` and `sdl.so`. An uncommented line naming a module that
is not installed is a start-up error, which is why upstream's default comments them all.

**The display is uncommented and the camera is not**, and the asymmetry is the point: which screen
a picture goes on is a property of the build, and which camera it comes from is a choice. Turning
on `avformat.so` is what sends yours — it registers a video source and the shipped ffmpeg carries
`video4linux2`. `x11.so` is not built, by rule; `fakevideo.so` is a null sink and `vidbridge.so` a
loopback, and both stay commented because each is something a person chooses deliberately.

## Shipped configuration for software that is not ours

`/etc/skel` also carries configuration for the third-party programs the system ships, so a new
account gets a working setup rather than each program's own defaults. These are yours to edit.

| Path | For | Notes |
|---|---|---|
| `~/.config/foot/foot.ini` | The terminal | Sets **server-side decorations**, so windows wear the compositor's frame rather than the terminal's own |
| `~/.config/foot/themes/kdos` | The terminal's colours | **Generated** |
| `~/.config/btop/btop.conf` | The system monitor | |
| `~/.config/btop/themes/kdos.theme` | Its colours | **Generated** |
| `~/.config/kdos/term-colors.conf` | The sixteen colours a program asks for, in this desktop's terminals | **Generated** |
| `~/.config/kdos/fzf-colors` | fzf's `--color` flags, sourced by `/etc/profile.d/30-kdos-colors.sh` | **Generated** |
| `~/.config/bat/themes/kdos.tmTheme` | bat's theme, selected by file stem | **Generated** |
| `~/.config/micro/colorschemes/kdos.micro` | micro's colorscheme | **Generated** |
| `~/.config/helix/themes/kdos.toml` | helix's theme | **Generated** |
| `~/.config/nvim/colors/kdos.vim` | neovim's colorscheme | **Generated** |
| `~/.config/git/kdos-delta` | delta's colours, `[include]`d from the shipped gitconfig | **Generated** |
| `~/.config/newsboat/kdos-colors` | newsboat's colours, as 256 indices | **Generated** |
| `~/.config/aerc/stylesets/kdos` | aerc's styleset | **Generated** |
| `~/.config/tmux/tmux.conf` | The terminal multiplexer | |
| `~/.config/starship.toml` | The shell prompt | Only the palette block between its markers is generated |
| `~/.config/fastfetch/config.jsonc` | The system-information tool | The login banner runs it with its own logo disabled |
| `~/.config/lf/lfrc`, `~/.config/lf/preview` | The terminal file manager | |
| `~/.config/GIMP/3.0/gimprc` | The image editor | Selects the system theme, or it keeps its own |
| `~/.config/gtk-3.0/settings.ini`, `~/.config/gtk-4.0/settings.ini` | Toolkit settings | Cursor theme and size |
| `~/.config/<desktop>-mimeapps.list` | Default handlers for ONE desktop | Consulted before the plain list at the same level |
| `~/.config/mimeapps.list` | Default handlers per file type | Consulted before the generated caches |
| `/etc/xdg/kdos-mimeapps.list` | Default handlers for this desktop, system-wide | Consulted before the plain list at the same level |
| `/etc/xdg/mimeapps.list` | The layer under the desktop's own table | Last, and a type belongs in exactly one of these two |
| `~/.config/user-dirs.dirs` | The standard user directories | Seeded from `/etc/skel`; there is no `xdg-user-dirs` here. `$HOME` is the only expansion read |
| `~/.config/kdos/places` | Extra rows on the places column, `Name = /path` one per line | Merged over the user directories; a row whose path is already listed is dropped, and one pointing at nothing is never shown. Written by *Add to Places* on the desktop |
| `~/.config/kdos/background.txt` | Your own console background: UTF-8 text with SGR colour, read by `libkvt`'s parser like anything a program writes to a terminal | **Ships absent**, and outranks every shipped piece. Colours reduce to the theme's eight slots, so the art follows `kdos theme`. Glyphs outside `ter-kdos32n`'s 512 draw blank on `tty1` and correctly in a terminal — see `/usr/share/kdos/backgrounds/README`. No cursor motion: the piece is measured by counting cells and lines |
| `~/.local/state/kdos/background` | Which shipped piece is in force, or `none` | Written by `kdos background`, which is what the chord and the `style.background` route run. **A name and never a path**: a chord that cycles pictures must not become a way to point the desktop at any file |
| `~/.config/yazi/theme.toml` | `yazi` in the active accent | **Generated by `kdos theme`; edits are overwritten.** Partial on purpose: yazi deserializes it OVER its own `theme-dark.toml` preset key by key, so only what the palette decides is written and the preset's icons, separators and file-type rules stand. `[flavor]` is not written — it is the one part the preset splits by dark and light mode |
| `~/.config/mc/ini` | How `mc` behaves: the KDOS skin, `F3` internal and `F4` to `$EDITOR`, no exit confirmation, the panel's directory in the window title | **The section a key is in is part of the key** — mc reads its behaviour flags out of `[Midnight-Commander]` and its screen layout out of `[Layout]`, and a key under the wrong header is silently never read. `kdos theme` merges `skin` into this file rather than replacing it, and generates the skin itself into `~/.local/share/mc/skins/kdos.ini`: mc looks for skins under `<data>/mc/skins`, `/etc/mc/skins` and `/usr/share/mc/skins` and nowhere else |
| `~/.config/mc/mc.ext.ini` | What `Enter` does on a file in `mc` | Replaces the system file wholesale — mc does not merge them. Only the archive rows whose VFS helper is on this image are carried; everything else falls to the catch-all, which is `kdos-appbox open` |
| `~/.config/mc/menu` | `mc`'s `F2` user menu | Eight verbs, each naming a program on the image; `testing/preflight.sh` refuses one that is not |
| `/etc/profile.d/30-open.sh` | What `$BROWSER` is | `xdg-open`, which on this image is `kdos-appbox open` — so the variable and the mimeapps table are one road rather than two that drift. Never set over a value you already exported. A login shell reads this, which is the only way into a session here |
| `/etc/profile.d/20-lesspipe.sh` | What `less` shows for a file that is not text | Sets `LESSOPEN` to `lesspipe.sh` and `LESS=-R`, neither over a value you already set. The filter is driven by `file -L -s -b --mime` and nothing else, which is why `file` on this image is the one with a magic database |
| `~/.mbsyncrc` | Fetching mail | **Mode 600** — it carries a password. Empty lines delimit sections, so a commented block must keep its blank lines or a `Channel` lands inside the `Store` above it. Ships with no accounts |
| `~/.msmtprc` | Sending mail | **Mode 600**; msmtp refuses a file carrying a `password` line that others can read, and `passwordeval` avoids the question. `/usr/sbin/sendmail` and `/usr/bin/sendmail` are links to this program. Ships with no accounts |
| `~/.config/notmuch/default/config` | The mail index | `mail_root` is **relative** and expands against `$HOME`, which is what makes one shipped file right for every account. It names `~/Mail`, and so do the other two |
| `~/.config/notmuch/default/hooks/` | What `notmuch new` does around the scan | `pre-new` fetches with `mbsync -a`, `post-new` tags. A non-zero `pre-new` **aborts** `notmuch new` — deliberately, because indexing after a fetch that did not happen reports an empty inbox |
| `~/.config/khal/config` | The calendar | **Live, not commented out** — khal with no `[calendars]` section refuses to start, so it points at an empty store instead. `type = discover` over a glob, because `type = calendar` **creates** the path it is given the first time anything reads it. The `[locale]` dates are ISO and `kdos-cal` depends on it: khal parses its command line with that format and prints with it too |
| `~/.config/vdirsyncer/config` | Syncing a calendar or address book with a server | Live but with no pair configured: `vdirsyncer sync` then exits 0, silently, and creates nothing. `[general]` and `status_path` are the minimum that parses — without them it exits 1. `conflict_resolution` has no safe default and the commented example says so |
| `~/.config/khard/khard.conf` | The address book | Live for the same reason; khard with no entry exits 3. `khard list` on an empty book prints `Found no contacts` and exits **1** — that is "nothing matched", not a fault |
| `~/.config/aerc/aerc.conf` | What a mail client shows and how it renders a part | Only the keys that differ from aerc's compiled-in defaults, plus `[filters]` — which is **not** a struct with defaults behind it and therefore replaces the whole set, so a type absent from it gets aerc's *No filter configured* card rather than a rendering. Every row but `colorize` goes through `kdos-part`, one script installed in aerc's own filter directory; its stderr is the message body, so a row naming a program that is not on the image shows the shell's `command not found` where the message should be |
| `~/.config/aerc/accounts.conf` | Your mail accounts | **Not shipped, and must not be.** aerc refuses to start on one that group or other can read, and everything the build ships is 644; with no file, aerc's wizard writes it at 600 |
| `~/.config/aerc/binds.conf` | aerc's keys | **Not shipped either.** aerc's compiled-in bindings are empty and the file is the only source, so a partial one would unbind every key it did not name. aerc installs its own beside `aerc.conf` on first run |
| `~/.config/xdg-desktop-portal-wlr/config` | The screen-capture backend | Uses an output picker; the alternative silently captures the first output, which is wrong the moment a second screen is plugged in |

## Generated files you should not edit

All are rewritten by `kdos theme`:

| Path | Read by |
|---|---|
| `~/.themes/KDOS-<accent>/` | GTK applications in boxes. The accent is in the NAME because GTK rebuilds its style cascade when the theme name moves and never when one file under a fixed name is rewritten |
| `~/.themes/KDOS` | A symlink to the above, for everything written against the fixed spelling |
| `~/.icons/KDOS/`, `~/.icons/KDOS-cursors/` | Every toolkit, host and box |
| `~/.config/gtk-3.0/settings.ini`, `~/.config/gtk-4.0/settings.ini` | The theme name, where a toolkit cannot reach the portal |
| `~/.config/gtk-4.0/gtk.css` | libadwaita, which ignores GTK themes entirely. **There is no GTK3 copy**: GTK loads that file once at startup and never again, so a palette pinned there outranks the theme for the life of the process and would stop every GTK3 application following an accent switch |
| `~/.config/kdeglobals` | Qt applications — **merged**, so your own settings survive |
| `~/.config/foot/themes/kdos` | The terminal |
| `~/.config/btop/themes/kdos.theme` | The system monitor |
| `~/.config/tmux/themes/kdos` | The terminal multiplexer |
| `~/.config/kdos-comp/themerc-override` | The compositor's frames |
| The palette block in `~/.config/starship.toml` | The shell prompt, **between markers** — the rest of the file is yours |
| `~/.cache/kdos/wallpaper.png` | The compositor |
| `~/.cache/kdos/theme` | Everything: **one word**, the accent name |

`kdos theme --audit` reports anything that differs from what this machine's palette produces.

## See also

- [Administration](../02-user-guide/administration.md) — using these in the jobs they belong to
- [Theming](../02-user-guide/theming.md) — the generated files and what regenerates them
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor keys in context
- [kdos-appbox](../04-programs/kdos-appbox.md) — box profiles in full
- [Filesystem and IPC](filesystem-and-ipc.md) — the paths and sockets these name
