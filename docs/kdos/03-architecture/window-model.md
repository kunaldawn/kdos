# The window model

This chapter describes where a window goes on a KDOS desktop and what moves it: how a new window is
placed and focused, how it snaps to an edge or a corner, the custom tiles and the automatic tiler,
the virtual desktops, the window switcher and Overview, the keys that drive all of it, the rules
KDOS writes for boxed applications' windows, and more than one screen. It is written for a person
who wants to know why a window landed where it did, and for a contributor changing KDOS's window
defaults.

Windows are managed by KWin, Plasma's compositor and window manager, and every mechanism on this
page is KWin's. KDOS does not patch where KWin puts windows; it ships defaults, two KWin scripts, a
KWin effect, a bundled tiling script, and the window rules for boxes. The user's side of the same
behaviour (the shortcut table, the panel, the desks) is in [The desktop](../02-user-guide/desktop.md),
and why windows float rather than tile by default is in
[Decisions](../01-philosophy/decisions.md#floating-windows-with-smart-snap-tiling-on-request).

## What KDOS sets, and where

| What | File | Read by |
|---|---|---|
| Four virtual desktops in one row, one window switcher entry per application, the input method, the auto-tiler's switch | `/usr/share/kdos/xdg/kwinrc` | KWin |
| Centred placement, the thumbnail-grid window switcher, the Breeze decoration at normal borders | Each global theme's `contents/defaults` | Plasma, which writes them into the account's defaults when the theme is applied |
| Square corners, the outline, the title alignment | `/usr/share/kdos/xdg/breezerc` | The Breeze decoration |
| The shortcuts KDOS adds or moves | `/usr/share/kdos/xdg/kglobalshortcutsrc` | `plasma-workspace/env/kdos-shortcuts.sh`, which seeds them into the account's own file at login |
| The colour of each box's title bars | `~/.config/kwinrulesrc`, groups `kdos-box-<box>` | KWin's window rules |
| Window memory, run-or-raise, the key card | KWin scripts and a KWin effect under `/usr/share/kwin/` | KWin |
| Automatic tiling | The Krohnkite script, `/usr/share/kwin/scripts/krohnkite/`, off | KWin, while `kwinrc [Plugins] krohnkiteEnabled` is true |

Every value KDOS ships is a default in the sense of
[the design language](design-language.md#where-the-defaults-live): the account's own setting,
written by any System Settings page, outranks it.

## Focus and stacking

KDOS ships no focus setting, so KWin's own hold: a click focuses a window and raises it, a newly
opened window takes the focus unless KWin's focus-stealing prevention judges it an interruption of
the window being typed into, and a dialog stays above the window it belongs to and moves to another
desktop with it. Global shortcuts are KWin's: KWin holds the keyboard before any application sees a
key, so a chord bound in System Settings works whichever window has the focus, and the input method
receives keys only after KWin has passed on them.

## Placement

A new window with no remembered place opens **centred** on the screen that has the focus: every
global theme sets `kwinrc [Windows] Placement=Centered`, which is also KWin's own default. A Wayland
client cannot choose its own position, so on KDOS where a window first appears is always KWin's
decision, or the window memory's below.

## Window memory

`kwin-script-kdos-window-memory` makes a program's window open where its last window closed, at the
same size, and maximised when it closed maximised. It is a KWin script, on by default and turned off
on System Settings' KWin Scripts page. It records one place per program, keyed by the window's
desktop-file name (its class when it has none), in `$XDG_STATE_HOME/kdos/window-memory`, most
recent first and at most 200 programs; the file is written through a temporary file and a rename,
so a crash leaves the old record or the new one.

What it leaves alone:

- a window that opens maximised, full screen or tiled;
- a dialog, a panel, or any window that is not a program's ordinary top-level window, because a
  dialog carries its parent's application id and remembering one would put the next document
  window where the last dialog was;
- a window whose remembered top-left corner is within eight pixels of an open window of the same
  program's, which would hide that window; KWin's own placement handles it;
- the programs on its exclusion list, which place their own windows or are prompts: the panel,
  KRunner, Spectacle, Yakuake, the polkit and wallet prompts, the Welcome tour and the Xwayland
  video bridge.

A remembered place is moved and shrunk into the usable area of the screen nearest to it, so a
window remembered on a screen that is gone, or on a wider one, opens whole on one that is there. A
window rule that forces a position wins over it, because KWin refuses the move. Two settings in
`kwinrc [Script-kdos_window_memory]` change it: `Exclude`, a comma-separated list of application ids
that replaces the built-in one, and `RememberDesktop`, off by default, which also moves a window
back to the virtual desktop it closed on.

## Snapping and custom tiles

Windows float, and snap. These are KWin's defaults and KDOS leaves them on:

| Gesture | Result |
|---|---|
| Drag a window to the top edge | Maximise |
| Drag to the left or right edge | The half of the screen on that side |
| Drag to a corner | That quarter |
| Meta+Left, Right, Up, Down | The same halves and quarters from the keyboard |
| Meta+PgUp, Meta+PgDown | Maximise, minimise |
| Meta+T | The tile editor: divide the screen into custom tiles |
| Drag a window with Shift held | Drop it into one of the custom tiles |

Custom tiles are kept per virtual desktop and per screen, so a desktop used for writing can be
split in two and another in three columns. A tiled window that is dragged away returns to the size
it had before it was tiled.

## Auto-tiling

The automatic tiler is **Krohnkite**, a KWin script (the maintained fork on Codeberg) that places
every new window into a layout (tile, monocle, three columns, spiral and others) and moves,
resizes and swaps them from the keyboard. It is bundled and **off**: KWin loads it only while
`kwinrc [Plugins] krohnkiteEnabled` is true, and the shipped `kwinrc` says false.

One switch turns it on and off: `kdos tiling on|off|toggle`, which writes that key and asks KWin to
reconfigure, loading or unloading the script at once. The same switch is Meta+Shift+T (the action
"Toggle Auto-Tiling", a KDOS desktop entry under `/usr/share/kglobalaccel` running `kdos tiling
toggle`), a checkbox on the Welcome tour's desk page, and part of the desks: the Tiling desk turns
it on and the KDOS and Dock desks turn it off, so a desk is the whole arrangement and not half of
one.

`kdos tiling on --only <desktop>[,<desktop>…]` tiles only the named virtual desktops and leaves
windows floating on the rest. Krohnkite itself names the desktops it ignores
(`kwinrc [Script-krohnkite] ignoreVDesktop`), so the command writes the list of every *other*
desktop's name, read from KWin; it therefore needs a running session. A plain `on` clears the list.

Krohnkite's own default shortcuts collide with keys KDE and KDOS already use, so the shipped
`kglobalshortcutsrc` leaves seven of them unbound: Focus Next (Meta+., the emoji picker), Focus
Right (Meta+L, the lock), Decrease (Meta+D, Show Desktop), Rotate and Rotate Part (Meta+R and
Meta+Shift+R, Spectacle's recording), Set Master (Meta+Return, Konsole) and Select Layout – Tile
(Meta+T, the tile editor). Each can be bound on the Shortcuts page. The key card leaves out
Krohnkite's rows while the script is off, because kglobalaccel goes on listing a script's keys after
it is unloaded.

## Virtual desktops

There are four virtual desktops in one row, named `main`, `www`, `hack` and `misc`, and the KDOS
desk's panel shows them in a pager. Ctrl+F1 to Ctrl+F4 switch to each, and Meta+Ctrl+Left and Right
step between them; Overview's grid shows all four. Each desktop has its own custom tiles, and
Krohnkite can be confined to some of them (above).

Plasma's activities service runs, because Dolphin, Gwenview and the recent-documents lists use it,
but no activity switcher is offered: its key, Meta+Q, closes a window instead.

## Overview and switching windows

| Key or gesture | Result |
|---|---|
| Meta+W, or the pointer pushed into the top-left corner | Overview: every window of this desktop, with a search field |
| Meta+G | The grid of every virtual desktop |
| Alt+Tab, Meta+Tab | Walk the applications, one entry each for its most recent window, shown as a grid of thumbnails |
| Alt+\`, Meta+\` | Walk the windows of the current application |
| Meta+1 to Meta+9 | The task manager's entry in that position |
| Meta+D | Show the desktop |

## Run or raise

`kwin-script-kdos-runraise` gives each of KDOS's terminal programs one key. The key raises the
program's window when one is open and opens one in Konsole when none is; pressed again from one of
its windows it moves to the next, so repeated presses walk all of them.

| Key | Program |
|---|---|
| Meta+Shift+M | `mc`, files |
| Meta+Shift+E | `aerc`, mail |
| Meta+Shift+B | `lynx`, the web |
| Meta+Shift+U | `rmpc`, music |
| Meta+Shift+C | `ikhal`, the calendar |
| Meta+Shift+G | `iamb`, chat |
| Meta+Shift+W | `micro`, the editor |

Konsole gives every window one application id, so a program's window is found by its caption: a
window the key opened runs `konsole -p tabtitle=<program> -e <program>`, and a window opened any
other way carries Konsole's own tab title `<directory> : <program>`; both are matched. A KWin
script cannot start a program, so each program has a desktop entry under `/usr/share/kglobalaccel`
that the script invokes over D-Bus to open a window. The keys are KWin actions and can be changed on
the Shortcuts page.

## The shortcuts KDOS changes

KDE's defaults own the window-management keys. KDOS adds keys only where KDE leaves the chord
free, and moves two of KDE's:

| Chord | Action | KDE's own use of it |
|---|---|---|
| Meta+Q | Close the window (beside Alt+F4) | The activity switcher, which KDOS does not offer |
| Meta+F1 | Open the key card | Switch to desktop 1, which keeps Ctrl+F1 |
| Meta+/ | Open the key card | None |
| Meta+Space | KRunner (beside Alt+Space, Alt+F2 and Search) | None |
| Meta+Return | Konsole (beside Ctrl+Alt+T) | None |
| Meta+Shift+T | Toggle auto-tiling | None |
| Shift+Print, Meta+Shift+S | Spectacle: a rectangular region (beside Meta+Shift+Print) | None |
| Alt+Print, Meta+R | Spectacle: record a region (beside Meta+Shift+R) | None |
| Meta+Ctrl+Shift+N | Turn Night Light on or off | None |

Print opens Spectacle. Phosphor mode has no key, so a stray chord cannot flash the whole screen.
Meta+Space is also fcitx5's default key for switching input-method groups; KWin sees every key
first, so the shipped `/etc/xdg/fcitx5/config` moves fcitx5's group switch to Meta+Alt+Space and
Meta+Alt+Shift+Space.

kglobalaccel reads only the account's own `~/.config/kglobalshortcutsrc`, never one in a
configuration directory, so `plasma-workspace/env/kdos-shortcuts.sh` copies each shipped entry into
it at login, once per entry and only where the account has no value of its own; a key a person
changes or resets stays as they set it. `testing/chordcheck.py` fails a chord with two owners
among everything KDOS ships, KDE's defaults and fcitx5's.

The **key card** (`kwin-effect-kdos-keys`) is a full-screen, searchable card of every global
shortcut the session holds, grouped into Windows, Desktops, Launch, KDOS, Accessibility and System.
It asks kglobalaccel each time it opens, so a chord changed on the Shortcuts page is on the card
the next time, and it opens under software rendering too.

## Window rules KDOS writes

A boxed application's windows are told apart from the host's by the box's colour. `kdos-appbox`
writes one KWin window rule per box into `~/.config/kwinrulesrc`, in a group named
`kdos-box-<box>`, which forces the title bar's colour scheme: `KDOS<Title>` of the accent in the
box's profile (`accent =` in `~/.config/kdos/boxes/<box>.conf`), or `KDOSBox` for a box with none.

The rule is per box and not per application because of how KWin names a sandboxed window: it
replaces the window's own application id with the security context's app id, which `kdos-boxsock`
sets to the box's launcher id (`<box>` for a box with one launcher, `<box>.<stem>` for each of a box
with several), and takes the window's class and desktop-file name from it. A rule keyed on the
application's upstream class would therefore match no boxed window; one keyed on
`^<box>(\.[^.]+)?$` matches every window of every launcher in the box and nothing else.

Only the `kdos-box-*` groups are `kdos-appbox`'s. It drops and rewrites them whole and keeps every
other group and key of the file exactly as it was, with the user's own rules first in the order
lists, so rules made on System Settings' Window Rules page are never touched. A window that draws its
own title bar takes no decoration colour, and KWin has no rule that would outline it instead; such
a window is still marked by its entry in the task manager. A window whose frame KWin draws also
carries the box glyph Breeze puts before a sandboxed window's caption. What the tag and the glyph
do and do not guarantee is in [Sandboxed clients](security-model.md#sandboxed-clients).

## More than one screen

Screens are arranged by KScreen, on System Settings' Display Configuration page, which writes the
layout per set of connected screens and restores it when the same set returns. A new window opens on
the screen that has the focus. The window memory moves a remembered window onto the nearest screen
that exists. The `kdos-addons` set carries `org.kdos.activescreen`, a KWin script, off until it is
enabled on the KWin Scripts page, that opens each new window on the screen under the pointer
instead, and `org.kdos.pinpip`, which keeps a browser's picture-in-picture video above every window
on every desktop.

## X11 windows

A program with no Wayland path runs through Xwayland, which KWin starts when the first X11 client
connects. KWin manages its windows like any other: they snap, tile, follow the rules and appear in
Overview. An X11 window carries no security-context tag (its surface is Xwayland's, one client to
KWin), so a boxed X11 application is not marked as sandboxed and none of the box's Wayland
restrictions apply to it.

## See also

- [The desktop](../02-user-guide/desktop.md) — the desks, the panel and the full table of keys, as a user meets them
- [KWin](../04-programs/kwin.md) — the compositor itself, its settings, the KDOS scripts and effects, and how to debug them
- [The design language](design-language.md) — the decoration, the colours and the desks
- [Decisions](../01-philosophy/decisions.md#floating-windows-with-smart-snap-tiling-on-request) — why windows float and tiling is on request
- [The security model](security-model.md#sandboxed-clients) — what KWin refuses a boxed window
- [kdos-appbox](../04-programs/kdos-appbox.md) — box profiles and the launcher ids the rules are keyed on
- [The kdos command](../04-programs/kdos-command.md) — `kdos tiling` and `kdos desk`
- [Configuration](../06-reference/configuration.md) — `kwinrc`, `kglobalshortcutsrc` and the other files KDOS ships
- [Glossary](../06-reference/glossary.md) — KWin, window rule, virtual desktop and the other terms used here

<!-- book-nav -->
---

*Part III — Architecture, chapter 19.* Previous: [18. The design language](design-language.md) · [Contents](../README.md) · Next: [20. The programs](../04-programs/README.md)
