# kdos-shell

One binary providing forty-seven commands, dispatched on the name it was invoked as: the panel,
and every surface that pops up from it or is reached by a key. This is the largest program in
KDOS and the one most of the desktop actually is.

Everything here follows [the design language](../03-architecture/design-language.md), and the
shared rules live there rather than being restated per surface.

## One binary, many names

The authoritative list is the name table in `main.c`. A name in that table with no matching entry
point is a **link error**, not a missing feature — the table and the implementation are one edit.
A name the build does not create a link for is a program nothing can reach, which is the other
half of the same mistake.

| Name | Is | Section |
|---|---|---|
| `kdos-shell` | The panel | [The panel](#the-panel) |
| `kdos-start` | The Start menu, with DESQview's two-letter codes | [kdos-start](#kdos-start) |
| `kdos-launcher` | Full-screen application search, and files behind a key | [kdos-launcher](#kdos-launcher) |
| `kdos-menu` | Root, System and window menus | [kdos-menu](#kdos-menu) |
| `kdos-desk` | The desktop and its icons | [kdos-desk](#kdos-desk) |
| `kdos-pick` | The file chooser and browser | [kdos-pick](#kdos-pick) |
| `kdos-notifyd` | The notification daemon | [Notifications](#notifications) |
| `kdos-netagent` | The passphrase NetworkManager asks for | [The device managers](#the-device-managers) |
| `kdos-notify` | The notification centre | [Notifications](#notifications) |
| `kdos-osd` | Volume and brightness | [kdos-osd](#kdos-osd) |
| `kdos-cal` | The calendar | [The small surfaces](#the-small-surfaces) |
| `kdos-settings` | Settings | [kdos-settings](#kdos-settings) |
| `kdos-net` | Networking | [The device managers](#the-device-managers) |
| `kdos-bt` | Bluetooth | [The device managers](#the-device-managers) |
| `kdos-audio` | Audio devices | [The device managers](#the-device-managers) |
| `kdos-devices` | Cameras, microphones, removable media | [The device managers](#the-device-managers) |
| `kdos-disks` | Disks: mount, unlock, SMART, partition, erase | [The device managers](#the-device-managers) |
| `kdos-connect` | A folder on another machine, over SMB | [The device managers](#the-device-managers) |
| `kdos-print` | Printers: what is set up, what is on the network | [The device managers](#the-device-managers) |
| `kdos-time` | The zone, the clock, and whether the clock is right | [The small surfaces](#the-small-surfaces) |
| `kdos-users` | The accounts, and which one tty1 logs in | [The small surfaces](#the-small-surfaces) |
| `kdos-update` | What is behind, what is vulnerable, which slot is live | [The small surfaces](#the-small-surfaces) |
| `kdos-firewall` | Which services answer the network | [The small surfaces](#the-small-surfaces) |
| `kdos-backup` | What is in the restic repository, and adding to it | [The small surfaces](#the-small-surfaces) |
| `kdos-clip` | Clipboard history | [The small surfaces](#the-small-surfaces) |
| `kdos-status` | The overflow popup | [The overflow chevron](#the-overflow-chevron) |
| `kdos-traymenu` | A tray item's own menu, from its `dbusmenu` tree | [The tray](#the-tray) |
| `kdos-tip` | Tooltips | [Tooltips](#tooltips) |
| `kdos-ime` | The input-method candidate window | [The candidate window](#the-candidate-window) |
| `kdos-teams` | The window list | [The small surfaces](#the-small-surfaces) |
| `kdos-display` | Screen configuration | [The small surfaces](#the-small-surfaces) |
| `kdos-keys` | The keybinding card | [The small surfaces](#the-small-surfaces) |
| `kdos-doc` | The documentation viewer | [The small surfaces](#the-small-surfaces) |
| `kdos-openwith` | Choose a handler | [The small surfaces](#the-small-surfaces) |
| `kdos-run` | The run box | [The small surfaces](#the-small-surfaces) |
| `kdos-prompt` | Yes/no by exit status; `--input` asks for a line instead | [The small surfaces](#the-small-surfaces) |
| `kdos-slit` | The dockapp column | [The small surfaces](#the-small-surfaces) |
| `kdos-saver` | Attract mode between idle and lock | [The small surfaces](#the-small-surfaces) |
| `kdos-about` | What this machine is | [The small surfaces](#the-small-surfaces) |
| `kdos-palette` | One search over everything the desktop can reach | [kdos-palette](#kdos-palette) |
| `kdos-style` | How the screen looks: the accent, and the font | [The small surfaces](#the-small-surfaces) |
| `kdos-calc` | The calculator | [The small surfaces](#the-small-surfaces) |
| `kdos-note` | The scratch pad | [The small surfaces](#the-small-surfaces) |
| `kdos-chars` | The character map | [The small surfaces](#the-small-surfaces) |
| `kdos-contacts` | The address book | [The small surfaces](#the-small-surfaces) |
| `kdos-ascii` | A picture, as characters | [The small surfaces](#the-small-surfaces) |
| `kdos-trash` | What was deleted, and the way back | [kdos-trash](#kdos-trash) |
| `kdos-peek` | What is in a file, without its application | [kdos-peek](#kdos-peek) |
| `kdos-find` | Files by name or contents, applications, recents | [kdos-find](#kdos-find) |
| `kdos-pix` | One picture, and the folder it is in | [kdos-pix](#kdos-pix) |
| `kdos-rec` | Record a microphone, and transcribe it | [kdos-rec](#kdos-rec) |

## Places

Home, the XDG user directories that exist, then whatever `~/.config/kdos/places` adds. **One
reader**, `kxdg_places()`, so the desktop folder, the Places menu, the chooser and *Add to Places*
cannot disagree — see [libkxdg](../05-developer/c-libraries.md#libkxdg) for what they used to
disagree about.

| Where | How |
|---|---|
| `kdos-menu --places` | The whole column, plus Trash and Computer |
| `kdos-desk` | *Add to Places* on a folder's context menu, and on the wallpaper, where it keeps `~/Desktop`; never on a file, because a file is not a place |

**`kdos-desk` draws the console's ground and never the compositor's.** Under the compositor the
wallpaper is a PNG the compositor draws and this surface is transparent over it, so art painted here
would be a rectangle of opaque cells on somebody's photograph. On the console there is no wallpaper
and no compositor: `background.c` turns the chosen piece into cells through `libkvt`'s parser and
its render boundary — the parser that reads a terminal, because the file is exactly what a program
writes to one — and the colours reduce to the theme's eight slots, so `kdos theme` moves the art.
It is reloaded on the same signal the accent is, which is also what `kdos background` sends. The
piece is **centred and clipped**, never scaled, and its blank cells are not painted: the art is
behind the icons, and drawing its spaces would put a second ground over the desktop's.

**A real picture is the second tier, where the display has pixels.** The same setting takes an
image as well as character art — `sh_bg_path()` answers with the kind it found, decided by the
extension alone because `background.c` links no pixel code at all. A picture goes through
`picture.c`, the same decode-crop-scale-and-cut every other picture on this desktop uses, and is
registered as a grid of sprite tiles behind the icons.

It is **cut to cover the desktop, centred, and never stretched**: the crop is the largest centred
rectangle of the file that has the screen's own shape, and *that* is what is scaled to every cell.
The shape is taken in **pixels and not in cells** — a cell is about twice as tall as it is wide, so
a ratio taken from the grid would stretch every wallpaper by that factor. The cut is redone only
when the grid moves, which is what a font step or a resized display is.

**Where there are no pixels there is no picture**: a `tty1` at the 512-glyph console font, and a
view over `ssh`, have none, and `sh_pic_view()` says so by failing — the desktop falls back to the
theme's ground. Character art is the tier that draws everywhere, which is why `.txt` is tried first
when both exist. Icon labels stay readable over a photograph because they paint their own `KT_BG`
rather than letting the ground show through.
| `kdos-pick` | `Ctrl+P` opens the column over the file list, with a **Recent directories** group under it |

**Recent** is the same shape: `kdos-appbox open` is the one function every open passes through, so
it is the only place `recently-used.xbel` is written. `kdos-start`'s right column draws a RECENT
group from `kxdg_recent_all()` — only when there is something in it, because a heading over nothing
reads as a list that failed to load rather than as a machine nobody has opened anything on. A row
opens through `kdos-appbox open` again, so a recent file opens with whatever its **type** is bound
to rather than with whatever last touched it.

**The Files row is not the same program on both desktops.** On the console it opens `mc` in a
terminal — two panels, ten function keys and an `F2` user menu, which is what a text desk is for and
what works on `tty1` with nothing else running; under the compositor it opens `kdos-pick` on `$HOME`,
which is the chooser every other surface already reaches. The branch is `sh_session_prog()`'s, so
which desktop this is is decided in one place. The places rows above it open `kdos-pick` on both,
because a place is a jump to a directory rather than a file manager.

**Recent directories come from `zoxide`**, which `bash.bashrc` already initialises, so the group is
the tree a person has actually walked rather than one the chooser invented. It is read when the
column is opened — one process, not one per frame — and is silent when zoxide or its database is
absent, which is the same answer either way. It is a **separate call** from the places themselves:
*Add to Places* decides whether a folder is already a place by asking for the column, and a
frecency guess folded into that answer would make it refuse a folder somebody had merely visited.

**An archive opens two ways, and they are different questions.** `mc` mounts a tar, a cpio, a zip or
an `ar` as a **directory** through its own VFS — the right answer when you want one file out of one,
and a view no MIME type expresses. Everything its VFS cannot reach on this image goes to
`kdos-openarchive`, which extracts beside the file. Only 7z and rar get a row: that is the
intersection of what `ouch` reads and what shared-mime-info can name, and a row for a type the image
cannot produce is one nobody can tell is dead.

**The desktop's name editor is left by Escape and by a click, and its entrance is the pointer.**
New Folder, New File and Rename put a line editor on the status row, and the menu that opens them is
reached with the right button — so a state a pointer can enter must be one a pointer can leave. A
click abandons the half-typed name rather than applying it, and the press is then handled like any
other: a click that visibly did nothing is how a person concludes the desktop has stopped answering.
Escape is the Cancel rung of the window contract's own ladder, so there is one cancel and not two.
Without the click, an editor opened by the pointer swallowed every pointer event over the whole
desktop — and on a display that gives this surface no keyboard, that is a state with no exit at
all.

**The desktop's menu is the same table with its own rows under it.** `kdos-desk` draws libkxdg's
file verbs, a rule, then what only a desktop can answer — New Folder, Sort Icons, Change Wallpaper
and the rest. On bare wallpaper it offers only the verbs that mean *here*: Terminal, Find, Add to
Places and Git. Open, Share and Move to Trash read as acting on **the thing**, and on the wallpaper
there is none — a Move to Trash there would act on the desktop folder itself.

**The chooser gets a list, not a sidebar, and that is the width talking.** It is sixty-four columns
and already spends its right-hand column on a preview pane; a third column would leave a file's
name about thirty cells, and a chooser that cannot show a name is not a chooser.

## The keys every surface answers

Twenty of these names answer the same contract, and the bottom row of each says what the rest of
its keys do **right now**. `Esc` steps back one level and only then closes; `F1` opens the
surface's page in `kdos-doc` where it has one. The full rule — the ladder, the pushed row, the
`&`-marked accelerators — is in
[the design language](../03-architecture/design-language.md#the-keys-every-surface-answers).

Seven surfaces claim an `F1` page today: `kdos-settings`, `kdos-display`, `kdos-net`, `kdos-bt`,
`kdos-devices`, `kdos-connect` and `kdos-rec`, plus [`kdos-res`](kdos-res.md). The pages live in `/usr/share/kdos/doc` and
`testing/preflight.sh` refuses a claim with no file behind it, so a surface that does not appear
here neither advertises `F1` nor answers it.

**Two menus take the ladder and not the row**: `kdos-menu`'s cascade and its `--windows` list. Both
are overlays sized to their content, so a hint row has to be bought out of the height they ask for
— and the System menu is 22 items against a 24-row cap, exactly full, so reserving one scrolls
*Shut Down* off the bottom of the menu somebody opened to shut down with. The row would also be
saying what a menu already says: Enter, the arrows and Escape are what a column of labels means.

Nine names deliberately do **not** take the contract, and the reason is the same one each time:
they take no keyboard. `kdos-osd`, `kdos-tip`, `kdos-slit`, `kdos-saver` and `kdos-ime` are
transient chrome with no input at all; `kdos-notifyd` is a daemon; `kdos-ascii` is a filter;
`kdos-prompt` and `kdos-run` are shorter than the eight rows below which a hint line would eat a
third of the window.

## The panel

Two rows, on the bottom edge by default. The **second row is not padding**: it carries the
clock's date, a window button's own title under its application name, and the meters strip.

### What the bar is drawn with, and what happens where there are no pixels

Every affordance on this bar — the body, the edge against the desktop, a button's plate, the
hairline between two segments, a meter's gradient — is **pixel chrome**, drawn by libkchrome into
the backdrop one layer under the cell grid. It costs no columns and no rows, which is why the bar
can be two rows and still look like chrome.

**The console has no such layer.** `libkcon` carries cells and nothing else, so on that display
every one of those is drawn nowhere. What was left was text on a body one shade off the desktop's
own: no boundary, no separators, and a `Start` button whose label is drawn in the *plate's* colour
and therefore vanished the moment the pointer touched it. `cells_only()` is the one question that
decides, and three things answer it:

- **the bar takes one more row** and spends it on its own edge, in the double horizontal every KDOS
  window frame is drawn with, on the side the desktop is. A docked surface cannot change its
  thickness after it attaches, so this is decided before `kdisp_init`;
- **a segment boundary is the double vertical**, in `KT_MID`, in the column the layout already
  reserves for it;
- **the `Start` button's plate is a cell fill** — `KT_DIM` at rest, the accent under the pointer,
  the warning slot while its menu is open — and the ink is read against whatever the plate turned
  out to be.

**Every picture on the bar is registered for, and drawn into, the content rows**, never the
surface's full height. The edge row is stamped across every column *after* the layout, so a sprite
that claimed the whole surface loses its top row of tiles to the double horizontal and what is left
of the picture then sits high in the rows that remain.

**The Start button pads by a whole column at each end, in both renderings** — `START_PAD`. The tile
pads by one cell and the character fallback by one column, so the mark starts a column in whichever
tier drew it and the plate keeps the same air at both ends whatever the mark and the word measure.
It takes **no height parameter**: it is laid out in `bar_y0`/`bar_h` like everything else on the
bar, because it is the one control anchored in the screen's corner and a mark drawn into the edge
row, or into column 0, is unmissable there.

`applet_row`, `bar_y0` and `bar_h` are where the content lives inside those rows, decided once per
frame and read by everything the frame calls. Two functions used to derive the row themselves from
the height, which is exactly what an edge row would have broken.

**And the pictures arrive.** On the console libkcon puts a sprite's BYTES on the wire through a
callback the surface registers, and this one registered none — so every icon the bar drew reached
the display as metadata with no pixels and became a blank cell that had still spent its columns.
`sh_pic_backend()` is that callback and `sh_pic_cell_w()` is the nominal cell the wire is bounded by,
and both are what a surface passes to `kicon_init()`. `kdisp_cell_w()` is **1** on a console surface
— there are no pixels on that side of the socket — and libkicon refuses a cell under four pixels, so
a surface that hands it the backend's cell gets no pictures at all. `icons_drawable()` is the one place
that asks whether a picture can be drawn at all, so no control spends cells on one that cannot.

### Nothing on this bar may move while it is being read

Every field on the right wing is a **fixed** width, and none of them is the width of the value
currently in it. The wing is laid out right to left, so an item that grew by a column carried the
separator, the meters strip and the right edge of the window list sideways with it — the bar
shifted under the eye whenever a percentage crossed 10 or 100, or a rate gained a unit.

| Field | Reserved | The widest legitimate value |
|---|---|---|
| A meter's reading | `MET_VAL_W`, 4 (plus the arrow for a mirrored band) | `100%`, and `fmt_rate` never writes more than four |
| The clock's segment | `clock_field()`, measured once over a synthetic year | whatever the person's own `strftime` format can produce |
| The full-disk mark | one column, always | it is blank when the disk is not full |
| An applet tile | `AP_TILE_W`, 3 — `AP_NET_W`, 4 for a rate | a rate is a number **and** a unit |
| The one-row CPU applet | `AP_CPU_W`, 8 | `CPU 100%` |

`applet()` is `applet_w()` sized to its own label, for a readout whose width cannot change; anything
whose reading grows a column passes the field it reserves.

`fmt_rate` is capped at four cells with the unit always present: it could write `1023k`, and a
five-character value in a four-cell field lost its letter from the right, so `1.2M` reached the
screen as `1.2` — one and a fifth bytes a second. The decimal is what gives way, and only below ten
of a unit.

The sparkline's zero is a **baseline**, not a space. `ramp_index(0)` is exact empty — right for a
gauge, wrong for a chart — so an idle meter drew ten spaces between its label and its reading and
the track vanished: the wing read as words with gaps rather than as charts.

### Autohide

**`panel_autohide` in `comp.conf` is the pointer's.** Hidden, the panel drops its exclusive zone, so
the strip it was holding goes back to the windows; what stays on screen is one row of accent shade,
because a panel that vanished completely is a panel nobody finds again. An enter over that strip
shows it at once and a leave arms a deadline rather than hiding under the hand.

**`Super+Shift+space` is a person's, and it outranks the pointer.** `kdos panel toggle` signals the
running panel — `SIGUSR1`, by name, which reaches the panel and not the desktop icons or the
notification daemon, the other `argv[0]`s of the same binary — and while the bar is put away the
pointer will not bring it back. Without that it returned the first time the mouse crossed the bottom
row, which reads as a chord that did not work. The console's half of the chord is a session action
with no process to signal; one name for both is what keeps the key card one card.

### Layout and degradation

The bar is laid out in **four passes**, and the order is the priority:

| Pass | Gives up |
|---|---|
| 0 | Nothing — the whole bar |
| 1 | The meters strip |
| 2 | Also the Start button's word, leaving its mark |
| 3 | Also the quick-launch row |

**No pass may drop a window button.** Before pass 1 is even reached, the window list degrades on
its own: full labels, then labels squeezed to their floor, then **icon mode** — three cells per
window, the picture centred over both rows with a state marker under it — and only after that an
overflow cell.

Icon mode before overflow is the right trade: a picture that identifies the window is worth more
than a word beside it, and dropping a window from view while the row still has room for a picture
of it is not.

**One floor, not two.** The right-hand wing reserves space against the window list's **icon**
floor, and the acceptance test measures against the same figure. Two statements of the same rule
is how a pass throws away the layout it was measured for.

**The last column of the bar is show desktop, and it goes both ways.** Anything on screen and a
click minimises it; nothing on screen and the same click puts back what that column hid.

**The way back is the remembered set and not the live list.** kdos-comp re-reports a window on
another workspace as minimised — the pager's occupancy is derived from exactly that — so a column
that un-minimised everything the bar calls minimised would haul every other workspace's windows
onto the one in front of you, and would undo a minimise somebody made on purpose before the press
as well. The memory is window **ids**, matched against the live list on the way back, so a window
that has closed or that something else restored is simply not found. It is the panel's own: a panel
started against an already-cleared desk has hidden nothing, and the column then does nothing until
there is something on screen to hide — the same answer it gives for a desk somebody else cleared.
The console session's `show-desktop` chord (`Super+Shift+d`, see [kdos-con](kdos-con.md)) keeps its
own memory the same way and is limited to the current desk; the two are separate controls over the
same windows, so pressing one and then the other can leave the other's direction inverted for a
press.

### The Start button

Three states, one function drawing both the pixel tile and the character fallback — a control
whose two renderings disagree about its own state is worse than either.

- **Quiet at rest.** The accent belongs to hover, and the warning colour to the menu being open.
  At full strength it is the loudest object on screen, on the one control that is never the thing
  being looked at.
- **It carries the word**, because a button that is a picture the same size as the application
  icons beside it does not read as the way in. The word is measured through the canvas, so a
  process that draws canvases and never loads a cell font has to have brought `fcft` up — see
  [libkcell](../05-developer/c-libraries.md#libkcell). A measurement of zero there is a button
  that silently sizes itself to its mark alone and drops both the word and the plate.
- The tile lays out padding, mark, gap, word, padding — where padding and gap are **different**
  numbers — and the content is then **centred** in the tile, because a tile is a whole number of
  cells and the content is not. The rounding slack split between both ends is invisible; pushed to
  one end it is an asymmetry you can see.
- **Where the mark falls back to the literal word, the label is dropped**, since the brand printed
  beside itself says it twice.
- **The mark takes the button's whole height less a fifth of a cell at each end** — the same air
  every other picture on the row leaves, so the mascot is not the smallest thing on a bar of
  application icons, and it still does not touch the top and bottom of its own plate.
- **On a character grid the plate is the tile's own background slot.** There is no pixel layer to
  record a plate into there, and the backend fills a sprite cell's background before compositing
  the picture over it — so the three states travel as `KT_DIM`, `KT_ACCENT` and `KT_WARN` in that
  slot, with the word inked to read against whichever of them was drawn. Left at `KT_SURFACE` the
  button is a mascot floating on the bar with no plate, no word behind it and no visible hover.

### Quick launch

Pinned applications, from `~/.config/kdos/favorites`.

- **Hover is a fill behind the icon**, never a tint of the icon: tinting changes what the
  application looks like.
- **A launch pulses** the fill for about a second, and the panel shortens its own poll only while
  one is running.
- **Dragging reorders**, and the order is written back. The **button** is what is remembered
  across events, because plain and dragged motion are indistinguishable in the protocol; the
  launch therefore happens on **release**, since a launch on press fires before a drag can begin.
  Released off the row it does nothing.

### The window list

| Button | Does |
|---|---|
| Left | Toggles: minimises the window you are in, restores one you are not, opens the member list for a group |
| Middle | Opens a **new instance**, from the pinned entry's command or the window's own desktop entry |
| Right | The **window menu** |

Right opens the menu rather than minimising, because minimising is what left already does and the
right button is the one every other desktop reserves for these verbs. Closing is the menu's, not
the middle button's: middle is the button a hand hits by accident on a wheel, and on a row of
icon-mode squares what it closed would have had no name on it and no confirmation.

**A window button is a button.** An inactive chip is filled, so the row reads as controls rather
than as floating words; hover is a step brighter, and focused brighter still, so hover cannot be
misread as "this is the window you are in".

**The shape is a pixel plate, or a cell fill where there is no pixel layer.** Under a compositor
the cells stay `KT_SURFACE` and the plate is the button; a cell fill there would paint over it. A
character grid has no plate, so the chip fills itself — `KT_DIM` at rest, `KT_MID` hovered,
`KT_ACCENT` focused, each with its slots swapped — and without that the row is a line of floating
words with no edges.

**The state cue is a pixel underline, and a cell marker where there is no pixel layer.** Running,
focused and minimised are a two-pixel accent line under the plate — one fact in one place, so a
chip carrying its application's own picture is not also carrying a glyph saying the same thing. The
console has no pixel layer, so a group that is entirely minimised marks the **column between the
picture and the label**, which is the chip's one spare cell. Without it, a minimised window and a
visible one are the same button there.

**Icon mode needs the pixel layer, whatever `task_labels` says.** A dock button is a 40x40 square
whose shape is a plate and whose state is an underline, and both are pixels: on a character grid
the same button is a 2x2 picture on the bar's own background with nothing saying it is a button, is
running, or is minimised — and an application the atlas has no artwork for arrives as one lowercase
letter in four cells. The labelled chip is what the cell layer can draw, so the console gets it.

**The label is the desktop entry's name**, resolved once when the identifier arrives rather than
per frame: name, then title, then the raw identifier. An application identifier is chosen so it
cannot collide, so the reverse-DNS form is not what belongs where a human name goes. The entry is
found by its id first and then by the window-class field, which is what an X11 client under
Xwayland needs.

**The box appears in a label only when it disambiguates** — where the same application runs in two
boxes. A badge on every button on a machine where every application is boxed says nothing.

Applications can put a **count badge or a progress bar** on their button through the launcher-entry
interface, which `unity.c` implements.

### The meters strip

Fifteen cells of live graphs, drawn as a pixel tile on the second row. `meters =` in `panel.conf`
selects them and **the order is the order of importance**, because a narrow bar drops them from
the right.

| Meter | Source |
|---|---|
| `cpu` | Aggregate processor time |
| `ram` | **Available** memory, never total-minus-free — Linux spends every spare page on cache, and that arithmetic reports a healthy machine at 95% |
| `net` | Received and sent, mirrored about a midline on one shared scale |
| `disk` | Filesystem usage, sampled every ten seconds |
| `diskio` | Bytes read and written, **whole disks only** — the kernel lists partitions beside their disk and summing everything counts each byte twice |

Rules that make the charts readable rather than merely present:

- **They sample on their own clock, not on the draw loop.** The loop is woken by events, so
  re-sampling per frame divides by whatever interval happened to pass — and moving the mouse made
  the reading flash between extremes. It is not a rendering fault; the *number* was wrong.
- **The wait is shortened to whatever is left of the interval.** With a flat one-second wait, any
  event returned early, found the deadline not due, and then waited a full second again — so
  samples landed irregularly, and a chart draws one sample per pixel.
- **The interval is half a second**, not because it is more accurate but because a chart is a thing
  in motion. The label is a smoothed average; the chart plots the raw samples, because the point of
  a chart is the spikes.
- **A nice scale with hysteresis.** The axis snaps to a ladder of round numbers and grows as soon
  as a sample does not fit — a clipped chart is a lie — but shrinks only well below the current
  rung, because one threshold in each direction oscillates for a stream sitting on the boundary.
- **A filled area under a line, not a row of bars.** At one sample per pixel, bars are grass.
- **A flat band still shows time passing**: a faint gridline every ten seconds keyed to the
  absolute sample number, so it marches left with the samples. Without it an idle link renders a
  still image.
- **Every series is pushed on every tick**, carrying the last value forward when a reading is
  momentarily unavailable. A series that skips a tick is *shorter* than the one beside it and the
  two creep out of step for the rest of the session. A series with **no** sample at all is left
  empty rather than held at zero.
- **The trace spans the whole band from the first sample**, so the picture moves from the first
  sample onward rather than staying pinned to the right edge until the ring fills.

Clicking opens the resource monitor; middle opens the stutter attribution; right opens the energy
report. All three are real features of this system that nothing else pointed at.

### The status wing

Two rows, and **every applet uses both** — a readout with thirty-two pixels of nothing under it
looks unfinished beside a clock that uses both.

Two shapes, and the split is what each carries: **a number goes under its picture, a name goes
beside it.**

- The **compact** applet is three cells: an icon over a reading. Three, not two, so the icon is
  centred in the tile rather than in its left half — and so that a one-character value is not
  pushed under the icon's left edge.
- The **wide** applet is an icon with a headline and a detail line, for the two readouts carrying
  somebody's name: the media title, and the application holding the microphone.

**The media title comes from whichever source can answer.** MPRIS over the session bus is the
protocol a desktop player speaks, and a player that speaks it also answers the transport keys beside
the cell. mpd speaks none — so `kdos-mpctl watch` writes the same answer to
`$XDG_RUNTIME_DIR/kdos/nowplaying` and the widget falls back to that file, reading it at most once a
second because the draw is not on a tick. The file's leading `>` or `||` is the play state and is
stripped before the title is drawn. **One widget reading both**, because two cells disagreeing about
what is playing is worse than one that is sometimes empty. The same file is
[`kdos-con`'s own bar](kdos-con.md) when that one is drawn.

**An applet tile is a fixed width whatever it says.** The wing is laid out right to left and
everything to its left starts where that walk stopped — so a readout going from three characters
to two would narrow the wing and slide every chart on the panel sideways.

The pager is little screens: one filled cell per workspace in its own state colour with the number
under it, and the second cell of the stride left as background. **Its hover is the width of what
was drawn**, not of the stride, or the highlight lights two cells under a square that is one.

**The window list stops at the wing.** The wing's left edge is whichever of its two rows reaches
further left, and the meters strip is handed the room the window list needs as its floor — so on a
narrow bar the strip degrades rather than the window list overwriting the charts.

### The disk warning

The `disk` meter charts `/`, because a chart has room for one number. The **warning** reads every
writable filesystem, so it reaches a separate `/home` or the stick somebody is copying onto.

`/proc/mounts` and `statvfs`, on the meter's ten-second cadence — one `statvfs` per mount, and one
on a network mount can block. `kdos-mountd` cannot answer this: it is wheel-gated, its reply
carries no free-space field, and it lists the media that are *not* mounted, which is the complement
of the set that can be full.

- **Pseudo-filesystems are skipped.** A tmpfs is sized from RAM, which the memory meter already
  charts, and calling one "disk almost full" names the wrong resource.
- **Read-only mounts are skipped.** A squashfs is 100% full by construction and a warning nobody
  can act on is noise.
- **Deduplicated by the source device.** A btrfs subvolume and a bind mount are further names for
  one filesystem; without this a single full disk warns three times.
- **Full means full FOR THIS USER** — `f_bavail`, which excludes the blocks a filesystem reserves
  for root. `df` on this image is toybox's and reports the reserve as available, so on an `ext4`
  whose reserve is intact `df` says 92% where the panel says 100%. The panel's number is the one
  that matters: those blocks are not yours to write.
- **A fixture root gets no reading at all.** `statvfs` cannot be pointed at a recorded machine, so
  `KDOS_PANEL_ROOT` suppresses the walk rather than measuring the machine running the dump.

At 90% a mark appears **one column inside the clock's own segment** — an exclamation mark, warning
coloured, error coloured past 95%. A letter and not a glyph slot, because nothing in the tiers is
a warning sign and a missing glyph draws a box. The clock is the one landmark on the bar that
never moves, and the warning is about the machine rather than about whichever applet is next to it.

**One notification per step past 90**, so 90, 95 and 100 speak and nothing between them does. The
step each mountpoint has already been warned about is latched in
`~/.local/state/kdos/diskwarn` — **on disk, not in memory**, or a disk that stays full warns again
at every login. A step that falls is recorded too, so emptying the disk and filling it again warns
again.

The chevron's popup carries the row whatever `overflow =` says, since the mark has no widget of its
own and cannot say which filesystem or by how much. Opening it runs `ncdu -x` **on that
mountpoint**: the question a full disk asks is where the space went, which is a recursive sum no
listing shows, and `-x` keeps the scan off every other filesystem.

### The overflow chevron

Widgets that only *sometimes* have something to say — the stutter chip, the restart mark, the
clipboard depth — appeared and vanished, changing the wing's width by several columns each time
and sliding every chart. So they live behind **one chevron of fixed width, drawn whether or not
anything is in it**. `overflow =` in `panel.conf` names them, using the same names `right =` uses.

The chevron is **two columns and no gap**, like the tray items beside it — a full applet tile would
be four, and four columns is the whole network chart on an eighty-column bar. It carries **no
count**: a digit is one cell wide in a two-cell box and would sit half a cell off. Colour says
whether anything is there and whether it wants attention; the tooltip names the first items; the
popup has the list.

It **returns one column short of its own left edge**, because the widget to its left draws a
separator at that column — returning otherwise puts the rule through the chevron's left cell and
the two-cell picture comes up as its own right half.

### The tray

A full StatusNotifierItem host: KDOS is the watcher and the host, because nothing else here is.
That matters more here than elsewhere, since **a boxed application that minimises to a tray which
does not exist has minimised to nowhere**.

An item is **one cell**: the first letter of its identifier, coloured by its status — `KT_MID` for
passive, the text colour for active, the accent and reversed for needs-attention. `KT_MID` and not
`KT_DIM`, because the fallback is a **letter** and `dim` is the palette's fill: at 1.17:1 against
the bar, the cell reads as empty. The identifier
rather than the icon name, because a letter from a name a human chose beats a letter from a theme
lookup that will never happen on a character grid.

Left activates, middle secondary-activates, right opens the menu — and **which menu depends on
what the item published**. An item with a `com.canonical.dbusmenu` path gets its tree drawn here by
`kdos-traymenu`; one with none gets `ContextMenu`, which is what an application with a menu window
of its own answers. An item that also sets `ItemIsMenu` has no useful Activate at all — the spec's
answer to a click on one is "show the menu" — so for those the **left** button opens it too.

Three rules, each a defect that was already there:

- **Nothing blocks the panel.** Property reads have a short ceiling and use one bulk call rather
  than several — several against a wedged application is over a second of dead panel — and every
  method call to an item is fire-and-forget.
- **Properties are never read from inside a bus callback.** A synchronous call there does not get
  its reply, because the bus library is already processing a message. Registration marks the item
  as needing properties and the next dispatch reads them.
- **The interface spelling is only recorded when the other one answered.** Two spellings are in
  use; flipping on the first failure sent every subsequent click to an interface the application
  did not implement — and a fire-and-forget click reports nothing, so it failed in total silence.

**When something else owns the watcher, the list is adopted** rather than contested — which is
what makes a dump taken beside a running panel show the same items rather than an empty tray.

**An item may resolve to a themed name first**, and exactly one class does: the input-method item
every session has publishes no usable icon name, so the identifier fallback found its own
full-colour artwork sitting between a phosphor network card and a phosphor speaker. Input-method
identifiers map to a keyboard icon before the application-artwork lookup runs. This is not a
licence to restyle other people's marks — it is the narrow case where the item is a *system*
function.

**The menu is its own process**, which is the rule every popup on this bar keeps: the panel's event
loop owns one surface and one cell buffer, and an application slow to answer `GetLayout` must not
take the bar with it. The panel hands it the item's bus name, the `Menu` object path and the item's
`Id` for the title — the menu object publishes no name of its own.

**What a row does not carry is a picture or a chord.** `icon-name` is a theme lookup and
`icon-data` a PNG per row, on a character grid; `shortcut` is the application's own chord and is not
this desktop's to press. A **toggle** is drawn, because a row that says "Pause" with no mark is a
row whose state is a guess. The tooltip says which button does what, because the cell cannot: an
item that declares `ItemIsMenu` and publishes no path to a tree has nothing anybody can do with it,
and that is the one thing a tray item cannot say for itself.

Because items can be hidden, the drawn order is **recorded** and the click reads that, rather than
deriving an index from the pointer's column.

### The privacy indicator

Which application is using your microphone or camera, by name. Every phone platform answers this
and no other Linux desktop does — not for difficulty, but because it needs four owners and nobody
owns all four. KDOS owns all four.

**Two sources, because there are two ways to record:**

| | Found by | Why not the other way |
|---|---|---|
| Microphone | An audio-server capture stream, counted **only while running** | The process list says the audio server holds the device, which is the non-answer every other desktop gives |
| Camera | A process holding a descriptor on a video device | Almost nothing takes the camera through the portal, so the audio server would report nothing at all |

**A stream that exists is not a stream that is recording.** An application that opened the
microphone and went idle must not light the lamp. The camera is counted the other way round on
purpose: an open descriptor on a camera *is* use, there being no other reason to hold one.

The name is the application's own, then its node name, then the process name. One application is
named and the rest are counted, because the panel is one row and three truncated names say less
than one name and a number.

Drawn in the secondary colour, with the camera additionally reversed — the one thing on the panel
that is a warning rather than a fact.

**The tooltip names the box; the bar does not.** On a machine where every fat application is a
container, "firefox is recording" leaves out the half that says *which* firefox — and there can
legitimately be two.

**The microphone lamp is a control.** Clicking mutes. An indicator that names the application
recording you and cannot stop it is one people learn to ignore.

### The stutter chip

The compositor reports every late frame on a socket. The panel holds it open, counts the drops of
the last ten seconds, and shows a cell when there have been at least three; clicking opens the
attribution.

**The cell goes away when the desktop stops missing frames**, which is the honest shape — an
indicator that is always up says nothing.

The read is non-blocking with a reconnect no oftener than every ten seconds, because the frame loop
is what this must never slow. There is no structured-data parser in this binary: the count field is
scanned for literally, and the line it scans is written three files away in the same repository.

### Tooltips

Half the panel is pictures with no words. Hovering one thing for about three quarters of a second
raises `kdos-tip`, which says what it is and what its three buttons do.

It is **a separate process**, because the toolkit has one cell buffer per process — the rule every
popup here keeps. It takes **no input at all**, or it would eat the click aimed at the thing it
describes. The panel shortens its own poll to the dwell deadline, exactly as it does for the
meters and the launch pulse.

**A window button's tooltip carries a live picture of the window**, which is the only way to tell
three terminals apart.

**A click spends the dwell.** Killing the tip on motion clears the dwell because the pointer has
moved to something else; a *click* moves nothing, so without spending it the tooltip goes straight
back up over whatever the click just opened.

### Configuration and reload

`~/.config/kdos/panel.conf`:

| Key | Is |
|---|---|
| `right` | The notification-area widgets, in order |
| `overflow` | Which of them live behind the chevron |
| `meters` | Which meters, in order of importance |
| `task_labels` | `auto`, `yes` or `no` — always, never, or the ladder |
| `tray_hide` | Tray item identifiers to hide |
| `start_label` | Whether the Start button carries its word |
| `icons` | Whether pictures are drawn at all |

An unknown widget name is **reported**, not ignored.

The panel re-reads this on the same signal a theme change sends, so changes take effect on the bar
that is on screen. The loader **restores every default before parsing**, because it runs again on
that signal and a reload that only ever *added* would leave a widget hidden after the line hiding
it was deleted.

## kdos-start

The Start menu, and the front door.

The left column is what you **use**: pinned above the rule, most-frequently-launched below it —
which needed a **usage count**, kept in the state directory and written atomically.

**All Programs opens the category list in place.** A cascade needs a surface per level and buys
nothing on a grid.

**Three columns from a hundred columns wide, two below it** — favourites and applications, then
places and files, then the system group. Under a hundred the system group folds behind one
`Settings ▸` row and becomes a page of its own with a `Back` at the top: fourteen system rows
scrolling inside a sixteen-row body is a list whose end nobody finds. **The menu asks for the wide
size and lays out from the width it was given.** A surface size is a request — the console session
clamps it to the work area — so asking for the narrow size and branching on the request would be a
three-column menu that exists only in a dump. **The fold moves the right column and nothing else**;
the two application submenus rebuild the left one, and the search walks the whole array whether a
row is folded away or not, so typing `bluetooth` finds it from either page. **Which system rows
stay outside the fold is `@toplevel` in `menu.conf`**, and nowhere in `start.c`: promoting
Bluetooth is a line in a file.

**The way back is a row**, because Escape and the right button are not discoverable and a
pointer-only user is exactly the first-time user of a Start menu. It is the first row of the left
column, and it never closes the menu. A search gets the same row as "clear search".

**The field looks like a field**: placeholder text, a block caret, sunken, lighting under the
pointer, and going to the accent the moment it is active. A control that looks identical before
and after being clicked is one people click again to find out whether it worked. The clear mark is
drawn only while there is something to clear.

**With nothing typed, that row explains the selection** — every desktop entry carries a comment and
this menu was throwing it away.

**The selected row is a plate, and an accent fill where there is no pixel layer.** The plate with
its accent left edge is the desktop's one selection, from the tone table the taskbar and the
cascading menu share; `kch_px_live()` is what says whether a recorded pixel op can reach a screen
at all — a backdrop installed, and not a console surface. On the console session and in every offscreen dump it cannot, so the row draws the accent
fill with its slots swapped instead — which is what the row's mark is already coloured for. Without
it a menu on the console has no visible cursor, on the one display where the cursor is the only
thing saying what Enter will do.

**Search reaches the fixed rows too.** Every one carries synonyms, so typing `wifi` finds the
network manager; the hits are appended under a rule. A search over the application index alone
answered `wifi` with an empty list on a machine whose network tool is three rows up the same menu.

**The routes are searched beside them.** `/etc/kdos/menu.conf`, merged under the user's copy, is
`route = argv` — a name a script can hold, which a chord and a menu row are not. They have no column
of their own: their whole existence is a name to search for, so a search is where they appear.
`--route NAME` opens the menu with the name already in the field, which is what `kdos menu summon`
passes and the one code path that finds a route.

**Applications on the medium are listed** under their category with a medium icon, and under
`INSTALL FROM THE MEDIUM` in a search. A row is *open this*: the pack is installed if it is not,
and the application opens. Read from the medium's own index rather than over a socket, because the
search runs on every keystroke.

**`[box]` marks a boxed application**, because the first launch of one costs a container start and
that is something a person is entitled to know before clicking. The mark comes from the **shared
index**, so the Start menu and the launcher cannot disagree about it.

**Pinning is here**, at the right edge of a row. Before that the only writer was the *window* menu
— so an application had to be started before it could be pinned. The mark is checked before the
row's own action, or clicking it would launch the application and close the menu under the hand.

**Suspend and Restart are ordinary rows**, and the footer carries a row of power and session
buttons beside the search field — lock, restart, log out and shut down among them. Every power
verb is reachable with a pointer, without knowing a key or a right-click.

The category you were last in is preselected — it does not *open*, which costs nobody a keystroke
and saves one for somebody who lives in Graphics.

**Two rows change on the console desktop**, and both because a Wayland client's surface is pixels
and that desktop is a grid of characters. Terminal starts `kdos-term` — a cell surface, so it opens
as a window there — instead of `foot`, and a **Desktop** row appears that starts the full graphical
session on a terminal of its own. The Desktop row is not built at all under the compositor: you are
already in it.

The same rule runs one level down. **Launching a graphical application from the console hands the
argv to the session and the session decides how it is held.** By default it starts one `kdos-cage
--embed` for the application and every toplevel that application maps becomes a window on the grid;
a box profile carrying `display = vt` gets a terminal of its own instead, and `kcon_run` answers
with which of the two happened. A `Terminal=true` entry takes neither: it becomes a `kdos-term`
window.

**Every launch surface in this binary calls `sh_launch()`** — the Start menu, the palette, the
desktop's icons, `kdos-find`, *Open With*, the run box, the panel's quick-launch row, its taskbar
chips and `kdos-menu`'s rows — and the one path is what makes them agree about three things a
surface splitting the line for itself gets wrong:

| The shared path | A surface that splits its own line |
|---|---|
| `kxdg_exec_split()` reads the line's quoting | `Exec=foot --title="Install KDOS" -- sudo kinstall` reaches `foot` as `--title="Install` and a stray `KDOS"`, so the installer's own icon opens a terminal that exits |
| Field codes are spent on the documents the launch carries, wherever they sit inside a word | `--open=%f` is handed to the program with the `%f` still in it, and the program reports a missing file |
| `kcon_run()` gives a graphical program to the console session | The child has no display and nothing holding it, so a boxed application exits at once with nothing on the screen to say why |

`sh_launch()` lives in `apps.c`; `launch.h` carries the rule and is the header a new surface
includes.

**The line that reaches it is the line the entry wrote, field codes and all.** The application
index and the desktop's icons keep `Exec=` verbatim, because the shared path reads those codes
twice: it **spends** them on the documents the launch carries, and it takes a line with **none** as
the kind that wants its documents appended — which is the only way `Exec=xterm` can be handed a
file. A surface that deleted the codes as it read the entry makes every line look like that second
kind, so `--open=%f` runs as `--open=` with the path as a word of its own and `%u` takes four
documents where the entry asked for one. Deleting them — `sh_strip_field_codes()` — belongs to the
one reader that cannot spend one: the haystack a search is matched against, where a `%U` is two
more letters for a subsequence matcher to travel through, so `fu` would find every entry whose
`Exec` ends in one. It is a **copy** that is stripped; the launch reads the same buffer. With nothing to open, `kxdg_exec_split()` drops every code and leaves no empty argument
behind, which is why nothing has to be deleted first.

**A `verbatim` launch is a typed command line** — the run box, and *Open With*'s *Other
command…* row. A `%` somebody typed is a character the program must see, so no field code is spent
and a file travels as a trailing argument; the quoting is read either way, so `mpv "my film.mkv"`
is two arguments from the run box as much as from an entry.

**A surface that holds an ID rather than a line calls `sh_launch_id()`**, which resolves the entry
through the XDG data directories and hands the whole of it over. The panel's quick-launch row, its
taskbar chips' *New window* and `kdos-menu`'s window menu are the callers: none of the three builds
the application index, which walks every directory on the machine, and a row that read only `Exec`
would start a `Terminal=true` application with no terminal round it and put a KDOS surface in a
cage. **The row is resolved twice on purpose** — once when it is drawn, for the label, and again
when it is clicked. A row is drawn far more often than it is clicked and a package upgrade rewrites
the entry between the two, so the copy that decides how something starts is read when it starts.

**A row whose command this tree wrote is marked `host` and is never handed to the session.**
`kdos-menu`'s System column is the case: `kdos-power suspend` in a cage would be a wlroots
compositor started to run a one-line verb, and Log Out's `pkill` on the session would leave that
cage outliving what it killed. It is set only by a surface whose rows are constants in its own
source — never from an entry, where the entry's own keys are the only thing that may decide.

**Nothing here names a terminal emulator.** `sh_term()` answers with `kdos-term` when `$KDOS_CON` is
set and `foot` otherwise, and every place that opens one — the root menu's rows, Places, Open
Terminal Here, the manual-page link, the CPU tile, Open With, the run box's *In Terminal*, the
launcher, the desktop icons and the key card — asks it. Both emulators take `-e CMD` and `-D DIR`
with the same meaning, so naming the program is the whole of the difference. A call site that spells
`"foot"` instead is a row that does nothing on the console desktop, where there is no compositor for
a Wayland client to be under.

**A terminal running somebody else's program is given that program's name.** The compositor matches
a window to its desktop entry by app-id, so a `Terminal=true` entry started as a bare `foot -e btop`
is a taskbar row called foot, wearing foot's icon, however many are open. `sh_term_argv_in()` writes
the emulator, the identity and the `-e` together: `--app-id` for foot, `--title` for `kdos-term`,
taken from the first word of the entry's `Exec`. **The flag belongs to the emulator, not to the
**`X-KDOS-Float` and `X-KDOS-Size` say how a terminal entry's window should open**, and they reach
it the same way `X-KDOS-Term` does: through the `kdos-term` argv this builds, as `--float` and
`--size COLSxROWS`. `foot` has neither flag and a window under the compositor is the compositor's to
place, so they are emitted only for `kdos-term`. **A hint is dropped where there is no room** in the
caller's argv — thirteen callers size their own, and a window that opens the ordinary way is better
than a terminal that does not open because its wrapper would not fit. Both keys are read here and in
`kdos-desk`, which reads the entry through the same call: a key read in one and not the other is one
entry with two answers.

**`X-KDOS-Cells=true` is what keeps a KDOS surface out of a cage on the console.** The session wraps
every non-terminal program it is handed in a kiosk compositor, because a Wayland client's surface is
pixels and that desktop composites cells; a program that attaches to the session *itself* needs none
of it. Without the key `kdos-res` is a whole wlroots compositor holding a grid of text the session
was already drawing, and `kdos-term` is a terminal emulator inside a kiosk inside the console. Only
an entry can answer it — the session is handed an argument vector, and a program's name says nothing
about what it will draw — and an entry that does not carry it is treated as a graphical application,
which is the safe direction.

**All seven launch keys are read once, by `kxdg_launch_read()`.** The application index, the
desktop's icons, the *Open With* chooser and `kdos-appbox open` each used to carry a private copy of
the list, and a key added to one of them was a row that behaved differently depending on which
surface it was clicked from. `NoDisplay` and `Hidden` are not among them: they say whether an entry
belongs in a **menu**, which is a question for whoever is drawing one, and the MIME route opens a
`NoDisplay` entry on purpose.

desktop** — an entry may ask for `kdos-term` with `X-KDOS-Term` while the compositor is up, and
choosing the flag by which session is running would hand it a `--app-id` it does not know. The two rows that open a terminal *as itself* —
the menu's Terminal, Open Terminal Here — pass no identity, because there the emulator is the
application.

![kdos-start](../../screenshots/start-menu.png)


## kdos-launcher

Full-screen search over the same application index. `Super+D`.

## kdos-menu

The root menu, the System menu, and the window menu.

`menu.xml` deliberately **lists no applications**: the compositor's built-in default is a
compositor's menu rather than a desktop's, and an application menu built at compositor startup
would be the one that went stale. Applications, Places and System are this program, reading the
same entries the launcher and the panel do.

The **taskbar's window menu** carries Restore, Minimize, Maximize/Restore Down, Fullscreen and
Close, and under them the verbs for the whole group — Minimize all, **Restore all (n)**, Close all — over the
window titles. It reads the window's own state, so Maximize says *Restore Down* when the window is
maximised rather than being a toggle whose direction nobody can see, and **Restore all** is drawn
only when some of the group is minimised, counting those: a row that is always there and does
nothing most of the time teaches people to stop reading the menu.

**It is a taskbar row's menu, not a frame's, and the two are different lists.** This one is per
application and carries the verbs for a whole group of windows; the console frame's own menu —
[`kdos-con`](kdos-con.md), on `Alt+Space` or a right press on a title row — is per window and
carries every verb that frame has, including lower, the scratchpad mark and send-to-workspace,
which no taskbar row offers.

**Both window menus wait for the app's own windows before deciding they have none.** A compositor
announces its toplevels inside the connect call; the console returns from it without reading a
byte, so a list read the instant the connect returns is empty there whatever is on screen. The menu
reads until a window carrying the app_id it was given arrives, or half a second passes — waiting
for *any* window would end the wait on somebody else's row and leave the menu with nothing to draw.
Without the wait every window menu the taskbar opens on the console exits before it draws, which
takes away every route back from a minimise that lets a person pick the window.

**The wait works because the session re-sends the list to a shell that attaches**, and that is the
condition it depends on: `kdos-menu` connects with `manage` set, so the session counts it as a
shell surface, and [kdos-con](kdos-con.md) re-sends every window when that count *rises*. A menu
that starts in the same turn another shell surface goes away sees no rise, so no re-send: it waits
its half second, prints `kdos-menu: no windows for <app_id>` and exits without drawing, and the
next click on the chip — a rise again — opens normally. That message and *this display server does
not offer a window list* are different answers on purpose: the second is a server with no window
list at all, asked of the display backend rather than of a count, because a count of zero is also
an ordinary empty desktop.

**Move and Size are deliberately absent**: the protocol the panel uses has no request for either,
and a menu row that did nothing would be worse than the title-bar drag that is the move.

The menu is titled with the box qualification when there is one, so it says the same words the
taskbar button did.

## kdos-desk

The desktop, its icons, and its own context menu.

**It claims the whole surface.** Claiming only the cells its icons occupy let a click on bare
wallpaper reach the compositor's root menu — at the cost of the desktop having no menu of its own,
so creating a folder was reachable only by right-clicking an existing icon, and on a fresh login
the only icons are two pinned places.

So the desktop answers its own wallpaper: New Folder, New File, Sort Icons, Refresh, plus
Applications, Change Wallpaper, Display Settings and Settings — everything the compositor's root
menu offered, because dropping the claim without replacing what it fed would have been the
regression. `Super+Space` still opens the compositor's own menu.

**And the verbs that act on the whole desktop**: Tile Windows, Cascade Windows, Show Desktop, Window
List, Restore All, Screenshot and Lock Screen. Those were bound to chords and to nothing else, so somebody using a mouse had
no way to tile their windows at all — and the desktop's own right press is where a root menu has
always been. The rows do not DO any of it: they ask the session by NAME through
`kdisp_session_action()`, because the windows are the session's and a surface that arranged them
itself would be a second window model. The name is `keys.conf`'s own, so the row and the chord reach
the same code. **It is a management request** and this surface therefore asks for `manage` — it is
the session's own chrome, started beside the panel by the session script, which is the line that
privilege is drawn on.

**Under the compositor the same rows take a different road, and four of them are not drawn at all.**
Show Desktop goes to `ToggleShowDesktop` on the command socket; Lock Screen and Screenshot are
programs there and are spawned, using the command `rc.xml` binds to `Print` so the row and the key
take one screenshot in one way. Tiling, cascading, the window list and restore-all are the console
session's own arithmetic — labwc has no action for any of them and no chord runs them either, which
`selftest.sh`'s one-sided chord table already records — so those rows are **hidden** there rather
than drawn and inert. A menu row that does nothing is a lie about what the machine can do, which is
the one thing this menu exists not to be.

**The menu is two halves.** The file verbs come from `libkxdg`'s table — the same one `kdos-pick`
and `mc`'s `F2` read, so a verb appears on all three at once — then a rule, then the rows above,
which are the ones only a desktop can answer. The rule is drawn only when both halves have
something.

On bare wallpaper the shared half is masked to the verbs that mean *here*: Open Terminal Here, Find
Here, Add to Places, Git Status Here. Open, Share and Move to Trash read as acting on **the thing**,
and there is none — a Move to Trash on the wallpaper would act on `~/Desktop` itself.

**Two shared rows keep a local action, and the row stays the table's.** *Open* goes through the
desktop's own opener, because a `.desktop` icon is an application to run rather than a file to open
and the Trash icon is a place; handing either to the MIME chain would make the menu row mean
something different from `Enter` on the same icon. **An icon that is a `.desktop` file runs through
`sh_launch()`** with the `Terminal`, `X-KDOS-Term`, `X-KDOS-Float` and `X-KDOS-Size` keys this
surface parsed — the same call the Start menu makes, so an icon and a menu row for one entry cannot
open two different windows, and a boxed application dropped on the desktop reaches the session
rather than being forked beside it. Everything that is not a `.desktop` icon goes to
`kdos-appbox open`. *Move to Trash* asks first and refuses the two
pinned places — `kdos trash <file>` confirms nothing, which is right for a prompt and for `mc` and
wrong for the row sitting beside `Delete` on this surface.

The context menu is drawn **into its own grid**, since this surface owns the screen and a popup
here is not a second surface. Its local rows carry a **scope** — icon, wallpaper, or both — because
two menus would be two places for New Folder to drift.

**Nothing is selected until something selects it.** The desktop opens with no highlight, `Escape`
puts the highlight down from any icon — that is what the `Deselect` rung on the Esc ladder means —
and a press on bare wallpaper does the same, because the menu that opens there acts on the folder
and a highlight left on a file names something none of its rows touches. An arrow, `Tab` or
`Shift+Tab` on a desktop with nothing selected selects the first icon and steps no further: a
keystroke that reached for the grid and stepped inside it lands on an icon nobody aimed at. The
ends of the grid stop rather than wrap — a grid is a surface, and a jump from the last icon to the
first is the width of the screen for a key that means one cell. `Home` and `End` name a cell rather
than a step, so `End` with nothing selected means the last icon.

**The hint row is drawn only while the desktop holds the keyboard.** This is a background layer
with on-demand keyboard: the console gives it the focus on a press on the icon layer and takes it
back on the next press on a window, so between boot and the first click the arrows, `Enter`, `Del`
and `Shift+F10` are not answered and are therefore not named. `Enter open` and `Del trash` are
named only where there is a selection to act on, and `Arrows select` takes their place where there
is not.

`~/Desktop` is created if it is missing.

## kdos-pick

The file chooser, and — with a browse flag — the file **manager**.

This is the dialog **every boxed application reaches through the portal**, so it is the one surface
on the system that other people's software puts in front of you.

As a browser, opening a file hands it to the system's open resolution, and **the dialog stays up**:
a browser that closed after one file would be a chooser wearing the wrong name.

**The preview pane parses P6 and nothing else**, and everything else becomes one: `kdos thumb --ppm`
owns the decoders and the helper forks and hands back a small picture. That is what lets the chooser
show a photograph, a PDF's first page or a frame of a video without linking an image library into a
dialog that has to build on a bare host. The fork is synchronous and sits in the idle slot — where
the twenty-megabyte read it replaced already was — so a slow helper stalls the dialog for as long as
it runs.

`Shift+F10` and the right button on a row open the **file verbs** — the same table `libkxdg` gives
the desktop and `mc`'s `F2`, so the same file offers the same things wherever you meet it. A verb
whose program is not on the machine is not offered, which is why the list is shorter than the table.
On the empty space below the list the right button still means *up*, which is where a right click in
a file list has gone since Norton Commander.

**Places is a LIST on `Ctrl+P`, not a fourth column, and the width is the reason.** The dialog is
sixty-four columns and already spends its right-hand one on the preview; a third column leaves a
file's name about thirty cells, and a chooser that cannot show a name is not a chooser. The list
opens over the file list as the outer of the dialog's two Esc rungs, with the line editor over it.

**The portal's dialog is this dialog, at this width.** Widening it for boxed applications was
considered and refused: the smallest screen this desktop is drawn for is eighty columns, a sidebar
that could be read is about sixteen more than sixty-four, and a chooser needing all eighty would
have no frame, no ground and nowhere for the bar. Two dialogs at two widths would also be two
layouts and two sets of reference frames. See
[Decisions](../01-philosophy/decisions.md#one-file-chooser-at-one-width-not-a-wider-one-for-the-portal).
It carries Home, the user directories that exist and `~/.config/kdos/places`, then the directories
a shell has actually been in from `zoxide` — read when the list is asked for rather than on every
frame, because that last part is a fork.

The hint row does not name `Shift+F10`, and that is the width: this dialog's row is thirty-eight
cells and a fourth hint would take the `Esc` one with it. The menu is the one verb here with a
**pointer** path, and this is the dialog every boxed application's Open reaches — the person in
front of it is usually holding a mouse.

![kdos-pick — the file dialog every boxed application reaches through the portal](../../screenshots/pick.png)


## kdos-trash

What was deleted, when, and where it came from — one row per item, newest first, with `Enter`
putting a row back where it was. The Trash icon on the desktop opens it.

**Put back is the point.** A trash without it is a slower delete: the desktop already moves a file
in and `kdos trash` already lists what is there, but the way *back* was a command line and a name
nobody had written down.

**It calls `kb_trash_*` and nothing else.** The specification — the escaping, the `.trashinfo`
record, the unique-name walk, the refusal to overwrite whatever is already at the origin — is one
implementation in `libkbase`, and a surface reimplementing any of it would be a second answer to
where a deleted file lives. The three failures the library distinguishes are said in its own words:
a record that cannot be parsed and a file that is already back are different problems, and a person
can act on the difference.

**The Trash icon does not open a file manager.** Its path is a directory, and a browser on it shows
the escaped names in `files/` with no origin and no deletion date; the record carrying both is in
`info/` beside it.

**A destructive row asks first, and the question is a declared `Esc` rung** rather than a flag — so
`Escape` while it is up answers *no* and leaves the list exactly as it was, which is what `Escape`
means on every other surface here.

**A folder's size is named, not measured.** `bytes` in a record is the directory inode, not a
recursive total, so the column reads `folder`: a number that is wrong is worse than one that is
missing.


## kdos-pix

One picture, fit to the window, with the folder it is in as the album. `+` and `-` zoom, `0` is
fit again, `Space` and `Backspace` step, the arrows pan.

**The folder is the album.** Opening one picture opens the sorted list of every picture beside it,
so `Space` is the next photograph rather than an error. `kdos-peek` deliberately does not do this:
a quick look is about the file somebody named.

**Zoom is a source rectangle, not a scaled sprite.** The window keeps a factor and a centre, and
what is registered is the crop those describe scaled to the pane — so zooming in reads *more* of
the original's pixels rather than enlarging the ones already on the screen. That is the difference
between a viewer and a magnifying glass over a thumbnail. It stops at 800%, where a cell is a
colour, and at 5%, where the picture is smaller than the border around it.

**Fit never enlarges.** A 32-pixel icon opened here is 32 pixels; `+` is how a person asks for it
bigger, and a viewer that guessed would show every icon on the machine as a blur.

**A pan is a tenth of what is on the screen**, not a fixed number of pixels, so one press moves the
same visible distance at every zoom.

**The folder is listed by extension, not by magic.** It may hold thousands of files and opening
each one to sniff it would be the slowest part of starting a viewer. A named file that is not in
the list — an extension nothing here reads — still shows, as a list of one.

**A new picture is shown whole.** Carrying the previous one's zoom would put somebody at 400% in
the corner of a photograph they have not seen yet.

**It is the console's handler for the three types `libkimg` decodes** — PNG, JPEG and WebP — and
`timg` keeps GIF, BMP and TIFF, which it cannot open. A row for a type the viewer refuses would be
a window that opens and says it cannot read the file. Under the compositor the boxed viewer keeps
them: it has real pixels at the screen's own resolution.

The decode, the crop, the scale, the tiles and the draw are `picture.c`'s, shared with `kdos-peek`.

## kdos-find

One question, and the four places an answer could be. `Super+Shift+F` on both desktops, *Find Here*
in the file verbs, and `f` on `mc`'s `F2`.

| Source | From | When |
|---|---|---|
| Applications | The launcher's own index, `sh_apps_match()` | Every keystroke |
| Recent | `libkxdg`'s recently-used list | Every keystroke |
| Names | `fd` | When the question stops changing |
| Contents | `rga` | Only after `Ctrl+G` |

**Each source is somebody else's answer.** A walker written here would disagree with `fd` about
hidden files, ignore rules and symlinks, and a content search written here would be `rga` without
the archive and document readers that are the point of it.

**The two forks stream.** A search over a home directory is seconds, and a surface that waited for
it would be a window that cannot be closed while doing the one thing it is for. The child writes
into a pipe read in the poll loop; `Escape` kills it. The same shape `kdos-status` uses.

**Contents are opt-in.** Names come back in milliseconds and are what somebody usually means; a
content search reads every file under the directory, and starting one per keystroke is a machine
that never stops working. Once asked for, it follows every later question.

**Names are searched literally, not as a regex** — `--fixed-strings`. Somebody typing `report.c`
means the dot, and a pattern that swallowed it would match `reportxc`.

**A result opens through `kdos-appbox open`**, the one resolution the desktop, the chooser and `mc`
all use. An application row is resolved to its entry again at the moment it is chosen, because a
boxed app's `Exec` reads `kdos-appbox -b <pack> run <command>` — `kdos-appbox run <command>` for an
app belonging to no pack — and its id is not that command: handing the id to `run` starts nothing at
all. A consumer that recognises the line by a fixed prefix sees only the packless half; the binary
and the verb are what identify it. **The entry is then handed to `sh_apps_launch()`**, the
launcher's own call, so a row found here and the same row in the Start menu open the same window —
and so that a boxed application reaches the session that gives it a display.

**The root is the directory the verb named, else home.** A search with no root is a search of the
filesystem, which is not what *Find Here* means and not what a chord with no context should start.

## kdos-rec

Pick an input, record through `sox`, watch the level, keep the file. *Recorder* in the Start menu,
*Recording…* under Settings' Hardware page, and `F1` opens `/usr/share/kdos/doc/rec.txt`.

**A card is not a microphone.** The list is `kpr_sound_pcms()` — `/proc/asound/pcm`, one line per
PCM — filtered to those carrying a capture stream. An HDMI codec is a card with four playback PCMs
and no capture stream at all, so a picker built from `/proc/asound/cards` offers a monitor's audio
output as an input, and the recorder that accepts it fails to open with a message about the device
rather than about the choice.

**`Default` is the first row and always present.** It is the one that works while the session's
PipeWire holds the card, through `pipewire-alsa`; a `hw:C,D` row names one PCM directly, which is
what answers on a console with no session running. A live session can therefore refuse a `hw:` row
with `EBUSY` while `Default` records — the list is the kernel's PCMs, not PipeWire's graph, and the
surface shows `sox`'s own message rather than an empty file.

**The input is named on argv**, `-t alsa hw:C,D`, never left to `rec` or `-d`: sox's default-device
probe opens a card for *playback*, so it skips a card that has no DAC.

```
sox -q --input-buffer 3200 -t alsa hw:1,0 -t raw -e signed -b 16 -c 1 -r 16000 -
```

`sox` does the part only `sox` can do — open the device at whatever rate, format and channel count
it has, and resample. **16 kHz mono s16 is not a preference**: it is exactly what `whisper-cli`
requires, so the file this writes is the file the transcriber reads with no second conversion.
`kdos-rec` writes the 44-byte header itself and **rewrites both length fields every tenth of a
second** — a header written only at the end leaves a file no player will open whenever the machine
goes away mid-recording, which in the rig is every run. Files land in `~/Recordings`, made on
demand.

**The level is the recorder's own peak over the bytes it wrote**, not `sox --show-progress`, which
is fourteen text steps two decibels apart on a throttled repaint. One entry per **tick**, never per
read: a read's size is the pipe's, not time's, and a sparkline whose column spacing is the
scheduler's is a chart of the scheduler. The ring holds `max|s| / 32768` — a fraction — and
`ktui_sparkline` is given `vmax = 1.0`, **pinned, not fitted**: an autoscaled meter paints a
microphone's noise floor as a solid bar, which is the one reading a level meter must never give.

dBFS is a table of integer peak thresholds at half-decibel midpoints, so the printed number is the
nearest decibel and nothing in this tree gains a maths library for one logarithm. **Clipping is the
word `CLIP` and not a red chart**: `ktui_sparkline` takes no foreground slot, and adding one to a
function `kdosbuild` and `kdos-res` also call is a wider change than this surface earns.

**Recording refuses to start while the input is muted**, and says so on the row. A recorder that
quietly records a muted input is the worst thing this surface can do.

**Transcribe is enabled by the model gate alone.** Whether `whisper-cli` is on `$PATH` is asked
only when a child is started: `$PATH` is not frozen in a reference frame, so a button that changed
shade with the host's packages could not have one. The three model locations, `$KDOS_WHISPER_MODEL`'s
exclusive semantics and the four-byte magic test are in
[configuration](../06-reference/configuration.md#speech-to-text-models).

**The words have never been read back on this tree.** No model ships and the desktop cannot fetch
one, so what is proved about transcription is the gate, the argv, the spawn and the exit status.

**And this surface transcribes a FILE.** `whisper-stream` on the image transcribes a live
microphone and is a terminal program with no desktop verb in front of it: it opens SDL's audio
device, which raises no window, so it runs on the console with nothing above it. Both want the same
model.

## kdos-palette

`Super+Space`. One input row, one result list, and six sources searched at once: **windows,
applications, routes, settings pages, files and chords.** `kdos-launcher` is this program showing
applications only — one binary and one flag, because two search programs meant two matchers and two
ideas of ranking.

**The heading order is fixed and is not the ranking.** Windows first, because the cheapest thing to
want is the window you already had; then applications, routes, settings, files, chords. The score
orders the rows *inside* a heading, so a very good file match never climbs above the window you were
just looking at. **A heading with no hits is not drawn** — a column of empty category names is a list
that looks broken — and each kind is capped, so one source cannot crowd out the rest.

**Files only after three characters.** Every other source is a table already in memory; the file
source forks `fd`, and doing that per keystroke is a directory walk per keystroke. Its lines arrive
over several frames and are merged into the list as they come.

**A chord row shows the chord and does not press it.** Enter runs a chord's program where the chord
runs one, and otherwise says which keys to press. **A client cannot fire the session's own actions
and must not be able to**: synthesised input over the surface socket is a way to drive somebody's
desktop for anything that can reach it, which is the rule the protocol is shaped around. The row is
still worth having — "what was the chord for tiling" is asked far more often than a chord is
rebound.

**A row is named by what it does, and on the two desktops that is two different fields.** The
console's chord action *is* the verb — `tile`, `launcher` — while the compositor's is labwc's, so
nearly every row there would be the word `Execute`; those are named by the command they run instead.
A list of forty rows all called Execute is a list nobody can search.

**`Super+Space` was the Start menu on the console and labwc's root menu under the compositor.** Both
now open this. A pointer wants rows and a keyboard wants a search, so the taskbar's Start button and
the right button still open the menu, and `Super+F10` still reaches it from the keyboard. The
first-run tour's second step moved with the chord — the tour names the action, and `selftest.sh`
fails the build when a step names something nothing binds, which is how the move was caught on both
desktops.

**Two of the six sources cannot appear in a golden**, and that is stated rather than left to be
noticed: the dump harness stubs the window list to zero, and the file source forks `fd`, which a
dump stops before drawing — a golden of somebody's home directory is a golden of whoever ran the
suite.

## kdos-peek

What is in a file, without starting the application that owns it. *Peek* in the file verbs, `k` on
`mc`'s `F2`, and the handler for the five document types nothing else on this image opens.

**Four kinds and one decision**, taken in this order because the cheap and certain tests come first:

| Test | Kind | Shown as |
|---|---|---|
| The magic bytes of a PNG, JPEG or WebP | Picture | The picture, tiled into the cell grid |
| A `.pdf`, `.epub`, `.cbz`, `.xps` or `.fb2` | Document | One page at a time, rendered by `mutool` |
| `libarchive` agrees to open it | Archive | The entries, name and size |
| No NUL in the first four kilobytes | Text | `less`, in a terminal — and this exits |

Anything left is refused **by name**, in the middle of the window, rather than shown as mojibake.

**A directory is refused, and that is a decision rather than a gap.** A file manager drawn inside a
viewer that a file manager opened is the circularity this desktop refused for tabs; `mc` shows
directories and this shows files. The refusal is taken from `stat` before any display is opened, so
a directory named over `ssh` fails on the argument rather than on the display, and the message names
where to open one — a program that exits silently reads as a broken one.

**Nothing here decodes a picture.** `libkimg` is the one place in KDOS that turns untrusted image
bytes into pixels, under a budget checked before any allocation, and a page from `mutool` arrives as
a PNG and goes through the same call. The scale and the cut into sprite tiles are `libkcell`'s one
implementation, shared with the terminal's inline pictures.

**The magic is sniffed here as well as inside `libkimg`**, because the decision is taken before the
file is read: a two-gigabyte video must not be loaded into memory to discover that it is not a PNG.

**A document is chosen by extension and rendered at the pane's pixel size.** `mutool draw -w -h`
with no `-r` fits the page inside that box and keeps its aspect, so a resize is a re-render rather
than a rescale of what was already drawn. The page count comes from one `mutool info` at open; when
it says nothing usable the title shows a page number without a total, because a total this program
guessed would be a number that is wrong rather than missing.

**`libarchive` opening the file is the test for whether it is an archive** — the format probe is the
same code that would read it, rather than a table of extensions that would disagree with it. The
listing stops at 4096 entries, because the entry count is the archive's choice.

**Text is the pager's.** A pager inside this window would be a second implementation of scrolling,
searching and line wrapping, and the one on the machine is better than the one this file would
grow. `--dump` never forks it: it draws what it would have done, the same split between measuring
and acting the panel keeps.

**On the console the picture's bytes are offered to `libkcon` explicitly**, after `kdisp_init` and
never before — the console backend clears its client state when it connects. A sprite the session
was never sent maps to −1 and its cells become spaces, so the failure is a blank pane rather than
the fallback codepoint. The cell size there is nominal for the same reason `kdos-term`'s is: a console surface has no
pixels of its own, and the display scales what arrives.

**Peek is not the default handler for a book.** `epy` is: a reader keeps a position, a table of
contents and a search, which is the difference between reading an epub and glancing at a page of
one. The Peek verb still shows any of the five, because it asks no handler table at all.

**A directory is refused.** A file manager inside a viewer that was opened from a file manager is a
circle; `mc` shows directories and this shows files.


## Notifications

`kdos-notifyd` owns the bus name; `kdos-notify` is the centre. The daemon owns the list and the
front end draws it — the same split the clipboard uses.

**Sending one is `Notify` on the session bus, and this tree has three callers of it — one per kind
of caller there is.** `kb_notify()` double-forks `gdbus` and is for a program with no bus
connection of its own, which is `kdos notify`, `kdos-term` and the console's terminal. The panel
sends on the connection its tray already holds, because opening a second one to say one sentence is
a second thing to keep alive. The compositor spawns `gdbus` itself, because it links neither
libkbase nor sd-bus. `kdos-notify` sends nothing at all: it is a viewer of what has already
arrived.

**A notification that expired is not a notification that was read.** Every toast joins the history
on the way out, whatever took it out — expiry, a click, or the sending application closing it —
because the ones nobody saw are exactly the ones the centre exists to answer for. That lands harder
here than elsewhere: a boxed application's notification is often the only thing that says the work
it was doing has finished.

A ring of recent entries, and a short connection per request on a socket:

| Verb | Does |
|---|---|
| `count` | How many, and how many unseen |
| `list` | The history |
| `seen` | Clear the unseen count |
| `open` | Activate an entry |
| `clear` | Empty the history |
| `dnd [on\|off\|toggle]` | Set do not disturb, and answer with the state as it then reads |

**Unseen is what the badge counts**, cleared by the centre being opened and by nothing else. A
count that cleared itself on a timer is a count nobody trusts.

**Do not disturb is only honest with a history behind it.** Silencing toasts without somewhere for
them to go would mean losing them; with the ring in place the notification is kept, the badge still
counts it, and the sending application cannot tell — the identifier is returned and the close
signal is still emitted, so nothing hangs waiting. **An urgent notification is shown anyway**: a do
not disturb that hid a battery-critical warning would be a switch nobody dares leave on.

**It is one flag, and it is the `dnd` toggle file.** The daemon keeps no copy of its own: a second
flag OR'd with the toggle is a state the centre's own button cannot clear, so Allow Toasts would
leave the toasts silenced and say it had not. The `dnd` verb writes the file and replies with what
the file then reads rather than with what it was asked for, so a state directory that cannot be
written leaves the button drawn the way things actually are. `kdos toggle dnd`, a chord and a
script all set the same switch, and it outlives the daemon.

**Hovering a toast holds its countdown.** A toast that disappears while it is being read has to be
read twice, and it cannot be. The remaining time is banked and restored on leave with a floor, so a
pointer merely crossing the corner never costs a notification. The border changes colour while it
is held, because a countdown that quietly stops is one nobody can tell has stopped — and **urgent
keeps its own colour**, since a warning that changed colour under the hand would be saying
something it does not mean.

The resume walks **every** held toast rather than one an index names, because the stack moves under
the pointer whenever a toast is dropped.

**A toast dismisses on click**, with the protocol's dismissed reason. The surface takes no
keyboard — a toast must never steal focus — so the pointer is the only way to make one go away
early.

## kdos-osd

Two surfaces sharing only the mixer helpers, because they are opposites.

The **bezel** is what the media keys raise: it takes no input at all, having once eaten every click
under it.

The **slider** is what clicking the volume applet opens: anchored, interactive, clickable along its
length — a control that can only be nudged in fixed steps is one people give up on — with mute as a
labelled button beside it.

**And it is dragged.** The button is remembered across events; the implicit grab means motion keeps
arriving after the pointer leaves the popup, so a hand that runs past the end of the bar lands at
the maximum rather than stopping wherever the surface did.

The level wears the same speaker icon the panel applet resolves, so the readout and the popup it
opens cannot show two different pictures of one number.

The mixer is **cached open** and re-read through the library's event call: a panel asks once a
second, and opening and loading the mixer per tick rebuilds it sixty times a minute.

## kdos-settings

**It opens on a grid of icons.** A sidebar of words is a fine way to move between pages once you
know what is on them and a poor way to find anything the first time. Escape steps **back to the
grid** rather than out of the program, so the unsaved-changes guard stays at the single exit.
A page flag still lands directly on a page, because an applet deep-linking into a page and then
making you pick it again would be a link that does half its job.

The tile icon names are checked against the **shipped icon set**, not taken from the naming
specification, and the blurbs are cut to what a tile holds at eighty columns.

**Its two panes each carry a caret and only one carries a plate.** The category column and the
field list are drawn through `ktui_sel_row()`, which fills the row of the pane holding the keyboard
and marks the other's with `►` alone. Before that both panes filled — the focused one in the accent
and the cold one in `KT_DIM`, which measures about 1.6:1 against the page and so said almost
nothing. A form of eight numbered rows was eight accent-coloured slider tracks and one lit
full-width bar following the pointer; there was no hierarchy in it to read.

**The scope tag and the value column lift off the muted colour when their row is selected.** `live`
and `login` are the answer to *did that do anything*, and a selected row is exactly when somebody
is asking — but muted on the selection fill measures 2.18:1 to 3.43:1 depending on the accent, so
left alone the right-hand half of the row would disappear at the moment it matters.
`ktui_sel_dim()` is that question asked in one place. `KT_DIM` survives in one role only, and off
the fill: an `(unset)` value, where it is carrying the distinction rather than trying to be read.

**And the recorder's scrollbar is a control.** `kch_scrollbar` takes an id precisely so the bar that
was DRAWN is the thing a press is measured against; that surface drew one beside its file list and
answered neither its press nor its drag — a bar people try to drag once and then stop trusting.

**The wheel scrolls every table.** Ten surfaces drew a `ktui_table`, answered a press on a row and
answered no detent — backup, connect, disks, firewall, print, timezone, update, users, contacts and
chars — so a person on the eleventh row of forty could see thirty of them and reach none. It is
`ktui_table_event` now: the wheel scrolls, a press moves the caret, a press on the row it is already
on picks, the right button is Back. The same rule `ktui_rows_event` gives a list, because ten copies
of it would be ten answers to what a press means.

**The volume is a slider, not a bar.** A progress bar drawn where a control belongs is a control the
pointer cannot find, and this is the one surface a person opens *because* they want to change the
number: press the track, drag it — the drag belongs to the press, so the pointer may leave the row —
or roll the wheel over it. A muted device draws at zero and says `mute` beside it; its real level is
still there and comes back with the mute, because a track showing a level while the machine is
silent would be the control disagreeing with the speaker.

**The greeter and the lock screen were the last two with no pointer at all.** On a machine with
three accounts, choosing which one to log in as was an arrow key and nothing else — on the one
screen a person reaches before they know anything about this desktop. A press picks an account and
steps the session, and the password is still typed, which is not a gap: there is no on-screen
keyboard here, and a field a pointer could fill would be a field filled by whatever else can reach
the pointer. **On the lock screen a press clears the field and that is all it may do**: the single
action there is typing a secret, and a button that unlocked would be a lock with a way past it that
is not the password.

**Eleven of these surfaces answered no pointer event at all**, and now all of them do: `kdos-style`,
`kdos-chars`, `kdos-calc`, `kdos-palette`, `kdos-trash`, `kdos-find`, `kdos-contacts`, `kdos-notes`,
`kdos-rec` and both viewers each ended their event loop with `if (ev.type != KT_EVT_KEY) continue;`.
The accent picker is the plainest case: it is what Settings' `Accent…` row opens, and somebody who
arrived there with a mouse could look at seven schemes and choose none of them. The rule is one
function's — `ktui_rows_event` — so a press means the same thing on all of them: move the caret,
pick the row it is already on, wheel to walk, right button for Back.

**Every value is a control, and the mouse alone can set all of them.** A number is a
`ktui_slider` — press the track, drag it, roll the wheel over it, or click an end cap for one step
— and a choice is a `ktui_dropdown`, which opens under the row and picks on a click. Both answer
the keyboard exactly as before. This was the surface's own worst failure and it was not a small
one: every knob on the machine was a printed string changed with `Left` and `Right`, so somebody
who arrived here with a pointer could select a row and do nothing else with it.

**A text value is `ktui_input`, the toolkit's own field.** It was a hand-rolled buffer that took
backspace and printable bytes and nothing else — no caret to move, no paste, and one backspace could
cut a UTF-8 character in half. The control is a frame control, so the key is handed to the DRAW
rather than spent in the loop; clicking inside the field places the caret and clicking away KEEPS
what was typed, because clicking away from a field is not how anybody means to discard it.

**The control answers the FIRST press, not the second.** A row is selected and its slider is set by
one gesture; a slider that needed the row selecting first would be two movements for one. The drag
belongs to the press that began it, so the pointer may leave the column and go on setting the
value, and the wheel turns the control it is over and scrolls the page everywhere else. **An open
dropdown owns the pointer and the keyboard while it is down**, because it is drawn over the rows
beneath it and a press tested against those rows would pick whatever the list is covering.

**The Appearance page does not list the accents.** Its `Accent…` row opens `kdos-style`, which
draws every scheme in its own colours and previews it live; a row of names beside it would be a
second way to choose one and the worse of the two.

**Every page is cut into sections.** A page of forty knobs read as one undifferentiated list, and
the store a row writes — the console's file, the compositor's, the panel's, the monitor's — is
exactly what a person needs to know before they change one. A section rule says it once for the
rows under it instead of every row's help line saying it again, and the caret **steps over** a rule
in whichever direction it was travelling: a heading that could be selected is a row where Enter,
Left and Right all do nothing, which is the shape of a control that is broken.

**It writes nine files, and the console session's is one of them.** `con.conf` was reachable from
this window by nothing at all — on the desktop that is the DEFAULT one. Most of that file is here
now: the appearance keys, the login keys, the idle trio and the saver, the seven role programs and
the terminal, the pointing-device block and the window-gesture block. The keys it does **not**
offer are the ones naming a program the session starts — `menu`, `launcher`, `lock`, `saver`, the
notice verbs — because a row that edits a command line is a row that can leave a chord opening
nothing.

Those rows say **login** and cannot say anything else: `kcon_conf_*` reads that file once, on the
first lookup, and holds the answer for the life of the session, so no signal is sent for them — one
that did nothing would be the program pretending otherwise. They are shown on both desktops and
apply to one, which every such row says in its own help rather than leaving the category to imply
it.

**It reads `/etc/kdos/con.conf` first and the home file over it**, which is the order the session
itself reads them in. `/etc/skel` ships no `~/.config/kdos-con/con.conf`, so a page loaded from the
home file alone showed this program's own defaults as though they were the machine's, and every row
read `*` the moment it was touched. What is written is still the home file; `/etc` stays the
administrator's.

**The Input page has controls.** It was two read-only notes. `kdos-view` opens the pointing devices
and applies `con.conf`'s pointer block to every device on the seat that accepts each key — speed,
natural scroll, tap-to-click, tap-drag, disable-while-typing, left-handed, middle emulation — so
these rows change what the hand feels. A key left out is not the same as a key set to `no`: left
out, libinput's own default stands. The **keyboard** is still notes at the end of the page, for the
reason this whole surface exists: the layout comes from `/etc/keymap` by way of `kdos-desktop` and
every keyboard knob the compositor has is `rc.xml`'s, and a row that wrote a file no program opens
is exactly the "change a thing, see nothing" this window promised not to be.

**The pending counter and the quit guard count every store.** Both asked `comp.conf` alone, so a
change to the panel's file, the monitor's or the console session's showed `0 pending` on the Apply
button and was discarded by a single Escape with the guard saying nothing — which is the one thing
that guard exists to stop.

**Nine categories:** Appearance, Panel, Desktop, Hardware, Session, Input, Apps, Boxes and
**System** — the machine itself. System opens the resource monitor, the power page, disks,
printers, accounts, the clock, the sync configuration, the firewall, updates and the backup
repository: ten surfaces this binary already is, which were reachable from their own chords and
from nowhere a person looking for a control centre would go. Only the service list is still
unwritten, and the category says so in a row rather than implying that the machine is not reachable
from here.

**`kdos settings [page]`** opens it from a prompt, and the page word is passed through without being
checked here: `kdos-settings` owns the list of page names, and a second copy of it in the command
would be a second list to keep in step — the failure being a page that exists and cannot be reached
from the command line.

![kdos-settings, which opens on a grid of labelled pictures rather than a sidebar of words](../../screenshots/settings.png)


**The Boxes page is the configuration half** of box management — the runtime half is `kdos-box` and
the monitor's Boxes page. Before it, every property of a box could only be set by knowing a
configuration file existed.

**It never writes a profile.** It reads them, and writes by **running** `kdos-box`, which is the
writer a person at a prompt reaches. That is what makes "a box created here is identical to one
created by hand with the same answers" true rather than approximately true, and it keeps the three
things `kdos-box` knows and this page must not re-derive: which keys the container engine can
enforce, which it cannot, and that a namespace change needs the box recreating.

Only the keys that **changed** are passed, because the writer rewrites the file from what it
loaded and passing everything would also rewrite the keys this page does not show.

**A box a launch created is always a row**, profile or not — an application pack gets a box at
first launch and nobody ever described it, so a page listing only profile files showed no boxes on
a machine with several running. Such a row reads *no profile yet* rather than showing defaults as
though somebody chose them.

The name is editable only while creating, since renaming is an operation with no verb. And the
network warning for a registry base is **a row, not a footnote**.

`kdos-settings` also writes `panel.conf`, which the panel re-reads on signal, `res.conf` and
`term.conf`, which `kdos-res` and `kdos-term` re-read on one — so a terminal already open changes
under the hand — and `launcher.conf` and the user's `menu.conf`, which get no signal because the
launcher and the Start menu are opened by their chords and read those files as they come up. In
`menu.conf` this page owns one `@` setting and no route at all: every other line is copied through
byte for byte, which is what makes writing into a route table safe. Each signal is sent with an **exact** name match:
`kdos-res` is a prefix of the setuid `kdos-resctl`, which handles no signals, and `kdos-comp` is a
prefix of the shell script that owns the whole graphical session.

## The device managers

`kdos-net`, `kdos-bt` and `kdos-audio` share their chrome; **`kdos-devices` does not**, and looks
it beside the other three — it has its own sections and a hint row rather than a header band,
group headings and a button bar. That is stated rather than implied.

Everything these drive has worked on this system since before there was a desktop. What was missing
was a surface: the alternative was a network tool in a terminal.

Three things every control panel of the classic lineage has and these did not:

- **A header band** — accent-filled, two rows, an icon and a **subject line** saying what its
  subject is doing right now. "Am I connected" is the question the window is opened to answer, and
  it was somewhere in a list.
- **Group headings**, so eight rows read as two groups.
- **Real buttons**, labelled with verbs, clickable, each **enabled from the selection** — a Connect
  that fails when pressed teaches people to stop trusting the row it was on.

**`kdos-devices` notices an application set on a stick.** A `.ktar` on a mounted removable
filesystem gets a row under `APPLICATION SETS ON A STICK`, and Enter imports it — offline, with
every pack verified where it mounts. That is the only route to software on a machine with no
network, and a stick somebody wrote on another machine is exactly what this surface exists to
notice. It reads one directory per *already mounted* filesystem rather than scanning the machine,
because the surface is waiting for a keystroke and a scan would not be fine on a panel tick.

**Anchored means popup; centred means window.** Each is both things and the difference is which
one asked: from the panel it is the bar's own popup, sized like one and dismissed by a click
elsewhere; typed by name or opened from the Start menu it is the application, centred, full size
and staying up — because somebody who went looking for the network tool will look at something else
in the middle of using it, and a pairing confirmation must not vanish because the pointer went to
the device on the desk. One flag decides both.

Per-manager:

- **`kdos-net`** talks to the network service over the system bus. **The list does not reorder
  under the pointer**: signal strength moves on its own, so it is sorted once per refresh and the
  selection follows a **kind-qualified key** rather than the row index — an SSID for an access
  point and an object path for a saved profile, because a VPN may be named the same as a network.
  The passphrase typed here is written into the profile as it is created; **every later question
  belongs to `kdos-netagent`**, because the service raises a secret request against its registered
  agents rather than against whichever program started the activation.

  **Saved VPN and WireGuard profiles are rows too**, at the left margin under the radios because
  they belong to no radio. Enter is a toggle rather than a join: the service refuses to re-activate
  something already active. A profile's TYPE is not a property of its connection object — only
  Unsaved, Flags and Filename are — so it comes from one `GetSettings` per path, cached, and a row
  appears the moment that answer lands. The window lists and toggles them; it does not create one.

  **`h` shares the machine's network over its own radio**, where the radio reports the access-point
  capability. That is `ipv4.method = shared`, which starts a DHCP and DNS server on the access
  point's interface and installs a NAT table of its own — so `/etc/nftables.conf` has to leave
  forwarding and those two ports open for the shared subnet, and it does. **A change made through
  `kdos-firewall` while a hotspot is up takes its NAT down**: applying a rule change re-runs the
  whole file, which begins by flushing the ruleset, and the service installs its table only at
  activation. The hotspot keeps its clients and stops routing.

  ![kdos-net: the header band says what the subject is doing now, and the buttons are enabled from the selection](../../screenshots/net.png)

- **`kdos-netagent`** is not a window that opens from the panel: it is started with the session,
  holds no display while idle, and raises one dialog when the network service asks it for a secret.
  Without it the service fails such an activation **in silence** — it never prompts on its own. The
  reply is **deferred** the way `kdos-bt`'s is; the box is a **toplevel** rather than a layer
  surface, because the compositor focuses a toplevel when it maps and focuses an on-demand layer
  surface only when it is pressed, and a passphrase field that swallowed the first keystrokes would
  be worse than none. It stores nothing, so a request without the interaction flag — the service
  polls its agents for saved secrets — is answered at once rather than with a dialog.
- **`kdos-bt`** registers a pairing **agent**, without which a keyboard cannot be paired at all:
  the service asks the agent to confirm a passkey and refuses the pairing when nobody answers. The
  confirmation is a **deferred reply** — the handler retains the message and returns without
  replying — because a handler that sat in its own loop would stop answering the service.
- **`kdos-devices`** enumerates cameras by device call rather than through a library, finds who is
  holding one by walking the process table, and previews a grabbed frame through the shape-matching
  character renderer. **Its scanner section is `scanimage -L` and not `libsane`**: linking the
  library would pull every backend's shared object and its configuration into the process to ask a
  question `scanimage` already answers, and the scanning is `scanimage` too, so the link buys
  nothing. The probe walks a USB bus and the network, so it runs once per refresh and never on a
  keystroke. **Opening a camera to preview it is using it**, so the privacy lamp lights for
  this program too and the descriptor is closed with the frame. It also fronts removable media. Its
  microphone list is `kpr_sound_pcms()` filtered to the PCMs that carry a **capture stream** —
  `kdos-rec` reads the same function, because two surfaces must not give two answers to what a
  microphone is, and a list built from `/proc/asound/cards` offers an HDMI codec as an input. Its
  list **scrolls**, through `ktui_table`: the sections are as long as the machine's devices, so a
  screen shorter than they are must still be able to reach the tail. The section captions are
  furniture — every verb here acts on a device, so the selection steps over them rather than
  landing where nothing would happen.
- **`kdos-disks`** is the surface for the `kdos-mountd` verbs that were reachable from nothing:
  `unlock`, `close`, `format` and `smart`. **Every privileged operation is a daemon verb and this
  program runs as the user** — it opens no block device, forks no `mkfs` and holds no capability;
  what it does is draw a list the daemon published and send back a row number. A disks window that
  needed root would be a setuid binary with a text editor's attack surface. **Partitioning is
  `cfdisk` in a terminal** and is not reimplemented: a partition editor is a program in its own
  right, `cfdisk` is on the image and is what somebody who partitions disks already knows, and it
  is pointed at the row's whole DISK — a partition editor aimed at `/dev/sdb1` opens the table
  inside a filesystem, which it reads as an empty disk and offers to write. **There is no `fsck`**:
  on a mounted volume it corrupts, on an unmounted one it takes minutes with no progress anybody
  can read, and a button that started one and could not be stopped would be the most dangerous
  control on this desktop — the honest place for it is a shell. Erase asks for the device's own
  name typed, which is the daemon's rule and not this surface's decoration.
- **`kdos-connect`** is the same shape for a folder on another machine: five fields — server,
  share, user, domain, password — handed to `kdos-mountd`'s `cifs` verb, and a list of what is
  mounted underneath. **The password is a second frame and never a token**, for the reason the
  passphrase in `kdos-disks` is: the request line is split on spaces. **The four names are not
  checked here, they are checked there** — `mount.cifs` builds its option string by concatenation
  and escapes nothing but the password, so the allowlist that refuses a comma has to be the
  daemon's, because a check in a surface is a check nothing else talking to the socket gets.
  **The form takes every key before the rung pool**, Escape excepted: a surface whose text field
  sat under the pool would close the window the first time somebody typed a letter the pool had a
  meaning for.
- **`kdos-print`** is `lpstat`, `lpinfo` and `lpadmin`, **not libcups and not IPP**. Those three are
  on the image, they are what the CUPS documentation tells a person to type, and they are the
  interface upstream keeps stable; linking libcups would put a second client library and its config
  parsing in the panel binary to re-derive answers three programs beside it already give. **It runs
  as the user**, because `/etc/group` grants the desktop user `lpadmin` — the authority CUPS itself
  defines. That is the difference between printing and mounting: CUPS shipped the privilege split
  and the kernel did not, so printing needs no daemon of ours in front of it. **`-m everywhere` and
  nothing else**: IPP Everywhere is what a driverless printer advertises and what CUPS resolves
  without a PPD, and a driver picker would be a thousand `lpinfo -m` rows to choose from — the
  dialog that made printing on Linux notorious. A printer needing more than that needs its vendor's
  tooling, and the surface says so. The `file` and `serial` backends are **dropped from the
  discovered list**: `lpinfo -v` names every backend CUPS has, and offering one that prints to a
  file or one that names an empty serial port is offering a queue that will never produce a page.
  The queue name is derived from the URI rather than asked for, because CUPS refuses a name
  carrying a space, a slash or a `#` and a device URI is full of all three.

## kdos-store

What this machine can build, and one tick to build it. The tab row is `Groups`,
`All`, then a tab per category the catalogue actually uses (most-populated
first), then `Installed`.

```
┌ Applications ─────────────────────────────────────────┐
│ Groups │ All │ Graphics │ Office │ Installed          │
├────────────────────────────────────────────────────────┤
│ [x] GIMP        rt-gtk   Create images and edit photos │
│ [ ] Krita       rt-qt    Digital painting              │
│ [·] Inkscape    rt-gtk   Vector graphics     installed │
├────────────────────────────────────────────────────────┤
│ 2 ticked · ~1.2 GB    F5 install  F6 import  F7 export │
└────────────────────────────────────────────────────────┘
```

**It computes nothing.** `kdos-appbox catalogue` answers what exists and what is
installed — in one call, with the state as a field, so the three surfaces that
ask cannot disagree — and `kdos-appbox install` knows how to build one. A
surface that re-derived either would be a second answer that drifts, which is
the rule `kdos-update` keeps about the version comparison.

**The category tabs are built from the data**, not listed in C, so a new
category in the catalogue is a tab with no code change. `Other` is not given a
tab: it is where a row with no `meta` lands, and a tab named after the absence
of information is not a place anybody looks.

**`[·]` is an application already here and cannot be ticked.** Installing it
again rebuilds an image that exists, and a tick that does nothing is a tick
somebody will count. A group row shows `[x]`, `[-]` or `[ ]` for all, some or
none of its members.

**The size is an estimate and the footer says so.** What apt resolves on the day
depends on the snapshot, and a runtime's layers are counted once on disk however
many applications name them.

**A build runs in a terminal and this window stays alive.** Installing is podman
and apt — minutes for one application, the better part of an hour for a group —
and a surface that ran it would be frozen for all of it, with one status line
standing in for output that is worth reading when a package fails to resolve.
The verb is handed to `kb_terminal()` and detached, so the build survives the
window being closed and `r` picks up the result. On a bare virtual terminal
there is no emulator to open, and the surface says so rather than naming a
window nobody gets.

Space ticks, Enter installs the row under the cursor (or ticks a group), `F5`
installs the ticked set, `F6` imports, `F7` exports, `r` refreshes.

## The small surfaces

| Name | Notes |
|---|---|
| `kdos-cal` | The calendar. It grew the two arrows and a Today button every calendar has had for decades — it had the wheel and no sign that it did anything. **It now shows what is on**: a day with an event carries a mark in the column the grid already leaves spare, and today's events are listed under the month. `khal` is asked when the popup opens and when the month changes, **never from the draw path** — a fork there would run once a frame and would put `$PATH`, which nothing fixes for a dump, inside the picture. The strip costs rows only when there is something to put in them, so a machine with no calendar draws the popup it always drew |
| `kdos-clip` | Clipboard history. The daemon owns the list; this draws it |
| `kdos-teams` | The window list, and what the panel's overflow cell opens — previously that cell stepped the row by one per click, so reaching the third hidden window took three clicks and three reflows |
| `kdos-display` | Screens. It grew a button bar, because a pointer could select a screen and then not switch it off or apply anything. `m` and the Mode button open a **dropdown** of the modes the monitor published: a screen that cannot show the mode being tried is a black screen and a wait for the revert, so the list is read before it is chosen from, never stepped blindly through |
| `kdos-keys` | The keybinding card, in six sections — launch, window, workspace, tools, media, system. **It reads whichever desktop it is opened on**: `rc.xml` under the compositor, and `kdos-con --keys` on the console, which prints the chord table after the `keys.conf` overlay. One reader and one writer — a second copy of the table is a copy that goes stale, and a card that is confidently wrong is worse than no card. **The card owns only the wording and the grouping**, in `con_section()`; an action it has no row for is dropped, so a chord added to the session and not here works and appears nowhere a person would look for it. `selftest.sh` fails the build on that, and it looks for the **row shape** rather than the action's name anywhere in the source: `net`, `power` and `settings` are ordinary words that appear there as other strings, and a bare name grep passed for eleven chords the card was in fact dropping. **`--print` writes the same rows to standard output**, two columns at 132 characters, form-fed between pages — for a printer and for a wall. It runs before any display server is opened, so it works over ssh, from a script and on a machine whose session is not up, which is most of the times somebody wants the card on paper. The same rows as the surface draws, because a printed sheet that disagreed with the screen is what a second hand-written table becomes. **`--first-run` is the login spawn's flag** and puts a four-row tour above the list — open a terminal, reach the menu, switch workspaces, reach another terminal — with each row's chord looked up in the same parse the list came from, so a rebound terminal moves the tour in the same edit and a step nothing binds is absent from the tour rather than wrong in it. The hint row names the chord that brings the card back, which is the one frame whose reader has not already used it. The tour is dropped below twelve rows, where what it pushes off the bottom is the card itself. **It is searchable.** An input row filters the rows through `kb_fuzzy()` — the same matcher the
palette and the launcher use, so three surfaces cannot rank one query three ways — and **both
columns are searched**, because somebody after the tiling chord may type `tile` or may type `Super`.
A section whose rows all fail the filter draws no heading. The field is always drawn rather than
appearing once typing starts: the card is what people open when they do not know what to press, and
the one thing it must say is that it can be asked. `Esc` clears a query before it closes the card,
through the same layer contract every other surface uses, so the hint row says which of the two the
next press does.

**`Tab` shows the focused program's own keys**, for the three that publish them, and is offered only
where a reader exists — a Tab that leads to an empty screen teaches that the feature is broken
rather than that the program does not publish its keys. The three are not alike, and the page says
which it is by naming the program in its title:

| Program | Read from | What it does not show |
|---|---|---|
| `tmux` | `tmux list-keys -N -T prefix`, live, plus `show-options -gv prefix` so a row reads `C-b c` and not `c` | The `root` and two `copy-mode` tables — 161 of the 267 default bindings, none reachable from the prompt the card is drawn over. `-N` lists only bindings carrying a note, because a row nobody wrote a description for has none |
| `mc` | `/etc/mc/mc.keymap`, with the user's copy preferred | The `Ctrl-x` second table, the editor and the viewer. And it shows the keymap FILE, not mc's live bindings: an action mc has a built-in key for that the file does not mention is absent |
| `micro` | `bindings.json` — **the overrides only** | Everything else, which is nearly everything: micro's defaults are compiled in, no flag prints them, and this image ships no micro configuration. On a fresh install the reader answers nothing and the page does not appear. Those rows are the diff, not the list |

**`helix` is named by the plan and is not here**, measured: the port exists and is in no
`packages.txt` at all, so it has never been built or shipped — a reader for it could not run and
could not be checked. It goes in when the port does.

**`--print` prints the page that is showing**, and a print run has no screen to read that off:
`--print` returns before any display is opened, which is the whole reason it works over `ssh`, so
there is no focused window to ask. `--program NAME` is how a print run says which page it wants, and
without it the card prints this desktop's chords.

**On the console it also lists the recorded scripts**: a row per letter under
`~/.config/kdos-con/scripts/`, showing the first ten keys of each and a count of the rest, so
`Super+Alt+r` has somewhere to look up which letters are taken. Read out of the directory by this
program running as the same person — the session grew no verb that could be asked, which is what
keeps a client on the surface socket from learning what somebody has recorded. **This program
decides whether the welcome is due**, from `~/.config/kdos/first-run`, so a session that asks at every login still shows it once |
| `kdos-style` | How the screen looks, `Super+Ctrl+Shift+Space`, on two pages — the accent and the font — because they are one question and a second window would be a second thing to find. **`kdos-theme` is a different program**: the artwork generator `kdos theme` runs, and one name for both left the picker unreachable. **The font page's list is the DISPLAY'S**: a surface never loads a font, so the faces come from the display through `libkdisp` and what goes back is an INDEX into that list and never a name — the display may be at the far end of an ssh link with its own machine's fonts. **The sample row is the screen**: a cell grid has one font at a time, so the arrows put the highlighted face on live and every cell in the window is then a sample of it; only `Enter` writes the state file, because a step that persisted would make the last face a highlight passed over the one the next login wears. On a view inside somebody else's terminal the list is empty and the page says **the terminal this view runs in owns the font — change it there**, which is the sentence the font chords put on the bar. The accent page: `Super+Ctrl+Shift+Space`. One row per scheme, **each drawn in its own colours**, the desktop repainting live as the highlight moves; `Enter` keeps and `Esc` puts back the one it opened on. **The swatches are the only literal colours on this desktop** — everything else draws in named slots so one word repaints all of it, and a swatch that took the accent in force would show seven identical rows; `preflight.sh` names this file as the exception rather than dropping the check. The blocks come from the glyph table like every other picture-character here, so the swatch is still a swatch on a terminal with no UTF-8. **A preview is half a theme**: `kdos theme --preview` writes the accent state file and signals the session, and generates none of the GTK, icon, cursor or foreign-configuration artefacts, which take seconds and are read by programs that are not running — so the desktop moves and a boxed application does not, and `Enter` runs the real switch. Leaving any other way, including a `SIGTERM`, puts the original back, because a picker killed halfway would otherwise leave the desktop wearing an accent nothing else had been regenerated for. **The window sizes itself from the scheme table**, so an accent added to `kcolor.h` needs no edit here |
| `kdos-doc` | The documentation viewer |
| `kdos-openwith` | Choose a handler, and optionally always use it. **The chosen entry is launched through `sh_launch()`** with the file as its document — the difference between a chooser and a launcher is which entry is picked and nothing else, so `%f` lands where the entry put it, a quoted argument survives, and a graphical handler on the console reaches the session. The *Other command…* row is a `verbatim` launch: the typed words become an argument vector, so a `;` in the box is an argument rather than a second program, and the path follows as a trailing argument. A plain command cannot be made the default — `mimeapps.list` records an ENTRY, and inventing one would leave a file in the user's applications directory nobody asked for |
| `kdos-run` | The run box. A click places the caret, and the button bar is on the surface because *In Terminal* is a **modifier**: a modifier nothing draws is a feature nobody finds. It is a `verbatim` `sh_launch()`: quoting is read, so `mpv "my film.mkv"` is two arguments, and a `%` reaches the program as typed. *In Terminal* wraps it in this desktop's emulator; without it, a graphical program is handed to the console session, which is what gives it a display |
| `kdos-prompt` | Yes or no, answering by **exit status** — which is what the compositor reads. `--input` is a second shape: one row with a text box, the typed line on **stdout**, 0 for an answer and 254 for Escape or an empty box. A mode and not a third button, because the yes/no shape's status is `kdos-comp`'s contract and must not gain a second meaning. It is a loop of its own — the widget is immediate-mode and wants the event inside `ktui_frame_begin()`, which is the opposite of the yes/no loop's hand-written key switch |
| `kdos-status` | The overflow popup; see below |
| `kdos-slit` | The dockapp column. Off by default: a slit nobody configured is a column of marks |
| `kdos-saver` | Attract mode, between idle and lock. **Eight effects, one row of a table each**: `art` drifts the picture and bounces it, `bounce` is that same effect under the name it is known by, `rain` falls as columns of shade, `matrix` falls as the same columns in characters, `pipes` grows and turns and clears when the screen fills, `starfield` flies stars past, `fire` climbs the palette from dim through urgent to text, and `clock` is the art with the time in block digits as its grid. `random` picks one per start and `off` is an honest off, drawing nothing and connecting to nothing, so an idle policy can start it unconditionally. `con.conf`'s `saver_mode` chooses; **`--mode` beats it**, which is load-bearing rather than conventional — `kcon_conf` reads `/etc/kdos/con.conf` before any XDG path and nothing can shadow it, so a golden would otherwise draw whatever the developer's own machine says. **`art` and `bounce` are a transform over one loaded grid** and the grid is a file — `~/.config/kdos/screensaver.txt` over `/usr/share/kdos/screensaver.txt` — so the picture belongs to whoever is looking at it; not `logo.txt`, which is the login banner's and must not change when a screensaver does. The other six need no file, which is what a machine with no art falls back to. **The vocabulary is the console font's 512 glyphs**: the density effects take the same three-level ramp the rain does, `pipes` has its own ASCII tier, and `matrix` and `starfield` are ASCII outright — an effect drawn out of what `ter-kdos32n` lacks is blank on `tty1` and correct in a terminal, which is the worst way to fail. It never watches input and claims no pointer region: a screensaver that decided for itself when to go away could decide wrong, and one that took the keyboard would be a lock screen with no password |
| `kdos-about` | What this machine is: the KDOS logo beside the version, kernel, libc, userland, session, terminal, grid, CPU, memory, uptime and package count. **Every fact is read, never forked** — `uname`, `/proc`, `/etc/os-release` and the package database are files this process can open, and a screenfetch spawned to render them would draw a second program's colours and ANSI onto a surface that paints in slots, and would make this the one surface with no offscreen dump |
| `kdos-calc` | The calculator, `Super+Ctrl+q`. **It does not do the arithmetic** — `qalc` does, and the tree already carries `libqalculate`, which parses what a person actually typed: units, hexadecimal, `to`, and precedence that matches a pocket calculator rather than a programming language. **Forked, not linked**: `libqalculate` is C++ and this binary is C and carries thirty-one other surfaces, so linking it would put libstdc++ on the panel package on every image for one accessory. **Once per pause, not once per keystroke** — the evaluation happens when the poll loop goes idle with the input changed, which is a debounce that costs no timer. `Enter` copies the answer, because the answer to "what is three inches in millimetres" is nearly always going somewhere else |
| `kdos-note` | The scratch pad, `Super+Ctrl+n`: one buffer per user at `~/.local/share/kdos/scratch.txt`, saved on close and every thirty seconds. **It is not an editor and must not grow into one** — `micro` is the editor and `Ctrl+O` opens this same file in it, and every feature past "type a line and find it later" already exists there and is better done there |
| `kdos-time` | The zone, the clock, and whether the clock is right. **The zone list is `zone1970.tab`, read** — tzdata ships here and carries the canonical list, and the hand-written table this replaced had already gone stale in the installer. **Setting it is a `kdos-powerd` verb**, because `/etc/localtime` and the profile's `TZ` are root's and the person setting a zone is the one administering the machine, which is what `wheel` already means; a setuid helper for one write would be a worse answer to a question that daemon already answers. `chronyc tracking` is **read and never driven** — whether to step the clock, how far and how fast is chrony's decision and a good one, and a "sync now" button would be `chronyc makestep`, the wrong thing to offer beside a clock already being disciplined. After a change the surface calls `tzset()` on itself, or its own clock keeps drawing the zone `TZ` named at the first call |
| `kdos-users` | The accounts, and the one thing about them this can change. **Split by privilege and it says which side each row is on**: reading `/etc/passwd` and `/etc/group` is anybody's, and creating an account, changing a password and editing group membership are root's — this program does none of them. An `Add user` button that answered "permission denied" would be worse than no button: it would read as a fault in the machine rather than as the boundary it is. `passwd`, `adduser`, `deluser` and `usermod` are on the image and are what a person changing accounts uses; wrapping them would put a root-spawning argument builder in the panel binary to reproduce their prompts and failure modes for a job done once per machine. The one thing it does change is the **autologin**, through `kdos-powerd` — `/etc/kdos/con.conf` is a KDOS file and "is this person administering the machine" is the question that daemon already answers. The list is `kb_users()`, the same call the greeter makes |
| `kdos-update` | What is behind, what is vulnerable and which slot is live. **It computes nothing**: `kdos update check --json`, `kdos cve --json` and `kdos-bootctl status` already answer these three, and a surface re-deriving any of them would be a second answer that drifts — the version comparison in particular is the packaging system's and is subtle. **It also applies nothing**: `kdos update apply` compiles packages, can take hours and on an A/B machine writes the OTHER slot, so a button behind a one-line status would be a progress bar over an unattended build with no way to see what it was doing; the surface says what to type and shows which slot it will land in. **The security table's age is on the screen beside the count** — a table three months stale reporting nothing to fix is worse than no answer. The JSON is read by a bounded key scan rather than a parser: both producers are in this tree, their shape is fixed, and a JSON library in the panel binary to read two documents it also writes would be a dependency bought for nothing |
| `kdos-firewall` | Which of this machine's services answer the network. **It carries no table of ports** — `kdos-powerd` owns the names, because a client that could name a port could open any port, and a second copy would be a second answer to what `ssh` is. It edits `/etc/nftables.d/50-kdos-services.nft` and only that; the daemon rewrites it whole, so anything hand-written belongs in another file beside it — said on the surface as well as in the file, because somebody who edited the wrong one would lose it on the next click. **It is not a firewall editor**: the shipped policy is a workstation's, and the only question here is which of a short list may be reached from outside. **The default is drawn on the screen** under the list, because every row is an exception to it and a list of exceptions with the rule missing reads as the whole policy. `open` is drawn in the warning slot rather than the accent — a port answering the network is the state worth noticing |
| `kdos-contacts` | The address book, `Super+Ctrl+b` — Sidekick's fifth accessory dialed a modem and its descendant is a lookup: type a name, `Enter` copies the address or the number. **The store is `khard` and this window holds none**: a second vCard parser would be a second answer to what a contact is, and the one that is not the store's is the one that goes stale. Two forks per query, never from the draw. **Two things khard does that cost a line each**: `email --parsable` prints `searching for '' ...` as its first row unless told not to, results or not; and an empty book exits non-zero while printing nothing, so what is read is the OUTPUT and the status is not consulted — a reader that gave up on the status would report an empty book the first time a search simply missed. An empty book names the program that fills it rather than saying only "0" |
| `kdos-chars` | The character map, `Super+Ctrl+e` — Sidekick's ASCII table with a search box. **The name index is built at BUILD time by a program that links ICU; this binary does not and must not** — ICU is thirty megabytes of library and data, and every one of the thirty-one surfaces this binary is would carry it for one accessory. The index is `mmap`ped and searched in place rather than read: a megabyte of names copied into the heap on every summon is a megabyte of dirty pages per surface instead of one page cache all of them share. **The blob is stored upper case**, so a keystroke folds the query and not forty thousand names. `Enter` copies the CHARACTER — not its name and not its number, which is what a person who wanted `U+2192` would have typed |
| `kdos-ascii` | A picture, as characters |

**`kdos-status` has a second half worth knowing about.** The hidden-widget list is published **by
the panel** into a file, rather than re-derived — re-deriving "three restarts" in the popup would
be a second implementation of the same reading, asking the system again at the moment somebody
clicked. Its other half is a **live pane**: it runs the stutter, restarts or energy report onto a
pipe and drains it without blocking, so the two most KDOS-specific tools on the machine are read in
a scrollable popup instead of in a terminal that covers the desktop and scrolls a fresh paragraph
per dropped frame. Long lines are soft-wrapped **on the way in**, because a stutter report is a
hundred columns wide and the half that gets clipped is the half naming the process.

## Popups and anchoring

**A menu opens under the word that was clicked.** Layer-shell surfaces have no coordinates, so "at
x" is an anchor plus a margin in pixels, which the panel passes. Without it every menu opened in the
**centre** of the screen and read as a dialog.

**The console session honours the same three fields**, in cells rather than pixels — and a caller's
number is the same on both, because `kdisp_cell_w()` answers 1 there. `kdos-con` passes the column
of the element that was clicked out of its own hit map, so `Start` and the clock open their menus
above themselves exactly as the panel's applets do.

**Which anchor depends on the bar's own edge**, because a popup belonging to a bar on the other edge
has to grow the other way.

**A popup's margin is measured from the output, and the exclusive zone decides that.** A layer
surface with a zone of zero is arranged inside the *usable* area, which already has the panel's
zone taken out of it — and the panel passes its own height as the margin, so the two applied one
after the other and every popup floated exactly one bar height above the bar it belonged to. A zone
of **minus one** means "do not move me out of anyone's exclusive zone", so the anchor is the output
edge and the margin is the only offset — which is the arithmetic the caller already did. It is set
only when a margin was actually given: a centred dialog, a notification with its default corner
margin, and the volume bezel are all asking to be *placed*.

**A keyboard overlay that loses focus closes itself**, gated on having seen focus first — the
compositor decides when an on-demand layer surface gets the keyboard, and a focus loss before any
gain would close the surface during its own appearance. There is no "unfocused menu" state worth
having.

**The panel lights the word the pointer is over.** Whether a menu is open is never known here — the
menu is a separate process and does not report back — so hover is what the bar actually knows, and
it is what makes three words read as three buttons.

## Dumping a surface

Every front end can render one frame with no display at all.

| Flag | Produces |
|---|---|
| `--dump` | The cell buffer as plain text |
| `--dump-cells` | One line per painted cell: row, column, character, colours, attributes |
| `KDOS_DUMP_SIZE=WxH` | Render at that size |

`--dump` proves the **layout**; `--dump-cells` is what makes a **colour** regression visible as
well as a geometric one. Reference frames for both are committed and compared by the test suite.

Dumping at a size that forces degradation is how the Start menu's columns were caught running
through their own footer — invisible for as long as neither column was long enough to reach it, and
invisible to the compiler, to the committed frames and to a running session.

## See also

- [The desktop](../02-user-guide/desktop.md) — using all of this
- [The design language](../03-architecture/design-language.md) — the rules every surface follows
- [Writing desktop software](../05-developer/writing-desktop-software.md) — adding a surface
- [kdos-comp](kdos-comp.md) — what supervises it, and the sockets it reads
- [Configuration](../06-reference/configuration.md) — `panel.conf` and the rest


## The candidate window

`kdos-ime` is the twenty-ninth name, and it exists because the candidate window was **the one thing
on this desktop that was not cells**: an input engine draws its own with its own renderer, which on
a character grid is a rounded antialiased panel sitting on top of a text-mode desktop.

It speaks **kimpanel**, the generic D-Bus panel protocol of the input-method framework and the same
mechanism KDE's plasmoid and the GNOME extension use. So this is not an input method and knows
nothing about any language: the engine decides what the candidates are and this draws them, with
`libkchrome` furniture and `libkcolor` slots through `libkdisp` — one surface on the Wayland desktop
and one on the console.

**Both halves of the protocol, because it is two.** The preedit, the auxiliary string and the show
and enable flags arrive as *signals* on `org.kde.kimpanel.inputmethod`; the candidate list arrives
as a *method call*, `org.kde.impanel2.SetLookupTable`, on the panel's own object. A panel that only
listened would show a preedit with nothing under it. The signal match names no path — fcitx5 5.1
exports `/kimpanel` and older panels documented `/kimpanel/inputmethod` — and only the methods this
can actually answer are declared, because the engine reads the introspection to decide what to send.

**Starting it IS selecting it.** fcitx5's kimpanel module has a UI priority above its own classic
interface and becomes available the moment `org.kde.impanel` has an owner, so the session bring-up
starting `kdos-ime` is the whole of choosing this panel over the engine's own.

**There can be only one, and the protocol cannot hand the name back.** A second `kdos-ime` refuses
to start and names the program that owns it, rather than taking the name and leaving whatever was
drawing the candidates believing it is still the panel.

**AND THE TWO DESKTOPS SHARE ONE SESSION BUS**, so that rule decides which screen the candidates
appear on. The console's panel starts at boot and holds the name; a graphical session is started
from that console and is the one with the keyboard, so `kdos-desktop-start` ends the console's panel
and starts its own, and kills it again when the compositor exits. The console's is a respawn loop
for exactly that reason: it fails while the graphical session holds the name and has the window back
within five seconds of it ending. Without the handover the engine converts perfectly and draws its
candidates on a virtual terminal nobody is looking at.

**The engines a key can reach are a shipped file**, `~/.config/fcitx5/profile` from `/etc/skel`,
because the configuration tool upstream ships is built on a toolkit this host does not have. Its
group names `keyboard-us` first — a login types Latin — then `pinyin`, `anthy` and `hangul`, which
are the engines the image installs. fcitx5 started with no such file has a group holding nothing but
the keyboard layout: `fcitx5-remote -s pinyin` then silently does nothing, every keystroke arrives
as Latin, and no candidate window is ever asked for.

**On the console the window is drawn and the engine is not running** — the limit and its reason are
in [known-gaps](../06-reference/known-gaps.md), which is the one page that states it.
