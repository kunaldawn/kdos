# The desktop

This page covers working in the KDOS desktop: the panel, the Start menu, windows and workspaces,
the complete keyboard shortcut table, notifications, files, locking, displays and removable media.

Every surface KDOS paints is a grid of character cells — the panel and everything that pops out of
it, the desktop icons, the lock screen, the boot splash and `tty1`, all drawn by `libktui` in one
palette at Terminus 32px. The compositor's own chrome is the exception: titlebars, the root menu
and the window-switcher OSD are drawn with pango at a size matched to the cell, so they line up
with the grid without being on it. An application running in a box draws whatever its toolkit
draws. [The design language](../03-architecture/design-language.md) explains why, and
[kdos-shell](../04-programs/kdos-shell.md) explains how.

![The KDOS desktop: the panel along the bottom, desktop icons on the wallpaper, and a terminal](../../screenshots/desktop.png)

## What is on the screen

| Piece | Program |
|---|---|
| Window frames, the root menu, the phosphor shader | [`kdos-comp`](../04-programs/kdos-comp.md), the compositor |
| The panel along the bottom | `kdos-shell` |
| Icons on the wallpaper | `kdos-desk` |
| Everything that pops up from the panel | Other names of the same `kdos-shell` binary |

There is one panel per screen, on the bottom edge, two rows tall. The second row is not padding:
it carries the clock's date, the live meters strip, and — on a bar whose window buttons carry
their names — each window's own title under its application name.

## The panel

Left to right, and these are the pieces that are always there. The occasional ones — the media
keys, the clipboard depth, the CPU readout, the recording lamps, the update and restart marks, the
stutter chip — are widgets in the same list, and [the notification area](#the-notification-area)
below is how the list is set.

| Element | Left click | Middle click | Right click |
|---|---|---|---|
| Start button | Opens the Start menu | Run a command | The System menu |
| Window list | Pinned and not running: launches it. Running: toggles the window, or opens the member list for a group | Launches another instance | The window menu, or Unpin on a pinned button with nothing running |
| Meters strip | `btop` in a terminal | The stutter report | The energy report |
| Workspace squares | Switch workspace | — | — |
| Tray items | Activate the item | Secondary activate | The item's context menu |
| Overflow chevron | The status popup | — | — |
| Network | The network manager | — | The network manager |
| Volume | A slider you can drag; the wheel over it changes the volume | Mute | Audio devices and volumes |
| Battery | The calendar | — | Settings' Session page |
| Notification badge | The notification centre | Do not disturb, on and off | — |
| Clock | The calendar — one popup with the battery, so either cell shuts what the other opened | — | — |
| Show desktop (the last column) | Minimises every window, or brings every minimised one back when nothing is on screen | — | — |

The pinned launchers and the window list are one row: a pinned application that is running
occupies its pinned slot rather than appearing twice, and the underline under a button is what
says it is running at all. Drag a pinned icon along the row to reorder it; the order is written to
`~/.config/kdos/favorites`, and dropping an icon off the row does nothing.

By default a window button is a picture with no label — `task_labels = no` in
`~/.config/kdos/panel.conf`, which is what makes this a dock. `auto` lets the bar decide, and
`yes` keeps the label whatever the room.

The panel degrades in four passes as the bar fills up, and the order is the priority: pass 0 is
the whole bar, pass 1 drops the meters, pass 2 also collapses the Start button to its mark, and
pass 3 also drops the pinned applications that are not running. No pass ever drops a window
button; windows too many for the row go behind a `+N` cell, which the wheel over the row also
steps. On `task_labels = auto` the row gives up its *text* before any of that, because a picture
that identifies the window is worth more than a word beside it.

### The meters strip

The strip is sixteen cells of live graphs on the panel's second row. The shipped set is CPU,
memory and the network — the network drawn as a mirrored pair, received above the midline in the
accent colour and sent below in the secondary, on one shared scale. Summing them would hide the
only thing anybody watches a network meter for.

Choose which meters appear with `meters =` in `~/.config/kdos/panel.conf`. Six are available:

| Name | Shows | Cells |
|---|---|---|
| `cpu` | Processor load, as a percentage band | 5 |
| `ram` | Memory in use, as a percentage band | 5 |
| `disk` | Fullness of `/`, as a percentage band | 5 |
| `temp` | The hottest sensor, on a fixed 0–100 °C band | 5 |
| `net` | Received and sent, mirrored | 6 |
| `diskio` | Read and written, mirrored | 6 |

The widths total sixteen cells for any three-meter set that includes one mirrored pair, and sixteen
is the hard ceiling: a sprite cell carries its sub-cell coordinate in four bits each way, so a tile
can never be wider. Order the names by importance, because a narrow bar drops them from the right.
`meters =` with nothing after it turns the strip off.

### The notification area

The widgets on the right are a list rather than a fixed layout. `right =` in `panel.conf` names
them in order, and an unknown name is reported rather than ignored.

Widgets that only sometimes have something to say — the stutter chip, the restart mark, the
clipboard depth — live behind one chevron of fixed width, listed by `overflow =`. That keeps the
panel from changing width when one of them appears, which would slide every chart sideways.

### Tooltips

Hover anything on the panel for about three quarters of a second and a tooltip says what it is and
what its three buttons do. A window button's tooltip carries a small live picture of the window,
which is the only way to tell three terminals apart by looking.

## The Start menu

Press `Super+A`, or click the Start button.

The left column is what you use: pinned applications above the rule, most-frequently-launched
below it. The right column carries Places, a Recent group where anything has been opened, and the
System group under it. The five power actions — Lock Screen, Suspend, Restart, Log Off, Shut Down
— are the footer, and the search reaches them as well as the columns. **All Programs** opens the
category list in place rather than cascading.

Start typing to search, and the search covers more than applications. Every fixed row carries
synonyms, so typing `wifi` finds the network manager. Applications that are in the catalogue but
not installed appear under **INSTALL FROM THE MEDIUM**, and choosing one installs the application
and opens it in the same action.

An application that runs in a container is marked `[box]`, because its first launch costs a
container start and you are entitled to know before you click. The mark at the right edge of a row
pins that application to the panel's launcher row.

Terminal programs are applications here. btop, lazygit, yazi, aerc, calcurse, visidata, nmtui and
the rest of the installed catalogue each carry a desktop entry, so they appear as rows in the menu,
answer to the search, pin to the panel and open with a double-click, exactly like
anything with a window of its own. Each entry names the program rather than an emulator, and the
desktop supplies the terminal, which is `foot`. The entry belongs to the package, so installing a
program adds its row and removing it takes the row away.

![The Start menu: pinned applications on the left with `[box]` markers, Places and System on the right, and the search field showing the selected row's description](../../screenshots/start-menu.png)

## Windows

Window frames are drawn by the compositor and belong to the same visual set as everything else:
square corners, a two-pixel accent border, and hard-edged block buttons. The title is pango text
in `Terminus (TTF)` at 24 points, which at 96dpi is 32 pixels — one cell of the grid everything
else is drawn on, so the bar is a cell tall even though it is not made of cells.

A frame may carry a small coloured square at the left of the title. That is a box chip, showing
which container the window came from. It appears only for a box that has been given its own accent
colour, so a default install shows none. Where the same application runs in two boxes, the box name
is appended to its taskbar label to tell them apart; where it does not need disambiguating, nothing
is appended.

To move a window, drag its title bar, or hold `Super` and drag anywhere in the window. To resize
one, drag a border, or hold `Super` and right-drag anywhere in the window. Most alien applications
draw their own decorations and have no title bar to aim at, which is why the `Super` forms exist.

## Keyboard shortcuts

The tables below are the complete shipped set, from `~/.config/kdos-comp/rc.xml`, where `Super` is
written `W`. `Alt+Tab`, `Alt+Shift+Tab` and `Alt+F4` are not in that file: they are compositor
defaults, which its `<default />` line is what pulls in. `Super+F1` opens the same card generated
from your own file.

### Applications and shell surfaces

| Shortcut | Opens |
|---|---|
| `Super+Return` | A terminal (`foot`) |
| `Super+grave` | The scratchpad terminal |
| `Super+A`, `Super+F10` | The Start menu |
| `Super+D`, `Super+F7` | The launcher — full-screen search over applications |
| `Super+Space` | The command palette — one search over everything the desktop can reach |
| `Alt+F2` | Run a command |
| `Super+E` | Files (`mc` in a terminal) |
| `Super+I` | Settings |
| `Super+/` | The documentation browser |
| `Super+F1` | The keyboard shortcut card, generated from your own `rc.xml` |
| `Super+C` | The calendar popup |
| `Super+F2` | The Team Monitor — every window with its process and box, for force-quitting one |
| `Super+F3` | Audio devices and volumes |
| `Super+F4` | Network |
| `Super+F5` | Bluetooth |
| `Super+F6` | Devices and removable media |
| `Super+Shift+F` | Find: files by name or contents, applications, recents |
| `Super+Shift+N` | The notification centre |
| `Super+Shift+Space` | Put the panel away, and bring it back |
| `Ctrl+Shift+Escape`, `Super+Ctrl+T` | The resource monitor |

The scratchpad is one terminal started under the `kdos-scratchpad` application id. The first press
opens it; every press after that toggles whether it follows you between workspaces, raising and
focusing it. Taking the flag off returns it to the workspace it was opened on.

### Terminal programs, on one key each

| Shortcut | Program |
|---|---|
| `Super+E` | `mc` — the file manager |
| `Super+Shift+E` | `aerc` — mail |
| `Super+Shift+B` | `lynx` — the web |
| `Super+Shift+U` | `rmpc` — music |
| `Super+Shift+C` | `ikhal` — the calendar |
| `Super+Shift+G` | `iamb` — chat |
| `Super+Shift+W` | `micro` — the editor |

Each of these is run-or-raise: the key focuses and raises the window if one is already open, and
starts `foot -e `*program* only when there is none. A key whose program is not installed opens
nothing and is dropped from the shortcut card, which reads the program name out of the binding.

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
| `Super+Ctrl+H` | The palette, opened on its setup routes: printers, users, the clock |
| `Super+Ctrl+Shift+Space` | The accent and font picker |
| `Super+Ctrl+R` | Add a reminder |
| `Super+Ctrl+Alt+R` | List the reminders |
| `Super+Ctrl+Shift+R` | Clear the reminders |

Each accessory is summoned over whatever is on the screen and dismissed leaving it untouched.

### Notifications and session switches

| Shortcut | Does |
|---|---|
| `Super+X` | Dismiss the toast on screen |
| `Super+Shift+X` | Dismiss every toast |
| `Super+Ctrl+X` | Do not disturb, on and off |
| `Super+Alt+X` | Bring the last dismissed notification back |
| `Super+Ctrl+Alt+T` | Announce the time as a notification |
| `Super+Ctrl+Alt+B` | Announce the battery as a notification |
| `Super+Ctrl+I` | Stay awake — stop the screen locking or blanking on idle, and let it again |
| `Super+Ctrl+Shift+N` | Night light — warm the palette, and cool it again |

### Windows

| Shortcut | Does |
|---|---|
| `Super+Q`, `Alt+F4` | Close |
| `Super+N` | Minimise |
| `Super+M` | Maximise, or restore |
| `Super+F` | Fullscreen, or restore |
| `Super+S` | Shade — roll the window into its title bar |
| `Super+T` | Always on top |
| `Super+O` | Show on all workspaces |
| `Super+Tab`, `Alt+Tab` | Next window |
| `Super+Shift+Tab`, `Alt+Shift+Tab` | Previous window |
| `Super+←` `→` `↑` `↓` | Snap to that edge, combining into quarters |
| `Super+Alt+←` `→` `↑` `↓` | Move to that edge without resizing |
| `Super+Ctrl+←` `→` `↑` `↓` | Grow to that edge |
| `Super+Shift+D` | Show the desktop |
| `Alt+Space` | The window menu — every verb for this window, each row printing its own chord |

### Workspaces

Four workspaces ship, named `main`, `www`, `hack` and `misc`. The panel draws each as a small
screen with a label under it, and the label is the workspace's own name only where that name is
one or two characters wide — never a truncation, because two characters of `Workspace 3` is `Wo`.
The four shipped names are longer than that, so the strip reads `1 2 3 4`.

| Shortcut | Does |
|---|---|
| `Super+1` … `Super+9` | Go to that workspace |
| `Super+Shift+1` … `Super+Shift+9` | Send the window there |
| `Super+,` / `Super+.` | Previous / next workspace, wrapping at the ends |
| `Super+Page Up` / `Super+Page Down` | Previous / next workspace, without wrapping |
| `Super+Shift+,` / `Super+Shift+.` | Send the window to the previous / next workspace |

Keys for workspaces 5 to 9 are bound already, so raising `<desktops number>` in `rc.xml` makes them
work with no further edit. The strip grows two cells per workspace to match, and the wheel over it
steps workspaces, wrapping at the ends. Where the bar cannot afford the squares they collapse to a
compact `N/M` readout, which is a readout and not a control: only what was drawn is clickable.

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
| Volume up, down, mute | Change the volume, with an on-screen gauge |
| Mic mute | Mute the microphone |
| Brightness up, down | Change the panel brightness, with an on-screen gauge |
| Play/pause, stop, next, previous | Media transport, over MPRIS |

`Super+Escape` writes the list of open windows *before* it asks the question, because after the
answer there is no session left to ask. That list is read back only when
`~/.config/kdos/session-restore` exists.

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
| Right-press the wallpaper | The root menu |
| Wheel on the wallpaper | Previous / next workspace |

### Changing the bindings

Edit `~/.config/kdos-comp/rc.xml` and keep `<default />` as the first child of both `<keyboard>`
and `<mouse>`. The compositor loads its built-in bindings only when your file defines none of that
kind, so a file that binds one key throws every default away — including click-to-focus, the title
bar drag and the three window buttons. Put your own bindings after that line; a later binding
replaces an earlier one with the same chord.

## Notifications

Toasts appear in a corner and expire. Hovering one holds its countdown while you read it, and its
border changes colour so you can see that it has stopped.

Nothing that expired is lost. `Super+Shift+N` opens the notification centre, which holds the
history — including everything that arrived while the screen was locked or while you were looking
at another workspace. The panel badge counts what has arrived since you last opened it.

Middle-click the badge to toggle do not disturb, which silences toasts without losing them. The
notification is still recorded and still counted, and the sending application cannot tell the
difference. A notification marked urgent is shown anyway.

Clicking a toast dismisses it, and so does `Super+X` — the cheapest chord on the keyboard, because
a toast interrupts. `Super+Alt+X` brings the last one back for the press that came a moment too
early. It comes back without its buttons: the notification it came from is closed, and its actions
belong to the program that sent it.

Each of the three session switches — stay awake, night light, do not disturb — raises a one-line
notice saying where it now stands. Two of them change nothing you can see, and a keystroke answered
by nothing is one you cannot tell from a broken key.

## Files

`kdos-desk` draws the desktop. Right-click the wallpaper for the verbs that mean *here* — Open
Terminal Here, Find Here, Add to Places, Git Status Here — then New Folder, New File, Sort Icons,
Refresh, Applications, Change Wallpaper, Display Settings, Settings, and the three that act on the
whole desktop: Show Desktop, Screenshot and Lock Screen. Right-click an icon instead and you get
the file verbs: Open, Peek, Edit, Open Terminal Here, Share, Git Status Here, Extract Here and
Move to Trash, with Find Here and Add to Places added when the icon is a folder. Rename is there
too, and Empty Trash on the Trash icon. `~/Desktop` is created if it is missing.

That file half is `libkxdg`'s table, which `kdos-pick` reads too, so a verb arrives on both at
once and a verb whose program is not installed is offered on neither. `mc`'s `F2` menu carries the
same verbs from a shipped file, `~/.config/mc/menu`, which a missing program does not hide a row
from.

No icon is selected until you select one. Arrow keys, `Tab` or a click pick one out; `Esc`, or a
click on bare wallpaper, puts it back down. The bottom row of the desktop says what the keys do
while the desktop holds the keyboard — it takes the keyboard when you click it and gives it back
the moment you click a window — so the row is not a strip of text lying along the wallpaper for the
rest of the session. With nothing selected it offers the arrows and `Shift+F10`, which opens the
wallpaper's own menu. With an icon selected, `Enter` opens it and `Delete` moves it to the trash.

`Delete` on a desktop icon moves the file to the freedesktop trash, which is the same
implementation `kdos trash` uses from a prompt. Opening the Trash icon opens `kdos-trash`: what was
deleted, when, and where it came from, with `Enter` putting a row back where it was. A trash you
cannot get anything out of is a slower delete.

`kdos-pick --browse` is the file browser. The same program is the file dialog that containerised
applications get through the portal, so Open and Save in Firefox or GIMP are drawn on this grid
rather than by their own toolkit.

### Opening a file

Double-clicking a file opens it with the handler for its type. `kdos-openwith` chooses a different
one.

A `.csv` goes to `visidata`, which claims `text/csv` in its own desktop entry and is built for
exploring columns. A spreadsheet is `sc-im`, which reads and writes `.xlsx` in C with nothing in
between, so a file somebody sent you can be handed back as the file they sent — but its entry
claims no MIME type, so it is what **Open With** offers rather than what a double-click picks.
`visidata` can open an `.xlsx` too if you ask it to, which is what `openpyxl` is on the image for.

A link opens the same way a file does. `mailto:` reaches `aerc`; `text/html`, `http` and `https`
reach `w3m`, which is a default in `/etc/xdg/mimeapps.list` and not a fallback — a text browser
answers the same way on `tty1` as it does under a compositor. A browser installed as a container
publishes itself as a candidate, and `kdos-openwith` is how you make it the default. A link
clicked on the desktop or in a terminal program goes through `xdg-open`, which here *is* the same
resolver a double-click uses. A link clicked inside a containerised application goes to the portal
instead, which resolves it itself — but out of the same tables, so both end at the same handler. A
handler that wants a terminal is given one wherever it is launched from.

## The clipboard

Copy and paste work in both directions, including the middle-click primary selection.
`Super+Ctrl+V` opens the clipboard history, which is also reachable from the panel's overflow
popup.

## Locking, idle and power

`Super+L` locks the screen. The lock screen asks the session to hold every output, and the session
— not the lock client — owns the locked state. If the lock program crashes, the screen stays
covered and a new lock client can replace it. The unlock is a message the lock client sends, never
something inferred from it exiting.

The idle policy is one timer, measured from your last activity rather than from the previous stage:
dim, then lock, then outputs off. Activity ends the dim and powers the screen back on. It never
unlocks. An application holding an idle inhibitor stops the policy entirely.

The timers default to zero in a virtual machine, because a blanked screen over a remote display is
indistinguishable from a crashed session. Set any `idle_*` key in `~/.config/kdos/comp.conf` to
turn them on anyway; the shipped values, commented out, are 300, 600 and 900 seconds. See
[Configuration](../06-reference/configuration.md).

Lock, suspend, restart, log off and shut down are the Start menu's footer, and the same five sit
on the System menu. Restart, log off and shut down each ask first; suspend does not, because it is
the one power action that undoes itself.

## Displays

`Super+P` opens `kdos-display`: the outputs, their modes, scale, and which is enabled.

Nothing is applied until you press `Enter`. Every key before it edits a plan — `m` opens the list
of modes the selected monitor published, `Space` turns an output on and off, `s` cycles the scale,
`t` the rotation, and `[` and `]` move a screen in the order. `Esc` leaves without applying.

An apply that the compositor accepts is still not kept: a mode a monitor cannot show comes back as
a success and a screen you cannot read, so `Enter` opens a fifteen-second countdown. `K` keeps the
result and writes it to `~/.config/kdos/displays.conf`; `R` or the timeout puts back the snapshot
taken before the first edit. `kdos-display --apply` replays that file headless, which is what a
session start and a hotplug run.

Screens are laid out edge to edge from x=0 in list order. A vertical arrangement, an overlap or a
deliberate gap cannot be expressed. That is a deliberate narrowing, since what people usually want
is an order.

Each output gets its own panel and its own desktop icons, and each panel lists the windows on its
own screen. The notification daemon is not per-output — it owns one bus name — so a toast lands
wherever the compositor puts it.

## Removable media and devices

`Super+F6` opens `kdos-devices`: removable media, cameras and the rest. A stick is offered by index
rather than by path, and the mount is performed by a root daemon that decides the device, the
mountpoint and the options itself.

Everything removable is mounted `nosuid,nodev` and, by default, `noexec` — `exec = yes` in
`/etc/kdos/mountd.conf` is how you say you meant it. The mountpoint is `/media/<user>/<label>`, or
the device name where the filesystem has no label.

The daemon refuses to offer five things: an internal disk, a filesystem this kernel cannot mount,
anything already mounted, anything named in `/etc/fstab`, and the medium this system booted from.
See [the daemons](../04-programs/daemons.md).

![kdos-devices: removable media, cameras and microphones on one surface](../../screenshots/devices.png)

## Who is using the camera and microphone

The panel shows a lamp naming the application currently recording — the application's own name, not
`pipewire`. There are three: the microphone, the camera, and the screen, which is its own lamp
because somebody watching your screen is not the same event as somebody watching your face. The
microphone lamp is also a control: click it to mute, or press the mic-mute key.

The microphone counts a PipeWire capture node only while it is *running*, not merely open, because
a lamp that cannot tell those apart is one nobody believes twice. The camera is found by walking
`/proc` for an open descriptor on `/dev/video*`, since almost nothing takes a camera through the
portal.

The tooltip names the container the application is in, which on a machine where every application
is containerised is the half that says *which* Firefox.

## When the desktop misbehaves

A stutter chip appears when the compositor has dropped at least three frames in the last ten
seconds — behind the overflow chevron by default, since it is one of the occasional widgets
`overflow =` lists. Click it for the attribution: which frames were late, by how much, whether the
compositor itself was slow, and who was busy at the time. The chip goes away when the desktop stops
missing frames.

For everything else, the meters strip is the way in — left click opens `btop` in a terminal, middle
the stutter attribution and right the per-application energy share. The panel's own CPU readout
opens [`kdos-res`](../04-programs/kdos-res.md), which is the monitor that can name a boxed
application rather than its processes. When
a fullscreen application has stopped answering and owns the screen, `Super+F2` opens the Team
Monitor: `Enter` asks it to close politely, and `k` sends the process behind it a SIGTERM.

## See also

- [kdos-shell](../04-programs/kdos-shell.md) — every surface on this page, in detail
- [kdos-comp](../04-programs/kdos-comp.md) — frames, the phosphor pass, and the configuration keys
- [Applications](applications.md) — installing and launching containerised software
- [Theming](theming.md) — accents, the shader knobs, wallpaper and fonts
- [Configuration](../06-reference/configuration.md) — every key on this page, with defaults
- [The design language](../03-architecture/design-language.md) — why it looks like this
