# kdos-comp

This page describes `kdos-comp`, the KDOS compositor: the program that draws the screen, places
and decorates windows, reads the keyboard and mouse, and keeps the desktop's own panel, icons and
notification daemon running. It is for anyone configuring the desktop beyond the settings window,
diagnosing a desktop problem, or working on the compositor itself.

`kdos-comp` is a frozen hard fork of the labwc 0.20.0 Wayland compositor, carrying sixteen KDOS
additions. If you only want to change a key binding or a setting, read
[Configuration](#configuration) and [Bindings](#bindings). If the desktop misbehaves, start with
[Debugging](#debugging) and the session log. To change the compositor's code, read
[Working on the compositor](#working-on-the-compositor). The user's view of the same desktop is
[The desktop](../02-user-guide/desktop.md).

The case for forking an existing compositor rather than writing one is set out in
[Decisions](../01-philosophy/decisions.md).

## Overview

| | |
|---|---|
| Binary | `/usr/bin/kdos-comp` |
| Upstream configuration | `~/.config/kdos-comp/rc.xml` and `menu.xml`, both copied from `/etc/skel` for a new user; also read from `/etc/xdg/kdos-comp/` |
| Generated theme | `~/.config/kdos-comp/themerc-override`, written by `kdos theme` |
| KDOS configuration | `~/.config/kdos/comp.conf` (`$XDG_CONFIG_HOME/kdos/comp.conf`) |
| Started by | `kdos-desktop-start`, the session script — see [The session](../03-architecture/session.md) |
| Log | `$XDG_RUNTIME_DIR/kdos-comp.log`; the previous session's is kept as `kdos-comp.log.old` |
| Command socket | `$XDG_RUNTIME_DIR/kdos-cmd.sock` |
| Frame-timing socket | `$XDG_RUNTIME_DIR/kdos-frames.sock` |
| Built with | meson, from `src/desktop/kdos-comp` in the repository — there is no source archive to fetch |

The tree is labwc 0.20.0 under the name kdos-comp, and upstream changes are not merged into it.
`KDOS-FORK` at the root of the port records the upstream tarball and its checksum. Upstream's licence (GPL-2.0)
and copyright headers are kept.

Because this is a fork rather than a set of patches, labwc's documentation for `rc.xml`, `menu.xml`
and the theme applies as written: window management, key and mouse bindings, window rules, theme
keys and menus all work as labwc's documentation describes. The one difference is the directory:
where labwc reads `~/.config/labwc/`, `kdos-comp` reads `~/.config/kdos-comp/`, and that is also
where labwc's `environment`, `autostart` and `shutdown` files go.

The manual pages are not installed (the port builds with `-Dman-pages=disabled`), so
`man labwc-config` finds nothing on a KDOS machine. Read the 0.20.0 pages upstream at
[labwc.github.io](https://labwc.github.io/): `labwc-config(5)` for `rc.xml`, `labwc-actions(5)`
for the actions, `labwc-menu(5)` for `menu.xml` and `labwc-theme(5)` for the theme keys.

### Command-line options

These are labwc's, under the new name.

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
| `-t`, `--title <fmtstr>` | The window title to use when running nested inside another compositor |
| `-v`, `--version` | Show the version and quit |
| `-V`, `--verbose` | Informational logging — already the default here; after `-d` it lowers the level back |

`-r` and `-e` find the running compositor through `LABWC_PID`, which `kdos-comp` sets in the
environment of everything it starts. `kdos theme` reloads the compositor by sending `SIGHUP`
directly.

## Configuration

Configuration is split between two files, and the split is strict:

- `~/.config/kdos/comp.conf` holds only the KDOS keys listed below;
- `~/.config/kdos-comp/rc.xml` holds everything labwc understands — bindings, startup commands,
  workspaces, mouse behaviour, window rules and theme settings.

`comp.conf` is one `key = value` per line, with `#` starting a comment line. The shipped file has
every key commented out, so the defaults below are what an unedited machine runs with. The same
keys are listed in [Configuration](../06-reference/configuration.md#configkdoscompconf), and most
can be changed from the settings window.

Every line that does not take effect is written to the log by name: a binding, startup,
workspace or mouse line (those belong in `rc.xml`), an unknown key, a key with an empty value
(write `wallpaper = none` rather than `wallpaper =`), and a value out of range or of the wrong
kind. A setting that silently does nothing cannot be told from a typo, so check the log when a
change seems ignored. A `panel_bottom` line has no effect, and the log names
`panel = bottom|top|off` as the key to use.

A path value may start with `~/` or `$HOME/`; both are expanded. A yes/no value accepts `yes`/`no`,
`true`/`false`, `on`/`off` and `1`/`0`, in any case.

### Keys applied on reload

These take effect as soon as the compositor reloads, which happens on `SIGHUP`, `kdos-comp -r`, the
Reload Configuration entry in the desktop's right-click menu, and every `kdos theme`.

| Key | Default | Range | Does |
|---|---|---|---|
| `wallpaper` | `/usr/share/backgrounds/kdos/default-wallpaper.png` | a PNG path, or `none` | The wallpaper — see [The wallpaper](#the-wallpaper) for which file wins |
| `crt` | `55` | 0–100 | Phosphor pass strength, per cent. `0` turns the pass off |
| `crt_scanlines` | `0` | 0–100 | Scanline depth; `60` is a good strength if you want them |
| `crt_curve` | `0` | 0–100 | Barrel distortion |
| `crt_fullscreen` | `yes` | yes/no | Whether the pass runs over a fullscreen window |
| `idle_dim` | `300` | 0–86400 s | Seconds of inactivity before the screen dims; `0` is never |
| `idle_lock` | `600` | 0–86400 s | Seconds before the session locks; `0` is never |
| `idle_off` | `900` | 0–86400 s | Seconds before the screens are powered off; `0` is never |
| `lid_close` | `suspend` | `suspend`, `lock`, `off` | What closing a laptop lid does; any other value is refused by name |
| `window_memory` | `yes` | yes/no | Whether an application opens where its window last was |

In a virtual machine the idle timers and the lid default to off unless `comp.conf` sets them —
see [Idle, dim, lock and lid](#idle-dim-lock-and-lid).

### Keys applied at the next login

Each of these becomes part of a supervised child's command line, so a change takes effect when the
session next starts. A reload logs the change by name, for example
`panel_font changed — applies at the next login`, and keeps the running value until then, so the
compositor never believes a setting that the panel on screen is not using.

| Key | Default | Range | Does |
|---|---|---|---|
| `panel` | `bottom` | `bottom`, `top`, `off` (or `none`) | Where the panel goes, or no panel |
| `panel_cells` | `2` | 1–4 | Panel height in text cells |
| `panel_font` | `Terminus:pixelsize=20` | a font pattern | The bar's own font; empty follows `chrome_font` |
| `panel_autohide` | `no` | yes/no | Whether the bar hides when the pointer leaves it |
| `panel_margin` | `0` | 0–64 px | Gap between the bar and the screen edge |
| `panel_opacity` | `80` | 20–100 % | Bar opacity |
| `desktop_icons` | `yes` | yes/no | Whether the desktop icon surface runs |
| `slit` | `no` | yes/no | The dockapp column |
| `clipboard` | `yes` | yes/no | The clipboard history daemon |
| `icons` | `yes` | yes/no | Whether the panel and the desktop draw pictures at all |
| `chrome_font` | `Terminus:pixelsize=32` | a font pattern | The font every KDOS surface draws with |
| `clock_format` | `%H:%M` | a `strftime` format | The panel clock |

`panel_opacity` stops at 20 rather than 0. A bar at zero would not be see-through; it would be
invisible while still catching the pointer, with no way back except editing this file from a text
console.

The panel's height follows its font: a cell is half as wide as it is tall, so
`Terminus:pixelsize=20` with two cells makes a 40-pixel bar while the menus it opens stay at the
32-pixel chrome font.

The two font keys are also written by the font page of `kdos-style` and by `kdos-settings`. They
change only the family and keep the size each key already carried, leaving every other line of
`comp.conf` as it was. The compositor passes `--font` to each supervised child as it starts it, so
the desktop picks up the new family as each surface next starts.

### Files whose existence is the setting

Two files ship absent, and absent is a working default for both. The compositor does not read them
itself; they are listed here because they sit beside `comp.conf`.

| File | Enables | Read by |
|---|---|---|
| `~/.config/kdos/session-restore` | Reopening the previous session's windows at login | The session scripts |
| `~/.config/kdos/a11y` | The accessibility stack inside boxes | `kdos-appbox` |

`~/.config/kdos/favorites` has the same shape but ships with a handful of pinned entries, because
an empty list makes both the quick-launch row and the Start menu's pinned column look broken on a
new machine. Delete every line if you want it empty. An identifier with no matching desktop entry
is skipped silently, so an application that is not installed leaves no launcher that opens
nothing.

## Bindings

The shipped bindings are listed in [The desktop](../02-user-guide/desktop.md), and `Super+F1` shows
the card generated from your own `rc.xml`. Two rules about editing `rc.xml` matter more than any
single binding, because breaking either one breaks the whole desktop without any message.

### The one line that must not be lost

`<default />` must be the first child of both `<keyboard>` and `<mouse>` in `rc.xml`:

```xml
<keyboard>
  <default />
  <keybind key="W-Return"><action name="Execute" command="foot"/></keybind>
</keyboard>
```

The compositor loads its built-in bindings only when your file defines none of that kind, so a
file that binds a single key throws every default away: click-to-focus, dragging by the title bar,
the window buttons, resizing by the border, the root menu, window cycling, close, and the snap
arrows. On a running system the symptom is "the mouse does not work", and nothing checks for it —
not the compiler, not the recipe parser, not XML validation. Put your own bindings after
`<default />`, because when a key is bound twice the later binding wins.

`testing/preflight.sh` fails a shipped `rc.xml` that gets this wrong.

### `--` may not appear inside an XML comment

`rc.xml` is XML, and XML forbids `--` inside a `<!-- -->` comment. A comment that mentions a
command-line option such as `--app-id` makes the whole file invalid, and a compositor that cannot
parse its configuration loads none of the bindings in it. Nothing says so; the chords are simply
missing, one by one, in whatever order you happen to try them. `testing/preflight.sh` parses the
shipped file with a real XML parser for this reason; run `xmllint --noout rc.xml` after editing
yours.

### KDOS actions

Besides labwc's actions, the fork adds three for [window groups](#window-groups-and-window-memory),
usable in any binding or menu: `AddToTabGroup`, `RemoveFromTabGroup` and `NextInTabGroup`. The
shipped `rc.xml` does not bind them. To reach them, add bindings after `<default />`; `Super+g`,
`Super+Ctrl+g` and `Super+Alt+g` are free in the shipped file:

```xml
<keybind key="W-g"><action name="AddToTabGroup"/></keybind>
<keybind key="W-C-g"><action name="RemoveFromTabGroup"/></keybind>
<keybind key="W-A-g"><action name="NextInTabGroup"/></keybind>
```

## Decorations

The window frame is generated from the same palette as everything else, into
`themerc-override`, which the compositor reads over its built-in theme. Frames therefore retint
live with the panel and the phosphor pass. The reasoning is in
[the design language](../03-architecture/design-language.md); the mechanics are these:

- `<cornerRadius>0</cornerRadius>` in `rc.xml`, and `border.width: 2` in the generated override.
  A one-pixel hairline, as modern toolkits draw, disappears beside a 32-pixel text cell.
- The title bar is drawn with the same double rule the cell grid uses. The title-bar fill is one
  pixel wide and stretched, so anything that varies only vertically costs nothing — and a double
  horizontal rule varies only vertically.
- The title text and every button sit on the plain background instead. A rule behind a word looks
  like a word struck through, and a rule behind the minimise button, itself a horizontal line,
  would leave the button unreadable.
- Button glyphs are small bitmaps enlarged by a whole number with nearest-neighbour filtering.
  Upstream's resize path only ever shrinks, so without this the glyphs would sit at their own few
  pixels in the middle of a large button.
- The hover highlight is translucent. There are no separate hover icons: the plain image is copied
  and a colour laid over it, so an opaque colour would paint the symbol out and leave every button
  blank under the pointer.

The title-bar font must name a scalable face. Pango, which draws the titles, does not render bitmap
fonts, so naming the bitmap console font silently falls back to a generic sans for every title bar
and menu. The shipped `rc.xml` names `Terminus (TTF)` at 24 points — 32 pixels at 96 dpi — so a
title bar is exactly one cell tall. A machine without that font falls back to Noto Sans, which
`56-noto-preferred.conf` puts first for `sans-serif`.

To tell the two apart, measure rather than look: count the brightness levels of the text in a
screenshot. Bitmap text has three and no midtones; an antialiased face has well over a hundred.

The generated theme sets `menu.width.max: 900`. The upstream default is sized for a small font,
which at 32 pixels truncates menu entries at about eleven characters. A maximum only ever
truncates, so a generous one costs a short menu nothing.

## The prompt command

`<core><promptCommand>` in `rc.xml` is:

```
kdos-prompt --message '%m' --no '%n' --yes '%y'
```

labwc's conditional action (`If` with a prompt) runs the prompt command and acts on its exit
status: zero takes the affirmative branch, a specific code means cancelled, and anything else takes
the negative branch.

Upstream's own prompt program, labnag, is not built (`-Dlabnag=disabled`). `kdos-prompt` —
`kdos-shell` under another name — answers with the same exit codes. It is what lets ending the
session, restarting and shutting down ask before they act.

## Frame pacing and the output mode

Nothing in the compositor sets a frame rate. It draws only when an output's backend reports that
the screen is ready for another frame; the handler composites once and returns. There is no timer
and no fixed period, so the rate is the display mode's: a 144 Hz panel gets 144 frames a second for
the same reason a 60 Hz one gets 60. An output with nothing to redraw skips the frame, which is why
an idle desktop costs nothing.

Two rate limits exist, and neither holds the frame rate back. Interactive resizing sends a window
at most one new size per refresh interval of its output. The shutdown animation steps every 16 ms,
after the display has already stopped.

**Choosing the mode.** The resolution comes first and then the highest refresh rate that works:

1. The panel's preferred mode (its EDID preferred timing) fixes the resolution.
2. Every mode at that resolution with a higher refresh rate is tried, fastest first.
3. The preferred mode itself is tried.
4. The first mode that passes its test is used. If none passes, every other mode the output lists
   is tried, in the order the output lists them. If none of those passes either, the output is
   committed with no fixed mode.

Taking the preferred mode's own rate would be a trap: panels commonly advertise 60 Hz as preferred
and list 120 or 144 Hz elsewhere, and the session would sit at 60 Hz with nothing on the desktop
offering a choice. Trying fastest first is safe, because a rate the cable or link cannot carry
fails its test and the next one down is tried.

Two things override this. A mode a program requests through the output-management protocol (for
example `kdos-display`) is tried exactly as asked, which is how you pin a rate. And
`reuseOutputMode` in `rc.xml` keeps a mode that is already set ahead of any of this, which stops a
handover from re-setting the mode of a screen that is already working.

## The phosphor pass

The compositor draws the whole desktop through a shader that imitates a CRT: optional scanlines on
every third physical row, a three-tap horizontal bleed, a vignette, optional barrel distortion, and
a faint phosphor floor so that black is never quite black. It is on by default at 55 per cent, with
scanlines and curvature off. The user's guide is
[Theming](../02-user-guide/theming.md#the-phosphor-pass).

Scanlines ship off because they fight the text underneath. The desktop is a grid of 16×32 cells,
and a dark line on every third physical row crosses the glyphs at a period nothing on screen
shares, so crisp two-colour text arrives striped. `crt_scanlines = 60` turns them on.

The pass needs the GLES2 renderer. On software rendering — including a virtual machine with plain
graphics — it is switched off at startup and the log says so.

### How it is implemented

wlroots, the library under the compositor, has no shader interface: its rendering pass offers
textures and rectangles, and its scene graph has no callback node. What it does offer is a
documented seam — the scene's build-state call accepts a custom swapchain. So the scene composites
into a buffer of the compositor's own, and KDOS code copies that buffer into the output's real
buffer with the effect applied. Both swapchains come from the library's own configuration call,
so neither needs guesswork about formats or modifiers.

Four constraints shape it:

- **Direct scanout is off for the whole session while the pass is on.** In that mode the scene
  hands the display one client's buffer and a rectangle rather than a picture of the desktop, and
  the pass would stretch, say, the panel over the whole screen. wlroots has exactly one switch for
  it, the `WLR_SCENE_DISABLE_DIRECT_SCANOUT` environment variable read when the scene is created,
  so the pass sets it before the scene exists. A foreign buffer that arrives anyway is shown
  unprocessed rather than mangled.
- **The texture is imported every frame and destroyed after the pass.** Caching one per swapchain
  slot looks like an easy optimisation and deadlocks the swapchain: importing locks the buffer, a
  slot is reused only when its last lock goes, and a full set of cached textures leaves no free
  buffer, so the scene stops rendering.
- **Neither fallback can produce a black screen.** A renderer other than GLES2 gets no pass at all,
  reported at startup — a fullscreen post-process in software is a slideshow. Anything that fails at
  run time puts that output on the plain path for a cooldown: five seconds, doubling after each
  consecutive failure up to a minute, and reset by a pass that completes. Sixty identical error
  lines a second would be worse than missing scanlines, and giving up for good would leave a screen
  unthemed for the whole session over one hot-plug hiccup.
- **The magnifier takes the frame instead, whole.** The magnified inset is drawn inside the call the
  pass replaces, so with both on, a magnified frame would lose the inset whenever the scene redrew
  and keep it whenever it did not, flickering between two pictures. The pass stands aside while the
  magnifier is on, which is also right for its own sake: an accessibility zoom is harder to read
  through scanlines.

The curvature is normalised by how far the corners move, so no setting crops the desktop.

Colours come from the shared palette and the accent is re-read on reload, so a theme change
retints the running shader with the same signal that repaints the panel.

### What it costs

The pass does not limit the frame rate. Timed off-screen at the shipped defaults on a GeForce RTX
4060:

| Resolution | Pass | Plain copy of the same buffer |
|---|---|---|
| 1920×1080 | 0.033 ms | 0.018 ms |
| 2560×1440 | 0.060 ms | 0.033 ms |
| 3840×2160 | 0.138 ms | 0.070 ms |

The effect costs roughly as much as moving the pixels a second time, and even the 4K figure is two
per cent of a 144 Hz frame.

`crt_fullscreen = off` skips the pass for fullscreen windows: one render instead of two for video
and games. It is a battery setting rather than a frame-rate one.

### Inspecting it without a screen

`KDOS_CRT_DUMP=<prefix>` writes the pass's input and output once, as `<prefix>-in.ppm` and
`<prefix>-out.ppm`.
`KDOS_CRT_DUMP_FRAME=<n>` waits until frame *n* before dumping, to get past the empty first frame.
The input is read back through the texture the shader sampled rather than from the buffer, because
that buffer is not always readable.

One property only real hardware can confirm. The composite and the pass share one GL context, so
their order is guaranteed, but the final display commit relies on implicit buffer fencing. Getting
that wrong shows up as a torn frame, not as an error.

## The wallpaper

The compositor draws the wallpaper itself: one scene buffer per output at the bottom of the scene,
rebuilt when outputs are added, removed or rearranged. A separate wallpaper program, the usual
answer on Wayland, would be the one program on this desktop that is not a character grid.

Which image is drawn:

1. With `wallpaper = none`, no image; the background is the accent's deep colour.
2. Otherwise, `~/.cache/kdos/wallpaper.png` when it exists. `kdos theme` writes this file: the
   shipped wallpaper retinted to the accent.
3. Otherwise, the `wallpaper =` path from `comp.conf`.

A missing or unreadable file also leaves the accent's deep colour, and the log names the path.
Because the retinted cache takes precedence, a picture of your own is replaced at the next accent
change; [Theming](../02-user-guide/theming.md#wallpaper) explains how to keep one. The image is
scaled to cover the output and centred. Only PNG is decoded.

The file carries the smallest buffer implementation that works, because wlroots has no public way
to make a buffer from memory. The decoder converts the PNG's channel order to the one the buffer
needs, and only a fully initialised buffer can leave it, because libpng reports a bad file by
jumping back after the allocation.

## Idle, dim, lock and lid

One timer drives three stages — dim, then lock, then screens off — each measured from your last
input rather than from the previous stage. Ten seconds before the lock, a toast says
`Locking soon`, and any input keeps the session open. Input ends the dim and powers the screens
back on; it never unlocks.

The idle policy stops completely while any program holds an idle inhibitor (a video player, for
example) or while the `stay-awake` toggle is on (`kdos toggle stay-awake`, `Super+Ctrl+i`).

The dim is a translucent layer raised over everything, with the lock screen raised above it; it
does not touch gamma. The lock stage starts `kdos-lock` — see
[daemons](daemons.md#kdos-lock).

**In a virtual machine** the three timers default to zero (never) and `lid_close` to `off`, unless
`comp.conf` sets them. A blanked screen over a remote display cannot be told from a crashed
compositor. Only a line that parses counts: `idle_dim = 5m` is refused and leaves the virtual
machine default in place.

**The lid.** `lid_close = suspend` hands the machine to `kdos-power suspend`, which locks first;
`lock` locks and powers the screens off; `off` only powers the screens off, which suits a closed
laptop driving an external monitor. Opening the lid powers the screens back on and counts as input;
it never unlocks.

**The compositor, not the lock program, owns the locked state.** If the lock program dies without
unlocking, the session stays locked: the lock surfaces keep covering every screen, and a new lock
program can take over from the one that died, which is exactly the recovery a crash needs. A lock
screen that unlocked when it crashed would be the failure the lock protocol exists to prevent.

## The frames socket

The compositor writes one line of JSON per late frame to `$XDG_RUNTIME_DIR/kdos-frames.sock`. This
is what [`kdos stutter`](kdos-command.md#kdos-stutter) reads.

A client first receives a `hello` record naming the compositor and wlroots versions and the counts
so far, then one `miss` record per late frame:

```json
{"event":"miss","mono_ms":…,"wall_ms":…,"output":"eDP-1","source":"present","late_ms":…,"dropped":7,"render_ms":…,"refresh_hz":60.00}
```

- **`source`** says where the timing came from. Presentation events carry when the picture actually
  reached the screen, and are used where the backend has them. Headless and nested backends do not
  report presentation, so the frame clock is the fallback. The two are labelled rather than
  averaged: a presentation gap is what you saw, and a frame gap is what the compositor was given.
- **A frame is late** when it arrives more than one and a half refresh intervals after the previous
  one. An output with nothing to draw is not counted as late.
- **`render_ms`** is the compositor's own render cost. When it is a large fraction of the frame
  budget, the desktop itself was late — the one causal claim the tooling makes.

The socket never slows the frame loop: both ends are non-blocking, and a reader that cannot keep up
loses lines. There is no history, so a reader that connects late has missed what happened.

This is deliberately not a Wayland protocol. It is a channel between two KDOS programs; a client
that wants its own timing has the standard presentation-time protocol.

## The command socket

Other KDOS programs ask the compositor questions over `$XDG_RUNTIME_DIR/kdos-cmd.sock`:
one JSON request line in, one JSON response line out, then the connection closes.
[`kdos hey`](kdos-command.md#kdos-hey) is the command-line front end.

| Request | Answers or does |
|---|---|
| `{"cmd":"list"}` | Every window: id, app_id, title, workspace, geometry, pid, box, instance and state (focused, minimized, maximized, fullscreen, shaded) |
| `{"cmd":"outputs"}` | The outputs and their scales |
| `{"cmd":"boxes"}` | The distinct boxes that currently have a window on screen |
| `{"cmd":"run","action":"Close","id":7}` | Runs a labwc action, on the window with that id when one is given |
| `{"cmd":"peek","on":true}` | Fades every window to reveal the desktop; `false` or no `on` restores them |
| `{"cmd":"thumb","app_id":"foot","w":64,"h":36}` | Writes a picture of that application's front window and answers with its path |

A window's `id` is fixed for the life of the session and never reused, so an id handed back late
cannot reach the wrong window. `run` resolves the action name against the same table `rc.xml` is
parsed with, so it can do exactly what a key binding can. An action that needs an argument the
request cannot carry — `Execute`'s command, `SnapToEdge`'s direction — is refused. Every refusal is
`{"ok":false,…}` with a reason, never a silent no-op.

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

The box collector uses `boxes` to ask whether a box still has a window before stopping it.

## Window thumbnails

A Wayland client cannot see another client's pixels, so the compositor renders a window to a file
on request (the `thumb` verb above). That is what the panel's hover preview is made of.

The preview is drawn as solid blocks from the palette's brightness ladder rather than through the
shape-matching character renderer. A large window squeezed into a small grid puts many source
pixels in each cell, so no shape is left to match and every textured cell would pick the same
character.

The preview is never required. No compositor, no socket, a window whose pixels are not readable (a
common case for GPU-rendered windows on real hardware), or a file that does not parse — each one
leaves the tooltip as the two lines of text it always carries.

## Window groups and window memory

**Window groups** stack several windows into one frame with a tab per member, like tabs in a
browser: `AddToTabGroup` stacks the focused window onto the one behind it, `RemoveFromTabGroup`
takes it back out, and `NextInTabGroup` steps through the tabs. Clicking a tab raises that member.
The hidden members are minimised, so focus cycling and the panel's window list treat them
correctly, and all members share the showing member's position and size. Dragging a tab moves that
window; it does not take it out of the group — use `RemoveFromTabGroup`.

**Window memory** (`window_memory = yes`) reopens an application where you left it. When a window
closes, its application id, geometry, workspace and shaded state are saved to
`~/.local/state/kdos/winpos`; when a window of that application next opens, the record is applied.
It applies only where the compositor would otherwise have chosen the position:

- a window that positions itself, is placed by a window rule, or opens maximised, tiled or
  fullscreen is left alone;
- dialogs and other windows with a parent are neither saved nor restored;
- a restored rectangle is clamped into the output's usable area, so a window never comes back
  off-screen or under the panel.

## Box identity

The compositor knows which box each window came from. `kdos-boxsock` gives every box its own
Wayland socket and tags each client that connects through it with a security context naming the
box.

An X11 window needs another route. Its Wayland client is Xwayland, running on the host with no
context, so the context lookup would answer nothing for every X11-only application in the
catalogue. The window does carry its client's process id, and every process a box starts has
`KDOS_BOX=<name>` in its environment, so the compositor reads that from `/proc/<pid>/environ` and
caches it per window.

**The box chip** is a small square in the box's colour at the left of the title. To give a box a
colour, set `accent = <name>` in its profile, `~/.config/kdos/boxes/<name>.conf` (see
[Configuration](../06-reference/configuration.md#configkdosboxesnameconf)):

- A box in the session's own accent draws no chip, so a default install has none, and the first chip
  appears when you give a box a colour to tell it apart by.
- The chip's width is added to the title's left offset in the one place where the title's width and
  position are both computed, so a long title never runs under the chip.
- It is rebuilt rather than patched on every title change and resize, so a drag never stacks chips.
- The profile is read once per window and re-read on reload, because an accent switch changes which
  windows wear a chip.
- On an inactive window the colour is dimmed: the chip identifies the box; it does not show focus.

**Grants.** A client from a box may bind only a fixed allowlist of Wayland interfaces. A
`grant = …` line in the same profile adds named interfaces, such as `screencopy` for a screen
recorder in its own box. The grantable names are listed in
[The session](../03-architecture/session.md#granting-a-box-more-than-the-allowlist). A box's grants
are read once, on its first request, and re-read after a reload.

## Supervised children

The compositor starts seven programs from a table in `kdos-child.c` and restarts them when they
exit. Three run once per output; four own a single bus name, socket or subscription and run once
for the session.

| Child | Per output | Started when |
|---|---|---|
| `kdos-shell` (the panel) | yes | `panel` is not `off` |
| `kdos-desk` (desktop icons) | yes | `desktop_icons` |
| `kdos-slit` (dockapp column) | yes | `slit` |
| `kdos-notifyd` (notifications) | no | always |
| `kdos-netagent` (Wi-Fi and VPN passwords) | no | always |
| `kdos-mediad` (removable media) | no | always |
| `kdos-clip` (clipboard history) | no | `clipboard` |

The single-instance children are single for concrete reasons:

- `kdos-notifyd` owns the bus name `org.freedesktop.Notifications`, and a second instance could
  not take it.
- `kdos-netagent` registers one secret agent with NetworkManager, and a second would ask for the
  same password twice.
- `kdos-mediad` holds one subscription to `kdos-mountd`, and a second would offer every removable
  device twice.
- `kdos-clip` owns one socket in `$XDG_RUNTIME_DIR` and keeps the history in memory, so a second
  would be a second history nobody could reach.

The panel receives its settings from `comp.conf` on its command line (`--top` or `--bottom`,
`--cells`, `--clock`, `--autohide`, `--margin`, `--opacity`, `--font`); the panel and the desktop
icons also receive `--no-icons` when `icons = no`. That is why those keys apply at the next login.

**When a child keeps crashing.** A child that exits more than five times within 30 seconds is not
restarted again. The log records it, and a dialog says which program was given up on. Reload
Configuration (in the desktop's right-click menu), or `kdos-comp -r`, resets the counters and gives it another run of
attempts.

Before starting a child, the compositor clears every signal the session had ignored, as well as the
signal mask. Ignored signals survive `exec`, so a session started under something like `nohup`
would otherwise pass that on, and the live retint, which is delivered as `SIGHUP`, would never
arrive.

Programs launched by key bindings are not supervised: a terminal opened with a key is not part of
the desktop's chrome.

The per-output rules and the session start-up around them are in
[The session](../03-architecture/session.md#supervised-chrome).

## Xwayland

The compositor runs Xwayland rootless, so X11-only applications in boxes work. Xwayland is built
without GLX, because the graphics stack is built without X11 platform support, so X11 clients get
no OpenGL. See [Principles](../01-philosophy/principles.md).

The compositor sets `DISPLAY` only in the environment of programs it starts itself, so
`kdos-appbox` finds the X socket on its own and adds `DISPLAY` to a box's environment.

## Shutdown

`SIGTERM`, `SIGINT` and `kdos-comp -e` stop the event loop. The KDOS parts are then taken down in
this order:

1. the command socket, so no `kdos hey run` can act on a session that is ending;
2. the shutdown animation — the screen collapsing to a dot — which is limited to a fixed deadline;
3. the wallpaper, the frames socket, window memory, window groups, box chips, the lid, peek and the
   idle policy;
4. the phosphor pass;

and then the server itself. The panel and the notification daemon notice the compositor has gone
and exit on their own.

When the compositor exits with a non-zero status, `kdos-desktop-start` prints the last 20 lines of
the log and offers to restart the session. After three crashes within 60 seconds it stops offering
and returns you to the text console.

## Debugging

The first place to look is `$XDG_RUNTIME_DIR/kdos-comp.log` (usually `/run/user/1000/kdos-comp.log`),
with the previous session's in `kdos-comp.log.old`. `kdos doctor` checks that the compositor, its
sockets and the portals are up.

| Variable or option | Effect |
|---|---|
| `KDOS_COMP_DEBUG=1`, or `-d` | Log at debug level; each late frame is logged as well |
| `KDOS_CRT_DUMP=<prefix>` | Write the phosphor pass's input and output once, to `<prefix>-in.ppm` and `<prefix>-out.ppm` |
| `KDOS_CRT_DUMP_FRAME=<n>` | Wait until frame *n* before dumping |

The fork logs at the informational level by default, where upstream logs errors only. At errors
only, the KDOS additions' decisions — which wallpaper was loaded, whether the phosphor pass is on,
which `comp.conf` lines were ignored — would be invisible, and an empty log would look like a hung
session.

## Working on the compositor

This section is for someone changing the compositor's code rather than configuring it.

### Finding the KDOS additions

The KDOS code is easy to find. It lives in sixteen `src/desktop/kdos-comp/src/kdos-*.c` files plus one
shared header, `src/desktop/kdos-comp/include/kdos.h`. Upstream files carry only small hooks, each marked with a comment beginning
`/* KDOS` — grep for that to find every touch point. There are 107 such markers across 29 upstream
files, 29 of them in `main.c`.

| File (under `src/desktop/kdos-comp/src/`) | What it adds |
|---|---|
| `kdos-config.c` | The KDOS configuration file and its reload |
| `kdos-child.c` | [Supervised children](#supervised-children): the panel, desktop icons and session daemons |
| `kdos-wallpaper.c` | [The wallpaper](#the-wallpaper), as part of the scene rather than a separate program |
| `kdos-crt.c` | [The phosphor pass](#the-phosphor-pass) |
| `kdos-frames.c` | [The frames socket](#the-frames-socket), reporting late frames |
| `kdos-idle.c` | [Idle, dim, lock and lid](#idle-dim-lock-and-lid): the idle policy |
| `kdos-lid.c` | Laptop lid behaviour |
| `kdos-cmd.c` | [The command socket](#the-command-socket) other KDOS programs query |
| `kdos-thumb.c` | [Window thumbnails](#window-thumbnails) for hover previews |
| `kdos-peek.c` | Fading windows to reveal the desktop |
| `kdos-appid.c` | Recording the application identifiers windows actually present |
| `kdos-boxchip.c` | [The box colour chip](#box-identity) on a title bar |
| `kdos-grant.c` | Per-box grants beyond the sandbox allowlist |
| `kdos-group.c` | [Window groups](#window-groups-and-window-memory) (tabbed stacks) |
| `kdos-layerfocus.c` | Closing menus and other on-demand surfaces when you click elsewhere |
| `kdos-winpos.c` | [Window memory](#window-groups-and-window-memory): reopening windows where they were |

### What the compositor does not decide

The compositor owns no window-model arithmetic. Where a new window lands, what a tiled state
becomes and what rectangle it occupies, which edge a moving edge stops against, and how workspace
stepping skips empty workspaces all come from the `libkwm` library. `kdos-comp` calls eight of its
functions: `kwm_place`, `kwm_tile_geom`, `kwm_tile_next`, `kwm_ws_adjacent`, `kwm_edge_check`,
`kwm_edge_best`, `kwm_clip_add` and `kwm_clip_sub`. The library is tested against a fixture with no
compositor running, so every one of those rules can be checked without booting anything.

What stays here is what only a compositor can do: walking its own window list, asking the
decoration how thick it is, working out which edges are actually visible, and deciding how a drag
feels as it crosses one. See [The window model](../03-architecture/window-model.md).

## See also

- [The session](../03-architecture/session.md) — what starts the compositor, and what it starts
- [The desktop](../02-user-guide/desktop.md) — the bindings and the user-facing behaviour
- [Theming](../02-user-guide/theming.md) — the accent, the wallpaper and the phosphor pass from the user's side
- [The window model](../03-architecture/window-model.md) — the arithmetic `libkwm` owns
- [The design language](../03-architecture/design-language.md) — why the frame looks like that
- [kdos-shell](kdos-shell.md) — the chrome it supervises
- [The kdos command](kdos-command.md) — `kdos hey` and `kdos stutter`
- [Configuration](../06-reference/configuration.md) — every key above, with defaults
