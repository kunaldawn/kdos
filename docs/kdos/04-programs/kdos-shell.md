# kdos-shell

One binary providing thirty-three commands, dispatched on the name it was invoked as: the panel,
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
| `kdos-launcher` | Full-screen application search | [kdos-launcher](#kdos-launcher) |
| `kdos-menu` | Root, System and window menus | [kdos-menu](#kdos-menu) |
| `kdos-desk` | The desktop and its icons | [kdos-desk](#kdos-desk) |
| `kdos-pick` | The file chooser and browser | [kdos-pick](#kdos-pick) |
| `kdos-notifyd` | The notification daemon | [Notifications](#notifications) |
| `kdos-notify` | The notification centre | [Notifications](#notifications) |
| `kdos-osd` | Volume and brightness | [kdos-osd](#kdos-osd) |
| `kdos-cal` | The calendar | [The small surfaces](#the-small-surfaces) |
| `kdos-settings` | Settings | [kdos-settings](#kdos-settings) |
| `kdos-net` | Networking | [The device managers](#the-device-managers) |
| `kdos-bt` | Bluetooth | [The device managers](#the-device-managers) |
| `kdos-audio` | Audio devices | [The device managers](#the-device-managers) |
| `kdos-devices` | Cameras, microphones, removable media | [The device managers](#the-device-managers) |
| `kdos-clip` | Clipboard history | [The small surfaces](#the-small-surfaces) |
| `kdos-status` | The overflow popup | [The overflow chevron](#the-overflow-chevron) |
| `kdos-tip` | Tooltips | [Tooltips](#tooltips) |
| `kdos-ime` | The input-method candidate window | [The candidate window](#the-candidate-window) |
| `kdos-teams` | The window list | [The small surfaces](#the-small-surfaces) |
| `kdos-display` | Screen configuration | [The small surfaces](#the-small-surfaces) |
| `kdos-keys` | The keybinding card | [The small surfaces](#the-small-surfaces) |
| `kdos-doc` | The documentation viewer | [The small surfaces](#the-small-surfaces) |
| `kdos-openwith` | Choose a handler | [The small surfaces](#the-small-surfaces) |
| `kdos-run` | The run box | [The small surfaces](#the-small-surfaces) |
| `kdos-prompt` | Yes/no, answering by exit status | [The small surfaces](#the-small-surfaces) |
| `kdos-slit` | The dockapp column | [The small surfaces](#the-small-surfaces) |
| `kdos-saver` | Attract mode between idle and lock | [The small surfaces](#the-small-surfaces) |
| `kdos-about` | What this machine is | [The small surfaces](#the-small-surfaces) |
| `kdos-calc` | The calculator | [The small surfaces](#the-small-surfaces) |
| `kdos-note` | The scratch pad | [The small surfaces](#the-small-surfaces) |
| `kdos-ascii` | A picture, as characters | [The small surfaces](#the-small-surfaces) |
| `kdos-trash` | What was deleted, and the way back | [kdos-trash](#kdos-trash) |
| `kdos-peek` | What is in a file, without its application | [kdos-peek](#kdos-peek) |

## Places

Home, the XDG user directories that exist, then whatever `~/.config/kdos/places` adds. **One
reader**, `kxdg_places()`, so the desktop folder, the Places menu, the chooser and *Add to Places*
cannot disagree — see [libkxdg](../05-developer/c-libraries.md#libkxdg) for what they used to
disagree about.

| Where | How |
|---|---|
| `kdos-menu --places` | The whole column, plus Trash and Computer |
| `kdos-desk` | *Add to Places* on a folder's context menu, and on the wallpaper, where it keeps `~/Desktop`; never on a file, because a file is not a place |
| `kdos-pick` | `Ctrl+P` opens the column over the file list, with a **Recent directories** group under it |

**Recent** is the same shape: `kdos-appbox open` is the one function every open passes through, so
it is the only place `recently-used.xbel` is written. `kdos-start`'s right column draws a RECENT
group from `kxdg_recent_all()` — only when there is something in it, because a heading over nothing
reads as a list that failed to load rather than as a machine nobody has opened anything on. A row
opens through `kdos-appbox open` again, so a recent file opens with whatever its **type** is bound
to rather than with whatever last touched it.

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

Five surfaces claim an `F1` page today: `kdos-settings`, `kdos-display`, `kdos-net`, `kdos-bt`
and `kdos-devices`, plus [`kdos-res`](kdos-res.md). The pages live in `/usr/share/kdos/doc` and
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

### The Start button

Three states, one function drawing both the pixel tile and the character fallback — a control
whose two renderings disagree about its own state is worse than either.

- **Quiet at rest.** The accent belongs to hover, and the warning colour to the menu being open.
  At full strength it is the loudest object on screen, on the one control that is never the thing
  being looked at.
- **It carries the word**, because a button that is a picture the same size as the application
  icons beside it does not read as the way in.
- The tile lays out padding, mark, gap, word, padding — where padding and gap are **different**
  numbers — and the content is then **centred** in the tile, because a tile is a whole number of
  cells and the content is not. The rounding slack split between both ends is invisible; pushed to
  one end it is an asymmetry you can see.
- **Where the mark falls back to the literal word, the label is dropped**, since the brand printed
  beside itself says it twice.

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

An item is **one cell**: the first letter of its identifier, coloured by its status — dim for
passive, the text colour for active, the accent and reversed for needs-attention. The identifier
rather than the icon name, because a letter from a name a human chose beats a letter from a theme
lookup that will never happen on a character grid.

Left activates, middle secondary-activates, right opens the context menu.

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

**Known gap: menus published over the menu protocol are not rendered.** An item that expects the
host to draw its menu will do nothing when clicked. Such an item is hidden by default through
`tray_hide =`, listed in the chevron's popup where the row can say what it is, and its tooltip
says why it cannot be clicked. One line of `panel.conf` brings it back.

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

**The way back is a row**, because Escape and the right button are not discoverable and a
pointer-only user is exactly the first-time user of a Start menu. It is the first row of the left
column, and it never closes the menu. A search gets the same row as "clear search".

**The field looks like a field**: placeholder text, a block caret, sunken, lighting under the
pointer, and going to the accent the moment it is active. A control that looks identical before
and after being clicked is one people click again to find out whether it worked. The clear mark is
drawn only while there is something to clear.

**With nothing typed, that row explains the selection** — every desktop entry carries a comment and
this menu was throwing it away.

**Search reaches the fixed rows too.** Every one carries synonyms, so typing `wifi` finds the
network manager; the hits are appended under a rule. A search over the application index alone
answered `wifi` with an empty list on a machine whose network tool is three rows up the same menu.

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

The same rule runs one level down. **Launching a graphical application from the console asks the
session for a terminal**, which wraps it in `kdos-cage`; a `Terminal=true` entry becomes a
`kdos-term` window instead.

**Nothing here names a terminal emulator.** `sh_term()` answers with `kdos-term` when `$KDOS_CON` is
set and `foot` otherwise, and every place that opens one — the root menu's rows, Places, Open
Terminal Here, the manual-page link, the CPU tile, Open With, the run box's *In Terminal*, the
launcher, the desktop icons and the key card — asks it. Both emulators take `-e CMD` and `-D DIR`
with the same meaning, so naming the program is the whole of the difference. A call site that spells
`"foot"` instead is a row that does nothing on the console desktop, where there is no compositor for
a Wayland client to be under.

**A terminal running somebody else's program is given that program's name.** The compositor matches
a window to its desktop entry by app-id, so a `Terminal=true` entry started as a bare `foot -e btop`
is a taskbar row called foot, wearing foot's icon, however many are open. `sh_term_argv()` writes
the emulator, the identity and the `-e` together: `--app-id` for foot, `--title` for `kdos-term`,
taken from the first word of the entry's `Exec`. The two rows that open a terminal *as itself* —
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

The **window menu** carries Restore, Minimize, Maximize/Restore Down, Fullscreen and Close, plus
Minimize all and Close all for a group, over the window titles. It reads the window's own state, so
Maximize says *Restore Down* when the window is maximised rather than being a toggle whose
direction nobody can see.

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
something different from `Enter` on the same icon. *Move to Trash* asks first and refuses the two
pinned places — `kdos trash <file>` confirms nothing, which is right for a prompt and for `mc` and
wrong for the row sitting beside `Delete` on this surface.

The context menu is drawn **into its own grid**, since this surface owns the screen and a popup
here is not a second surface. Its local rows carry a **scope** — icon, wallpaper, or both — because
two menus would be two places for New Folder to drift.

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

**A directory is refused.** A file manager inside a viewer that was opened from a file manager is a
circle; `mc` shows directories and this shows files.


## Notifications

`kdos-notifyd` owns the bus name; `kdos-notify` is the centre. The daemon owns the list and the
front end draws it — the same split the clipboard uses.

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
| `dnd` | Toggle do not disturb |

**Unseen is what the badge counts**, cleared by the centre being opened and by nothing else. A
count that cleared itself on a timer is a count nobody trusts.

**Do not disturb is only honest with a history behind it.** Silencing toasts without somewhere for
them to go would mean losing them; with the ring in place the notification is kept, the badge still
counts it, and the sending application cannot tell — the identifier is returned and the close
signal is still emitted, so nothing hangs waiting. **An urgent notification is shown anyway**: a do
not disturb that hid a battery-critical warning would be a switch nobody dares leave on.

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

**Nine categories:** Appearance, Panel, Desktop, Hardware, Session, Input, Apps, Boxes and
**System** — the machine itself, where the resource monitor and the power page live. Most of what
belongs in System is not written; the category ships with a row saying which commands do that work
today, because a control centre whose front door has no door for the machine teaches that the
machine is not reachable from here, and that is the harder thing to unteach. A row that opened
nothing would be worse than no row.

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

`kdos-settings` also writes `panel.conf`, which the panel re-reads on signal.

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

**Anchored means popup; centred means window.** Each is both things and the difference is which
one asked: from the panel it is the bar's own popup, sized like one and dismissed by a click
elsewhere; typed by name or opened from the Start menu it is the application, centred, full size
and staying up — because somebody who went looking for the network tool will look at something else
in the middle of using it, and a pairing confirmation must not vanish because the pointer went to
the device on the desk. One flag decides both.

Per-manager:

- **`kdos-net`** talks to the network service over the system bus. **The list does not reorder
  under the pointer**: signal strength moves on its own, so it is sorted once per refresh and the
  selection follows the network name rather than the row index. **Known limit:** there is no secret
  agent, so a passphrase is written into the connection at creation and the service cannot come back
  and ask for another — enterprise authentication and one-time-password VPNs need the full text
  tool.

  ![kdos-net: the header band says what the subject is doing now, and the buttons are enabled from the selection](../../screenshots/net.png)

- **`kdos-bt`** registers a pairing **agent**, without which a keyboard cannot be paired at all:
  the service asks the agent to confirm a passkey and refuses the pairing when nobody answers. The
  confirmation is a **deferred reply** — the handler retains the message and returns without
  replying — because a handler that sat in its own loop would stop answering the service.
- **`kdos-devices`** enumerates cameras by device call rather than through a library, finds who is
  holding one by walking the process table, and previews a grabbed frame through the shape-matching
  character renderer. **Opening a camera to preview it is using it**, so the privacy lamp lights for
  this program too and the descriptor is closed with the frame. It also fronts removable media.

## The small surfaces

| Name | Notes |
|---|---|
| `kdos-cal` | The calendar. It grew the two arrows and a Today button every calendar has had for decades — it had the wheel and no sign that it did anything |
| `kdos-clip` | Clipboard history. The daemon owns the list; this draws it |
| `kdos-teams` | The window list, and what the panel's overflow cell opens — previously that cell stepped the row by one per click, so reaching the third hidden window took three clicks and three reflows |
| `kdos-display` | Screens. It grew a button bar, because a pointer could select a screen and then not switch it off or apply anything |
| `kdos-keys` | The keybinding card, in six sections — launch, window, workspace, tools, media, system. **It reads whichever desktop it is opened on**: `rc.xml` under the compositor, and `kdos-con --keys` on the console, which prints the chord table after the `keys.conf` overlay. One reader and one writer — a second copy of the table is a copy that goes stale, and a card that is confidently wrong is worse than no card. **The card owns only the wording and the grouping**, in `con_section()`; an action it has no row for is dropped, so a chord added to the session and not here works and appears nowhere a person would look for it. `selftest.sh` fails the build on that, and it looks for the **row shape** rather than the action's name anywhere in the source: `net`, `power` and `settings` are ordinary words that appear there as other strings, and a bare name grep passed for eleven chords the card was in fact dropping. **`--print` writes the same rows to standard output**, two columns at 132 characters, form-fed between pages — for a printer and for a wall. It runs before any display server is opened, so it works over ssh, from a script and on a machine whose session is not up, which is most of the times somebody wants the card on paper. The same rows as the surface draws, because a printed sheet that disagreed with the screen is what a second hand-written table becomes |
| `kdos-doc` | The documentation viewer |
| `kdos-openwith` | Choose a handler, and optionally always use it |
| `kdos-run` | The run box. It takes a click to place its caret, and grew a button bar because its one feature beyond a prompt was a **modifier** that nothing announced |
| `kdos-prompt` | Yes or no, answering by **exit status** — which is what the compositor reads |
| `kdos-status` | The overflow popup; see below |
| `kdos-slit` | The dockapp column. Off by default: a slit nobody configured is a column of marks |
| `kdos-saver` | Attract mode, between idle and lock |
| `kdos-about` | What this machine is: the KDOS logo beside the version, kernel, libc, userland, session, terminal, grid, CPU, memory, uptime and package count. **Every fact is read, never forked** — `uname`, `/proc`, `/etc/os-release` and the package database are files this process can open, and a screenfetch spawned to render them would draw a second program's colours and ANSI onto a surface that paints in slots, and would make this the one surface with no offscreen dump |
| `kdos-calc` | The calculator, `Super+Ctrl+q`. **It does not do the arithmetic** — `qalc` does, and the tree already carries `libqalculate`, which parses what a person actually typed: units, hexadecimal, `to`, and precedence that matches a pocket calculator rather than a programming language. **Forked, not linked**: `libqalculate` is C++ and this binary is C and carries thirty-one other surfaces, so linking it would put libstdc++ on the panel package on every image for one accessory. **Once per pause, not once per keystroke** — the evaluation happens when the poll loop goes idle with the input changed, which is a debounce that costs no timer. `Enter` copies the answer, because the answer to "what is three inches in millimetres" is nearly always going somewhere else |
| `kdos-note` | The scratch pad, `Super+Ctrl+n`: one buffer per user at `~/.local/share/kdos/scratch.txt`, saved on close and every thirty seconds. **It is not an editor and must not grow into one** — `micro` is the editor and `Ctrl+O` opens this same file in it, and every feature past "type a line and find it later" already exists there and is better done there |
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
starting `kdos-ime` is the whole of the configuration. Nothing is written to a config file.

**There can be only one, and the protocol cannot hand the name back.** A second `kdos-ime` therefore
refuses to start and names the program that owns it, rather than taking the name and leaving
whatever was drawing the candidates believing it is still the panel.

**On the console the window is drawn and the engine is not running.** fcitx5 speaks
`input-method-v2` to a compositor and there is none on that path, which is why
[known-gaps](../06-reference/known-gaps.md) still records no input method there.
