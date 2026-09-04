# Configuration

Every configuration file a KDOS user or administrator may edit, every key in it, and **when a
change takes effect**. There is no configuration abstraction layer here: each of these is a file
that takes effect.

## Where configuration lives

| Tier | Path | Belongs to |
|---|---|---|
| Machine | `/etc/kdos/`, `/etc/` | The administrator |
| Per user | `~/.config/kdos/`, `~/.config/kdos-comp/`, `~/.config/kdos-con/` | You |
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
| `panel` | `bottom` | **next login** | `bottom`, `top` or `off` |
| `panel_cells` | `2` | **next login** | Panel height in cells |
| `panel_font` | `Terminus:pixelsize=20` | **next login** | The panel's font pattern |
| `panel_autohide` | `no` | **next login** | |
| `desktop_icons` | `yes` | **next login** | Whether desktop icons run |
| `slit` | `no` | **next login** | The dockapp column |
| `clipboard` | | **next login** | The clipboard history daemon |
| `chrome_font` | `Terminus:pixelsize=32` | **next login** | The font every KDOS surface draws with |
| `clock_format` | `%H:%M` | **next login** | |
| `window_memory` | | **next login** | Remember window positions |

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
| `right` | `pager tray more media privacy mpris clipboard cpu stutter restart net volume battery notify clock` | The notification-area widgets, **in order** |
| `overflow` | `stutter restart clipboard` | Which of them live behind the chevron |
| `meters` | `cpu ram net` | Which meters, **in order of importance** — a narrow bar drops them from the right |
| `task_labels` | `auto` | `auto`, `yes` or `no`: the ladder, always, or never |
| `tray_hide` | the input-method identifiers | Tray items to hide |
| `start_label` | `yes` | Whether the Start button carries its word |
| `icons` | `yes` | Whether pictures are drawn |

Available meters: `cpu`, `ram`, `disk`, `net`, `diskio`.

**An unknown widget name is reported, not ignored.** The loader restores every default before
parsing, because it runs again on reload and a reload that only ever *added* would leave a widget
hidden after the line hiding it was deleted.

## `~/.config/kdos/favorites`

One desktop-entry identifier per line — the file name under the applications directory, without
the extension.

**Two surfaces read this one file**: the panel's quick-launch row and the Start menu's pinned
column. Two lists of favourites would be two things to keep in agreement, and one always loses.

It **ships populated**. An empty list makes both surfaces look broken on a freshly booted machine;
delete every line for an empty one. **An identifier with no matching entry is skipped in silence**,
so an application this image's catalogue does not carry leaves no launcher that opens nothing.

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

## `~/.config/kdos/a11y`

**Ships absent.** Its existence — an empty file is enough — opts boxed applications into the
accessibility stack. `KDOS_A11Y=1` does the same for one launch.

The host runs no accessibility registry, so the default avoids a startup probe that always times
out. A screen reader running **inside** a box can reach that box's own registry, which is what this
enables.

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
| `gpu`, `audio` | Ride on `devices` | |
| `memory` | Budget, enforced by **the memory daemon**, not the engine | immediate |
| `accent` | The box's colour, which draws a title-bar chip | on reload |
| `autostop` | Idle timeout for the collector | |
| `grant` | Compositor globals the sandbox allowlist otherwise refuses | on reload |
| `image` | The reference, for a registry base | **create time** |
| `display` | `vt` pins its applications to a virtual terminal of their own on the console desktop, instead of the window they otherwise become | next launch |

**A namespace key applies at create time** and cannot be re-flagged on a live container, so
changing one says to recreate the box rather than silently doing nothing.

**`gpu` and `audio` are not independently enforceable** — there is no flag that grants a box a
speaker and denies it a camera. The profile says so rather than pretending.

**`base = image:` is an online operation** and announces itself before doing anything.

Edited by `kdos-box profile`, or by the Settings program, which **runs that command** rather than
writing the file itself.

## `~/.config/kdos/res.conf`

The resource monitor. Sort keys use the page identifiers from its own registry, so there is one
spelling.

| Key | Means |
|---|---|
| `sort` | Sort key per page |
| `columns` | Which columns to show |
| `interval` | Sampling interval |
| `units` | Unit style |
| `icons` | Whether to draw pictures |
| `cpu_percent` | Per-core or aggregate |
| `kernel_threads` | Include kernel threads |
| `pss` | Use proportional memory accounting |
| `temperature` | Show temperatures |
| `virtual_drives`, `virtual_net` | Include virtual devices |
| `machine` | Machine identification |

## `~/.config/kdos/term.conf`

The terminal. Every key has a working default and the file need not exist; an unknown key is
reported by name rather than ignored.

| Key | Default | Means |
|---|---|---|
| `shell` | `$SHELL`, then `/bin/sh` | What an argument-less `kdos-term` runs. Split as a desktop entry's `Exec` is — there is no shell |
| `font` | the toolkit's | fontconfig name |
| `columns` | 80 | Columns asked for on the first configure |
| `rows` | 24 | Rows asked for on the first configure |
| `scrollback` | 2000 | Lines kept above the screen |
| `images` | `yes` | Decode pictures. `no` turns the three image protocols off in the parser, not merely in the drawing |
| `image_max` | 1024 | The cap on one image payload, in kilobytes |
| `image_cells` | 200 | The widest and tallest a picture may be, in cells |

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

## `/etc/kdos/con.conf`

The console desktop. `~/.config/kdos-con/con.conf` overrides it key by key, and a key in neither
file takes the built-in default — a machine with no file at all boots a working desktop.

| Key | Default | Means |
|---|---|---|
| `greet` | `no` | Whether tty1 asks who you are. The installer writes `yes` |
| `autologin` | `kdos` | Which account `greet = no` logs in |
| `remote` | `no` | Whether a view may attach from another machine |
| `font` | `monospace:size=12` | The console font `kdos-view` rasterises on a KMS device |
| `sessions` | `4` | How many workspaces, 1 to 9 |
| `scrollback` | `2000` | Lines a terminal window keeps, **per window** |
| `idle_saver` | `300` | Seconds of no input before the saver covers the screen; `0` never |
| `idle_lock` | `600` | Seconds of no input before the screen locks; `0` never |
| `idle_off` | `900` | Seconds before the screen powers down; `0` never |
| `terminal` | `sh` | What `Super+Return` opens |
| `menu` | `kdos-start` | What `Super+space` starts |
| `launcher` | `kdos-launcher` | What `Super+d` starts |
| `lock` | `kdos-lock` | What `Super+l` starts |
| `saver` | `kdos-saver` | What `idle_saver` and `Super+Shift+l` start |
| `embed` | `yes` | Whether a graphical application becomes a window. `no` gives every one of them a terminal of its own |

**`remote = yes` opens no port.** There is no TCP listener anywhere in this desktop. It permits
`kdos con attach` over an ssh channel that forwards the unix socket, which is why a remote desktop
here needs no protocol of its own and no second authentication.

**Nine is the ceiling on `sessions`** because nine is the last digit `Super` can reach; a tenth
workspace would exist with no way to get to it. Lowering the count does not lose the windows on the
workspaces above it — they are on a workspace nothing switches to, so raising it again reaches them.

**There is no `idle_dim`.** `comp.conf` has one because a compositor can dim a framebuffer; a dim
is a brightness, and this desktop's colours are eight palette slots with no brightness between
them, so a "dimmed" grid would be a different picture rather than a darker one. The saver is not a
dim under another name: it is a picture, which a grid draws exactly. Each of the three delays is
measured from the last input rather than from the step before it, so they want to be written in
increasing order; the lock happens before the blank and never the other way, or the screen would
come back on showing what was on it. **In a virtual machine all three default to `0`** — a blanked
screen over VNC cannot be told from a crash — and writing a key here overrides that, including
writing `0`.

Chords are **not** here; they are in `keys.conf` below. Which key runs the launcher is a keyboard
question and which program *is* the launcher is not.

Read once, so applies at **next start** of the session — `greet` at **boot**, since
`kdos-con-login` reads it before anything is drawn.

## `~/.config/kdos-con/keys.conf`

The console desktop's chords, one `chord = action` per line. Shipped in `/etc/skel`.

**The same chords `rc.xml` binds.** The graphical desktop reads labwc's XML and this one reads no
XML at all, so the defaults are written twice in two syntaxes — but they are the same defaults,
because a person who learns `Super+Return` on one desktop must not have to unlearn it on the other.
Changing a default in one file changes it in the other.

| Action | Default | Action | Default |
|---|---|---|---|
| `terminal` | `Super+Return` | `next` | `Super+Tab` |
| `close` | `Super+q` | `prev` | `Super+Shift+Tab` |
| `maximise` | `Super+m` | `next-alt` | `Alt+Tab` |
| `fullscreen` | `Super+f` | `prev-alt` | `Alt+Shift+Tab` |
| `minimise` | `Super+n` | `snap-left` … `snap-down` | `Super+`arrow |
| `restore` | `Super+Shift+n` | `focus-left` … `focus-down` | `Super+Shift+`arrow |
| `workspace-prev` | `Super+PageUp` | `swap-left` … `swap-down` | `Super+Alt+`arrow |
| `workspace-next` | `Super+PageDown` | | |
| `menu` | `Super+space` | `launcher` | `Super+d` |
| `lock` | `Super+l` | `quit` | `Super+Shift+q` |
| `saver` | `Super+Shift+l` | `leader` | `Ctrl+a` |

Modifiers are `Super`, `Shift`, `Alt` and `Ctrl`, joined with `+`. An action no line names keeps
its default, so rebinding one key does not mean restating the rest.

**Workspaces by number are not in the file.** `Super+1..9` switches and `Super+Shift+1..9` sends the
focused window; eighteen lines saying one thing is not a configuration format. `workspace-prev` and
`workspace-next` are, because they are two chords rather than eighteen — they step to the next
workspace that has a window on it, skipping the empty ones, and wrap once.

**`focus-*` moves the focus and `swap-*` moves the window.** A directional focus goes to the
nearest window that starts past the focused one *and* shares rows or columns with it; nothing
overlapping means the focus stays where it is, and Alt-Tab is the way to a window the arrows cannot
see. A swap trades two rectangles, maximise and tile state included, and the focus follows the
window rather than the place.

**`Super+Shift+`arrow is unbound on the graphical desktop**, which has no directional-focus action.
Every other family in this table means the same thing on both.

**`Super` is the desktop's own modifier** — a chord without it belongs to whatever program has the
focus, and the two `Alt+Tab` lines are the exception a lifetime of muscle memory earns.

**`leader` is the other exception, and it cannot be on `Super`**: it exists for the views where
`Super` never arrives. A terminal reports it only through the kitty keyboard protocol, and one that
does not implement it — xterm, most VTEs, and the Linux VT a `--tty` view runs in — would otherwise
leave every chord above unreachable. The leader followed by a chord's own key runs that chord;
pressing it twice sends the literal to the focused window, so the key it occupies is not taken
away. It is one key deep with no timeout, and a second key naming no chord is swallowed rather than
typed.

Applies at **next start** of the session.

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

## `/etc/kdos/pack-sources`

Where application updates are looked for: **one directory per line**. A source is a directory with
a pack index in it — a second stick, a mounted share, a directory somebody copied a repository
into. The medium is always consulted and needs no line.

**A URL is never written here, and there is no line to uncomment that makes the machine reach the
network.** That is `kdos app update --online <url>`, an argument given each time, so it is visible
at the moment it is used.

**Nothing here is trusted**: every pack is hashed and verified where it is mounted, so a source can
offer a file and still not get it installed.

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

Everything removable is mounted without setuid and without device nodes regardless. `exec = yes`
is how somebody says they meant it: a setuid binary on somebody else's stick is a local root hole.

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

Applies at **boot**. Terminal one runs `kdos-con-login`, which greets or autologins according to
`con.conf`; terminal two is an ordinary login and is the **recovery console**; the serial line
gives a login on demand. Both terminals are wrapped by the console-font loader.

Renaming the desktop user must rewrite `con.conf`'s `autologin`. It names the account tty1 logs
in, so a name that matches nothing leaves the machine reachable only from terminal two.

## `/etc/service.disabled/<name>`

**A marker file, not a setting.** Creating one stops the matching init script running at boot:

```sh
sudo touch /etc/service.disabled/cups
```

## `/etc/keymap`

The console keymap, written by the installer, loaded on every terminal — and translated into a
graphical keyboard layout when a session starts.

## Shipped configuration for software that is not ours

`/etc/skel` also carries configuration for the third-party programs the system ships, so a new
account gets a working setup rather than each program's own defaults. These are yours to edit.

| Path | For | Notes |
|---|---|---|
| `~/.config/foot/foot.ini` | The terminal | Sets **server-side decorations**, so windows wear the compositor's frame rather than the terminal's own |
| `~/.config/foot/themes/kdos` | The terminal's colours | **Generated** |
| `~/.config/btop/btop.conf` | The system monitor | |
| `~/.config/btop/themes/kdos.theme` | Its colours | **Generated** |
| `~/.config/tmux/tmux.conf` | The terminal multiplexer | |
| `~/.config/starship.toml` | The shell prompt | Only the palette block between its markers is generated |
| `~/.config/fastfetch/config.jsonc` | The system-information tool | The login banner runs it with its own logo disabled |
| `~/.config/lf/lfrc`, `~/.config/lf/preview` | The terminal file manager | |
| `~/.config/GIMP/3.0/gimprc` | The image editor | Selects the system theme, or it keeps its own |
| `~/.config/gtk-3.0/settings.ini`, `~/.config/gtk-4.0/settings.ini` | Toolkit settings | Cursor theme and size |
| `~/.config/mimeapps.list` | Default handlers per file type | Consulted before the generated caches |
| `~/.config/user-dirs.dirs` | The standard user directories | |
| `~/.config/xdg-desktop-portal-wlr/config` | The screen-capture backend | Uses an output picker; the alternative silently captures the first output, which is wrong the moment a second screen is plugged in |

## Generated files you should not edit

All are rewritten by `kdos theme`:

| Path | Read by |
|---|---|
| `~/.themes/KDOS/` | GTK applications in boxes |
| `~/.icons/KDOS/`, `~/.icons/KDOS-cursors/` | Every toolkit, host and box |
| `~/.config/gtk-3.0/gtk.css`, `~/.config/gtk-4.0/gtk.css` | Applications that ignore themes |
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
