# The window model

This chapter explains the rules that decide where a window goes on the KDOS desktop: where a new
window opens, what tiling and snapping do to it, which edge it stops against when it is pushed, how
a dialog travels with the window that opened it, where a window reopens after it was closed, and
which workspace a switch lands on. It is written for anyone who wants to know why a window ended up
where it did, and for contributors changing the compositor, `kdos-comp`, or the library that holds
its geometry, `libkwm`.

If you only want the keys and mouse gestures, read [The desktop](../02-user-guide/desktop.md#windows)
first; this chapter explains what those gestures do underneath. Contributors should read
[What the library is, and is not](#what-the-library-is-and-is-not),
[The library's entry points](#the-librarys-entry-points) and [The contract](#the-contract) before
changing any rule described here.

Two terms run through the whole chapter. An *output* is a screen. Its *usable area* is that screen
less the space panels reserve for themselves, and every rectangle below is measured inside it. A
*margin* is the thickness of the decoration the compositor draws around a window: the title bar on
top and the border on the other three sides.

## What the library is, and is not

KDOS keeps the geometry of window management, the arithmetic, in a small C library, `libkwm`
(`src/libs/libkwm`, four source files and one header), outside the compositor that obeys it. The
compositor, `kdos-comp`, is a fork of the labwc Wayland compositor; see
[kdos-comp](../04-programs/kdos-comp.md), and
[How KDOS differs](../01-philosophy/how-kdos-differs.md#the-desktop) for why KDOS ships one desktop
of its own rather than an existing one.

`libkwm` is handed rectangles and told what is being asked. It knows nothing about windows.

| The library answers | The caller still owns |
|---|---|
| Where a new window lands among the ones already there | What a window *is*, and which output it is on |
| What the tiled state becomes, and what rectangle that state occupies | Whether the window is maximised, and whether a client accepted the size |
| Which other edge a moving edge meets first | Walking the window list, deciding which edges are visible, and reading how thick the decoration is |
| The nearest occupied workspace in one direction | Which workspaces exist, and what "occupied" means |
| Whether a press-and-move has become a drag | Which surface the press was on |

That split is what lets every rule be checked against a table of rectangles with no compositor and
no display running. [The library's entry points](#the-librarys-entry-points) lists where each
answer is used, and [The contract](#the-contract) describes the table.

## Focus and stacking

A window takes the keyboard when you click it. A left, right or middle press inside a window
focuses it and raises it; this is a compositor default binding, and the shipped
`~/.config/kdos-comp/rc.xml` keeps it by starting its mouse bindings with `<default />`. Focus does
not follow the pointer: the shipped `rc.xml` does not set `<focus><followMouse>`, and the
compositor's default is off.

A *shaded* window is rolled up into its title bar: the frame stays where it was and the content is
hidden. The shipped `rc.xml` shades a window when the wheel turns up over its title bar and unshades
it when the wheel turns down; the wheel-up binding also takes the keyboard away from the window,
and the wheel-down binding gives it back. A double-click on the title and `Super+s` toggle shading.

`Super+m` toggles maximise and `Super+f` toggles fullscreen. Maximise is a compositor state of its
own, per axis: see [Opposing edges are not a tile](#opposing-edges-are-not-a-tile).

## Tiling is two steps

Tiling a window answers two separate questions: which *state* the window goes to (left half,
top-right quarter), a decision over a bitmask of edges (`kwm_tile_next()`), and which *rectangle*
that state covers on this screen, arithmetic over the usable area (`kwm_tile_geom()`). Keeping them
apart is what makes each testable on its own.

The edge bits are `TOP`, `BOTTOM`, `LEFT`, `RIGHT` and `CENTER`, with the same values as the
compositor's own `enum lab_edge`, so the compositor passes its enum straight in. Changing a value on
one side without the other is a silent geometry error.

### The transition

The transition splits the current tiled state into the part *parallel* to the direction of the snap
and the part *orthogonal* to it. The shipped `rc.xml` binds `Super+Left`, `Super+Right`,
`Super+Up` and `Super+Down` to `SnapToEdge` with `combine="yes"`:

| Window is | You press | It becomes | Why |
|---|---|---|---|
| Untiled | `Super+Left` | Left half | The request is taken as it is |
| Left half | `Super+Up` | Top-left quarter | A half plus an orthogonal edge is a quarter |
| Top-left quarter | `Super+Down` | Left half | Snapping away from the edge it holds on that axis drops that edge and leaves the half on the other axis |
| Top-left quarter | `Super+Up` | Top half | See below |
| Left half | `Super+Right` | Right half | The opposite edge replaces the one it had |
| Left half | `Super+Left` | Right half, on the screen to the left | See below |

The fourth row is the least obvious. A quarter snapped towards the edge it already occupies collapses
to a half: the parallel part is then neither the opposite of the request nor absent, so no combining
rule applies, the request is taken unchanged and the orthogonal part is dropped. That is the
compositor's behaviour and the fixture pins it.

The last row is snapping *across outputs*, which every `SnapToEdge` action asks for (a drag to a
screen edge does not).
When the window is already tiled to exactly the requested edge, the compositor moves it to the
neighbouring screen in that direction with the edge inverted, so a left-tiled window snapped left
again lands right-tiled on the screen to its left. With no usable screen there, nothing changes.
This rule applies whether `combine` is on or off.

With `combine` off, the request always wins and a quarter is never built. A request that is not a
single edge (a corner asked for directly, `CENTER`) is taken as given, and a window tiled to
`CENTER` never combines.

Two cases never reach the library, because they are about window state. A maximised window is
unmaximised first and takes the requested edge unchanged. A fullscreen window ignores
`SnapToEdge` entirely.

`ToggleSnapToEdge` is the same action, except that a window already tiled to exactly the requested
edge is untiled and returned to its restore rectangle instead. The shipped `rc.xml` does not bind
it.

### The gap arithmetic

The two halves of an axis come from different expressions: `(size + gap) / 2` and
`(size - gap) / 2`. That puts one whole gap between two tiled windows rather than half a gap each,
and it means an odd dimension gives the right or bottom half one extra pixel. The extra pixel is
intended; the fixture pins it.

The gap is `<core><gap>` in `~/.config/kdos-comp/rc.xml`; KDOS ships it at `0`, which is also the
compositor's default.

Every tiled rectangle is then inset by the margin on each side, so the title bar and border fit
inside the tile. A state that has none of the four edge bits (`CENTER`, or no state at all) is the
whole usable area inset by the gap and the margin. The centre state is therefore shaped like a
maximised window, not centred.

### Opposing edges are not a tile

A state holding both edges of one axis collapses that axis. Left sets the far bound to the midpoint
and right sets the near bound to the midpoint, so together they give a width of zero with no gap,
and a negative one once a gap or the margins come off.

That is not a tile the model defines. The transition never produces an opposing pair, so the
fixture has no row for one and none should be invented. Maximise is not built from all four edges
either: the compositor keeps it as its own state (`view->maximized`, per axis: horizontal, vertical
or both) and computes its rectangle from the usable area directly.

### A drag that ends against an edge

Dragging a window by its title bar and letting go near a screen edge snaps it there. The pointer
counts as at an edge when it is within 10 pixels of the usable area's edge
(`<snapping><range inner="…" outer="…">`, both 10 by default; the inner value applies where another
screen lies beyond that edge, the outer value where none does). Within 50 pixels of a corner along
that edge (`<snapping><cornerRange>`) it counts as the corner, and the window takes the matching
quarter. A preview of the target is drawn after the pointer has rested there for 500 milliseconds
(`<snapping><overlay><delay inner="…" outer="…">`, both 500 by default). The shipped `rc.xml`
sets none of these, so the compositor defaults apply.

The drag snaps with combining turned off, so the edge the pointer landed on is taken as it is: a
window dragged to the right edge becomes the right half, never a half of a half. A tiled or
maximised window stays where it is until the pointer has moved 20 pixels
(`<resistance><unSnapThreshold>`). A window maximised on one axis only is held until the pointer
has moved 150 pixels along the axis it is maximised on (`<unMaximizeThreshold>`); along the free
axis it moves at once and stays maximised. Past the threshold, the window returns to its restore
rectangle under the pointer and floats for the rest of the drag.

The top edge alone is different: it maximises. That is `<snapping><topMaximize>`, default `yes`.
A top half is available from `Super+Up`.

### One restore rectangle

Each window keeps a single *restore rectangle* (labwc calls it the "natural geometry"): the size and
place an untile, unmaximise or un-fullscreen returns to. It is written only from an untiled
rectangle. Snapping, maximising and going fullscreen all refuse to overwrite it while the window is
tiled or fullscreen, and a window maximised on one axis records only the other axis. Leaving
fullscreen re-derives a tiled window's rectangle from its tiled state rather than replaying a
stored one.

Fullscreen is independent of the tile. If going fullscreen overwrote the restore rectangle, a later
untile would return the window to the tile it is already in. Re-deriving on the way out is also
what puts a window that was fullscreen across a panel change or a screen resize into the tile the
new layout gives.

A change of screen layout (a screen connected, removed, resized or moved) keeps each window's
state. A tiled, maximised or fullscreen window has its rectangle derived again from that state on
the screen it was on, or on the screen it is moved to when that one is gone; a floating window keeps
its position relative to its screen (or its place in the whole layout when that screen is gone) and
is pulled back on-screen if it would fall off. That is
`view_adjust_for_layout_change()` in `src/view.c`.

## Placement is a search, not a cascade

A new window that did not place itself is placed by minimising its overlap with the windows already
on its screen. This is labwc's `Automatic` placement policy and the compositor's default;
`<placement><policy>` in `rc.xml` also accepts `Center`, `Cursor` and `Cascade`, and only
`Automatic` uses `libkwm`. The shipped `rc.xml` does not set it. The algorithm is Openbox's
minimal-overlap placement, transcribed rather than reinvented, because the fixture records where it
puts windows and a tidier search would put them somewhere else.

The search works like this:

1. Every window on that output and on the current workspace is taken as a box, already enlarged by
   its own margin. A shaded window counts only as its title bar.
2. The edges of those boxes are extended across the whole usable area, which divides it into an
   irregular grid. Every cell of the grid is then either wholly covered by a given window or not
   covered by it at all, which is what makes the overlap count exact.
3. Each cell is counted for how many windows cover it.
4. The new window, enlarged by its margin and by the gap on both sides, is slid across the grid in
   four directions, and its overlap is the covered area it would sit on, weighted by that count.
   The first position with no overlap at all ends the search. Otherwise the position with the least
   overlap wins.

With nothing else on the output, if the grid cannot be allocated, or if the window is larger than
the usable area, it lands in the upper-left corner, inset by both its margin and the gap.

### The decoration margin has four sides

`KwmBorder` is `top, right, bottom, left`, and every caller fills in all four. `kdos-comp` measures
in pixels and hands in its title bar's height and its border's width, which are not the same
number. A caller that passed one number four times would give the model a margin its own frames do
not have, and every placed window would be off on one axis.

Where the margin's cost falls depends on which rectangle is fixed:

- A **freely placed** window (placed by the search, dragged, or restored by window memory) keeps
  its *content* rectangle, and the frame is added outside it. A thicker border reaches further
  across the desktop, and the program keeps all of its content.
- A window whose **outer** rectangle is fixed (maximised or tiled) has the margin subtracted from
  that rectangle, so its content is smaller than the space it fills by the margin on each side.

A thicker border therefore costs space in every maximised and tiled window, and none in a floating
one.

### A placement keeps the frame on the screen

A content rectangle that fits the usable area can still put its title bar and borders off the edge.
A frame drawn off-screen is a window with nothing to grab: no title bar to drag, no border to
resize. So:

- a window too large for the usable area, less the margin on each side, is shrunk to fit before it
  is placed, which is the tiled-window cost again;
- a window returning to its restore rectangle, or moved by a change in the screen layout, is moved
  until its title bar starts inside the usable area: its content's top-left corner is at least the
  margin in from the usable area's top-left corner.

### Window memory

Window memory saves each application's rectangle, workspace and shaded state when its window
closes, and puts them back the next time a window with the same application id opens. It is
`window_memory` in `~/.config/kdos/comp.conf`, on by default, and lives in the compositor
(`src/kdos-winpos.c`), not in `libkwm`. The records are kept in `$XDG_STATE_HOME/kdos/winpos`, most
recent first, and only the 200 most recent applications are kept.

It applies only to a window the compositor would otherwise place itself. These are left alone:

- a window that positioned itself (an X11 window with a position hint);
- a window a *window rule* placed (a `<windowRule>` entry in `rc.xml`, which matches windows by
  application id, title or box), or one with the `fixedPosition` rule;
- a window that maps maximised, tiled or fullscreen;
- a window with an owner (see [below](#where-an-owned-window-opens)).

A remembered rectangle may come from a screen of another size, so it is clamped into the usable
area of whichever screen its centre lands nearest: first shrunk to fit, then moved inside. The
clamp applies to the content rectangle, not the frame, so a window remembered against the top or
left edge can reopen with its title bar outside the usable area. If another window of the same
application already sits at that position on the same workspace, the remembered rectangle is not
used and the overlap search places the new window, so a second instance does not open exactly on
top of the first.

## A window that belongs to another window

One program is not one window. An image editor's toolbox, its image window, two docks and a modal
file chooser can be five windows over one connection. The compositor therefore has words for how
they relate:

- A window may have an **owner** (a *parent*): the Wayland `xdg_toplevel.set_parent` request, or
  X11's transient-for hint under Xwayland. A window with an owner is called *owned* below. The
  top-level owner with everything it owns, directly or through another owned window, is a
  *family*.
- An owned window may also be **modal**: the `xdg-dialog-v1` modal flag, or X11's modal state. A
  modal dialog blocks its owner until it is answered.

This relation belongs to the compositor. `libkwm` never sees it.

### Where an owned window opens

A Wayland window with an owner opens centred on its owner, at the size it asked for, shrunk first
if that is larger than the usable area, and kept inside the usable area. It does not go through the
overlap search: a dialog belongs next to the window that raised it, not wherever there happened to
be room. An X11 window follows the X11 rules instead: one that gives a position keeps it, and one
that does not is placed by the placement policy like any other window.

Window memory neither records nor restores a window with an owner. Every window of one program
carries the same application id, so a remembered file chooser would be saved as the rectangle the
program's main window opens at next time.

### Raising, lowering and minimising a family

| You do this | What happens |
|---|---|
| Raise any member (click, focus, the switcher) | The top-level owner is raised first, then every owned window in the order they were already stacked, then the window you named goes on top of them all. Nothing else can come between a dialog and its owner |
| Focus any member when the family has a modal dialog | The keyboard goes to the modal dialog, not to the window you clicked, whichever way the family was focused |
| Lower (the `Lower` action, bound to no key or button in the shipped configuration) | The owned windows go to the bottom, then the owner below them |
| Minimise any member | The whole family is minimised, whichever member asked. Restoring brings the whole family back |

Owned windows keep their stacking order among themselves on a raise because they are collected back
to front and each is moved to the front in turn.

### What a family does not share

These act on one window, not on its family:

- **Workspaces.** `SendToDesktop` (`Super+Shift+1` to `Super+Shift+9`) moves only the window it
  names, and the view follows it by default. Its dialogs stay where they were.
- **The taskbar.** The panel lists every window with its own button, owned ones included; it
  receives the ownership the compositor reports and ignores it.
- **The window switcher.** Modal dialogs are left out of the switcher's ring; the owner stays in it,
  and focusing the owner hands the keyboard to the dialog anyway. Non-modal owned windows (a
  toolbox, a dock) stay in the ring, because they are exactly what someone switches to.

A modal dialog is modal to its application, not to the machine: every other window carries on.

### Tab groups

Tab groups are a relation separate from ownership: the compositor can stack windows as tabs. The
`AddToTabGroup` action folds the focused window onto the one behind it, `RemoveFromTabGroup` takes
it out, and `NextInTabGroup` steps through the tabs; clicking a tab on the title bar shows that
member. Hidden members are minimised windows sharing the shown member's rectangle, so the taskbar
reports them as minimised with no extra code. Dragging a tab moves the member it names; it does not
tear the member out of the group. None of the three actions is bound to a key in the shipped
`rc.xml`. Tabs stack; they do not tile. See
[Decisions](../01-philosophy/decisions.md#narrowings).

## The edge search is one question

*If this edge moves in this direction, which other edge does it meet first?* Three keyboard
commands are built on that question:

| Command | Shipped keys | What it does |
|---|---|---|
| `MoveToEdge` | `Super+Alt+arrow` | Moves the window until it meets the next edge |
| `GrowToEdge` | `Super+Ctrl+arrow` | Moves the window's leading edge out to the next edge |
| `ShrinkToEdge` | not bound | Pulls the window's trailing edge in to the next edge |

`MoveToEdge` first aims for the edge of the screen's usable area, less the gap and the margin, and
then stops short of it at the first window edge in the way. Its `snapWindows` attribute, `yes` by
default, can be set to `no` to consider the screen edge alone.

`kwm_edge_best()` answers "which of these two candidates is nearer, the way I am moving"; the
caller supplies everything else. An edge that does not exist on a side is `INT_MIN` or `INT_MAX`,
and the addition and subtraction saturate (`kwm_clip_add()`, `kwm_clip_sub()`) so those values
survive the arithmetic rather than wrapping. A bounded edge always beats an unbounded one. An edge
counts if it lies between where the moving edge is and where it is going: the target inclusive, the
current position exclusive, so an edge the window is already sitting on is not a stopping point.

Two windows that meet side by side, their *opposing* edges touching, end up one gap apart. Two
windows lined up on the same side, *aligned* edges, end up flush. To get that result the
compositor pads the other window's aligned edges by the gap before asking, because the moving
window's own rectangle already includes it.

An edge the compositor reports as not visible is pushed out of bounds rather than dropped, so it
loses every comparison without the search needing a special case. Working out what is visible
needs the scene graph (the compositor's tree of drawn nodes), so that stays in the compositor, as
does the walk that finds candidate edges at all. `libkwm` holds the arithmetic, not the walk.

### More than one screen

The layout is one coordinate space, so a window dragged past the right edge of one screen is on the
next; there is no boundary in the space itself to stop at.

A keyboard move stops at a screen edge before it crosses it. `MoveToEdge` moves the window to the
next edge within the usable area of the screen it is on. Pressed again with the window already
against that edge of the screen, it moves the window to the neighbouring screen in that direction,
against the near edge of that screen, unless the window is maximised, when it stays. With no screen
in that direction, nothing moves. For `Super+arrow` across screens, see
[The transition](#the-transition).

## Occupancy is an input

Whether a workspace is "occupied" is asked by two programs, and they mean different things:

- **The compositor** counts windows that are not on every workspace (not *omnipresent*). A
  minimised window still counts: it has a taskbar button and comes back where it was.
- **The panel** marks the workspace being shown as having windows when a window on any screen is
  not minimised, because the workspace protocol (`ext-workspace-v1`) reports *active*, *urgent*
  and *hidden* but never "there is something here", and the compositor reports a window on another
  workspace as minimised. The panel therefore learns occupancy only for workspaces it has seen
  shown.

Both rules are correct for their program, so `libkwm` implements neither. `kwm_ws_adjacent()` is
told which workspaces are occupied and finds the nearest one in a direction. With wrapping on, it
wraps at most once, so a set of empty workspaces ends the search instead of circling it
forever; with wrapping off, it stops at the end of the list. It returns `-1` when there is none.
The compositor builds the occupancy list for the first 64 workspaces.

In the compositor this search backs the `left-occupied` and `right-occupied` targets of
`GoToDesktop` and `SendToDesktop`. Every target of those two actions, plain or occupied, wraps
from the last workspace to the first unless `wrap="no"` is given. The shipped `rc.xml`
names four workspaces (`main`, `www`, `hack`, `misc`) and uses only the plain `left` and `right`
targets, which step to the neighbouring workspace whether or not it is in use:

| Keys | Action |
|---|---|
| `Super+Page_Up`, `Super+Page_Down` | `GoToDesktop` `left`, `right` |
| `Super+comma`, `Super+period` | `GoToDesktop` `left`, `right` (`wrap="yes"` restates the default) |
| `Super+Shift+comma`, `Super+Shift+period` | `SendToDesktop` `left`, `right` |
| Wheel over the bare desktop, with `desktop_icons = no` | `GoToDesktop` `left`, `right` (a compositor default) |

To skip empty workspaces, bind `to="left-occupied"` and `to="right-occupied"` instead.

## What the model cannot express

Each of these is a deliberate narrowing. They are listed so that nobody spends time looking for the
setting that would widen one.

### Screen layout is an order, not a geometry

`kdos-display`, the screen-arrangement tool (`Super+p`), places screens edge to edge from
`x = 0`, tops aligned, in list order. A vertical arrangement, an overlap or a deliberate gap cannot
be expressed. See [Decisions](../01-philosophy/decisions.md#narrowings).

### How a drag feels

A drag runs screen edges and window edges through the same kind of edge search, and near an edge it
resists or attracts. That is interaction, and it stays in the compositor (`src/resistance.c`).
`<resistance><screenEdgeStrength>` in `rc.xml` (default 20) is the distance in pixels over which
a screen edge holds a window before it goes over; `<windowEdgeStrength>` (default 20) is the same
for window edges. A positive value resists entry, a negative one attracts, and `0` takes that kind
of edge out of the search entirely, so the drag crosses freely. What the library shares is where an
edge *is*, not how it feels to cross one.

### The size a client accepted

A client may ignore the size it is configured with, as a terminal does when it keeps to whole
character cells. The library hands back the rectangle that was asked for and has no notion of the
one that was taken. A client that ignores its configure shows as content cut off at the frame's
edge. For resizing towards an edge, the compositor (`src/snap-constraints.c`) remembers the size
the client chose, so that a second `GrowToEdge` in the same direction goes past the edge the client
stopped short of instead of aiming for it again.

### Where a window was last time

Remembering a rectangle across a close and an open is a question about a *program*, which the
library has no word for. [Window memory](#window-memory) answers it in the compositor.

### A relation between windows

`libkwm` computes rectangles from the space available and the obstacles present; it has no notion
of a neighbour. That is why ownership and tab groups are the compositor's, and why there are no tile
groups: two windows side by side that move, resize and minimise together.

### A window between two places

Every rectangle in this chapter is final from the moment it is computed: a window is never
halfway between two of them, and nothing here has a duration. With `window_motion = yes` in
`comp.conf` the compositor draws a window on its way (rising into place as it opens, sinking as it
closes or is minimised, sliding sideways with a workspace switch; see
[kdos-comp](../04-programs/kdos-comp.md#window-transitions)), but that is a picture over a state
that has already changed. Placement, snapping, the edge search, window memory and occupancy all see
the final rectangle and the final workspace. The one thing that follows the picture is the pointer,
which meets a window where it is drawn.

### Which workspace a window is on

A workspace is a set the compositor keeps, and "on every workspace" (`ToggleOmnipresent`,
`Super+o`) is a flag on a window rather than a number. Nothing asks `libkwm` about it.

## The library's entry points

The library includes nothing but the C library's `<stdlib.h>` and `<limits.h>`, calls no other
KDOS library, and needs no maths library: the placement search compares doubles with plain
arithmetic. That constraint matters because of how the compositor is built.
`kdos-comp`'s `build.sh` compiles `libkbase`, `libkcolor` and `libkwm` into one static archive,
`libkdos.a`, and hands it to meson through `LDFLAGS`; a library here that needed a real `-l`
dependency would have to carry that dependency into the compositor's meson build with it.

Every entry point in the library has a caller in a shipped program:

| Entry point | Called from |
|---|---|
| `kwm_place()` | `kdos-comp`, `src/placement.c`: the overlap search |
| `kwm_tile_next()` | `kdos-comp`, `src/view.c`: the tiled-state transition |
| `kwm_tile_geom()` | `kdos-comp`, `src/view.c`: the rectangle of a tiled state |
| `kwm_edge_check()` | `kdos-comp`, `src/snap.c`: the snap rule |
| `kwm_clip_add()`, `kwm_clip_sub()`, `kwm_edge_best()` | `kdos-comp`, `include/edges.h`: the edge-search arithmetic |
| `kwm_ws_adjacent()` | `kdos-comp`, `src/workspaces.c`: the nearest occupied workspace |
| `kwm_drag_threshold()` | `kdos-desk`, the `kdos-shell` surface that draws the desktop icons (`src/desktop/kdos-shell/desk.c`): when a press on an icon becomes a drag |

`kwm_edge_between()` and the inline helpers `kwm_edge_is_cardinal()` and `kwm_edge_invert()` have
no caller outside the library. They are reached through `kwm_edge_check()` and `kwm_tile_next()`,
and `kwm_edge_between()` is exported so that the contract's `btwn` rows can test it directly. A
rule kept in the library that nothing calls would be a second answer to a question the compositor
already answers, and the contract cannot arbitrate between two copies when only one of them ships.

Window cycling (the window switcher: `Super+Tab` and `Super+Shift+Tab` in the shipped `rc.xml`,
`Alt+Tab` and `Alt+Shift+Tab` from the compositor's built-in bindings) is not in the library.
`kdos-comp` keeps its cycle list as a `wl_list` and steps it by following links, with the list head
as a sentinel so the ring always closes. An index-based library function cannot take a linked list
without turning each step into a scan, and a copy of the rule that nothing calls could drift from
the one that runs without anything noticing.

A drag is measured in cells. `kwm_drag_threshold()` reports a drag as soon as the pointer leaves
the cell it went down in, because the desktop icons are laid out on a character-cell grid and every
geometry there is in cells; a threshold in pixels would make the same gesture pick an icon up at one
scale and only select it at another.

## The contract

`testing/fixtures/wm/geometry.txt` is the contract between `libkwm` and the compositor. Each
section of the file names the function its rows were derived from: a `kdos-comp` function for
every kind except `drag`, whose only caller is `kdos-shell` and which cites `kwm_drag_threshold()`
itself. `testing/selftest.sh` replays the file against `libkwm` (the replay is `test_wm()` in
`src/libs/selftest.c`) rather than asserting anything of its own. To add a case, add a row under
the section that names where the behaviour lives.

The file records what the compositor (and, for `drag`, the desktop icons) does, read off its
source. A row that fails means `libkwm` and the compositor have drifted apart, and the named
function is where to look. Where a rule in the library disagrees with a row, the row is right.

There are 106 rows in eight kinds, counted by the command below:

| Kind | Rows | Covers |
|---|---|---|
| `tile` | 28 | The tiled-state transition |
| `geom` | 26 | The rectangle a tiled state occupies |
| `wsadj` | 12 | The nearest occupied workspace |
| `place` | 10 | The overlap search |
| `btwn` | 9 | Edge search: does an edge lie between here and the target |
| `best` | 9 | Edge search: the nearer of two candidate edges |
| `drag` | 6 | When a press-and-move has become a drag |
| `clip` | 6 | Edge search: saturating addition and subtraction |

One `place` row (`place unusable … -> @unchanged`) states what the caller does rather than what the
library returns: `kwm_place()` is never reached when the output is unusable. The replay counts that
row without driving it, so it exercises 105 rows and accounts for all 106. Most `place` rows assert
an exact position; one asserts only that the answer is not the default corner (`@not-0,0`), because
where a crowded search lands depends on its iteration order and a hand-computed answer would test
the arithmetic of whoever wrote the row.

The replay fails in two further cases. A row whose kind it does not recognise is an error rather
than a silent skip, so a new kind cannot enter the file without a branch that drives it. And fewer
than 80 rows means the file was not found or lost its rows.

Count the rows with:

```sh
grep -vE '^\s*(#|$)' testing/fixtures/wm/geometry.txt | awk '{print $1}' | sort | uniq -c
```

## See also

- [The desktop](../02-user-guide/desktop.md#windows): the window keys and gestures from the user's side
- [The C libraries](../05-developer/c-libraries.md#libkwm): the constraints `libkwm` is built under
- [kdos-comp](../04-programs/kdos-comp.md): the compositor that calls it
- [How KDOS is built](../05-developer/how-kdos-is-built.md#the-desktop-05_desktop): the build phase
  that compiles `libkwm` into the compositor
- [The ports catalogue](../06-reference/ports-catalogue.md#the-desktop-phase): the compositor and
  the rest of the desktop's ports
- [Testing](../05-developer/testing.md): how the contract file is replayed
- [Decisions](../01-philosophy/decisions.md#narrowings): the screen-layout and tab-group narrowings
- [The design language](design-language.md): how a surface answers the pointer, including the wheel

<!-- book-nav -->
---

*Part III — Architecture, chapter 19.* Previous: [18. The design language](design-language.md) · [Contents](../README.md) · Next: [20. The programs](../04-programs/README.md)
