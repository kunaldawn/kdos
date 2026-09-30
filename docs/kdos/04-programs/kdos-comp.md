# kdos-comp

`kdos-comp` is the KDOS compositor: the program that draws the screen, places and decorates
windows, reads the keyboard and the pointer, and starts and supervises the panel, the desktop icons
and the session's small daemons. This chapter describes how it is configured, what it adds to the
Wayland compositor it is forked from, how each addition behaves, and how to work on its code. It is
for anyone configuring the desktop beyond the settings window, diagnosing a desktop problem, or
changing the compositor itself. Read [The desktop](../02-user-guide/desktop.md) first for the
user's view of the same desktop, and [The session](../03-architecture/session.md) for what starts
the compositor.

If you only want to change a setting or a key binding, read [Configuration](#configuration) and
[Bindings](#bindings). If the desktop misbehaves, start with [Debugging](#debugging) and the
session log. To change the compositor's code, read
[Working on the compositor](#working-on-the-compositor). The case for forking an existing
compositor rather than writing one is set out in
[Decisions](../01-philosophy/decisions.md#the-compositor-is-a-frozen-fork-of-labwc), and how that
desktop compares with those of other distributions in
[How KDOS differs](../01-philosophy/how-kdos-differs.md#the-desktop).

## Overview

`kdos-comp` is a hard fork of the [labwc](https://labwc.github.io/) 0.20.0 Wayland compositor,
built on wlroots (the `wlroots` port, version 0.20.2). A *hard fork* here means the source tree is
labwc's own, renamed and extended in place, and upstream changes are not merged into it. The KDOS
additions live in files of their own (twenty-three `kdos-*.c` sources, two headers and the
application-first switcher); upstream files carry only small, marked hooks into them (see
[Finding the KDOS additions](#finding-the-kdos-additions)).

| | |
|---|---|
| Binary | `/usr/bin/kdos-comp` |
| Upstream configuration | `~/.config/kdos-comp/rc.xml` and `menu.xml`, both copied from `/etc/skel` for a new user; `/etc/xdg/kdos-comp/` is also searched, and nothing ships there |
| Generated theme | `~/.config/kdos-comp/themerc-override`, written by `kdos theme` |
| KDOS configuration | `~/.config/kdos/comp.conf` (`$XDG_CONFIG_HOME/kdos/comp.conf`) |
| Started by | `kdos-desktop-start`, with no options — see [The session](../03-architecture/session.md) |
| Log | `$XDG_RUNTIME_DIR/kdos-comp.log`; the previous session's is kept as `kdos-comp.log.old` |
| Command socket | `$XDG_RUNTIME_DIR/kdos-cmd.sock` |
| Frame-timing socket | `$XDG_RUNTIME_DIR/kdos-frames.sock` |
| Source and recipe | `src/desktop/kdos-comp/` in the repository, with its `kpkgbuild` and `build.sh`; built by meson in the `50_desktop` phase, with no source archive to fetch |

`KDOS-FORK` at the root of the source tree records the upstream tarball and its checksum. Upstream's
licence (GPL-2.0) and copyright headers are kept.

The program has four parts. labwc's core does the window management: placement, focus, moving
and resizing, workspaces, bindings, menus and decorations. The KDOS additions sit beside it:
[the phosphor pass](#the-phosphor-pass), [the wallpaper](#the-wallpaper),
[idle, dim, lock and lid](#idle-dim-lock-and-lid),
[window groups and window memory](#window-groups-and-window-memory),
[box identity](#box-identity), [accessibility](#accessibility), [motion](#motion) and
[render-late scheduling](#render-late-scheduling). Eight
[supervised children](#supervised-children) (the panel, the desktop icons, the dockapp column, four
small session daemons and the on-screen keyboard) are started and restarted by the compositor. Two
sockets let other KDOS programs talk to it: [the command socket](#the-command-socket) and
[the frames socket](#the-frames-socket). A screen reader talks to it over the session bus, through
[the keyboard monitor](#the-keyboard-monitor).

### How labwc's documentation applies

Because this is a fork rather than a set of patches, labwc's documentation for `rc.xml`, `menu.xml`
and the theme applies as written: window management, key and mouse bindings, window rules, theme
keys and menus all behave as labwc's documentation describes. The differences are these:

- **The configuration directory.** Where labwc reads `~/.config/labwc/`, `kdos-comp` reads
  `~/.config/kdos-comp/`, and that is also where labwc's `environment`, `autostart` and `shutdown`
  files go. None of the three is shipped. Themes are still looked up under
  `themes/<name>/labwc/`, as upstream does.
- **Additions.** Nine actions, three for window groups and six accessibility switches (see
  [KDOS actions](#kdos-actions)), an `apps`
  style for the window switcher (see [The app-first window switcher](#the-app-first-window-switcher)),
  and a `flat kdos` title-bar fill in the theme (see [Decorations](#decorations)).
- **The prompt command runs without a shell.** See [The prompt command](#the-prompt-command).
- **Windows on other workspaces are reported as minimised** to foreign-toplevel clients, the
  programs, such as the panel, that list other programs' windows through the wlr
  foreign-toplevel protocol. See
  [Smaller changes to upstream behaviour](#smaller-changes-to-upstream-behaviour).
- **Things that are not built.** The fork strips upstream's `docs/`, `clients/` (the `labnag` dialog
  and `lab-sensible-terminal`), `t/` (its tests), `po/` and `data/` directories, and the recipe
  configures meson with `-Dxwayland=enabled -Dicon=disabled -Dsvg=disabled -Dnls=disabled
  -Dman-pages=disabled -Dlabnag=disabled -Dsystemd-session=disabled`. So there are no window icons
  in title bars, no SVG button images, no translations, no session file and no systemd target.

The manual pages are not installed, so `man labwc-config` finds nothing on a KDOS machine. Read the
0.20.0 pages upstream at [labwc.github.io](https://labwc.github.io/): `labwc-config(5)` for
`rc.xml`, `labwc-actions(5)` for the actions, `labwc-menu(5)` for `menu.xml` and `labwc-theme(5)`
for the theme keys.

### Command-line options

These are labwc's, under the new name. `kdos-comp -v` prints `kdos-comp (labwc fork)`, the labwc
version, the compiled-in features and the wlroots version.

| Option | Does |
|---|---|
| `-c`, `--config <file>` | Use this `rc.xml` |
| `-C`, `--config-dir <dir>` | Use this configuration directory |
| `-d`, `--debug` | Log everything, including debug messages |
| `-e`, `--exit` | Tell the running compositor to exit |
| `-h`, `--help` | Show the options and quit |
| `-m`, `--merge-config` | Merge configuration and theme files from every XDG base directory |
| `-r`, `--reconfigure` | Tell the running compositor to reload its configuration |
| `-s`, `--startup <command>` | Run a command on startup |
| `-S`, `--session <command>` | Run a command on startup and exit when it exits |
| `-t`, `--title <fmtstr>` | The window title to use when running nested inside another compositor; the default is `kdos-comp - %o` |
| `-v`, `--version` | Show the version and quit |
| `-V`, `--verbose` | Informational logging, which is already the default here; after `-d` it lowers the level back |

`-r` and `-e` find the running compositor through `LABWC_PID`, which `kdos-comp` sets in the
environment of everything it starts. From a shell that the compositor did not start (a text
console, or an SSH login) they fail with `LABWC_PID not set`; send `SIGHUP` or `SIGTERM` to the
process instead. `kdos theme` reloads the compositor by sending `SIGHUP` directly.

## Configuration

Configuration is split between two files, and the split is strict:

- `~/.config/kdos/comp.conf` holds only the KDOS keys listed below;
- `~/.config/kdos-comp/rc.xml` holds everything labwc understands: bindings, startup commands,
  workspaces, mouse behaviour, window rules and theme settings.

`comp.conf` is one `key = value` per line, parsed and never executed as a script. A line whose first
non-blank character is `#` is a comment. The shipped file has every key commented out at its
default, with an explanation beside each, so the defaults below are what an unedited machine runs
with. The same keys are listed in
[Configuration](../06-reference/configuration.md#configkdoscompconf), and most can be changed from
the settings window.

Every line that does not take effect is written to the log with its file name and line number:

- a line with no `=`;
- a key with an empty value (write `wallpaper = none` rather than `wallpaper =`);
- an unknown key;
- a value out of range, or of the wrong kind, in which case the default stands;
- a line starting with `bind`, `startup`, `workspaces` or `mouse`, which belongs in `rc.xml`;
- a `panel_bottom` line, for which the log names `panel = bottom|top|off` as the key to use.

A setting that silently did nothing could not be told from a typo, so check the log when a change
seems to be ignored.

A path value may start with `~/` or `$HOME/`; both are expanded. A yes/no value accepts `yes`/`no`,
`true`/`false`, `on`/`off` and `1`/`0`, in any case.

### Keys applied on reload

These take effect when the compositor reloads, which happens on `SIGHUP`, `kdos-comp -r`, the
**Reload Configuration** entry in the desktop's right-click menu, and every `kdos theme`.

| Key | Default | Range | Does |
|---|---|---|---|
| `wallpaper` | `/usr/share/backgrounds/kdos/default-wallpaper.png` | a PNG path, or `none` | The wallpaper; see [The wallpaper](#the-wallpaper) for which file wins |
| `crt` | `55` | 0–100 | Strength of [the phosphor pass](#the-phosphor-pass), per cent. `0` turns the pass off |
| `crt_scanlines` | `0` | 0–100 | Scanline depth; `60` is the strength the rest of the pass is tuned against |
| `crt_curve` | `0` | 0–100 | Barrel distortion |
| `crt_fullscreen` | `yes` | yes/no | Whether the pass runs over a fullscreen window; `no` lets that window's frames [scan out directly](#fullscreen-scanout-variable-refresh-and-tearing) |
| `idle_dim` | `300` | 0–86400 s | Seconds of inactivity before the screen dims; `0` is never |
| `idle_lock` | `600` | 0–86400 s | Seconds before the session locks; `0` is never |
| `idle_off` | `900` | 0–86400 s | Seconds before the screens are powered off; `0` is never |
| `lid_close` | `suspend` | `suspend`, `lock`, `off` | What closing a laptop lid does; any other value is refused by name |
| `window_memory` | `yes` | yes/no | Whether an application opens where its window last was |
| `motion` | `yes` | yes/no | The desktop's reduce-motion switch; `no` makes every compositor fade a single frame. See [Motion](#motion) |
| `window_motion` | `no` | yes/no | [Window transitions](#window-transitions); only with `motion = yes` |
| `max_render_time` | `off` | 1–100 ms, or `off` | [Render-late scheduling](#render-late-scheduling): how long before the vertical blank each frame is composited |
| `sticky_keys` | `no` | yes/no | [Sticky keys](#the-keyboard-aids) |
| `slow_keys` | `no` | yes/no | [Slow keys](#the-keyboard-aids) |
| `slow_keys_delay` | `300` | 100–5000 ms | How long a key must be held before slow keys accepts it |
| `bounce_keys` | `no` | yes/no | [Bounce keys](#the-keyboard-aids) |
| `bounce_keys_delay` | `300` | 100–5000 ms | How soon after its release a second press of the same key is ignored |
| `dwell_click` | `no` | yes/no | [Dwell click](#dwell-click) |
| `dwell_click_delay` | `1200` | 100–5000 ms | How long the pointer rests before the click |
| `cursor_size` | `0` | 0–256 px | The [pointer size](#the-pointer-size); `0` keeps `XCURSOR_SIZE` from the session |
| `large_cursor` | `no` | yes/no | Whether the pointer is drawn at `large_cursor_size` |
| `large_cursor_size` | `48` | 16–256 px | The size `large_cursor` and `ToggleLargeCursor` switch to |

Two limits apply to the phosphor keys, because two decisions about the pass are made once, when
the compositor starts:

- If `crt` is `0` at login, or the renderer cannot run the pass, the pass is never set up. Raising
  `crt` afterwards takes effect at the next login, and the reload logs that. Lowering it to `0`
  takes effect at once: frames go out unprocessed, and a fullscreen window may scan out.
- A non-zero `crt_curve` at login switches the session to a software cursor, because the hardware
  cursor plane is drawn after the shader and would not follow the distortion. Turning curvature on
  during a session leaves the hardware cursor in place, and the pointer drifts away from what it
  points at towards the screen's edges until the next login.

In a virtual machine the idle timers and the lid default to off unless `comp.conf` sets them; see
[Idle, dim, lock and lid](#idle-dim-lock-and-lid).

### Keys applied at the next login

Each of these becomes part of a [supervised child](#supervised-children)'s command line, so a change
takes effect when the session next starts. A reload logs the change, for example `comp.conf:
panel_font changed — applies at the next login`, and keeps the running value until then, so the
compositor never believes a setting that the panel on screen is not using.

| Key | Default | Range | Does |
|---|---|---|---|
| `panel` | `bottom` | `bottom`, `top`, `off` (or `none`) | Where the panel goes, or no panel |
| `panel_cells` | `2` | 1–4 | Panel height in text cells |
| `panel_font` | `Terminus:pixelsize=20` | a fontconfig pattern | The bar's own font; it does not follow `chrome_font` |
| `panel_autohide` | `no` | yes/no | Whether the bar hides when the pointer leaves it |
| `panel_margin` | `0` | 0–64 px | Gap between the bar and the screen edge |
| `panel_opacity` | `80` | 20–100 % | Opacity of the bar's background |
| `desktop_icons` | `yes` | yes/no | Whether the desktop icon surface runs |
| `slit` | `no` | yes/no | The dockapp column (the slit): one line of text per *dockapp*, a command re-run on an interval, as set in `~/.config/kdos/slit.conf` |
| `clipboard` | `yes` | yes/no | The clipboard history daemon |
| `icons` | `yes` | yes/no | Whether the panel and the desktop draw pictures at all |
| `chrome_font` | `Terminus:pixelsize=32` | a fontconfig pattern | The font of every [supervised child](#supervised-children) except the panel: the desktop icons, the slit and the session daemons. The panel does not pass it on to the menus and popups it opens, which draw in `libkwl`'s default |
| `clock_format` | `%H:%M` | a `strftime` format | The panel clock |
| `osk` | `off` | `off`, `manual`, `auto` | [The on-screen keyboard](#the-on-screen-keyboard): not started, started hidden, or also shown while a text field has the focus |

`chrome_font` and `clock_format` are empty in the compositor when unset, and the defaults shown are
what the receiving programs use in that case: the `Terminus:pixelsize=32` of `libkwl` (the
Wayland drawing library, see [The C libraries](../05-developer/c-libraries.md#the-set)) and the
panel's `%H:%M`.

`panel_opacity` stops at 20 rather than 0. A bar at zero would not be see-through; it would be
invisible while still catching the pointer, with no way back except editing this file from a text
console.

The panel's height follows its font. A cell is half as wide as it is tall, so
`Terminus:pixelsize=20` with two cells makes a 40-pixel bar, while the menus it opens stay at the
32-pixel chrome font. Terminus is a bitmap face, so name a size it has (12, 14, 16, 18, 20, 22, 24,
28 or 32 pixels); any other size is answered with the nearest one it has.

The font page of `kdos-style` also writes both font keys. It changes only the family, keeps the
size each key already carried and leaves every other line of `comp.conf` as it was.
`kdos-settings` edits the two keys as plain text, so the value typed there, size included, is the
value written. Surfaces that read `comp.conf` when they start, such as a menu opened after
the change, show the new family at once; the supervised children receive their font from the
compositor, which keeps the value it read at login.

### Files whose existence is the setting

Two files ship absent, and absent is a working default for both. The compositor does not read them;
they are listed here because they sit beside `comp.conf`.

| File | Enables | Read by |
|---|---|---|
| `~/.config/kdos/session-restore` | Reopening the previous session's applications at login | The session scripts |
| `~/.config/kdos/a11y` | The accessibility stack inside boxes | `kdos-appbox` |

`~/.config/kdos/favorites` has a similar role but ships with seven pinned entries, because an empty
list leaves the Start menu's pinned column and the palette's pinned rows blank on a new machine.
Delete every line if you want it empty. The panel's quick-launch row reads the whole line as the
identifier, and every shipped line carries a `code=`, so on a new account that row is empty; see
[Configuration](../06-reference/configuration.md#configkdosfavorites). An identifier with no
matching desktop entry is skipped silently, so an application that is not installed leaves no
launcher that opens nothing.

## Bindings

The shipped bindings are listed in [The desktop](../02-user-guide/desktop.md), and `Super+F1` shows
a card generated from your own `rc.xml`. Two rules about editing `rc.xml` matter more than any
single binding, because breaking either one breaks the whole desktop without any message.

### The one line that must not be lost

`<default />` must be the first child of both `<keyboard>` and `<mouse>` in `rc.xml`:

```xml
<keyboard>
  <default />
  <keybind key="W-Return"><action name="Execute" command="foot"/></keybind>
</keyboard>
```

The compositor loads its built-in bindings only when your file defines none of that kind, so a file
that binds a single key throws every default away: click-to-focus, dragging by the title bar, the
window buttons, resizing by the border, the root menu, window cycling, close and the snap arrows.
On a running system the symptom is that the mouse appears not to work, and nothing at run time
warns about it. Put your own bindings after `<default />`: when a key is bound twice, the later
binding wins.

`testing/preflight.sh` fails a shipped `rc.xml` that binds anything in `<keyboard>` or `<mouse>`
without `<default />` in that section. It does not see the copy in your home directory.

### `--` may not appear inside an XML comment

`rc.xml` is XML, and XML forbids `--` inside a `<!-- -->` comment. A comment that mentions a
command-line option such as `--app-id` makes the whole file invalid, and a compositor that cannot
parse its configuration loads none of the bindings in it. Nothing says so; the chords are missing,
one by one, in whatever order you happen to try them. `testing/preflight.sh` parses the shipped
file with a real XML parser for this reason. Run `xmllint --noout rc.xml` after editing yours.

### KDOS actions

Besides labwc's actions, the fork adds nine, usable in any binding or menu and through
`kdos hey run`. Six are the [accessibility](#accessibility) switches, bound in the shipped `rc.xml`
beside labwc's magnifier actions:

| Action | Shipped binding | Does |
|---|---|---|
| `ToggleStickyKeys` | `Super+Alt+S` | Sticky keys on or off for the session |
| `ToggleSlowKeys` | `Super+Alt+L` | Slow keys on or off |
| `ToggleBounceKeys` | `Super+Alt+B` | Bounce keys on or off |
| `ToggleDwellClick` | `Super+Alt+D` | Dwell click on or off |
| `ToggleLargeCursor` | `Super+Alt+C` | Switch the pointer between its size and `large_cursor_size` |
| `ToggleOnScreenKeyboard` | `Super+Alt+K` | Show or hide the on-screen keyboard; with `osk = off` a notification says so |
| `ToggleMagnify` (labwc's) | `Super+=` | The magnifier on or off |
| `ZoomIn`, `ZoomOut` (labwc's) | `Super+Alt+=`, `Super+Alt+-` | Magnify more or less |

Each of the six switches posts a notification saying which way it went, such as `Sticky keys on`. A
keyboard aid switched on by accident otherwise leaves a keyboard that misbehaves with nothing on the
screen to say why. A switch lasts for the session. A reload changes it only when the value
`comp.conf` gives it has changed since the last load, because every `kdos theme` is a reload and an
accent switch must not undo a keyboard aid somebody has just switched on.

The other three are for [window groups](#window-groups-and-window-memory): `AddToTabGroup`,
`RemoveFromTabGroup` and `NextInTabGroup`. The shipped `rc.xml` does not bind them. To reach them,
add bindings after `<default />`; `Super+g`, `Super+Ctrl+g` and `Super+Alt+g` are free in the
shipped file:

```xml
<keybind key="W-g"><action name="AddToTabGroup"/></keybind>
<keybind key="W-C-g"><action name="RemoveFromTabGroup"/></keybind>
<keybind key="W-A-g"><action name="NextInTabGroup"/></keybind>
```

### The app-first window switcher

The window switcher accepts a third on-screen style, `apps`, beside labwc's `classic` and
`thumbnail`. It shows one row per application rather than one per window, with a count when an
application has several windows (`Firefox ×4`). `Tab`, `Left` and `Right` step between
applications; `Up` and `Down` step between the windows of the selected application, and the
selected row shows the title of the window that would be focused. Releasing the modifier focuses
that window. Windows are grouped by application identifier, and a window with none is a group of
its own.

The shipped `rc.xml` leaves the switcher at labwc's default, `classic`. To use the application
style:

```xml
<windowSwitcher>
  <osd style="apps" />
</windowSwitcher>
```

The deprecated attribute form, `<windowSwitcher style="…">`, does not accept `apps`.

## Decorations

The window frame is generated from the same palette as everything else, into `themerc-override`,
which the compositor reads over its built-in theme. Frames therefore retint live with the panel and
the phosphor pass. `kdos theme` rewrites that file whole on every run, so edits made to it by hand
are lost; lines that must survive belong in a style file (`kdos theme style`, see
[The kdos command](kdos-command.md#kdos-theme)), which keeps them in `~/.config/kdos/style-themerc`
and appends them after the generated block. The reasoning behind the frame's look is in
[the design language](../03-architecture/design-language.md); the mechanics are these:

- `<cornerRadius>0</cornerRadius>` in `rc.xml`, and `border.width: 2` in the generated override.
  A one-pixel hairline disappears beside a 32-pixel text cell.
- The title bar carries the same double rule the cell grid draws. The generated theme selects it
  with `window.active.title.bg: flat kdos` (and the same for `inactive`), a fill value the fork
  adds. The title-bar fill is one pixel wide and stretched, so anything that varies only
  vertically costs nothing, and a double horizontal rule varies only vertically. Its edges are
  hard steps, not gradients.
- The title text and every button sit on the plain background instead. A rule behind a word looks
  like a word struck through, and a rule behind the minimise button, itself a horizontal line,
  would leave the button unreadable.
- Button glyphs are eight-by-eight bitmaps, enlarged by a whole number with nearest-neighbour
  filtering. Upstream's resize path only shrinks and upstream's glyphs are six pixels square, so
  without this change they would sit at their own few pixels in the middle of a 32-pixel button.
- The hover highlight is translucent (`window.button.hover.bg.color` with alpha `66`). There are
  no separate hover icons: the plain image is copied and a colour laid over it, so an opaque colour
  would paint the symbol out and leave every button blank under the pointer.

The title-bar font must name a scalable face. Pango, which draws the titles, does not render bitmap
fonts, so naming the bitmap console font silently falls back to a generic sans for every title bar
and menu. The shipped `rc.xml` names `Terminus (TTF)` at 24 points (32 pixels at 96 dpi) for the
title bars, the menus and the on-screen display, so a title bar is exactly one cell tall. The
face comes from the `terminus-ttf` port. A machine without it falls back to Noto Sans, which the
`noto-fonts` port's preference file puts first for `sans-serif`.

To tell the two apart, measure rather than look: count the brightness levels of the text in a
screenshot. Bitmap text has three and no midtones; an antialiased face has well over a hundred.

The generated theme sets `menu.width.max: 900`. The upstream default is sized for a small font,
and at 32 pixels it truncates menu entries at about eleven characters. A maximum only ever
truncates, so a generous one costs a short menu nothing.

## The prompt command

`<core><promptCommand>` in `rc.xml` is:

```sh
kdos-prompt --message '%m' --no '%n' --yes '%y'
```

labwc's conditional action (`If` with a `<prompt>`) runs the prompt command and acts on its exit
status: `0` takes the `then` branch, `254` means cancelled, and anything else takes the `else`
branch. Upstream's own prompt program, `labnag`, is not built. `kdos-prompt` is `kdos-shell` under
another name and answers with the same exit codes. It is what lets ending the session
(`Super+Escape`), restarting and shutting down ask before they act.

Upstream runs this one command through `/bin/sh -c`. The fork splits it into an argument vector
with glib's shell-word parser and executes it directly, so the quoting in the default command
works, but variables, globbing and other shell expansion do not. The same holds for every
`Execute` action, as it does upstream.

## Frame pacing and the output mode

Nothing in the compositor sets a frame rate. It draws only when an output's backend reports that
the screen is ready for another frame; the handler composites once and returns. There is no fixed
period, so the rate is the display mode's: a 144 Hz panel gets 144 frames a second for the same
reason a 60 Hz one gets 60. An output with nothing to redraw skips the frame, which is why an idle
desktop costs almost nothing. The one timer in the loop is optional:
[render-late scheduling](#render-late-scheduling) moves the composite later within the same
refresh, and never adds a frame.

Two rate limits exist, and neither holds the frame rate back. Interactive resizing sends a window
at most one new size per refresh interval of its output (250 per second when the output reports no
refresh rate). The shutdown animation steps every 16 ms, after the event loop has already stopped.

### Choosing the mode

The resolution comes first and then the highest refresh rate that works:

1. The panel's preferred mode (its EDID preferred timing) fixes the resolution.
2. Every mode at that resolution with a higher refresh rate is tried, fastest first.
3. The preferred mode itself is tried.
4. The first mode that passes its test is used. If none passes, every other mode the output lists
   is tried, in the order the output lists them. If none of those passes either, the output is
   committed with no fixed mode.

Taking the preferred mode's own rate would leave many machines below their panel's rate: panels
commonly advertise 60 Hz as preferred and list 120 or 144 Hz elsewhere, and the session would sit at
60 Hz with nothing on the desktop offering a choice. Trying fastest first is safe, because a rate
the cable or link cannot carry fails its test and the next one down is tried.

Two things override this. A mode a program requests through the output-management protocol (for
example `kdos-display`) is tried exactly as asked, which is how you pin a rate. And
`reuseOutputMode` in `rc.xml` keeps a mode that is already set ahead of any of this, which stops a
handover from re-setting the mode of a screen that is already working.

### Fullscreen: scanout, variable refresh and tearing

A frame can skip compositing altogether. When the only thing visible on an output is one window's
buffer and the display controller accepts that buffer as it is, wlroots hands it to the display
instead of drawing it into a buffer of the compositor's. This is *direct scanout*, and it is
decided again on every frame of every output. It is allowed on a frame the phosphor pass leaves
alone: `crt = 0`, a renderer that cannot run the pass, a pass in its failure cooldown, and, the case
it is for, a fullscreen window under `crt_fullscreen = no`. A frame the pass draws never scans out,
because the pass needs a picture of the whole desktop (see
[How it is implemented](#how-it-is-implemented)). Nor does a frame while the magnifier is on,
because the magnified inset is drawn into the composited buffer. Setting
`WLR_SCENE_DISABLE_DIRECT_SCANOUT=1` in the session's environment turns it off for the session.

wlroots still refuses a frame it cannot scan out, and composites it as usual:

- a software cursor visible on that output. A virtual machine's virtio display forces one, and so
  does a non-zero `crt_curve` at login, so with the curve on, a fullscreen window scans out only
  while the pointer is hidden or on another screen;
- anything else visible over the window, such as a notification or the on-screen keyboard;
- a buffer whose rotation, colour description or format the display cannot take, or a test commit
  that fails;
- night light on an output that has no hardware gamma table, since the correction is then drawn.

The shipped `rc.xml` sets two upstream options in `<core>` for fullscreen windows:

- **`<adaptiveSync>fullscreen</adaptiveSync>`** turns variable refresh on while a window is
  fullscreen, on an output that reports it, and off again when none is. The desktop itself never
  runs at a varying rate.
- **`<allowTearing>fullscreen</allowTearing>`** lets a fullscreen window that asks to tear (through
  the tearing-control protocol, which games send) have its frames flipped without waiting for the
  vertical blank. A window that does not ask is unaffected, and `fullscreenForced` would tear every
  fullscreen window. Only a frame that goes the plain way can tear. With `crt_fullscreen = yes`,
  the default, a fullscreen window's frames go through the pass and flip on the vertical blank.

A display or driver that supports neither refuses the request, and the output keeps its fixed rate.
To see whether a frame scanned out, run the session with `KDOS_COMP_DEBUG=1`: wlroots logs
`Direct scan-out enabled` and `disabled` on each change. The frame's cost shows as `render_ms` in
[`kdos stutter`](kdos-command.md#kdos-stutter). The test rig cannot show either, because its virtio
display forces a software cursor.

### Render-late scheduling

By default a frame is composited the moment the output's frame event arrives. On a directly driven
display that is just after the previous frame was shown, so the finished frame then waits in the
display for most of a refresh, and a program's buffer that arrives a moment after the composite
waits a whole refresh more. `max_render_time = <ms>` in `comp.conf` holds the composite back until
that many milliseconds before the next vertical blank, predicted from the last presentation and
the refresh the display reports, so whatever programs commit in the meantime is in it. At 60 Hz
that brings such a commit to the screen up to one refresh, about 16 ms, sooner. This is an
estimate: the test rig has no display with a vertical blank, so it has not been measured.

The budget is yours to size, and it is off by default for that reason. It must cover the composite
and [the phosphor pass](#the-phosphor-pass) on the machine's GPU. The compositor cannot measure
that: the `render_ms` it reports to [`kdos stutter`](kdos-command.md#kdos-stutter) is processor time
up to the point the GL commands are submitted, not the time the GPU takes to finish them. Too small
a budget misses the blank, and the frame is shown one refresh late, which `kdos stutter` reports
as a miss. Start at half the refresh interval (8 at 60 Hz) and lower it while `kdos stutter` stays
quiet.

The frame is composited at once, as with the key off, whenever the moment cannot be predicted:

- on an output that is not driven directly, such as a nested window or a virtual output, whose
  frame events are not the display's blanks;
- when nothing was presented within the last refresh, which is the first frame after the desktop
  was idle, because a prediction from an old timestamp drifts;
- when the display reports no refresh rate;
- while variable refresh is on, because the blank then follows the commit;
- for a frame that may tear (see
  [Fullscreen: scanout, variable refresh and tearing](#fullscreen-scanout-variable-refresh-and-tearing));
- when the wait would be under a millisecond, the timer's resolution.

While the timer runs, the output reports a frame as already pending, so a program's damage does
not start a second frame in the middle of the wait; it is picked up by the composite the timer
starts. A commit that reaches the output some other way during the wait, such as a mode change or
variable refresh switched on for a fullscreen window, cancels the timer, and that commit's own frame
event decides again; one that switches the output off lowers the pending flag the wait was
holding. The key is read on every frame, so a reload applies it at once.

### When an output appears

Each new output gets its own panel, desktop icons and dockapp column
(see [Supervised children](#supervised-children)), and the compositor runs
`kdos-display --apply`, which re-applies the layout saved in `~/.config/kdos/displays.conf`. The
apply is delayed by one second and restarted by each further output, so a dock that brings up three
screens causes one apply, not three.

## The phosphor pass

The compositor draws the whole desktop through a shader that imitates a CRT: optional scanlines on
every third physical row, a three-tap horizontal bleed, a vignette, optional barrel distortion, and
a faint phosphor floor in the accent colour so that black is never quite black. It is on by default
at 55 per cent, with scanlines and curvature off. The user's guide is
[Theming](../02-user-guide/theming.md#the-phosphor-pass).

Scanlines ship off because they fight the text underneath. The desktop is a grid of 16×32 cells,
and a dark line on every third physical row crosses the glyphs at a period nothing on screen
shares, so crisp two-colour text arrives striped. `crt_scanlines = 60` turns them on.

The pass needs the GLES2 renderer. On software rendering, including a virtual machine with plain
graphics, it is switched off at startup and the log says so.

Two short animations belong to the pass and run only when it is active and `motion` is on (see
[Motion](#motion)):

- **The degauss.** Every reload, and therefore every theme change, plays 400 ms of decaying
  horizontal wobble with a slight brightening, the way a CRT's degauss coil shook the picture. The
  compositor forces frames while it runs, so it plays on a static desktop too.
- **The power-down.** When the compositor exits, the last picture collapses to a bright horizontal
  line over about 350 ms and then to a dot over about 100 ms, with a hard 600 ms deadline so that
  it can never hold up a shutdown. Up to eight outputs take part.

### How it is implemented

wlroots, the library under the compositor, has no shader interface: its rendering pass offers
textures and rectangles, and its scene graph (the tree of buffers the compositor composites into
each frame, called the *scene* below) has no callback node. What it does offer is a
documented seam: the scene's build-state call accepts a custom swapchain. So the scene composites
into a buffer of the compositor's own, and KDOS code draws that buffer into the output's real
buffer with the effect applied. Both swapchains come from the library's own configuration call, so
neither needs guesswork about formats or modifiers.

The pass redraws only what changed whenever it can. With `crt_curve` at `0` and no degauss running,
an output pixel depends on the scene pixel under it and the ones beside it on its row, so the
scene's damage for the frame, grown by two columns and one row, contains what changed on screen.
Each output keeps a *buffer-age ring* over its output buffers: for every buffer, what has changed
since that buffer last held a picture. A frame redraws that region of the buffer it is handed, one
scissored draw per rectangle (the region's bounding box past 16), and commits with the frame's own
damage, which a display that supports damage clips can use to refresh only a strip. The whole
output is redrawn, and committed as damage, instead when:

- `crt_curve` is above `0`, because the curve moves every pixel;
- the degauss is playing, and on the first frame after it ends;
- any of the pass's settings or the phosphor colour changed since the last committed frame;
- the screen does not hold the pass's last frame: none has gone out on this output yet, the last
  one failed to commit, or anything other than the pass committed a frame since — the magnifier,
  `crt_fullscreen`'s bypass, a cooldown or a scanout buffer. Such a commit consumes the scene's
  damage record, so the pass cannot tell what changed under its own buffers, and it leaves a
  different picture at every pixel, which a display refreshing only the damage clips would keep.
  The first frame back on the pass is drawn even when nothing on the desktop moved, or the
  unprocessed picture would stay up until something did.

A buffer the pass has not drawn into before, after a mode change for example, is redrawn whole, but
its commit carries only the frame's damage: the screen already holds the previous frame.

The power-down always draws the whole output. The shader and the region code live in
`kdos-crt-pass.c`, apart from everything that touches wlroots, so that `testing/selftest.sh` can run
them offscreen (see [Working on the compositor](#working-on-the-compositor)).

Five constraints shape it:

- **Direct scanout is off for every frame the pass builds.** In that mode the scene hands the
  display one client's buffer and a rectangle rather than a picture of the desktop, and the pass
  would stretch, say, the panel over the whole screen. wlroots has one switch for it that covers
  every output, a field of the scene, so the compositor writes it on every frame of every output:
  off before the pass builds, and allowed before a frame goes the plain way (the magnifier, a
  cooldown, `crt_fullscreen`'s bypass, `crt = 0`). A foreign buffer that reaches the pass anyway
  is shown unprocessed rather than mangled, and the log says so once per output.
- **The texture is imported every frame and destroyed after the pass.** Caching one per swapchain
  slot looks like an easy optimisation and deadlocks the swapchain: importing locks the buffer, a
  slot is reused only when its last lock goes, and a full set of cached textures leaves no free
  buffer, so the scene stops rendering.
- **Neither fallback can produce a black screen.** A renderer other than GLES2 gets no pass at all,
  reported at startup, because a full-screen post-process in software cannot keep up with the
  refresh rate. Anything that fails at run time puts that output on the plain path for a cooldown:
  five seconds, doubling after each consecutive failure up to a minute, and reset by a pass that
  completes. Sixty identical error lines a second would be worse than a missing effect, and giving
  up for good would leave a screen unthemed for the whole session over one transient failure such
  as a hot-plug.
- **labwc's magnifier (the `ToggleMagnify`, `ZoomIn` and `ZoomOut` actions) takes the frame instead,
  whole.** The magnified inset is drawn inside the call the pass replaces, so with both on, a
  magnified frame would lose the inset whenever the scene redrew and keep it whenever it did not,
  flickering between two pictures. The pass stands aside while the magnifier is on, which is also
  right for its own sake: an accessibility zoom is harder to read through scanlines.
- **Night light survives the pass.** The scene applies the gamma-control protocol as a colour
  transform in the state it builds, and the pass carries that transform onto its own commit.
  Dropping it would make night light do nothing while the pass is on.

The curvature is normalised by how far the corners move, so no setting crops the desktop.

The phosphor colour is the accent's primary colour from the shared palette, read from
`$XDG_CACHE_HOME/kdos/theme` at startup and again on every reload, so a theme change retints the
running shader with the same signal that repaints the panel.

### What it costs

The pass does not limit the frame rate: it runs inside the same frame event as the composite, once
per frame that has something to draw. Its costs are these:

- one extra draw per frame, with three texture reads per pixel for the bleed, over what changed
  since the buffer being drawn last held a picture: a typed character or a clock tick costs a
  strip, and with `crt_curve` above `0` every frame costs the whole screen;
- a second swapchain per output (at 3840×2160 each buffer in it is about 33 MB);
- no direct scanout while the pass runs, so a fullscreen video is composited and then passed over,
  rather than handed straight to the display controller.

`crt_fullscreen = no` skips the pass on an output whose topmost window on the current workspace is
fullscreen, and that frame may [scan out](#fullscreen-scanout-variable-refresh-and-tearing): no
composite at all where the display accepts the window's buffer, one instead of two where it does
not. At 1920×1080 each full-screen pass reads and writes about 16 MB, so that is roughly 16–33 MB
less memory traffic per frame (an estimate, not measured on hardware). `crt = 0` removes the extra
draw everywhere.

### Inspecting it without a screen

`KDOS_CRT_DUMP=<prefix>` writes the pass's input and output once, as `<prefix>-in.ppm` and
`<prefix>-out.ppm`. `KDOS_CRT_DUMP_FRAME=<n>` waits until frame *n* before dumping, to get past the
empty first frames. The input is read back through the texture the shader sampled rather than from
the buffer, because that buffer is not always readable; when the input is an external image it
cannot be read back at all, and only the output is written.

One property only real hardware can confirm. The composite and the pass run in wlroots' one GL
context, so their order is guaranteed, but the final display commit relies on implicit buffer
fencing. Getting that wrong shows up as a torn frame, not as an error.

## The wallpaper

The compositor draws the wallpaper itself: one scene buffer per output at the bottom of the scene,
rebuilt when outputs are added, removed or rearranged. A separate wallpaper program, the usual
answer on Wayland, would be the one program on this desktop that is not a character grid.

Which image is drawn:

1. With `wallpaper = none`, no image.
2. Otherwise, `$XDG_CACHE_HOME/kdos/wallpaper.png` (`~/.cache/kdos/wallpaper.png`) when it exists.
   `kdos theme` writes this file: the shipped wallpaper retinted to the accent.
3. Otherwise, the `wallpaper =` path from `comp.conf`.

Under every output, image or no image, lies a rectangle in the accent's deep colour, so
`wallpaper = none` and a missing or unreadable file both show that colour; the log names the path
that failed. Because the retinted cache takes precedence, a picture of your own is replaced at the
next accent change; [Theming](../02-user-guide/theming.md#wallpaper) explains how to keep one. The
image is scaled to cover the output and centred, so an image of another aspect ratio is cropped
rather than distorted. Only PNG is decoded.

A reload re-decodes the image only when the chosen file or its modification time has changed, and
redraws the deep-colour rectangles every time, since the accent may have moved.

`kdos-wallpaper.c` implements its own minimal wlroots buffer, because wlroots has no public way to
make a buffer from memory. The decoder converts the PNG's channel order to the one the
buffer needs, and only a fully initialised buffer can leave it, because libpng reports a bad file
by jumping back after the allocation.

## Idle, dim, lock and lid

One timer drives three stages (dim, then lock, then screens off), each measured from your last input
rather than from the previous stage. A stage set to `0` is skipped, so `idle_off` alone works. Ten
seconds before the lock, a notification says `Locking soon`, and any input keeps the session open.
Input ends the dim and powers the screens back on; it never unlocks. A screen that fails to power
back on is retried at the next input.

The idle policy stops completely while any program holds an idle inhibitor (a video player, for
example) or while the `stay-awake` toggle is on (`kdos toggle stay-awake`, bound to
`Super+Ctrl+i`). The toggle is read every time the timer is re-armed, so it takes effect without a
reload. An inhibitor is either the Wayland idle-inhibit protocol, which a native client such as mpv
uses, or a cookie from `org.freedesktop.ScreenSaver`'s `Inhibit`, which `kdos-comp` serves on the
session bus at `/ScreenSaver` and `/org/freedesktop/ScreenSaver` for X11 clients under Xwayland,
such as VLC. A program that leaves the bus without calling `UnInhibit` has its cookies returned.

The dim is a black layer at 55 per cent opacity raised over everything, with the lock screen raised
above it; it does not touch gamma, which some backends cannot set and which would stay applied if
the compositor died. The lock stage starts `kdos-lock` (see [The daemons](daemons.md#kdos-lock)),
unless the session is already locked.

### In a virtual machine

The three timers default to zero (never) and `lid_close` to `off`, unless `comp.conf` sets them. A
blanked screen over a remote display cannot be told from a crashed compositor. The machine counts as
virtual when the firmware names a hypervisor, and also when the compositor runs nested or headless
with no seat session. Setting any one of the three timers overrides the virtual-machine default for
all three, so `idle_dim = 0` alone leaves the lock and screen-off timers at their defaults of 600
and 900 seconds. Only a line that parses counts: `idle_dim = 5m` is refused and leaves the
virtual-machine default in place.

### The lid

No other program on KDOS watches the lid switch; the compositor receives it from libinput.
`lid_close = suspend` hands the machine to `kdos-power suspend`, which locks first; `lock` locks and
powers the screens off; `off` only powers the screens off, which suits a closed laptop driving an
external monitor. Opening the lid powers the screens back on and counts as input; it never unlocks.

### Who owns the locked state

The compositor, not the lock program, owns the locked state. If the lock program dies without
unlocking, the session stays locked: the lock surfaces keep covering every screen, and a new lock
program can take over from the one that died, which is the recovery a crash needs. A lock screen
that unlocked when it crashed would be the failure the lock protocol exists to prevent.

## The frames socket

The compositor writes one line of JSON per late frame to `$XDG_RUNTIME_DIR/kdos-frames.sock`. This
is what [`kdos stutter`](kdos-command.md#kdos-stutter) reads.

A client first receives a `hello` record naming the compositor and wlroots versions, with the
monotonic time and the session's frame and miss counts so far. After that it receives one `miss`
record per late frame:

```json
{"event":"miss","mono_ms":…,"wall_ms":…,"output":"eDP-1","source":"present","late_ms":…,"dropped":7,"render_ms":…,"refresh_hz":60.00}
```

- **`source`** says where the timing came from: `present` or `frame`. Presentation events carry
  when the picture actually reached the screen, and are used where the backend has them. Headless
  and nested backends do not report presentation, so the frame clock is the fallback. The two are
  labelled rather than averaged: a presentation gap is what you saw, and a frame gap is what the
  compositor was given.
- **A frame is late** when it arrives more than one and a half refresh intervals after the previous
  one. `late_ms` is the gap, and `dropped` is the number of whole intervals missed. A gap that
  follows a frame with nothing to draw is idleness, not lateness, and is not counted.
- **`render_ms`** is the compositor's own render cost for the frame. When it is a large fraction of
  the frame budget, the desktop itself was late, which is the one causal claim the tooling makes.
  It is timed from the start of the composite, so a [render-late](#render-late-scheduling) wait is
  not part of it, and it is processor time up to the GPU submission, not the GPU's own time.

The socket never slows the frame loop: both ends are non-blocking, and a reader that cannot keep up
loses lines. There is no history, so a reader that connects late has missed what happened. Each
miss is also written to the log at debug level.

This is deliberately not a Wayland protocol. It is a channel between two KDOS programs; a client
that wants its own timing has the standard presentation-time protocol.

## The command socket

Other KDOS programs ask the compositor questions over `$XDG_RUNTIME_DIR/kdos-cmd.sock`: one JSON
request line in, one JSON response line out, then the connection closes.
[`kdos hey`](kdos-command.md#kdos-hey) is the command-line front end.

| Request | Answers or does |
|---|---|
| `{"cmd":"list"}` | Every window: `id`, `app_id`, `title`, `workspace`, `x`, `y`, `w`, `h`, `pid`, `box`, `instance`, and the states `focused`, `minimized`, `maximized`, `fullscreen` and `shaded` |
| `{"cmd":"outputs"}` | Every output: `name`, `w`, `h`, `scale`, `x`, `y` and `enabled` |
| `{"cmd":"boxes"}` | The distinct boxes that currently have a window on screen |
| `{"cmd":"run","action":"Close","id":7}` | Runs a labwc action, on the window with that id when one is given |
| `{"cmd":"peek","on":true}` | Fades every window to reveal the desktop; `false` or no `on` restores them |
| `{"cmd":"thumb","app_id":"foot","w":64,"h":36}` | Writes a picture of that application's front window and answers with its path |

A success answers `{"ok":true,…}`. Every refusal is `{"ok":false,"err":"…"}` with a reason, never a
silent no-op.

A window's `id` is fixed for the life of the session and never reused, so an id handed back late
cannot reach the wrong window. `run` resolves the action name against the same table `rc.xml` is
parsed with, so it can do exactly what a key binding can. An action that needs an argument the
request cannot carry, such as `Execute`'s command or `SnapToEdge`'s direction, is refused.

The socket protects the compositor and the machine:

- It never blocks the frame loop. A client that connects and says nothing, or asks and never reads
  the answer, is dropped after 5 seconds idle, and at most 8 clients are connected at once.
- The peer's user id is checked, because the socket runs actions. This separates users, not boxes:
  a box runs as the same user and shares the runtime directory, so what a box may do is decided by
  the [sandbox allowlist](../03-architecture/session.md#granting-a-box-more-than-the-allowlist).
- A request line over 4096 bytes is refused rather than buffered.
- `thumb` always writes to `$XDG_RUNTIME_DIR/kdos-thumb.ppm`, overwritten each time; the path is
  never taken from the request, so the socket cannot be used to write files elsewhere.
- There is no history and no subscription. A program that wants a stream of events wants the
  frames socket.

`kdos-box gc` uses `boxes` to ask whether a box still has a window before stopping it.

## Window thumbnails

A Wayland client cannot see another client's pixels, so the compositor renders a window to a file
on request (the `thumb` verb above). That is what the panel's hover preview is made of.

The compositor picks the most recently active window with the requested application identifier,
box-filters its buffer down to the requested size and writes a binary PPM. The size must be between
8×8 and 160×100 pixels; the file is small on purpose, because the consumer reduces it to a few dozen
text cells. A box filter rather than point sampling is used because the content is mostly text, and
a one-pixel stroke would rarely be the pixel a point sampler kept.

The panel draws the preview as solid blocks from the palette's brightness ladder rather than
through the shape-matching character renderer. A large window squeezed into a small grid puts many
source pixels in each cell, so no shape is left to match and every textured cell would pick the
same character.

The preview is never required. No compositor, no socket, a window whose pixels are not readable (a
client drawing into GPU memory, which is common on real hardware), a buffer in a format other than
32-bit ARGB or XRGB, or a file that does not parse: each one leaves the tooltip as the two lines of
text it always carries.

## Window groups and window memory

### Window groups

Window groups stack several windows into one frame with a tab per member, like tabs in a
browser: `AddToTabGroup` stacks the focused window onto the one behind it, `RemoveFromTabGroup`
takes it back out, and `NextInTabGroup` steps through the tabs. Clicking a tab raises that member.
The hidden members are minimised, so focus cycling and the panel's window list treat them
correctly, and all members share the showing member's position and size. Dragging a tab moves that
window; it does not take it out of the group. Use `RemoveFromTabGroup` for that.

### Window memory

Window memory (`window_memory = yes`) reopens an application where you left it. When a window
closes, its application identifier, geometry, workspace and shaded state are saved to
`$XDG_STATE_HOME/kdos/winpos` (`~/.local/state/kdos/winpos`); when a window of that application
next opens, the record is applied. The file keeps the 200 most recently used applications, and is
written to a temporary file, flushed, and renamed into place so that a crash cannot leave it half
written. The record applies only where the compositor would otherwise have chosen the position:

- a window that positions itself, is placed by a window rule, or opens maximised, tiled or
  fullscreen is left alone;
- dialogs and other windows with a parent are neither saved nor restored;
- a second window of an application already open at the remembered position on the same workspace
  is left to the ordinary cascade, rather than opened exactly on top of the first;
- a restored rectangle is clamped into the output's usable area, so a window never comes back
  off-screen or under the panel.

## Box identity

The compositor knows which box each window came from. A *box* is the container an application runs
in (see [Packs and boxes](../03-architecture/packs-and-boxes.md)). `kdos-boxsock` gives every box
its own Wayland socket and tags each client that connects through it with a security context
naming the box, and a second field distinguishing two runs of the same application.

An X11 window needs another route. Its Wayland client is Xwayland, running on the host with no
context, so the context lookup would answer nothing for every X11-only application in the
catalogue. The window does carry its client's process id, and every process a box starts has
`KDOS_BOX=<name>` in its environment, so the compositor reads that from `/proc/<pid>/environ` and
caches the answer.

### The box chip

The box chip is a small square in the box's colour at the left of the title. To give a box a
colour, set `accent = <name>` in its profile, `~/.config/kdos/boxes/<name>.conf` (see
[Configuration](../06-reference/configuration.md#configkdosboxesnameconf)):

- A box in the session's own accent draws no chip, so a default install has none, and the first chip
  appears when you give a box a colour to tell it apart by.
- The chip's width is added to the title's left offset in the one place where the title's width and
  position are both computed, so a long title never runs under the chip.
- It is rebuilt rather than patched on every title change and resize, so a drag never stacks chips.
- The profile is read once per window and re-read on reload, because an accent switch changes which
  windows wear a chip.
- On an inactive window the colour is drawn at a third of its strength: the chip identifies the
  box; it does not show focus.

### Grants

A client from a box may bind only a fixed allowlist of Wayland interfaces. A `grant = …` line in the
same profile adds named interfaces, such as `screencopy` for a screen recorder in its own box. The
grantable names are listed in [The
session](../03-architecture/session.md#granting-a-box-more-than-the-allowlist). A box's grants are
read once, on its first request, and re-read after a reload; a client that is already running keeps
what it bound.

## Accessibility

The compositor is where input arrives, so the aids that change what a key or the pointer does live
in it (`kdos-a11y.c`), and so does the interface a screen reader uses to hear the keyboard
(`kdos-a11ymon.c`). The user's side of all of this is in
[Accessibility](../02-user-guide/accessibility.md).

### The keyboard aids

| Aid | Key | What it does |
|---|---|---|
| Sticky keys | `sticky_keys` | A modifier pressed and released on its own applies to the next key; pressed twice it stays on until pressed a third time |
| Slow keys | `slow_keys`, `slow_keys_delay` | A key counts only once it has been held for the delay. Released sooner, it never happened |
| Bounce keys | `bounce_keys`, `bounce_keys_delay` | A second press of the same key within the delay after its release is dropped, with its release |

All three start off, take a changed value on a reload, and are flipped for the session by
[their actions](#kdos-actions). They share three rules:

- **They act on physical keyboards only.** A virtual keyboard (the on-screen keyboard, the input
  method re-sending a key it did not use, a remote-desktop server) is a program typing on purpose,
  and delaying or dropping its keys would break it.
- **Slow and bounce keys leave the modifiers and the lock keys alone.** wlroots updates the
  keyboard's modifier state after the compositor has seen a key, whatever the compositor does with
  it, so a dropped `Shift` press would still reach the focused window as a held `Shift`, and a
  dropped `Caps Lock` would still turn Caps Lock on. The keys passed through are `Shift`, `Ctrl`,
  `Alt`, `Super`, `AltGr`, `Caps Lock`, `Shift Lock` and `Num Lock`.
- **Sticky keys works through the keyboard's own state.** A latched modifier goes into xkb's latched
  mask and a locked one into its locked mask, on one member of the keyboard group, which wlroots
  copies to the rest. So the focused window and the compositor's own bindings both see it:
  `Super`, released, then `D` opens the launcher. A modifier held while another key is pressed is
  a chord and latches nothing. The mask is rewritten from an idle callback after the key, because
  wlroots updates the state after the compositor's key handler returns and would otherwise
  overwrite it.

Slow keys holds one key at a time. A second key pressed before the first has been held long enough
replaces it, and the first stays swallowed until it is released. A key it accepts reaches the
window stamped with the time it was accepted, not the time it went down.

### Dwell click

With `dwell_click` on, the pointer resting for `dwell_click_delay` clicks the left button where it
rests. Motion within 4 pixels of where the count started does not restart it, because a resting hand
still moves a mouse a pixel or two. After a click the pointer has to travel 16 pixels before it can
click again, so a pointer left alone clicks once. A real button press cancels the count, no dwell
click fires while a button is held, and none fires during an interactive move or resize, where a
click would end it. There is no visual countdown. The click goes through the same path as any
emulated button, so window bindings, menus and the root menu all answer it.

### The pointer size

`cursor_size` sets the pointer's size, and `large_cursor` (or `ToggleLargeCursor`) switches it to
`large_cursor_size`. The `KDOS-cursors` theme draws 24, 32, 48, 64 and 96 pixels. `cursor_size = 0`
keeps the size the session started with, which is `XCURSOR_SIZE` (24, from
`/etc/profile.d/10-wayland.sh`, or whatever `~/.config/kdos-comp/environment` sets). The chosen
size is written back into the compositor's own `XCURSOR_SIZE`, so a program started after the
change inherits it; one already running keeps the size it read. A change applies at once.

### The on-screen keyboard

`osk` decides whether the compositor runs `wvkbd-deskintl`, the on-screen keyboard, as a
[supervised child](#supervised-children). It is started with `--hidden` and shown and hidden by
signal: `SIGUSR2` shows it and `SIGUSR1` hides it.

| `osk` | Does |
|---|---|
| `off` | The keyboard is not started. `ToggleOnScreenKeyboard` posts a notification saying so |
| `manual` | Started hidden; `ToggleOnScreenKeyboard` (`Super+Alt+K`) shows and hides it |
| `auto` | As `manual`, and also shown whenever a text field has the keyboard focus and hidden when none does |

`auto` follows the text-input protocol: the keyboard is shown when a window's text field enables
text input and hidden 300 ms after the last one disables it, so moving from one field to the next
does not drop the keyboard and raise it again. A window is told about text input only while an
input method is running, so `auto` needs `fcitx5`, which the session starts (see
[Input methods](../03-architecture/session.md#input-methods)). An X11 window under Xwayland does not
speak the protocol, and `auto` never shows the keyboard for it.

`wvkbd` types through the virtual-keyboard protocol as an ordinary host program. It is not passed
`chrome_font`: it names its font with `-fn`, and handed `--font` it would exit with a usage error
at every start.

### The keyboard monitor

A Wayland window receives keys only while it has the focus, so a screen reader in a window of its
own can neither echo what is typed elsewhere nor answer its own shortcuts. The compositor therefore
owns `org.freedesktop.a11y.Manager` on the session bus and serves
`org.freedesktop.a11y.KeyboardMonitor` at `/org/freedesktop/a11y/Manager`, the interface
at-spi2-core's `AtspiDeviceA11yManager` uses and Orca reaches through it:

| Member | Does |
|---|---|
| `WatchKeyboard`, `UnwatchKeyboard` | Every key reaches the caller as a `KeyEvent` and the focused window as well |
| `GrabKeyboard`, `UngrabKeyboard` | Every key reaches the caller and nothing else |
| `SetKeyGrabs(au modifiers, a(uu) keystrokes)` | `modifiers` are keysyms the caller uses as its own modifier, such as the Orca key: each is grabbed, and so is every key pressed while one is held. Pressed twice within the key-repeat delay with no other key between, the second press and its release go through as an ordinary key, so Caps Lock as the Orca key still toggles Caps Lock. `keystrokes` are keysym and modifier-state pairs, grabbed when the state matches exactly |
| signal `KeyEvent(b released, u state, u keysym, u unichar, q keycode)` | Sent to each interested caller alone, never broadcast. `state` is the modifier mask before the key, `keysym` the first translated keysym, `keycode` the xkb code (the evdev code plus 8) |

A grabbed key reaches no window. That includes the lock it would toggle: xkb has already flipped
Caps Lock or Num Lock by the time the key is seen, so the flip is put back. A caller's grabs end when
it leaves the bus, because a reader that crashed never calls `UngrabKeyboard`.

Only a caller that owns one of the allowed well-known names may call; anything else is answered
`org.freedesktop.DBus.Error.AccessDenied`. The one allowed name is `org.gnome.Orca.KeyboardMonitor`,
which the client library requests for Orca before its first call. Boxes share the session bus (see
[The security model](../03-architecture/security-model.md)), and a program in a box could request
the same name while no reader holds it. The name check keeps an ordinary program from subscribing
to keystrokes by accident; it does not stop a hostile one. While the session is locked nothing is
sent and nothing is grabbed, because a password is typed there.

The monitor hears physical keyboards only, and it hears each key after the keyboard aids have had
it, so a reader announces what slow and bounce keys let through. The input method re-sends the keys
it does not use through a virtual keyboard, and passing those on as well would give a reader every
key twice; the price is that keys typed on the on-screen keyboard are not announced.

The compositor connects to the session bus when it starts. With no session bus it logs
`a11y monitor: no session bus` and runs without the monitor; if the bus goes away during the
session the monitor is switched off for the rest of it. The toggle notifications travel over the
same connection, to `org.freedesktop.Notifications`, and are not sent without it.

## Motion

The compositor fades three things, and a client takes no part in any of them. With
`window_motion` on it also moves windows (see [Window transitions](#window-transitions)). (The
phosphor pass's [degauss and power-down](#the-phosphor-pass) are its other two animations.)

| What | Fade | Length |
|---|---|---|
| A surface on the top or overlay layer mapping: a menu, a toast, a tooltip, an on-screen display, the panel at login | from transparent to opaque | 120 ms |
| The same surface unmapping | from where it was to transparent | 90 ms |
| [Peek](#smaller-changes-to-upstream-behaviour) starting or ending | to 12 per cent opacity, or back | 150 ms |

Each fade eases out: most of the change comes in the first frames, so a menu is legible from its
first frame and settles rather than arrives. The client commits its buffer once and the compositor
re-blends it at a rising or falling alpha, so the fade costs the client nothing and a surface that
knows nothing about motion fades as well. Background and bottom layer surfaces (the wallpaper, the
desktop icons) do not fade on map or unmap, and windows fade only with `window_motion` on.

A closing surface usually exits with its window, so its close fade is drawn from a snapshot: the
compositor copies the surface's last buffers into scene nodes of its own at unmap, keeps them alive
until the fade ends, and lets the pointer pass through them to whatever is beneath. The snapshot
goes with the output if the output is removed first.

A surface that took the keyboard exclusively gets no close fade and disappears at once. That is a
selection overlay such as the one `kdos-shot region` draws with `slurp`, and the screenshot taken
straight after it would otherwise contain the overlay fading out. Nor does a surface that was not
on screen when it closed, such as a top-layer surface under a fullscreen window, which hides the
top layer. A screenshot taken within 90 ms of a menu closing does contain the menu's fading
snapshot.

`motion = no` in `comp.conf` makes every fade a single frame: a surface appears and disappears in
one frame, peek jumps and windows make no transitions whatever `window_motion` says. It also skips
the degauss on a reload and the power-down at exit. It applies on the next fade after a reload; a
fade already running ends at once, at its end state. `motion` is the desktop's one reduce-motion
key.

What a fade costs: wlroots treats a buffer as opaque only while its opacity is exactly 1, so a fading
surface hides nothing and everything beneath it is drawn for the length of the fade. Each fade frame
redraws the surface's own area (and the phosphor pass redraws the same area); a fade always ends
on exactly 1, after which the surface occludes again. A fade is timed by the clock, not by frames,
so a dropped frame shortens a step rather than lengthening the fade.

### Window transitions

`window_motion = yes` in `comp.conf`, with `motion` on as well, gives windows short transitions.
It is off by default.

| What | Transition | Length |
|---|---|---|
| A window mapping, or coming back from being minimised | fades in from transparent while it rises 12 pixels into place | 150 ms |
| A window unmapping, or being minimised | fades out from where it was while it sinks 12 pixels | 120 ms |
| A workspace switch | the new workspace's windows fade in while sliding 48 pixels in from the side the switch goes towards; the old workspace's windows fade out while sliding 48 pixels the other way | 150 ms |

They use the same curve and clock as every other fade. A fullscreen window fades but does not
move, because its edge would pull away from the screen's edge. Windows shown on every workspace, and
a window being dragged across a workspace switch, take no part in it.

Every transition is a picture of a change that has already happened. The window is mapped,
closed, minimised or on the other workspace the moment the event arrives; focus has moved, and
keys and the pointer act on the new state at once. Nothing waits for a transition to end, so any
of them can be cut short by the next action, and a second transition on the same window starts
from wherever the first had got to. That holds for a window coming back while its own snapshot is
still leaving, as on a switch straight back to the workspace just left or a minimise undone at
once: the window takes over the snapshot's alpha and position and the snapshot goes, so it is
never drawn twice. While a window rises into place it takes the pointer where it
is drawn. A window that labwc places again part way through (window rules and window memory move a
window after it maps) carries on towards its new position.

A closing window is drawn from a snapshot, as a closing menu is: its buffers and its border
rectangles are copied into nodes of the compositor's own, the pointer passes through them, and
the copy is freed when the fade ends. The old workspace's windows leave as snapshots in the same
way. Only position and opacity change, never size. A window is a tree of the program's buffers and
the compositor's border rectangles, and a buffer can be drawn at another size while a rectangle
cannot, so a scaled window would pull its border away from its contents.

A live window's border does not fade: a scene rectangle has no opacity of its own, only a colour
that the compositor rewrites as focus changes. The outline is there from the first frame and the
contents fade in inside it, which is also what [peek](#smaller-changes-to-upstream-behaviour)
leaves on screen. A peek ends any window transition running when it starts, and none starts while
a peek holds the windows' opacity.

What a transition costs is what a fade costs: a moving or fading window hides nothing beneath it,
so everything under it is composited for its length, which is why it is off by default. A moving
window is not snapped to the character grid on its way: the compositor has no single cell pitch,
since every program chooses its own font size and scale.

## Supervised children

The compositor starts eight programs from a table in `kdos-child.c` and restarts them when they
exit. Three run once per output; five run once for the session, four of them because they own a
single bus name, socket or subscription.

| Child | Per output | Started when |
|---|---|---|
| `kdos-shell` (the panel) | yes | `panel` is not `off` |
| `kdos-desk` (desktop icons) | yes | `desktop_icons` |
| `kdos-slit` (dockapp column) | yes | `slit` |
| `kdos-notifyd` (notifications) | no | always |
| `kdos-netagent` (Wi-Fi and VPN passwords) | no | always |
| `kdos-mediad` (removable media) | no | always |
| `kdos-clip` (clipboard history) | no | `clipboard` |
| `wvkbd-deskintl` (on-screen keyboard) | no | `osk` is not `off` |

The single-instance children are single for concrete reasons:

- `kdos-notifyd` owns the bus name `org.freedesktop.Notifications`, and a second instance could
  not take it.
- `kdos-netagent` registers one secret agent with NetworkManager, and a second would ask for the
  same password twice.
- `kdos-mediad` holds one subscription to `kdos-mountd`, and a second would offer every removable
  device twice.
- `kdos-clip` owns one socket in `$XDG_RUNTIME_DIR` and keeps the history in memory, so a second
  would be a second history nobody could reach.
- `wvkbd-deskintl` types into the one keyboard focus, and it is supervised so that the signals that
  show and hide it always reach the live process.

The per-output children are per output because a layer surface (a surface that a program places
on a screen layer above or below the windows, through the wlr layer-shell protocol) without a named
output is placed on one screen only, and the libraries these programs draw with hold a single
cell buffer, so a second screen needs a second process. Each is started with `--output <name>`.
When an output goes away, its children are sent `SIGTERM` and not restarted. The table holds 40
children: the five session-wide ones plus three per output, which is eleven outputs' worth.

The children receive their settings from `comp.conf` on their command line, which is why those keys
apply at the next login:

- every child gets `--font` with `chrome_font` when it is set, except the panel, which gets
  `panel_font` instead, and the on-screen keyboard, which gets no font;
- the panel also gets `--top` or `--bottom`, `--cells`, `--margin`, `--opacity`, and `--clock` and
  `--autohide` when those are set;
- the panel and the desktop icons get `--no-icons` when `icons = no`. No other child is given it,
  because a child that does not parse a flag exits with a usage error and would never start;
- the on-screen keyboard gets `--hidden`.

### When a child keeps crashing

A child that exits more than five times within 30 seconds is not restarted again. The log records
it, and a `kdos-prompt` dialog names the program that was given up on (one dialog per reload,
however many children fail). **Reload Configuration** in the desktop's right-click menu, or
`kdos-comp -r`, resets the counters and gives it another run of attempts.

Before starting a child, the compositor clears the signal mask and resets every signal the session
had ignored. Ignored signals survive `exec`, so a session started under something like `nohup`
would otherwise pass that on, and the live retint, which is delivered as `SIGHUP`, would never
arrive.

Programs launched by key bindings are not supervised: a terminal opened with a key is not part of
the desktop's chrome.

The session start-up around these children is in
[The session](../03-architecture/session.md#supervised-chrome).

## Xwayland

The compositor runs Xwayland rootless, and it is the one X server on the system: it serves
X11-only applications in boxes and host applications that have no Wayland path. Xwayland starts
when the first X11 client connects, unless `rc.xml` asks for it to persist. It is built with
glamor, DRI3 and GLX (`-Dglx=true` in `ports/core/x11/xwayland/build.sh`), and Mesa is built with the
X11 platform and `-D glx=dri` behind libglvnd, so an X11 client on the host that draws through GLX
gets OpenGL. An X11 client in a box draws with its box's Mesa. See
[Principles](../01-philosophy/principles.md#no-xorg-server-and-one-carve-out).

The compositor sets `DISPLAY` only in the environment of programs it starts itself. A launcher run
from elsewhere may not have it, so `kdos-appbox` finds the X socket in `/tmp/.X11-unix` on its own
and adds `DISPLAY` to a box's environment.

## Smaller changes to upstream behaviour

Several additions are too small for a section of their own:

- **Peek.** The `peek` verb on the command socket fades every window to 12 per cent opacity while
  the pointer rests on the panel's Show Desktop button, and back when it leaves, each way over
  150 ms ([Motion](#motion)). Windows on other workspaces are set as well, so a workspace switch
  during a peek brings none back translucent. Nothing is minimised
  or unfocused, so a peek interrupted by a crash changes no window state. Labwc's own Show Desktop
  action, which minimises, is unchanged.
- **Click-away for menus.** The desktop's menus, launcher, run box and similar front ends are
  layer surfaces that take the keyboard on demand and close when they lose it. A press anywhere
  other than the focused one (the desktop, the wallpaper, the panel) releases its keyboard, so the
  menu closes. Layer surfaces that hold the keyboard exclusively, such as a lock screen, are left
  alone.
- **Windows on other workspaces are reported minimised** through the wlr foreign-toplevel protocol,
  unless they are shown on every workspace. The panel derives which workspaces have windows from
  this, since the workspace protocol has no such state.
- **The application-identifier ledger.** Every application identifier a mapped window presents is
  appended, once, to `~/.local/share/kdos/observed-app-ids`, which
  [`kdos appid`](kdos-command.md#kdos-appid) compares launchers against.
- **The default terminal** in labwc's built-in bindings and menu is `foot`.

## Shutdown

`SIGTERM`, `SIGINT` and `kdos-comp -e` stop the event loop. The KDOS parts are then taken down in
this order:

1. the command socket, so no `kdos hey run` can act on a session that is ending;
2. the keyboard monitor's bus connection and the accessibility timers;
3. the render-late timers, so none fires inside the power-down animation;
4. the power-down animation (see [The phosphor pass](#the-phosphor-pass)), limited to 600 ms and
   skipped with `motion = no`;
5. the wallpaper, the frames socket, window memory, window groups, box chips, the lid, peek, the
   fades (any running fade ends, a moving window is put back at rest, and every snapshot is
   freed) and the idle policy;
6. the phosphor pass;

and then the server itself. The panel and the notification daemon notice the compositor has gone
and exit on their own.

When the compositor exits with a non-zero status, `kdos-desktop-start` prints the last 20 lines of
the log and offers to restart the session. At the third crash within 60 seconds it stops offering
and returns you to the text console. A clean exit is a logout.

## Debugging

The first place to look is `$XDG_RUNTIME_DIR/kdos-comp.log` (usually
`/run/user/1000/kdos-comp.log`), with the previous session's in `kdos-comp.log.old`.
`kdos doctor` checks, among other things, that the compositor's two sockets and the portals are up.

| Variable or option | Effect |
|---|---|
| `KDOS_COMP_DEBUG=1`, or `-d` | Log at debug level; each late frame is logged as well |
| `KDOS_CRT_DUMP=<prefix>` | Write the phosphor pass's input and output once, to `<prefix>-in.ppm` and `<prefix>-out.ppm` |
| `KDOS_CRT_DUMP_FRAME=<n>` | Wait until frame *n* before dumping |

The fork logs at the informational level by default, where upstream logs errors only. At errors
only, the KDOS additions' decisions would be invisible: which wallpaper was loaded, whether the
phosphor pass is on and why not, the idle timers and each idle stage as it fires, the lid policy,
and which `comp.conf` lines were ignored. An empty log would look like a hung session.

## Working on the compositor

This section is for someone changing the compositor's code rather than configuring it.

### Finding the KDOS additions

The KDOS code lives in twenty-three `src/desktop/kdos-comp/src/kdos-*.c` files, one shared header,
`src/desktop/kdos-comp/include/kdos.h`, through which every addition enters, the phosphor pass's
own `include/kdos-crt-pass.h`, and one further
source file, `src/cycle/osd-apps.c`, the application-first switcher. Upstream files carry only
small hooks, each marked with a comment containing `KDOS`. Most begin `/* KDOS`: there are 148
such comments across 34 upstream files (27 sources and 7 headers), 37 of them in `main.c`. Some
hooks sit inside a longer comment whose line begins ` * KDOS:`, and three files carry only that form
or the `LAB_GRADIENT_KDOS_RULE` fill: `include/theme.h`, `include/buffer.h` and
`src/ssd/ssd-button.c`. Grep for `KDOS` rather than `/* KDOS` to find
them all. The three `meson.build` files mark their changes with `# KDOS`.

Three kinds of change carry no marker. The calls into `libkwm` (see below) are found by grepping
for `kwm_`, and the stripped upstream directories are recorded only in the top-level `meson.build`.
The fastest-first mode selection (see [Choosing the mode](#choosing-the-mode)) is
`highest_refresh_at()` and the loop that calls it in `output_test_auto()` in `src/output.c`.

| File (under `src/desktop/kdos-comp/src/`) | What it adds |
|---|---|
| `kdos-config.c` | The KDOS configuration file, its reload, and the accent lookup every addition shares |
| `kdos-child.c` | [Supervised children](#supervised-children) and the display-layout apply on a new output |
| `kdos-wallpaper.c` | [The wallpaper](#the-wallpaper), as part of the scene rather than a separate program |
| `kdos-crt.c` | [The phosphor pass](#the-phosphor-pass), the degauss and the power-down |
| `kdos-crt-pass.c` | The pass's shader and which pixels a frame redraws: GLES2 and pixman only, so a test can link it |
| `kdos-frames.c` | [The frames socket](#the-frames-socket), reporting late frames |
| `kdos-idle.c` | [Idle, dim, lock and lid](#idle-dim-lock-and-lid): the idle policy and the virtual-machine test |
| `kdos-lid.c` | The lid switch and `lid_close` |
| `kdos-cmd.c` | [The command socket](#the-command-socket) other KDOS programs query |
| `kdos-thumb.c` | [Window thumbnails](#window-thumbnails) for hover previews |
| `kdos-peek.c` | Fading windows to reveal the desktop |
| `kdos-motion.c` | [Motion](#motion): the layer surfaces' open and close fades, the moving and snapshot fades windows use, and the clock every fade shares |
| `kdos-winmotion.c` | [Window transitions](#window-transitions): which windows move where on a map, an unmap, a minimise and a workspace switch |
| `kdos-sched.c` | [Render-late scheduling](#render-late-scheduling): the per-output timer and its prediction |
| `kdos-appid.c` | The ledger of application identifiers windows actually present |
| `kdos-boxchip.c` | [The box colour chip](#box-identity) on a title bar |
| `kdos-grant.c` | Per-box grants beyond the sandbox allowlist |
| `kdos-group.c` | [Window groups](#window-groups-and-window-memory) (tabbed stacks) |
| `kdos-layerfocus.c` | Closing menus and other on-demand surfaces when you click elsewhere |
| `kdos-winpos.c` | [Window memory](#window-groups-and-window-memory): reopening windows where they were |
| `kdos-a11y.c` | [Accessibility](#accessibility): the keyboard aids, dwell click, the pointer size and the on-screen keyboard |
| `kdos-a11ymon.c` | [The keyboard monitor](#the-keyboard-monitor) on the session bus, and the toggle notifications |
| `kdos-screensaver.c` | `org.freedesktop.ScreenSaver` on the session bus: the idle inhibitor for X11 clients under Xwayland ([Idle, dim, lock and lid](#idle-dim-lock-and-lid)) |

The box lookup itself (`kdos_view_box()` and the `/proc` read for X11 windows) lives in the
upstream `view.c`, beside the security-context lookup it extends.

### What the compositor does not decide

The compositor owns no window-model arithmetic. Where a new window lands, what a tiled state
becomes and what rectangle it occupies, which edge a moving edge stops against, and how workspace
stepping skips empty workspaces all come from the `libkwm` library. `kdos-comp` calls eight of its
functions: `kwm_place` (in `placement.c`), `kwm_tile_geom` and `kwm_tile_next` (in `view.c`),
`kwm_edge_check` (in `snap.c`), `kwm_ws_adjacent` (in `workspaces.c`), and `kwm_edge_best`,
`kwm_clip_add` and `kwm_clip_sub` (in `include/edges.h`, the edge-search arithmetic). The library is tested
against a fixture with no compositor running, so every one of those rules can be checked without
booting anything.

What stays here is what only a compositor can do: walking its own window list, asking the
decoration how thick it is, working out which edges are actually visible, and deciding how a drag
feels as it crosses one. See [The window model](../03-architecture/window-model.md).

### Building and testing

`build.sh` compiles `libkbase`, `libkcolor` and `libkwm` from `src/libs/` into one static archive,
hands it to meson through `LDFLAGS`, and then runs the ordinary meson build and install. Editing
any of those libraries therefore changes the compositor. The compositor is built in the
`50_desktop` phase and named in its list, `script/phases/50_desktop/packages.txt`, under the
`src-desktop` heading; the same list builds `wlroots` (`ports/core/wl/wlroots/`) ahead of it. The
Wayland base under both comes from the `41_system` and `42_graphics` phases (see
[How KDOS is built](../05-developer/how-kdos-is-built.md#the-desktop-50_desktop)). The ports it
ships beside are listed under
[`src/desktop` in the ports catalogue](../06-reference/ports-catalogue.md#srcdesktop).
To rebuild only the compositor:

```sh
make build BUILD_ARGS="--phases 50_desktop --rebuild kdos-comp"
```

Six checks cover it without a full build:

- `testing/selftest.sh` compiles every `kdos-*.c` file against the installed wlroots headers,
  where the host has pkg-config entries for `wlroots-0.20`, GLES2, EGL, wayland-server, pixman,
  libdrm, libpng, libxml2, cairo, pango, glib and basu; elsewhere it reports the step as skipped.
- `testing/selftest.sh` also runs `testing/fixtures/crt/scopecheck.c` wherever EGL, GLES2, pixman
  and wayland-server are installed, which includes the development container (software GL is
  enough). It draws a scripted run of damage through the pass's own shader and region code and
  wlroots' own damage ring over three output buffers, and compares every frame with a whole-output
  draw. It also runs itself with the reach and with the buffer age switched off, and fails unless
  both of those runs fail.
- `testing/selftest.sh` runs `testing/fixtures/motion/motioncheck.c` where `wlroots-0.20` is
  installed. It compiles `kdos-motion.c` against wlroots' own scene graph and plays a layer surface
  mapping, unmapping half way through its fade, its client exiting, its output going and the
  compositor shutting down, and checks where the snapshot stands, that it keeps and then releases
  its buffers, that it lets the pointer through, and that every fade ends on its exact end value.
  It plays a window's tree the same way: an open that labwc moves part way through, a close whose
  snapshot copies the border rectangles as buffers, a workspace switch's snapshot stacked below
  its anchor, a window taking over its own leaving snapshot, and `motion` switched off
  mid-transition, and checks that a window always ends at rest and opaque. `kdos-winmotion.c`,
  which decides which windows move, is covered only by the compile.
- `testing/selftest.sh` runs `testing/fixtures/sched/schedcheck.c` under the same condition. It
  compiles `kdos-sched.c` with a hand-built output and a real event loop, and checks the
  prediction, that a deferred frame holds the output's pending flag for the wait and lowers it
  before the composite, that any other commit (with a buffer, without one, or switching the output
  off) or a refused frame event cancels the timer, that every case it cannot predict composites at
  once, and that the timer goes with its output and at shutdown.
- `testing/preflight.sh` checks the shipped `rc.xml` for well-formed XML and `<default />`, and
  that every command named in `rc.xml` and `menu.xml` exists.
- `testing/quick.sh kdos-comp` rebuilds the port and patches it into a booted ISO for a screenshot;
  see [Testing](../05-developer/testing.md) for what that harness can and cannot show.

The [QEMU rig](../05-developer/testing.md#the-qemu-rig) boots with plain graphics and uses
software rendering, so the phosphor pass never appears in its screenshots. Use `KDOS_CRT_DUMP` or
real hardware to see it.

## See also

- [The session](../03-architecture/session.md): what starts the compositor, and what it starts
- [The desktop](../02-user-guide/desktop.md): the bindings and the user-facing behaviour
- [Theming](../02-user-guide/theming.md): the accent, the wallpaper and the phosphor pass from the
  user's side
- [The window model](../03-architecture/window-model.md): the arithmetic `libkwm` owns
- [The design language](../03-architecture/design-language.md): why the frame looks the way it does
- [kdos-shell](kdos-shell.md): the chrome it supervises
- [The kdos command](kdos-command.md): `kdos hey`, `kdos stutter`, `kdos theme` and `kdos appid`
- [Configuration](../06-reference/configuration.md): every key above, with defaults
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#the-desktop): the desktop beside those of
  other distributions

<!-- book-nav -->
---

*Part IV — Programs, chapter 21.* Previous: [20. The programs](README.md) · [Contents](../README.md) · Next: [22. kdos-shell](kdos-shell.md)
