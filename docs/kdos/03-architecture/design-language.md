# The design language

Every surface KDOS ships is a grid of character cells drawn in one palette, and they all agree.
This page is the specification that makes that true: the window frame, the chrome primitives,
colour, the pointer contract, hit maps, the glyph tiers, and the checklist a new surface is not
finished without.

It is a rule, not a taste. When one surface does not follow it, the result does not read as a
variant — it reads as somebody else's program dropped into the middle of this one.

For how to build a surface, see
[Writing desktop software](../05-developer/writing-desktop-software.md).

## Everything is a grid of character cells

There is no scene graph, no widget hierarchy with its own layout engine, and no vector rendering.
A surface is a two-dimensional buffer of cells; each cell holds a character, a foreground slot, a
background slot and a small attribute set. Drawing is writing cells; presenting is diffing against
what was last shown and sending only what changed.

The same buffer is painted by three different backends — a terminal, a Wayland surface, and an
offscreen dump — and **nothing above that line knows which**. That is what makes the resource
monitor identical on `tty1`, in a window, and in a committed test fixture.

The consequence to internalise: **a control is as tall as one row of text**. Where that is genuinely
wrong, the answer is a picture drawn into whole cells, not a second renderer. See
[Pictures](#pictures-are-an-enhancement-layer).

**And nothing on this desktop is drawn by another toolkit.** The last thing that was is the
input-method candidate window: an engine draws its own with its own renderer, which on a character
grid is a rounded antialiased panel sitting on top of a text-mode desktop. `kdos-ime` draws it here
instead — the same chrome, the same slots, one program on both desktops — by speaking the
input-method framework's own generic panel protocol rather than by writing an input method.

## A window is a double-line box

```
╔══ Resources ══════════════════════╗
║                                   ║
║  body starts at column 1          ║
║                                   ║
╚═══════════════════════════════════╝
```

Column 0, the last column and the last row belong to the frame. The body starts at column 1 and
stops one row short of the bottom. The title hangs on the top edge.

**The header band draws between column 1 and the second-to-last column**, so a surface that draws
a header without a box around it has a one-column margin with nothing in it — which is the
give-away that a box was forgotten.

A surface too small for a frame keeps the whole area rather than drawing a box with nothing
inside it.

**Under the compositor, the server-side decoration *is* that box**, so a toplevel window
suppresses its own drawn frame and keeps only the inset. Two boxes nested one inside the other is
the tell that a program drew chrome the compositor had already drawn for it.

## The chrome primitives

Chrome is drawn by `libkchrome` and never by a second copy. Two implementations of a button bar
are two button bars, and the one nobody is looking at is the one that drifts.

| Primitive | Draws |
|---|---|
| `kch_header` | The accent band and its subject line |
| `kch_group` | A heading inside the body |
| `kch_buttons` | The verb bar |
| `kch_list_wheel`, `kch_list_clamp` | The scrolling rule |
| `kch_scrollbar` and its drag family | A scrollbar that can be dragged |
| `kch_tile_*` | A block of cells drawn as pixels |
| `kch_tone`, `kch_slot_rgb` | Palette-derived shading |

**The controls are `libktui`'s and there is one of each.** A surface that drew its own is a second
answer to a shape this desktop has already settled, and the one nobody is looking at is the one that
drifts:

| Control | Is |
|---|---|
| `ktui_button`, `ktui_check`, `ktui_radio` | A verb, a flag, one of a set |
| `ktui_slider` | **A number on a track** — press, drag, wheel, or an end cap for one step |
| `ktui_dropdown_*` | A choice, as a list that opens under its row |
| `ktui_input` | A line of text with a caret |
| `ktui_list`, `ktui_table` | Rows, and rows with columns |
| `ktui_rows_*` | Which row a pointer is on, for a surface that draws its own |
| `ktui_table_event` | The same, for a table: wheel, press, pick, Back |
| `ktui_tabs_*` | A strip of pages |
| `KtuiMenu`, `ktui_modal` | A pane of verbs, and a question |
| `ktui_progress`, `ktui_gauge`, `ktui_sparkline`, `ktui_heat` | What is happening, in four shapes |

**Each is a `draw`/`key`/`hit` trio with one frame control on top of it**, which is what lets a
surface that runs its own event loop — `kdos-settings`, `kdos-display` — use the same control as one
written inside `ktui_frame_begin()`. A control that existed only inside the frame is a control those
surfaces write a second time.

**EVERYTHING THE KEYBOARD CAN DO, THE POINTER CAN DO.** Not "most things": a verb reachable by a
chord and by nothing else is a verb that does not exist for somebody using a mouse, and the shipped
key card is not a thing anybody reads first. The rule binds both ways — every control here answers
arrows and `Enter` as well as a press — and it is why the window menu carries every per-window verb
rather than the memorable half of them, and why a number in a settings form is a track and not a
printed string.

**`if (ev.type != KT_EVT_KEY) continue;` is how that rule is broken, and eleven surfaces had it.**
The accent picker, the character map, the calculator, the command palette, the trash, the file
search, the contacts list, the notes pad, the recorder and both viewers each dropped every pointer
event before anything could be asked of it — so a person could look at seven colour schemes and
choose none of them. A surface that draws its own rows uses `ktui_rows_event` rather than inventing
an answer: a press MOVES the caret, a press on the row the caret is already on PICKS, the wheel
walks, the right button is Back. One implementation, because eleven would be eleven desktops.

**A button bar drops buttons; it does not vanish.** Buttons are ordered most-useful-first and
dropped from the **right** when the surface is too narrow, never half-drawn. A bar that
disappeared entirely would fall back to a row of key hints — which is what the buttons exist to
replace.

**Anything that overdraws part of a row owns all of it.** A button bar shares its row with the
status line, and the single column *between* two buttons belongs to neither — so a leftover glyph
shows through the gap. The bar clears its whole span first, and the status text is clipped to
what is left.

**A hint row is drawn whole or not at all**; a fragment of one sentence against the start of
another is worse than an empty half-row. A **message** takes whatever room there is, because it is
what the user just did.

## The keys every surface answers

Five keys mean the same thing on every surface here, and the bottom row says what the rest of them
do **right now**.

| Key | What it does | Where it comes from |
|---|---|---|
| `F1` | Opens this surface's page in `kdos-doc` | `KtuiKeys.doc`, drawn first in the row |
| `F10` | Opens the surface's menu bar | `KtuiMenu.has_bar` |
| `Shift+F10` | Opens the menu of the thing under the caret | `KtuiKeys.ctx_at` |
| `Alt+letter` | Opens a menu pane by its underlined letter | the `&` in a pane title |
| `Esc` | Steps back **one** level, and only then closes | the Esc ladder |

**A key with nothing behind it is neither advertised nor answered.** A surface with no page in
`/usr/share/kdos/doc` leaves `doc` NULL, and `F1` then returns PASS: a key that opens an index
reading *no such document* teaches that help is broken, which is worse than never offering it.
`testing/preflight.sh` refuses a `.doc` naming a file that does not ship. The same rule holds for
`F10` on a surface with no bar and `Shift+F10` on one with nothing focused.

**Esc is a ladder of declared layers, asked at the instant the key arrives and never cached.** A
surface registers each raised state once with `ktui_keys_layer()`, outermost first, and
`ktui_keys()` takes exactly one rung per press. A dialog dismissed by a click leaves no raised bit
behind — the predicate is a question, not a flag — which is the defect that makes a hand-written
ladder swallow the next keystroke. `ktui_esc_verb()` names the topmost open rung, so the row
cannot read *Esc Close* on a screen where Escape goes back.

**The row is pushed during the draw, by whatever holds the focus.** `ktui_hint(key, verb)` and
`ktui_hint_if(cond, …)` push; `ktui_hint_row()` draws and clears the pool. A fixed string cannot
follow the focus, and a row naming keys the focused control does not answer is worse than no row:
the whole value of the line is that a key on it works.

**The row drops its tail.** A hint that does not fit takes every hint after it, and `Esc` is
pushed last — so on a narrow surface it is the first thing lost. Where a surface is too narrow for
everything it answers, drop a hint deliberately rather than reordering `Esc`; widening the surface
is the other honest fix.

**A menu's `sel` indexes its items, never its drawn rows.** A caller's `show` callback hides rows,
and a selection counted in drawn rows lands on a different item the moment one is hidden. The same
callback answers the drawing and the hit test, from one walk: two copies of a visibility rule
disagree eventually, and a click then runs the row above the one under the pointer.

**An accelerator is marked, not assumed.** `&` before a letter in a label or a pane title marks
it; `&&` is a literal ampersand. The letter is underlined where the tier has underline and
bracketed where it does not — a Linux VT has none, and drawing one there puts an unowned colour on
the screen.

## Colour

Colour comes from a **slot**, never from a literal value:

| Slot | Role |
|---|---|
| `KT_ACCENT` | The accent |
| `KT_ERR` | Urgent |
| `KT_WARN` | Secondary — a caution, not a failure |
| `KT_TEXT` | Body text |
| `KT_MID` | Labels, secondary text, borders |
| `KT_DIM` | **A fill** — see below |
| `KT_SURFACE` | Raised chrome background |
| `KT_BG` | The background |

The values behind those slots are one table in `libkcolor`, expanded at compile time by everything
that draws. Nobody keeps a second copy of the numbers, which is why one word repaints the whole
desktop.

**The one exception is a colour that is not ours to name.** A program running in a terminal may ask
for a 24-bit colour, or one of the 216-colour cube: no palette names those, nothing about them
follows an accent, and reducing them to eight slots is what loses a photograph. Such a cell carries
the literal **beside** the slot it reduces to, and only terminal content ever does. **Chrome is
slots, always** — a piece of chrome holding a literal is a piece of chrome that stops following
`kdos theme`, and the ANSI sixteen stay slots for the same reason: they are colours this desktop's
palette names.

**The second exception is translucency, and it is a mix of two slots rather than a colour of its
own.** A grid holds one colour per cell and nothing behind it, so seeing through a window is a pass
over the rectangle after it is drawn: `ktui_draw_bg_take()` copies the background colours that were
there, and `ktui_draw_blend()` mixes what has since been drawn back towards them. The result is
written as the cell's literal and the slot is left exactly as the caller drew it, so a display that
declined the colour run — a `--tty` view, a golden, a braille reader — shows an opaque window, which
is the honest answer where there is nothing to mix with. **Both ends of the mix come out of the
palette in force and it is recomputed every frame**, so a retint moves it like everything drawn in
slots; that is why this is not the rule above being broken. **Backgrounds only** — a translucent
glyph is a glyph nobody can read — and a sprite cell is skipped, because a picture's pixels are not
a background. `window_opacity` and `panel_opacity` in `con.conf` are where a person asks for it, and
`Super+Ctrl+=` / `Super+Ctrl+-` / `Super+Ctrl+Alt+0` are where one window disagrees; on a compositor
a surface asks through `KDispConfig.opacity`, which dims one slot in `libkcell` and takes an alpha
buffer instead.

**The drop shadow is the same mechanism.** It darkens rather than erases: every cell under the
one-cell strip keeps its glyph and both halves are mixed towards `KT_BG`, so the window underneath
stays legible and an embedded application's picture is left alone entirely. The background slot goes
to `KT_BG` beside the literal, and that is the whole of the shadow where the colour run was
declined. A shadow that wrote a blank cut a rectangular bite out of whatever it fell on, which reads
as a compositing defect rather than as depth.

Two rules on top of that, both of which have shipped as defects:

### `KT_DIM` is a fill, not a label colour

Measured against the palette, `KT_DIM` as a **foreground** lands around **1.5–1.7:1** on both the
surface and the background, in every accent. `KT_MID` is **3.4–5.4:1**. There is no reading below
about 3:1.

So **a label is `KT_MID`**. Hint rows, empty-state messages, help text and the brackets around a
button all belong there. Searching for `KT_DIM` in a foreground position is how a new surface is
checked.

**A two-state colour is a different question.** Where `KT_DIM` is one branch of a pair — a
scrollbar's track against its thumb, an unpinned star against a pinned one — it is carrying the
*distinction*, and flattening it onto `KT_MID` deletes the state rather than making it readable.
Those pairs stay `KT_DIM`.

For text that must be muted **and readable**, there is a derived mixed colour that measures around
**4.1–4.7:1**. Every muted *text* role uses it. `KT_DIM` keeps fills, borders and selection
backgrounds, where contrast is not the question.

### Emphasis is a fill with swapped slots

**Never a reverse attribute over a label.** The attribute inverts only the cells a glyph covers,
so a two-word name comes out as one lit block per word with a hole between them, and looks
correct for every name that happens to have no space in it.

Fill the rectangle, swap the foreground and background slots, then draw the text.

### A mark on a fill is a shape, not a contrast

A glyph on a filled plate is read by the plate **around** it. The dark slots of this palette are
one colour to the eye — `KT_BG` measures **1.00–1.20:1** against `KT_SURFACE` in every scheme — so
dark ink on a bright fill cannot be told from the chrome behind the fill by its colour, whichever
of the two it is drawn in. What separates them is the fill enclosing the mark.

**So a mark sitting on a fill is a glyph its cell holds clear of every edge, and the clearance is
the whole of the boundary.** The mark is drawn in the slot that fills the body below the plate — on
a focused frame chip, `KT_SURFACE` both times — so the strip of plate between ink and cell floor
is the only thing dividing the mark from the body, and a thin strip reads as the plate ending early
rather than as carrying a mark. It is a threshold, not an absolute: in the console's `ter-kdos32n`,
`_`
is 24 lit pixels of 512 on rows 27–28 of 32, which leaves **three** rows of plate at the floor, and
it is the one shape a fill cannot hold. `■` is 108 pixels on rows 10–21, `X` is 80 on rows 6–25 and
`↓` is 68 on rows 6–25: centred masses keeping **six or more** rows clear above and below, which is
why those three are the window frame's chips. Six of thirty-two holds; three does not.

The same measurement is why the ink on a bright fill is dark at all: `KT_TEXT` is **1.10:1** on
`KT_ACCENT` and **2.17:1** on `KT_ERR`, so a bright glyph on a lit or urgent plate is a plate with
nothing drawn on it. `KT_SURFACE` clears **3.4:1** on `KT_MID`, **5.2:1** on `KT_ERR` and
**10.4:1** on `KT_ACCENT`.

## The pointer contract

Every surface answers the pointer, and answers it the same way. A surface with rows and no motion
handler is a *picture*: the only way to discover that a row is a control is to click one.

| Gesture | Means |
|---|---|
| Motion | Lights what is under it, and selects a list row |
| Left press | Activates. On a row that is already selected, opens it |
| Wheel | Steps the cursor while the list **fits**; moves the **viewport** when it does not |
| Right press | Backs out one level, then closes |
| Scrollbar | Is **dragged**, not merely looked at |
| Column header | Sets the sort; a second press reverses it |
| Click away | Closes a transient surface |

### Drawing the pointer

**The view draws it, never the session.** The session owns the windows and the view owns the
screen, so a pointer the session drew would cost a round trip for every motion event and trail the
hand moving it. The view already holds the device and already knows where it is.

**The pointer is an arrow where there are pixels and the reversed cell everywhere else.** `libkkms`
composites an arrow into the framebuffer it already owns and puts it at the DEVICE'S OWN PIXEL, so
it moves as smoothly as the hand holding it; a `--tty` view, a `--dump`, a view forwarded over `ssh`
and `tty1` reverse the cell under it, which is the pointer every text mode has drawn and is as fine
as a grid of characters goes. **The reversed cell is not a fallback anything may drop**
— `a11y = yes` runs this desktop on a `--tty` view precisely so `brltty` can read `/dev/vcsa`, and
a session looked at through two views at once is pointed at through both.

`ktui_draw_cursor(x, y)` names the cell; `ktui_draw_flush()` decides which of the two is drawn, and
how it does it is the whole of the contract:

- **A backend with pixels claims the pointer through `KtuiBackend.pointer`**, and answering `1` is
  a promise that the cells reach the screen exactly as the session composed them. `libkkms` is the
  only backend that answers; every other leaves the entry `NULL` and gets the reverse. It cannot be
  drawn in `libktui`: that library links nothing but musl and has to keep doing so, and an arrow
  needs a pixel buffer and a colour in it.
- **The hook is called on every flush, including the ones with no pointer to report.** A negative
  `x` is no pointer at all, and it is the only thing that tells a backend to take the last arrow
  off the screen — one told nothing leaves an arrow at the last place the hand was.
- **The hook carries cells, and a backend that owns the device draws finer than that.** `libkkms`
  reports a cooked motion only when the *cell* changes, so a hook handed pixels would be handed the
  same pixel until it did — what the hook says is that there IS a pointer and which cell the session
  believes it is on, which is what decides whose it is to draw. `libkkms` then draws at its own
  `ptr_px`/`ptr_py`, the position it read off the device, and its flush repaints on a move of one
  pixel. The cell is still the truth for a pointer this library did not move: a finger's cell comes
  from the touch recogniser and leaves the device position where the mouse last was, so the arrow
  snaps to the named cell whenever the two disagree about which cell they are in.
- **The view presents AFTER it drains its input, not before.** The cell the session is told about
  and the pixel the arrow is drawn at come out of the same queue, so a frame flushed first is a
  frame drawn one cell behind the hand.

And where the reverse is what gets drawn:

- **The reverse goes on for the flush and comes straight back off.** `back` is where the session's
  cells accumulate and it survives between frames, so a reverse left in it is a stain the next
  frame draws around — one per cell the pointer was ever over.
- **Taking it off again is what erases it.** `front` keeps the reversed cell and `back` does not,
  so the cell differs and repaints as itself the moment the pointer leaves. One XOR does both jobs.
- **A cell with no glyph still honours the reverse.** A space and a control cell carry colour and
  nothing to draw, and the fill pass has already painted each in its background slot — so a painter
  that skips them loses the swap, and the pointer becomes visible only where it happens to sit over
  text. `kcell_paint` fills those cells with the foreground slot instead.
- **Over an embedded guest's own pixels nothing is drawn at all — one pointer at a time, arrow or
  reverse.** A guest's window is a pixel surface somebody else is compositing, and that compositor
  draws a cursor into the frames this desktop pastes in. A second pointer a cell from the first is
  one too many, and the one a person aims a two-pixel scrollbar with is the guest's.
  `ktui_draw_flush()` applies the rule *before* it offers the hook, so a backend with pixels and one
  without are told the same thing about the same cell: the hook is sent a negative `x`, and the cell
  is left alone — no XOR, no blank, the picture intact.
- **The cell says so itself, and the view must also hold the picture.** `KT_A_GUEST` is set by the
  session on a guest's content cells and on nothing else — but the bit says the session drew that
  cell from a guest's pixels, not that this view has them. A `--tty` view, a view with no pixel
  library, a dump and a view over `ssh` all draw the sprite's fallback *mark*, and there is no
  composited cursor on a mark to collide with; suppressing the pointer there would leave that view
  with none at all over the one window a person most needs to aim at, and that view is the
  accessible one. So the flush asks both: the bit, and `ktui_sprite_get()` for a picture this view
  actually carries. With both, the flush reads a fact rather than guessing
  from the pixels. It is a bit in the low attribute byte, which the eight-byte per-cell run carries
  unconditionally — a bit above the eighth reaches only a view that asked for the colour run, and a
  view that declined it would draw two pointers. A guess made instead from how often a sprite slot
  is re-registered reads a guest that is merely *still* as one that is not there: a hand resting on
  a window makes no damage, so no frame is published and no slot is re-registered, and the pointer
  comes back on top of a cursor that is still on the screen — which is exactly the moment a person
  clicks.
- **A desktop icon, a panel icon and an image in a terminal keep the pointer.** They are sprites
  with pixels exactly as a guest's block is, and nothing draws a cursor on any of them, so they
  carry no bit. Holding over those would take the pointer off the icon grid, which is where it is
  needed most.
- **A guest that has not drawn keeps it too.** A block the session had no sprite slot for is a shade
  mark with no bit, so a window waiting on its first frame is still a window a person can point at.
- **A guest that hides its own cursor has none inside its window**, which is what hiding it means;
  a player and a game do it on purpose. The chrome round the window is drawn in cells and carries no
  bit, so the pointer is never more than one cell from visible — and that border is where a window
  is grabbed, moved and resized.
- **A sprite with no pixels is not a picture at all, and keeps the pointer.** A tty, a view with no
  pixel library and a dump all carry the sprite's fallback *mark*, and nothing there is drawing a
  cursor. `ktui_sprite_get()` answers `NULL` for a slot with no picture.
  The mark is then set aside exactly as a glyph is: the flush swaps the cell's character for a
  blank alongside the XOR and puts it back afterwards, because reverse under a sprite is a fill the
  painter composites over. Inverting a picture's own pixels instead would be wrong — `KT_A_REVERSE`
  is also how a selected row is drawn, and a panel icon on a hovered row would come out in negative.
- **Motion is a change even when no cell's content is**, so the framebuffer is marked dirty for it.
  Otherwise the pointer moves only when something else on the screen happens to. On the arrow path
  that is two jobs and both are `libkkms`'s: the cells under the *old* position are put back into
  the row diff, which is the only thing that erases an arrow the cell model does not know is there,
  and the move carries the frame past the nothing-changed exit so it is presented at all. Skipping
  either leaves a trail of arrows down the screen, one per place the hand stopped.
- **The arrow is drawn in code and scaled to the cell**, an 11x18 mask at a whole number of pixels
  each so it is about one cell tall at every font size — the same footprint the reversed cell has.
  The body is `KT_TEXT` and the outline `KT_BG`: most of any screen is the background slot, so a
  foreground body is legible over all of it without relying on one pixel of anything, and the
  outline is what rescues it over the minority that is foreground-coloured. The outline is computed
  from the mask rather than written into it, because an outline maintained by hand grows a hole and
  a hole is exactly where the arrow vanishes.

Nothing is drawn before the first motion — the named cell starts at no cell at all — so a machine
with no pointing device does not wear a pointer in its corner for the life of the session.

**A guest that draws no cursor shows none, and that is the application speaking.** A full-screen
player and a game hide the pointer on purpose, and the mark is on the cell whatever the guest chose
to draw there. The chrome round an embedded window is cells and carries no mark, so this desktop's
pointer returns the moment the hand reaches the border — which is where a window is grabbed, moved
and resized. It is never more than one cell from visible.

The sub-cell offsets in the wire format, biased so that zero is the centre of a cell, are not for
this. They are for the one thing on the desktop that can be pointed at more finely than a cell: an
embedded pixel guest, which is told where inside the cell the press landed. **Motion finer than a
cell is carried by the raw stream and by nothing else.** `libkkms` reports a cooked event only when
the cell changes — nothing this desktop ROUTES on is finer than a cell — and emits the pixel and
the delta for *every* device sample beside it, so a guest is aimed at the pixel. Sending the dropped
motion as a cooked event too would deliver one movement twice to every guest, once as a pixel and
once as the middle of a cell it is already inside. The ARROW is not routing: it is drawn by the
backend that owns the device, out of the same `ptr_px` the raw stream carries, so it moves a pixel
at a time while the cooked stream steps cells.

### Pointer state: the window answers, not the pointer

**The pointer does not change shape.** Every other desktop answers "what happens if I press here"
by turning the arrow into a double arrow. This one cannot promise that: the same session is drawn
as an arrow on a screen with a framebuffer and as a reversed cell on a `--tty` view, a `--dump` and
the far end of an `ssh` forward, and a cell has no room for a second shape. A shape published per
motion would also be a commit per motion — the round trip the pointer is drawn by the view to
avoid — and the view that drew it is the one place that does not know what is under the pointer.

**So the window answers instead.** The same hit test the press asks — `win_grab_at()` in
`kdos-con` — is asked with the left button standing in for the press that has not happened, and
what it answers is lit on the frame:

| What a press would arm | What lights | Unicode / ASCII tier |
|---|---|---|
| A move — the title row, or Super anywhere inside | the frame's four corners, as studs | `■` / `#` |
| A resize taking the left or right edge | that whole border, along its length | `◀` `▶` / `<` `>` |
| A resize taking the bottom edge | the bottom row | `↓` / `v` |
| A resize taking the top edge | the two top corners, so the title keeps its row | `↑` / `^` |
| Nothing — a frame chip, a panel, a fullscreen window, the desktop | nothing | |

One answer between the light and the drag is what stops them disagreeing, and it is what keeps the
frame chips dark: `↓`, `■` and `X` sit on the title row's right end, which is a corner arm, so a
grip taken from the geometry alone would promise a resize over the cell that closes the window.
A chip is lit by the pointer, not by the grip. The two sets collide on **two** glyphs and both
readings agree: `↓` on a chip is the window going down to the taskbar and `↓` along the bottom row
is that edge about to be dragged down; `■` on a chip is the window filling the screen as a block
and `■` on the frame's four corners is the move grip. The studs are still unambiguous — they are
`KT_ACCENT` on the frame's own background rather than dark ink on a fill, they are drawn only while
a grab is armed under the pointer, and they sit on the corner cells, which the chip run never
reaches: `btn_run` stops one column short of the frame's right edge, so a frame being moved reads
`↓ ■ X ■` across the title row with only the last mark lit.

**What lights is what a press arms, cell for cell.** `win_grab_arms()` hands the grip the same two
arm lengths `win_grab_at()` measures a press against, so a single-axis run stops where the corner
arms begin. A run that lit a whole side would promise a one-axis resize over the cells at each end
that in fact take two, and a hand that trusted it would find the window changing width as well as
height. A corner grab lights the two arms that take both axes with `■` on the cell they share — the
one cell where an arrow would promise a single axis. It says more than a pointer shape can, and it
changes only when the pointer crosses a zone,
which is a commit every few seconds rather than one every few milliseconds. The glyphs come from
`ktui_glyph`, so both tiers are covered by the table that already chooses them, and the ink is
`KT_ACCENT` on the frame's own background — `KT_SURFACE` on a window rung for attention, which is
filled in `KT_ACCENT` and would otherwise swallow the grip whole.

**A drag in progress outranks the pointer.** The window follows the hand, so the pointer is off the
border from the first cell of the drag; the grip follows the *grab* while one is held, or it would
go out at the instant it began to mean something. Under a lock, a saver, a mark, a pick or a guest
that has taken the pointer nothing is lit, because no press reaches a frame in those modes and a
lit edge would promise a drag that cannot start.

**Over text the pointer is already the text pointer**, and over a terminal the reverse is what
marks a selection out, so neither carries a shape of its own.

## Touch

A touchscreen answers the same contract, because **one recogniser turns a finger into the pointer
events above**. `ktui_gesture_feed` in `libktui` is fed by `wl_touch` on the graphical desktop and
by `libinput` on the console; a disambiguator written inside a backend would be written twice and
would disagree twice.

| Gesture | Reported as | And also arrives as |
|---|---|---|
| Tap | `KT_GEST_TAP` | a left press and release |
| Long press | `KT_GEST_LONG` | nothing — a surface that wants a context menu reads the gesture |
| Drag | `KT_GEST_DRAG` | motion with the button held |
| Two-finger scroll | `KT_GEST_SCROLL` | one wheel tick per row the pair's centroid crosses |
| Pinch | `KT_GEST_PINCH` | nothing |
| Edge swipe | `KT_GEST_SWIPE_EDGE` | motion, and it says which edge it came from |

So **a surface written before touch existed already works under a finger**, and one that wants more
reads `ev.gesture` on a `KT_EVT_TOUCH`.

Two rules that are each a defect if missed:

- **Movement is measured in CELLS.** A drag begins when the finger leaves the cell it went down in.
  Coarse on purpose: everything here is a grid, and a threshold in pixels is a number the console
  cannot see. The same threshold decides when a pointer press became a drag, from `libkwm`, so both
  desktops pick a file up on the same gesture.
- **One finger of two says nothing.** Moving away from a stationary finger is a pinch and a scroll
  at the same time; the answer arrives when the second finger agrees or disagrees. Guessing makes
  the two flip back and forth mid-gesture, which is unusable.
- **Only the FIRST finger of a sequence synthesises a pointer press, and the second closes it.**
  A press under every finger clicks whatever that finger landed on, so putting two down to scroll
  would select two rows of a list and a pinch over a button would activate it. Presses and
  releases stay paired: the release goes out on the finger that opened the capture, and a finger
  that never opened one closes nothing when it lifts.
- **The synthesised wheel is counted at the CENTROID, one click per row.** Touch arrives a finger
  at a time, so a click per finger delta reports the same row of travel twice and the page scrolls
  twice as far as the hand moved. The gesture's own `dy` keeps the per-finger delta, which is what
  a surface forwarding the gesture needs.

Long press has no event to arrive on — the finger is down and nothing is moving — so it is polled
with `ktui_gesture_tick` from the backend's idle wait, and reported **once**. **Both backends poll
it**: `libkwl` from the Wayland loop and `libkkms` from the KMS one, with the same
`CLOCK_MONOTONIC` milliseconds the recogniser was fed — a deadline compared against a different
clock never expires, and nothing says so.

**A long press is `Shift+F10` with a finger, and the contract answers it.** `ktui_keys()` opens the
surface's context pane on `KT_GEST_LONG`, so a surface that declared one inherits touch without a
touch path of its own — and it opens **at the finger**, not where `ctx_at` says the keyboard's focus
is drawn, because a person holding a row expects the menu on that row. A surface that refuses a
context menu refuses a finger too, and a **tap** opens nothing: a tap is a click, every widget
already handles one, and a tap that opened a menu would put a pane under every finger.

## Drops

A drop is a position **and** a payload, and an event has room for one of them. `KT_EVT_DROP`
carries where; `ktui_drop_take` yields the payload, once, so two surfaces in one process cannot both
act on one drop.

`text/uri-list` arrives as it came — CRLF-separated URIs, comment lines and all — because what a URI
means differs per surface.

Three subtleties that are each a defect if missed:

- **Motion arrives as a drag event.** The Wayland protocol reports plain movement and dragged
  movement identically, so testing an event's "pressed" flag for truth makes every mouse *move* a
  click. The **button state** is what must be remembered across events, which is why a slider
  arms on press, tracks on motion, and commits on release.
- **A motion that did not move is not a motion.** An absolute pointing device sends the position
  again with a wheel event, so a naive handler steps the cursor and then immediately puts it back
  under a pointer that has not moved a pixel. Motion is dropped when the **cell** is unchanged.
- **One press, not a double click.** Nothing in this toolkit measures a double click, and opening
  a detail view is not destructive — the destructive verbs are behind a confirmation.

**The wheel has exactly one implementation.** A list that fits gets a cursor step; a list that
scrolls gets its viewport moved with the cursor left where it was. The flag that says "pull the
selection into view" is set by everything that **moves the cursor** and by nothing that scrolls the
page — otherwise the next frame undoes the scroll.

**And a list that scrolls says so.** A scrollbar is one column, drawn in the low glyph tier so it
renders identically on a console. Nothing is drawn when everything fits, because a full-height
thumb says exactly what no bar says. Where rows would reach that column, the **rows** give up a
cell rather than the bar overdrawing them: a selected row is a filled highlight, and a bar drawn on
top of it puts a notch in it.

## Hit maps

**A hit map is recorded from what was drawn.** It is never derived a second time from the geometry
the draw computed.

Both copies are right until the window is resized, a sidebar collapses or a border is added — and
then a click lands on the row above the one under the pointer. On a network dialog that means
joining the wrong network.

Two supporting rules:

- **The frame subtracts its body origin once** and hands a page its own coordinates. A page that
  subtracts an origin itself is the second copy.
- **A control with no room records an empty span.** A hit map that outlives what it describes is
  how a narrow screen mutes itself when somebody aims at the clock.

## The glyph tiers

Ramps and box drawing come from one of three tables, chosen from the terminal's capabilities:

| Tier | When | Ramp | Levels |
|---|---|---|---|
| rich | UTF-8, not a console | Eighth blocks | 8 |
| **vt** | UTF-8 **on a Linux console** | `░▒█` | 3 |
| ascii | Neither | `.:#` | 3 |

**The vt tier exists because the console font is 512 glyphs.** A glyph the font does not carry
renders as a **blank** on `tty1` — so an eighth-block bar there is not ugly, it is invisible. Three
levels is the honest resolution of that font.

What the console font **has**: `░ ▒ █`, the single and double box-drawing sets, and
`· • ■ … ° ↑ ↓ ◀ ▶`.

What it **does not have**: eighth blocks, half blocks, `▓`, braille, and **`← →`** — which is why
the shared glyph table carries `◀ ▶` instead.

**Anything that can reach `tty1` stays inside the vt tier.** Rich-tier ramps are for a surface
that only ever runs under the compositor's font renderer. Check the font's character list before
using a glyph that is not on the list above.

**A wide glyph is measured, not assumed.** The toolkit computes display width and reserves a
continuation cell, so double-width text does not corrupt row layout. The console font carries none
of those characters and every one of its cells is one column wide, so on `tty1` a wide codepoint is
written as `?` in a single cell and its continuation as a space: the row keeps its columns and the
character is what is lost. Chrome that must read on both surfaces stays in the small set.

## Pictures are an enhancement layer

Icons and pixel tiles are drawn into **whole cells**, with the slot and sub-cell position encoded
in the cell itself — so the ordinary row diff is already the damage mechanism, and a text backend
renders the fallback character.

**Every consumer must draw correctly when the picture is unavailable.** The icon lookup answers
"none" on a terminal, when icons are disabled, when there is no icon atlas, and when the sprite
table is full — and each of those is a normal state, not an error. The test harness stubs it to
exactly that, so a committed reference frame is the **character grid**: a layout that only lines
up once the pictures load is a layout that is broken.

Two rules for pixel tiles:

- **A tile owns two slots and alternates between them.** A cell encodes the *slot*, not the
  picture, so redrawing a tile's contents in place changes no cell, the diff sees nothing, and the
  frame is never presented — a clock tile would freeze at the minute it was first drawn. Swapping
  slots on every content change repaints exactly the rows it covers.
- **The geometry is decided before the tile is claimed.** Bailing out after claiming it leaves the
  tile believing it drew that content, and the next frame presents a stale slot.

**A NEW PICTURE IN THE SAME SLOT CHANGES NO CELL**, and that is what `ktui_draw_dirty()` exists for.
A sprite cell encodes the *slot*, not the picture, so an animation's next frame writes bytes
identical to the last one and the flush's row diff finds nothing to send — the screen would hold the
first frame of every embedded application for ever. Marking the rectangle the slot covers costs that
rectangle; the alternative, a whole-screen repaint per arriving tile, costs every glyph on the
desktop and a full framebuffer upload dozens of times a second.

**A backend that keeps its own previous frame has to implement `dirty` — and mark every copy it
keeps.** `libkkms` keeps one per *screen* where libktui's is per *session*, and it overwrites
libktui's from the new frame before diffing. `libkwl` keeps three: the cells the compositor is
showing, which is what the damage rectangles are cut from, and one shadow per shm buffer, which is
what the paint diffs against — mark only the shadows and the pixels land in a buffer nobody is told
to re-read; mark only the screen copy and the damage names rows nothing repainted. Spoiling only
libktui's copy changes nothing such a backend reads at all. Every one of these failures is silent
and looks exactly like a guest that stopped drawing. A backend with no `dirty` is one that diffs
against the `prev` it is handed, and needs nothing more.

**A picture from a terminal is the same sprite**, which is why nothing new was invented to draw
one. A picture wider or taller than sixteen cells becomes a grid of sprites sharing a key prefix,
all-or-nothing, so it evicts and re-registers as a unit rather than leaving three quarters of a
photograph on the screen. Each backend does what it can:

| Backend | What a picture is |
|---|---|
| Wayland, KMS | Real pixels, scaled to the cells it occupies |
| The console wire | The bytes, forwarded once per slot; the **display** scales them to its own cell size, because a client has no way to know what that is |
| A tty, or a display built without a pixel library | The fallback shade, in every cell of the picture |

**A picture that renders as nothing is worse than one that renders as a mark.** Blank cells are
indistinguishable from output that never arrived, which is why the tiled path carries a fallback
codepoint rather than a space.

**A tile that IS a control carries the control's state in its sprite cells' background slot.** The
backend fills a cell's background before compositing the picture over it, so that slot is the body
of the button wherever there is no pixel layer to record a plate into. `KT_SURFACE` is the right
answer only where there is one — it is the slot the backdrop owns, so the recorded plate shows
through — and on a character grid it is the bar's own body: no fill, no edges, and a hover or an
open-menu state the surface has already computed painted out by the very draw that should show it.
The slot then takes the same ladder the pixel plate draws: quiet fill at rest, accent under the
pointer, warning while the control's own menu is up, with the ink following the fill. The Start
button is the case, and the rule is the tile's — an icon sitting *inside* a wider field says its
state with the field.

**A sprite is two cells wide and one tall** wherever it sits beside text. A cell is twice as tall
as it is wide, so two cells across one row is a square on the same optical line as the text. Asking
for a two-cell-wide, two-row box next to a single row of text centres the picture across the
boundary between rows and makes the whole surface look misaligned.

## The compositor's decoration is part of the set

The window frame is the one piece of chrome a KDOS program does not draw for itself, so it is
themed to match rather than left as the upstream compositor's:

- **Square corners.** A radius is the one thing a cell grid cannot express, so it is the one thing
  that gives a server-drawn frame away.
- **A two-pixel accent border**, because a hairline disappears beside a 32-pixel cell.
- **A title bar carrying the same double rule the grid draws with**, broken by the title and by
  each button — so it reads as `════ Title ════[_][=][X]`.
- **Buttons are small bitmaps enlarged by a whole number with nearest-neighbour filtering**, so
  they are hard-edged cells rather than smeared glyphs. **The marks are the compositor's own, not
  the grid's** — a bar, a framed box, a cross and a rule stack, against the console's `↓ ■ X` — and
  the grounds are what part them. A button image is one colour on a titlebar of another
  (`window.active.button.unpressed.image.color` is the accent; the titlebar behind it is not), so
  the minimise bar is a bar against its ground with the single row of eight it keeps clear beneath
  it. A chip's mark is `KT_SURFACE`, which is the slot the frame body under it is filled with —
  1.00:1, the same slot — so its only boundary is the plate inside the cell, and `_` leaves three
  rows of thirty-two there, which reads as the plate ending early. Hence `↓` on the grid and a bar
  here.
- **The hover plate carries an alpha.** An opaque colour laid over the button image paints the
  symbol out, leaving every button blank under the pointer — the one moment a button most needs to
  say what it is.

All of it is generated from the same palette as everything else, and retints on the same signal.

## The checklist

A new surface is not finished until every line is answered.

1. `ktui_draw_box` around it, title on the top edge.
2. `kch_header` for the band, `kch_group` for headings, `kch_buttons` for verbs.
3. Slots for colour; `KT_MID` for labels; fills for emphasis, never a reverse attribute.
4. Motion, press, wheel, scrollbar and header-sort — **all five**.
5. Hit map recorded from the draw; coordinates handed down by the frame.
6. `--dump` at 80x24 and 132x43, with a reference frame committed for both.
7. Read it back at the **vt** tier before believing it reads on `tty1`.
8. One `KtuiKeys`, `ktui_keys()` first in the dispatch and `ktui_hint_row()` last in the draw —
   on **every** path, the `--dump` one included, because the row is what clears the pool.
9. Every raised state declared with `ktui_keys_layer()` rather than written into an `Esc` arm.

`grep -c KT_EVT_MOUSE` returning zero for a new file is the same defect four surfaces have
shipped with.

## See also

- [Writing desktop software](../05-developer/writing-desktop-software.md) — implementing all of this
- [The C libraries](../05-developer/c-libraries.md) — what draws it
- [Theming](../02-user-guide/theming.md) — the palettes these slots resolve to
- [kdos-shell](../04-programs/kdos-shell.md) — the largest set of surfaces following this page
- [kdos-comp](../04-programs/kdos-comp.md) — the generated frame theme
