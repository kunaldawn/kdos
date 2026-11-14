# The desktop

This chapter is for anyone using the KDOS desktop day to day. The desktop is KDE Plasma 6.8, with
KWin as its compositor and window manager, set up and dressed by KDOS: three *desks* to choose
from, the KDOS Modern look in eight accents, floating windows that snap, KRunner as a command
palette, a key card for every shortcut, and the KDOS pieces Plasma does not have, such as the
privacy lamps, the update tray and box-aware windows. It describes the screen as it comes up, the
panel, the Start menu and KRunner, windows and workspaces, every keyboard shortcut KDOS adds or
moves, notifications, files and the clipboard, locking and power, displays, removable media, and
what to do when something misbehaves.

Plasma's own documentation applies to everything that is not described here; KDOS changes Plasma's
defaults, not how Plasma works. For the colours and the look see [Theming](theming.md); for
installing software see [Applications](applications.md); for the machine as a whole, including
networking, printers and users, see [Administration](administration.md). Why the desktop is shaped
this way is in [Decisions](../01-philosophy/decisions.md), and how KWin and Plasma are configured
and patched in [KWin](../04-programs/kwin.md) and [The Plasma shell](../04-programs/plasma-shell.md).

`Meta` below is the key with the Windows or Command logo.

## What is on the screen

After the login screen and the splash, the desktop shows the wallpaper of the accent in force and,
on the default KDOS desk, one opaque panel along the bottom of each screen. On the first login the
Welcome Center opens over it with the KDOS tour; close it, or walk through it to choose the look,
the desk and groups of programs to install.

| Where | What |
|---|---|
| Bottom-left | **Start**: the KDOS mark and the word Start. It opens Kickoff, the application menu |
| Beside it | The pager: the four desktops, `main`, `www`, `hack` and `misc` |
| Middle | The task manager: pinned applications, and a button for each open window |
| Right of the tasks | The meters: processor, memory and network, drawn by the KDOS Meters face |
| Right | The system tray: volume, network, Bluetooth, battery, notifications, devices, the clipboard, the update tray and, while something is capturing, the privacy lamps |
| Far right | The clock with the date, in the monospace face; then Show Desktop |
| The desktop | The wallpaper. Right-click it for **Enter Edit Mode** and **Desktop and Wallpaper** (`Ctrl+Shift+D`), beside Create New, Icons and Display Configuration; widgets are added from Edit Mode's **Add or Manage Widgets…** |

Every panel, applet and widget is Plasma's and is changed the Plasma way: right-click an empty part
of the panel and choose **Add or Manage Widgets…** or **Show Panel Configuration**. A change made there is yours and
outlives every accent switch and every update; only laying out a desk again replaces it (see
[The desks](#the-desks)).

KWin draws the whole screen with the GPU where there is one. In a virtual machine with no 3D
acceleration it draws through Mesa's software renderer: everything works, slower, and Phosphor mode
cannot be turned on (see [Phosphor mode](theming.md#phosphor-mode)).

## The desks

A *desk* is a whole arrangement of panels, with the window behaviour that goes with it. KDOS ships
three:

| Desk | Panels | Windows |
|---|---|---|
| **KDOS**, the default | One opaque panel along the bottom of each screen: Start, the desktops, pinned applications, the meters, the tray, the clock and Show Desktop | Float, and snap to the edges and corners |
| **Dock** | A thin bar across the top with the Start mark, the focused window's application menu, the tray and the clock; and a floating dock at the bottom, as long as its contents, holding the pinned applications, that moves out of a window's way | Float, and snap |
| **Tiling** | One thin bar across the top: the Start mark, the desktops by name, the window list, the processor and memory as text, the tray and the clock. No launchers | Every new window is tiled edge to edge: laying out this desk turns auto-tiling on |

Choose one on the **Desk** page of the Welcome tour, by typing `desk dock` (or `desk kdos`, `desk
tiling`) in KRunner, or from a prompt:

```sh
kdos desk                 # which desk this account is on
kdos desk tiling          # lay the panels out as the Tiling desk
kdos desk undo            # put back the panels the last desk replaced
```

Laying out a desk replaces every panel. Before it does, KDOS copies the current panels twice: once
for `kdos desk undo` (and the tour's **Undo** button), and once as a dated backup under
`~/.local/share/kdos/desk-backup/`, which nothing removes and whose path is printed. When your
panels were changed since they were last laid out, the tour asks before replacing them. The desk
also sets auto-tiling: the Tiling desk turns it on and the other two turn it off, so a desk is the
whole arrangement and never half of one.

The KDOS desk's panel needs about 1,400 logical pixels, the screen's width divided by its scale
(78 grid units at the default font), to hold everything. On a narrower screen it is laid out with less: under 1,400 logical pixels (1920
wide at 150%, 1366×768 at 100%) Start shows the mark alone and the network meter is left out, and
under 1,150 (1920 wide at 175% or 200%, 1024 wide at 100%) the pager is left out too. Under 900
logical pixels the panel does not fit at all; use the Dock or Tiling desk there. The width is the
one the screen has when the desk is laid out. When the screen later becomes narrower, through its
scale, its resolution or a larger font, the panel keeps everything by shrinking the pinned
applications' icons and the meters' charts, the figures staying whole; run `kdos desk kdos` to lay
it out again for the new width.

The pinned applications on the KDOS and Dock desks are Konsole, Dolphin, Firefox ESR and Kate, the
first entries of `~/.config/kdos/favorites`. That file is also the list of boxes started in the
background at login, so a pinned boxed application opens without waiting for its box (see
[Launching](applications.md#launching)).

The first login of every account lays out the desk named in `/etc/xdg/kdosrc`, which the installer
writes; an account's own choice is kept in `~/.config/kdosrc`. Each desk is also a Plasma layout,
applied by the global theme of every accent, so System Settings' Global Theme page lays out the same
desk when it is asked to replace the layout.

## The panel

The KDOS desk's panel is not floating: it sits on the screen edge, so the edge stays a target the
pointer cannot overshoot, and it is opaque, drawn by the KDOS Plasma style with a one-pixel rule in
the accent. Its height follows the font size.

### The meters

Three small charts show the processor, memory and network, through the **KDOS Meters** face: a
sparkline in the accent and the current figure in the monospace face, over a scanline ground when
Phosphor mode is on. The network meter is two, side by side: ↓ download and ↑ upload. Click one
for a larger chart of its sensors in a popup, which grows its charts when you resize it; System
Monitor itself is in the Start menu. Each is a Plasma System Monitor widget, so
**Configure** on its right-click menu chooses other sensors, including the KDOS ones: each box's
processor, memory, disk and energy use, the energy each application has used, and the A/B slot
state. System Monitor opens on an Overview of the processor, graphics card, memory, disks, network
and applications, and has a KDOS page with the box, energy and slot sensors, under Overview, Boxes,
Energy and Slots.
On the Tiling desk the processor and memory are plain text.

### The system tray

The tray holds Plasma's own items (volume, network, Bluetooth, battery and brightness, notifications,
the Device Notifier, the clipboard, Night Light, KDE Connect when paired) and three of KDOS's:

| Item | Shows | Acts |
|---|---|---|
| **Software updates** | A system update ready to install, one installed and waiting for a restart, and known vulnerabilities an update closes | Opens Discover's Updates page. Nothing is ever installed unasked |
| **Privacy lamps** | A lamp for the microphone, the camera and the screen, each only while something captures it, naming the program and its box | A click mutes every microphone or stops every camera or screen capture; the middle button lists each capture with its own Mute or Stop. See [Who is using the camera and microphone](#who-is-using-the-camera-and-microphone) |
| **Getting started** | A first-run checklist that ticks itself as the machine finds each step done: a backup set up, a phone paired, the regional formats set, a box made, no firmware update waiting, the disk encrypted. An item whose program is not installed is left out, and the item hides once everything is done | Opens the list, where an item can be dismissed |

Plasma's own microphone item and camera indicator are kept out of the tray, because the privacy
lamps show both and name the box behind each. Merkuro's contacts widget is kept out too: loading it
starts Akonadi, KDE's PIM store, and its agents at every login. Each can be turned back on in the
tray's settings.

## The Start menu

**Start**, or `Meta` on its own, opens Kickoff. Its favourites are your browser, Konsole, Dolphin,
System Settings and Discover; the categories below hold every installed application, KDOS's own
among them, with two KDOS categories added:

| Category | Holds |
|---|---|
| **Boxes** | Applications exported from a box with `kdos-box export`, whose own categories describe another system. An application installed from the catalogue or a pack keeps its own categories and is filed with them |
| **Library** | The offline books, maps, models, docsets and courses on this machine, the same as Dolphin's `library:/` (see [Files](#files)) |

A boxed application has a row like any other, with its own icon; its first launch in a session is
slower, because its container has to start (see [Applications](applications.md#launching)). Typing
in Kickoff searches through KRunner, so everything in the next section is found from Start as well.

Terminal programs such as `btop` or `mc` open in Konsole. Right-click any row for **Add to
Favourites**, **Pin to Task Manager**, **Add to Desktop** and **Edit Application**; **Edit
Applications…** opens the menu editor.

## KRunner and the KDOS runner

`Meta+Space`, `Alt+Space` or `Alt+F2` opens KRunner, a search box at the top of the screen. It
finds applications, files, settings pages, open windows, bookmarks and contacts, does arithmetic
and unit conversion, and runs a command line typed as it is (`ls -la` offers *Run ls -la*). KDOS
adds two runners to it, served by one small program, `kdos-runner`, which the session bus starts
on the first query.

**The KDOS command palette** offers the `kdos` verbs with their arguments filled in, so a word or
two runs them. Its rows come first, above Applications and System Settings, so `night light` and
`Enter` run the verb rather than an application whose name starts the same way:

| Type | Gets |
|---|---|
| `accent amber`, `accent next`, `phosphor on`, `night light`, `reduced motion` | The accent, or the next or previous one; Phosphor mode, Night Light, reduced motion |
| `desk dock`, `tiling off` | A desk; auto-tiling on or off |
| `update check`, `doctor` | The update check; `kdos doctor` |
| `box stop www`, `install gimp` | A box's lifecycle; an application from the catalogue |
| `why /etc/fstab` | What provides a file and why it is that way |
| `keys tile` | The global shortcuts whose names match, with their current keys |
| a favourite's two-letter code | The favourite: the `code=` field of its line in `~/.config/kdos/favorites` |
| `install gimp` | Also the packs and sets on the boot medium, opened in Discover |
| `setup network` | A route of `/etc/kdos/menu.conf` by the words of its name, shown as *Set up network* over its name `route:setup.network` |
| `lib photosynthesis` | KRunner again, with the offline library alone |
| `ask what is a box?` | The answer of the language model on this machine, when a model pack is installed; Enter copies it ([Ask](../04-programs/plasma-shell.md#ask)) |

KDOS's own System Settings pages are found by System Settings' own runner, by the keywords in
their metadata as well as their names: `previous version` finds System Versions.

**Files** are found from the file index, which an installed system keeps of file names. The live
medium keeps no index, so there KRunner offers recent documents and typed paths, and Dolphin's
search, which walks the folder, finds a file by name.

**The offline library** finds headings in the KDOS and *KDOS from Scratch* books, `kdos*` manual
pages, entries of every Zeal docset, a typed coordinate as a map location, title suggestions from
the ZIM archives `kiwix-serve` serves, and Kolibri's resources. Its results rank below everything
else; `lib <words>` opens KRunner again on those words with the library alone.

## Windows

Windows float. KWin centres a new window on the screen that has the focus, and a program's window
opens again where its last window closed, at the same size and maximized if it was: KDOS's
window-memory script records one place per application in `$XDG_STATE_HOME/kdos/window-memory`,
and is turned off on System Settings' KWin Scripts page.

Dragging a window to an edge or a corner snaps it: to the left or right half, a quarter, or the
whole screen at the top. `Meta` with an arrow key does the same from the keyboard. Beyond halves and
quarters, `Meta+T` opens the tile editor, where you draw your own tiles for each screen; holding
`Shift` while dragging a window drops it into one of them. The full model, with focus, placement and
rules, is in [The window model](../03-architecture/window-model.md).

**Auto-tiling** arranges every new window into tiles instead of letting it float. It is the bundled
Krohnkite script, off by default, and `Meta+Shift+T` switches it on and off; so do `kdos tiling
on|off|toggle`, the Welcome tour's Desk page and the KWin Scripts page. `kdos tiling on --only
hack,misc` tiles those desktops alone. While it is on, its own keys are listed on the key card; the
eight chords Krohnkite would take from the session (`Meta+L`, `Meta+D`, `Meta+T`, `Meta+.`,
`Meta+I`, `Meta+R`, `Meta+Shift+R` and `Meta+Return`) are left unbound for it, so turning it on
never moves a key you use.

**Boxed windows** wear the box's colour. Each box gets a KWin window rule that paints its windows'
title bars in the colours of one of the accents, chosen on the box's page in System Settings, so the
windows of one box are told apart from the host's and from another box's at a glance. The task
manager's tooltip for a boxed window names its box. See [Boxes](applications.md#boxes).

The title bar is Breeze's decoration with square corners, a one-pixel outline in the accent around
the active window and the title on the left. Right-click it, or press `Alt+F3`, for the window menu: move, resize, keep
above, move to another desktop or screen, fullscreen, and **More Actions → Configure Special
Window Settings**, which writes a rule for that window.

## Workspaces

Four virtual desktops, named `main`, `www`, `hack` and `misc`, in one row. `Ctrl+F1` to `Ctrl+F4`
go to each, and `Meta+Ctrl+←` and `Meta+Ctrl+→` move along the row. `Meta+W` opens the Overview,
which shows every window and every desktop and searches like KRunner; `Meta+G` shows the desktops
as a grid. Desktops are added, removed and renamed on System Settings' Virtual Desktops page. Each
desktop has a fixed id, so a restored session puts each window back on its own desktop.

## Keyboard shortcuts

Every global shortcut is KDE's, with a thin KDOS layer on top: two KDE chords given to something
else, and a few additions on chords KDE leaves free. No KDOS chord takes one from KDE without
saying so here, and `testing/chordcheck.py`, run by the tree's checks, holds every KDOS chord
against KDE's own defaults, every installed application's and fcitx5's. The live list is the key
card:

| Key | Opens |
|---|---|
| `Meta+F1`, `Meta+/` | The key card: every global shortcut the session holds, grouped into Windows, Desktops, Launch, KDOS, Accessibility and System, with a search box; `Tab` and the arrow keys move from the search box through the rows. It asks KWin's shortcut daemon for the current keys each time it opens, so a chord you rebound is shown as it now is. The same keys, or `Esc`, close it |

`kdos help --keys` prints the same list in a terminal. The tables below are the shortcuts KDOS adds
or moves, and the KDE ones a newcomer reaches for first.

### Applications and the command palette

| Key | Action | Whose |
|---|---|---|
| `Meta` | Kickoff | KDE's |
| `Meta+Space`, `Alt+Space`, `Alt+F2` | KRunner | `Meta+Space` is KDOS's; KDE's other two stay |
| `Meta+Return`, `Ctrl+Alt+T` | Konsole | `Meta+Return` is KDOS's |
| `Meta+E` | Dolphin | KDE's |
| `Meta+V` | The clipboard history | KDE's |
| `Meta+Shift+T` | Auto-tiling on or off | KDOS's |

### Terminal programs, one key each

Seven terminal programs each have a key that raises the program's window when one is open and opens
one in Konsole when none is; pressed again from one of its windows, it moves to the next. The Konsole
window stays open after the program exits, so an error it printed stays readable, and until that
window is closed the key raises it rather than starting the program again.

| Key | Program |
|---|---|
| `Meta+Shift+M` | `mc`, files |
| `Meta+Shift+E` | `aerc`, mail |
| `Meta+Shift+B` | `lynx`, the web |
| `Meta+Shift+U` | `rmpc`, music |
| `Meta+Shift+C` | `ikhal`, the calendar |
| `Meta+Shift+G` | `iamb`, chat |
| `Meta+Shift+W` | `micro`, the editor |

They are KWin actions, listed under KWin on the Shortcuts page, where each can be changed.

### Windows

| Key | Action |
|---|---|
| `Meta+Q`, `Alt+F4` | Close the window. `Meta+Q` is KDOS's; KDE gives it to the activity switcher, and KDOS ships no activities interface |
| `Alt+Tab`, `Alt+Shift+Tab` | Switch applications: one entry for each, its most recent window |
| `Meta+↑`, `Meta+↓`, `Meta+←`, `Meta+→` | Snap to the top, bottom, left or right half; twice towards a corner for a quarter |
| `Meta+PgUp`, `Meta+PgDown` | Maximize; minimize |
| `Meta+T` | The tile editor |
| `Alt+F3` | The window menu |
| `Meta+W`, `Meta+G` | The Overview; the desktop grid |
| `Meta+D` | Peek at the desktop |

### Workspaces

| Key | Action |
|---|---|
| `Ctrl+F1` … `Ctrl+F4` | Go to desktop 1 to 4. KDE also puts desktop 1 on `Meta+F1`, which KDOS gives to the key card |
| `Meta+Ctrl+←`, `Meta+Ctrl+→` | The desktop to the left or right |

### Notifications and session switches

| Key | Action |
|---|---|
| `Meta+Ctrl+Shift+N` | Night Light on or off, at once: on warms the screen now and all day, whatever schedule the Night Light page had; KDE leaves it without a chord, and this one is KDOS's |

Do Not Disturb, Night Light, staying awake and reduced motion are also switches from a prompt or
KRunner, each the same switch Plasma's own applet or settings page shows:

```sh
kdos toggle                  # every switch and its state
kdos toggle dnd              # Do Not Disturb on or off
kdos toggle night-light on
kdos toggle stay-awake       # hold off sleep and the screen locker until switched off
kdos toggle reduced-motion   # every animation off
```

Run from a key binding, with no terminal to print to, `kdos toggle` says the new state in a
notification.

### Session, screen and media

| Key | Action |
|---|---|
| `Meta+L` | Lock the screen |
| `Ctrl+Alt+Del` | The logout screen: log out, restart, shut down, sleep |
| `Print` | Screenshot of the whole screen, with Spectacle |
| `Shift+Print`, `Meta+Shift+Print`, `Meta+Shift+S` | Screenshot of a rectangle you draw |
| `Alt+Print`, `Meta+Shift+R`, `Meta+R` | Record a rectangle to a video |
| `Meta+P` | Display configuration: extend, mirror or switch screens |
| Volume, brightness and media keys | Plasma's: the volume and brightness show an on-screen level, and the media keys drive whichever player is playing, `mpd` included |

`Print`, `Shift+Print` and `Alt+Print` are KDOS's: KDE starts Spectacle's window on `Print` and
takes the whole desktop on `Shift+Print`, and gives `Alt+Print` to nothing. Spectacle's window,
from Kickoff, offers every other mode, and `kdos-shot` takes the same shots from a prompt.

### Accessibility

| Key | Action |
|---|---|
| `Meta+Alt+S` | The Orca screen reader on or off, on the desktop and on the login screen (silent there; see [Known gaps](../06-reference/known-gaps.md#the-screen-reader-at-the-login-screen-has-no-sound)) |
| `Meta+=`, `Meta+-`, `Meta+0` | Zoom in, zoom out, back to actual size |

The rest is in [Accessibility](accessibility.md).

### Changing the bindings

System Settings → Keyboard → Shortcuts lists every action with its keys, KDOS's among them, and
changes any of them; resetting a row puts back the key it shipped with. Your changes are kept in
`~/.config/kglobalshortcutsrc`.

KDOS's own chords reach that file once. They are shipped in `/usr/share/kdos/xdg/kglobalshortcutsrc`,
and as the session starts a small step copies into your file every KDOS chord your file does not
hold and that it has not copied before. A chord you reset to KDE's default therefore stays reset,
and a chord you moved stays moved. Plasma's shortcut daemon reads only your own file, never the
cascade of configuration directories other KDE settings follow, which is why the chords are copied
rather than read in place.

The input-method switch is not a global shortcut: fcitx5 turns the input method on and off on
`Ctrl+Space` and cycles its input-method groups on `Meta+Alt+Space`, moved off `Meta+Space` so
KRunner can have it; keyboard layouts switch with KWin's `Meta+Alt+K` (see
[Input methods](administration.md#input-methods)).

## Notifications

Plasma draws every notification, keeps their history in the tray's bell and honours Do Not
Disturb. A program in a box notifies through the same service as a host program, and the
notification names the program.

KDOS posts a few notices of its own, through `kdos-notice`, a small program the session starts:

| Notice | Stays until dismissed |
|---|---|
| Programs or the desktop were updated and are still running the old version, with Log Out for the desktop | yes |
| Known vulnerabilities in installed packages | yes |
| Applications chosen during the install that are not installed yet, with Install in Discover | yes |
| This version is running on trial after an update | no |
| The last update did not start, so the previous version is running | no |
| A program, boxed or not, was closed to free memory | no |

Plasma's own free-space notifier warns when a disk falls to 10% free, or to about 98 GiB free
on a disk larger than about a terabyte; the warning opens the disk in Filelight or in Dolphin, or
changes the thresholds (Configure Warning…).

`kdos notify "text"` raises a notification from a prompt or a script (`make && kdos notify done`),
and `kdos remind in 10m "tea"` one later.

## Files

Dolphin is the file manager, on `Meta+E` and pinned to the panel. Its **Places** hold your home
and its folders, the trash, network locations and removable devices as they appear, and the KDOS
**Library**, `library:/`: Books (ZIM archives and the two KDOS books), Maps, Models (language
models, speech models and voices), Docsets and Courses, each with its size. Opening an item opens
it with its program; a map opens Organic Maps. `kdos places` lists the Places and
`kdos places add DIR` adds one from a prompt.

Dolphin keeps its own defaults, which KDOS does not change: folders sort before files, every
folder shows previews (pictures, documents and, through ffmpegthumbs, video frames), and `F4` opens
a Konsole panel in the folder.

Dolphin's right-click menu gains five KDOS actions:

| Action | Does |
|---|---|
| **Open in a throwaway box** | Opens the file with the boxed application for its type, in a box made for that one file: no network, an empty home, no camera, nothing of the session's processes, and only the file itself, read-only. Whatever the application writes is destroyed with the box when it exits. For a document from somewhere you do not trust |
| **Send with a Code Word** | Sends the selected files or folders to another machine over one code word, with [`kdos share`](../04-programs/kdos-command.md#kdos-share), in a Konsole window that shows the code and its QR code |
| **Git Status Here** | On a folder: opens lazygit in Konsole there |
| **Verify** | On any file: checks it against a `.sha256`, `.sha512`, `SHA256SUMS` or `SHA512SUMS` beside it, and a `.kpack`'s or a package's KDOS signature, with [`kdos verify`](../04-programs/kdos-command.md#kdos-verify), and says in a notification whether it matched |
| **Transcribe** | On a local audio or video file: writes its speech as text beside it, with the local Whisper model. Offered only while `whisper-cli` and a model are installed |

Double-clicking a `.ktar` application set or a `.kpack` pack opens it in Discover, which installs it
offline (see [Carrying a set to another machine](applications.md#carrying-a-set-to-another-machine)).

A drag between two folders on one filesystem moves without asking; across filesystems the
Move/Copy/Link menu still opens.

### The trash

Deleting in Dolphin moves to the trash, `~/.local/share/Trash`, the freedesktop location every
desktop and file manager shares. `kdos trash <file>` does the same from a prompt, and
`kdos trash --restore` puts files back.

### Opening a file

Double-clicking a file opens it with its type's default application. The defaults are KDE's
applications unless an application on the image is clearly better at the job: folders open in
Dolphin, PDF in Okular, e-books in Calibre's viewer, text in Kate, pictures in Gwenview, video in
Haruna, music in Elisa, web links in Firefox ESR and mail links in Thunderbird. The full table is in
[Default applications](applications.md#default-applications). Right-click a file for **Open With**
and *Other Application…*, where *Set as default app to open … files* makes the choice the default
for that type; System Settings' Default Applications page sets the default for each role, and its
File Associations page for one type. Your choices are kept in `~/.config/mimeapps.list` and
outrank the shipped ones.

A boxed application can open a file like any other, and a link clicked inside a box opens on the
host through the desktop portal.

## The clipboard

Klipper, in the tray and on `Meta+V`, keeps a history of what was copied, text and images, and
pastes any entry back. Wayland keeps the clipboard and the primary selection (select to copy,
middle-click to paste) apart; both work across native, boxed and Xwayland windows. `wl-copy` and
`wl-paste` reach the clipboard from a prompt.

## Locking, idle and power

On mains power the screen dims after 5 minutes idle, locks after 10 and turns off after 15. The
lock has a 5-second grace, in which a key or a move unlocks without a password. On battery the
dimming and switching off come sooner, as Plasma's power settings ship them. The screen also locks
before every sleep, so a machine never wakes unlocked. `Meta+L` locks at once.

| Setting | Where |
|---|---|
| When the screen dims, turns off, and when the machine sleeps | System Settings → Power Management |
| When the screen locks, and the lock screen's wallpaper and clock | System Settings → Screen Locking |
| What the lid and the power button do | System Settings → Power Management, inside a session. With nobody logged in, closing the lid suspends and the power button powers off |
| The power profile: power saver, balanced, performance | The battery item in the tray. Each profile runs TLP's tuning for it, so TLP's own settings in `/etc/tlp.d` stay in force |

The lock screen and the login screen are drawn alike, in the accent. A virtual machine and the live
medium never lock by themselves: KDOS puts a set of defaults in front of its usual ones there, and
`Meta+L` and the lock before sleep still work. In a virtual machine the screen does not dim or turn
off either, and the machine does not go to sleep when idle or when a lid closes.

`kdos toggle stay-awake` holds off sleep and the screen locker until it is switched off again, for
a presentation or a long job; the battery item lists it as what is blocking sleep.

Log out, restart, shut down and sleep are on Kickoff's power buttons, `Ctrl+Alt+Del` and the
lock screen. From a prompt, `kdos-power suspend`, `poweroff` and `reboot` ask the session manager,
elogind, as the buttons do.

## Displays

System Settings → Display & Monitor arranges the screens, and sets each one's resolution, refresh
rate, scale, rotation and colour profile, and which is the primary. `Meta+P` switches between
extending, mirroring and using one screen. Scale is set per screen, and may be a fraction.
`kscreen-doctor -o` lists the outputs from a prompt, and `kscreen-doctor output.HDMI-A-1.scale.1.5`
changes one. Night Light, which warms the colours in the evening, is on System Settings' Night
Light page and `Meta+Ctrl+Shift+N`.

On a new machine with one screen there is nothing to set: the screen comes up at its preferred
mode.

## Removable media and devices

Plug in a stick, a disk or a phone and the Device Notifier in the tray offers to open it. Opening
it mounts it, as you, at `/run/media/<user>/<label>`; **Safely remove** unmounts and powers it down.
Mounting, unlocking an encrypted stick and ejecting removable media need no password for the person
at the machine. An internal disk is the administrator's: for a member of `wheel` at the machine it
mounts without a password too, and anyone else is asked for an administrator's password.

In a live session the medium the system is running from is not listed: it is in use for the whole
session and cannot be removed, and its files stay at `/mnt/iso`. The RAM root of the live session
is not listed as a device either.

When a stick holds an application set (a `.ktar` file at its top level), a notification offers to
open it in Discover, which installs it offline.

| Job | Program |
|---|---|
| Partition, format, resize, check | Partition Manager |
| Write a disk image to a stick | ISO Image Writer, the default for `.iso` files |
| A disk's health | Plasma's disk monitor notifies when a disk's SMART health fails, and Info Center shows each disk's report |
| An encrypted folder | Plasma Vaults, in the tray |
| A camera's photos | `camera:/` in Dolphin, or Gwenview's importer |
| A phone's files | KDE Connect over the network, or `mtp:/` in Dolphin over USB |

## Who is using the camera and microphone

The privacy lamps in the tray light while something is capturing: one for the microphone, one for
the camera, one for the screen. Each names what is capturing and, for a boxed program, its box:
"Firefox in box www". A click mutes every microphone capture, or stops every camera or screen
capture that can be stopped; the middle button lists each capture with its own Mute or Stop. While
nothing captures, the lamps are out of sight.

The same from a prompt:

```sh
kdos privacy                  # who has the microphone, the camera and the screen now
kdos privacy --watch --json   # one line now and one at every change
kdos privacy mute <id>        # mute one capture the list names; also unmute, stop
```

The microphone and the screen are seen through PipeWire's streams, the screen as KWin's
screencast streams whether Spectacle or a program sharing the screen asked for them, and the
camera through PipeWire's camera nodes and the holders of `/dev/video*`, so a program that opens a
camera device directly is seen too.

## When the desktop misbehaves

Start with `kdos doctor`, which checks the session's own pieces (KWin, the Plasma shell, the KDE
portal, the session manager, the polkit agent, the password store, the disk service and the update
service) along with the rest of the machine; System Settings' Health page shows the same report.

| Symptom | What to do |
|---|---|
| The panel or the desktop is frozen or gone | A shell that stopped comes back by itself within a few seconds. One that is frozen: `kquitapp6 plasmashell; kstart plasmashell` from Konsole or KRunner (`kstart` detaches it, so closing the Konsole leaves it running), or log out and in. The panels are kept in `~/.config/plasma-org.kde.plasma.desktop-appletsrc` |
| The panels are not as you left them | `kdos desk undo` puts back the panels the last desk replaced; older layouts are under `~/.local/share/kdos/desk-backup/` |
| Windows tile when they should float | `kdos tiling off`, or `Meta+Shift+T` |
| A key does nothing | The key card, `Meta+F1`, shows what the key is bound to now; System Settings → Keyboard → Shortcuts changes it |
| The screen is black, or the session will not start | `Ctrl+Alt+F2` for the recovery console. The session's log is `~/.local/share/sddm/wayland-session.log`, the login screen's is `/var/log/sddm.log`, and the system log `/var/log/messages`, read with `sudo` |
| The look is wrong after an accent change | `kdos theme --audit` reports what does not match the palette; `kdos theme <accent>` repairs it |
| Phosphor mode left the screen hard to read | `kdos phosphor off`, from a prompt or KRunner |

When the session itself will not start, the login screen comes back, and the reason is in the
session log and in the system log. The session will not start without its session bus, so
`service --user status dbus`, run at the recovery console as the same user, says whether it is
running. The pieces of a session and how they start are in [The session](../03-architecture/session.md).

## See also

- [Theming](theming.md): the accents, Phosphor mode, fonts and the wallpaper
- [Applications](applications.md): Discover, boxes and the default applications
- [Administration](administration.md): networking, printers, users, updates and backups
- [Accessibility](accessibility.md): the screen reader, zoom and the keyboard aids
- [The window model](../03-architecture/window-model.md): placement, snapping, tiles and rules
- [KWin](../04-programs/kwin.md) and [The Plasma shell](../04-programs/plasma-shell.md): how KDOS
  configures them
- [The session](../03-architecture/session.md): what starts after the login

<!-- book-nav -->
---

*Part II — Using KDOS, chapter 7.* Previous: [6. Installation](installation.md) · [Contents](../README.md) · Next: [8. Applications](applications.md)
