# The design language

This page is the visual and interaction specification every KDOS surface follows: the panel, the
Start menu, the settings, the resource monitor, the installer, the lock screen and every popup.
It covers the window frame, the shared controls, colour, the keys and pointer gestures every
surface answers, touch, the glyph sets that decide what can be drawn on a text console, pictures,
and the frame the compositor draws round other programs' windows.

It is written for people building or reviewing a KDOS surface, and for anyone curious why the
desktop looks the way it does. If you are about to write a surface, read
[Every surface is a grid of character cells](#every-surface-is-a-grid-of-character-cells) and
[The checklist](#the-checklist) first, then
[Writing desktop software](../05-developer/writing-desktop-software.md) for the code.

The rules here are not a matter of taste. A surface that ignores them does not read as a variant;
it reads as somebody else's program dropped into the middle of this desktop.

## Every surface is a grid of character cells

A KDOS surface has no scene graph, no widget tree with its own layout engine and no vector
drawing. It is a two-dimensional buffer of *cells*. Each cell holds one character, a foreground
colour slot, a background colour slot and a few attributes (bold, underline, reverse). Drawing
means writing cells. Presenting means comparing the buffer with what was last shown and sending
only the cells that changed.

The same buffer is painted to three kinds of destination — a terminal, a Wayland window and an
offscreen text dump — and nothing above that layer knows which one it is talking to. Each
destination is a *backend* (a `KtuiBackend`): the terminal one is in `libktui`, the Wayland one in
`libkwl`, and the console one, which draws on the first text console, in `libkdisp`. That is why
the resource monitor looks the same on the first text console (`tty1`), in a window, and in a
committed test file.

The consequence to keep in mind: **a control is one row of text tall.** Where that is genuinely
wrong, the answer is a picture drawn into whole cells, not a second renderer. See
[Pictures are an enhancement layer](#pictures-are-an-enhancement-layer).

Two things on the screen are not cells:

- **The compositor's own chrome** — window title bars, the root menu and the window-switcher
  display — is drawn by the compositor with Pango, in `Terminus (TTF)` at 24 points (32 pixels at
  96 dpi, one cell tall), so the desktop still reads as one machine. See
  [The compositor's decoration is part of the set](#the-compositors-decoration-is-part-of-the-set).
- **A boxed application** (one running in a container) draws whatever its toolkit draws — arbitrary
  pixels — inside a frame this desktop owns.

No KDOS surface is drawn by another toolkit. The input-method candidate window shows how far that
goes: an input-method engine would draw its own with its own renderer, a rounded antialiased panel
on top of a text-mode desktop. `kdos-ime` draws it instead, with the same chrome and colour slots
as everything else, by speaking the input-method framework's generic panel protocol rather than by
being an input method.

## A window is a double-line box

```
╔══[ Resources ]════════════════════╗
║                                   ║
║  body starts at column 1          ║
║                                   ║
╚═══════════════════════════════════╝
```

`ktui_draw_box` draws it. The title is bracketed, not just padded with spaces: two blanks either
side of a title make the rule look broken where it stops, and the eye reads a missing segment
rather than a label. The box adds the brackets and the padding itself and trims whatever spaces
the caller passed, so `" Settings "` and `"Settings"` draw the same.

- Column 0, the last column and the last row belong to the frame. The body starts at column 1 and
  stops one row short of the bottom. The title hangs on the top edge.
- The header band draws from column 1 to the second-to-last column. A surface that draws a header
  with no box round it therefore shows an empty one-column margin, which is the sign that a box
  was forgotten.
- A surface too small for a frame keeps the whole area rather than drawing a box with nothing
  inside it.
- Under the compositor, the server-side decoration takes the box's place: a top-level window skips
  its own drawn frame and keeps only the inset. Two nested boxes on screen mean a program drew
  chrome the compositor had already drawn for it.

## The chrome primitives

Chrome — the parts of a surface that are not its content — is drawn by `libkchrome` and by nothing
else. Two implementations of a button bar are two button bars, and the one nobody is looking at is
the one that drifts.

| Primitive | Draws |
|---|---|
| `kch_header` | The accent band across the top and its subject line |
| `kch_group` | A heading inside the body |
| `kch_buttons` | The verb bar: a row of buttons, dropped from the right when narrow |
| `kch_button_at`, `kch_hover` | Which button the last frame drew under a cell, and where the pointer is, so a button lights under it |
| `kch_list_wheel`, `kch_list_clamp` | The scrolling rule for a list |
| `kch_scrollbar` and `kch_scrollbar_press` / `_drag` / `_release` / `_grabbed` | A scrollbar that can be dragged |
| `kch_tile_*` | A block of cells drawn as pixels |
| `kch_tone`, `kch_tone_alpha`, `kch_popup_alpha`, `kch_slot_rgb` | Shading and body opacity derived from the palette |
| `kch_px_*` | The plates, rules and rounded ends a backend with pixels can record under the cells |

The controls come from `libktui`, and there is one of each. A surface that draws its own is a
second answer to a shape this desktop has already settled.

| Control | What it is |
|---|---|
| `ktui_button`, `ktui_check`, `ktui_radio` | A verb, a flag, one of a set. A button is a plate with a `░` shadow; the shadow is what stops a filled rectangle with a word in it reading as a selected row |
| `ktui_slider` | A number on a track: press, drag, wheel, or an end cap for one step. `◄ █████▒▒▒ ► 55` — the track is a fill and the thumb is the only accent |
| `ktui_dropdown_*` | A choice, as a list that opens under its row |
| `ktui_input` | A line of text with a caret |
| `ktui_list`, `ktui_table` | Rows, and rows with columns |
| `ktui_sel_slots`, `ktui_sel_row`, `ktui_sel_dim` | What a selected row looks like, for every surface that has one |
| `ktui_rows_*` | Which row the pointer is on, for a surface that draws its own rows |
| `ktui_table_event` | The same for a table: wheel, press, pick, Back |
| `ktui_tabs_*` | A strip of pages |
| `KtuiMenu`, `ktui_modal` | A pane of verbs, and a question |
| `ktui_progress`, `ktui_gauge`, `ktui_sparkline`, `ktui_heat` | Progress and measurements, in four shapes |

Each control is a `draw` / `key` / `hit` trio with one frame-level call on top. That is what lets a
surface running its own event loop — `kdos-settings`, `kdos-display` — use the same control as one
written inside `ktui_frame_begin()`. A control that existed only inside the frame would have to be
written a second time by those surfaces.

### Everything the keyboard can do, the pointer can do

Not "most things". A verb reachable only by a key chord does not exist for someone using a mouse,
and the shipped key card is not the first thing anybody reads.

The rule works both ways: every control answers the arrow keys and `Enter` as well as a click. It
is why the window menu carries every per-window verb rather than the memorable half, and why a
number in a settings form is a slider rather than a printed value.

The usual way to break it is a line like `if (ev.type != KT_EVT_KEY) continue;`, which throws away
every pointer event, so the surface cannot be operated with a mouse at all, however much of it
looks clickable. A surface that draws its own rows uses `ktui_rows_event` rather than
inventing its own answer:

- a press **moves** the selection;
- a press on the row already selected **picks** it;
- each wheel detent steps the selection one row, whether or not the list fits;
- the right button is Back.

`ktui_rows_event` ignores motion: hovering over a row does not select it. A list that scrolls needs
the fit-aware wheel described under [Drops and the wheel](#drops-and-the-wheel) instead.

### Bars, rows and hints

**A button bar drops buttons; it does not vanish.** Buttons are ordered most-useful-first and
dropped from the **right** when the surface is too narrow, never half-drawn. A bar that disappeared
entirely would leave only a row of key hints, which is what the buttons exist to replace.

**Anything that draws over part of a row owns all of it.** A button bar shares its row with the
status line, and the single column *between* two buttons belongs to neither, so a leftover
character would show through the gap. The bar clears its whole span first, and the status text is
clipped to what is left (`kch_buttons` returns the leftmost column it took).

**A hint row is drawn whole or not at all.** A fragment of one hint against the start of another
is worse than an empty half-row. A **message** takes whatever room there is, because it describes
what the person just did.

## The keys every surface answers

Five keys mean the same thing on every surface, and the bottom row of the surface says what the
other keys do right now.

| Key | What it does | Declared by |
|---|---|---|
| `F1` | Opens this surface's help page in `kdos-doc` | `KtuiKeys.doc`, drawn first in the hint row |
| `F10` | Opens the surface's menu bar | `KtuiMenu.has_bar` |
| `Shift+F10` | Opens the menu for the thing under the selection | `KtuiKeys.ctx_at` |
| `Alt+letter` | Opens a menu pane by its underlined letter | the `&` in a pane title |
| `Esc` | Steps back **one** level, and closes only from the top level | the Esc ladder, below |

**A key with nothing behind it is neither advertised nor answered.** Help pages are text files in
`/usr/share/kdos/doc/<name>.txt` (in the tree, `fs/usr/share/kdos/doc/`). A surface with no page
leaves `doc` as `NULL`, and `F1` then passes the key on: a key that opens an index saying *no such
document* teaches that help is broken. `testing/preflight.sh` refuses a `keys.doc` naming a page
that does not ship. The same rule holds for `F10` on a surface with no menu bar, and `Shift+F10` on
one with nothing selected.

**Esc is a ladder of declared layers**, checked at the moment the key arrives and never cached. A
surface registers each raised state (an open dropdown, a dialog, a search field) once with
`ktui_keys_layer()`, outermost first, and `ktui_keys()` climbs exactly one rung per press. Each
layer is a question ("is this open?") rather than a stored flag, so a dialog dismissed by a click
leaves nothing behind to swallow the next keystroke. `ktui_esc_verb()` names the topmost open rung,
so the hint row never reads *Esc Close* on a screen where Escape goes back.

**The hint row is built during the draw**, by whatever has the focus. `ktui_hint(key, verb)` and
`ktui_hint_if(cond, …)` add hints; `ktui_hint_row()` draws them and clears the pool. A fixed string
cannot follow the focus, and a row naming keys the focused control does not answer is worse than no
row: the whole value of the line is that every key on it works.

**The row drops its tail.** A hint that does not fit takes every hint after it with it, and `Esc`
is added last, so on a narrow surface it is the first thing lost. If a surface is too narrow for
everything it answers, drop a hint deliberately rather than moving `Esc` earlier, or make the
surface wider.

**A menu's `sel` counts items, never drawn rows.** A caller's `show` callback can hide rows, and a
selection counted in drawn rows lands on a different item the moment one is hidden. The same
callback answers both the drawing and the hit test, from one walk, because two copies of a
visibility rule eventually disagree and a click then runs the row above the one under the pointer.

**An accelerator is marked, not guessed.** `&` before a letter in a label or pane title marks it;
`&&` is a literal ampersand. The letter is underlined where the glyph tier supports underline and
bracketed where it does not — a Linux text console has no underline, and drawing one there puts a
colour on screen that no slot owns.

## Colour

Colour comes from a **slot**, never from a literal value. There are eight slots and no more,
because the console font has 512 glyphs: the text console uses the foreground intensity bit as the
ninth glyph bit, so colours 8–15 cannot be used as a foreground. Designing for eight means a text
console and a true-colour window show the same picture.

| Slot | Role | Scheme field |
|---|---|---|
| `KT_ACCENT` | The accent | primary |
| `KT_ERR` | Urgent | urgent |
| `KT_WARN` | Secondary — a caution, not a failure | secondary |
| `KT_TEXT` | Body text | text |
| `KT_MID` | Labels, secondary text, borders | pdark |
| `KT_DIM` | **A fill** — see below | dim |
| `KT_SURFACE` | Raised chrome background | variant |
| `KT_BG` | The background | backdrop |

A scheme has a ninth field, `deep` — the darkest ground — which no slot takes. It exists for derived
colours such as `kcol_muted()` below.

The values behind the slots are one table in `libkcolor` (`KCOL_SCHEMES` in
`src/libs/libkcolor/kcolor.h`), expanded at compile time by everything that draws. Nobody keeps a
second copy of the numbers, which is why `kdos theme <accent>` repaints the whole desktop. The table
holds eight accents: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect` and `paper`.
The default, `bone`, is named (`KCOL_DEFAULT_ID`) rather than taken from a position, so the table
can be reordered without changing what an unconfigured machine starts in. How to choose one is in
[Theming](../02-user-guide/theming.md#the-eight-accents).

### The two exceptions

**A colour that is not the palette's to name.** A program running in a terminal may ask for a
24-bit colour, or one of the 256-colour palette's cube and grey entries. No palette names those, they do not follow an accent,
and reducing them to eight slots would ruin a photograph. Such a cell carries the literal colour
**beside** the slot it reduces to, and only terminal content ever does. Chrome is always slots: a
piece of chrome holding a literal would stop following `kdos theme`. The sixteen ANSI colours stay
slots for the same reason — they are colours this palette names.

**Translucency**, which is a mix of two slots rather than a colour of its own. A grid holds one
colour per cell and nothing behind it, so seeing through a window is a pass over its rectangle
after it is drawn: `ktui_draw_bg_take()` copies the background colours that were there, and
`ktui_draw_blend()` mixes what has since been drawn back towards them. The result is written as the
cell's literal and the slot is left as drawn, so a display that does not show literals — a `--tty`
run, a test dump, `tty1` — shows an opaque window, which is the honest answer where there is
nothing to mix with.

Both ends of the mix come from the current palette and it is recomputed every frame, so an accent
change moves it like everything else; that is why this does not break the slot rule. It applies to
backgrounds only — a translucent character is one nobody can read — and a picture cell is skipped,
because a picture's pixels are not a background. A surface asks for it through
`KDispConfig.opacity` (a percentage; `0` means unset), and `libkcell` then renders one slot
translucent into an alpha buffer.

The drop shadow uses the same mechanism. It darkens rather than erases: every cell under the
one-cell strip keeps its character, and both halves are mixed towards `KT_BG`, so the window
underneath stays readable and a picture under it is left alone. The background slot is set to
`KT_BG` beside the literal, which is the whole shadow where literals are not shown. A shadow that
wrote blank cells would cut a rectangular bite out of whatever it falls on, which looks like a
rendering fault rather than depth.

### `KT_DIM` is a fill, not a label colour

Measured against all eight accents, `KT_DIM` used as a **foreground** gives a contrast of only
**1.42:1 to 2.12:1** against the surface and the background. `KT_MID` gives **3.40:1 to 6.30:1**.
Nothing is comfortably readable below about 3:1.

So a label is `KT_MID`. Hint rows, empty-state messages, help text and the brackets round a button
all belong there. Searching a new surface for `KT_DIM` in a foreground position is how it is
checked.

A two-state colour is a different matter. Where `KT_DIM` is one half of a pair — a scrollbar's
track against its thumb, an unpinned star against a pinned one — it carries the *difference*, and
changing it to `KT_MID` would erase the state rather than make it readable. Those pairs stay
`KT_DIM`.

For text that must be muted **and** readable there is a derived colour, `kcol_muted()`: the
scheme's `deep` field (the darkest ground, which no slot uses) mixed 56% of the way towards its text
colour. It measures **3.47:1 to 6.26:1**
against the background and the surface — above 3:1 in every accent, and more than twice as far from
`deep` as `KT_DIM`, which the self-test asserts. Every muted *text* role uses it. `KT_DIM` keeps
fills, borders and selection backgrounds, where contrast is not the question.

### Emphasis is a fill with swapped slots

Never use the reverse attribute over a label. Reverse inverts only the cells a character covers, so
a two-word name comes out as one lit block per word with a hole between them — and looks correct
for every name that happens to have no space in it.

Instead: fill the rectangle, swap the foreground and background slots, then draw the text.

### A selected row is `KT_DIM` under `KT_TEXT`, and the accent is one cell

```
    Files                FM  ■        a row
  ► System Monitor       MO  ■        the selection, in a pane without the keyboard
  ►▓Git▓▓▓▓▓▓▓▓▓▓▓▓GI▓▓■▓            the selection, in the focused pane
```

`ktui_sel_slots()` decides this, and no surface works it out for itself. There are three states,
because they say three different things:

| State | Drawn as |
|---|---|
| A row | `KT_TEXT` on the page, no fill |
| The selection, pane not focused | `►` in `KT_MID`, label in `KT_TEXT`, still no fill |
| The selection, pane focused | the row filled `KT_DIM`, `►` in `KT_ACCENT`, label in `KT_TEXT` |

Measured against the palette, `KT_TEXT` on `KT_DIM` is **8.30:1** in the worst accent and
**10.22:1** in the best, so the label clears 7:1 everywhere, and the `►` marker clears **3.68:1**
everywhere. The fill is quiet because it is a fill, and the accent is spent on the one cell that
says where the selection is.

Do not fill a row with `KT_ACCENT`. Writing `bg = on ? KT_ACCENT : KT_SURFACE` puts the background
colour on the label and drags a lit plate across the screen under the pointer, and it breaks the
rule above: `KT_DIM` owns selection backgrounds.

A pane without the keyboard keeps its marker and loses its fill. A surface with two panes has two
selections; filling both says both are live, and dropping the inactive one loses the place that
pane will return to.

`KT_ACCENT` as a fill has exactly two uses, and neither is a row:

- a **state indicator** — the current workspace chip, the installer's current step, a window
  flashing its bell;
- a **button**, where the plate is the control.

Both are small, both are one thing on screen rather than one per row, and both mean *this*, not
*here*. `grep 'KT_ACCENT :'` over the tree is the check; a match on a list row is a surface making
its own rule.

The muted colour cannot be read on the selection fill — **2.44:1 to 3.38:1**. A row with a
secondary column (a tag, a two-letter code, a unit) raises that column to full text colour when the
row is selected, and `ktui_sel_dim()` answers that in one place. Left muted, the right-hand half of
the row would disappear exactly when someone is looking at it.

### A shadow darkens, and the clamp is what guarantees it

`ktui_draw_shadow()` mixes the shadow strip towards `KT_BG` and then clamps each colour channel so
it is never lighter than it started. `KT_BG` is not the darker of the two slots in every accent: in
`bone` the background is lighter than the surface in red and green, and in `ice` in green and blue.
Without the clamp those accents would glow along two edges of every window. The clamp costs nothing
where the background is already darker, and it is what keeps a new accent from getting a luminous
shadow.

### A mark on a fill is a shape, not a contrast

A character on a filled plate is read by the plate **around** it. The dark slots of this palette
look like one colour — `KT_BG` measures **1.00:1 to 1.20:1** against `KT_SURFACE` in every
accent — so dark ink on a bright fill cannot be told apart from the chrome behind the fill by
colour, whichever of the two it is drawn in. What separates them is the fill surrounding the mark.

So a mark on a fill must be a character that stays clear of every edge of its cell; that clearance
is the whole boundary. On a focused window-frame chip, for example, the mark and the body below the
plate are both `KT_SURFACE`, so the strip of plate between the ink and the bottom of the cell is
the only thing dividing them. A thin strip reads as the plate ending early rather than as a mark on
it.

It is a threshold, measured on the console font `ter-kdos32n` (32 pixels tall):

| Character | Lit pixels (of 512) | Rows used | Clear rows below |
|---|---|---|---|
| `_` | 24 | 27–28 | 3 |
| `■` | 108 | 10–21 | 10 |
| `X` | 80 | 6–25 | 6 |
| `↓` | 68 | 6–25 | 6 |

Six clear rows of thirty-two hold; three do not. That is why `■`, `X` and `↓` are the window
frame's chips and `_` is not.

The same measurements explain why ink on a bright fill is dark at all. `KT_TEXT` drops as low as
**1.10:1** on `KT_ACCENT` and **2.17:1** on `KT_ERR`, so a light character on a lit or urgent plate
is effectively invisible. `KT_SURFACE` clears **3.40:1** on `KT_MID`, **5.23:1** on `KT_ERR` and
**5.51:1** on `KT_ACCENT` in the worst accent of each.

## The pointer contract

Every surface answers the pointer, and answers it the same way. A surface with rows and no motion
handling is a *picture*: the only way to discover that a row is a control is to click it.

| Gesture | Means |
|---|---|
| Motion | Lights what is under it; on a list that follows hover, selects the row |
| Left press | Activates. On a row that is already selected, opens it |
| Wheel | Steps the selection while the list **fits**; moves the **view** when it does not (see [the two wheel rules](#drops-and-the-wheel)) |
| Right press | Backs out one level, then closes |
| Scrollbar | Is **dragged**, not only looked at |
| Column header | Sets the sort; a second press reverses it |
| Click away | Closes a transient surface |

### The display draws the pointer, never the surface

A surface owns its cells and the display owns the screen. A pointer the surface drew would cost a
round trip for every motion event and lag behind the hand. The display already holds the device
and already knows where it is.

In the cells, the pointer is the **reversed cell** under it — the pointer every text mode has drawn,
and as fine as a grid of characters goes. Over a Wayland window there is also a real arrow, and it
is not drawn by the surface: `libkwl` answers the pointer-enter event with
`wp_cursor_shape_device_v1.set_shape`, and the compositor draws a cursor at the device's own pixel
position, so it moves as smoothly as the hand holding it. Without that request a client has no
cursor image and the pointer disappears over every surface this library draws.

### What a press would do

The toolkit defines seven pointer shapes, each saying what a press at that spot would do:
`KT_PTR_ARROW` (the default, and every unhandled case), `KT_PTR_IBEAM`, `KT_PTR_SIZE_NS`,
`KT_PTR_SIZE_WE`, `KT_PTR_SIZE_NWSE`, `KT_PTR_SIZE_NESW` and `KT_PTR_MOVE`.

The list is what this desktop can *mean*, not what any protocol carries. It is neither the cursor
shape protocol's list nor X11's, for the same reason the raw event codes are not libinput's: a
number that happens to equal an upstream one is a coupling neither side can see.

A backend holds no window state, so it cannot choose a shape; it cannot tell a border from a
box-drawing character. Whatever owns the pointer names the shape with `ktui_draw_cursor_shape()` —
**only when it changes**, because a pointer crossing a window spends hundreds of frames over the
same thing — and `KtuiBackend.pointer` carries it to the backend.

**The shape is a hint and never a promise.** No surface sets a shape, and no
backend fills `KtuiBackend.pointer`: a terminal, a dump, `tty1` and a Wayland window all reverse
the cell whatever the shape says, and on Wayland the compositor shows the default arrow. So a
control must say what it does in its own cells; the shape is a second telling for those who can see
it. A backend that meets a shape it does not recognise draws the arrow rather than failing, so a
newer session with more shapes still has a pointer on an older display.

Nothing shows a busy pointer. `libkdisp`'s cursor list includes a progress shape, and nothing sets
it: nothing here tracks a window as not answering in a way a pointer could report.

### The flush decides which is drawn

`ktui_draw_cursor(x, y)` names the cell the pointer is on; `ktui_draw_flush()` decides how it is
drawn. The contract:

- **A backend with pixels may claim the pointer through `KtuiBackend.pointer`.** Returning `1` is a
  promise that the cells reach the screen exactly as the surface composed them, with the backend
  drawing its own arrow. A backend that leaves the entry `NULL` — every backend in the tree does —
  gets the reversed cell. The arrow cannot be drawn in `libktui` itself: that library links nothing
  but the C library (it is linked by the installer in the first build phase, before any other
  library exists), and an arrow needs a pixel buffer.
- **The hook is called on every flush, including those with no pointer to report.** A negative `x`
  means no pointer, and it is the only thing that tells a backend to remove the last arrow; a
  backend told nothing would leave one where the hand last was.
- **The hook carries a shape beside the cell, and a backend may ignore it.** A shape needs its own
  hotspot: an arrow points with its tip, a resize arrow and an I-beam with their middle. Drawing
  every shape from a fixed corner would put a resize arrow half a cell off the border it belongs
  to — exactly enough to make a border seem to move when you reach for it.
- **The hook carries cells, and a backend that owns the device may draw finer.** What the hook
  says is that there *is* a pointer and which cell the surface believes it is on, which decides
  whose it is to draw.

Where the reversed cell is what gets drawn:

- **The reverse goes on for the flush and comes straight back off.** The back buffer accumulates
  the surface's cells between frames, so a reverse left in it would be a stain on every cell the
  pointer ever crossed.
- **Taking it off is what erases it.** The front buffer keeps the reversed cell and the back buffer
  does not, so the cell differs and repaints as itself the moment the pointer leaves. One XOR does
  both jobs.
- **A cell with no character still shows the reverse.** A space carries colour and nothing to draw,
  and the fill pass has already painted it in its background slot, so a painter that skipped it
  would lose the swap and the pointer would show only over text. `kcell_paint` fills those cells
  with the foreground slot instead.
- **A picture cell keeps the pointer.** A desktop icon, a panel icon and a picture in a terminal
  are things people aim at, so the reverse goes over them too: the flush swaps the cell's character
  for a blank alongside the XOR and puts it back afterwards. Inverting the picture's own pixels
  would be wrong — `KT_A_REVERSE` is also how selected rows are drawn, and a panel icon on a
  hovered row would come out in negative.
- **Motion is a change even when no cell's content changed**, so the frame is marked dirty for it;
  otherwise the pointer would only move when something else on screen did. On a backend that draws
  its own arrow that is two jobs: the cells under the *old* position go back into the row diff (the
  only thing that erases an arrow the cell model does not know about), and the move pushes the
  frame past the nothing-changed shortcut so it is presented at all. Skipping either leaves a trail
  of arrows down the screen.

Nothing is drawn before the first motion — the pointer starts on no cell — so a machine with no
pointing device does not wear a pointer in its corner all session.

Motion finer than a cell travels only in the raw input stream. `KtuiBackend.poll_raw` reports
every motion in the backend's own pixels beside the cell-level queue; `libkwl` fills it, and
nothing reads it. Nothing this desktop routes on is finer than a cell.

## Touch

A touchscreen follows the same contract, because one recogniser turns fingers into the pointer
events above. `ktui_gesture_feed` in `libktui` is fed by `wl_touch` in `libkwl`; a recogniser
written inside each backend would be written twice and disagree twice.

| Gesture | Reported as | Also arrives as |
|---|---|---|
| Tap | `KT_GEST_TAP` | a left press and release |
| Long press | `KT_GEST_LONG` | nothing — a surface wanting a context menu reads the gesture |
| Drag | `KT_GEST_DRAG` | motion with the button held |
| Two-finger scroll | `KT_GEST_SCROLL` | one wheel step per row the fingers' centre crosses |
| Pinch | `KT_GEST_PINCH` | nothing |
| Edge swipe | `KT_GEST_SWIPE_EDGE` | motion, and it says which edge it came from |

So a surface written without touch in mind already works under a finger, and one that wants more
reads `ev.gesture` on a `KT_EVT_TOUCH` event.

Four rules, each a bug if missed:

- **Movement is measured in cells.** A drag begins when the finger leaves the cell it went down in.
  That is coarse on purpose: everything here is a grid, and a threshold in pixels is a number a
  surface drawn in cells cannot see. Pointer drags on the desktop icons use the same one-cell rule,
  from `libkwm`'s `kwm_drag_threshold()`.
- **One finger of two says nothing yet.** Moving away from a still finger is both a pinch and a
  scroll; the answer comes when the second finger agrees or disagrees. Guessing makes the gesture
  flip between the two midway, which is unusable.
- **Only the first finger synthesises a pointer press, and releasing it closes that press.** A press
  under every finger would click whatever each finger landed on, so two fingers down to scroll
  would select two rows and a pinch over a button would press it. Presses and releases stay paired:
  the release goes out on the finger that opened the press, and a finger that never opened one
  closes nothing when it lifts.
- **The synthesised wheel is counted at the fingers' centre, one step per row.** Touch arrives one
  finger at a time, so a step per finger movement would count the same row twice and scroll twice
  as far as the hand moved. The gesture's own `dy` keeps the per-finger movement, for a surface that
  forwards the gesture.

A long press has no event to arrive on — the finger is down and nothing moves — so it is checked
with `ktui_gesture_tick` from the backend's idle wait and reported **once**, using the same
`CLOCK_MONOTONIC` milliseconds the recogniser was fed. A deadline compared against a different clock
never expires, and nothing reports that.

A long press is `Shift+F10` with a finger, and the contract answers it: `ktui_keys()` opens the
surface's context menu on `KT_GEST_LONG`, so a surface that declared one supports touch without a
touch path of its own. It opens **at the finger**, not where `ctx_at` says the keyboard focus is,
because someone holding a row expects the menu on that row. A surface with no context menu offers
none to a finger either, and a **tap** opens nothing: a tap is a click, every control already
handles one, and a menu on every tap would put a pane under every finger.

## Drops and the wheel

A drag-and-drop *drop* is a position **and** a payload, and an event has room for only one of them.
`KT_EVT_DROP` carries the position; `ktui_drop_take` returns the payload, once, so two surfaces in
one process cannot both act on one drop. A `text/uri-list` payload arrives exactly as sent — URIs
separated by CRLF, comment lines included — because what a URI means differs from surface to
surface.

Four points, each a bug if missed:

- **Motion arrives as a drag event.** Wayland reports plain movement and dragged movement the same
  way, so treating an event's "pressed" flag as true would make every mouse *move* a click. The
  **button state** is what must be remembered across events, which is why a slider arms on press,
  follows motion, and commits on release.
- **A motion that did not move is not a motion.** An absolute pointing device sends the position
  again with each wheel event, so a naive handler would step the selection and immediately put it
  back under a pointer that has not moved. Motion is dropped when the **cell** is unchanged.
- **A wheel step is a press with no release.** A wheel detent arrives as a press whose button is
  `KT_MB_WHEEL_UP` or `KT_MB_WHEEL_DOWN`, and no release follows it. Dispatch on the **button**,
  not on the press alone, or a scroll will raise, focus, move or close whatever is under the
  pointer. A scroll reaches the thing under the pointer and does nothing else to it: it does not
  raise it, take the keyboard or start a move or resize.
- **One press, not a double click.** Nothing in this toolkit measures a double click. Opening a
  detail view is not destructive, and the destructive verbs sit behind a confirmation.

The fit-aware wheel is `kch_list_wheel()` in `libkchrome`: a list that fits gets a selection step,
and a list that scrolls gets its view moved by three rows with the selection left where it was.
`kch_list_clamp()` keeps the view inside the list afterwards. `ktui_rows_event` does not follow
that rule: it steps the selection on every detent, fits or not, and leaves the view to follow the
selection. Twelve shipped surfaces use the fit-aware rule; the five that draw their rows through
`ktui_rows_event` (in `kdos-shell`: the recorder, trash, palette, theme and find surfaces) step. The flag that says "pull the
selection into view" is set by everything that **moves the selection** and by nothing that scrolls
the page, or the next frame would undo the scroll.

A list that scrolls shows a scrollbar. It is one column wide, drawn in the characters every glyph
tier has so it looks the same on a console. Nothing is drawn when everything fits, because a
full-height thumb says nothing a missing bar does not. Where rows would reach that column, the
**rows** give up a cell rather than the bar drawing over them: a selected row is a filled band, and
a bar on top of it would put a notch in it.

## Hit maps

A *hit map* records where each clickable thing was drawn, so a click can be matched to it. It is
recorded from what was drawn and never worked out a second time from the geometry the drawing
computed.

Both copies agree until the window is resized, a sidebar collapses or a border is added — and then
a click lands on the row above the one under the pointer. In a network dialog, that means joining
the wrong network.

Two supporting rules:

- **The frame subtracts its body origin once** and hands a page its own coordinates. A page that
  subtracts an origin itself is the second copy.
- **A control with no room records an empty span.** A hit map that outlives what it describes
  sends a click on a narrow screen to a control the current frame did not draw — someone aims at
  the clock and operates whatever sat there in an earlier frame.

## The glyph tiers

What a surface can draw depends on the terminal or display. `libktui` picks from its capabilities
(`ktui_caps`):

| Tier | When | Box drawing and symbols | Ramp (bars, charts) | Ramp levels |
|---|---|---|---|---|
| rich | UTF-8, not a Linux console | `glyph_utf8` | eighth blocks `▏…█`, `▁…█` | 8 |
| **vt** | UTF-8 **on a Linux console** (`KT_CAP_LINUXVT`) | `glyph_utf8` | `░ ▒ █` | 3 |
| ascii | no UTF-8 | `glyph_ascii` | `. : #` | 3 |

The box-drawing and symbol table (`glyph_utf8` in `src/libs/libktui/ktui_draw.c`) is the same for
the rich and vt tiers, so it must stay inside what the console font can draw. Only the ramps differ.

The vt tier exists because the console font, `ter-kdos32n`, has 512 glyphs. A character the font
lacks is drawn as a **blank** on `tty1` — so an eighth-block bar there is not ugly, it is
invisible. Three levels is the honest resolution of that font.

What the console font **has**: `░ ▒ █`, the **single** box-drawing set, the double rules `═ ║`, the
double corners `╔ ╗ ╚ ╝` and `╬`, and `· • ■ … ° ↑ ↓ ◀ ▶`.

What it **does not have**: eighth blocks, half blocks (`▀ ▄`), `▓`, braille, and — less expectedly
— the **double tees** `╠ ╣ ╦ ╩` and the mixed joins `╡ ╞`. The double set is only partly there:
the rules and the corners are, the tees are not.

Six of its 512 glyphs are double box-drawing characters swapped in over spacing accents by the
`terminus-font` recipe (`ports/core/terminus-font/build.sh`); the KDOS block logo needs them. The
font also answers to 627 codepoints in all, because its duplicate tables map extra codepoints onto
existing shapes. `▲ ▼ ◄ ►` and `← →` are present that way, drawn as `↑ ↓ ◀ ▶` — so `◄ ►` and
`◀ ▶` in the glyph table draw the same two shapes on `tty1`. Present is not distinct: two entries
that must look different there need two different glyphs, not two names for one.

Anything that can reach `tty1` stays inside the vt tier, and `glyph_utf8` is *entirely* inside it.
The toolkit uses that table whenever the backend reports UTF-8, which the Linux console does, so an
entry the font lacks is a blank on `tty1` — not a fallback, a blank, with nothing reporting it. A
slider built on `▓` would draw its filled part as nothing at all.

`testing/preflight.sh` reads the built `ter-kdos32n` (from `build/fs`) and refuses any
`glyph_utf8` entry the font cannot draw, because a written list of what the font carries is the
thing that goes stale. Two consequences are visible in the controls: the slider separates its track
from its fill by colour slot rather than by a third shade, and a button's shadow is `░` rather than
a half block.

A frame's title is bracketed `[ like this ]`, and the brackets are ASCII for a second reason beyond
the font. The test dumps are drawn at the **ascii** tier — `+=[ title ]===+` is what a committed
frame looks like — and every join in that table collapses to `+`, so a title bracketed with `╡ ╞`
or `┤ ├` would read there as two corners in the middle of the top rule. `[` and `]` are the same at
all three tiers, exist in every font, and are the bracket the DOS file managers put a title in.

A wide character is measured, not assumed. The toolkit computes display width and reserves a
continuation cell, so double-width text does not break the row layout. The console font has no
wide characters and every cell is one column, so on `tty1` a wide character is written as `?` in
one cell and its continuation as a space: the row keeps its columns and only the character is lost.
Chrome that must read on both stays inside the small set.

## Pictures are an enhancement layer

A *sprite* is a picture registered in a numbered slot of the backend's *sprite table* and drawn
into whole cells; the *icon atlas* is the theme's icon set, one
file (`/usr/share/kdos/icons/atlas.kia`, read by `libkicon`).

Icons and pixel tiles are drawn into **whole cells**, with the picture's slot and position within
it encoded in the cell itself. The ordinary row comparison therefore already tracks what changed,
and a text backend draws the fallback character instead.

Every surface must draw correctly when the picture is unavailable. The icon lookup answers "none"
on a terminal, when icons are turned off (`icons = no` in `comp.conf`), when there is no icon atlas,
and when the sprite table is full — each of those is a normal state, not an error. The test harness
stubs it to exactly that, so a committed reference frame is the **character grid**: a layout that
only lines up once the pictures load is a broken layout.

Two rules for pixel tiles:

- **A tile owns two slots and alternates between them.** A cell encodes the *slot*, not the
  picture, so redrawing a tile's contents in place changes no cell, the comparison sees nothing,
  and the frame is never presented — a clock tile would freeze at the minute it was first drawn.
  Swapping slots on every content change repaints exactly the rows it covers.
- **Decide the geometry before claiming the tile.** Giving up after claiming it leaves the tile
  believing it drew that content, and the next frame presents a stale slot.

### Damage, for a picture that changed in place

A new picture in the same slot changes no cell, which is what `ktui_draw_dirty()` is for. An
animation's next frame writes cells identical to the last, so the flush finds nothing to send and
the screen would keep the first frame forever. Marking the rectangle the slot covers costs that
rectangle; the alternative — repainting the whole screen for every arriving tile — costs every
character on the desktop and a full upload dozens of times a second.

A backend that keeps its own copy of the previous frame must implement `dirty` and mark **every**
copy it keeps. `libkwl` keeps three: the cells the compositor is showing (which the damage
rectangles are cut from) and one per shared-memory buffer (which each paint compares against).
Mark only the buffer copies and the pixels land in a buffer the compositor is never told to re-read;
mark only the screen copy and the damage names rows nothing repainted. Every one of these failures
is silent and looks like an animation that stopped. A backend without `dirty` compares against the
`prev` frame it is handed and needs nothing more.

### A picture from a terminal is the same sprite

A picture shown in a terminal uses the same sprite mechanism. A slot covers at most 16×16 cells, so
a larger picture becomes a grid of sprites sharing a key prefix, handled all-or-nothing: it is
evicted and re-registered as a unit rather than leaving three quarters of a photograph on screen.
Each backend does what it can:

| Backend | What a picture is |
|---|---|
| Wayland | Real pixels, scaled to the cells it occupies |
| A text terminal, or a display built without a pixel library | The fallback shade, in every cell of the picture |

A picture that shows as nothing is worse than one that shows as a mark: blank cells look like
output that never arrived. That is why the tiled path carries a fallback character rather than a
space.

A tile that *is* a control carries the control's state in its cells' background slot. The backend
fills a cell's background before drawing the picture over it, so that slot is the button's body
wherever there is no pixel layer to record a plate into. `KT_SURFACE` is right only where there is
one, since the recorded plate shows through it; on a plain character grid it would leave the
button with no fill, no edges, and no visible hover or open-menu state. So the slot follows the
same ladder the pixel plate draws: a quiet fill at rest, the accent under the pointer, the warning
colour while the control's own menu is open, with the ink following the fill. The Start button is
the example. An icon sitting *inside* a wider field shows its state through the field instead.

A sprite beside text is two cells wide and one tall. A cell is twice as tall as it is wide, so two
cells across one row is a square on the same line as the text. A two-by-two box next to a single
row of text centres the picture across the boundary between rows and makes the whole surface look
misaligned.

## The compositor's decoration is part of the set

The window frame is the one piece of chrome a KDOS program does not draw for itself. `kdos theme`
generates it into `~/.config/kdos-comp/themerc-override` for the current accent, and the compositor
reloads it on the same signal that retints everything else. It is designed to match the cell grid:

- **Square corners** (`<cornerRadius>0</cornerRadius>` in `rc.xml`). A rounded corner is the one
  thing a cell grid cannot express, so it is the one thing that gives a server-drawn frame away.
- **A two-pixel accent border** (`border.width: 2`), because a hairline disappears beside a
  32-pixel cell.
- **A title bar carrying the same double rule the grid draws with**, broken by the title and by
  each button, so it reads as `════ Title ════[_][=][X]`. This is the fork's own `flat kdos` title
  texture.
- **Buttons are 8×8 bitmaps enlarged exactly four times to 32×32 with nearest-neighbour
  filtering**, so they are hard-edged blocks rather than smeared glyphs; a non-whole multiple would
  give one stroke two pixels and the next three. The marks are the compositor's own — a bar, a
  framed box, a cross and a stack of rules — rather than the grid's, because they sit on different
  grounds. A button image is drawn in the accent
  (`window.active.button.unpressed.image.color`) on a title bar that is not, so the minimise bar
  works there with the one clear row it keeps beneath it. On the grid the chip mark is `KT_SURFACE`
  on a plate over a `KT_SURFACE` body, where `_` would leave only three clear rows (see
  [A mark on a fill is a shape](#a-mark-on-a-fill-is-a-shape-not-a-contrast)); hence `↓` on the
  grid and a bar here.
- **The close button is the urgent colour** (`window.active.button.close.unpressed.image.color`),
  and the others are the accent. It is the one window control that cannot be undone, and on an
  eight-colour palette that difference is the whole signal.
- **The hover plate is translucent** (`window.button.hover.bg.color` carries alpha `66`). The
  compositor lays that colour over the plain button image, and an opaque colour would paint the
  symbol out, leaving every button blank under the pointer — the moment it most needs to say what
  it is.
- **A box chip.** A window from a box that has been given its own accent colour carries a small
  square of that colour at the left of its title, so you can see which box it came from.

## The checklist

A new surface is not finished until every line is answered.

1. `ktui_draw_box` around it, with the title on the top edge.
2. `kch_header` for the band, `kch_group` for headings, `kch_buttons` for verbs.
3. Colour from slots; `KT_MID` for labels; fills for emphasis, never the reverse attribute.
4. Motion, press, wheel, scrollbar drag and header sort — **all five**.
5. A hit map recorded from the draw, with coordinates handed down by the frame.
6. `--dump` at the sizes the surface is used at — 80x24 and 132x43 for a window, its own size for a
   popup — with a reference frame (a *golden*, under `testing/goldens/`) committed for each.
7. Read it at the **vt** tier before believing it works on `tty1`.
8. One `KtuiKeys`, with `ktui_keys()` first in the event dispatch and `ktui_hint_row()` last in the
   draw — on **every** path, the `--dump` one included, because that call is what clears the hint
   pool.
9. Every raised state declared with `ktui_keys_layer()` rather than handled in an `Esc` branch.

If `grep -c KT_EVT_MOUSE` on a new source file returns zero, line 4 has not been answered.

## See also

- [Writing desktop software](../05-developer/writing-desktop-software.md) — implementing all of this
- [The C libraries](../05-developer/c-libraries.md) — `libktui`, `libkchrome`, `libkcolor` and the rest
- [Theming](../02-user-guide/theming.md) — the accents these slots resolve to
- [Testing](../05-developer/testing.md) — goldens and the dump harness
- [kdos-shell](../04-programs/kdos-shell.md) — the largest set of surfaces following this page
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor that draws the generated frame
- [The window model](window-model.md) — where the windows those frames surround go
