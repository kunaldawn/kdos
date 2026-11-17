# Shortcuts and window rules

Every desktop-wide key chord on KDOS is a KDE global shortcut, held by `kglobalaccel` inside KWin,
and every per-window setting is a KWin window rule. This page covers both as tasks: finding which
[shortcuts]{.idx term="global shortcut"} exist, the chords KDOS adds to KDE's, changing and removing
a chord, the touchpad and the mouse, [virtual desktops]{.idx term="virtual desktop"},
[window rules]{.idx term="window rule"} and the rule each box gets, the KDOS KWin scripts, and what
to check when a chord does nothing. The full table of shipped chords is in
[Keyboard shortcuts](../../kdos/02-user-guide/desktop.md#keyboard-shortcuts), and how KWin is set up
on KDOS is in [KWin](../../kdos/04-programs/kwin.md).

## Installation

| Port | Provides |
|---|---|
| [kwin](#port-kwin) | The compositor, window rules, virtual desktops and the scripting and effect interfaces |
| [kglobalacceld](#port-kglobalacceld) | The global shortcut daemon KWin loads |
| [systemsettings](#port-systemsettings) with [plasma-desktop](#port-plasma-desktop) | The **Shortcuts**, **Window Rules**, **Virtual Desktops**, **KWin Scripts**, **Mouse** and **Touchpad** pages |
| [kwin-effect-kdos-keys](#port-kwin-effect-kdos-keys) | The key card on `Meta+F1` and `Meta+/`: every global shortcut the session holds, searchable |
| [kwin-script-kdos-runraise](#port-kwin-script-kdos-runraise) | One key per KDOS terminal program, which raises its window or opens one in Konsole |
| [kwin-script-kdos-window-memory](#port-kwin-script-kdos-window-memory) | A program's window opens where its last one closed |
| [krohnkite](#port-krohnkite) | The auto-tiling script, installed and off |
| [kdos-appbox](#port-kdos-appbox) | Writes one window rule per box |

The KDOS defaults are in `/usr/share/kdos/xdg/`, the last of the configuration directories (see
[Session and environment](session-and-environment.md#where-the-kdos-defaults-live)). Nothing here
is enabled by hand.

## Configuration

### The KDOS shortcuts

KDE's defaults keep the window-management chords. KDOS rebinds two and adds the chords KDE leaves
free, from `fs/usr/share/kdos/xdg/kglobalshortcutsrc`:

<!-- kfs:include fs/usr/share/kdos/xdg/kglobalshortcutsrc lines=30-81 pin=39167e823377 -->

`kglobalaccel` reads `~/.config/kglobalshortcutsrc` alone, never a file in a configuration
directory. So `plasma-workspace/env/kdos-shortcuts.sh` copies each entry above into a person's
file before KWin starts, once: only an entry the person's file has no value for, and only one it
has not copied before. The copies are recorded in
`$XDG_STATE_HOME/kdos/kglobalshortcutsrc.seeded`, so a chord a person resets to KDE's default
stays reset. A `kglobalshortcutsrc` in `/etc/xdg` is a layer above KDOS's, read the same way.

Every chord passes `testing/chordcheck.py`, which holds it against KDE's compiled-in defaults,
every installed `X-KDE-Shortcuts` and fcitx5's hotkeys, and fails a chord the auto-tiler would
take from a binding in force once it is switched on.

`Meta+Shift+T` and `Meta+Ctrl+Shift+N` are not in that file: they are the desktop entries
`org.kdos.tiling.desktop` and `org.kdos.nightlight.desktop` in `/usr/share/kglobalaccel/`, from
[kdos-tools](#port-kdos-tools), running `kdos tiling toggle` and `kdos toggle night-light`.

A new account runs every `kconf_update` step at its first login, with KWin already up, and KWin's
`kwin-6.1-remove-gridview-expose-shortcuts` would then delete the Present Windows keys (`Meta+F9`,
`Meta+F10`, `Meta+F7`) the Window View effect has just registered.
`fs/usr/share/kdos/xdg/kconf_updaterc` records that step as done, and `kconf_update` reads its
record along the configuration directories:

<!-- kfs:include fs/usr/share/kdos/xdg/kconf_updaterc lines=24-25 pin=bc3cd6a55ee8 -->



### Virtual desktops

Four desktops in one row, from `fs/usr/share/kdos/xdg/kwinrc`:

<!-- kfs:include fs/usr/share/kdos/xdg/kwinrc lines=21-31 pin=ead3226488be -->

`Ctrl+F1` to `Ctrl+F4` switch to them; the Virtual Desktops page adds, removes and renames them.

### Window rules

KWin keeps window rules in `~/.config/kwinrulesrc`. A rule matches windows by class, title, role
or type and sets or forces any window property: position, size, desktop, layer, decoration,
colour scheme, focus. KDOS ships no rule of its own; kdos-appbox writes one per box, a group named
`kdos-box-<box>` matching the box's windows by class `^<box>(\.[^.]+)?$`, which forces the title
bar's colour scheme to the box's accent, or `KDOSBox` for a box with none. It rewrites only its own
groups and keeps every other rule where it was. A window that draws its own title bar takes no
decoration colour.

### The KDOS KWin scripts

The run-or-raise keys of [kwin-script-kdos-runraise](#port-kwin-script-kdos-runraise):

| Key | Program |
|---|---|
| `Meta+Shift+M` | `mc`, files |
| `Meta+Shift+E` | `aerc`, mail |
| `Meta+Shift+B` | `lynx`, the web |
| `Meta+Shift+U` | `rmpc`, music |
| `Meta+Shift+C` | `ikhal`, the calendar |
| `Meta+Shift+G` | `iamb`, chat |
| `Meta+Shift+W` | `micro`, the editor |

Pressed again from one of the program's windows, the key moves to the next. The window memory
script keeps one place per program, by application id, in `$XDG_STATE_HOME/kdos/window-memory`.
Both are on by default; the KWin Scripts page turns either off.

## Usage

### Finding the chords that exist

- `Meta+F1` or `Meta+/` opens the key card, grouped into Windows, Desktops, Launch, KDOS,
  Accessibility and System, with a search field. It asks kglobalacceld each time it opens, so it
  shows the chords in force now.
- System Settings → **Shortcuts** lists every component's actions and lets you search by key.
- From a terminal: `qdbus6 org.kde.kglobalaccel /kglobalaccel allMainComponents`.

### Changing a shipped binding

1. System Settings → **Shortcuts**.
2. Find the action, for example **KWin → Close Window**.
3. Click the key field and press the new chord, or **Add** a second one. A chord another action
   holds is reported, with the choice to take it.
4. **Apply**. It applies at once and is saved in `~/.config/kglobalshortcutsrc`.

**Use Default** puts KDE's chord back; a KDOS chord reset that way is not copied in again at the
next login.

### Removing a binding

On the Shortcuts page, select the action and clear its key field (the cross beside it), then
**Apply**.

### Binding a command

1. System Settings → **Shortcuts** → **Add New** → **Command or Script**.
2. Give the command, for example `kdos theme amber`, and a chord.
3. **Apply**. The entry is a desktop file in `~/.local/share/applications/` with the chord
   recorded against it.

### Adding a window rule

1. Right-click a window's title bar → **More Actions** → **Configure Special Window Settings**.
   The rule starts filled in with that window's class and title.
2. Add a property, for example **Virtual Desktop**, set it to **Force** and the value, and
   **Apply**.

System Settings → **Window Rules** lists every rule, a box's among them, and imports and exports
them as files.

### Turning on auto-tiling

```bash
kdos tiling on      # or off, or toggle
```

or turn **Krohnkite** on on the KWin Scripts page. While it is on, windows tile by themselves;
the eight chords it would ask for that the session already holds stay unbound until you bind
them.

### Setting the touchpad and the mouse

System Settings → **Touchpad** and **Mouse**: tap to click, natural scrolling, speed and
acceleration, per device. KWin applies them at once and keeps them in `~/.config/kcminputrc`.

## Tips

- A chord that a program grabs itself, such as a terminal's `Ctrl+Shift+C`, reaches the program
  only when no global shortcut holds it.
- `kdos tiling on --only www,hack` tiles only the virtual desktops named, by their names in the
  running session; Krohnkite leaves every other desktop floating.
- The key card is an effect: System Settings → **Desktop Effects** → **KDOS Key Card** turns it
  off, and its chords go with it.

## Troubleshooting

**A chord does nothing.**

1. Open the key card and search for the chord. A chord listed under another action is held by it.
2. Check that no application grabbed the keyboard: a virtual machine viewer or a game in full
   screen keeps chords from KWin while it has focus.
3. `qdbus6 org.kde.kglobalaccel /component/kwin shortcutNames` lists KWin's own actions; an
   action missing there belongs to a script or effect that is off.

**A KDOS chord is missing after an account was copied from another machine.** The copy brought a
`kglobalshortcutsrc.seeded` record that says the chord was already given. Delete
`~/.local/state/kdos/kglobalshortcutsrc.seeded` and log in again; only entries your file has no
value for are copied.

**A boxed window's title bar is not tinted.** The window draws its own title bar, or its box has
no rule: `grep -A6 kdos-box- ~/.config/kwinrulesrc`. `kdos-appbox` writes the rule when it
generates the box's launchers.

## Known gaps

- [Known gaps](../../kdos/06-reference/known-gaps.md): the desktop's open items

## See also

- [Keyboard shortcuts](../../kdos/02-user-guide/desktop.md#keyboard-shortcuts): every shipped chord
- [KWin](../../kdos/04-programs/kwin.md): KWin as KDOS configures it, its scripts and effects
- [The window model](../../kdos/03-architecture/window-model.md): placement, tiling and snapping
- [Session and environment](session-and-environment.md), [Accessibility](accessibility.md),
  [Input methods](input-methods.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 438.* Previous: [437. Session and environment](session-and-environment.md) · [Contents](../README.md) · Next: [439. The panel and the launcher](panel-and-launcher.md)
