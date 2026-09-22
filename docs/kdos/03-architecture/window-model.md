# The window model

Where a new window lands, what tiling does to it, which edge it stops against,
and which workspace a switch moves to. All of that arithmetic lives outside the
compositor that obeys it, in `libkwm`, which is what lets every rule on this
page be asserted against a fixture with no display anywhere.

## What the library is, and is not

`libkwm` is handed rectangles and told what is being asked. It knows nothing
about windows.

| The library answers | The caller still owns |
|---|---|
| Where a new window lands among the ones already there | What a window *is*, and which output it is on |
| What the tiled state becomes, and what rectangle that state occupies | Whether the view is maximised, and whether a client accepted the size |
| Which other edge a moving edge meets first | Walking the view list, and reading decoration thickness |
| The nearest occupied workspace in one direction | Which windows exist, and what "occupied" means here |

That division is the whole reason the rules can be replayed with no compositor
running. `libkwm` links `libkbase` and no maths library — the same constraint
`libkcolor` and `kcell_ascii.c` are written under.

Window cycling is not in the library. `kdos-comp` holds its cycle list as a
`wl_list` and steps it by following a link, using the list head as a sentinel
so the ring always closes; an index-based signature cannot take a linked list
without turning a pointer hop into a scan. Stating the rule a second time here
would be a copy with no caller, and the contract cannot arbitrate between two
copies when only one of them runs.

## The contract

`testing/fixtures/wm/geometry.txt` is the contract. Every row cites the line of
`kdos-comp` it was derived from, and the self-test **replays the file** rather
than asserting anything of its own. Adding a case means adding a row and citing
its line.

The file is not a description of what the model ought to do. It is a record of
what the compositor does, read off its source. A row that fails means `libkwm`
and the compositor have drifted apart, and the line it cites is where to look.

There are 106 rows in eight kinds:

| Kind | Rows | Covers |
|---|---|---|
| `tile` | 28 | The tiled-state transition |
| `geom` | 26 | The rectangle a tiled state occupies |
| `wsadj` | 12 | The nearest occupied workspace |
| `place` | 10 | The overlap search |
| `btwn` | 9 | Edge-search primitive |
| `best` | 9 | Edge-search primitive |
| `drag` | 6 | Pointer drags ending against an edge |
| `clip` | 6 | Edge-search primitive |

One `place` row states what the *caller* does rather than what the library
returns — `kwm_place` is never reached when the output is unusable — so the
replay drives 105 of them and counts the last, which is how the file and the
replay reconcile.

## Tiling is two steps

What the state becomes is a decision over a bitmask. What that state looks like
is arithmetic over a rectangle. Keeping them apart is what makes either
testable.

The transition splits the current tiled state into the component parallel to
the snap axis and the component orthogonal to it. A half plus an orthogonal
edge is a quarter; a quarter snapped against its own parallel component is the
half that remains.

A quarter snapped towards the edge it already occupies collapses to a half. The
parallel component is then neither the inverse of the request nor absent, so no
branch matches, the request is taken unchanged, and the orthogonal component is
discarded. That reads as a defect and is not.

### The gap arithmetic

The two halves of an axis come from different expressions — `(size + gap) / 2`
and `(size - gap) / 2`. That is what puts a *whole* gap between two tiled
windows rather than half a gap each, and it means an odd dimension gives the
right or bottom half one extra pixel.

A state matching none of the four cardinal bits is the whole usable area inset
by the gap, so the centre state is maximise-shaped rather than centred.

### Opposing edges are not a tile

A state holding **both** edges of an axis collapses that axis. Left sets the
axis's far bound to the midpoint and right sets its near bound to the same
midpoint, so the two together give a width of zero — a negative one once the
margins come off.

That is not a tile the model defines. The transition above never produces an
opposing pair, so the contract fixture has no row for one and none may be
invented. A caller that means "fill the area" must ask for the area, not for
all four edges at once. Maximise is that caller: it keeps the four-edge mask as
its own *state*, so one restore rectangle serves every tile, and computes the
rectangle from the work area itself.

### A drag that ends against an edge

A pointer drag builds its tile from nothing. The caller clears the tiled state
before it asks for the edge the hand landed on, because `kwm_tile_next()`
*composes* — a left-half window dragged to the right edge would otherwise
become a half of a half rather than the right half the gesture asked for.

The top edge alone is the exception and maximises. "Fill the screen" is what
dragging a window to the top of it means everywhere else, and a half the height
of the work area is not a tile anybody asks for by that gesture. Every other
edge and every corner gives the rectangle the matching `Super`+arrow gives, so
the pointer and the keyboard produce the same window.

### One restore rectangle

A window carries a single restore rectangle and it means exactly one thing:
what an untile returns to. It is therefore written only from an *untiled*
rectangle — by a snap, by maximise and by fullscreen alike — and leaving
fullscreen re-derives the tile rectangle from the tiled state instead of
replaying a stored one.

Fullscreen is orthogonal to the tile. A fullscreen that overwrote the restore
rectangle would send the later untile to the tile the window is already in, and
a caller writing that rectangle to a geometry table when the window closes
would carry the loss past the session. Re-deriving is also what lands a window
that was fullscreen across a panel dock or an output resize in the tile the
grid has now.

## Placement is a search, not a cascade

A new window is placed by minimising overlap. An irregular grid is built by
extending the edges of every window on the output to infinity, each interval is
counted for how many windows cover it, and the candidate is convolved across
the grid in four directions. The first position with no overlap at all ends the
search.

With nothing else on the output the grid is empty and the window lands in the
upper-left corner, inset by both the decoration margin and the configured gap.

### The decoration margin has four sides

`KwmBorder` is `top, right, bottom, left` and every caller fills all four,
because a session whose units are not square does not have a square border.
`kdos-comp` measures in pixels and hands in its title bar's height and its
border's width. A caller that passed one number four times would hand the model
a margin its own frames do not have, and every placed window would sit out on
one axis.

Which side of the margin the cost falls on depends on which rectangle is fixed.
A **freely placed** window — landed by the overlap search, dragged, restored
from a saved session, or opened at the rectangle a named layout gives it —
holds its *content* rectangle, and the margin is added outside it: a thicker
border reaches further across the desk and the program keeps every cell of its
content. A window whose **outer** rectangle is the work area or a division of
it — maximised, snapped, tiled — has the same margin subtracted from that fixed
rectangle instead, so its content is twice the margin smaller than the space it
fills.

A margin raised on either axis therefore takes cells from every window in the
second class and from none in the first. That is where the question "what does
a thicker border cost?" has to be asked window by window.

### What a placement keeps on the grid is the frame

A content rectangle wholly on the grid still puts everything the margin adds
outside it — the outer rules, all four corners, and the band a press is read as
a resize in — at a negative row or column. A border drawn nowhere is a window
with no edge to take hold of, no corner to pull and nothing to drag it by.

So every placement road ends at one fit, and the area that fit is given is the
work area **deflated by the margin** for a window the server draws chrome
round, and the whole undeflated area for one it does not: a panel, a layer, the
icon layer, a fullscreen window and the lock. A window too large for what is
left is shrunk to it, which is the second-class cost again.

A named rectangle is fitted like any other. The rectangles in a saved session
are carried between machines and written on a screen of another size, so they
are asked for and not obeyed: one naming a column inside the border lands
against that edge instead of over it.

## A window that belongs to another window

One process is not one window, and a window is not one application. A single
client may map as many toplevels as it likes — an editor's toolbox, its image
window, two docks and a modal file chooser are five windows over one
connection.

The model therefore has a word for the relation between them. A window may name
an **owner**, and one that does may additionally be **modal**. Both are plain
facts about a window, not about what is inside it, and every rule below is
about the relation alone.

### Where an owned window opens

An owned window opens centred on its owner, at the size it asked for — not by
the overlap search. A dialog belongs to the window that raised it, and a search
that put it wherever there happened to be room is a question a person has to
hunt for. It is clamped into the work area afterwards, because an owner may be
at an edge or larger than the area itself.

An owned window is remembered nowhere. It is a *float*, which already means
"opens where the eye is, at its own size, and is recorded in no geometry
table". Every window of one guest carries the same program name, so a file
chooser that was remembered would write its rectangle as the one the document
window opens at next time — to disk, for every session after this one.

### Raising and lowering a family

An owned window rides its owner's raises and ends on top of them. The stack is
a list and raising is a move to the front, so a raise starts at the head of the
family whichever member was named, and moves the family in front of it.

The members keep the order they were already in, the modal above its siblings.
Each move is to the front, so moving them in the order they were found would
turn the family over on every raise, and four docks would swap places each time
their image window came up. A question a person has to answer must not open
behind the dialog beside it.

The window that was named ends on top of the family, which is what makes a
click on one of four docks bring that dock out rather than whichever of them
was in front. A dialog left behind the window it is asking about is an
application that looks frozen; a dialog raised on its own that left its owner
buried is the same picture from the other side. The keyboard still goes to the
window that was named.

A lower is that move read backwards. Every member goes to the **tail**,
deepest-first, so a grandchild is moved before its parent and the owner is
moved last of all — a parent moved first would be buried under its own
children. The modal moves first and therefore ends highest of its siblings, for
the reason it is collected first on the way up. A lower names the head of the
family whichever member was given, as a raise does. labwc's own `Lower` is the
caller, and it is bound to nothing by default.

The chain is followed a few levels and no further, in both directions: a guest
names the owner, and nothing this side can promise the chain has no cycle in
it.

### Putting a family away

An owned window goes where its owner goes. Sending a window to another
workspace sends what it owns, and minimising one minimises them; naming a
dialog instead resolves up to the window it belongs to and moves that. A
question left on a desk its owner has left is a frame belonging to nothing — it
has no row of its own to reach it by — and a stranded modal goes on blocking an
owner the person is looking at somewhere else. A sticky child is not moved,
being on every workspace already.

A run-or-raise chord means the family too. The chord matches on the program
name and every window of one guest carries the same one, so the search answers
with the head of a family and never with the dialog in front of it. A chord
that landed on the question would leave the document it is about behind, and a
second press — which steps to the program's next window — would have nowhere to
step from. What the chord brings back from a minimise is the whole family, the
way the minimise took it away.

### The taskbar and the ring

An owned window carries no taskbar row of its own. One application is one row;
a dialog, a dock and a splash belong to a window that already has one.

A non-modal dialog, a dock and a splash stay in the cycle ring, because one of
those is exactly what a person is switching to. None of them can be minimised
on its own, because the row is the way back from a minimise and these have
none — their frames are drawn without the minimise button for that reason. A
question is answered or closed, not put away. Put the owner away and they go
with it, and they come back on the owner's row.

A modal takes its owner's keyboard and nothing else on the desktop. A raise
aimed at the owner — from the ring, from a click, from the owner's taskbar row
— raises the owner's whole family and then hands the keyboard to the modal
instead of the owner: the focus path resolves sideways through the family
before it sets focus, so the question is what answers every way in. The raise
moves the family together with the modal kept above its owner, so nothing can
come between the two and the owner is never buried under the question it is
asking.

The modal is what leaves the ring, not the owner. The switcher's criteria
exclude a modal dialog, and the owner keeps its ring entry and its taskbar row
— a step that landed on a window whose keyboard is redirected is a step to the
question anyway. Everything else on the desktop carries on, because a modal is
modal to its application and not to the machine.

### When an owner closes

An owner that goes leaves **one** window carrying the row. The orphan nearest
the front takes the owner's place — its row, its place in the ring, no owner of
its own — and the rest are re-parented onto it, so an application that outlives
the window a person opened is still one entry. Giving each of them a row would
turn one entry into six the moment an image window closed, which is the bar
this whole relation exists to stop growing.

Modality does not survive: there is nothing left for it to block. A splash is
never the heir, having no frame to close it by and no place in the ring.

## The edge search is one question

*Moving this edge in this direction, which other edge does it meet first?*
`MoveToEdge`, `GrowToEdge` and `ShrinkToEdge` are all built on that question.
`kwm_edge_best` is the whole of "which of these two candidates is nearer the
way I am pointing", and the caller supplies the rest.

An **opposing** edge keeps the gap and an **aligned** edge does not. The first
is two windows placed beside each other, the second is two windows lined up, so
only the aligned edge is padded.

An edge the caller reports as not visible is pushed out of bounds rather than
dropped, so it loses every comparison without the search needing a case for it.
Working out what is visible needs the scene graph, so that stays with the
caller.

The moving edge sweeps a quadrilateral, and the test is against that
quadrilateral's extent at the obstacle's own offset, interpolated between the
two ends of the move. That test lives in the compositor, next to the
scene-graph walk that supplies the obstacle. `libkwm` holds the arithmetic and
not the sweep.

## Occupancy is an input

Whether a workspace is "occupied" is asked by two programs, and they mean
different things. The compositor counts views that are not omnipresent. The
panel counts windows that are not minimised, because the workspace protocol
reports active, urgent and hidden but never *there is something here*.

Two rules, two right answers. `libkwm` picks neither: it is told which
workspaces are occupied and finds the nearest one, wrapping **at most once** so
that a set of empty workspaces terminates the search rather than circling it.

A minimised window still holds its workspace, because it has a taskbar row and
comes back to where it was, while a panel, a toast and the icon layer are
nobody's work. A step that skipped the empty ones is an arrow nobody presses
twice on a set of nine with two in use.

## What the model cannot express

Each of these is a deliberate narrowing, and each is stated here so that time
is not spent looking for the key that would widen it.

Screen layout is an order, not a geometry. Screens are placed edge to edge
from the left in list order; a vertical arrangement, an overlap or a deliberate
gap cannot be said. What people usually want is an order, and the narrowing is
recorded in [Decisions](../01-philosophy/decisions.md#narrowings).

Heads are positioned from `x = 0` in list order and the output layout is one
coordinate space, so a window dragged past the right edge of one screen is on
the next: there is no boundary in the space itself to stop at.

An arrow never crosses a seam; a drag does. `MoveToEdge` clamps the window
into the usable area of the screen it is already on and moves nothing when it
is already against that edge, so a keyboard nudge stays on one screen however
many are lit. Crossing is the pointer's job: a drag runs the output edges
through the same edge search the window edges go through, and
`screenEdgeStrength` under `<resistance>` is how hard the seam holds the window
before it goes over. Zero takes the output edges out of the search entirely and
the drag crosses freely.

Which pointer event does what is not in the model. `libkwm` is asked for a
rectangle and never for what asked. The rule a caller's router has to keep is
that pointer routing dispatches on the **button** and not on the press kind: a
wheel detent is delivered as a press carrying `KT_MB_WHEEL_UP` or
`KT_MB_WHEEL_DOWN`, and no release ever follows it, so a router that tested the
press kind alone would raise, focus, move, minimise, maximise or close a window
on a scroll. A scroll reaches the window under the pointer and does nothing
else to it: it does not raise it, does not take the keyboard, and does not arm
a move or a resize.

Pointer resistance is not in the model. How a drag feels as it crosses an
edge — the resist and attract zones — is interaction, and it stays in the
compositor with its own validator. What is shared is where an edge *is*, not
how it feels to cross one.

Effective geometry after a refused resize is not in the model either. A client may
ignore the size it is configured with; the library hands back the rectangle
that was asked for and has no word for the one that was taken. The compositor
answers that itself: a client that ignores its configure shows as content cut
off at the frame's edge rather than as any number the model holds. That is a
policy about programs, not arithmetic about rectangles.

Where a window was last time is not in the model. The library places a
rectangle from the space available and the obstacles present; remembering one
across a close and an open is a question about a *program*, which is a thing
the library has no word for. `comp.conf`'s `window_memory` answers it, keeps
its own file, and clamps what it remembered itself — in pixels, against the
usable area of whichever output that box lands nearest.

Nor is a relation between two windows. `libkwm` computes rectangles from
the space available and the obstacles present. It has no word for a neighbour,
which is why there are no tile groups either.

Which workspace a window is on is not in the model. The library places and
fits rectangles; a workspace is a set the compositor keeps, and "on every one
of them" is a flag on a window rather than a number it holds — labwc's
omnipresence is that flag, and nothing asks `libkwm` about it.

## See also

- [The C libraries](../05-developer/c-libraries.md#libkwm) — the constraint it is built under
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor that calls it
- [Testing](../05-developer/testing.md) — how the contract file is replayed
- [Decisions](../01-philosophy/decisions.md#narrowings) — the layout narrowing, stated
