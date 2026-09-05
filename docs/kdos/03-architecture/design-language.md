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
| `KT_WARN` | Urgent |
| `KT_TEXT` | Body text |
| `KT_MID` | Labels, secondary text, borders |
| `KT_DIM` | **A fill** — see below |
| `KT_SURFACE` | Raised chrome background |
| `KT_BG` | The background |

The values behind those slots are one table in `libkcolor`, expanded at compile time by everything
that draws. Nobody keeps a second copy of the numbers, which is why one word repaints the whole
desktop.

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

**The pointer is the cell under it, reversed** — on a screen of its own, in a terminal, and over
`ssh` alike. It is the pointer every text mode has drawn. It needs no artwork, no pixel layer and
no second code path, and a person looking at the same session through two displays sees the same
picture in both.

`ktui_draw_cursor(x, y)` names the cell; `ktui_draw_flush()` does the drawing, and how it does it
is the whole of the contract:

- **The reverse goes on for the flush and comes straight back off.** `back` is where the session's
  cells accumulate and it survives between frames, so a reverse left in it is a stain the next
  frame draws around — one per cell the pointer was ever over.
- **Taking it off again is what erases it.** `front` keeps the reversed cell and `back` does not,
  so the cell differs and repaints as itself the moment the pointer leaves. One XOR does both jobs.
- **A cell with no glyph still honours the reverse.** A space and a control cell carry colour and
  nothing to draw, and the fill pass has already painted each in its background slot — so a painter
  that skips them loses the swap, and the pointer becomes visible only where it happens to sit over
  text. `kcell_paint` fills those cells with the foreground slot instead.
- **Motion is a change even when no cell's content is**, so the framebuffer is marked dirty for it.
  Otherwise the pointer moves only when something else on the screen happens to.

Nothing is drawn before the first motion — the named cell starts at no cell at all — so a machine
with no pointing device does not wear a pointer in its corner for the life of the session.

The sub-cell offsets in the wire format, biased so that zero is the centre of a cell, are not for
this. They are for the one thing on the desktop that can be pointed at more finely than a cell: an
embedded pixel guest, which is told where inside the cell the press landed.

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
| Two-finger scroll | `KT_GEST_SCROLL` | a wheel tick |
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

Long press has no event to arrive on — the finger is down and nothing is moving — so it is polled
with `ktui_gesture_tick` from the backend's idle wait, and reported **once**.

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
continuation cell, so double-width text does not corrupt row layout — but the console font carries
none of those characters, so what renders on `tty1` is nothing at all. Chrome that must read on
both surfaces stays in the small set.

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
  they are hard-edged cells rather than smeared glyphs.
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
