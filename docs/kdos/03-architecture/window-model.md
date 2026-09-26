# The window model

This page explains the rules that decide where a window goes on the KDOS desktop: where a new
window opens, what tiling and snapping do to it, which edge it stops against when you push it, how
dialogs travel with the window that opened them, and which workspace a switch lands on. It is for
anyone who wants to know *why* a window ended up where it did, and for contributors changing the
compositor, `kdos-comp`, or the library that holds its geometry, `libkwm`.

If you only want the keys and mouse gestures, read [The desktop](../02-user-guide/desktop.md#windows)
first; this page explains what those gestures do underneath. Contributors should read
[What the library is, and is not](#what-the-library-is-and-is-not) and
[The contract](#the-contract) before changing any rule here.

## What the library is, and is not

KDOS keeps the geometry of window management — the arithmetic — in a small C library, `libkwm`
(`src/libs/libkwm`), outside the compositor that obeys it. The compositor, `kdos-comp`, is a fork
of the labwc Wayland compositor; see [kdos-comp](../04-programs/kdos-comp.md).

`libkwm` is handed rectangles and told what is being asked. It knows nothing about windows.

Two terms run through the whole page. An *output* is a screen. Its *usable area* is that screen
less the space panels reserve (the panel's strip), and every rectangle below is measured inside it.

| The library answers | The caller still owns |
|---|---|
| Where a new window lands among the ones already there | What a window *is*, and which output it is on |
| What the tiled state becomes, and what rectangle that state occupies | Whether the window is maximised, and whether a client accepted the size |
| Which other edge a moving edge meets first | Walking the window list, and reading how thick the decoration is |
| The nearest occupied workspace in one direction | Which windows exist, and what "occupied" means |
| Whether a press-and-move has become a drag | Which surface the press was on |

That split is what lets every rule be checked against a table of rectangles with no compositor and
no display running. `libkwm` links `libkbase` and nothing else — no maths library either, the same
constraint `libkcolor` and `kcell_ascii.c` are written under. `kdos-comp` compiles `libkbase` and `libkcolor`
into a static archive for its meson build, so a real `-l` dependency here would have to travel with that
archive.

Every rule in the library is reached from a shipped program. `kwm_edge_between()` and the inline
helpers `kwm_edge_is_cardinal()` and `kwm_edge_invert()` have no caller outside the library: they
are reached through `kwm_edge_check()`, and `kwm_edge_between()` is exported so the fixture's `btwn`
rows can test it directly. The entry points programs call:

| Entry point | Called from |
|---|---|
| `kwm_place()` | `kdos-comp`, `src/placement.c` — the overlap search |
| `kwm_tile_next()` | `kdos-comp`, `src/view.c` — the tiled-state transition |
| `kwm_tile_geom()` | `kdos-comp`, `src/view.c` — the rectangle of a tiled state |
| `kwm_edge_check()` | `kdos-comp`, `src/snap.c` — the snap rule |
| `kwm_clip_add()`, `kwm_clip_sub()`, `kwm_edge_best()` | `kdos-comp`, `include/edges.h` — the edge-search arithmetic |
| `kwm_ws_adjacent()` | `kdos-comp`, `src/workspaces.c` — the nearest occupied workspace |
| `kwm_drag_threshold()` | `kdos-desk` (`kdos-shell`, `desk.c`) — when a press on an icon becomes a drag |

Window cycling (the window switcher, `Super+Tab` and `Alt+Tab`) is not in the library. `kdos-comp` keeps its cycle list as a
`wl_list` and steps it by following links, with the list head as a sentinel so the ring always
closes. An index-based library function cannot take a linked list without turning each step into a
scan, and a second copy of the rule that nothing calls could drift from the one that runs without
anything noticing.

## The contract

`testing/fixtures/wm/geometry.txt` is the contract. Each section of it cites the source lines its
rows were derived from, and `testing/selftest.sh` replays the file against `libkwm` (the replay is
in `src/libs/selftest.c`) rather than asserting anything of its own. To add a case, add a row and
cite the line it comes from.

The file records what the compositor *does*, read off its source. It is not a wish list. A row that
fails means `libkwm` and the compositor have drifted apart, and the cited line is where to look.
Where a rule in the library disagrees with a row, the row is right.

There are 106 rows in eight kinds:

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

One `place` row (`place unusable … -> @unchanged`) states what the *caller* does rather than what
the library returns: `kwm_place()` is never reached when the output is unusable. The replay counts
that row without driving it, so it exercises 105 rows and accounts for all 106.

Count the rows yourself with:

```sh
grep -vE '^\s*(#|$)' testing/fixtures/wm/geometry.txt | awk '{print $1}' | sort | uniq -c
```

## Tiling is two steps

Tiling a window answers two separate questions. *What state* does the window go to — left half,
top-right quarter? That is a decision over a bitmask of edges (`kwm_tile_next()`). *What rectangle*
does that state cover on this screen? That is arithmetic over the usable area (`kwm_tile_geom()`).
Keeping them apart is what makes each testable on its own.

The edge bits are `TOP`, `BOTTOM`, `LEFT`, `RIGHT` and `CENTER`, with the same values as the
compositor's own `enum lab_edge`, so the compositor passes its enum straight in.

### The transition

The transition splits the current tiled state into the part *parallel* to the direction you are
snapping and the part *orthogonal* to it. With `Super+Left`, `Super+Right`, `Super+Up` and
`Super+Down` (bound with `combine="yes"` in the shipped `rc.xml`):

| Window is | You press | It becomes | Why |
|---|---|---|---|
| Untiled | `Super+Left` | Left half | The request is taken as it is |
| Left half | `Super+Up` | Top-left quarter | A half plus an orthogonal edge is a quarter |
| Top-left quarter | `Super+Down` | Left half | Snapping away from the edge it holds on that axis drops that edge and leaves the half on the other axis |
| Top-left quarter | `Super+Up` | Top half | See below |
| Left half | `Super+Right` | Right half | The opposite edge crosses over |

The fourth row surprises people. A quarter snapped towards the edge it already occupies collapses
to a half: the parallel part is then neither the opposite of the request nor absent, so no
combining rule applies, the request is taken unchanged and the orthogonal part is dropped. That is
the compositor's behaviour and the fixture pins it.

With `combine` off, the request always wins and a quarter is never built. The shipped `Super+arrow`
bindings ask the compositor to snap *across outputs*: when the window is already tiled to that
edge, it moves the window to the neighbouring screen with the edge inverted, so a left-tiled window snapped left again lands
right-tiled on the screen to its left. With no usable screen there, nothing changes.

A window that is maximised is unmaximised first and takes the requested edge unchanged. That check
is about window state, so it stays in the compositor.

### The gap arithmetic

The two halves of an axis come from different expressions: `(size + gap) / 2` and
`(size - gap) / 2`. That puts one *whole* gap between two tiled windows rather than half a gap
each, and it means an odd dimension gives the right or bottom half one extra pixel. That pixel is
the behaviour, not a rounding error to correct.

The gap is `<core><gap>` in `~/.config/kdos-comp/rc.xml`; KDOS ships it at `0`.

A state that has none of the four edge bits — `CENTER`, or no state at all — is the whole usable
area inset by the gap. So the centre state is shaped like a maximised window, not centred.

### Opposing edges are not a tile

A state holding **both** edges of one axis collapses that axis. Left sets the far bound to the
midpoint and right sets the near bound to the same midpoint, so together they give a width of zero,
and a negative one once the margins come off.

That is not a tile the model defines. The transition never produces an opposing pair, so the
fixture has no row for one and none should be invented. Maximise is not built from all four edges:
the compositor keeps it as its own state (`view->maximized`, per axis) and computes its rectangle
from the usable area directly.

### A drag that ends against an edge

Dragging a window by its title bar and letting go at a screen edge snaps it there. The drag snaps
with combining turned off, so the edge the pointer landed on is taken as it is: a left-half window
dragged to the right edge becomes the right half, not a half of a half. A corner gives the matching
quarter.

The top edge alone is different: it maximises. That is `<snapping><topMaximize>` in `rc.xml`,
default `yes`. Dragging a
window to the top of the screen means "fill the screen" on almost every desktop, and a top half is
not what anyone asks for with that gesture.

### One restore rectangle

Each window keeps a single *restore rectangle* (labwc's "natural geometry"): the size and place an
untile, unmaximise or un-fullscreen returns to. It is written only from an *untiled* rectangle —
snapping, maximising and going fullscreen all refuse to overwrite it while the window is tiled or
fullscreen — and leaving fullscreen re-derives a tiled window's rectangle from its tiled state
rather than replaying a stored one.

Fullscreen is independent of the tile. If going fullscreen overwrote the restore rectangle, a later
untile would return the window to the tile it is already in. Re-deriving on the way out is also
what puts a window that was fullscreen across a panel change or a screen resize into the tile the
new layout gives.

## Placement is a search, not a cascade

A new window that did not place itself is placed by minimising overlap with the windows already on
that screen. This is labwc's `Automatic` placement policy, the default; `<placement><policy>` in
`rc.xml` also accepts `Center`, `Cursor` and `Cascade`, and only `Automatic` uses `libkwm`.

The search works like this:

1. Every window on that output, on the current workspace, is taken as a box, already enlarged by its own decoration.
2. Their edges are extended across the whole screen, which divides it into an irregular grid.
3. Each cell of that grid is counted for how many windows cover it.
4. The new window is slid across the grid in four directions, and the first position with no
   overlap at all ends the search. Otherwise the position with the least overlap wins.

With nothing else on the output — or if the grid cannot be allocated — the window lands in the
upper-left corner, inset by both its decoration margin and the gap.

### The decoration margin has four sides

`KwmBorder` is `top, right, bottom, left`, and every caller fills in all four. `kdos-comp` measures
in pixels and hands in its title bar's height and its border's width, which are not the same
number. A caller that passed one number four times would give the model a margin its own frames do
not have, and every placed window would be off on one axis.

Where the margin's cost falls depends on which rectangle is fixed:

- A **freely placed** window — placed by the search, dragged, or restored by window memory — keeps
  its *content* rectangle, and the frame is added outside it. A thicker border reaches further
  across the desktop, and the program keeps all of its content.
- A window whose **outer** rectangle is fixed — maximised or tiled — has the margin subtracted from
  that rectangle, so its content is smaller than the space it fills by twice the margin.

So a thicker border costs space in every maximised and tiled window, and none in a floating one.

### A placement keeps the frame on the screen

A content rectangle that fits the usable area can still put its title bar and borders off the edge.
A frame drawn off-screen is a window with nothing to grab: no title bar to drag, no border to
resize. So:

- a placed or restored window is moved until its title bar starts inside the usable area — its
  content's top-left corner is at least the margin in from the usable area's top-left corner;
- a window too large for the usable area, less the margin on each side, is shrunk to fit, which is
  the tiled-window cost again.

Window memory (`window_memory` in `~/.config/kdos/comp.conf`, on by default) saves each
application's rectangle at close and puts it back at the next open. A remembered rectangle may come
from a screen of another size, so it is clamped into the usable area of whichever screen it lands
nearest rather than obeyed as it is.

## A window that belongs to another window

One program is not one window. An image editor's toolbox, its image window, two docks and a modal
file chooser can be five windows over one connection. The compositor therefore has a word for how
they relate:

- A window may have an **owner** (a *parent*): the Wayland `xdg_toplevel.set_parent` request, or
  X11's transient-for hint under Xwayland. A window with an owner is called *owned* below, and the
  owner with everything it owns is a *family*.
- An owned window may also be **modal**: the `xdg-dialog-v1` modal flag, or X11's modal state. A
  modal dialog blocks its owner until it is answered.

This relation belongs to the compositor. `libkwm` never sees it.

### Where an owned window opens

A Wayland window with an owner opens centred on its owner, on the owner's screen, at the size it
asked for, shrunk if that is larger than the usable area — not by the overlap search. A dialog belongs next to the window that raised it, not
wherever there happened to be room.

Window memory neither records nor restores a window with an owner. Every window of one program
carries the same application id, so a remembered file chooser would be saved as the rectangle the
program's main window opens at next time.

### Raising, lowering and minimising a family

| You do this | What happens |
|---|---|
| Raise any member (click, focus, the switcher) | The owner is raised first, then every owned window in the order they were already stacked, then the window you named goes on top of them all. Nothing else can come between a dialog and its owner |
| Focus any member when the family has a modal dialog | The keyboard goes to the modal dialog, not to the window you clicked. The question is what answers every way in |
| Lower (the `Lower` action, bound to no key by default) | The owned windows go to the bottom, then the owner below them |
| Minimise any member | The whole family is minimised, whichever member asked. Restoring brings the whole family back |

Owned windows keep their stacking order among themselves on a raise because the children are
collected back to front and each is moved to the front in turn.

### What a family does not share

These act on one window, not on its family:

- **Workspaces.** `SendToDesktop` (`Super+Shift+1`…`9`) moves only the window it names. Its
  dialogs stay where they were.
- **The taskbar.** The panel lists every window with its own button, owned ones included; it does
  not read the ownership the compositor reports.
- **The window switcher.** Modal dialogs are left out of the switcher's ring; the owner stays in it,
  and focusing the owner hands the keyboard to the dialog anyway. Non-modal owned windows (a
  toolbox, a dock) stay in the ring, because they are exactly what someone switches to.

A modal dialog is modal to its application, not to the machine: every other window carries on.

### Tab groups

A separate relation, not an ownership: the compositor can **stack** windows as tabs. The
`AddToTabGroup` action folds the focused window onto the one behind it, `RemoveFromTabGroup` takes
it out, and `NextInTabGroup` steps through the tabs; clicking a tab on the title bar shows that
member. Hidden members are minimised windows sharing the shown member's rectangle. None of the
three actions is bound to a key in the shipped `rc.xml`. Tabs stack; they do not tile — see
[Decisions](../01-philosophy/decisions.md#narrowings).

## The edge search is one question

*If this edge moves in this direction, which other edge does it meet first?* Three keyboard
commands are built on that question:

| Command | Shipped keys | What it does |
|---|---|---|
| `MoveToEdge` | `Super+Alt+arrow` | Moves the window until it meets the next edge |
| `GrowToEdge` | `Super+Ctrl+arrow` | Moves the window's leading edge out to the next edge |
| `ShrinkToEdge` | not bound | Pulls the window's trailing edge in to the next edge |

`kwm_edge_best()` answers "which of these two candidates is nearer, the way I am moving"; the
caller supplies everything else. An edge that does not exist on a side is `INT_MIN` or `INT_MAX`,
and the addition and subtraction saturate (`kwm_clip_add()`, `kwm_clip_sub()`) so those values
survive the arithmetic rather than wrapping. An edge counts if it lies between where the moving edge
is and where it is going — the target inclusive, the current position exclusive, so an edge you are
already sitting on is not a stopping point.

Two windows that meet side by side — their **opposing** edges touching — end up one gap apart. Two
windows lined up on the same side — **aligned** edges — end up flush. To get that result the search
pads the other window's aligned edges by the gap, because the moving window's own rectangle already
includes it.

An edge the compositor reports as not visible is pushed out of bounds rather than dropped, so it
loses every comparison without the search needing a special case. Working out what is visible
needs the scene graph, so that stays in the compositor, as does the walk that finds candidate edges
at all. `libkwm` holds the arithmetic, not the walk.

## Occupancy is an input

Whether a workspace is "occupied" is asked by two programs, and they mean different things:

- **The compositor** counts windows that are not on every workspace (not *omnipresent*). A
  minimised window still counts: it has a taskbar button and comes back where it was.
- **The panel** marks a workspace as having windows when a window on it is not minimised, because
  the workspace protocol (`ext-workspace-v1`) reports *active*, *urgent* and *hidden* but never
  "there is something here". It learns this only for workspaces it has seen shown.

Two rules, two right answers. `libkwm` picks neither. `kwm_ws_adjacent()` is told which workspaces
are occupied and finds the nearest one in a direction, wrapping **at most once**, so a set of empty
workspaces ends the search instead of circling it forever. It returns `-1` when there is none.

In the compositor this search backs the `left-occupied` and `right-occupied` targets of
`GoToDesktop` and `SendToDesktop`. The shipped `rc.xml` has four workspaces (`main`, `www`,
`hack`, `misc`) and binds `Super+Page_Up` and `Super+Page_Down` to the plain `left` and `right`
targets, which step to the neighbouring workspace whether or not it is in use. To skip empty ones,
bind `to="left-occupied"` and `to="right-occupied"` instead.

## What the model cannot express

Each of these is a deliberate narrowing. They are listed so that nobody spends time looking for the
setting that would widen one.

**Screen layout is an order, not a geometry.** `kdos-display` places screens edge to edge from
`x = 0`, tops aligned, in list order. A vertical arrangement, an overlap or a deliberate gap cannot
be expressed. See [Decisions](../01-philosophy/decisions.md#narrowings).

**The layout is one coordinate space**, so a window dragged past the right edge of one screen is on
the next; there is no boundary in the space itself to stop at.

**A keyboard move stops at a screen edge before it crosses it.** `MoveToEdge` (`Super+Alt+arrow`)
moves the window to the next edge within the usable area of the screen it is on. Pressed again with
the window already against that edge of the screen, it moves the window to the neighbouring screen
in that direction — unless the window is maximised, when it stays. `Super+arrow` on a window already
tiled to that edge moves it to the neighbouring screen with the edge inverted, as described under
[The transition](#the-transition). With no screen in that direction, nothing moves. A drag runs
the screen edges through the same edge search the window edges go through, and
`<resistance><screenEdgeStrength>` in `rc.xml` (default 20) is how hard a screen edge holds the
window before it goes over; `<windowEdgeStrength>` (default 20) is the same for window edges. `0`
takes that kind of edge out of the search entirely, and the drag crosses freely.

**How a drag feels is not in the model.** The resist-and-attract behaviour near an edge is
interaction, and it stays in the compositor (`src/resistance.c`). What the library shares is where
an edge *is*, not how it feels to cross one.

**The size a client accepted is not in the model.** A client may ignore the size it is configured
with; the library hands back the rectangle that was asked for and has no notion of the one that was
taken. A client that ignores its configure shows as content cut off at the frame's edge.

**Where a window was last time is not in the model.** Remembering a rectangle across a close and
an open is a question about a *program*, which the library has no word for. Window memory answers
it in the compositor (`src/kdos-winpos.c`), in its own file, `$XDG_STATE_HOME/kdos/winpos`.

**A relation between windows is not in the model.** `libkwm` computes rectangles from the space
available and the obstacles present; it has no notion of a neighbour. That is why ownership and tab
groups are the compositor's, and why there are no tile groups — two windows side by side that move,
resize and minimise together.

**Which workspace a window is on is not in the model.** A workspace is a set the compositor keeps,
and "on every workspace" (`ToggleOmnipresent`, `Super+o`) is a flag on a window rather than a
number. Nothing asks `libkwm` about it.

## See also

- [The desktop](../02-user-guide/desktop.md#windows) — the window keys and gestures from the user's side
- [The C libraries](../05-developer/c-libraries.md#libkwm) — the constraints `libkwm` is built under
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor that calls it
- [Testing](../05-developer/testing.md) — how the contract file is replayed
- [Decisions](../01-philosophy/decisions.md#narrowings) — the screen-layout and tab-group narrowings
- [The design language](design-language.md) — how a surface answers the pointer, including the wheel
