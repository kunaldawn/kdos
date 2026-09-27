# The design language

This chapter is the visual and interaction specification that every KDOS surface follows: the
panel, the Start menu, the settings, the resource monitor, the installer, the lock screen and every
popup. It describes the cell grid everything is drawn on, the window frame, the shared controls,
colour, the keys and pointer gestures every surface answers, touch, the glyph sets that decide what
can be drawn on a text console, pictures, and the frame the compositor draws round other programs'
windows. It is written for people building or reviewing a KDOS surface, and for anyone who wants
to know why the desktop looks the way it does.

A *surface* here means any program, or any front end of a program, that draws its own screen with
the KDOS toolkit; `kdos-shell` alone provides more than fifty of them. Read
[The architecture overview](overview.md) first for where these programs sit in the running
system. If you are about to write a surface, read
[Every surface is a grid of character cells](#every-surface-is-a-grid-of-character-cells) and
[The checklist](#the-checklist) first, then
[Writing desktop software](../05-developer/writing-desktop-software.md) for the code.

The rules are not a matter of taste. A surface that ignores them looks like a different program
placed inside the desktop.

## Every surface is a grid of character cells

A KDOS surface has no scene graph, no widget tree with its own layout engine and no vector drawing.
It is a two-dimensional buffer of *cells* (`KtuiCell` in `src/libs/libktui/ktui.h`). Each cell
holds one character, a foreground colour slot, a background colour slot and a set of attributes:
bold, reverse, underline (with a style: single, double, curly, dotted or dashed), italic,
strikethrough and overline. Drawing means writing cells. Presenting means comparing the buffer with
what was last shown and sending only the cells that changed.

The same buffer is presented to three kinds of destination, and nothing above that layer knows
which one it is talking to. Each destination is reached through a *backend*, a `KtuiBackend`
table of functions:

| Destination | Backend | Where it lives |
|---|---|---|
| A terminal, including the Linux text console `tty1` | the built-in terminal backend | `libktui` (`ktui_draw.c`) |
| A Wayland window under the compositor | `kwl_backend` | `libkwl` |
| An offscreen text dump (`--dump`, which prints one frame as text; a committed one is a *golden*) | the offscreen buffer | `libktui` (`ktui_offscreen_init`) |

On `tty1` the terminal backend installs the palette into the console with `PIO_CMAP` and reads the
mouse from `/dev/input/event*` itself, because the Linux console reports no mouse events of its own.
`libkdisp` sits one level above the backends and decides which display server a surface talks to;
it names no implementation itself, and the one that ships is `kwl_impl` in `libkwl`. That is why
the resource monitor looks the same on `tty1`, in a window, and in a committed test file.

`libktui` needs nothing beyond the C library and two of KDOS's own libraries that need nothing
either, `libkbase` and `libkcolor`. The installer compiles all three into itself in the first build
phase, before any other library is built, and that constraint shapes several rules below: anything
that needs pixels, fonts or a display connection lives in another library.

The consequence to keep in mind is that **a control is one row of text tall.** Where that is wrong,
the answer is a picture drawn into whole cells, not a second renderer. See
[Pictures are an enhancement layer](#pictures-are-an-enhancement-layer).

Three things on the screen are not cells:

- **The compositor's own chrome.** Window title bars, the root menu and the window-switcher display
  are drawn by the compositor with Pango, in `Terminus (TTF)` at 24 points (set in
  `fs/etc/skel/.config/kdos-comp/rc.xml`), which is 32 pixels at 96 dpi and so one cell tall. The
  cell grid itself is drawn from the bitmap Terminus face; Pango cannot render bitmap fonts, which
  is why the compositor asks for the TTF conversion by its own family name. See
  [The compositor's decoration is part of the set](#the-compositors-decoration-is-part-of-the-set).
- **The pixel layer under the cells.** A surface with a Wayland backend may record plates, rules
  and rounded ends that are painted beneath its cells. Layout, hit testing and every dump stay in
  cells. See [The pixel layer under the cells](#the-pixel-layer-under-the-cells).
- **A boxed application** (one running in a container) draws whatever its toolkit draws, inside a
  frame this desktop owns.

No KDOS surface is drawn by another toolkit. The input-method candidate window shows how far that
goes. An input-method engine normally draws its own candidate list with its own renderer, which on
this desktop would be a rounded, antialiased panel over a text-mode screen. `kdos-ime` draws it
instead, with the same chrome and colour slots as everything else, by speaking `kimpanel`, the
input-method framework's generic D-Bus panel protocol. It is not an input method and knows nothing
about any language: the engine decides the candidates and `kdos-ime` draws them.

## A window is a double-line box

```
╔═[ Resources ]═════════════════════╗
║                                   ║
║  body starts at column 1          ║
║                                   ║
╚═══════════════════════════════════╝
```

`ktui_draw_box` draws it; its last argument chooses the double-line set or the single one. The title
is bracketed rather than padded with spaces, because two blanks either side of a title make the rule
look broken where it stops, and the eye reads a missing segment rather than a label. The box adds
the brackets and the padding itself and trims whatever spaces the caller passed, so `" Settings "`
and `"Settings"` draw the same. A title is drawn only when the box is more than six columns wide,
and is clipped to eight columns less than the box.

- Column 0, the last column and the last row belong to the frame. The body starts at column 1 and
  stops one row short of the bottom. The title hangs on the top edge.
- The header band (`kch_header`) fills from column 1 to the second-to-last column. A surface that
  draws a header with no box round it therefore shows an empty one-column margin, which is the sign
  that a box was forgotten.
- A surface too small for a frame keeps the whole area rather than drawing a box with nothing
  inside it.
- Under the compositor, the server-side decoration (the frame the compositor draws round a window)
  takes the box's place: a top-level window asks `kdisp_decorated()` and, when the compositor is
  drawing a frame, skips its own and keeps only the background (`sh_frame` in `kdos-shell`). Two
  nested frames on screen mean a program drew chrome the compositor had already drawn for it.

## The chrome primitives

*Chrome* is the part of a surface that is not its content. It is drawn by `libkchrome` and by
nothing else. A second implementation of a control is not updated when the first changes, and the
two diverge.

| Primitive | Draws |
|---|---|
| `kch_header` | The two-row accent band across the top, with an optional icon, title and subtitle, and a rule under it; returns the first body row |
| `kch_group` | A heading inside the body, followed by a rule to the right margin |
| `kch_buttons` | The verb bar: a row of buttons (at most six), dropped from the right when narrow |
| `kch_button_at`, `kch_hover` | Which button the last frame drew under a cell, and where the pointer is, so a button lights under it |
| `kch_list_wheel`, `kch_list_clamp` | The scrolling rule for a list |
| `kch_scrollbar` and `kch_scrollbar_press` / `_drag` / `_release` / `_grabbed` | A scrollbar that can be dragged; up to four per surface |
| `kch_tile_*` | A block of cells drawn as pixels |
| `kch_tone`, `kch_tone_alpha`, `kch_popup_alpha`, `kch_slot_rgb` | Shading and body opacity derived from the palette |
| `kch_px_*` | The plates, rules and rounded ends a backend with pixels records under the cells |

The controls come from `libktui`, and there is one of each. A surface that draws its own is a
second answer to a question this desktop has already settled.

| Control | What it is |
|---|---|
| `ktui_button`, `ktui_check`, `ktui_radio` | A verb, a flag, one of a set |
| `ktui_slider`, `ktui_slider_*` | A number on a track: press, drag, wheel, or an end cap for one step |
| `ktui_dropdown_*` | A choice, as a list that opens under its row |
| `ktui_input` | A line of text with a caret |
| `ktui_textarea_*` | Several lines of text with a caret |
| `ktui_list`, `ktui_table` | Rows, and rows with columns |
| `ktui_sel_slots`, `ktui_sel_row`, `ktui_sel_dim` | What a selected row looks like, for every surface that has one |
| `ktui_rows_*` | Which row the pointer is on, for a surface that draws its own rows |
| `ktui_table_event` | The same for a table: wheel, press, pick, Back |
| `ktui_tabs_*` | A strip of pages |
| `KtuiMenu`, `ktui_modal_*` | A pane of verbs, and a question |
| `ktui_progress`, `ktui_gauge`, `ktui_sparkline`, `ktui_heat` | Progress and measurements, in four shapes |

Colours here are named by *slot* (`KT_TEXT`, `KT_MID`, `KT_DIM`, `KT_ACCENT` and four more; see
[Colour](#colour)), and the *accent* is the scheme colour chosen with `kdos theme`.

A button is a plate with a `░` shadow one column to the right and one row below it, drawn in
`KT_DIM`. The shadow is what stops a filled rectangle with a word in it from reading as a selected
row. A focused button also carries `►` and `◄` at its two ends, so that focus is shown by a shape as
well as a colour; a disabled button has no shadow, because it cannot be pressed.

A slider is drawn as `◄ ████■▒▒▒ ►  55`. The track is `▒` in `KT_DIM`, the run up to the value is
`█` in `KT_MID`, and the thumb is `■` in the accent while the slider is not focused (in `KT_TEXT`
when it is), so the accent marks the value and nothing else. On a selected row (a `KT_DIM` fill)
each role steps up one slot, to `KT_MID` for the track and `KT_TEXT` for the run, so the track does
not vanish into the fill. On a row filled with `KT_ACCENT` the thumb is `KT_SURFACE`, or `KT_BG`
when focused. The end caps are dropped below 14 columns (`KT_SLIDER_MIN_W`) and the number below
ten.

Most controls are a `draw` / `key` / `hit` trio with one frame-level call on top. That is what
lets a surface running its own event loop, such as `kdos-display` or `kdos-audio`, use the same
control as one written inside the immediate-mode frame (`ktui_frame_begin()`), in which a surface
redraws every control on every frame and reads input as it draws, and which `kdos-settings`,
`kdos-prompt` and the installer use. A control that existed only inside the frame would have to be
written a second time by every other surface. A control that takes the focus also records what it
is for a screen reader (`ktui_announce`); see [Accessibility](../02-user-guide/accessibility.md).

### The pixel layer under the cells

A backend with pixels can paint beneath the cell grid. While a surface draws, `libkchrome` records
a display list of plates (`kch_px_plate`), selected-row bars (`kch_px_row`), rules and rounded
rectangles; the Wayland backend replays it under the cells at the output's scale. The rule is
*pixels for paint, cells for layout*: the fallbacks, the hit maps and every `--dump` stay in cells,
so a surface is correct without the layer and only better with it.

The plates take their colours from a *tone ladder* (`kch_tone`): a rest tone, a hover tone and a
focused tone, each a `kcol_mix()` of two scheme colours and each laid down at its own alpha, so an
accent change retints them. The ladder exists because the palette's dark end is compressed: the
surface and background slots measure between 1.00:1 and 1.20:1 against each other in every accent,
so a panel painted in its own background slot is the same colour as the desktop behind it. The
focused plate is solved rather than chosen: `libkchrome` walks the mix from 68% down and stops at
the first plate that both stands off the bar and carries its label at 7:1, falling back to the best
separating plate where no mix reaches 7:1. A bar's body is laid at 80% opacity and a popup's at 95%
(`kch_popup_alpha`), because a menu is read and a taskbar is glanced at. The plate radius is three
pixels (`KCH_PLATE_RADIUS`); a larger radius would sit badly beside window frames that are square
on purpose.

`kch_px_popup()` gives a popup that body in one call, with no bright edge line, because a popup
draws its own `╔═[ Title ]══╗` and a second border a few pixels above it reads as a fault.
`kch_px_bare()` is the same hand-off with no body at all, for a surface that is only separate cards,
such as a stack of notifications.

### Bars and rows

A button bar drops buttons rather than vanishing. Buttons are ordered most-useful-first and dropped
from the right when the surface is too narrow, never half-drawn. A bar that disappeared entirely
would leave only a row of key hints (see [The hint row](#the-hint-row)), which is what the buttons
exist to replace.

Anything that draws over part of a row owns all of it. A button bar shares its row with the status
line, and the single column between two buttons belongs to neither, so a leftover character would
show through the gap. The bar clears its whole span first, and `kch_buttons` returns the leftmost
column it took so the status text can be clipped to what is left. `ktui_sel_row` follows the same
rule and clears the full row before it draws its marker.

## The keys every surface answers

Five keys mean the same thing on every surface, and the bottom row of the surface says what the
other keys do at that moment.

| Key | What it does | Declared by |
|---|---|---|
| `F1` | Opens this surface's help page in `kdos-doc` | `KtuiKeys.doc`, drawn first in the hint row |
| `F10` | Opens the surface's menu bar | `KtuiMenu.has_bar` |
| `Shift+F10` | Opens the menu for the thing under the selection | `KtuiKeys.ctx_at` |
| `Alt+letter` | Opens a menu pane by its underlined letter | the `&` in a pane title |
| `Esc` | Steps back one level, and closes only from the top level | the Esc ladder, below |

A surface holds one `KtuiKeys`, calls `ktui_keys()` first in its event dispatch, and calls
`ktui_hint_row()` last in its draw. `ktui_keys()` takes every event, not only keys, because it
routes into the surface's menu first and a menu also answers the pointer. It returns
`KTUI_KEY_PASS` for everything it does not own, so the surface's own dispatch sees the rest
unchanged. Thirty-nine source files outside `libktui` and its self-test call it.

### Help and absent keys

A key with nothing behind it is neither advertised nor answered. Help pages are text files in
`/usr/share/kdos/doc/<name>.txt` (in the tree, `fs/usr/share/kdos/doc/`, which holds nine pages and
an index). A surface with no page leaves `doc` as `NULL`, and `F1` then passes the key on: a key
that opens an index reading *no such document* teaches that help is broken. `testing/preflight.sh`
refuses a `keys.doc` that names a page which does not ship. The same rule holds for `F10` on a
surface with no menu bar, and for `Shift+F10` on one with nothing selected.

### The Esc ladder

`Esc` walks a ladder of declared layers, checked at the moment the key arrives and never cached. A
surface registers each raised state (an open dropdown, a dialog, a search field) once with
`ktui_keys_layer()`, outermost first and innermost last, up to six of them. Each press of `Esc`
closes the innermost layer that is up. Each layer is a question ("is this open?") rather than a
stored flag, so a dialog dismissed by a click leaves nothing behind to swallow the next keystroke.
With no layer up, `ktui_keys()` returns `KTUI_KEY_CLOSE` and the surface closes itself; the toolkit
does not own a program's lifetime. An open menu is always innermost, above the ladder.
`ktui_esc_verb()` names what `Esc` does at that moment (a layer's verb, `Cancel` while a menu is
open, otherwise `Close`), so the hint row never reads *Esc Close* on a screen where Escape goes
back.

### The hint row

The hint row is built during the draw, by whatever has the focus. `ktui_hint(key, verb)` and
`ktui_hint_if(cond, key, verb)` add hints to a pool of at most twelve; `ktui_hint_row()` clears the
pool first, then draws what it held. Clearing first matters because `kdos-shell` is one binary with
many front ends, and a pool emptied only at flush time would carry one surface's hints into the
next `--dump` in the same process. The key is drawn in the accent, the verb in `KT_MID`, and hints
are separated by a `│` in `KT_DIM`. No row is drawn on a window shorter than eight rows. A fixed
string cannot follow the focus, and a row naming keys the focused control does not answer is worse
than no row, because the value of the line is that every key on it works.

Each hint is drawn whole or not at all: a fragment of one hint against the start of another is
worse than an empty half-row. A status message is different. Where a surface's status row beside its
button bar shows either a message or the key hints, the message takes whatever room is left, because
it describes what the person has just done.

The row drops its tail: a hint that does not fit ends the row, and every hint after it is lost
with it. Surfaces push `Esc` last, so on a narrow surface it is the first thing lost. If a surface is
too narrow for everything it answers, drop a hint deliberately rather than moving `Esc` earlier, or
make the surface wider.

### Menus and accelerators

A menu's `sel` counts items, never drawn rows. A caller's `show` callback can hide rows, and a
selection counted in drawn rows lands on a different item the moment one is hidden. The same
callback answers both the drawing and the hit test, from one walk, because two copies of a
visibility rule eventually disagree and a click then runs the row above the one under the pointer.
An item with no label is a rule: it is drawn and can never hold the selection.

An accelerator is marked, not guessed. `&` before a letter in a label or pane title marks it;
`&&` is a literal ampersand. `ktui_menu_label` underlines the letter, except on a Linux console,
where it brackets it as `[F]ile`: the console has no underline, and drawing one there puts a colour
on screen that no slot owns.

## Colour

Colour comes from a *slot*, never from a literal value. There are eight slots and no more, because
the console font has 512 glyphs: the Linux console uses the foreground intensity bit as the ninth
glyph bit, so colours 8 to 15 cannot be used as a foreground. Designing for eight means a text
console and a true-colour window show the same picture. The same reason is why bold is suppressed
on a Linux console.

| Slot | Role | Scheme field |
|---|---|---|
| `KT_ACCENT` | The accent | `primary` |
| `KT_ERR` | Urgent | `urgent` |
| `KT_WARN` | Secondary: a caution, not a failure | `secondary` |
| `KT_TEXT` | Body text | `text` |
| `KT_MID` | Labels, secondary text, rules | `pdark` |
| `KT_DIM` | A fill; see below | `dim` |
| `KT_SURFACE` | Raised chrome background | `variant` |
| `KT_BG` | The background | `backdrop` |

A scheme has a ninth field, `deep`, the darkest ground, which no slot takes. It is what a scheme's
legibility is measured against, and it feeds derived colours such as `kcol_muted()` below.

The values behind the slots are one table in `libkcolor` (`KCOL_SCHEMES` in
`src/libs/libkcolor/kcolor.h`), expanded at compile time by everything that draws. Nobody keeps a
second copy of the numbers, which is why `kdos theme <accent>` repaints the whole desktop. The table
holds eight accents: `phosphor`, `amber`, `ice`, `bone`, `norton`, `borland`, `perfect` and `paper`.
`paper` is the one light scheme. The default, `bone`, is named (`KCOL_DEFAULT_ID`) rather than taken
from a position, so the table can be reordered without changing what an unconfigured machine starts
in. How to choose one is in [Theming](../02-user-guide/theming.md#the-eight-accents).

Every contrast ratio in this section is the WCAG 2 contrast ratio, computed from the eight schemes
in that table; a range gives the worst and best accent.

### The two exceptions

**A colour that is not the palette's to name.** A program running in a terminal may ask for a 24-bit
colour, or for one of the 256-colour palette's cube and grey entries. No palette names those, they
do not follow an accent, and reducing them to eight slots would ruin a photograph. Such a cell
carries the literal colour beside the slot it reduces to (`fgc`, `bgc` and `ulc` in `KtuiCell`, with
one attribute bit per colour saying which literal is meaningful). Terminal content reaches a frame
through `ktui_draw_put`. The only other literals are the shadow and the blend described below, which
are mixed from the current palette each time they are drawn. A display that draws in slots alone,
such as a dump or a sixteen-colour terminal, reads the slot and ignores the literal. Chrome never
names a colour of its own, because a piece of chrome holding a fixed literal would stop following
`kdos theme`. The sixteen ANSI colours stay slots for the same reason: they are colours this palette
names.

**Translucency**, which is a mix of two colours rather than a colour of its own. A surface that
wants its body seen through sets `KDispConfig.opacity`, a percentage where `0` means unset and is
treated as 100. Below 100 the display dims the one slot the body is drawn in (`KT_BG` for a
top-level window, `KT_SURFACE` for everything else) and uses an alpha-capable buffer, so the
wallpaper and the windows show through while the text and the fills stay opaque. It is the
background slot only, because a translucent character is one nobody can read. The panel and
`kdos-term` use it.

For a rectangle of cells rather than a whole surface, the toolkit also has a pair of calls:
`ktui_draw_bg_take()` copies the background colours of the rectangle, and `ktui_draw_blend()` mixes
what has since been drawn there back towards them. The result is written as each cell's literal and
the slot is left as drawn, so a display that does not show literals (a `--tty` run, a test dump,
`tty1`) shows an opaque rectangle, which is the correct answer where there is nothing to mix with.
Both ends of the mix come from the current palette and are recomputed every frame, so an accent
change moves them like everything else. A picture cell is skipped, because a picture's pixels are
not a background. No surface in the tree calls the pair; only the self-test exercises it.

### `KT_DIM` is a fill, not a label colour

Used as a foreground, `KT_DIM` measures only 1.42:1 to 2.12:1 against the surface and background
slots across the eight accents. `KT_MID` measures 3.40:1 to 6.30:1. Nothing is comfortably readable
below about 3:1.

So a label is `KT_MID`. Hint rows, empty-state messages, help text and the brackets round a button
all belong there. Searching a new surface for `KT_DIM` in a foreground position is how this is
checked.

A two-state colour is a different matter. Where `KT_DIM` is one half of a pair, such as a
scrollbar's track against its thumb or an unpinned mark against a pinned one, it carries the
difference, and changing it to `KT_MID` would erase the state rather than make it readable. Those
pairs stay `KT_DIM`.

For text that must be muted and still readable there is a derived colour, `kcol_muted()`: the
scheme's `deep` field mixed 56% of the way towards its `text` colour. It measures 3.47:1 to 6.26:1
against the background and surface slots, above 3:1 in every accent, and the self-test asserts that
it sits more than twice as far from `deep` in luma (perceived brightness) as `KT_DIM` does. Every
muted text role in the theme generators uses it. `KT_DIM` keeps fills, borders and selection
backgrounds, where contrast is not the question.

### Emphasis is a fill with swapped slots

Never use the reverse attribute over a label. Reverse inverts only the cells a character covers, so
a two-word name comes out as one lit block per word with a hole between them, and looks correct for
every name that happens to contain no space.

Instead, fill the rectangle, swap the foreground and background slots, then draw the text.
`kch_header` is drawn this way: an accent fill with the title in `KT_SURFACE` on it.

### A selected row is `KT_DIM` under `KT_TEXT`, and the accent is one cell

```
    Files                FM  ■        a row
  ► System Monitor       MO  ■        the selection, in a pane without the keyboard
  ►▓Git▓▓▓▓▓▓▓▓▓▓▓▓GI▓▓■▓            the selection, in the focused pane (▓ is the fill)
```

`ktui_sel_slots()` decides this, and no surface works it out for itself; `ktui_sel_row()` fills the
row and draws the marker. There are three states, because they say three different things:

| State | Drawn as |
|---|---|
| A row | `KT_TEXT` on the page, no fill |
| The selection, pane not focused | `►` in `KT_MID`, label in `KT_TEXT`, still no fill |
| The selection, pane focused | the row filled `KT_DIM`, `►` in `KT_ACCENT`, label in `KT_TEXT` |

`KT_TEXT` on `KT_DIM` measures 8.30:1 in the worst accent and 10.22:1 in the best, so the label
clears 7:1 everywhere, and the `►` marker (`KT_ACCENT` on `KT_DIM`) clears 3.68:1 everywhere. The
fill is quiet because it is a fill, and the accent is spent on the one cell that says where the
selection is. The page slot is a parameter, `KT_BG` or `KT_SURFACE`, because a control that guessed
would paint an opaque band across a translucent window.

Do not fill a row with `KT_ACCENT`. Writing `bg = on ? KT_ACCENT : KT_SURFACE` puts the background
colour on the label and drags a lit plate across the screen under the pointer, and it breaks the
rule above: `KT_DIM` owns selection backgrounds.

A pane without the keyboard keeps its marker and loses its fill. A surface with two panes has two
selections; filling both says both are live, and dropping the inactive one loses the place that
pane will return to.

`KT_ACCENT` as a fill has three uses, and none of them is a list row:

- the header band drawn by `kch_header`;
- a **state indicator**: the current workspace in the pager (the panel's one cell per workspace),
  the focused window's task button on a panel with no pixel layer, the installer's current step;
- a **button**, where the plate is the control: a focused primary button.

Each is one thing on screen rather than one per row, and each means *this*, not *here*.
`grep 'KT_ACCENT :'` over the tree lists every conditional use of the accent; most are foregrounds
or state indicators, and one used as a list row's background is a surface making its own rule. The
process, application, box and device tables in `kdos-res` break this rule: they fill the selected
row with `KT_ACCENT` and the hovered row with `KT_MID`.

The secondary colours cannot be read on the selection fill: `KT_MID` on `KT_DIM` measures 2.19:1
to 4.21:1, and `kcol_muted()` 2.44:1 to 3.38:1. A row with a secondary column (a tag, a two-letter
code, a unit) therefore raises that column to `KT_TEXT` when the row is selected in a focused pane,
and `ktui_sel_dim()` answers that in one place. Left muted, the right-hand half of the row would
disappear exactly when someone is looking at it.

### A shadow darkens, and the clamp is what guarantees it

The drop shadow uses the same literal-colour mix as `ktui_draw_blend()`, and the modal, the menu,
the open dropdown list and the installer's dialogs draw one. `ktui_draw_shadow()` darkens rather
than erases: every cell in the one-cell strip to the right of and below the rectangle keeps its
character, and both its colours are mixed towards `KT_BG`, keeping 110/255 of the original, so the
window underneath stays readable. A picture under the strip is left alone. The background slot is
set to `KT_BG` beside the literal, which is the whole shadow where literals are not shown. A shadow
that wrote blank cells would cut a rectangular bite out of whatever it falls on, which looks like a
rendering fault rather than depth.

After the mix, `ktui_draw_shadow()` clamps each colour channel so it is never lighter than it
started. `KT_BG` is not the darker of the two background slots in every
accent: in `bone` the background is lighter than the surface in red and green, and in `ice` in green
and blue. Without the clamp those accents would glow along two edges of every window. The clamp
costs nothing where the background is already darker, and it is what keeps a new accent from
getting a luminous shadow.

### A mark on a fill is a shape, not a contrast

A character on a filled plate is read by the plate around it. The dark slots of this palette look
like one colour (`KT_BG` measures 1.00:1 to 1.20:1 against `KT_SURFACE`), so dark ink on a bright
fill cannot be told apart by colour from the chrome behind the fill, whichever of the two it is
drawn in. What separates them is the fill surrounding the mark.

So a mark drawn in `KT_SURFACE` on a plate that sits on a `KT_SURFACE` body must be a character
that stays clear of every edge of its cell; that clearance is the whole boundary between the mark
and the body. A mark that runs close to the bottom of the cell reads as the plate ending early
rather than as a mark on it.

It is a threshold, measured on the console font `ter-kdos32n` (16 by 32 pixels, so 512 pixels a
cell):

| Character | Lit pixels (of 512) | Rows used | Clear rows below |
|---|---|---|---|
| `_` | 24 | 27–28 | 3 |
| `■` | 108 | 10–21 | 10 |
| `X` | 80 | 6–25 | 6 |
| `↓` | 68 | 6–25 | 6 |

Six clear rows of thirty-two hold; three do not. `■`, `X` and `↓` can carry a mark on a plate, and
`_` cannot. The compositor's own title-bar buttons are not bound by this, because their ground is a
different colour from their mark; see
[The compositor's decoration is part of the set](#the-compositors-decoration-is-part-of-the-set).

The same measurements explain why ink on a bright fill is dark. `KT_TEXT` drops as low as 1.10:1 on
`KT_ACCENT` and 2.17:1 on `KT_ERR`, so a light character on a lit or urgent plate is effectively
invisible. `KT_SURFACE` clears 3.40:1 on `KT_MID`, 5.23:1 on `KT_ERR` and 5.51:1 on `KT_ACCENT` in
the worst accent of each.

## The pointer contract

Every surface answers the pointer, and answers it the same way. A surface with rows and no motion
handling is a picture: the only way to discover that a row is a control is to click it.

| Gesture | Means |
|---|---|
| Motion | Lights what is under it; on a list that follows hover, selects the row |
| Left press | Activates. On a row that is already selected, opens it |
| Wheel | Steps the selection while the list fits; moves the view when it does not (see [the two wheel rules](#drops-and-the-wheel)) |
| Right press | Backs out one level, then closes |
| Scrollbar | Is dragged, and its end caps step one row |
| Column header | Sets the sort; a second press reverses it |
| Click away | Closes a transient surface |

### Everything the keyboard can do, the pointer can do

A verb reachable only by a key chord does not exist for someone using a mouse, and the shipped key
card is not the first thing anybody reads. The rule works both ways: every control answers the
arrow keys and `Enter` as well as a click. That is why the window menu carries every per-window
verb, and why a number in a settings form is a slider rather than a printed value.

The usual way to break the rule is a line such as `if (ev.type != KT_EVT_KEY) continue;`, which
throws away every pointer event, so the surface cannot be operated with a mouse at all however much
of it looks clickable. A surface that draws its own rows uses `ktui_rows_event` rather than
inventing its own answer:

- a press moves the selection;
- a press on the row already selected picks it;
- each wheel detent steps the selection one row, whether or not the list fits;
- the right button is Back.

`ktui_rows_event` ignores motion, so hovering over a row does not select it. A list that scrolls
needs the fit-aware wheel described under [Drops and the wheel](#drops-and-the-wheel) instead.

### The display draws the pointer, never the surface

A surface owns its cells and the display owns the screen. A pointer drawn by the surface would cost a
round trip for every motion event and lag behind the hand. The display already holds the device and
already knows where it is.

In the cells, the pointer is the reversed cell under it: the pointer every text mode has drawn, and
as fine as a grid of characters goes. Over a Wayland window there is also a real arrow, and the
surface does not draw it: `libkwl` answers the pointer-enter event with
`wp_cursor_shape_device_v1.set_shape`, and the compositor draws a cursor at the device's own pixel
position, so it moves as smoothly as the hand holding it. The enter event carries the only serial a
set-cursor request may use, so a client that does not answer it has no cursor image, and the pointer
disappears over every surface the library draws.

### What a press would do

The toolkit defines seven pointer shapes, each saying what a press at that spot would do:
`KT_PTR_ARROW` (the default, and every unhandled case), `KT_PTR_IBEAM`, `KT_PTR_SIZE_NS`,
`KT_PTR_SIZE_WE`, `KT_PTR_SIZE_NWSE`, `KT_PTR_SIZE_NESW` and `KT_PTR_MOVE`.

The list is what this desktop can mean, not what any protocol carries. It is neither the cursor
shape protocol's list nor X11's, for the same reason the raw event codes are not libinput's: a
number that happens to equal an upstream one is a coupling neither side can see. Whatever forwards a
shape maps it in a switch.

A backend holds no window state, so it cannot choose a shape; it cannot tell a border from a
box-drawing character. Whatever owns the pointer names the shape with `ktui_draw_cursor_shape()`,
only when it changes, because a pointer crossing a window spends hundreds of frames over the same
thing, and `KtuiBackend.pointer` carries it to the backend.

**The shape is a hint and never a promise.** No surface sets a shape, and no backend fills
`KtuiBackend.pointer`: a terminal, a dump, `tty1` and a Wayland window all reverse the cell whatever
the shape says, and on Wayland the compositor shows the arrow that `libkwl` requests. So a control
must say what it does in its own cells; the shape is a second telling for those who can see it. A
backend that meets a shape it does not recognise draws the arrow rather than failing, so a newer
session with more shapes still has a pointer on an older display.

Nothing shows a busy pointer. `libkdisp`'s cursor list (`enum kdisp_cursor`) includes default,
text, hand and progress shapes, and no surface sets any of them: nothing here tracks a window as not
answering in a way a pointer could report.

### The flush decides which is drawn

`ktui_draw_cursor(x, y)` names the cell the pointer is on; `ktui_draw_flush()` decides how it is
drawn. The contract:

- **A backend with pixels may claim the pointer through `KtuiBackend.pointer`.** Returning `1` is a
  promise that the cells reach the screen exactly as the surface composed them, with the backend
  drawing its own arrow; returning `0` declines for that frame. A backend that leaves the entry
  `NULL`, as every backend in the tree does, gets the reversed cell. The arrow cannot be drawn in
  `libktui` itself, because that library has no pixel or font dependency and an arrow needs a
  pixel buffer.
- **The hook is called on every flush, including those with no pointer to report.** A negative `x`
  means no pointer, and it is the only thing that tells a backend to remove the last arrow; a
  backend told nothing would leave one where the hand last was.
- **The hook carries a shape beside the cell, and a backend may ignore it.** A shape needs its own
  hotspot: an arrow points with its tip, a resize arrow and an I-beam with their middle. Drawing
  every shape from a fixed corner would put a resize arrow half a cell off the border it belongs
  to, enough to make a border seem to move when you reach for it.
- **The hook carries cells, and a backend that owns the device may draw finer.** What the hook
  says is that there is a pointer and which cell the surface believes it is on, which decides whose
  it is to draw.
- **A cell marked `KT_A_GUEST` gets no pointer.** The bit marks a cell drawn from an embedded
  client's own pixels, where the client's compositor has already drawn a cursor, and a second one a
  cell away is the one nobody is aiming with. The rule is applied before the hook, so every backend
  is told the same thing. The flush honours the bit, but nothing in this tree sets it.

Where the reversed cell is what gets drawn:

- **The reverse goes on for the flush and comes straight back off.** The back buffer accumulates
  the surface's cells between frames, so a reverse left in it would stain every cell the pointer
  ever crossed.
- **Taking it off is what erases it.** The front buffer keeps the reversed cell and the back buffer
  does not, so the cell differs and repaints as itself the moment the pointer leaves. One XOR does
  both jobs.
- **A cell with no character still shows the reverse.** A space carries colour and nothing to draw,
  and the fill pass has already painted it in its background slot, so a painter that skipped it
  would lose the swap and the pointer would show only over text. `kcell_paint` fills those cells
  with the foreground colour instead.
- **A picture cell keeps the pointer.** A desktop icon, a panel icon and a picture in a terminal
  are things people aim at, so the reverse goes over them too: the flush swaps the cell's character
  for a blank alongside the XOR and puts it back afterwards. Inverting the picture's own pixels
  would be wrong, because `KT_A_REVERSE` is also how selected text is drawn, and a panel icon on a
  hovered row would come out in negative.
- **Motion is a change even when no cell's content changed**, so the frame is marked dirty for it;
  otherwise the pointer would move only when something else on screen did. On a backend that draws
  its own arrow that is two jobs: the cells under the old position go back into the row comparison
  (the only thing that erases an arrow the cell model does not know about), and the move pushes the
  frame past the nothing-changed shortcut so it is presented at all. Skipping either leaves a trail
  of arrows down the screen.

Nothing is drawn before the first motion, since the pointer starts on no cell, so a machine with no
pointing device does not show a pointer in its corner all session.

Motion finer than a cell travels only in the raw input stream. `KtuiBackend.poll_raw` reports every
key as a switch and every motion in the backend's own pixels, beside the cell-level queue; `libkwl`
fills it, and nothing in the tree reads it. Nothing this desktop routes on is finer than a cell.

## Touch

A touchscreen follows the same contract, because one recogniser turns fingers into the pointer
events above. `ktui_gesture_feed` in `libktui` is fed by `wl_touch` in `libkwl`; a recogniser
in each backend would duplicate the code and let the two backends interpret the same fingers
differently.

| Gesture | Reported as | Also arrives as |
|---|---|---|
| Tap | `KT_GEST_TAP` | a left press and release |
| Long press | `KT_GEST_LONG` | nothing; a surface wanting a context menu reads the gesture |
| Drag | `KT_GEST_DRAG` | motion with the button held |
| Two-finger scroll | `KT_GEST_SCROLL` | one wheel step per row the fingers' centre crosses |
| Pinch | `KT_GEST_PINCH` | nothing |
| Edge swipe | `KT_GEST_SWIPE_EDGE` | motion with the button held, and it says which edge it came from |

A tap is a touch shorter than 250 ms (`KT_TAP_MS`) that never left its cell; a long press is one
held past 500 ms (`KT_LONG_MS`) without leaving it. An edge swipe is a drag that began in the
outermost row or column. So a surface written without touch in mind already works under a finger,
and one that wants more reads `ev.gesture` on a `KT_EVT_TOUCH` event.

The recogniser follows four rules:

- **Movement is measured in cells.** A drag begins when the finger leaves the cell it went down in.
  That is coarse on purpose: everything here is a grid, and a threshold in pixels is a number a
  surface drawn in cells cannot see. Pointer drags of the desktop icons use the same one-cell rule,
  from `libkwm`'s `kwm_drag_threshold()`.
- **One finger of two says nothing yet.** Moving away from a still finger is both a pinch and a
  scroll; the answer comes when the second finger agrees or disagrees in direction. Guessing makes
  the gesture flip between the two midway, which is unusable.
- **Only the first finger synthesises a pointer press, and the second finger closes it.** A press
  under every finger would click whatever each finger landed on, so two fingers down to scroll would
  select two rows and a pinch over a button would press it. Presses and releases stay paired: the
  release goes out on the finger that opened the press, and a finger that never opened one closes
  nothing when it lifts.
- **The synthesised wheel is counted at the fingers' centre, one step per row.** Touch arrives one
  finger at a time, so a step per finger movement would count the same row twice and scroll twice
  as far as the hand moved. The gesture's own `dy` keeps the per-finger movement, for a surface that
  forwards the gesture.

A long press has no event to arrive on, because the finger is down and nothing moves, so it is
checked with `ktui_gesture_tick` from the backend's idle wait and reported once, using the same
`CLOCK_MONOTONIC` milliseconds the recogniser was fed. A deadline compared against a different clock
never expires, and nothing reports that.

A long press is `Shift+F10` with a finger, and the contract answers it: `ktui_keys()` opens the
surface's context menu on `KT_GEST_LONG`, so a surface that declared one supports touch without a
touch path of its own. It opens at the finger, not where `ctx_at` says the keyboard focus is,
because someone holding a row expects the menu on that row; `ctx_at` is still asked, so a surface
that refuses a key refuses a finger too. A tap opens nothing: a tap is a click, every control
already handles one, and a menu on every tap would put a pane under every finger.

## Drops and the wheel

A drag-and-drop *drop* is a position and a payload, and an event has room for only one of them.
`KT_EVT_DROP` carries the position; `ktui_drop_take` returns the payload once, so two surfaces in
one process cannot both act on one drop. A `text/uri-list` payload arrives exactly as sent, URIs
separated by CRLF with comment lines included, because what a URI means differs from surface to
surface.

Drop and wheel handling depends on four facts about the event stream:

- **Motion arrives as a drag event.** Wayland reports plain movement and dragged movement the same
  way, so treating an event's "pressed" flag as a button state would make every mouse movement a
  click. The button state is what must be remembered across events, which is why a slider arms on
  press, follows motion, and commits on release.
- **A motion that did not move is not a motion.** An absolute pointing device sends the position
  again with each wheel event, so a naive handler would step the selection and immediately put it
  back under a pointer that has not moved. `libkwl` drops motion when the cell is unchanged.
- **A wheel step is a press with no release.** A wheel detent arrives as a press whose button is
  `KT_MB_WHEEL_UP` or `KT_MB_WHEEL_DOWN`, and no release follows it. Dispatch on the button, not on
  the press alone, or a scroll will raise, focus, move or close whatever is under the pointer. A
  scroll reaches the thing under the pointer and does nothing else to it: it does not raise it, take
  the keyboard or start a move or resize.
- **One press, not a double click.** Nothing in this toolkit measures a double click. Opening a
  detail view is not destructive, and the destructive verbs sit behind a confirmation.

The fit-aware wheel is `kch_list_wheel()` in `libkchrome`: a list that fits gets a selection step,
and a list that scrolls gets its view moved by three rows (`SH_WHEEL_ROWS`) with the selection left
where it was. `kch_list_clamp()` keeps the view inside the list afterwards. `ktui_rows_event` does
not follow that rule: it steps the selection on every detent, fits or not, and leaves the view to
follow the selection. Twelve call sites in eleven source files use the fit-aware rule; the five
`kdos-shell` surfaces that draw their rows through `ktui_rows_event` (the recorder, trash, command
palette, theme and find surfaces) step the selection on every detent instead.

The flag that says "pull the selection into view" (`follow`) is set by everything that moves the
selection and by nothing that scrolls the page, or the next frame would undo the scroll. A surface
that assigns a new view from a scrollbar drag clears its own flag for the same reason.

A list that scrolls shows a scrollbar. It is one column wide: a `▲` cap, a `▒` track in `KT_DIM`
with a `█` thumb in `KT_MID`, and a `▼` cap, all characters every glyph tier has, so it looks the
same on a console. A cap at the end of its travel is drawn in `KT_DIM`. Nothing is drawn when
everything fits, because a full-height thumb says nothing a missing bar does not. Where rows would
reach that column, the rows give up a cell rather than the bar drawing over them: a selected row is
a filled band, and a bar on top of it would put a notch in it. `kch_scrollbar` is called every
frame, including frames where the list fits, so a bar that has stopped being drawn stops being
grabbable.

## Hit maps

A *hit map* records where each clickable thing was drawn, so a click can be matched to it. It is
recorded from what was drawn and never worked out a second time from the geometry the drawing
computed.

Both copies agree until the window is resized, a sidebar collapses or a border is added, and then a
click lands on the row above the one under the pointer. In a network dialog, that means joining the
wrong network.

Two rules keep the hit map and the drawing in step:

- **The frame subtracts its body origin once** and hands a page its own coordinates. A page that
  subtracts an origin itself is the second copy.
- **A control with no room records an empty span.** A hit map that outlives what it describes sends
  a click on a narrow screen to a control the current frame did not draw: someone aims at the clock
  and operates whatever sat there in an earlier frame.

## The glyph tiers

What a surface can draw depends on the terminal or display. `libktui` picks from its capabilities
(`ktui_caps`):

| Tier | When | Box drawing and symbols | Ramp (bars, charts) | Ramp levels |
|---|---|---|---|---|
| rich | UTF-8, not a Linux console | `glyph_utf8` | eighth blocks `▏…█`, `▁…█` | 8 |
| vt | UTF-8 on a Linux console (`KT_CAP_LINUXVT`) | `glyph_utf8` | `░ ▒ █` | 3 |
| ascii | no UTF-8 | `glyph_ascii` | `. : #` | 3 |

The box-drawing and symbol table (`glyph_utf8` in `src/libs/libktui/ktui_draw.c`) is the same for
the rich and vt tiers, so it must stay inside what the console font can draw. Only the ramps differ
(`ktui_chart.c`). With no UTF-8, every character above ASCII is drawn as `?`.

The vt tier exists because the console font, `ter-kdos32n`, has 512 glyphs. A character the font
lacks is drawn as a blank on `tty1`, so an eighth-block bar there is not merely coarse, it is
invisible. Three levels is the true resolution of that font.

What the console font has: `░ ▒ █`, the single box-drawing set, the double rules `═ ║`, the double
corners `╔ ╗ ╚ ╝` and `╬`, and `· • ■ … ° ↑ ↓ ◀ ▶`.

What it does not have: eighth blocks, half blocks (`▀ ▄`), `▓`, braille, and the double tees
`╠ ╣ ╦ ╩` and the mixed joins `╡ ╞`. The double set is only partly there: the rules and the
corners are, the tees are not.

The `terminus-font` recipe (`ports/core/terminus-font/build.sh`) builds the font from Terminus's
`xos4-2` character set and swaps six spacing diacritics for the double box-drawing characters
`═ ║ ╔ ╗ ╚ ╝`, which the KDOS block logo needs. The font answers to 627 codepoints in all, because
its duplicate tables map extra codepoints onto existing shapes. `▲ ▼` are drawn as `↑ ↓`, and
`◄ ►` and `← →` as `◀ ▶`, so the control glyphs `◄ ►` and the direction glyphs `◀ ▶` in the table
draw the same two shapes on `tty1`. A glyph the font answers to is not necessarily a distinct
shape, so two table entries that must look different on `tty1` need two different glyphs, not two
codepoints mapped to one.

Anything that can reach `tty1` stays inside the vt tier, and `glyph_utf8` is entirely inside it.
The toolkit uses that table whenever the backend reports UTF-8, which the Linux console does, so an
entry the font lacks would be a blank on `tty1`, with nothing reporting it. A slider built on `▓`
would draw its filled part as nothing at all.

`testing/preflight.sh` reads the built `ter-kdos32n` (from `build/fs`) and refuses any `glyph_utf8`
entry the font cannot draw, because a written list of what the font carries is the thing that goes
stale. Two consequences are visible in the controls: the slider separates its track from its run by
colour slot rather than by a third shade, and a button's shadow is `░` rather than a half block.

A frame's title is bracketed `[ like this ]`, and the brackets are ASCII for a second reason beyond
the font. The test dumps are drawn at the ascii tier (`+=[ backup ]=====+` is what the top of a
committed frame looks like), and every join in that table collapses to `+`, so a title bracketed
with `╡ ╞` or `┤ ├` would read there as two corners in the middle of the top rule. `[` and `]` are
the same at all three tiers, exist in every font, and are the bracket the DOS file managers put a
title in.

A wide character is measured, not assumed. The toolkit computes display width with its own table
(`ktui_wcwidth`, not the C library's) and reserves a continuation cell, so double-width text does
not break the row layout. The console font has no wide characters, so on `tty1` a wide character is
written as a single `?` in one cell. Chrome that must read on both stays inside the small set.

## Pictures are an enhancement layer

A *sprite* is a picture registered in a numbered slot of the toolkit's *sprite table* (up to 4096
slots) and drawn into whole cells. The *icon atlas* is the theme's icon set, one file
(`/usr/share/kdos/icons/atlas.kia`, read by `libkicon`).

Icons and pixel tiles are drawn into whole cells, with the picture's slot and position within it
encoded in the cell's character. The ordinary row comparison therefore already tracks what changed,
and a text backend draws the sprite's fallback character instead.

Every surface must draw correctly when the picture is unavailable. The icon lookup answers "none" on
a terminal, when icons are turned off (`icons = no` in `~/.config/kdos/comp.conf`), when there is no
icon atlas, and when the sprite table is full; each of those is a normal state, not an error. The
test harness (`testing/fixtures/shell/dumpmain.c`) stubs the lookup to exactly that, so a committed
reference frame is the character grid, and a layout that only lines up once the pictures load is a
broken layout.

A *pixel tile* (`kch_tile_*`) is a block of cells a surface paints as pixels through a canvas, such
as the panel's Start button and its meters. Two rules apply:

- **A tile owns two slots and alternates between them.** A cell encodes the slot, not the picture,
  so redrawing a tile's contents in place changes no cell, the comparison sees nothing, and the frame
  is never presented; a clock tile would freeze at the minute it was first drawn. Swapping slots on
  every content change repaints exactly the rows it covers.
- **Decide the geometry before claiming the tile.** Giving up after claiming it leaves the tile
  believing it drew that content, and the next frame presents a stale slot.

`kdos theme` drops every tile (`kch_tile_reset`), because each was rasterised in the palette being
replaced.

### Damage, for a picture that changed in place

A new picture in the same slot changes no cell, which is what `ktui_draw_dirty()` is for. An
animation's next frame writes cells identical to the last, so the flush finds nothing to send and
the screen would keep the first frame indefinitely. Marking the rectangle the slot covers costs that
rectangle; the alternative, repainting the whole screen for every arriving tile, costs every
character on the desktop and a full upload dozens of times a second.

A backend that keeps its own copy of the previous frame must implement `dirty` and mark every copy
it keeps. `libkwl` keeps three: the cells the compositor is showing (which the damage rectangles
are cut from) and one per shared-memory buffer (which each paint compares against). Mark only the
buffer copies and the pixels land in a buffer the compositor is never told to re-read; mark only the
screen copy and the damage names rows nothing repainted. Every one of these failures is silent and
looks like an animation that stopped. A backend without `dirty` compares against the `prev` frame
it is handed and needs nothing more.

A backend that caches uploaded pictures compares the sprite's generation counter (`gen`), not its
pointer: an animation re-registers the same key, and the allocator often hands the freed picture's
memory straight back, so a pointer comparison would take the first frame and no other.

### A picture from a terminal is the same sprite

A picture shown in a terminal uses the same sprite mechanism. A slot covers at most 16 by 16 cells,
so a larger picture becomes a grid of sprites sharing a key prefix, handled all-or-nothing: it is
evicted and re-registered as a unit rather than leaving three quarters of a photograph on screen.
Each backend does what it can:

| Backend | What a picture is |
|---|---|
| Wayland | Real pixels, scaled to the cells it occupies |
| A text terminal, a dump, or a display built without a pixel library | The fallback shade, in every cell of the picture |

A picture that shows as nothing is worse than one that shows as a mark, because blank cells look
like output that never arrived. That is why the tiled path carries a fallback character rather than
a space.

A tile that is a control carries the control's state in its cells' background slot. The backend
fills a cell's background before drawing the picture over it, so that slot is the button's body
wherever there is no pixel layer to record a plate into. `KT_SURFACE` is right only where there is
one, since the recorded plate shows through it; on a plain character grid it would leave the button
with no fill, no edges, and no visible hover or open-menu state. So the slot follows the same ladder
the pixel plate draws: a quiet fill at rest, the accent under the pointer, the warning colour while
the control's own menu is open, with the ink following the fill. The Start button is the example.
An icon sitting inside a wider field shows its state through the field instead.

A sprite beside text is two cells wide and one tall. A cell is twice as tall as it is wide (16 by
32 pixels at the default font), so two cells across one row is a square on the same line as the
text. A two-by-two block next to a single row of text centres the picture across the boundary
between rows and makes the whole surface look misaligned.

## The compositor's decoration is part of the set

The window frame is the one piece of chrome a KDOS program does not draw for itself. `kdos theme`
generates it for the current accent into `~/.config/kdos-comp/themerc-override`, the labwc theme
file the compositor reads last, and the compositor re-reads it on the same `SIGHUP` that retints
everything else. It is designed to match the cell grid:

- **Square corners** (`<cornerRadius>0</cornerRadius>` in `rc.xml`). A rounded corner is the one
  thing a cell grid cannot express, so it is the one thing that gives a server-drawn frame away.
- **A two-pixel accent border** (`border.width: 2`), because a hairline disappears beside a
  32-pixel cell. An inactive frame is drawn from the same scheme with the accent removed: its
  border is the `dim` colour, its title bar the `deep` ground, and its rule and label are `deep`
  mixed 25% and 45% towards `text`. No second hue appears, so focus does not read as two kinds of
  window.
- **A title bar carrying the same double rule the grid draws with**, broken by the title and by each
  button, so it reads as `════ Title ════[_][=][X]`. This is the `flat kdos` title texture of
  `kdos-comp`, a fork of labwc (see [kdos-comp](../04-programs/kdos-comp.md)); its `colorTo` is the
  rule's colour.
- **Buttons are 8×8 bitmaps enlarged exactly four times to 32×32 with nearest-neighbour
  filtering**, so they are hard-edged blocks rather than smeared glyphs; a fractional multiple would
  give one stroke two pixels and the next three. The marks (`src/desktop/kdos-comp/src/theme.c`) are
  the compositor's own: a bar, a framed box, a cross and a stack of rules. A button image is drawn
  in the accent (`window.active.button.unpressed.image.color`) on a title bar that is a different
  colour, so the minimise bar works there with the one clear row it keeps beneath it. On the cell
  grid the same bar would fail the six-row threshold in
  [A mark on a fill is a shape](#a-mark-on-a-fill-is-a-shape-not-a-contrast), which is why the two
  sets of marks are allowed to differ.
- **The close button is the urgent colour** (`window.active.button.close.unpressed.image.color`),
  and the others are the accent. It is the one window control that cannot be undone, and on an
  eight-colour palette that difference is the whole signal.
- **The hover plate is translucent** (`window.button.hover.bg.color` carries alpha `66`). The
  compositor has no separate hover images; it lays that colour over the plain button image, and an
  opaque colour would paint the symbol out, leaving every button blank under the pointer at the
  moment it most needs to say what it is.
- **Menus are sized for the cell.** The generated file sets `menu.width.max: 900`, because at
  32-pixel text the compositor's default cap of 200 pixels holds about eleven characters.
- **A box chip.** A window from a box whose profile sets its own accent (`accent =` in
  `~/.config/kdos/boxes/<name>.conf`) carries a square of that colour at the left of its title
  area, the height of the title bar, so you can see which box it came from. A box with the
  session's own accent draws no chip, so a default installation shows none.

Style overrides written by `kdos theme style` are kept in `~/.config/kdos/style-themerc` and
appended after the generated block each time it is written, so they survive an accent change.

## The checklist

A new surface is not finished until every line is answered.

1. `ktui_draw_box` around it, with the title on the top edge.
2. `kch_header` for the band, `kch_group` for headings, `kch_buttons` for verbs.
3. Colour from slots; `KT_MID` for labels; fills for emphasis, never the reverse attribute; no
   `KT_ACCENT` fill on a list row.
4. Every gesture in [the pointer contract](#the-pointer-contract): motion, press, wheel, right
   press as Back, scrollbar drag, header sort, and click-away for a transient surface.
5. A hit map recorded from the draw, with coordinates handed down by the frame.
6. `--dump` at the sizes the surface is used at (80x24 and 132x43 for a window, its own size for a
   popup), with a reference frame (a *golden*, under `testing/goldens/`) committed for each.
7. Read it at the vt tier before believing it works on `tty1`.
8. One `KtuiKeys`, with `ktui_keys()` first in the event dispatch and `ktui_hint_row()` last in the
   draw, on every path including the `--dump` one, because that call is what clears the hint pool.
9. Every raised state declared with `ktui_keys_layer()` rather than handled in an `Esc` branch.
10. A help page in `fs/usr/share/kdos/doc/` if `keys.doc` names one, and `doc` left `NULL` if not.

If `grep -c KT_EVT_MOUSE` on a new source file returns zero, line 4 has not been answered.

## See also

- [Writing desktop software](../05-developer/writing-desktop-software.md) — implementing all of this
- [The C libraries](../05-developer/c-libraries.md) — `libktui`, `libkchrome`, `libkcolor` and the rest
- [Theming](../02-user-guide/theming.md) — the accents these slots resolve to, and `kdos theme`
- [Accessibility](../02-user-guide/accessibility.md) — what the controls announce, and to whom
- [Testing](../05-developer/testing.md) — goldens and the dump harness
- [kdos-shell](../04-programs/kdos-shell.md) — the largest set of surfaces following this chapter
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor that draws the generated frame
- [How KDOS differs](../01-philosophy/how-kdos-differs.md#toolkits-on-the-host) — why the host has no widget toolkit, so every surface is cells
- [The window model](window-model.md) — where the windows those frames surround go
- [Glossary](../06-reference/glossary.md) — surface, slot, sprite, golden and the other terms used here

<!-- book-nav -->
---

*Part III — Architecture, chapter 18.* Previous: [17. The security model](security-model.md) · [Contents](../README.md) · Next: [19. The window model](window-model.md)
