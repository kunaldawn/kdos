# The desktop

This page is for anyone sitting at a KDOS desktop: what is on the screen, what each part of the
panel does when you click it, every keyboard shortcut the system ships, and how notifications,
files, locking, displays and removable media work. Read [Getting started](getting-started.md)
first if you do not yet have a running session.

After reading it you will be able to find and launch programs, arrange windows across workspaces,
change the panel and its shortcuts, and know which file to edit when you want something different.
Every file named here is described key by key in [Configuration](../06-reference/configuration.md).

## How the desktop is drawn

Almost everything KDOS draws is a grid of character cells in one palette, in the Terminus font:
the panel and everything that opens from it, the desktop icons, the lock screen, the boot splash
and the text console on `tty1`. The cells are 32 pixels tall for the popups, menus and desktop
icons, and 20 for the panel bar itself (see `panel_font` below). The drawing library behind all of them is `libktui`.

Three things are not on that grid:

- **The compositor's own chrome** — title bars, the compositor's menus and the window-switcher
  display. These are drawn as ordinary text at a size matched to one cell, so they line up with
  the grid without being part of it.
- **Applications running in a box.** A *box* is the container a graphical application runs in
  (see the [glossary](../06-reference/glossary.md)). It draws whatever its own toolkit draws.
- **Pictures** — icons and previews — which sit in whole cells.

[The design language](../03-architecture/design-language.md) explains why the desktop looks like
this, and [kdos-shell](../04-programs/kdos-shell.md) explains how each surface is built.

![The KDOS desktop: the panel along the bottom, desktop icons on the wallpaper, and a terminal](../../screenshots/desktop.png)

## What is on the screen

| Piece | Program |
|---|---|
| Window frames, the compositor's menus, the phosphor shader | [`kdos-comp`](../04-programs/kdos-comp.md), the compositor |
| The panel along the bottom | `kdos-shell` |
| Icons on the wallpaper | `kdos-desk` |
| Everything that opens from the panel | Other names of the same `kdos-shell` binary |

There is one panel per screen, on the bottom edge, two rows tall. The second row carries the
clock's date, the meters strip, and — when window buttons show their names — each window's own
title under its application name.

The panel's shape is set in `~/.config/kdos/comp.conf`. These keys are read once, when the session
starts, so a change takes effect at your next login:

| Key | Default | What it does |
|---|---|---|
| `panel` | `bottom` | `bottom`, `top`, or `off` |
| `panel_cells` | `2` | Height in cells. With `1` there is no second row, and a window button shows a name and no icon |
| `panel_font` | `Terminus:pixelsize=20` | The bar's font, which sets its height. Use a size Terminus has: 12, 14, 16, 18, 20, 22, 24, 28 or 32. Empty means use `chrome_font` |
| `panel_margin` | `0` | Pixels between the bar and the screen edge. Non-zero floats the bar off three sides |
| `panel_opacity` | `80` | Background opacity in percent, floored at 20. Text and icons stay opaque |
| `panel_autohide` | `no` | `yes` shrinks the bar to a one-row strip until the pointer reaches it |
| `desktop_icons` | `yes` | `no` turns off `kdos-desk` and leaves the wallpaper bare |
| `icons` | `yes` | `no` draws a character glyph wherever a picture would go |
| `slit` | `no` | `yes` starts `kdos-slit`, a column of small gadgets configured in `~/.config/kdos/slit.conf` |

Settings' Panel and Desktop pages write the same keys.

## The panel

These are the pieces that are always on the panel, from left to right. The occasional ones — the
recording lamps, the now-playing controls, the clipboard depth, the CPU readout, the update and
restart marks, the stutter chip — are widgets you can add, remove and reorder; see
[the notification area](#the-notification-area).

| Element | Left click | Middle click | Right click |
|---|---|---|---|
| Start button | Opens the Start menu | Run a command | The System menu |
| Window list | Pinned and not running: launches it. Running: shows or hides the window, or lists the windows of a group | Launches another instance | The window menu, or Unpin on a pinned button with nothing running |
| Meters strip | `btop` in a terminal | The stutter report | The energy report |
| Workspace squares | Switch workspace | — | — |
| Tray items | Activate the item | Secondary activate | The item's context menu |
| Overflow chevron | The status popup, listing what is behind it | — | — |
| Network | The network manager | — | The network manager |
| Volume | A slider you can drag; the wheel over it changes the volume | Mute or unmute | Audio devices and volumes |
| Battery | The calendar | — | Settings' Session page |
| Notification badge | The notification centre | Do not disturb, on and off | — |
| Clock | The calendar. The clock and the battery share this popup, so either one closes what the other opened | — | — |
| Show desktop (the last column) | Minimises every window, or brings them all back when nothing is on screen | — | — |

The widgets that come and go answer clicks like this:

| Widget | Left click | Middle click | Right click |
|---|---|---|---|
| Microphone lamp | Mute or unmute the microphone | Mute or unmute the microphone | Audio devices and volumes |
| Camera lamp | Devices (`kdos-devices`) | — | Devices |
| Removable media | Devices | — | Devices |
| Now playing | Play or pause | Previous track | Next track |
| CPU readout | The resource monitor, [`kdos-res`](../04-programs/kdos-res.md) | — | — |
| Clipboard depth | Clipboard history | — | — |
| Stutter chip | The stutter report | — | — |
| Restart mark | Which running programs an upgrade has replaced | — | — |
| Update mark | The update surface, `kdos-update` | — | — |

Pinned launchers and running windows share one row. A pinned application that is running uses its
pinned slot instead of appearing twice, and the underline under a button is what tells you it is
running. Drag a pinned icon along the row to reorder it; the order is saved to
`~/.config/kdos/favorites`. Dropping an icon off the row does nothing.

By default a window button is a picture with no label, which makes the panel a dock. The
`task_labels` key in `~/.config/kdos/panel.conf` changes that: `no` (the default) shows pictures
only, `auto` lets the bar choose — full labels, then shortened ones, then pictures — and `yes`
always keeps the label. `start_label = no` removes the word from the Start button and leaves only
its mark.

As the bar fills up it gives things up in four passes, in this order:

| Pass | What is shown |
|---|---|
| 0 | Everything |
| 1 | The meters are dropped |
| 2 | The Start button also shrinks to its mark |
| 3 | Pinned applications that are not running are also dropped |

No pass ever drops a window button. Windows that do not fit go behind a `+N` cell, and the mouse
wheel over the row steps through them. With `task_labels = auto` the row gives up its label text
before any of those passes, because a picture that identifies the window is worth more than a word
beside it.

### The meters strip

The meters strip is sixteen cells of live graphs on the panel's second row. The default set is
CPU, memory and network. The network meter is a mirrored pair: data received is drawn above the
midline in the accent colour, data sent below it in the secondary colour, on one shared scale, so
you can see each direction on its own.

Choose the meters with `meters =` in `~/.config/kdos/panel.conf`. Six are available:

| Name | Shows | Cells |
|---|---|---|
| `cpu` | Processor load, as a percentage | 5 |
| `ram` | Memory in use, as a percentage | 5 |
| `disk` | How full `/` is, as a percentage | 5 |
| `temp` | The hottest sensor, on a fixed 0–100 °C scale | 5 |
| `net` | Network received and sent, mirrored | 6 |
| `diskio` | Disk read and written, mirrored | 6 |

Any three meters that include one mirrored pair add up to exactly sixteen cells, and sixteen is the
limit: a wider strip cannot be drawn. List the names in order of importance, because a narrow bar
drops meters from the right. `meters =` with nothing after it turns the strip off.

For example, to watch disk activity instead of memory:

```ini
meters = cpu diskio net
```

That is 5 + 6 + 6 = 17 cells, so the last meter, `net`, is dropped. `meters = cpu diskio` fits.

### The notification area

The widgets on the right of the panel are a list you control. `right =` in `panel.conf` names them
in order, and a name the panel does not know is reported in the session log rather than silently
ignored. A widget you leave out of the list does not exist on your bar.

| Name | Widget |
|---|---|
| `pager` | The workspace squares |
| `tray` | Status icons from running applications |
| `more` | The overflow chevron |
| `media` | Removable media |
| `privacy` | The microphone, camera and screen-recording lamps |
| `mpris` | Now-playing controls for a media player |
| `clipboard` | Clipboard history depth |
| `cpu` | The CPU readout |
| `stutter` | The stutter chip |
| `update` | The update mark |
| `restart` | The restart mark |
| `net` | Network |
| `volume` | Volume |
| `battery` | Battery |
| `notify` | The notification badge |
| `clock` | The clock |

The default order is the order of that table:

```ini
right = pager tray more media privacy mpris clipboard cpu stutter update restart net volume battery notify clock
```

Some widgets only appear when they have something to say — by default the stutter chip, the
restart mark and the clipboard depth. They live behind the overflow chevron, which is listed by
`overflow =` (default `overflow = stutter restart clipboard`). The chevron takes two columns
whenever anything is listed for it, whether or not those widgets have something to show right now,
so the panel does not change width and the meters beside it do not slide sideways. `overflow =`
with nothing after it puts those widgets back on the bar. With `overflow =` and `tray_hide =` both
empty there is no chevron.

`tray_hide =` lists status-icon ids that are kept off the bar and shown in the chevron's popup
instead. The default is `tray_hide = fcitx fcitx5 org.fcitx.fcitx5`, so the input-method icon is
in the chevron's popup; `tray_hide =` with nothing after it puts it back on the bar.

Settings' Panel page writes these keys and tells the running panel at once. After editing
`panel.conf` by hand, tell the panel yourself:

```sh
pkill -x -HUP kdos-shell
```

### Tooltips

Hover over anything on the panel for about 0.7 seconds and a tooltip says what it is and what its
three mouse buttons do. A window button's tooltip includes a small live picture of the window,
which is how you tell three terminals apart.

## The Start menu

`Super` is the key with the Windows or logo mark on it, between `Ctrl` and `Alt`. Press
`Super+A` or `Super+F10`, or click the Start button.

**The left column** is what you use: pinned applications above the rule, and your most frequently
launched ones below it. **All Programs** opens the category list in place rather than as a cascade
of submenus.

**The right column** has three groups:

- **Places** — your home folder and the folders you have added, plus a **Files** row that opens
  the file browser.
- **Recent** — files you have opened recently. It only appears once there are some.
- **System** — Settings, Network, Bluetooth, Sound, Recorder, Notifications, Devices, Boxes,
  Displays, Terminal, Help and About.

**The footer** holds the five power actions: Lock Screen, Suspend, Restart, Log Off and Shut Down.
Restart, Log Off and Shut Down ask before they act. Suspend does not ask, because waking the
machine undoes it.

**Start typing to search.** There is no field to click; the first letter you type turns the left
column into results, `Esc` clears the search, and a second `Esc` closes the menu. The search covers
applications, every row in the right column, the power actions and every named route (a stable
name for a place in the system, defined in `/etc/kdos/menu.conf`). Each fixed row also answers to
synonyms: typing `wifi` finds Network. A search with results also offers **Clear search** as its
first row.

An application that runs in a box is marked `[box]`, because its first launch has to start a
container and takes noticeably longer. The mark at the right edge of a row pins that application
to the panel.

**Two-letter codes.** A line in `~/.config/kdos/favorites` may carry a code, for example
`mc code=FM`. The code is drawn on the pinned row, and typing both letters in the Start menu opens
that application at once, with no arrows and no `Enter`. The shipped favorites are:

| Application | Code |
|---|---|
| `mc`, the file manager | `FM` |
| `btop`, the system monitor | `MO` |
| `lazygit` | `GI` |
| `foot`, the terminal | `TE` |
| `firefox-esr` | `WW` |
| `org.xfce.mousepad`, a text editor | `ED` |
| `gimp` | `IM` |

The last three run in boxes and appear only once they are installed. At most eight pinned
applications are shown: the first eight entries whose application is installed. An entry with
nothing installed behind it is skipped and does not use up a place.

**Terminal programs are applications here.** btop, lazygit, yazi, aerc, calcurse, visidata, nmtui
and the rest of the installed terminal programs each carry a desktop entry, so they appear in the
menu, answer the search, pin to the panel and open with a double-click, exactly like a program with
its own window. The desktop supplies the terminal, which is `foot`. The entry belongs to the
package, so installing a program adds its row and removing the program removes the row.

**Installing applications.** When the medium you booted from carries application packs, a search
also lists matching applications that are not installed yet, under **INSTALL FROM THE MEDIUM**.
Choosing one installs it and opens it in one action. The standard image carries no packs, so this
section only appears on a medium that has them. To build applications from the catalogue instead,
open the application store, `kdos-store`: it is on the palette's setup routes (`Super+Ctrl+H`). See
[Applications](applications.md).

![The Start menu: pinned applications on the left with `[box]` markers, Places and System on the right, and the search field showing the selected row's description](../../screenshots/start-menu.png)

## Windows

Window frames are drawn by the compositor to match everything else: square corners, a two-pixel
accent border and hard-edged block buttons. The title is `Terminus (TTF)` at 24 points, which is 32
pixels at 96 dpi — exactly one cell — so the title bar is one cell tall even though it is not made
of cells.

A frame may carry a small coloured square at the left of its title. That is a *box chip*, showing
which box the window came from. It appears only for a box you have given its own accent colour, so
a fresh install shows none. When the same application runs in two boxes, the box name is added to
its window label to tell them apart.

To move a window, drag its title bar, or hold `Super` and drag anywhere in the window. To resize
one, drag a border, or hold `Super` and right-drag anywhere in the window. Many boxed applications
draw their own decorations and have no title bar to grab, which is why the `Super` forms exist.

Windows remember where they were. When an application closes, its position, size, workspace and
shaded state are saved, and the next time it opens it goes back there. A window that places itself,
a window placed by a rule, and a window that opens maximised, tiled or fullscreen are left alone.
Set `window_memory = no` in `comp.conf` to turn this off.

## Keyboard shortcuts

These tables are the complete set KDOS ships. They come from `~/.config/kdos-comp/rc.xml`, where
`Super` is written `W`, `Shift` `S`, `Ctrl` `C` and `Alt` `A`. `Alt+Tab`, `Alt+Shift+Tab` and
`Alt+F4` are not in that file: they are compositor defaults, which the file's `<default />` line
pulls in.

`Super+F1` shows a shortcut card generated from your own `rc.xml`, so it stays accurate when you
change the file.

### Applications and shell surfaces

| Shortcut | Opens |
|---|---|
| `Super+Return` | A terminal (`foot`) |
| `Super+grave` | The scratchpad terminal |
| `Super+A`, `Super+F10` | The Start menu |
| `Super+D`, `Super+F7` | The launcher — a full-screen search over applications |
| `Super+Space` | The command palette — one search over everything the desktop can reach |
| `Alt+F2` | Run a command |
| `Super+E` | Files (`mc` in a terminal) |
| `Super+I` | Settings |
| `Super+/` | The documentation browser |
| `Super+F1` | The keyboard shortcut card, generated from your own `rc.xml` |
| `Super+C` | The calendar |
| `Super+F2` | The Team Monitor — every window with its process and box, for force-quitting one |
| `Super+F3` | Audio devices and volumes |
| `Super+F4` | Network |
| `Super+F5` | Bluetooth |
| `Super+F6` | Devices and removable media |
| `Super+Shift+F` | Find: files by name or contents, applications, recent files |
| `Super+Shift+N` | The notification centre |
| `Super+Shift+Space` | Hide the panel, and bring it back |
| `Ctrl+Shift+Escape`, `Super+Ctrl+T` | The resource monitor |

The scratchpad is a single terminal started under the application id `kdos-scratchpad`. The first
press opens it. Each press after that switches whether it follows you between workspaces, and
raises and focuses it. When you switch following off, it goes back to the workspace it was opened
on.

### Terminal programs, one key each

| Shortcut | Program |
|---|---|
| `Super+E` | `mc` — the file manager |
| `Super+Shift+E` | `aerc` — mail |
| `Super+Shift+B` | `lynx` — the web |
| `Super+Shift+U` | `rmpc` — music |
| `Super+Shift+C` | `ikhal` — the calendar |
| `Super+Shift+G` | `iamb` — chat |
| `Super+Shift+W` | `micro` — the editor |

Each of these is *run-or-raise*: if the program already has a window, the key focuses and raises
it; if not, the key starts `foot -e `*program*. A key whose program is not installed opens nothing,
and that row is left off the shortcut card.

### Desk accessories

| Shortcut | Opens |
|---|---|
| `Super+Ctrl+Q` | Calculator |
| `Super+Ctrl+N` | Scratch pad |
| `Super+Ctrl+B` | Contacts |
| `Super+Ctrl+E` | Character map |
| `Super+Ctrl+V` | Clipboard history |
| `Super+Ctrl+P` | Energy — what is costing the battery |
| `Super+Ctrl+C` | The palette, opened on its capture routes: screenshots, recording, QR decoding |
| `Super+Ctrl+H` | The palette, opened on its setup routes: printers, users, the clock, the application store |
| `Super+Ctrl+Shift+Space` | The accent and font picker |
| `Super+Ctrl+R` | Add a reminder |
| `Super+Ctrl+Alt+R` | List the reminders |
| `Super+Ctrl+Shift+R` | Clear the reminders |

Each accessory opens over whatever is on the screen and closes leaving it untouched. A reminder is
saved as a job in `~/.config/kdos/timers.d`, so it survives logging out, and it is delivered once.

### Notifications and session switches

| Shortcut | Does |
|---|---|
| `Super+X` | Dismiss the notification on screen |
| `Super+Shift+X` | Dismiss every notification on screen |
| `Super+Ctrl+X` | Do not disturb, on and off |
| `Super+Alt+X` | Bring back the last dismissed notification |
| `Super+Ctrl+Alt+T` | Show the time as a notification |
| `Super+Ctrl+Alt+B` | Show the battery level as a notification |
| `Super+Ctrl+I` | Stay awake — stop the screen saver, the lock and blanking on idle, and allow them again |
| `Super+Ctrl+Shift+N` | Night light — warm the colours, and cool them again |

The same three switches — stay awake, night light, do not disturb — are available from a prompt as
`kdos toggle`.

### Windows

| Shortcut | Does |
|---|---|
| `Super+Q`, `Alt+F4` | Close |
| `Super+N` | Minimise |
| `Super+M` | Maximise, or restore |
| `Super+F` | Fullscreen, or restore |
| `Super+S` | Shade — roll the window up into its title bar |
| `Super+T` | Always on top |
| `Super+O` | Show on all workspaces |
| `Super+Tab`, `Alt+Tab` | Next window |
| `Super+Shift+Tab`, `Alt+Shift+Tab` | Previous window |
| `Super+←` `→` `↑` `↓` | Snap to that edge; two snaps combine into a quarter |
| `Super+Alt+←` `→` `↑` `↓` | Move to that edge without resizing |
| `Super+Ctrl+←` `→` `↑` `↓` | Grow to that edge |
| `Super+Shift+D` | Show the desktop |
| `Alt+Space` | The window menu — every action for this window, each row showing its own shortcut |

### Workspaces

Four workspaces ship, named `main`, `www`, `hack` and `misc`. The panel draws each as a small
screen with a label under it. The label is the workspace's name only when that name is one or two
characters long, because a shortened name would mislead: two characters of `Workspace 3` are `Wo`.
The shipped names are longer, so the labels read `1 2 3 4`.

| Shortcut | Does |
|---|---|
| `Super+1` … `Super+9` | Go to that workspace |
| `Super+Shift+1` … `Super+Shift+9` | Send the window there, and follow it |
| `Super+,` / `Super+.` | Previous / next workspace |
| `Super+Page Up` / `Super+Page Down` | Previous / next workspace |
| `Super+Shift+,` / `Super+Shift+.` | Send the window to the previous / next workspace, and follow it |

All of the previous/next keys wrap around from the last workspace to the first.

Keys for workspaces 5 to 9 are already bound, so raising `<desktops number="4">` in `rc.xml` makes
them work with no other edit. The panel's strip grows by two cells per workspace to match, and the
mouse wheel over it steps through workspaces. When the bar has no room for the squares they shrink
to a compact `N/M` readout, which shows where you are but is not clickable.

### Session, screen and media

| Shortcut | Does |
|---|---|
| `Super+L` | Lock the screen |
| `Super+Shift+L` | Start the screen saver |
| `Super+Escape` | End the session — it asks first |
| `Print` | Screenshot the whole screen |
| `Shift+Print`, `Super+Shift+P` | Screenshot a region |
| `Alt+Print` | Start the screen recording, and stop it |
| `Super+P`, the display key | Display settings |
| Volume up, down, mute | Change the volume by 5%, with an on-screen gauge |
| Mic mute | Mute the microphone |
| Brightness up, down | Change a built-in screen's brightness by 10%, with an on-screen gauge |
| Play/pause, stop, next, previous | Media controls, through `kdos-mpctl`: sent to mpd or to a media player on the session bus — `mpv`, `cmus`, a boxed player — whichever is playing |
| The power button | Shut the machine down — it asks first |
| The sleep or suspend key | Suspend |

The brightness keys reach a laptop's or all-in-one's own panel. An external monitor is not
controlled by them; set its brightness with `ddcutil setvcp 10 <0-100>`.

`Super+Escape` saves the list of open windows *before* it asks whether to end the session, because
once you answer there is no session left to save it from. The list is only used when
`~/.config/kdos/session-restore` exists: create that file (`touch ~/.config/kdos/session-restore`)
and your boxed applications are started again at the next login, two seconds apart.

### Pointer bindings

| Action | Does |
|---|---|
| Left click a window | Focus and raise it — focus does not follow the mouse |
| `Super`+left-drag | Move the window |
| `Super`+right-drag | Resize the window |
| Drag a border | Resize the window |
| Double-click the title | Shade, and unshade |
| Wheel up / down on the title bar | Shade / unshade |
| Right-press the title bar | The window menu |
| Right-click the wallpaper | The desktop's menu (see [Files](#files)) |

With `desktop_icons = no` in `comp.conf` there is no desktop surface over the wallpaper, and the
compositor answers it directly: any button opens the compositor's root menu, and the wheel steps to
the previous or next workspace.

### Changing the bindings

Edit `~/.config/kdos-comp/rc.xml`. The compositor is a fork of labwc, and labwc's `rc.xml`
documentation applies to it.

Keep `<default />` as the first line inside both `<keyboard>` and `<mouse>`. The compositor loads
its built-in bindings only when your file defines none of that kind, so a file that binds even one
key without `<default />` loses every default — including click-to-focus, dragging by the title
bar and the three window buttons, which makes the mouse appear not to work. Put your own bindings
after that line. When two bindings use the same chord, the later one wins.

For example, to make `Super+B` open Firefox:

```xml
<keyboard>
  <default />
  <!-- ...the shipped bindings... -->
  <keybind key="W-b"><action name="Execute" command="firefox-esr"/></keybind>
</keyboard>
```

The same file controls pointing devices — tap-to-click, natural scrolling, pointer speed — in a
`<libinput>` block that ships commented out with example values. Tap-to-click is already on by
default.

## Notifications

Notifications appear as small pop-ups (*toasts*) in a corner and disappear after a while. Hovering
over one pauses its countdown while you read it, and its border changes colour to show that it has
stopped.

Nothing that disappeared is lost. `Super+Shift+N` opens the notification centre, which keeps the
history — including everything that arrived while the screen was locked or while you were on
another workspace. The panel badge counts what has arrived since you last opened it.

Do not disturb (middle-click the badge, or `Super+Ctrl+X`) stops toasts from appearing without
losing them. Each notification is still recorded and counted, and the application that sent it
cannot tell the difference. A notification marked urgent is shown anyway.

Clicking a toast dismisses it, and so does `Super+X`. `Super+Alt+X` brings the last one back if
you dismissed it too early. It comes back without its buttons, because the original notification
is closed and its actions belonged to the program that sent it.

Each of the three session switches — stay awake, night light and do not disturb — shows a one-line
notice saying whether it is now on or off. Two of them change nothing visible, so without the
notice you could not tell a working key from a broken one.

From a prompt, `kdos notify <text>` raises a notification of your own — for example,
`make && kdos notify done`.

## Files

`kdos-desk` draws the desktop: the contents of `~/Desktop` as a grid of icons, with Home and Trash
pinned last. `~/Desktop` is created if it is missing.

**Right-click the wallpaper** for a menu about the desktop itself:

- Open Terminal Here, Find Here, Add to Places, Git Status Here
- New Folder, New File, Sort Icons, Refresh
- Applications, Change Wallpaper, Display Settings, Settings
- Show Desktop, Screenshot, Lock Screen

**Right-click an icon** for the file verbs. Which ones appear depends on what the icon is:

| Verb | Offered on |
|---|---|
| Open | Files and folders |
| Peek | Files |
| Edit | Files |
| Open Terminal Here | Files and folders |
| Find Here | Folders |
| Add to Places | Folders |
| Share | Files and folders |
| Git Status Here | Files and folders |
| Extract Here | Files |
| Move to Trash | Files and folders, but not the pinned Home and Trash icons |
| Rename | Files and folders, but not Home and Trash |
| Empty Trash | The Trash icon |
| New Folder, New File, Refresh | Every icon |

The file verbs come from one shared table that the file browser (`kdos-pick`) also uses, so the two
always offer the same verbs, and a verb whose program is not installed is offered on neither. The
`F2` menu in `mc` offers the same verbs from `~/.config/mc/menu`.

**Using the keyboard on the desktop.** No icon is selected until you select one. Click the
desktop, then use the arrow keys or `Tab` to pick an icon; `Esc` or a click on bare wallpaper
clears the selection. While the desktop has the keyboard, its bottom row shows which keys do what,
and the desktop gives the keyboard back the moment you click a window. With nothing selected,
`Shift+F10` opens the wallpaper's menu. With an icon selected, `Enter` opens it and `Delete` moves
it to the trash.

**The trash.** Deleting from the desktop moves the file to the standard freedesktop trash, the
same one `kdos trash <file>` uses from a prompt. Opening the Trash icon opens `kdos-trash`, which
lists what was deleted, when and from where; `Enter` puts the selected file back where it was.

**The file browser** is `kdos-pick --browse`. The same program is the Open and Save dialog that
boxed applications get through the file-chooser portal, so Open and Save in Firefox or GIMP are
drawn on this grid rather than by their own toolkit.

### Opening a file

Double-clicking a file opens it with the default handler for its type. `kdos-openwith` lets you
choose a different one.

A few defaults worth knowing:

| You open | It goes to |
|---|---|
| A `.csv` file | `visidata`, which claims `text/csv` and is built for exploring columns |
| A spreadsheet (`.xlsx`) | Offered to `sc-im` through **Open With**. sc-im reads and writes `.xlsx` directly, but claims no file type, so a double-click does not choose it. `visidata` can also open `.xlsx` |
| A `mailto:` link | `aerc` |
| A web page, `http:` or `https:` link | `w3m`, set in `/etc/xdg/mimeapps.list`, so links behave the same on `tty1` as on the desktop |

A browser you install in a box registers itself as a candidate handler; use `kdos-openwith` to make
it the default. A link clicked on the desktop or in a terminal program goes through `xdg-open`,
which here is the same resolver a double-click uses. A link clicked inside a boxed application goes
through the portal instead, which reads the same tables, so both end at the same handler. A handler
that needs a terminal is given one wherever it is launched from.

## The clipboard

Copy and paste work in both directions between the desktop and boxed applications, including the
middle-click primary selection.

`Super+Ctrl+V` opens the clipboard history, which is also reachable from the panel's clipboard
widget. The history is kept by `kdos-clip`, which also keeps the copied text alive after the
program you copied it from has closed. Set `clipboard = no` in `comp.conf` to turn it off; the
selection then disappears with the program that owned it.

## Locking, idle and power

`Super+L` locks the screen. The session, not the lock program, owns the locked state: if the lock
program crashes, the screen stays covered and a new lock program can take its place. Only a correct
password unlocks the screen.

The idle policy is one timer, measured from your last keyboard or pointer activity: first the
screen dims, then it locks, then the outputs switch off. Moving the mouse or pressing a key ends the
dim and turns the screen back on, but never unlocks it. An application that asks the system to stay
awake — a video player, for instance — pauses the whole policy while it does.

Set the timers in `~/.config/kdos/comp.conf`, in seconds, where `0` means never:

| Key | Default |
|---|---|
| `idle_dim` | `300` |
| `idle_lock` | `600` |
| `idle_off` | `900` |
| `lid_close` | `suspend` — or `lock`, or `off` |

In a virtual machine all three idle timers default to `0` and `lid_close` defaults to `off`,
because a blank screen seen over a remote display looks exactly like a crashed session. Setting any
of these keys in `comp.conf` overrides that, including setting one to `0`. See
[Configuration](../06-reference/configuration.md).

Lock, suspend, restart, log off and shut down are in the Start menu's footer, and the same five are
on the System menu (right-click the Start button).

## Displays

`Super+P`, or the display key on a laptop, opens `kdos-display`: your screens, their modes, their
scale and which are switched on.

Nothing is applied until you press `Enter`. Every key before that edits a plan:

| Key | Does |
|---|---|
| `m` | Choose from the modes the selected monitor reports |
| `Space` | Switch the selected screen on or off |
| `s` | Cycle the scale |
| `t` | Cycle the rotation |
| `[`, `]` | Move the selected screen left or right in the order |
| `Enter` | Apply the plan |
| `Esc` | Leave without applying |

A mode the monitor cannot actually show may still be accepted and leave you with a screen you
cannot read, so `Enter` starts a fifteen-second countdown. Press `K` to keep the result, which
saves it to `~/.config/kdos/displays.conf`. Press `R`, or let the countdown run out, and the
settings from before your first edit are put back. `kdos-display --apply` re-applies the saved file
without opening a window; the session runs it at startup and whenever a screen is plugged in.

Screens are placed edge to edge from the left, in list order. Stacking screens vertically,
overlapping them or leaving a gap between them is not supported.

Each screen gets its own panel and its own desktop icons, and each panel lists the windows on its
own screen. Notifications are not per-screen, so a toast appears wherever the compositor places it.

## Removable media and devices

`Super+F6` opens `kdos-devices`: USB sticks and other removable disks, cameras and microphones. You
pick a disk from the list, and a root service (`kdos-mountd`) mounts it, choosing the device, the
mount point and the options itself, so you do not need root.

Removable disks are always mounted `nosuid,nodev`, and `noexec` by default. To allow programs on
them to run, create `/etc/kdos/mountd.conf` (it does not exist by default) with the line
`exec = yes`. The mount point is `/media/<user>/<label>`, or the device name when the filesystem has
no label.

The service will not offer:

- an internal disk,
- a filesystem this kernel cannot mount,
- anything already mounted,
- anything listed in `/etc/fstab`,
- the medium this system booted from.

See [the daemons](../04-programs/daemons.md) for the full rules.

![kdos-devices: removable media, cameras and microphones on one surface](../../screenshots/devices.png)

## Who is using the camera and microphone

While something is recording, the panel shows a lamp naming the application — its own name, not
`pipewire`. There are three lamps: the microphone, the camera, and the screen. The screen has its
own lamp because somebody watching your screen is a different event from somebody watching your
face. Click the microphone lamp, or press the mic-mute key, to mute or unmute the microphone.

The microphone lamp lights only while a recording stream is actually *running*, not merely open,
so it does not light for an application that has a microphone open and idle. The camera lamp
lights when any process has a `/dev/video*` device open, because most applications use the camera
directly rather than through the portal.

The tooltip names the box the application is in, which tells you *which* Firefox it is when you
have more than one.

## When the desktop misbehaves

**Stutter.** A stutter chip appears when the compositor has dropped at least three frames in the
last ten seconds. By default it sits behind the overflow chevron. Click it to see which frames
were late, by how much, whether the compositor itself was slow, and what was busy at the time. The
chip disappears when the desktop stops dropping frames. `kdos stutter` gives the same report at a
prompt.

**Something is using too much.** The meters strip is the quickest way in: left click opens `btop`,
middle click the stutter report, right click each application's share of energy use. The panel's
CPU readout, `Ctrl+Shift+Escape` and `Super+Ctrl+T` open [`kdos-res`](../04-programs/kdos-res.md),
which can name a boxed application rather than only its processes.

**A fullscreen application has frozen and covers the screen.** Press `Super+F2` for the Team
Monitor. Select the window; `Enter` asks it to close normally, and `k` sends the process behind it
`SIGTERM`.

**The panel has gone.** `Super+Shift+Space` brings back a panel you hid. Check `panel` in
`comp.conf` if it is still missing.

## See also

- [kdos-shell](../04-programs/kdos-shell.md) — every surface on this page, in detail
- [kdos-comp](../04-programs/kdos-comp.md) — frames, the phosphor pass, and the configuration keys
- [Applications](applications.md) — installing and launching boxed software
- [Theming](theming.md) — accents, the shader, the wallpaper and fonts
- [Accessibility](accessibility.md) — the magnifier and what does not exist
- [Configuration](../06-reference/configuration.md) — every key on this page, with defaults
- [The design language](../03-architecture/design-language.md) — why it looks like this
