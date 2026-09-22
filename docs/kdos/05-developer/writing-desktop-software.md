# Writing desktop software

This page is the implementation guide for a KDOS surface: the toolkit's model, choosing a role, the
input rules, chrome, pictures, and how to look at what you drew without a screen.

Read [the design language](../03-architecture/design-language.md) first. It is the specification;
this page is how to implement it.

## What a surface is

A surface is a grid of character cells drawn by `libktui` onto one of three backends:

| Backend | Used by |
|---|---|
| A terminal | Anything run at a prompt |
| `libkwl` | Anything under the compositor |
| An offscreen buffer | `--dump`, and the committed reference frames |

Nothing above that line knows which. That is what makes a program identical at a prompt, in a
window and in a test fixture.

Every surface KDOS paints is one of these grids — the panel and its front ends, the resource
monitor, the terminal, the lock screen, the installer, the boot splash and `tty1` — handed to
`kdos-comp` as an ordinary Wayland surface. Two things on the screen are not. The compositor draws
its own chrome with pango — titlebars, the root menu and the window-switcher OSD, at a size matched
to the cell grid — and an application in a box draws whatever its toolkit draws. So the frame
around your window is not yours to lay out, and the pixels inside somebody else's are not cells.

### Reaching a display

A surface does not call a backend by name. `libkdisp` owns the choice and the whole surface
lifecycle — open, close, resize, autohide, cell size, scale, clipboard, cursor — and each program
states once which implementations it links:

```c
extern const KDispImpl kwl_impl;            /* libkwl */
const KDispImpl *const kdos_disp[] = { &kwl_impl };
const int kdos_disp_n = 1;
```

Order is the policy. The first implementation whose `probe` succeeds is used, and a probe must be
cheap and free of side effects, because `kdisp_init` probes implementations it will not go on to
use. A program that links none of them still compiles and draws through the terminal backend, which
is what a `--dump` is.

Every call site is then the same three lines regardless of server:

```c
KDispConfig cfg = { .role = KDISP_ROLE_TOPLEVEL, .app_id = "kdos-thing", … };
if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0)
        return 1;                            /* say so and exit, do not run blind */
```

`libkdisp` names no implementation and links none, so that array is what pulls Wayland into a
program, and it is the one line that changes when a display server is added or removed. `kwl_impl`
is the only implementation in the tree. Passing zero implementations selects the terminal backend,
which is what a `--tty` flag means.

`kdos-shell` alone opens a surface from fifty-five places. Branching on the display server at each
of them is the same decision written fifty-five times in one program and again in the next, which
is why the lifecycle is an interface.

A cell dump does not go through it. `--dump-cells` installs its own `KtuiBackend` with
`ktui_backend_set`, because the cell buffer is private to `libktui` and the vtable is its documented
seam. Routing a dump through `kdisp_init` would change every committed golden.

## The frame protocol

Drawing is immediate mode. Each pass through the loop draws the whole surface, and the toolkit
diffs against what was last presented and sends only what changed.

```c
while (running) {
    KtuiEvent ev;
    ktui_poll(&ev, timeout_ms);
    /* handle ev */

    ktui_draw_begin();
    draw_everything();
    ktui_draw_end();          /* diff and present */
}
```

Three rules hold:

- The frame state is private. Consume an event, query focus, take a wheel notch and read the focus
  rectangle through the accessors. There is no structure to assign to.
- Chrome uses caller-local identifiers in the reserved range, which never join the focus ring and
  never drag the page scroll. Claiming ordinary identifiers for chrome pushes every real control
  down the ring and parks the caret on a decoration.
- A resize is not applied until you apply it. The backend sets a flag; the reported size follows
  only when your loop calls the resize and invalidate calls. Any loop that owns a surface owns
  this: a surface that was a fixed size and then starts being resized draws against stale
  dimensions and silently fails its own bounds checks, painting nothing.

## Choosing a role

| Role | Is | Notes |
|---|---|---|
| `PANEL` | A layer surface with an exclusive zone | Per output; set `.cells`, not `.cols`/`.rows` |
| `BACKGROUND` | A layer surface behind everything | Per output |
| `OVERLAY` | A layer surface above windows | Menus, popups, tooltips |
| `TOPLEVEL` | An ordinary window | See below |
| `LOCK` | A session-lock surface | Covers every output |
| `SAVER` | The whole screen, above windows, taking nothing | See below |
| `NONE` | Connect and bind the globals, install no backend, create no surface | What `--dump` runs under |

A panel gives its thickness and nothing else. `.cells` is the depth across the edge; the extent
along it belongs to the display, because a layer surface is anchored to three sides. Set
`.cols`/`.rows` on a panel and a server may attach at that size instead, which is a bar the length
of a window sitting where nobody put it.

### A toplevel must ask for its frame

Bind the decoration protocol and ask for a server-side decoration on every toplevel. A client that
never binds it has not said which side draws the decoration and gets whatever the compositor
guesses, which here is no frame at all: nothing to drag, no close button, and the window controls
unreachable by pointer.

This goes unnoticed easily, because every other surface in this desktop is a layer or lock surface
and has no decoration to negotiate.

Ask for a size that leaves the frame somewhere to go, and treat it as a default rather than a
demand — the compositor's first configure carries the size it wants, and that wins.

Say the smallest grid you can compose on, in `.min_cols`/`.min_rows`. A window given fewer cells
than it needs draws nothing at all. Zero is no minimum, which is right for anything that reflows to
whatever it is given. It is not a licence to drop the surface's own `ktui_toosmall` check: a
compositor is told the same numbers and may ignore them.

### Bind the layer shell at the right version

On-demand keyboard interactivity is a later-version request. An older resource answers it with
*exclusive*, and the compositor then parks the seat's keyboard on that surface and refuses every
window focus — so nothing typed reaches any window until an overlay takes the focus and gives it
back.

The request is right, the protocol is right, and the number is wrong. It is invisible to everything.

### A lock surface takes no pre-configure commit

An empty commit before the first configure is a protocol error on a lock surface, though it is
normal elsewhere. The backend skips it for that role.

The lock role covers every output, because the protocol will not report the session locked until
they all have a surface. The toolkit has one cell buffer, so the prompt is on the first output and
the rest are filled with the background colour — a toolkit limitation, said out loud.

### A saver asks for no size and takes no input

`SAVER` is neither a big `OVERLAY` nor a soft `LOCK`.

It asks for no size. An overlay is centred and sized in cells by the client, and a client cannot
see the output; one that measured the screen itself would have to round pixels into cells, and a
row rounded down is a strip of desktop along the bottom edge of a surface whose whole job is to
cover it. The display sends the size in the first configure, exactly as it does for `BACKGROUND`.

It takes no keyboard and claims no pointer region — `kdisp_input_cells(NULL, 0)`. Every keystroke
and every click goes to what is underneath, which is what lets the display's idle policy see the
activity and take the surface away. A saver that swallowed input would be a lock screen with no
password. It is asked to close rather than killed, so exiting on `kdisp_should_close()` is not
optional: a saver that ignores the request stays on the screen.

### Anchoring a popup

Layer surfaces have no coordinates. "At x" is an anchor corner plus a margin in pixels.

Set the exclusive zone to −1 whenever you pass a margin. A zone of zero means the surface is
arranged inside the *usable* area, which already has the panel's zone removed — so a margin
computed from the panel's height applies the offset twice and the popup floats one bar height away
from the bar it belongs to. A zone of −1 means "do not move me out of anyone's exclusive zone", so
the anchor is the output edge and the margin is the only offset.

Set it only when a margin was given. A centred dialog, a notification with a corner margin and an
on-screen display are all asking to be *placed*.

Which corner depends on the bar's edge, because a popup belonging to a bar on the other edge has to
grow the other way.

A keyboard overlay that loses focus closes itself — gated on having seen focus first, because the
compositor decides when an on-demand surface gets the keyboard, and a loss before any gain would
close the surface during its own appearance.

## Input

The backend's rules, each guarding a distinct failure.

Motion arrives as a drag event. The protocol reports plain and dragged movement identically, so
testing an event's pressed flag for truth makes every mouse *move* a click. Remember the button
across events: a slider arms on press, tracks on motion, and commits on release. That is also why a
launch happens on release — a launch on press fires before a drag can begin.

A motion that did not move is not a motion. An absolute pointing device, which is what every
virtual machine presents, sends the position again with a wheel event, so a handler steps the
cursor and then immediately puts it back under a pointer that has not moved a pixel. The backend
drops a motion whose *cell* is unchanged; an enter seeds the comparison so an enter is never
swallowed.

An enter carries coordinates and they are not optional. Discarding them puts the first click after
an enter at an impossible position and leaves hover stale until the pointer moves, so a menu
opening under a stationary pointer misses its first click.

Key repeat is the client's job. The protocol has none: the compositor sends the repeat parameters
and every client repeats for itself. The poll timeout must be shortened to the next repeat, or
holding a key fires at whatever cadence your loop happened to poll at.

A wheel tick is not an axis event, and the two sources are different:

- A wheel is already quantised — one event per detent, with a discrete count. Running that through
  an accumulator leaves a remainder, so the next notch crosses twice and a list jumps two rows.
- A touchpad is not, and sends a stream of small continuous values from which a tick must be
  synthesised.

The source field says which. And one pointer frame is one detent on the discrete path: a front end
that turns one host scroll into two delivers a single frame carrying a count of two, and counting
it moves a list twice as far. The count is discarded and one tick emitted per frame; a duplicate
gate catches the same doubling arriving as two frames instead.

A leave must be reported. Hover state with no leave stays lit for the rest of the session.

A serial must be retained, from key, button and enter. Setting the selection, starting a drag and
setting the cursor all need one to present, so discarding them blocks the clipboard one level below
where anyone looks for it.

The event queue is a ring, not one slot. The client library delivers a whole batch of callbacks
from a single read, so a button press followed in that batch by its accompanying motion is
overwritten before any consumer sees it — every click under a moving hand is a coin toss. Motion
collapses onto motion; a button or a key never overwrites anything.

## Drawing

A double buffer needs a shadow per buffer. The paint diffs against a front buffer to damage only
what changed; with two buffers alternating and one shared shadow, the rows that changed while the
*other* buffer was in flight are never redrawn in it. Each buffer carries its own shadow, and a
full repaint is forced whenever a buffer has none or has been resized. Damage is still the
global diff; only the paint is per buffer.

The scale and the resized buffer must land in one commit. Split them and the compositor sees a
buffer whose size disagrees with its declared scale for a frame.

A widget you use already announces itself. Every toolkit widget states its role, its label where it
has one, and its position in its set at the point it computes focus, so a surface built out of them
needs no accessibility code of its own. What a widget cannot know it does not invent: a list and a
table take their rows from *your* callback, so they state "3 of 9" and leave the name to you. If
your rows have text a reader should hear, call `ktui_announce()` from the row callback for the
selected row; the queue is cleared every frame, so what you say is only ever about this one.

### Use the control, never a printed value

`libktui` carries one control of each shape — see
[the design language](../03-architecture/design-language.md#the-chrome-primitives) for the table —
and they are the reason a surface is usable with a mouse without your writing any pointer code. A
number is `ktui_slider`, a choice is `ktui_dropdown_*`, a line of text is `ktui_input`. A value the
surface prints and changes with `Left` and `Right` is a value nobody with a pointer can set at all.

A surface that draws its own rows still uses the toolkit's pointer rule. `ktui_rows_event` for a
list and `ktui_table_event` for a table answer the same four things — the wheel walks, a press
moves the caret, a press on the row it is already on picks, the right button is Back — and both are
the rule `kdos-pick` keeps. Writing that answer per surface gives a desktop as many pointer rules
as it has surfaces, several of which will be "drop the event".

Each control is a `draw`/`key`/`hit` trio with a frame call on top. If your surface runs
`ktui_frame_begin()`, call the one-liner; if it runs its own event loop, call the three and route
the press and the key yourself. Both reach the same code, so neither is a second implementation.

## Chrome

Use `libkchrome`. Two implementations of a button bar are two button bars.

```c
ktui_draw_box(krect(0, 0, w, h), " Resources ", KT_ACCENT, KT_BG, 1);
kch_header(...);
kch_group(...);
int left = kch_buttons(...);     /* returns its own left edge */
```

The button bar returns its left edge, because the status line shares that row. Draw the bar first
and clip the status text to what it left.

The bar clears its whole span, including the column *between* two buttons, which belongs to
neither. A leftover glyph otherwise shows through the gap.

A hint row is drawn whole or not at all. A message takes whatever room there is.

### The keys contract

Two calls and one descriptor. A surface holds a file-scope `KtuiKeys`, calls `ktui_keys()` first in
the dispatch it already has, and `ktui_hint_row()` last in the draw it already has.

```c
static KtuiKeys keys;

static int pane_up(void *user)    { (void)user; return detail_open; }
static void pane_close(void *user){ (void)user; detail_open = 0; }

/* Once, before the loop AND before any --dump branch: a dumped frame reads
 * the same Esc verb the live surface does. */
keys.doc = "settings";                 /* fs/usr/share/kdos/doc/settings.txt */
keys.help = sh_help;
ktui_keys_layer(&keys, "Back", pane_up, pane_close, NULL);

/* In the draw, last: */
ktui_hint_if(nrows > 0, "Enter", "open");
ktui_hint("Esc", ktui_esc_verb(&keys));
ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_SURFACE);

/* In the dispatch, first: */
int r = ktui_keys(&keys, &ev);
if (r == KTUI_KEY_CLOSE) goto done;
if (r == KTUI_KEY_TAKEN) continue;
```

`ktui_keys()` returns PASS for everything it does not own, so an unconverted surface behaves byte
for byte as it did. It takes every event and not only a key, because a surface with a menu would
otherwise need a second call site in its pointer path — and two call sites for one widget disagree
about which of them saw the click.

`ktui_hint_row()` must run on every path that draws, including `--dump`. It clears the pool as its
first act, before it measures the rect, so a zero-width rect is the right way to drain a frame
where a message owns the row. A frame that skips it carries its hints into the next one, and
`kdos-shell` is one binary with fifty-odd front ends sharing that pool.

Push only what the surface answers right now. `ktui_hint_if()` exists so a key that is inert in the
current state does not appear; that is the entire value of the line over a fixed string.

`kch_buttons()` already pushes `Enter <label>` for its focused enabled button, so do not push a
second one for it. A bar drawn with focus `-1` pushes nothing, and a surface using one that way
must push its own Enter hint.

`F1` is not pushed. It comes from `keys.doc` and is drawn first. A surface with no page in
`fs/usr/share/kdos/doc` leaves it NULL; `testing/preflight.sh` refuses a name with no file.

The list, wheel and scrollbar rule is `libkchrome`'s and has exactly one implementation. The
selection-follow flag is set by everything that *moves the cursor* and by nothing that scrolls the
page, or the next frame undoes the scroll.

## Pictures

Every consumer must draw correctly when the picture is unavailable. The icon lookup answers −1 on a
terminal, with icons off, with no artwork, and for a name nothing has a picture for — and each of
those is a normal state.

```c
int slot = kicon_slot(name, w_cells, h_cells);
if (slot >= 0) ktui_draw_sprite(x, y, slot, w_cells, h_cells);
else           ktui_draw_text(x, y, fallback_glyph, ...);
```

A sprite is two cells wide and one tall wherever it sits beside text. A cell is twice as tall as
wide, so that is a square on the text's own optical line. A two-by-two box next to a single row of
text centres the picture across the row boundary and makes the whole surface look misaligned.

### Pixel tiles

For content that genuinely cannot be a row of text — a chart, or a control with text at a size
other than the cell's — draw a canvas and hand it to the toolkit as a sprite. Three rules:

- Two slots per tile, alternating. A cell encodes the *slot*, not the picture, so redrawing a
  canvas in place changes no cell, the diff sees nothing, and the frame is never presented. A clock
  tile would freeze at the minute it was first drawn. Swapping slots on every content change
  repaints exactly the rows it covers, where a blanket invalidation would repaint the whole surface
  once a second for a fifteen-cell chart.
- Decide the geometry before claiming the tile. Bailing out after claiming it leaves the tile
  believing it drew that content, and the next frame presents a stale slot.
- A tile is never required, and the content hash excludes the accent, because a theme change drops
  every tile at the same moment the icons are retinted.

A tile is bounded to a modest number of cells, because the sub-cell coordinate is a few bits each
way. A page-wide chart is past that and must be drawn as cells — see
[kdos-res](../04-programs/kdos-res.md#the-charts).

### Two font traps, both silent

A repeated font property appends; it does not replace. Appending a size to a pattern that already
carries one yields two, and the first wins — so every canvas comes out at the cell's own size, with
the text still rendering. Strip the size the name carries before appending yours.

A bitmap font cannot be asked for an arbitrary size. It answers with the nearest strike it has.
Measure the result: a font that came back much shorter than requested is retried restricted to
scalable faces, and whichever is closer wins. A machine with no scalable face keeps the bitmap,
which is the honest answer.

## Colour

Slots only. `KT_MID` for labels — `KT_DIM` is a fill and measures below any text floor.

Emphasis is a fill with swapped slots, never a reverse attribute over a label. The attribute
inverts only the cells a glyph covers, so a two-word name comes out as one lit block per word.

A selected row's colours come from `ktui_sel_slots()`. A surface that works out its own — `bg = on
? KT_ACCENT : KT_SURFACE` is the shape to grep for — drags a lit plate under the pointer and puts
the background colour on the label:

```c
int fg, bg;

ktui_sel_slots(selected, pane_has_keyboard, KT_SURFACE, &fg, &bg);
```

`ktui_sel_row()` is the same answer with the fill and the `►` marker drawn for you. Pass the page
slot — `KT_BG` or `KT_SURFACE` — rather than letting the control guess, or a translucent window
gets an opaque band across it.

A secondary column on a selected row takes `ktui_sel_dim()`, never the muted colour. Muted on the
selection fill measures 2.18:1 to 3.43:1, so a tag, a two-letter code or a units suffix left muted
vanishes exactly when the row is selected.

## Adding a name to kdos-shell

1. Add a `<name>.c` with an entry point following the naming convention.
2. Add the name to the table in `main.c`. A name in the table with no entry point is a link error,
   not a missing feature — the table and the file are one edit.
3. Add the symlink in the recipe's build script. A name in the table that the build does not link
   is a program nothing can reach.
4. Declare it in `shell.h`. The table in `main.c` names the entry point and the header is where
   every other file learns of it.
5. Add a route or a menu row if a person should be able to reach it. `routes.c` reads
   `/etc/kdos/menu.conf`, so a shipped surface needs a row there; `testing/preflight.sh` checks the
   four places together, because a surface wired in three of them is a chord that opens nothing.
6. Give it `--dump` and commit reference frames. That is four more edits, and the suite passes with
   none of them:
   - the `for s in …` harness list in `testing/selftest.sh`, so the file compiles into the dump
     harness;
   - **both** `FRONT_END(<x>_main);` and a `{ "<x>", <x>_main }` row in
     `testing/fixtures/shell/dumpmain.c`. Missing either makes `dumpcheck --have <x>` answer no and
     the golden block print `skipped — not linked into the harness`;
   - a `golden <name> <size> …` call. The harness list and the golden-frames table are separate
     lists, and being in the first buys no coverage at all;
   - a fixture, if the surface reads anything the host owns. `--fixture <dir>` replaying recorded
     output is the idiom: `print` over recorded `lpstat`, `disks` over a socket that is not there,
     `store` over a recorded catalogue. Without one the golden holds the machine that wrote it and
     fails everywhere else.

   Every one of those failures reads as a pass. `ls testing/goldens/ | grep <name>` after the run
   is what tells a covered surface from an uncovered one; the exit code cannot.
7. If another tool spawns it, check the flags. `testing/preflight.sh` verifies that every flag one
   of these tools passes another is one the target accepts — an unknown argument prints a usage
   line to an error stream nobody reads and exits before a surface exists, so the symptom is a
   control that silently does nothing.

## Looking at it without a screen

```sh
kdos-start --dump
kdos-menu system --dump-cells
kdos-res --fixture testing/fixtures/res --dump-size 66x10 --dump
```

A `kdos-shell` front end draws its dump at a size compiled into the surface; only `kdos-res` takes
`--dump-size`. The size a reference frame is taken at comes from `$KDOS_DUMP_SIZE`, which is the
dump harness's — `testing/fixtures/shell/dumpmain.c` wraps `ktui_offscreen_init` for it — and not
something a shipped binary reads.

| Flag | Produces | Catches |
|---|---|---|
| `--dump` | The cell buffer as plain text | Geometry |
| `--dump-cells` | One line per painted cell, with colours and attributes | Colour regressions as well |

Dump at a size that forces degradation. A layout defect is usually a defect at *one* width, and a
full-size dump will not show it.

Reference frames are committed and compared by the test suite. Regenerating them is described in
[Testing](testing.md#goldens).

Read the low glyph tier before believing a surface reads on a console.

## The checklist

Restated from [the design language](../03-architecture/design-language.md#the-checklist) as a
procedure:

| # | Do | Check with |
|---|---|---|
| 1 | Box the surface, title on the top edge | `--dump` |
| 2 | Header band, group headings, button bar from `libkchrome` | `grep kch_` |
| 3 | Slots for colour; `KT_MID` for labels | `grep KT_DIM` in foreground positions |
| 3b | Selection through `ktui_sel_slots`; secondary columns through `ktui_sel_dim` | `grep 'KT_ACCENT :'` — a match is a surface deciding for itself |
| 4 | Motion, press, wheel, scrollbar, header sort | `grep -c KT_EVT_MOUSE` — zero is the defect |
| 5 | Hit map recorded from the draw | Resize and click the top row |
| 6 | Dump at two sizes, commit both | `testing/selftest.sh` |
| 7 | Read it at the vt tier, and use no glyph the console font lacks | `--dump` on a console; `testing/preflight.sh` reads the shipped font |
| 8 | One `KtuiKeys`; `ktui_keys()` first, `ktui_hint_row()` last, on every path | `grep -c ktui_hint_row` — the dump path counts |
| 9 | Every raised state a declared layer, never an `Esc` arm | `grep KT_K_ESC` — a remaining case is one the ladder should own |

## See also

- [The design language](../03-architecture/design-language.md) — the specification
- [The C libraries](c-libraries.md) — what you are building on
- [kdos-shell](../04-programs/kdos-shell.md) — the largest worked example
- [kdos-res](../04-programs/kdos-res.md) — a toplevel window and its charts
- [Testing](testing.md) — dumps, reference frames and fixtures
