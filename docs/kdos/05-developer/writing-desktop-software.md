# Writing desktop software

This chapter is for developers who write a new piece of the KDOS desktop (a panel applet, a
popup, a settings page, a window of its own) or change one that exists. In KDOS such a piece is
called a *surface*: a grid of character cells that the toolkit can put on a Wayland output, on a
terminal or into a text file. The chapter explains how a surface reaches the screen, how its event
loop is shaped, which kind of window to ask for, input and drawing, the shared chrome and the
keys contract, pictures and colour, a minimal surface that puts these together, the rules the
libraries already keep for you, how to add a new name to `kdos-shell` or a program of its own, and
how to test what you drew without a screen.

Read three things first:

- [The design language](../03-architecture/design-language.md). It specifies how a KDOS surface
  looks and behaves; this chapter is how to implement that specification.
- The table of libraries in [The C libraries](c-libraries.md). Everything here is built on
  `libktui`, `libkdisp`, `libkwl`, `libkchrome`, `libkicon` and `libkcell`, and that chapter holds
  the detail this one leaves out.
- An existing surface of the same shape. [kdos-shell](../04-programs/kdos-shell.md) holds 54 of
  them, and [kdos-res](../04-programs/kdos-res.md) is a complete window with charts.

A finished surface draws the same at a prompt, in a window and in a test, is reachable from the
desktop, and has committed reference frames that fail when its layout regresses.

## What a surface is

A surface is a grid of character cells drawn by `libktui` onto one of three kinds of backend. A
*backend* is a `KtuiBackend` vtable: it presents a frame, reports the grid size and the terminal
capabilities, and delivers events.

| Backend | Used by |
|---|---|
| The built-in terminal backend | Anything run at a prompt: `kdos-res --tty`, the installer, the build screen |
| `libkwl` | Anything under the compositor, `kdos-comp` |
| An offscreen buffer | `--dump`, `--dump-cells` and the committed reference frames |

A committed reference frame is called a *golden*; the goldens live under `testing/goldens/`, one
text file per surface and size, named `<name>-<cols>x<rows>.txt`. Nothing above the backend knows
which backend it is drawing on, and that is what makes a program identical at a prompt, in a
window and in a test fixture.

`kdos-shell` is a single binary that answers to many names; each name, with its own entry point,
is a *front end*. Every surface on the KDOS desktop is one of these grids: the panel and the other
`kdos-shell` front ends (including the file chooser that the desktop portal opens), the resource
monitor, the terminal and the lock screen, each handed to `kdos-comp` as an ordinary Wayland
surface. The installer (`src/packages/kdos-installer`) and the build screen
(`src/build/kdosbuild`) are the same grids drawn on a terminal. Two things on the screen are not
grids. The compositor draws its own chrome with Pango (window titlebars, the root menu and the
window-switcher OSD) at a size matched to the cell grid, and an application, whether natively
ported or running in a box, draws whatever its toolkit draws. The frame around your window is
therefore not yours to lay out, and the pixels inside another program's window are not cells.

The toolkits the applications use (GTK, libadwaita, WebKitGTK, Qt, KDE Frameworks, wxWidgets, FLTK
and Tk) are not available to a surface. A program under `src/desktop/` or `src/packages/` draws
through the `libk*` libraries and links no GUI toolkit and no Xlib; `testing/preflight.sh` reads the
`NEEDED` entries of every ELF file those ports install and fails the run when one names such a
library.

Most new surfaces are a new front end inside `kdos-shell` (see
[Adding a name to kdos-shell](#adding-a-name-to-kdos-shell)). A program with a separate life of its
own, such as `kdos-res` or `kdos-term`, gets its own directory under `src/desktop/` with a recipe
like the others there (see [A program of its own](#a-program-of-its-own)).

### Reaching a display

A surface does not call a backend by name. `libkdisp` owns the choice and the whole surface
lifecycle: opening and closing, overlay resize and autohide, cell size and scale, the clipboard and
drag, the pointer shape, input regions, the window title, the session lock, keyboard focus, the
window list and the display's fonts. Each program states once which implementations it links:

```c
extern const KDispImpl kwl_impl;            /* libkwl */
const KDispImpl *const kdos_disp[] = { &kwl_impl };
const int kdos_disp_n = 1;
```

Order is the policy. `kdisp_init()` walks the array and uses the first implementation whose
`probe` succeeds and whose `init` then succeeds. A probe must be cheap and free of side effects,
because `kdisp_init()` probes implementations it does not go on to use.

Every call site is then the same few lines, whatever the display server:

```c
KDispConfig cfg = { .role = KDISP_ROLE_TOPLEVEL, .app_id = "kdos-thing", … };
if (kdisp_init(&cfg, kdos_disp, kdos_disp_n) != 0)
        return 1;                            /* say so and exit, do not run blind */
```

`kdisp_init()` returns −1 when no implementation can draw, and the caller prints why and exits.
Passing zero implementations is not a failure: it returns 0 and leaves `libktui` on its built-in
terminal backend, which is what a `--tty` flag means. `kdos-res` shows the whole choice: with
`--gui`, or with `$WAYLAND_DISPLAY` set and no `--tty`, it calls `kdisp_init()`; otherwise it calls
`ktui_backend_set(NULL)`, which selects the terminal.

`app_id` must equal the file name of the program's `.desktop` entry without the suffix, because
that is how the panel and the window list match a window to its launcher. `kdos appid` checks the
match against `~/.local/share/kdos/observed-app-ids`, which `kdos-comp` appends to whenever a
window maps, so an entry is reported `ok` only after its window has been seen.

`libkdisp` names no implementation and links none, so the `kdos_disp` array is what pulls Wayland
into a program, and it is the one line that changes when a display server is added or removed.
`kwl_impl` is the only implementation in the tree. The indirection still earns its place as the
test seam: the *dump harness* (`testing/fixtures/shell/dumpmain.c`, the test build that links
every front end and records its goldens) replaces the `kdisp_*` entry points so that
`kdisp_init()` answers −1 and every front end takes its offscreen path with no compositor and no
display libraries. In `src/desktop/kdos-shell` alone there are 47 calls to `kdisp_init()`, one of
them the surface runner's on behalf of ten front ends; branching on the display server at each of
them would be the same decision written 47 times in one program and again in the next, which is
why the lifecycle is an interface.

A cell dump does not go through `libkdisp`. `--dump-cells` installs its own `KtuiBackend` with
`ktui_backend_set()`: most front ends use the backend that `sh_cells_backend()` in
`kdos-shell/cells.c` returns, and `kdos-settings` and `kdos-teams` carry their own. It is a backend
rather than `ktui_offscreen_init()` because the cell buffer is private to `libktui`, the backend vtable is its
documented seam, and offscreen mode skips the flush that the cell dump needs to see.

## The frame protocol

Drawing is immediate mode. Each pass through the loop draws the whole surface, and the toolkit
compares the result with what it last presented and sends only what changed.

```c
while (!kdisp_should_close()) {
    ktui_draw_clear();
    draw_everything();
    ktui_draw_flush();        /* diff and present */

    KtuiEvent ev;
    if (!ktui_backend()->poll_event(&ev, timeout_ms)) {
        if (ktui_resized) {   /* apply a resize before the next draw */
            ktui_resized = 0;
            ktui_draw_resize();
            ktui_draw_invalidate();
        }
        continue;             /* a timeout, a resize, or nothing to read */
    }
    /* handle ev */
}
```

Events come through the backend vtable rather than a free function: `ktui_backend()` returns the
backend the surface was initialised with, and `poll_event` is the one entry point every backend
implements. It returns 0 on a timeout, which is also where a surface with a clock or a meter does
its periodic work.

A `kdos-shell` front end need not write this loop. `sh_run()` in `shell.c` owns it, together with
the argument pass, the theme and the dump branch, and a front end on it states only what it draws
and how it answers (see [A minimal surface](#a-minimal-surface)). The loop above is for a program
of its own, and for the front ends whose loop has a shape of its own: the panel, the desktop, the
savers, the bezels and anything that holds two surfaces.

The loop above runs no immediate-mode frame, and a surface drawn with draw, key and hit functions
of its own needs none. A surface drawn with the immediate-mode controls (`ktui_button()`, a Tab
ring, `ktui_modal`) wraps each event and the draw that follows it in `ktui_frame_begin(&ev)` and
`ktui_frame_end()`, which is where a press is matched to a control and Tab walks the ring. The
installer's loop does this; on the runner it is the descriptor's `.frame` flag.

Five rules hold:

- The frame state is private. A surface consumes an event, queries focus, takes a wheel notch and
  reads the focus rectangle through accessors (`ktui_consume()`, `ktui_focus_get()`,
  `ktui_wheel_take()`, `ktui_focus_rect()`). There is no structure to assign to.
- A click is matched against the layout drawn on the previous frame. Controls register a hit
  rectangle as they draw, and `ktui_frame_begin()` swaps the hit lists before it dispatches the
  event, so the press lands on what the person saw.
- Mouse-only chrome, such as a sidebar entry, a tab or a title button, registers with
  `ktui_hit_chrome()`, whose identifiers start at `KTUI_ID_CHROME` (10000), far above any
  positional id. These never join the Tab ring and never drag the page scroll. Claiming
  ordinary identifiers for chrome pushes every real control down the ring and parks the caret on a
  decoration.
- A group of controls drawn only some of the time goes between `ktui_id_push("name")` and
  `ktui_id_pop()`. Outside a scope a control's id is its place in the frame, so a group that
  appears renumbers everything after it: the focus lands on a different control and a field takes
  another field's caret. Inside a scope the ids are hashed from the name, and the controls after
  the group keep theirs. The installer's passphrase pair under *Encrypt the root filesystem* is
  drawn this way. Tab follows the order the controls were drawn in, scoped or not.
- A resize is not applied until the loop applies it. The backend sets `ktui_resized`; the cell
  buffer follows only when the loop calls `ktui_draw_resize()` and `ktui_draw_invalidate()`, as
  above. Every loop that owns a surface owns this step, and on the runner the step is the
  runner's. A loop that omits it draws against the old dimensions after a resize, fails its own
  bounds checks and paints nothing.

## Choosing a role

`KDispConfig.role` says what kind of surface to create. The roles use four Wayland terms. A *layer
surface* is a surface of the wlr-layer-shell protocol (`zwlr_layer_shell_v1`): it is placed by
anchoring it to edges of an output rather than as a window. Its *exclusive zone* is the strip of the
output it reserves, so that windows are laid out clear of it. *xdg-shell* is the protocol for
ordinary windows. A *configure* is the compositor's message that gives a surface its size; the
client acknowledges it and then *commits*, which presents the buffer it has attached.

| Role | Is | Notes |
|---|---|---|
| `PANEL` | A layer surface with an exclusive zone | Per output; set `.cells`, not `.cols`/`.rows` |
| `BACKGROUND` | A layer surface behind every window, anchored on all four edges | Per output; no exclusive zone |
| `OVERLAY` | A layer surface above windows | Menus, popups, tooltips, toasts |
| `TOPLEVEL` | An ordinary window (xdg-shell) | See below |
| `LOCK` | A session-lock surface | Covers every output |
| `SAVER` | The whole screen, above windows, taking no input | See below |
| `NONE` | Connect and bind the globals, install no backend, create no surface | The panel's `--dump`, and `kdos-display --list` / `--apply` |

The other fields that matter most are `app_id`, `title` (toplevel only), `font`, `output` (the
compositor's name for a screen, such as `eDP-1`; `NULL` leaves the choice to the compositor, which
chooses exactly one), `keyboard`, `dismiss_on_unfocus`, `corner` with `margin_x`/`margin_y`
(overlay only), `floating` (a toplevel that opens centred at the size it asked for), `opacity`
(body opacity in percent; 0 means 100) and `manage` (the surface intends to act on other programs'
windows). `kdisp.h` documents each one.

A panel gives its thickness and nothing else. `.cells` is the depth across the edge; the extent
along it belongs to the display, because a layer surface is anchored to three sides. Set
`.cols`/`.rows` on a panel and a server may attach at that size instead, which produces a bar sized
like a window at a position the compositor picks.

The lock role covers every output, because the protocol will not report the session locked until
every output has a surface. `libktui` has one cell buffer, so the prompt is drawn on the first
output and the others are filled with the background colour. That is a limitation of the toolkit,
not of the protocol.

Each role also asks things of the display that `libkwl` already does for every surface: the
decoration request, the layer-shell version, the lock surface's first commit. They are described in
[What libkwl and libkcell do for a surface](#what-libkwl-and-libkcell-do-for-a-surface).

### Sizing a toplevel

Ask for a size that leaves the window frame somewhere to go, and treat it as a default rather than
a demand: the compositor's first configure carries the size it wants, and that size wins. With
`.cols` and `.rows` left at zero, `libkwl` commits 80×22 cells.

Declare the smallest grid the surface can compose on in `.min_cols`/`.min_rows`. The front ends in
`kdos-shell` that declare one use between 40×12 (`kdos-audio`) and 60×16 (`kdos-store`); most of
the settings-style windows use 56×12 or 56×14. Zero means no minimum, which is right for anything
that reflows to whatever it is given. The fields are a request and never a guarantee: no display in
the tree acts on them, so the surface's own `ktui_toosmall()` check is the only thing that stops it
composing nothing on a grid narrower than it needs.

### A saver asks for no size and takes no input

`SAVER` covers every pixel of one output, takes its size from the display, and passes all input
through to what is underneath.

It asks for no size. An overlay is centred and sized in cells by the client, and a client cannot
see the output; one that measured the screen itself would have to round pixels into cells, and a
row rounded down is a strip of desktop along the bottom edge of a surface whose job is to cover
it. The display sends the size in the first configure, exactly as it does for `BACKGROUND`.

It takes no keyboard and claims no pointer region (`kdisp_input_cells(NULL, 0)`). Every keystroke
and every click goes to what is underneath, which is what lets the display's idle policy see the
activity and take the surface away; a saver that took input would hide that activity and never be
removed. It is asked to close rather than killed, so the loop must exit when
`kdisp_should_close()` answers true; a saver that ignores the request stays on the screen.

### Anchoring a popup

Layer surfaces have no coordinates. "At x" is an anchor corner plus a margin in pixels.
`KDispConfig.corner` takes one of five values:

| Corner | Anchored to | Used for |
|---|---|---|
| `KDISP_CORNER_CENTER` | Nothing (centred) | The launcher, dialogs |
| `KDISP_CORNER_TOP_RIGHT` | Top and right | A toast |
| `KDISP_CORNER_TOP_LEFT` | Top and left | A dropdown under a word on a top bar |
| `KDISP_CORNER_BOTTOM_LEFT` | Bottom and left | A menu above a bottom bar |
| `KDISP_CORNER_BOTTOM_CENTER` | Bottom only | The volume and brightness bezel |

Which corner a popup uses depends on the bar's edge (`kdisp_edge_bottom()`), because a popup
belonging to a bar on the other edge has to grow the other way. A client cannot express "just
above the taskbar" by anchoring to the top with a computed margin, because it does not know the
output's height in pixels.

When a margin is given, `libkwl` sets the exclusive zone to −1. A zone of zero means the surface is
arranged inside the *usable* area, which already has the panel's zone removed, so a margin computed
from the panel's height would apply the offset twice and the popup would float one bar height away
from the bar it belongs to. A zone of −1 means "do not move me out of anyone's exclusive zone", so
the anchor is the output edge and the margin is the only offset. The zone is left alone when no
margin was given: a centred dialog, a notification at the default corner margin and the bezel are
asking to be placed, and being kept clear of the panel is right for them.

An overlay that should close when the keyboard focus goes elsewhere sets `.dismiss_on_unfocus`.
That is right for a menu, the launcher and the run box, and wrong for a dialog: the file chooser
and the yes/no prompt stay up when a person clicks back to the application mid-choice. The
dismissal is gated on having seen the keyboard enter first, because the compositor decides when an
on-demand surface gets the keyboard, and a leave before any enter would close the surface while it
appears.

## Input

Motion arrives as a mouse event with `btn == KT_MB_MOVE` and `press == KT_MP_DRAG` (2). The protocol
reports plain and dragged movement identically, so a handler that tests `press` for truth turns
every movement of the mouse into a click; test for `KT_MP_PRESS` and `KT_MP_RELEASE` by value, and
remember the button across events. A slider arms on press, tracks on motion and commits on release,
and a launch happens on release, because a launch on press fires before a drag can begin.

Everything else about input is cleaned up before it reaches the surface: a click arrives once, a
wheel notch moves one row, a held key repeats. The rules that make that true are in
[Input the backend cleans](#input-the-backend-cleans).

## Drawing

A widget announces itself. Every toolkit control states its role, its label where it has one and
its position in its set at the point it computes focus, so a surface built from them needs no
accessibility code of its own. What a widget cannot know it does not invent: a list and a table
take their rows from your callback, so they state "3 of 9" and leave the name to you. If your rows
have text a reader should hear, call `ktui_announce()` from the row callback for the selected row.
`ktui_frame_begin()` clears the queue (at most 16 entries) every frame, so what you announce is
only ever about the current one. See [Accessibility](../02-user-guide/accessibility.md) for who
reads the queue.

How a finished frame reaches the compositor is `libkwl`'s concern; see
[Presenting a frame](#presenting-a-frame).

### Use the control, never a printed value

`libktui` carries one control of each shape (see
[the design language](../03-architecture/design-language.md#the-chrome-primitives) for the table),
and they are the reason a surface is usable with a mouse without your writing any pointer code. A
number is `ktui_slider_*`, a choice is `ktui_dropdown_*`, a line of text is `ktui_field_*` (or
`ktui_input()` inside a frame), several lines are `ktui_textarea_*`. A value the surface prints and
changes with `Left` and `Right` is a value nobody with a pointer can set at all.

A surface that draws its own rows still uses the toolkit's pointer rule. `ktui_rows_event()` for a
list and `ktui_table_event()` for a table answer the same four things: the wheel walks, a press
moves the caret, a press on the row the caret is already on picks it, and the right button is Back.
Each returns `MOVED`, `PICKED`, `CLOSE` or `NONE` (`KTUI_ROWS_*`, `KTUI_TABLE_*`), and both are the
rule `kdos-pick` keeps. A surface that writes its own answer ends up with a pointer rule different
from the rest of the desktop, often one that ignores the event.

A list with columns is a `ktui_table`, and its header is part of that rule. Mark a column
`KT_COL_SORT` and a press on its title returns `KTUI_TABLE_SORT`. Answer it with
`ktui_table_sort()` over an index array that your cell callback reads the records through, and
bind a key to `ktui_table_sort_next()` so the keyboard reaches the same orders. Mark a number column
`KT_COL_RIGHT` and draw its cells with `ktui_table_text()` so they line up under the title. Mark a
column `KT_COL_RESIZE` and pass motion and release events as well as presses, and its edge drags.
A table on a `KT_SURFACE` window sets `page`, and one that should select by the selection rule sets
`selrule`. `kdos-trash` is the worked example: four sortable columns, a resizable name, sizes
right-aligned.

Each control is a `draw`/`key`/`hit` trio with a frame call on top. A surface that runs
`ktui_frame_begin()` calls the one-line frame call; a surface with its own event loop, such as
`kdos-display` or `kdos-audio`, calls the three and routes the press and the key itself. Both reach
the same code, so neither is a second implementation.

A text field in a surface with its own loop is a `KtuiField` over the surface's buffer. Act on the
keys that are the surface's (Enter, Esc, the list keys) and hand every other key to
`ktui_field_key()`; a paste arrives as a queue rather than as an event, so also call it with NULL on
every wake while the field has the focus. A surface with a buffer and its own Backspace and printable
arms is a field without the caret keys, the paste, the secret masking, the chord guard or the UTF-8
boundaries.

### Laying out a page

A form is a column of rows, and a page that keeps its own `y` and writes its label column out at
every field (`b.x + 17`) has a column one edit can move for one row and not the next. Take the rows
from a `KtuiLay` instead:

```c
KtuiLay l;
KRect f;

ktui_lay_begin(&l, body);
ktui_lay_section(&l, "YOU");
f = ktui_lay_field(&l, "username", 0);     /* label in KT_MID, control after it */
f.w = 34;
ktui_input(f, cfg.username, sizeof(cfg.username), 0, "kdos");
f = ktui_lay_field(&l, NULL, 0);           /* the same column, no label */
ktui_pw_meter(f.x, f.y, f.w, cfg.userpass);
ktui_lay_gap(&l, 1);
f = ktui_lay_row(&l, 1);
ktui_check(f.x, f.y, f.w, "Administrator", &cfg.user_wheel);
ktui_lay_indent(&l, 0);                    /* a note under the box, by its mark */
f = ktui_lay_row(&l, 1);
ktui_note(f.x, f.y, f.w, "may use sudo");
ktui_lay_unindent(&l, 0);
```

The label column is 16 cells, the key column `ktui_kv()` uses, so a form and a key/value list
beside it align; pass another width where a page needs one. A row with several controls side by
side is `ktui_lay_cols()` over a `KtuiCol` array, with the table's rule: fixed widths kept, one
cell between, the first zero-width column takes the rest. `ktui_lay_left()` is what is left for a
paragraph or a list at the bottom. The cursor only does arithmetic and draws labels through the
calls you would have made, so moving a page onto it changes no cell; the installer's *Accounts*
page is the worked example. It is not a frame control and needs no frame.

Two frame controls go with it, both named rather than counted, so each keeps its state in
`ktui_state()` whatever is drawn above it:

- `ktui_split(r, "name", KT_SPLIT_SIDE, at, min, &a, &b)` cuts `r` with a divider the pointer drags
  and the arrow keys move; `Home` restores it. A negative `at` sizes the second pane, which is what
  a side panel on the right wants: it keeps its width as the window grows.
- `if (ktui_fold_begin(x, y, w, "Advanced", 0)) { ...; ktui_fold_end(); }` is a section heading
  that opens and shuts. Call `ktui_fold_end()` only when it returned 1. The body is drawn inside an
  id scope the fold pushed, so its controls come and go without moving any id after it: a fold is
  the scoping rule under [The frame protocol](#the-frame-protocol) done for you.

Two of either with one name in one id scope are one control; wrap repeats in
`ktui_id_push_int()`.

## Chrome

*Chrome* is the part of a surface that is not its content: the box, the header band, group
headings, the button bar, the hint row, scrollbars. Use `libkchrome` for all of it. A second
implementation of any of these diverges from `libkchrome` as soon as either one changes.

```c
ktui_draw_box(krect(0, 0, w, h), " Updates ", KT_ACCENT, KT_BG, 1);
int top = kch_header(w, "system-software-update", "Updates", sub, icons_on);
kch_group(2, top, w - 4, "Behind");
int left = kch_buttons(w, h - 3, buttons, nbuttons, focus);  /* its own left edge */
```

`kch_header()` draws the band in rows 1 to 3 of a boxed window and returns the first body row.
`kch_buttons()` draws at most eight buttons (`SH_MAX_BTN`), right-aligned; when the bar is too wide
for the window it drops whole buttons from the right, so callers order them most useful first. It
returns its left edge because the status line shares that row: draw the bar first and clip the
status text to what it left.

The bar clears its whole span, including the column *between* two buttons, which belongs to
neither; otherwise a leftover glyph shows through the gap. It clears to the surface's own body slot,
because half of the desktop's surfaces use `KT_SURFACE` as their background and a hard-coded
`KT_BG` would paint an opaque band across a translucent window. Feed pointer motion to
`kch_hover()` so a button lights under the pointer, and ask `kch_button_at()` which button a press
landed on.

A hint row is drawn whole or not at all. A message takes whatever room there is.

### The keys contract

Every surface answers the same keys (see
[the design language](../03-architecture/design-language.md#the-keys-every-surface-answers)), and
the contract is two calls and one descriptor. A surface holds a file-scope `KtuiKeys`, calls
`ktui_keys()` first in the dispatch it already has, and `ktui_hint_row()` last in the draw it
already has.

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

`ktui_keys()` returns one of four values:

| Value | Means |
|---|---|
| `KTUI_KEY_PASS` | Not its event; the surface handles it |
| `KTUI_KEY_TAKEN` | Handled; draw again |
| `KTUI_KEY_CLOSE` | `Esc` arrived with no layer up; the surface closes itself, because the toolkit does not own the program's lifetime |
| `KTUI_KEY_MENU` | An item of the surface's menu was picked; `keys.menu_id` names it |

It takes every event, not only keys, because a surface with a menu would otherwise need a second
call site in its pointer path, and two call sites for one widget disagree about which of them saw
the click. Everything it returns `PASS` for reaches the surface's own dispatch unchanged.

`Esc` is a ladder of *layers*: raised states such as an open detail pane or a typed query, each
declared with `ktui_keys_layer()` as a verb, an "is it up" callback and a "take it down" callback.
Layers are registered once at start, innermost last, because `Esc` unwinds them from the end; at
most `KTUI_LAYER_MAX` (6) are allowed. The "is it up" callback is asked at the moment the key
arrives and never cached, so a state dismissed by a click does not leave a flag that swallows the
next `Esc`. `ktui_esc_verb()` returns the verb of the topmost open layer, or "Close".

`ktui_hint_row()` must run on every path that draws, including `--dump`. It clears the pool of
pushed hints as its first act, before it measures the rectangle, so a zero-width rectangle is the
right way to drain a frame where a message owns the row. A frame that skips it carries its hints
into the next one, and `kdos-shell` is one binary with 54 front ends sharing that pool. It draws
nothing in a window shorter than eight rows or too narrow for one whole hint. Both strings passed
to `ktui_hint()` are copied, so their lifetime is the caller's concern for the length of the call
only.

Push only what the surface answers right now. `ktui_hint_if()` exists so that a key which does
nothing in the current state does not appear; that is the entire value of the row over a fixed
string.

`kch_buttons()` already pushes `Enter <label>` for its focused, enabled button, so do not push a
second one for it. A bar drawn with focus `-1` pushes nothing, and a surface that uses one that way
must push its own `Enter` hint.

`F1` is not pushed. It comes from `keys.doc` and is drawn first; `keys.help` is called with that
name when `F1` is pressed (`sh_help()` in `kdos-shell`). A surface with no page in
`fs/usr/share/kdos/doc/` leaves `doc` `NULL`, and `F1` is then neither advertised nor answered.
`testing/preflight.sh` refuses a `keys.doc` that names no file there.

### Menus

A surface with a menu bar or a context menu declares a `KtuiMenu` (panes of `KtuiMenuItem`s, each
with a label, an id, an optional accelerator string and an enabled flag) and points `keys.menu` at
it. `ktui_keys()` then routes into the menu before the `Esc` ladder: `F10` opens the bar,
`Alt`+letter opens a pane by its marked letter, `Shift+F10` pops the context pane where the
surface's `ctx_at` callback says its focus is drawn, and `Esc` closes an open pane before it
reaches any layer. A pick returns `KTUI_KEY_MENU` with the item's id in `keys.menu_id`.
`ktui_menu_draw()` draws the bar and the open pane and pushes the menu's own hints. `kdos-pick`
and `kdos-desk` use it.

### Lists and the wheel

The list, wheel and scrollbar rule is `libkchrome`'s and has exactly one implementation. When a
list fits its window, the wheel is a cursor step, the same as `↑` and `↓`. When it scrolls, the
wheel moves the viewport by three rows (`SH_WHEEL_ROWS`) and the cursor stays on its row:
`kch_list_wheel()` returns 1 in that case, and the caller must not move its cursor.
`kch_list_clamp()` keeps the viewport inside the list and pulls the selection into view only when
its `follow` flag is set. Set that flag from everything that moves the cursor and from nothing that
scrolls the page, or the next frame undoes the scroll.

After the clamp, `kch_list_view(x, y, w, h, top)` declares where the rows are and which item is in
the first of them: the rows' cells alone, with no header and no scrollbar column. It is called on
every draw, and a list not declared in a frame stops gliding. Where the display has a frame clock
and motion is on, a change of `top` is then presented as the list gliding there: `libkwl` slides the
list's pixels from the picture on the screen to the new one over 100 ms (`KWL_GLIDE_MS`), eased out,
committing the frames in between itself, so a wheel notch's three rows arrive as a slide rather than
a jump and a second notch continues the slide from wherever it is. Nothing about the list moves by
less than a row: `top`, the hit map, the pointer's row and every dump are whole rows, and the last
frame of a glide is the cells' own picture. A move as long as the list or longer is a jump, and so
is a change of `top` that is not a move — a filter typed, a group opened — which `libkwl` tells from
the cells: at least half the rows the two frames share must be the same rows shifted. On a terminal
and in a dump the call does nothing.

`kch_scrollbar()` draws a draggable bar (up to `KCH_SCROLLBARS`, four, per surface) and nothing at
all when everything fits. Call it every frame, including frames where the list fits, so that a bar
which has stopped being drawn stops being grabbable.

## Pictures

A *sprite* is a picture registered in one of `libktui`'s numbered sprite slots (not to be confused
with the palette slots under [Colour](#colour)); a cell refers to the picture by its slot number.
The set of sprite slots is the *sprite table*. See also the
[glossary](../06-reference/glossary.md).

A picture is an enhancement layer over the cell grid, never a replacement, and every consumer must
draw correctly when the picture is unavailable. `kicon_slot()` answers −1 on a terminal, with icons
turned off (`icons = no` in `comp.conf`, or the `--no-icons` flag that the panel and several front
ends take), with no artwork, for a name nothing has a picture for, and when the sprite table is
full. Each of those is a normal state.

```c
int slot = kicon_slot(name, 2, 1);           /* two cells wide, one tall */
if (slot >= 0) ktui_draw_sprite(krect(x, y, 2, 1), slot, KT_TEXT, KT_BG);
else           ktui_draw_text(x, y, 2, fallback_glyph, KT_MID, KT_BG, 0);
```

A sprite is two cells wide and one tall wherever it sits beside text. A cell is 16×32 pixels on
this desktop, twice as tall as it is wide, so two by one is a square on the text's own line. A
two-by-two box next to a single row of text centres the picture across the row boundary and makes
the whole surface look misaligned. `kicon_slot()` draws the largest square that fits the box and
never changes an icon's aspect.

### Pixel tiles

For content that cannot be a row of text, such as a chart or a control that mixes text at a size
other than the cell's with drawing of its own, draw a canvas and hand it to the toolkit as a sprite through `libkchrome`'s tiles:
`kch_tile_begin()` returns a `KCellCanvas` to draw into (or `NULL`), `kch_tile_commit()` publishes
it, `kch_tile_slot()` says whether it is up (a slot, or -1) and `kch_tile_draw()` writes its cells
into a rectangle of the grid. Four rules apply:

- Two slots per tile, alternating. A sprite cell encodes the *slot* and the sub-cell coordinate, not
  the picture, so redrawing a canvas in place changes no cell, the comparison sees nothing and the
  frame is never presented; a clock tile would freeze at the minute it was first drawn. The tile
  swaps slots on every content change, which repaints exactly the cells it covers, where
  `ktui_draw_invalidate()` would repaint the whole surface once a second for a small chart.
- Draw the tile before asking for its slot. `kch_tile_slot()` answers -1 until the tile's first
  `kch_tile_commit()`, so code that draws a tile only when a slot already exists never draws it
  and stays on the glyph layout for good.
- Decide the geometry before claiming the tile. Bailing out after `kch_tile_begin()` leaves the
  tile believing it drew that content, and the next frame presents a stale slot.
- A tile is never required, and its content hash leaves out the accent colour, because a theme
  change drops every tile through `kch_tile_reset()` at the same moment the icons are retinted.

A tile can be any size up to the grid. One sprite covers at most 16×16 cells, because the sub-cell
coordinate is four bits each way, so a larger tile is published as a grid of sprites, one per 16×16
block, each a view onto the same canvas: publishing it copies no pixels, and `kch_tile_draw()` writes
every block's cells. That is why a tile is drawn with `kch_tile_draw()` and not with
`ktui_draw_sprite()` on the slot, which covers the first block only. A program holds at most 24
tiles (`TILE_MAX`), and each tile's two halves take one sprite slot per block out of the table's
4,096 (`KTUI_MAX_SPRITES`). A tile that stops being drawn keeps its two canvases until
`kch_tile_drop()`; drop it only once a frame that does not draw it is flushed, and invalidate
the next frame, for the reason given in [libkchrome](c-libraries.md#libkchrome).

A chart is not drawn by hand. `kch_plot()` draws an area chart, or a mirrored pair on one axis, as
a tile of its own and answers -1 where the caller must draw its cells; `kch_plot_draw()` draws the
same chart into part of a canvas the caller already holds. Both follow the style in
[the design language](../03-architecture/design-language.md#a-data-trace-is-antialiased-chrome-is-not),
and the marks they are built from, `kcell_canvas_series()` and `kcell_canvas_line()`, are there for
anything else that plots data. The panel's meters and the charts in `kdos-res` are both drawn this
way, each with a cell chart for the -1 answer; see [kdos-res](../04-programs/kdos-res.md#the-charts).

A program that draws tiles turns its pixel tier on after `kdisp_init()`, because the cell size and
the output scale are the display's: `kicon_init(kdisp_cell_w(), kdisp_cell_h(), kdisp_scale())`,
which refuses a display with no pixel cell and so leaves every tile off, and
`kch_tile_enable()` from the program's `icons` setting. The cell and scale answered then are the
font's at 1 until the surface is on a screen: a whole-number output moves the scale and a
fractional one moves the cell, to the font's at the device size. So the program also registers a
function with `kdisp_on_scale()` that calls `kicon_recell()` with the cell read again, which rebuilds
the icons when either arrives or changes; in `kdos-shell`, `sh_pic_backend()` does it. Tiles follow
both by themselves. A pixel position handed to another surface, such as a popup's anchor, is a
logical one and goes through `kdisp_px_logical()`; see
[The output scale](c-libraries.md#the-output-scale). A dump never makes the call, so its frame is
the cell layout. On a theme change such a program calls `kicon_retint()` and `kch_tile_reset()`
before it redraws: icons are tinted when they are loaded and tiles are rasterised in the palette in
force, and both are cached by what they show, not by palette, so a program that skips either comes
up in the new theme wearing the old accent.

### Display text

A heading or a figure that is only text, taller than a row, is not a tile. Draw it with
`kch_display_text()`:

```c
/* in the draw, after kch_px_reset() */
kch_display_text(krect(x, y, cols, 2), "62%", KT_SURFACE, KT_ACCENT, KCH_ALIGN_RIGHT);
```

It is one op on the pixel layer, `rows` of the cell tall (two rows of a 16×32 cell are Terminus
doubled, pixel for pixel), in the cell font's face, cut to its rectangle and aligned in it. The
rectangle's cells go blank on the slot the backdrop owns and `bg`, when it is another slot, is laid
under the text as a flat rectangle, so the text sits on whatever band the cells were. It costs no
sprite slot and no canvas; a changed string re-rasterises and repaints only its own rectangle.

It needs a backdrop, and a slot cleared for the backdrop to show through. A popup has one
(`kch_px_popup()`); an ordinary window hands its page to `kch_px_flat(page_slot)` after
`kdisp_init()`, which paints the page in the same slot, opaque, so nothing on screen changes. Where
it cannot draw, `kch_display_text()` fills the rectangle with `bg` and draws the string as cells
on the rectangle's first row and answers 0; that is the frame tty1 and every `--dump` show, so lay
the rectangle out so that its cell form reads on its own. A figure that only repeats what the cells
already say is drawn where `kch_display_live()` answers 1 and not at all otherwise, as `kdos-res`
draws the number in its band; size its rectangle with `kch_display_cols(s, rows)`. The strings of
one frame share a 2048-byte pool and the list's 256 ops are shared with the plates, and a string
that does not fit takes its cell form.

## Colour

Colour comes from the eight palette slots and never from a literal: `KT_BG`, `KT_ERR`,
`KT_ACCENT`, `KT_WARN`, `KT_DIM`, `KT_MID`, `KT_SURFACE` and `KT_TEXT`. Use `KT_MID` for labels.
`KT_DIM` is a fill and measures below any contrast floor for text.

Emphasis is a fill with swapped slots, never a reverse attribute over a label. The attribute
inverts only the cells a glyph covers, so a two-word name comes out as one lit block per word.

A selected row's colours come from `ktui_sel_slots()`. A surface that works them out itself
(`bg = on ? KT_ACCENT : KT_SURFACE` is the shape to search for) drags a lit plate under the pointer
and puts the background colour on the label:

```c
int fg, bg;

ktui_sel_slots(selected, pane_has_keyboard, KT_SURFACE, &fg, &bg);
```

There are three states: an ordinary row is `KT_TEXT` on the page; the caret in a pane without the
keyboard is a marker in `KT_MID` with no fill; the caret in the focused pane is a `KT_DIM` fill with
the marker in `KT_ACCENT`. `ktui_sel_row()` gives the same answer with the fill and the `►` marker
drawn for you; the row's text starts two cells in. Pass the page slot, `KT_BG` or `KT_SURFACE`,
rather than letting the control guess, or a translucent window gets an opaque band across it.

A secondary column on a selected row takes `ktui_sel_dim()`, never `KT_MID` (the muted colour).
`KT_MID` on the `KT_DIM` selection fill measures below any reading floor, so a tag, a two-letter
code or a units suffix left muted vanishes exactly when the row is selected.

## A minimal surface

The pieces above fit together in one descriptor. This is the shape of a boxed `kdos-shell` window
with a `--dump` path, reduced from `src/desktop/kdos-shell/firewall.c` (326 lines, one of the
smallest complete windows in the tree and a reasonable file to copy). The `kdos_disp` array is
defined once per program, in `kdos-shell`'s `main.c`, and declared in `shell.h`, where `ShSurface`
and `sh_run()` are declared too.

```c
static KtuiKeys keys;

static void draw(void)
{
        int w = ktui_w, h = ktui_h;

        ktui_draw_fill(krect(0, 0, w, h), KT_BG);
        sh_frame(w, h, "Thing", KT_ACCENT, KT_BG, 1);
        /* … the content, the header band, the button bar … */
        ktui_hint("Esc", ktui_esc_verb(&keys));
        ktui_hint_row(&keys, krect(2, h - 2, w - 4, 1), KT_BG);   /* last */
}

static int on_event(KtuiEvent *ev)     /* after ktui_keys() passed it */
{
        if (ev->type == KT_EVT_KEY && ev->key == 'q')
                return SH_EV_CLOSE;
        /* the surface's own keys and pointer handling */
        return SH_EV_TAKEN;
}

int thing_main(int argc, char **argv)
{
        static const ShSurface s = {
                .cfg = { .role = KDISP_ROLE_TOPLEVEL, .cols = 68, .rows = 16,
                         .title = "Thing", .app_id = "kdos-thing", .keyboard = 1 },
                .keys = &keys,           /* keys.doc stays NULL: no help page */
                .draw = draw,
                .event = on_event,
        };

        return sh_run(&s, argc, argv);
}
```

`sh_run()` does the rest, in this order, and a front end on it cannot leave any of it out:

1. It reads `--font NAME` and `--dump`, hands every other word to the descriptor's `arg()` and
   prints `usage: <app_id> <usage>` with exit status 2 for one that nobody takes.
2. It calls `start(dump)`, where the surface reads what it shows, and then applies the theme.
3. With `--dump` it draws one frame offscreen at `.cols` × `.rows` through the same `draw()` and
   prints it, before `kdisp_init()` is reached. That is what makes the golden a picture of the real
   surface.
4. Otherwise it calls `kdisp_init()`, or prints `<app_id>: no display server` and exits 1, puts the
   popup plate under an overlay that asks for one (`.popup`, `.popup_bg`) and calls `ready()`.
5. Each pass polls the theme file and draws; the poll waits `timeout()` milliseconds (1000 without
   one), and `wake()` runs after it whether an event came or not. A timeout runs `tick()` and then
   the resize step; an animation's frame tick runs the resize step alone, and the next pass draws. An event goes to `ktui_keys()` first, where `CLOSE` ends the loop, `TAKEN`
   goes no further and a menu pick goes to `menu(id)`, and then to `event()`.
6. When the loop ends it calls `stop()`, where a surface saves what it holds, and
   `kdisp_shutdown()`. Neither runs on the dump or the no-display path.

`event()` answers `SH_EV_PASS`, `SH_EV_TAKEN` or `SH_EV_CLOSE`, and `sh_run_close()` ends the loop
from anywhere else, such as a control drawn inside a frame. While `typing()` answers 1, a text
field owns the keyboard: `ktui_keys()` is asked about `Esc` alone, so `F1` and `F10` do nothing
there. A surface that sizes itself from what it found (`kdos-about`) fills `.cols` and `.rows` in
before it calls `sh_run()`, and cleanup that holds on every path goes after the call.

`.frame = 1` puts every event and the draw after it inside a `ktui_frame_begin()` and
`ktui_frame_end()` pair, then draws once more with no event so that what the event changed (a
focus Tab moved, a page a button opened) is on the screen without waiting for the next key. The
dump draws inside a frame too. It is off by default because it changes three things a surface
with draw, key and hit functions of its own relies on: `ktui_frame_end()` walks the Tab ring on
any Tab that `event()` did not answer `SH_EV_TAKEN`; `ktui_id_base()` stops restarting the id
counter, so a group that points the focus at its own members counts from the page's ids; and the
hit lists swap once per frame. No shipped front end on the runner takes it yet; the runner's own
test surface in the dump harness pins what it does.

Ten front ends run on `sh_run()`: `kdos-about`, `kdos-chars`, `kdos-contacts`, `kdos-firewall`,
`kdos-note`, `kdos-print`, `kdos-trash`, `kdos-update`, `kdos-users` and `kdos-verify`. The rest
keep the loop in [The frame protocol](#the-frame-protocol), written out with the same steps in the
same order; a new front end starts on the runner unless its loop has a shape of its own.

## What libkwl and libkcell do for a surface

A surface built on `libkdisp` gets everything in this section without writing any code. It is
written out because each rule fails silently, and it is the list that anyone changing `libkwl` or
`libkcell`, or adding a second display implementation, must keep.

### A toplevel must ask for its frame

`libkwl` binds the xdg-decoration protocol and asks for a server-side decoration on every
toplevel. A client that never binds it has not said which side draws the decoration and gets
whatever the compositor guesses, which here is no frame at all: nothing to drag, no close button,
and the window controls unreachable by pointer. A compositor that does not offer the protocol has
no manager to bind, and the window is then undecorated.

This is easy to miss, because every other role is a layer or lock surface and has no decoration to
negotiate.

### Bind the layer shell at the right version

On-demand keyboard interactivity is a version-4 request of `zwlr_layer_shell_v1`. On an older
resource wlroots reads the request as *exclusive*, and the compositor then parks the seat's
keyboard on that surface and refuses focus to every window, so nothing typed reaches any window
until an overlay takes the focus and gives it back.

`libkwl` binds the layer shell at version 4, or at the compositor's version when that is lower.
Below version 4 it says so on standard error. The background layer then takes no keyboard
(`kwl: layer-shell v<N> has no on-demand keyboard; this surface takes none`), and an overlay that
wants the keyboard takes it exclusively until it exits (`… this surface holds the seat's keyboard
until it exits`). Nothing else reports the mismatch; the only symptom is that typing stops
reaching windows.

A layer surface that wants no keyboard at all must say so. `libkwl` sends
`KEYBOARD_INTERACTIVITY_NONE` explicitly for an overlay with `.keyboard = 0`, because a commit that
carries no interactivity state is not arranged by the compositor: the surface is created, mapped
and given a buffer, and never appears. `kdos-tip` is the surface that depends on this.

### A lock surface takes no pre-configure commit

Layer and xdg surfaces make an empty commit to ask for their first configure. On a session-lock
surface that commit is a protocol error (a lock surface committed with a null buffer), and the
compositor disconnects the client for it; because the compositor is right to keep the session
locked when the lock client dies, the machine is left locked with no prompt on it. `libkwl` skips
the commit for the lock role, which is configured as soon as it is created.

### Input the backend cleans

A motion that did not move is not a motion. An absolute pointing device, which is what every
virtual machine presents, sends the position again with a wheel event, so a handler would step the
selection and then put it straight back under a pointer that has not moved a pixel. The backend
drops a motion whose *cell* is unchanged, and an enter seeds the comparison so that an enter is
never swallowed.

An enter carries coordinates, and they are not optional. `libkwl` reports the enter as a synthetic
motion to the entry cell. Discarding them puts the first click after an enter at an impossible
position and leaves hover stale until the pointer moves, so a menu opening under a stationary
pointer would miss its first click.

A leave must be reported. `libkwl` sends it as a motion to (−1, −1); hover state with no leave
stays lit for the rest of the session.

Key repeat is the client's job. The protocol has none: the compositor sends the repeat rate and
delay, and every client repeats for itself. `libkwl` does so, falling back to a 400 ms delay and
25 repeats a second when the compositor sends nothing, and it shortens the poll timeout to the
next repeat, so holding a key repeats at the configured cadence rather than at whatever interval
the loop happened to poll at. A key held when the focus leaves stops repeating.

A wheel tick is not an axis event, and the two sources behave differently:

- A wheel is already quantised: a count of detents with each event. Running that through an
  accumulator leaves a remainder, so the next notch crosses the threshold twice and a list jumps two
  rows.
- A touchpad is not quantised, and sends a stream of small continuous values from which ticks are
  synthesised: one tick per 10 units (`KWL_AXIS_TICK`), at most five per pointer frame.

The `axis_source` field says which is which. `libkwl` binds `wl_seat` at the lower of what the
compositor offers and version 9. From version 8 the count arrives as `axis_value120`, in 120ths of a
detent, and a wheel with a high-resolution mode sends one detent as several fractions: they add up
across frames, a whole detent is one tick, and a reversal starts the count again. Below version 8
the count is `axis_discrete`, whole detents. On either path one pointer frame is at most one tick: a
front end that turns one host scroll into two delivers a single frame carrying a count of two, and
honouring the count would move a list twice as far, so the rest of the frame's count is discarded.
When the same doubling arrives as two frames, a rate limit catches it: a second tick in the same
direction within 20 ms of the last (`KWL_WHEEL_MIN_MS`, overridden by `$KDOS_WHEEL_MIN_MS`) is
dropped, and a change of direction always passes. The limit is for detents: a finger has none, and
a touchpad crossing the tick threshold in two frames 15 ms apart is a fast scroll, so finger and
continuous sources are never gated. `KDOS_WHEEL_DEBUG=1` prints what the compositor actually sends.

A finger that leaves the touchpad while still moving *coasts*. `libkwl` measures the release speed
over the finger's last 100 ms of vertical samples; above 0.3 units a millisecond it keeps feeding
ticks as though the finger were still moving, slowing by a factor of 0.996 every millisecond (a time
constant of 250 ms), until the speed falls under one tick every half second or three seconds have
passed. A finger held still before it lifts measures nothing in that window and does not coast. The
next scroll, a button press, a key or the pointer leaving the surface stops a coast, and nothing
coasts where motion is off. The coast's steps ride the caller's wait in pieces, as the overlay's
refreshes do, so a loop never sees its own timeout early. The constants are one `#define` each in
`kwl.c` and were set by reading the protocol rather than on a touchpad; the coast is not fed into
the raw stream, where a pixel guest gets the finger's own end of gesture and runs its own kinetics.

A *serial* is the number the compositor attaches to an input event, which a later request must
quote to show it answers that event. A serial must be retained from key, button, enter and touch
events. Setting the selection, starting a drag and setting the cursor shape each have to present
one, so a backend that discards them breaks the clipboard, drag and cursor changes, and the failure
does not point at the input code.

The event queue is a ring of 16 events, not one slot. The client library delivers a whole batch of
callbacks from a single read, so with one slot a button press followed in the same batch by its
accompanying motion would be overwritten before any consumer saw it. Consecutive motion collapses
onto motion; a button or a key never overwrites anything.

### Presenting a frame

A double buffer needs a shadow per buffer. The paint compares each frame with a record of what the
buffer holds so that it redraws only what changed; with two buffers alternating and one shared
record, the rows that changed while the *other* buffer was in flight would never be redrawn in it.
Each buffer carries its own shadow, and a full repaint is forced whenever a buffer has none or has
been resized. The damage reported to the compositor is still the global comparison with what is on
screen, cut to the changed span of each changed row rather than the row's width; only the paint is
per buffer.

A grid that scrolled is moved, not repainted. When a band of rows equals the buffer's shadow
shifted up or down (a terminal taking a line of output, a list stepped by the wheel), the band's
pixels are moved inside the buffer and the shadow with them, and the paint covers only the rows
the band exposed. Nothing is asked of the surface: it draws its cells as always, and the row
comparison finds the shift. See
[Scrolling by moving pixels](c-libraries.md#scrolling-by-moving-pixels).

A surface with a backdrop (pixel chrome under the cells, see
[libkchrome](c-libraries.md#libkchrome)) is held to the same rule only when the backdrop can be
repainted in part. Each buffer records the key of the backdrop it was last painted over. When the
backdrop's current key is the same, only the changed cells are repainted. When it differs but the
backdrop can say which pixels moved since that key (a plate that moved, appeared or vanished), the
cells over those pixels are repainted as well. Either way each repainted cell is laid back on its
own rectangle of the cached backdrop first. The damage is the changed cells plus the pixels that
moved between the picture on screen and the new one. A commit whose backdrop changed its alpha, its
edge, the palette, the size or the scale, or moved a pixel no cell covers (the rule, the remainder
past the last cell), is painted and damaged in full. A scrolled band over a backdrop is moved only
where the picture is the same at both ends of the move: over the bare body that is every row no
plate or other drawing op reaches, and over the graded popup and taskbar bodies it is none, so a list in a popup is
repainted as it scrolls.
`KDOS_PAINT_FULL=1` forces that full path for every commit of every surface, which is the
comparison to make when a surface shows stale pixels.

`KDOS_INSPECT=1` shows it. Every row whose cells a commit changed is tinted in the accent and fades
over a second, so a row that repaints on every frame stays lit; a panel in the top right corner
gives commits and stashed frames a second, the paint time, the frame callback's latency, what the
last commit changed, the sprite bytes and the hit rects; and each hit rect of a frame surface is
outlined, the focused one in the accent. It is the answer to "why is this surface busy" without a
rig photograph: a clock that lights its whole row every second, a list that repaints when nothing
moved, a surface throttled because it draws faster than the display. The overlay makes every commit
whole, so under it the paint time is a full paint's; read the rows lit, not the damage, and compare
the paint time with `KDOS_PAINT_FULL=1` rather than with an uninspected run. See
[libkwl](c-libraries.md#libkwl).

The scale and the resized buffer must land in one commit. Split them and the compositor sees a
buffer whose size disagrees with its declared scale for a frame.

An unchanged frame is not committed at all, and a frame callback the compositor never answers (an
occluded surface, an output that is off) is given up on after 100 ms, so an idle surface costs
nothing and a hidden one does not freeze.

### Animating

Something that moves on a surface is a `KtuiAnim` read by the draw, never a timer the loop runs.
Start it when the thing happens and ask it for its value where it is drawn:

```c
static KtuiAnim glow;

/* on the click */
ktui_anim_start(&glow, 0.0f, 1.0f, 1100, KT_EASE_IN_OUT, 2);

/* in the draw, on the pixel layer */
if (kch_px_live() && ktui_anim_running(&glow))
        kch_px_round(x, y, w, h, KCH_PLATE_RADIUS, kch_slot_rgb(KT_ACCENT),
                     (uint8_t)(0x66 * ktui_anim_value(&glow)));
```

The loop needs nothing more if it draws on every return of `poll_event`, as `sh_run()` and the
panel do. While an animation runs, `libkwl` returns a `KT_EVT_TICK` as
an event once per display frame, so the draw happens at the display's rate; one more follows the
end, which draws the end value, and then the loop is back on its own timeout. A loop that treats
every return of 1 as input, or runs periodic work on every pass, must tell a tick apart by its type:
the panel skips its sysfs measurements on one, and `sh_run()` never calls `tick()` for one. A loop
that waits on the display descriptor itself gets no ticks, and its animations stand at their first
frame.

A selected row that should slide rather than jump needs no animation of its own: draw it with
`kch_px_row_anim(key, item, cx, cy, cw, KCH_T_ACTIVE)` instead of `kch_px_row()`, with one key per
list and the selected index as the item, and keep the cell form of the selection for where
`kch_px_live()` is false. The plate eases to a new item's row and lands at once when the same item
has only moved with the page; see [libkchrome](c-libraries.md#libkchrome).

Put the motion on the pixel layer, over or under cells that are already final, and give it an end
value that says everything the motion said: on `tty1`, under `--dump` and with `motion = no` in
`comp.conf` the value is the end from the start, and a golden is always the settled picture. Where
the motion is the whole message, show a state for as long as it lasts instead: `ktui_anim_running()`
is still true with motion off, and `KtuiAnim.still` says the value will not move. Durations and
curves are in [the design language](../03-architecture/design-language.md#motion).

### Loading a font at a given size

`libkcell` handles both of these rules when it loads a font for a canvas or for display text. Any
new code that asks fontconfig for a sized face has to keep them too.

A repeated fontconfig property appends; it does not replace. The chrome font is
`Terminus:pixelsize=32`, and appending `:pixelsize=39` to it yields a pattern carrying two sizes,
of which the first wins, so every canvas would come out at the cell's own size while the text
still renders. Strip the size the name carries before appending yours.

A bitmap font cannot be asked for an arbitrary size: fontconfig answers with the nearest strike,
scaled by nearest neighbour where it is more than a fifth away, and a fractional scale draws uneven
strokes. `libkcell` asks fontconfig which strike and which factor it chose, and where the bitmap is
not exact it takes the family's outline twin, `Terminus (TTF)`, at the requested size (see
[libkcell](c-libraries.md#the-cells-size)). A face still more than a tenth short is retried with
`:scalable=true`, keeping whichever is closer. A machine with no scalable face keeps the bitmap,
which is the correct answer for a minimal install.

## Adding a name to kdos-shell

`kdos-shell` dispatches on the name it was started under: `main.c` compares the basename of
`argv[0]` with a table and calls the matching entry point. A new front end is therefore wired in
several places, and most of them fail silently.

1. Add `src/desktop/kdos-shell/<x>.c` with an entry point `int <x>_main(int argc, char **argv)`,
   which fills an `ShSurface` and returns `sh_run()` (see [A minimal surface](#a-minimal-surface)).
2. Add a `{ "kdos-<x>", <x>_main }` row to the `TOOLS` table in `main.c`.
3. Declare the entry point in `shell.h`. The table names it and the header is how every other file
   learns of it.
4. Add `ln -s kdos-shell "$PKG/usr/bin/kdos-<x>"` to `src/desktop/kdos-shell/build.sh`. A name in
   the table that the build does not link is a program nothing can reach. Until the link exists,
   the front end is reachable by starting the binary under a name the table does not contain,
   with the front end's name as the first argument (`bash -c 'exec -a kdos-dev kdos-shell
   kdos-<x>'`): a basename that matches no row falls back to selecting by the first argument.
   `kdos-shell kdos-<x>` does not work, because `kdos-shell` is the panel's own row and the panel
   rejects the argument.
5. Add a route if a person or a script should be able to reach it. `routes.c` reads
   `/etc/kdos/menu.conf` and then `$XDG_CONFIG_HOME/kdos/menu.conf` (by default
   `~/.config/kdos/menu.conf`), where a route named again replaces the system one. A route is a
   `verb.noun` name mapped to an argument vector, run without a shell, such as
   `setup.network = kdos-net`; a shipped surface needs its row in `fs/etc/kdos/menu.conf`.

   A surface missing from any one of the table, the header, the link and the route is a chord that
   opens nothing, and none of those four fails at build time. `testing/preflight.sh` checks that
   every command `menu.conf` names exists, and checks all four places for `kdos-store`; for any
   other name, keeping them together is your responsibility.
6. Give it `--dump` and commit goldens. That is four more edits, and a fifth where the loop is
   worth pinning, and the suite passes with none of them:
   - add `<x>` to the `for s in …` candidate list in `testing/selftest.sh`, so the file is compiled
     into the dump harness;
   - add **both** `FRONT_END(<x>_main);` and a `{ "<short name>", <x>_main }` row to
     `testing/fixtures/shell/dumpmain.c`. Every entry point there is declared weak, so missing
     either makes `dumpcheck --have <short name>` answer no and the golden block print
     `(skipped — not linked into the harness)`;
   - add a `golden <name> <WxH> <argv…>` call to `testing/selftest.sh`. The candidate list and the
     golden calls are separate, and being on the first buys no coverage at all;
   - add a fixture if the surface reads anything the host owns. `--fixture <dir>` replaying recorded
     output is the idiom: `kdos-print` over recorded `lpstat` and `lpinfo` output, `kdos-store` over a
     recorded catalogue, and `kdos-disks` pointed at a `kdos-mountd` socket that does not exist.
     Without one the golden records the machine that wrote it and fails everywhere else;
   - add a `keydrive <name> <WxH> "<script>" <argv…>` call to `testing/selftest.sh` for a surface
     with a loop worth pinning. It runs the surface's live path over a scripted display (see
     [Looking at it without a screen](#looking-at-it-without-a-screen)) and commits the frame it
     ends on as `drive-<name>-<WxH>.txt`.

   Every one of those omissions reads as a pass. `ls testing/goldens/ | grep <name>` after the run
   is what tells a covered surface from an uncovered one; the exit code cannot.
7. If another tool starts it, check the flags. `testing/preflight.sh` verifies that every flag one
   `kdos-shell` tool passes another is one the target accepts. An unknown argument prints a usage
   line to an error stream nobody reads and exits before a surface exists, so the symptom is a
   control that silently does nothing. A front end on `sh_run()` takes `--font` and `--dump` in
   `shell.c`, and the check reads that file for them.
8. If it opens files by type, give it a desktop entry. The `kdos-shell` front ends that have one,
   `kdos-peek`, `kdos-pix` and `kdos-burn`, ship it under `fs/usr/share/applications/` with `NoDisplay=true`
   and a `MimeType=` line, so a file of that type can be opened with it while the menu does not
   list it. The entry's file name is the front end's `app_id` plus `.desktop`.

## A program of its own

A desktop program that is not a `kdos-shell` front end is a port of KDOS's own, wired in three
places:

1. A directory `src/desktop/<name>/` holding the sources, a `kpkgbuild` and a `build.sh`. The
   `kpkgbuild` names no source, so there is nothing to fetch, and `build.sh` compiles from
   `$PORT_SRC`, the port directory itself, together with the libraries under `src/libs` (see
   [A port of KDOS's own](writing-ports.md#a-port-of-kdoss-own)). The desktop phase searches
   `src/desktop` for recipes, so the directory name is the port name.
2. A row naming the port in `script/05_desktop/packages.txt`, which is what puts it in the build
   (the ports that phase already lists are in
   [The ports catalogue](../06-reference/ports-catalogue.md#the-desktop-phase)).
3. A `.desktop` file installed by its own `build.sh` into `$PKG/usr/share/applications/`, so that
   the package owns its menu entry (see
   [Desktop entries belong to the port](writing-ports.md#desktop-entries-belong-to-the-port)). Its
   file name without the suffix must equal the program's `app_id`.

`kdos-res` is the example: `src/desktop/kdos-res/` holds `kpkgbuild`, `build.sh` and
`kdos-res.desktop`, its `build.sh` installs the entry to
`$PKG/usr/share/applications/kdos-res.desktop`, and `script/05_desktop/packages.txt` lists
`kdos-res`. Once the window has been opened, `kdos appid` reports whether the entry and the window
match.

The recipe hash of such a port covers the whole port directory and the whole of `src/libs`, so an
edit to any file under the directory rebuilds it, and an edit to any library rebuilds every port
of KDOS's own, not only the ones that link that library. Where the desktop phase sits in the whole
build is told in [How KDOS is built](how-kdos-is-built.md#the-desktop-05_desktop).

## Building and trying a change

Most of the work on a surface happens without a build and without a display.

| To | Run |
|---|---|
| Check the libraries, and that every consumer still compiles, on any host | `testing/selftest.sh` |
| Run the same suite with every surface dump and golden, in the development image (`kdos-devdeps`) | `testing/devdeps-image.sh` |
| Rebuild one desktop program in the build tree | `make build BUILD_ARGS="--phases 05_desktop --rebuild kdos-shell"` |
| See the change on a running desktop without rebuilding the ISO | `testing/quick.sh kdos-shell -- --sleep 3 --shot build/shots/x.png` |

The dump harness builds only on a host with `libwayland-client`, `wayland-scanner`,
`wayland-protocols` (including `ext-workspace-v1.xml`) and the fetched wlroots source tarball under
`ports/core/wlroots/`; elsewhere `testing/selftest.sh` prints `front-end dumps (skipped …)`. A front
end whose headers are missing on the host (`pixman`, `fcft`, ALSA, PipeWire, the image decoders) is
dropped from the harness with a note rather than failing it. A pass on a bare host therefore says
little about how a surface draws. `testing/devdeps-image.sh` builds the `kdos-devdeps` container
image and runs the whole suite inside it, which is the run where nothing is skipped (see
[Testing](testing.md#the-machine-where-nothing-is-skipped)).

`testing/quick.sh` builds the named ports with no packaging, packs exactly the files the package
database says those ports own, and patches them into the RAM overlay of a booted ISO (the
writable in-memory layer the live system runs on; see [Testing](testing.md#the-fast-loop) and
[Developing](developing.md)). It needs a built tree and the container image of the *rig*, the QEMU
harness that boots the ISO, drives it and photographs it ([Testing](testing.md#the-qemu-rig), and
the [glossary](../06-reference/glossary.md)); the rig writes a PPM file whatever the extension. A
new symbolic link in the recipe travels with the package, but a new row under `fs/`, such as a
`menu.conf` route, does not unless the file is named in `KDOS_QUICK_FILES` (a path relative to
`build/fs`) after the phase-1 filesystem step (`01_phase1:00_file_system.sh`) has copied `fs/`
into the build tree. When in doubt, run the narrowed `make build` with packaging. The fast loop is
not evidence about the shipped image.

## Looking at it without a screen

```sh
kdos-start --dump
kdos-menu system --dump-cells
kdos-res --fixture testing/fixtures/res --dump-size 66x10 --dump
kdos-term --dump 44x8 -e /bin/echo hello
```

| Flag | Produces | Catches |
|---|---|---|
| `--dump` | The cell buffer as plain text, drawn in the ASCII glyph tier | Geometry |
| `--dump-cells` | One line per non-blank cell, `row col U+XXXX fg bg attr`, drawn in the rich tier | Colour and attribute regressions as well |

Every `kdos-shell` front end except `kdos-ascii`, `kdos-mediad`, `kdos-netagent` and `kdos-ime`
takes `--dump`, and so does `kdos-res`. `kdos-term` takes `--dump COLSxROWS` with the size as its
argument. `--dump-cells` is taken by `kdos-start`, `kdos-menu`, `kdos-keys`, `kdos-settings`,
`kdos-find`, `kdos-teams`, `kdos-doc`, `kdos-pick`, `kdos-openwith` and `kdos-res`.

Most `kdos-shell` front ends draw their dump at a size compiled into the surface. A few take a size
on the command line: `--dump-size WxH` on `kdos-res`, `kdos-keys`, `kdos-openwith`,
`kdos-settings`, `kdos-teams` and `kdos-doc`, and `--dump-size W H` (two arguments) on `kdos-desk`.
The size a golden is taken at comes from `$KDOS_DUMP_SIZE=WxH`, which only the dump harness reads:
`testing/fixtures/shell/dumpmain.c` wraps `ktui_offscreen_init()` for it, and no shipped binary
reads it. Forcing a common size also checks that a draw pass does not assume the buffer is exactly
the size it asked for.

Dump at a size that forces degradation. A layout defect is usually a defect at *one* width, and a
full-size dump will not show it. The design language asks for 80×24 and 132×43 for a window, and
the popup's own size for a popup.

A `--dump` golden is in the ASCII tier, because `ktui_caps` is 0 with no terminal, so read the
surface at the vt tier (see
[the glyph tiers](../03-architecture/design-language.md#the-glyph-tiers)) before believing it reads
on a console. `testing/preflight.sh` checks every chrome glyph against the shipped console font.

A `--dump` never runs the loop, so no dump shows an event handler, the resize step or `ktui_keys()`
being asked first. The dump harness can also run a surface's live path: with `$KDOS_DUMP_KEYS`
set, `kdisp_init()` there answers with a display that reads the variable as a script of events,
and the frame the surface last presented is printed when it shuts down, followed by
`-- N of M events read`. A surface that closed itself stops short of the total. The script is
whitespace-separated steps:

| Step | Is |
|---|---|
| `up` `down` `left` `right` `home` `end` `pgup` `pgdn` `ins` `del` `tab` `btab` `enter` `esc` `bs` `space` `f1` … `f12` | That key |
| `ctrl+K`, `alt+K`, `shift+K` | A named key or one character with that modifier |
| One character | That character |
| `text:WORD` | One key event per character of `WORD` |
| `click:X,Y`, `rclick:X,Y` | A press and its release at that cell, left or right button |
| `wheelup:X,Y`, `wheeldown:X,Y` | One detent |
| `tick` | A poll that times out |
| `resize:WxH` | The grid changes size, and the poll times out |

The grid is `$KDOS_DUMP_SIZE`, else the size the surface asked for. Only the dump harness reads the
variable; `testing/selftest.sh` commits these frames through `keydrive` as `drive-*` goldens.

Goldens are committed and compared by `testing/selftest.sh`; `KDOS_GOLDEN_UPDATE=1` rewrites them.
Regenerating them is described in [Testing](testing.md#regenerating).

## When a surface does not appear

A surface that fails to come up usually exits without drawing anything, and the reason is on its
standard error. `libkwl` prints:

| Message | Means |
|---|---|
| `kwl: protocol error <code> on <interface>#<id>` | The compositor refused a request and disconnected the client; the interface names the request |
| `kwl: the compositor closed the surface before it was configured` | The surface was closed during the handshake; `kdisp_init()` fails rather than returning success for a window nobody will see |
| `kwl: layer-shell v<N> has no on-demand keyboard; …` | The compositor offers a layer shell older than version 4; see [above](#bind-the-layer-shell-at-the-right-version) |

Four environment variables trace what the libraries see. `KDOS_WHEEL_DEBUG=1` prints every discrete,
high-resolution and continuous axis event, every pointer frame's result and every coast step. `KDOS_PANEL_DEBUG=1` makes the panel
say why a pixel tile declined and print one line per meter sample with its scale and value, and
makes `kdos-tip` print the size and position it was placed at. `KDOS_PAINT_FULL=1` paints and damages every
commit in full (see [Presenting a frame](#presenting-a-frame)): a defect that goes away under it is
in the partial repaint or the damage, and one that stays is in the drawing. `KDOS_INSPECT=1` draws
the changed rows, the frame timing and the hit rects over the surface itself (see
[Presenting a frame](#presenting-a-frame)); a frame surface that is on screen but not answering a
click shows there whether it registered a hit rect where the click landed.

## The checklist

The checklist in [the design language](../03-architecture/design-language.md#the-checklist),
restated as a procedure:

| # | Do | Check with |
|---|---|---|
| 1 | Box the surface with `ktui_draw_box()`, title on the top edge | `--dump` |
| 2 | Header band, group headings and button bar from `libkchrome` | `grep kch_` |
| 3 | Slots for colour, `KT_MID` for labels; selection through `ktui_sel_slots()`, secondary columns through `ktui_sel_dim()` | `grep KT_DIM` in foreground positions; `grep 'KT_ACCENT :'`, where a match is a surface deciding for itself |
| 4 | Motion, press, wheel, scrollbar drag and header sort | `grep -c KT_EVT_MOUSE`; zero is the defect |
| 5 | A hit map recorded from the draw | Resize the window and click the top row |
| 6 | Dump at two sizes and commit both goldens | `testing/devdeps-image.sh` |
| 7 | Read it at the vt tier, and use no glyph the console font lacks | `--dump`; `testing/preflight.sh` reads the shipped font |
| 8 | One `KtuiKeys`; `ktui_keys()` first (on the runner, `.keys` names it), `ktui_hint_row()` last, on every path | `grep -c ktui_hint_row`; the dump path counts |
| 9 | Every raised state a declared layer, never an `Esc` branch | `grep KT_K_ESC`; a remaining case is one the ladder should own |
| 10 | A help page in `fs/usr/share/kdos/doc/` if `keys.doc` names one, and `doc` left `NULL` if not | `testing/preflight.sh` |

## See also

- [The design language](../03-architecture/design-language.md) — the specification this chapter
  implements
- [The C libraries](c-libraries.md) — `libktui`, `libkdisp`, `libkwl`, `libkchrome` and the rest
- [kdos-shell](../04-programs/kdos-shell.md) — the largest set of worked examples
- [kdos-res](../04-programs/kdos-res.md) — a toplevel window with a terminal mode and charts
- [Testing](testing.md) — the dump harness, goldens and fixtures
- [Developing](developing.md) — build targets and the fast iteration loops
- [How KDOS is built](how-kdos-is-built.md) — the whole build, of which the desktop phase is one step
- [Writing ports](writing-ports.md#a-port-of-kdoss-own) — the recipe for a program of KDOS's own
- [Glossary](../06-reference/glossary.md) — terms used across the book

<!-- book-nav -->
---

*Part V — Building and developing, chapter 36.* Previous: [35. The C libraries](c-libraries.md) · [Contents](../README.md) · Next: [37. Testing](testing.md)
