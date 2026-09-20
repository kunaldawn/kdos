# The window model

Where a window goes, what tiling does to it, which edge it stops against, and
what order it is cycled in. **Two desktops obey this model and there is one
implementation of it**, `libkwm` — so a defect in any rule here is one fix, not
two that drift until somebody uses both desktops in the same day.

## What the library is and is not

`libkwm` is handed rectangles and told what is being asked. It knows nothing
about windows.

| It answers | The caller still owns |
|---|---|
| Where a new window lands among the ones already there | What a window *is*, and which output it is on |
| What the tiled state becomes, and what rectangle that state occupies | Whether the view is maximised, and whether a client accepted the size |
| Which other edge a moving edge meets first | Walking the view list, and reading decoration thickness |
| The next index in a ring, and the nearest occupied workspace | Which windows exist, and what "occupied" means here |

That division is the whole reason a compositor drawing pixels and a session
server drawing cells can share it. It links `libkbase` and **no maths library**
— the same constraint `libkcolor` and `kcell_ascii.c` are written under.

**One rule is shared without a shared call, and it is window cycling.**
`kwm_ring_next` steps a ring of `n` by index, which is what `kdos-con` holds;
`kdos-comp` holds its cycle list as a `wl_list` and steps it by following a
link, using the list head as the same sentinel. The rule is identical — the
sentinel between the last item and the first is stepped over, so the ring always
closes — but an index-based signature cannot take a linked list without turning
a pointer hop into a scan of the list. So the compositor keeps its walk, and
what the `ring` rows in the contract hold to account is the library's.

## The contract

`testing/fixtures/wm/geometry.txt`. Every row cites the line of `kdos-comp` it
was derived from, and the self-test **replays the file** rather than asserting
anything of its own. Adding a case means adding a row and citing its line.

The file is not a description of what the model ought to do. It is a record of
what the shipping compositor already does, taken by reading it, so that adopting
`libkwm` is a behaviour change only where a row says the old behaviour was
wrong.

Eleven kinds of row: `tile`, `geom`, `place`, `fit`, `drag`, `ring`, `wsadj`, `gaprule`, and the `clip`, `best` and `btwn`
primitives the edge search is built from.

## Tiling is two steps

**What the state becomes** is a decision over a bitmask, and **what that state
looks like** is arithmetic over a rectangle. Keeping them apart is what makes
either testable.

The transition splits the current tiled state into the component parallel to the
snap axis and the component orthogonal to it. A half plus an orthogonal edge is
a quarter; a quarter snapped against its own parallel component is the half that
remains.

**A quarter snapped towards the edge it already occupies collapses to a half.**
The parallel component is then neither the inverse of the request nor absent, so
no branch matches, the request is taken unchanged, and the orthogonal component
is discarded. That reads as a bug and is not.

**The two halves of an axis come from different expressions** — `(size + gap) / 2`
and `(size - gap) / 2`. That is what puts a *whole* gap between two tiled
windows rather than half a gap each, and it means an odd dimension gives the
right or bottom half one extra pixel.

A state matching none of the four cardinal bits is the whole usable area inset
by the gap, so the centre state is maximise-shaped rather than centred.

**A state holding BOTH edges of an axis collapses that axis.** Left sets the
axis's far bound to the midpoint and right sets its near bound to the same
midpoint, so the two together give a width of zero — a negative one once the
margins come off. That is not a tile the model defines: the transition above
never produces an opposing pair, so the contract fixture has no row for one and
none may be invented for it. **A caller that means "fill the area" must ask for
the area**, not for all four edges at once. `kdos-con`'s maximise is that
caller: it keeps the four-edge mask as its own *state* — so one restore
rectangle serves every tile — and computes the rectangle from the work area
itself.

**A pointer drag that ends against an edge builds its tile from nothing.** The console session
clears the tiled state before it asks for the edge the hand landed on, because `kwm_tile_next()`
*composes* — a left-half window dragged to the right edge would otherwise become a half of a half
rather than the right half the gesture asked for. The TOP edge alone is the exception and maximises:
"fill the screen" is what dragging a window to the top of it means everywhere else, and a half the
height of the work area is not a tile anybody asks for by that gesture. Every other edge and every
corner is the rectangle the matching `Super`+arrow gives, so the pointer and the keyboard produce
the same window.

**One rectangle, two states, and fullscreen is orthogonal to the tile.** A
window carries a single restore rectangle and it means exactly one thing: what
an untile returns to. So it is written only from an *untiled* rectangle — by a
snap, by maximise and by fullscreen alike — and leaving fullscreen re-derives
the tile rectangle from the tiled state instead of replaying a stored one. A
fullscreen that overwrote the restore rectangle would send the later untile to
the tile the window is already in, and the console writes that rectangle to its
geometry table when the window closes, so the loss would outlive the session.
Re-deriving is also what lands a window that was fullscreen across a panel dock
or an output resize in the tile the grid has now, rather than the one it had
then.

## Placement is a search, not a cascade

A new window is placed by **minimising overlap**: an irregular grid is built by
extending the edges of every window on the output to infinity, each interval is
counted for how many windows cover it, and the candidate is convolved across the
grid in four directions. The first position with no overlap at all ends the
search.

With nothing else on the output the grid is empty and the window lands in the
upper-left corner, inset by both the decoration margin and the configured gap.

**The decoration margin has four sides and they are not one number.** `KwmBorder`
is `top, right, bottom, left` and every caller fills all four, because a session
whose units are not square does not have a square border. `kdos-comp` measures in
pixels and hands in its title bar's height and its border's width. `kdos-con`
measures in **cells**, and a cell is twice as tall as it is wide: its border is
`CON_FRAME_X` = 2 columns at the sides and `CON_FRAME_Y` = 1 row top and bottom,
which is the same sixteen pixels of grabbable edge on all four and costs the
scarce axis nothing. A caller that passed one number four times would hand the
model a margin its own frames do not have, and every placed window would sit a
cell out on one axis.

**Which side of the margin the cost falls on depends on which rectangle is
fixed.** A **freely placed** window — landed by the overlap search, dragged,
restored from a saved session, or opened at the rectangle a named layout gives
it — holds its *content* rectangle, and the margin is added outside it: a
thicker border reaches further across the desk and the program keeps every cell
of its content. A window whose **outer** rectangle is the work area or a
division of it — maximised, snapped, tiled, and `kdos-con`'s scratchpad
drop-down — has the same margin subtracted from that fixed rectangle instead,
so its content is twice the margin smaller than the space it fills. On
`kdos-con`'s 80x24 test grid a half-screen snap is 40 columns of frame around 36
columns of terminal, and a maximised window over a hidden taskbar is 80 columns
around 76. **A margin raised on either axis therefore takes cells from every
window in the second class and from none in the first**, which is where the
question "what does a thicker border cost?" has to be asked window by window.

**What a placement has to keep on the grid is the frame, not the content.** A
content rectangle wholly on the grid still puts everything the margin adds
outside it — the outer rules, all four corners, and the band a press is read as
a resize in — at a negative row or column, and a border drawn nowhere is a
window with no edge to take hold of, no corner to pull and nothing to drag it
by. So every placement road in `kdos-con` ends at one fit, and the area that fit
is given is the **grid deflated by the margin** for a window the session draws
chrome round — and the whole grid, undeflated, for one it does not: a panel, a
layer, the icon layer, a fullscreen window, the lock and the saver. A window too
large for what is left is shrunk to it, which is the second-class cost again.

**A named rectangle is fitted like any other.** The rows of a layout file and
the rectangles in a saved session are edited by hand, carried between machines
and written by a session on a screen of another size, so they are asked for and
not obeyed: a row naming a column inside the border lands against that edge
instead of over it. `kdos-con --layout-save` writes only rectangles that were on
a grid, so a layout a session wrote reopens to the cell.

## A window that belongs to another window

**One process is not one window, and a window is not one application.** A `libkcon` client may hold
several surfaces and one `kdos-cage` speaks for as many toplevels as its guest maps — an editor's
toolbox, its image window, two docks and a modal file chooser are five windows over one channel. So
the model has a word for the relation between them: a window may name an **owner**, and one that
does may additionally be **modal**. Both are plain facts about a window, not about what is inside
it, and every rule below is about the relation alone.

**An owned window opens centred on its owner, at the size it asked for.** Not by the overlap search:
a dialog belongs to the window that raised it, and a search that put it wherever there happened to
be room is a question a person has to hunt for. It is clamped into the work area afterwards, because
an owner may be at an edge or larger than the area itself.

**It rides its owner's raises and ends on top of them.** The stack is a list and raising is a move
to the front, so a raise starts at the head of the family whichever member was named, and moves the
family in front of it. **The members keep the order they were already in**, the modal above its
siblings: each move is to the front, so moving them in the order they were found would turn the
family over on every raise and four docks would swap places each time their image window came up —
and a question a person has to answer must not open behind the dialog beside it. **The window that
was named ends on top of the family**, which is what makes a click on one of four docks bring that
dock out rather than whichever of them was in front. A dialog left behind the window it is asking
about is an application that looks frozen, and a dialog raised on its own that left its owner buried
is the same picture from the other side. The keyboard still goes to the window that was named. The
chain is followed a few levels and no further: a guest names the owner and nothing this side can
promise the chain has no cycle in it.

**And it rides its owner's lowers the same way round.** A lower is that move read backwards: every
member goes to the **tail**, deepest-first, so a grandchild is moved before its parent and the owner
is moved last of all — a parent moved first would be buried under its own children, which is the
dialog on the wrong side of the window it is asking about seen from below. The modal moves first and
therefore ends highest of its siblings, for the reason it is collected first on the way up. A lower
names the head of the family whichever member was given, as a raise does. The console desktop's
`Super+b` is the only caller: the compositor has labwc's own `Lower`, bound to nothing.

**It is remembered nowhere.** An owned window is a *float*, which already means "opens where the eye
is, at its own size, and is recorded in no geometry table". Every window of one guest carries the
same program name, so a file chooser that was remembered would write its rectangle as the one the
document window opens at next time — to disk, for every session after this one.

**It goes where its owner goes, and is put away with it.** Sending a window to another workspace
sends what it owns, and minimising one minimises them; naming a dialog instead resolves up to the
window it belongs to and moves that. A question left on a desk its owner has left is a frame
belonging to nothing — it has no row of its own to reach it by — and a stranded modal goes on
blocking an owner the person is looking at somewhere else. The chain is followed a few levels here
too, and a sticky child is not moved, being on every workspace already.

**A run-or-raise chord means the family too.** The chord matches on the program name and every
window of one guest carries the same one, so the search answers with the head of a family and never
with the dialog in front of it — a chord that landed on the question would leave the document it is
about behind, and a second press, which steps to the program's next window, would have nowhere to
step from. What the chord brings back from a minimise is the whole family, the way the minimise took
it away.

**It carries no taskbar row of its own.** One application is one row; a dialog, a dock and a splash
belong to a window that already has one. They stay in the cycle ring, because a dialog is exactly
what a person is switching to — and none of them can be minimised on its own, because the row is
the way back from a minimise and these have none. Their frames are drawn without the minimise
button for that reason: a question is answered or closed, not put away. Put the OWNER away and they
go with it, and they come back on the owner's row.

**A modal blocks its owner and blocks nothing else.** While one is up its owner cannot be raised,
cannot take the keyboard and cannot be closed, and a raise aimed at the owner — from the
directional search, from a number chord, from a click — lands on the modal and flashes it. That
flash is the answer: a click that did nothing at all reads as a desktop that has stopped rather than
as an application waiting to be answered. **A window a raise cannot land on is out of the ring**,
so Alt-Tab and `Super+Alt+`*n* skip the blocked owner and reach the modal instead: a ring entry
whose every step is redirected back to the question is one the ring can never advance past. The
window list keeps the owner's row, because a row is not a step — `Enter` on it is redirected and
flashes the question like any other raise — and a dialog has no row of its own to take its place. Everything else on the desktop carries on, because a modal is modal to its application and not
to the machine. A minimised modal blocks nothing, or a window somebody put out of the way would be a
window with no way back to it. Neither does one off its owner's desk: a question the person cannot
see blocking a window they can is a window that has stopped answering with nothing on the screen to
say why. Every path that moves one of the pair moves the other, so the rule is written once where
the block is decided rather than in each of them.

**An owner that goes leaves ONE window carrying the row.** The orphan nearest the front takes the
owner's place — its row, its place in the ring, no owner of its own — and the rest are
re-parented onto it, so an application that outlives the window a person opened is still one entry.
Giving each of them a row would turn one entry into six the moment an image window closed, which is
the bar this whole relation exists to stop growing. Modality does not survive: there is nothing left
for it to block. A splash is never the heir, having no frame to close it by and no place in the
ring.

## A window that is a tab of another window

**The second inter-window relation, and the last one there is.** A window may name a **stack**: the
id of the tab that is on screen. It is the console session's, not the library's — see [What cannot
be expressed](#what-cannot-be-expressed) — and it is stacking and nothing else. Two windows in a
stack are one rectangle showing one of them at a time, with a strip of tabs along the title row.

**The tab on screen is an ordinary window and every other member is hidden.** The head holds the
geometry, the tile state and the workspace; a member is put away with the same `hidden` flag the
[scratchpad](../04-programs/kdos-con.md#the-scratchpad) uses, which is what buys the whole of the
behaviour with no code of its own: its guest sleeps, it keeps no taskbar row, the cycle ring steps
past it, it claims no cells for a hit test and it carries no window-list row. **A stack is one
window to everything that counts windows.**

**Every member carries the same value, the head's included.** `stack == id` is what makes a window
the one on screen, so the id changes each time a different tab is brought up and a set with one
member left is not a stack at all. **The strip is ordered by id and not by the z-order**: bringing a
tab up moves it to the front of the list, so a strip drawn in list order would reshuffle on every
switch, and ids only ever go up.

**A member also carries a place in the strip, and it is zero until something moves a tab.** The
sort is that number first and the id after, so a strip nobody has reordered is in id order exactly
as it always was. A carry numbers the whole strip 1..n before it swaps two of them, which is what
makes a strip **either entirely explicit or entirely implicit and never half of each** — a member
left at zero would sort ahead of every numbered one, and a member carrying the number it had in
another stack would land in the middle of this one.

**Joining takes the head's rectangle before it hides.** A member is placed through the same call an
ordinary resize goes through — which reflows a terminal and configures a guest — because a member
still configured to its old size would draw the old size into the new rectangle the moment it came
up. The tile state travels with it too, so a snap after a tab switch is measured from what the
stack is in rather than from what the incoming tab was in before it joined.

**The keyboard moves only when the outgoing tab held it.** A stack stepped while somebody is typing
in another window leaves the keyboard where it is; a focus left on the tab that went away would
send the next keystroke to a window drawn nowhere.

**And a stack that loses the tab on screen promotes an heir**, exactly as an owner that goes does.
The frontmost survivor takes the head's rectangle and its place at the head and the rest re-point at
it — a member still naming a head that has gone is a hidden window with no row, no ring step and no
rectangle, which is a window a person can reach by nothing at all.

**A stack IS made by dragging.** A title-bar drag released over another window's title row joins
the two, and the drop is asked before the edge snap: a window let go on a title row against the top
of the work area is one gesture and cannot be both a stack and a maximise. A press on a tab of a
strip brings that tab up; the tab already on screen is not a target, because a press on it is a
hand reaching for the frame.

**What is not built:** chrome, a guest on a terminal of its own and the scratchpad cannot be tabs —
the scratchpad because its own chord shows and hides it through the same flag, so it would be a
window two mechanisms disagreed about. There are no **tile groups**: `tiled` is a per-window bitmask
resolved against the work area and never against a neighbour, and the arrangements clear it
afterwards precisely so that an arrangement is not a state, so a group of windows that move and size
together would reuse none of this. And a tab cannot be dragged **along** its strip: the drag is a
translation with a drop test at the end of it, not a position within a run. Both are in
[Known gaps](../06-reference/known-gaps.md).

## The edge search is one question

*Moving this edge in this direction, which other edge does it meet first?*
`MoveToEdge`, `GrowToEdge` and `ShrinkToEdge` are all built on it, and so is the console desktop's
directional focus — `kwm_edge_best` is the whole of "which of these two candidates is nearer the
way I am pointing", and the caller supplies the rest.

**Directional focus is that question with an overlap test in front of it.** A candidate has to start
past where the focused window starts — its leading edge, not its trailing one, so a window that
merely overlaps a little is still to the right of the one it overlaps — and it has to share rows
with it going sideways, or columns going up and down. Nothing overlapping means the focus does not
move: a window that shares no rows with the focused one is not to its right in any sense a hand
means, and the ring is the way to a window the arrows cannot see.

An **opposing** edge keeps the gap and an **aligned** edge does not — the first
is two windows placed beside each other, the second is two windows lined up — so
only the aligned edge is padded.

An edge the caller reports as not visible is pushed out of bounds rather than
dropped, so it loses every comparison without the search needing a case for it.
Working out what is visible needs the scene graph, so it stays with the caller.

The moving edge sweeps a quadrilateral, and the test is against that
quadrilateral's extent at the obstacle's own offset, interpolated between the
two ends of the move. That test lives in the compositor, next to the
scene-graph walk that supplies the obstacle; `libkwm` holds the arithmetic both
desktops share and not the sweep.

## Occupancy is an input

A workspace being "occupied" is asked by two programs and they mean different
things. The compositor counts views that are not omnipresent. The panel counts
windows that are not minimised, because the workspace protocol reports active,
urgent and hidden but never *there is something here*.

Two rules, two right answers. `libkwm` picks neither: it is told which
workspaces are occupied and finds the nearest one, wrapping **at most once** so
that a set of empty workspaces terminates the search rather than circling it.

The console desktop is the third caller and has a third rule: a minimised window still holds its
workspace, because it has a taskbar row and comes back to where it was, while a panel, a toast and
the icon layer are nobody's work. `Super+PageUp` and `Super+PageDown` step over the empty ones —
with nine workspaces and two in use, an arrow that stopped on every empty one between them is an
arrow nobody presses twice.

## What cannot be expressed

**Screen layout is an order, not a geometry.** Screens are placed edge to edge
from the left in list order; a vertical arrangement, an overlap or a deliberate
gap cannot be said. That is a deliberate narrowing — what people usually want is
an order — and it is recorded in [Known gaps](../06-reference/known-gaps.md).

**The console means it literally.** `libkkms` takes every connected connector in
DRM connector order — the kernel's own, stable across a boot, so a layout does
not rearrange itself depending on which monitor woke up first — and lays their
modes end to end into one virtual box. The grid the session is told about is
that box divided by the cell, so **a window dragged past the right edge of one
screen is on the next** because there was never a boundary in the grid to stop
at: the cut into screens happens at the paint, below everything that knows what
a window is. A screen showing fewer rows than the grid has shows the **top** of
it and is padded, never scaled — every cell is the same size on every screen,
which is what lets a window keep its shape across the seam.

**A moved window stops at the seam once.** With two or more screens lit, a move
runs `kwm_edge_output` over each output's columns before it is fitted to the
work area: the edge that was moving lands on the nearest screen boundary ahead
of it, and the other axis is left alone — a nudge is one direction, and snapping
both would put the window somewhere the arrow was not pointing. A second nudge
crosses. One screen has no seam, so the search is skipped entirely and a move is
the plain one.

**Which pointer event does what is not in the model.** `libkwm` is asked for a
rectangle and never for what asked. The rule the console's router keeps, and
the one any other caller has to keep, is that pointer routing dispatches on the
**button** and not on the press kind: a wheel detent is delivered as a press
carrying `KT_MB_WHEEL_UP` or `KT_MB_WHEEL_DOWN`, and no release ever follows
it, so a router that tested the press kind alone would raise, focus, move,
minimise, maximise or close a window on a scroll. A scroll reaches the window
under the pointer and does nothing else to it — it does not raise it, does not
take the keyboard, and does not arm a move or a resize.

**Pointer resistance is not in the model.** How a drag feels as it crosses an
edge — the resist and attract zones — is interaction, and it stays in the
compositor with its own validator. The two desktops share where an edge *is*,
not how it feels to cross one.

**Effective geometry after a refused resize is not in the model either.** A
client may ignore the size it is configured with; the library hands back the
rectangle that was asked for and has no word for the one that was taken. Each
desktop answers that itself. A terminal window on the console always accepts
what it is given, so the question never arises for one — but an embedded guest
is a compositor of its own, and its frames are always the size of the output the
session chose. A Wayland guest's refusal therefore shows as the cage's
background inside the frame or as content cut off at its edge rather than as any
number the session holds, and an X11 guest's cannot show at all: the cage is
that client's window manager, so its window is the size the cage set it to and
its buffer is that window. The console is told the size the guest asked for on
`KEMBED_SURFACE` — a Wayland guest's own window geometry, an X11 guest's
`ConfigureRequest` — and gives the window *that*, rounded up to whole cells —
under a bound that refuses a report the window already satisfies and caps a run
of resizes the guest alone drove, so one that answers every size with another
demand is followed a few times and then left alone. Which is a policy about
programs, not arithmetic about rectangles, and is why it lives in the session
and not here.

**Where a window was last time is not in the model either.** The library places a rectangle from
the space available and the obstacles present; remembering one across a close and an open is a
question about a *program*, which is a thing the library has no word for. Both desktops answer it
themselves — `comp.conf`'s `window_memory` and `con.conf`'s `remember` — and each keeps its own
file, because one is in pixels and the other in cells. `kwm_fit()` is the shared half: whatever
either of them remembers is fitted back into the area that exists now.

**Nor is a stack.** The console's tabbed windows are a relation between two windows, and `libkwm`
computes rectangles from the space available and the obstacles present — it has no word for a
neighbour, which is the same reason there are no tile groups. A stack is a field on a session's own
window record: nothing about it is shared with `kdos-comp`, which has no tabs, and nothing about it
is written to a saved session.

**Which workspace a window is on is not in the model.** The library places and
fits rectangles; a workspace is a set the session keeps, and "on every one of
them" is a flag on a window rather than a number it holds. The console's
[scratchpad](../04-programs/kdos-con.md#the-scratchpad) is that flag and the
compositor's omnipresence is the same statement, which is why neither desktop
asks `libkwm` about it.

## See also

- [The C libraries](../05-developer/c-libraries.md#libkwm) — the constraint it is built under
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor that calls it
- [Testing](../05-developer/testing.md) — how the contract file is replayed
- [Known gaps](../06-reference/known-gaps.md) — the layout narrowing, stated
