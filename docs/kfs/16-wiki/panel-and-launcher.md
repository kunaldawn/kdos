# The panel and the launcher

The panel, the desktop and the widgets on them are [plasmashell]{.idx}; the menu is Kickoff, the
Application Launcher; and the search that runs commands, finds files and starts programs is
[KRunner]{.idx}. This page covers the three KDOS desks and how to switch between them, the pinned
applications, the system tray's KDOS entries, adding an application to the menus, and the KDOS
runner's commands. How the shell is set up on KDOS is in
[The Plasma shell](../../kdos/04-programs/plasma-shell.md).

## Installation

| Port | Provides |
|---|---|
| [plasma-workspace](#port-plasma-workspace) | `plasmashell`, KRunner, the task manager, the system tray and the clock |
| [plasma-desktop](#port-plasma-desktop) | Kickoff, the pager, the panel and desktop containments, and the **Menu Editor**'s pages |
| [kmenuedit](#port-kmenuedit) | The Menu Editor, for the application menu |
| [kdeplasma-addons](#port-kdeplasma-addons) | The extra widgets and KRunner plugins |
| [kdos-plasma-look](#port-kdos-plasma-look) | The three desks, `org.kdos.desk.kdos`, `org.kdos.desk.dock` and `org.kdos.desk.tiling`, and the `kdos` Plasma style the panel is drawn in |
| [kdos-runner](#port-kdos-runner) | The KDOS command palette, `ask` for the local language model and the offline library in KRunner |
| [plasmoid-kdos-checklist](#port-plasmoid-kdos-checklist) | The Getting Started tray entry |
| [plasmoid-kdos-privacy](#port-plasmoid-kdos-privacy) | The privacy lamps in the tray, shown only while something captures the microphone, camera or screen |
| [kdos-welcome](#port-kdos-welcome) | The KDOS pages of the Welcome Center, among them the desk picker |
| [kdos-tools](#port-kdos-tools) | `kdos desk` and `kdos tiling` |

plasmashell is started by the session and is not a service.

## Configuration

### The desks

A desk is a Plasma layout template: every panel goes, and the template lays the new ones out on
each screen.

| Desk | Panels | Windows |
|---|---|---|
| `kdos` | One opaque bottom panel: Start (the mark and its label), the virtual desktops, the pinned applications, the meters, the tray and the clock | Floating, with snapping |
| `dock` | A top bar with Start, the focused window's application menu, the tray and the clock, and a floating dock of the pinned applications that moves out of a window's way | Floating, with snapping |
| `tiling` | One thin top bar: Start, the desktops by name, the window list, the processor and memory as text, the tray and the clock | Tiled by Krohnkite |

A new account starts on the `kdos` desk, and the Welcome Center's Desk page offers the others at
the first login. What a person changes afterwards is saved in
`~/.config/plasma-org.kde.plasma.desktop-appletsrc` and `~/.config/plasmashellrc`.

### The pinned applications

The `kdos` and `dock` desks pin the first word of each line of
[`~/.config/kdos/favorites`]{.idx term="~/.config/kdos/favorites"} when the desk is laid out. The
same file names the boxes warmed up at login, and a `code=XX` word on a line is the two letters
that open the entry from KRunner. From `fs/etc/skel/.config/kdos/favorites`:

<!-- kfs:include fs/etc/skel/.config/kdos/favorites lines=8-33 pin=4cece8b412cd -->

Pinning or unpinning a launcher on the panel afterwards changes the panel alone, not this file.

### The menus

Kickoff and KRunner read the menu `plasma-applications.menu`, named by `XDG_MENU_PREFIX=plasma-`,
with every file in `/etc/xdg/menus/applications-merged/` merged into it. KDOS ships
`kdos-boxes.menu` there, which puts the launchers of boxed applications in their categories, and
the offline library adds `kdos-library.menu`. Each launcher is a desktop entry under
`/usr/share/applications`, `~/.local/share/applications` or another `applications` directory of
`XDG_DATA_DIRS`. The menu is cached by `kbuildsycoca6`, which Plasma reruns when a directory
changes.

## Usage

### Switching desks

```bash
kdos desk                 # which desk this is: kdos, dock or tiling
kdos desk dock            # lay the panels out as the Dock desk
kdos desk undo            # put the last panels back
```

`kdos desk <name>` copies the panels file to an undo set and to a dated backup first, and prints
the backup's path. The `tiling` desk turns auto-tiling on and the other two turn it off. The
Welcome Center's Desk page (open it from Kickoff as **Welcome Center**) does the same with a
preview.

### Opening the launcher and searching

- `Meta` alone, or a click on Start, opens Kickoff. Typing searches at once.
- `Alt+Space`, `Alt+F2` or `Meta+Space` open KRunner on its own.

KRunner's KDOS command palette understands the `kdos` verbs with their arguments filled in, for
example `accent amber`, `tiling off`, `desk dock`, `update check` or `why /etc/fstab`; the two
letters of a favourite's `code=`; KDOS's own System Settings pages by what they are for; and
`keys <words>`, which lists the chords that match. A query beginning with `?` asks only the
offline library: the KDOS books, the manual pages, Zeal's docsets and the Kiwix archives.

### Adding an application to the menus

1. Write a desktop entry in `~/.local/share/applications/`, for example `mytool.desktop`:

   ```ini
   [Desktop Entry]
   Type=Application
   Name=My Tool
   Exec=mytool %F
   Icon=utilities-terminal
   Categories=Utility;
   ```

   `Terminal=true` runs it in Konsole.
2. It appears in Kickoff and KRunner within a few seconds. `kbuildsycoca6` rebuilds the cache by
   hand if it does not.

The Menu Editor (right-click Start → **Edit Applications**) does the same without a file, and
hides or moves an entry.

A boxed application's launcher is written by `kdos-appbox` when the application is installed;
see [Boxes](boxes.md).

### Adding a widget or a panel

Right-click the panel or the desktop → **Enter Edit Mode** (or **Add Widgets**). Drag a widget
onto the panel or the desktop; **Add Panel** adds another panel. **Exit Edit Mode** keeps the
result.

## Tips

- The privacy lamps appear in the tray only while a capture is running; a click mutes the
  microphone or stops a screen or camera capture, and the middle button lists what is capturing.
- The Getting Started entry hides itself once every item is done or dismissed.
- A desk replaces the panels whole. `kdos desk undo` brings back the panels from before the last
  switch; older ones are in the dated backups the command printed.

## Troubleshooting

**The panel is gone or broken.** Restart the shell: `plasmashell --replace &` from Konsole or
KRunner. If it comes back broken, lay a desk out again with `kdos desk kdos`.

**A new application is not in the menu.** Check the entry: `desktop-file-validate
~/.local/share/applications/mytool.desktop`, and that its `Exec` program is on `PATH`. Then run
`kbuildsycoca6`.

**A KDOS command does nothing in KRunner.** The runner is started by the session bus on the first
query: `qdbus6 io.kdos.Runner` lists its objects when it runs. Its output is in the session bus's
log, `$XDG_RUNTIME_DIR/ksvc/kdos-svc.dbus.log`.

## Known gaps

- [Known gaps](../../kdos/06-reference/known-gaps.md): the desktop's open items

## See also

- [The Plasma shell](../../kdos/04-programs/plasma-shell.md): the desks, the panel, Kickoff, KRunner and the KDOS widgets
- [The desktop](../../kdos/02-user-guide/desktop.md): the desktop from a user's side
- [Shortcuts and window rules](shortcuts-and-window-rules.md), [Boxes](boxes.md),
  [Session and environment](session-and-environment.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 437.* Previous: [436. Shortcuts and window rules](shortcuts-and-window-rules.md) · [Contents](../README.md) · Next: [438. Xwayland and X11 applications](xwayland-and-x11-apps.md)
