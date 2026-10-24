# The panel and the launcher

This page covers the [panel]{.idx} at the edge of the screen and the three ways of starting an
application from it or near it: the Start menu, the [launcher]{.idx} on `Super+D` and the command palette
on `Super+Space`. It gives the files that decide what the panel shows and where it sits, and the
procedures for moving and resizing the bar, choosing its widgets, pinning applications, adding an
application or a terminal program to the menus, giving a place in the system a route name, and
bringing back a panel that has gone. How each surface behaves is described in
[kdos-shell](../../kdos/04-programs/kdos-shell.md#the-panel); this page gives the procedures.

## Installation

All of it is one program. [kdos-shell](#port-kdos-shell) installs `/usr/bin/kdos-shell` and links
it under every surface's name, among them `kdos-start` (the Start menu), `kdos-launcher`,
`kdos-palette`, `kdos-menu` and `kdos-settings`; run as `kdos-shell`, it is the panel. It is built
in the `50_desktop` phase and is on every image.

The panel is not a service. [kdos-comp](#port-kdos-comp) starts one panel per screen when the
session begins, passes it the settings from `comp.conf` on its command line, and starts it again
if it exits. The Start menu, the launcher and the palette are started afresh by their chords each
time they open.

The per-user files come from the base filesystem's [`/etc/skel/.config/kdos/`]{.idx term="/etc/skel/.config/kdos"}, copied when the
account is created; [`/etc/kdos/menu.conf`]{.idx} is the base filesystem's own. The live medium and an
installed system ship the same files, and on the live medium an edit under `~/.config` lasts until
power-off.

## Configuration

### The panel's shape: ~/.config/kdos/comp.conf

Where the bar sits, how tall it is, its font, margin, opacity and autohide are compositor keys,
because they are the panel's command line. Every key ships commented out at its default:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=34-77 pin=79441209ff98 -->

The clock's format is in the same file:

<!-- kfs:include fs/etc/skel/.config/kdos/comp.conf lines=123-126 pin=a05a25d2e697 -->

These keys are read when the session starts, so a change takes effect at the next login. A reload
logs the change and keeps the running value until then. Every key and its range is in
[Keys applied at the next login](../../kdos/04-programs/kdos-comp.md#keys-applied-at-the-next-login).

### What the panel shows: ~/.config/kdos/panel.conf

The widgets on the right, which of them sit behind the chevron, the meters on the second row,
labels on window buttons and the Start button's word. The panel reads this file when it starts and
again on `SIGHUP`:

<!-- kfs:include fs/etc/skel/.config/kdos/panel.conf -->

The widget and meter names, and what an empty value means for each key, are in
[`~/.config/kdos/panel.conf`](../../kdos/06-reference/configuration.md#configkdospanelconf).

### Pinned applications: ~/.config/kdos/favorites

One desktop-entry identifier per line. The Start menu's pinned column, the palette and the panel's
quick-launch row read the first word of each line and ignore a `code=` after it:

<!-- kfs:include fs/etc/skel/.config/kdos/favorites -->

### Where applications are found

The menus, the launcher and the palette read desktop entries from the `applications` directory
under each of `$XDG_DATA_HOME` and `$XDG_DATA_DIRS`, which the login shell sets:

<!-- kfs:include fs/etc/profile.d/10-wayland.sh lines=50-55 pin=3d54cc20891a -->

So `~/.local/share/applications` holds an account's own entries, and `/usr/share/applications`
and `/usr/local/share/applications` the system's; the image fills only the first of the two
system directories, and `/usr/local/share/applications` does not exist until something creates
it. The launchers for boxed applications are
written into `~/.local/share/applications` by `kdos-appbox`; see
[kdos-appbox](../../kdos/04-programs/kdos-appbox.md).

### Routes: /etc/kdos/menu.conf

A route is a stable name for a place in the system. The system file is read first and
`~/.config/kdos/menu.conf` second, so a route of yours replaces the system's or adds a new one:

<!-- kfs:include fs/etc/kdos/menu.conf lines=16-45 pin=8c6d0cc82e3b -->

### ~/.config/kdos/launcher.conf

The settings window writes a `files` key here; no surface reads it (see [Known gaps](#known-gaps)):

<!-- kfs:include fs/etc/skel/.config/kdos/launcher.conf -->

## Usage

### Moving the panel, resizing it, hiding it

1. Open `~/.config/kdos/comp.conf` and uncomment the keys you want, for example:

   ```ini
   panel = top
   panel_cells = 1
   panel_autohide = yes
   ```

2. End the session and log in again. The compositor passes the new values to the panel when it
   starts it.

`panel = off` starts no panel at all.

To put the bar away for a while without editing anything, press `Super+Shift+Space`, or run:

```bash
kdos panel toggle
```

The same chord brings it back. While it is put away, the pointer at the screen edge does not bring
it back, even with autohide on.

### Choosing the panel's widgets and meters

1. Open **Settings** (`Super+I`) on its Panel page, or run `kdos-settings --page panel`. It writes
   `~/.config/kdos/panel.conf` and signals the panel itself.
2. Or edit the file by hand. For example, to put the stutter chip on the bar and chart disk use in
   place of the network:

   ```ini
   overflow = restart clipboard
   meters   = cpu ram disk
   ```

3. After a hand edit, signal every panel; `-x` matches the panels and none of the other surfaces:

   ```bash
   pkill -x -HUP kdos-shell
   ```

The bar on the screen changes at once. A misspelt name is reported in the compositor's log and
skipped.

### Pinning an application

1. Open the Start menu (`Super+A`), find the application, and click the pin mark at the right edge
   of its row. The menu appends the bare identifier to `~/.config/kdos/favorites`, so it appears
   in the Start menu's pinned column and on the panel.
2. Drag a pinned button along the panel to reorder it; the order is written back to the same file.
3. To unpin, right-click the pinned button while the application is not running, or choose
   **Unpin from taskbar** in a running window's menu.

To give a pinned row a two-letter code that opens it from an empty search, add `code=XX` to its
line by hand. The row then opens from the Start menu and the palette by typing the two letters.

### Adding an application to the menus

1. Write a desktop entry for it in your own applications directory, for example
   `~/.local/share/applications/mytool.desktop`:

   ```ini
   [Desktop Entry]
   Type=Application
   Name=My Tool
   Comment=What the tool is for
   Exec=/home/alice/bin/mytool %f
   Icon=utilities-terminal
   Terminal=false
   Categories=Utility;
   ```

2. Open the Start menu or press `Super+D` and type its name. Each of them reads the entries when it
   opens, so nothing has to be reloaded.

The `Exec` line is split with desktop-entry quoting and run without a shell, so a pipe, a
redirection or `$VARIABLE` in it is passed to the program as text. For a command that needs a
shell, write a script and name it in `Exec`. `Terminal=true` opens the program in the session's
terminal.

### Adding a terminal program as an application

`kdos app tui` writes the entry for you:

```bash
kdos app tui add "Disk usage" ncdu
kdos app tui ls
kdos app tui rm disk-usage
```

`add` prints the new entry's identifier, `kdos-tui-disk-usage` here, and writes
`~/.local/share/applications/kdos-tui-<slug>.desktop` with `Terminal=true`, where the slug is the
name in lower case with dashes; `rm` takes the slug. `add` also takes `--float`,
`--size COLSxROWS`, `--icon NAME` and `--category X`; see
[`kdos app`](../../kdos/04-programs/kdos-command.md#kdos-app).

### Opening a place by its route

```bash
kdos menu summon setup.network
kdos menu toggle system.monitor
```

`summon` opens the palette with the route's name already typed into its search, so the route's
row is the one selected, and it returns when the palette closes; `toggle` closes a palette that
is open and otherwise opens one. To add a route of your own, put a line in `~/.config/kdos/menu.conf`:

```ini
system.top = foot -e btop
```

The value is split on spaces and run without a shell or quoting. A route of yours with the same
name as a system route replaces it; no route can be deleted.

### Changing the clock

1. Set `clock_format` in `~/.config/kdos/comp.conf` to a `strftime` format, for example
   `clock_format = %a %d %H:%M`.
2. Log in again.

## Tips

- `Super+Space` searches open windows, applications, routes, settings pages and chords, and files
  under your home folder from the third character; `Super+D` searches applications only.
- An identifier in `favorites` with no installed entry is skipped without a message, so pinning
  an application that is not installed yet leaves no dead button.
- Typing `wifi`, `bluetooth` or a route name such as `setup.network` into the Start menu's search
  finds the fixed rows as well as applications.
- `kdos-shell --dump` prints one frame of the bar as text, two rows 100 columns wide, and exits; it needs no
  display.
- Middle-click a window button for another instance of the same application.

## Troubleshooting

**The panel is not on the screen.**

1. Press `Super+Shift+Space`; a panel put away with the chord comes back only with the chord.
2. Check `panel` in `~/.config/kdos/comp.conf`; `off` or `none` starts no panel.
3. Check whether it is running, and why it stopped:

   ```bash
   pgrep -ax kdos-shell
   grep kdos-shell "$XDG_RUNTIME_DIR/kdos-comp.log" | tail
   ```

   A running panel shows the settings the compositor passed it, one line per screen, such as
   `kdos-shell --output Virtual-1 --font Terminus:pixelsize=20 --bottom --cells 2 --margin 0 --opacity 80`.
   A panel that exits is started again within a few seconds, under a new process id.

   A panel that exits more than five times within 30 seconds is not started again, and a dialog
   names it. `kdos-comp -r` in a terminal the compositor started, or `pkill -HUP -x kdos-comp`
   from any shell, gives it another run of attempts. `kdos doctor` also reports a session with no
   `kdos-shell` running.

**A change to `comp.conf` did nothing.** The panel keys apply at the next login. The compositor's
log names every line it ignored, with the file and line number:

```bash
grep comp.conf "$XDG_RUNTIME_DIR/kdos-comp.log"
```

**A change to `panel.conf` did nothing.** The panel re-reads the file only on `SIGHUP`; run
`pkill -x -HUP kdos-shell`. A name the panel does not know is reported in the log as
``panel.conf: no widget named `x` — ignored`` or ``no meter named `x` — ignored``.

**An application is missing from the menus.** Check that its entry is in one of the directories
above, has a `Name=` line, and has no `NoDisplay=true` or `Hidden=true`. A boxed application's entry is written when it is installed; see
[Applications](../../kdos/02-user-guide/applications.md).

**A menu row or a route opens nothing.** A program started from a menu has no terminal attached,
so a terminal program needs `Terminal=true` in its entry, or a route that opens a terminal. Two
shipped routes are known to show nothing; see [Known gaps](#known-gaps).

## Known gaps

- [The launcher's `files` setting does nothing](../../kdos/06-reference/known-gaps.md#the-launchers-files-setting-does-nothing)
- [Two shipped chords and two routes show nothing](../../kdos/06-reference/known-gaps.md#two-shipped-chords-and-two-routes-show-nothing)
- [Workspace occupancy is inferred, not reported](../../kdos/06-reference/known-gaps.md#workspace-occupancy-is-inferred-not-reported)
- [A tray menu does not update while it is open](../../kdos/06-reference/known-gaps.md#a-tray-menu-does-not-update-while-it-is-open)
- [Only two ways of ending the session save the list of open windows](../../kdos/06-reference/known-gaps.md#only-two-ways-of-ending-the-session-save-the-list-of-open-windows)
- [KDOS's own surfaces have not been seen at a fractional scale](../../kdos/06-reference/known-gaps.md#kdoss-own-surfaces-have-not-been-seen-at-a-fractional-scale)

## See also

- [The panel](../../kdos/04-programs/kdos-shell.md#the-panel): every part of the bar, its layout and its degradation
- [kdos-start](../../kdos/04-programs/kdos-shell.md#kdos-start), [kdos-palette and kdos-launcher](../../kdos/04-programs/kdos-shell.md#kdos-palette-and-kdos-launcher): the menus and the searches
- [The panel](../../kdos/02-user-guide/desktop.md#the-panel) and [The Start menu](../../kdos/02-user-guide/desktop.md#the-start-menu) in the user's guide
- [`/etc/kdos/menu.conf`](../../kdos/06-reference/configuration.md#etckdosmenuconf-merged-under-configkdosmenuconf): every shipped route
- [Supervised children](../../kdos/04-programs/kdos-comp.md#supervised-children): how the compositor starts and restarts the panel
- [Session and environment](session-and-environment.md), [Compositor keybindings](compositor-keybindings.md),
  [Fonts and theming](fonts-and-theming.md), [Boxes](boxes.md), [Desktop troubleshooting](desktop-troubleshooting.md)

<!-- book-nav -->
---

*Part XVI. System administration, chapter 413.* Previous: [412. Compositor keybindings](compositor-keybindings.md) · [Contents](../README.md) · Next: [414. Xwayland and X11 applications](xwayland-and-x11-apps.md)
