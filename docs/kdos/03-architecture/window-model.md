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

Six sections: `tile`, `geom`, `place`, and the `clip`, `best` and `btwn`
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

## Placement is a search, not a cascade

A new window is placed by **minimising overlap**: an irregular grid is built by
extending the edges of every window on the output to infinity, each interval is
counted for how many windows cover it, and the candidate is convolved across the
grid in four directions. The first position with no overlap at all ends the
search.

With nothing else on the output the grid is empty and the window lands in the
upper-left corner, inset by both the decoration margin and the configured gap.

## The edge search is one question

*Moving this edge in this direction, which other edge does it meet first?*
`MoveToEdge`, `GrowToEdge` and `ShrinkToEdge` are all built on it.

An **opposing** edge keeps the gap and an **aligned** edge does not — the first
is two windows placed beside each other, the second is two windows lined up — so
only the aligned edge is padded.

An edge the caller reports as not visible is pushed out of bounds rather than
dropped, so it loses every comparison without the search needing a case for it.
Working out what is visible needs the scene graph, so it stays with the caller.

The moving edge sweeps a quadrilateral, and the test is against that
quadrilateral's extent at the obstacle's own offset. It interpolates with
doubles: plain arithmetic that calls nothing, so the no-maths-library rule holds.

## Occupancy is an input

A workspace being "occupied" is asked by two programs and they mean different
things. The compositor counts views that are not omnipresent. The panel counts
windows that are not minimised, because the workspace protocol reports active,
urgent and hidden but never *there is something here*.

Two rules, two right answers. `libkwm` picks neither: it is told which
workspaces are occupied and finds the nearest one, wrapping **at most once** so
that a set of empty workspaces terminates the search rather than circling it.

## What cannot be expressed

**Screen layout is an order, not a geometry.** Screens are placed edge to edge
from the left in list order; a vertical arrangement, an overlap or a deliberate
gap cannot be said. That is a deliberate narrowing — what people usually want is
an order — and it is recorded in [Known gaps](../06-reference/known-gaps.md).

**Pointer resistance is not in the model.** How a drag feels as it crosses an
edge — the resist and attract zones — is interaction, and it stays in the
compositor with its own validator. The two desktops share where an edge *is*,
not how it feels to cross one.

**Effective geometry after a refused resize is not in the model either.** A
Wayland client may ignore the size it is configured with, and remembering what
was asked for is a Wayland problem; a terminal window on the console always
accepts the size it is given.

## See also

- [The C libraries](../05-developer/c-libraries.md#libkwm) — the constraint it is built under
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor that calls it
- [Testing](../05-developer/testing.md) — how the contract file is replayed
- [Known gaps](../06-reference/known-gaps.md) — the layout narrowing, stated
