# The C libraries

Nineteen static libraries under `src/libs/`, the constraint they are built under, the dependency
direction that must not be violated, and the invariants each one exists to protect.

Everything KDOS writes is built on these. Adding one is a small decision; giving one a new
dependency is not.

## The constraint

**A `libk*` library links nothing but the C library — with exactly one declared exception.**

That is not minimalism for its own sake. `libktui` has to be usable in **phase 1**, before any
library exists to link against, because the installer is built there and links the toolkit. If a
library ever needs a real link flag, every phase-1 consumer moves to a later phase with it.

**`libkwl` is the exception, and being a separate archive is how the rule survives it.** It is
`libktui`'s Wayland backend — the cell grid painted into a compositor surface instead of a terminal
— so it needs a font renderer, a pixel library, a keyboard library and a Wayland client library,
none of which phase 1 has. Splitting it out rather than folding it into the toolkit is what keeps
the installer linking zero libraries on the first bootable image.

`libkcell` is a separate archive for the same reason one level down: a consumer wanting the cell
painter is not made to link a Wayland client library to get it.

## The set

| Library | Prefix | Owns | May link |
|---|---|---|---|
| `libkbase` | `kb_` | Allocation and its failure hook, fatal and warning output, strings, files, paths, locking, monotonic time, group membership, the argument-vector builder and process helpers, and the freedesktop trash | Nothing |
| `libkcolor` | `kcol_` | **The palette table**, colour-space conversion, mixing, the readable muted colour, the hue-family classifier, remapping and retinting | Nothing |
| `libktui` | `ktui_` | Terminal ownership, the cell buffer and its diff, key and mouse decoding, character width, paste, immediate-mode widgets, modals, **the keys contract — the hint row, the Esc ladder and the menu**, **the selection rule every surface draws its rows with**, the three glyph tiers, charts, offscreen rendering | Nothing |
| `libkxdg` | `kxdg_` | Desktop entries, the MIME glob table, **the one correct way to turn a command line into an argument vector**, and the places column | `libkbase` |
| `libkpkg` | `kp_` | The package database, the ports tree, dependency parsing and solving, version comparison, the recipe and build-config hashes | `libkbase` |
| `libksig` | `ksig_` | Signing and verification, key files, keyrings. **The one library with vendored third-party source** | `libkbase` |
| `libkbuild` | `kbuild_`, `kj_` | Phase discovery, the phase metadata block, the build plan, the snapshot inventory, a read-only structured-data scanner | `libkbase` |
| `libkproc` | `kpr_` | Every reading about the running machine, from a **movable root**: processes, uptime, container identity, processor, memory, block devices, network, power, graphics, sound PCMs — and the sample ring | `libkbase` |
| `libkpack` | `kpk_` | The pack format: the footer, the metadata blob, the requirement solve, the payload hash, the signature block, and the index | `libkbase`, `libksig`, `libkpkg` |
| `libkvt` | `kvt_` | The terminal: the VT100-VT520 state machine, the screen, scrollback, selection, the pty, and one render boundary that turns it all into cells. **A hard fork of libtsm 4.7.1** | `libktui` |
| `libkimg` | `kimg_` | **The only place untrusted image bytes are decoded**, and reachable by anything that can write to a terminal. Two entry points — one picture, or every frame of the one format that has more than one — five optional decoders, and a budget enforced from the header the format declares *before* any allocation | pixman, plus png/jpeg/webp/sixel/gif where present |
| `libkkms` | `kkms_` | **The cell grid on a screen**: seat, connector, mode, a dumb buffer, libinput and xkb. The one thing on the console path that needs a GPU device, which is why only the view links it | `libkcell`, plus drm, input, seat, xkb, udev |
| `libkcon` | `kcon_` | **A surface over a socket**, both ends: the wire, the client's `KDispImpl` and `KtuiBackend`, and the server side a display composites. **No file descriptors cross it**, which is what makes it forwardable | `libkdisp`, `libktui` |
| `libkwm` | `kwm_` | **The window model both desktops obey**: placement, the tiled-state transition and its geometry, the neighbour-edge search, ring walks for cycling and workspaces | `libkbase` |
| `libkdisp` | `kdisp_` | **Which display server, decided once**: the surface config, the seven roles, the lifecycle every surface asks for — init, close, resize, autohide, cell size, scale, clipboard, cursor — and the window list a panel manages | `libktui` |
| `libkchrome` | `kch_` | The window furniture: the header band, group headings, the button bar, the list and scrollbar rule, the pixel tile | `libktui`, `libkicon`, `libkcell`, `libkdisp`, `libkwl` |
| `libkicon` | `kicon_` | **A name becomes a sprite slot, or −1** | `libktui` |
| `libkcell` | `kcell_` | The glyph cache and the cell painter — a grid of cells into a pixel buffer, the character ramp built from it, the pixel canvas, and the one scale-and-cut of a decoded picture into sprite tiles | A font renderer, a pixel library |
| `libkwl` | `kwl_` | The toolkit's **Wayland backend**: surface roles, buffers, scale, the font in force, input, clipboard, compose, cursors, frame throttling | `libkcell`, plus Wayland client libraries |

## Dependency direction

```
libkwl → libkcell → libktui → libkcolor → libkbase
libkwl → libkdisp → libktui
libkchrome → libkicon, libkcell, libkdisp, libkwl, libktui
libkicon   → libktui
libkdisp   → libktui
libkxdg    → libkbase
libkpkg    → libkbase
libksig    → libkbase
libkbuild  → libkbase
libkproc   → libkbase
libkwm     → libkbase
libkpack   → libksig, libkpkg, libkbase

libkvt     → libktui, libkcolor, libkbase
libkcon    → libkdisp, libktui, libkcolor, libkbase
libkimg    → libkbase
libkkms    → libkcell, libktui, libkcolor, libkbase
```

**The four edges worth stating are the console's.** `libkvt` is a terminal's
private screen and reaches libktui only at its render boundary, in one file.
`libkcon` carries cells over a socket and links **no** pixel library, which is
what lets the session come up on a machine whose GPU driver does not. `libkimg`
decodes untrusted bytes and depends on nothing but libkbase, so the decoder
cannot reach the toolkit. `libkkms` is the only one of the four that opens a
device, and it is a separate archive for exactly that reason: `kdos-con` links
none of it and only the view does.

## libkwm

The window model, and only the model. `kdos-comp` draws windows in pixels and
`kdos-con` draws them in cells. Placement, tiling, the focus stack and the ring
walks live here and nowhere else, so a defect in one of those is **one fix**.
The neighbour-edge search is half shared: the arithmetic is this library's and
the compositor calls it, but the walks that *find* the candidate edges exist
twice — the compositor's across its scene graph, this library's across a region
array — so a defect in a walk is two fixes, and the fixture below is the
arbiter of what the two must agree on.

It is handed rectangles and told what is being asked. What a window *is*, which
output it is on, whether a client accepted its size and whether it is maximised
all stay with the caller — which is what lets a compositor and a cell grid share
it at all.

**The contract is `testing/fixtures/wm/geometry.txt`**, every row of which cites
the line of `kdos-comp` it was derived from, and the self-test replays the file
rather than asserting anything of its own. Adding a case means adding a row and
citing its line.

Three things that file pins, each of which reads as a bug and is not:

- **A quarter snapped towards the edge it already occupies collapses to a
  half.** The parallel component is then neither the inverse of the request nor
  absent, so no branch of the transition matches and the orthogonal component is
  discarded.
- **The two halves of an axis come from different expressions** — `(size + gap) / 2`
  and `(size - gap) / 2` — which is what puts a whole gap between two tiled
  windows rather than half a gap each. An odd dimension therefore gives the right
  or bottom half one extra pixel.
- **Occupancy is an input, not a derivation.** The compositor counts views that
  are not omnipresent; the panel counts windows that are not minimised, because
  the workspace protocol has active, urgent and hidden but no "there is something
  here". Two rules, two right answers, and this library picks neither.

**No maths library**, the constraint `libkcolor` and `kcell_ascii.c` are already
written under. The placement grid's interval search compares doubles, which is
plain arithmetic and calls nothing.

## libkdisp

Which display server a surface reaches, decided in one place.

`kdos-shell` alone opens a surface from **more than twenty** call sites, and each
then asks whether it should close, resizes itself, or hides its panel. Branching
on the server at every one of those is the same decision written twenty times in
one program and again in the next.

**The consumer decides what it links.** This library names no implementation and
pulls in none; a caller hands over the ones it compiled, in preference order, so
a console-only program never sees Wayland:

```c
extern const KDispImpl kcon_impl;  /* libkcon — the console session   */
extern const KDispImpl kwl_impl;   /* libkwl  — the Wayland compositor */
static const KDispImpl *const have[] = { &kcon_impl, &kwl_impl };
kdisp_init(&cfg, have, 2);
```

**Both, and the console first.** Every shipped surface registers this pair: a
program that offered only `kwl_impl` compiles and runs and simply cannot be
opened on the console desktop, which is the default session. The order is the
preference order, and the console comes first because a program started inside
a console session must not find a Wayland display left over from somewhere else
and attach to that instead.

Each program states that list once, and it is the single line that changes when
a third server is added.

**The screen's font is here for the reason the window list is.** A surface never
loads a font — it draws cells and something else turns them into pixels — so
`kdisp_font_count` / `_at` / `_current` / `_set` is the only way a picker can
ask what faces exist, and the list is the **display's**: on the console the view
gathers it, and that view may be at the far end of an `ssh` link with its own
machine's fonts. `font_set` takes an **index into that list and never a name**,
so no fontconfig syntax crosses from a surface to a display that has never seen
it. A backend leaves the entries NULL where the font is not the desktop's to
change — the compositor, where every program carries its own — and a count of
zero is then the honest answer rather than an empty list. `keep` is whether the
choice survives the logout, so a picker's arrows pass 0 and only its `Enter`
passes 1.

**A caller re-reads the list on each turn**, the rule the window list keeps: the
answer arrives over a socket some pumps after `kdisp_font_ask()`, and a caller
that believed the first count would draw an empty list for ever.

**A server that cannot answer an entry leaves it NULL** and the forwarder
returns the neutral answer rather than crashing — a console has no server-side
decoration to report and no Wayland handle to hand out. `kwl_display` and
`kwl_seat` are deliberately *not* in the vtable for that reason: they hand out a
Wayland object and nothing else can stand in for one.

**Somebody else's windows are five entries, and they are asked for.** `win_count`, `win_at`,
`win_activate`, `win_close` and `win_set_state` are what a panel, a task switcher
and a window menu need — enough to draw a row and act on the one that was clicked. A surface only
receives them if it set `manage` on its configuration, which on the console makes it a shell in the
session's eyes and under a compositor is what `wlr-foreign-toplevel-management` grants anyway. An
**id** crosses the interface, never a handle: a caller draws a list in one frame and acts on a row
in a later one, and a stale handle is a request to a destroyed proxy that kills the connection.
**Announcement order is list order, and a row is only offered once it is settled.** A panel draws
task N at position N, so a removal closes the gap rather than filling it from the end — an
unordered swap teleports the last button into the middle of the bar. A handle whose property batch
has not been closed by `done` is left out of both the count and the walk, compacted rather than
holed, because a hole would hide every settled window behind it.

**The Wayland side binds foreign-toplevel on first use, not at start-up.** A compositor announces
every window to whoever binds it, and a terminal, a lock screen and a resource monitor all link
this library and want none of that traffic.

**Two edge vocabularies in this tree, and they must not be conflated.**
`KDISP_EDGE_*` is a sequence naming which edge a panel is anchored to.
`KWM_EDGE_*` is a bitmask whose values match the compositor's own enum, so that
corners are combinations. They are different questions.

**Nothing points back up.**

**`libkproc` links `libkbase` and nothing else**, and that is what lets a root daemon take it: two
root daemons do, and every library they link is code running as root.

## libkbase

The floor. Everything else is built on it.

**A library does not own the exit path.** The allocator calls whatever failure handler was
registered rather than knowing that a terminal exists and that the program is called something
specific. A companion call supplies the program name for that message and for the fatal and warning
output.

Two members worth knowing about specifically:

- **`kb_copy_file` streams.** It is what moves application packs, and those run to hundreds of
  megabytes, with the pack daemon copying one into the store as **root** on every install. Reading
  a file whole to write it whole asks for its size in anonymous memory for no reason.
- **The process helpers send a child's error output to nothing unless verbose output is enabled**,
  so anything whose *failure* is diagnosed by the child's own message has to turn that on.
- **A secret reaches a child on a descriptor and never in argv.** `kb_run_feed` writes to the
  child's stdin; `kb_run_feed_env` does the same and additionally sets names in the child's
  environment and captures its stderr, which is what a mount helper needs — `mount.cifs` reads a
  password from the descriptor `$PASSWD_FD` names and reports its refusal on stderr, and a caller
  without both would either put the secret in an argument list or report a status with no reason.
  **The input must fit in one pipe buffer** in the feeding-and-reading forms: nothing is read back
  until the whole input has been written.
- **The freedesktop trash lives here**, so a prompt and the desktop's delete key are one
  implementation. See [The kdos command](../04-programs/kdos-command.md#trash).
- **`kb_fuzzy` is the desktop's only answer to "does this row match what was typed".** A
  subsequence rather than a substring — so `sm` finds `System Monitor`, which no substring search
  can — scored so a prefix beats an acronym, an acronym beats a run, and a run beats a scatter.
  **Higher is better and zero is no match.** Each needle character prefers a word start over an
  earlier occurrence inside a word, which is what makes the acronym score reachable; because that
  preference is greedy it can consume a character the rest of the needle still needs, so a
  preferring pass that fails is redone taking the leftmost occurrence of each character. **Any
  needle that is a subsequence of the row therefore matches** — a row never loses to a prefix of
  its own name. It is here rather than in a surface because three of
  them search the same applications: the palette, the launcher and the Start menu. Before it they
  ranked one query three ways — the launcher scored a subsequence and *lower* was better, the
  application index did a case-insensitive substring in six bands and higher was better — and a
  person who finds something in one surface and nothing in another has learned that the desktop's
  search cannot be relied on. **A caller that sorts on it must sort descending**; the matcher it
  replaced in the launcher was lower-is-better, and a sort left as it was ranks a correct list
  backwards, which reads as bad ranking rather than as a bug.

**One trap the argument-vector builder carries, and it has bitten several callers:** it **stores
the pointer and does not copy**. Several arguments built one after another in a single reused
buffer therefore all point at the same bytes, and the program is handed the last value repeatedly.
Use the formatting variant, which writes into the vector itself. This is a property of the builder,
not of any one caller.

## libkcolor

**The palette table is an X-macro**, so every consumer expands the same literal values into its own
table **at compile time**. The toolkit projects them onto its colour slots; the theme generators
expand them into stylesheets, vector artwork and cursor images. **Nobody keeps a second copy of the
numbers.**

**The colour-space conversions reproduce a specific reference implementation exactly**, including
its unusual intermediate expression, its handling of a negative hue, and its round-half-to-even
rounding.

That is not pedantry: the vendored icon and cursor artwork is **committed**, and a generator that
rounds differently produces a diff against files already in version control. Verified across
thousands of colours in every accent.

It stays off the maths library for the same reason the toolkit does — a phase-1 consumer cannot
link one — so the modulo is done in a loop and the rounding by hand.

**The two mixing functions are not interchangeable.** The integer and floating-point forms disagree
by a unit on some inputs, and **each generated file was written against exactly one of them**. The
test suite asserts that they disagree, so nobody "fixes" it.

**Two derived colours, and confusing them is a legibility bug.** The dim value is a **fill** and
measures below any text floor against the background; the derived muted colour is the readable one
and every text role goes through it. See
[the design language](../03-architecture/design-language.md#colour).

## libktui

The toolkit. Terminal ownership, the cell buffer, the diff, input decoding, widgets, and the charts.

**Three glyph tiers**, chosen from the terminal's capabilities, because the console font is 512
glyphs and a character it lacks renders as a **blank**. The table is in
[the design language](../03-architecture/design-language.md#the-glyph-tiers).

**What a terminal can do is asked for once, in one write, on entering the screen.** Two facts
follow from no `TERM` value and no capability database entry: the kitty keyboard flags (`CSI ? u`),
which is what makes `Super` arrive at all, and synchronized output (`CSI ? 2026 $ p`), which is what
lets a frame be bracketed. Both queries go out together and their replies are told apart by scanning
the buffer — asking in turn pays the timeout twice on a terminal that answers neither, and the
second wait would swallow a slow answer to the first as if it were its own. A DECRQM value of `0`
means "not recognised" and `4` means "can never be set", so only `1`, `2` and `3` set `KT_CAP_SYNC`.
**The replies are consumed there or they are typed into the desktop**, and the Linux VT, which
answers neither, is skipped rather than waited on.

**A frame is bracketed where that bit is set** — `CSI ?2026h` before the diff and `CSI ?2026l`
after it — because a diff frame is a scatter of cursor moves and single cells, and a terminal
drawing as they arrive shows a menu before the one under it is erased. The close is written again
when a bounded flush dropped the frame, and once more when the terminal is handed back:
**a terminal left inside the block draws nothing further**, so an unclosed bracket is a frozen
screen rather than a tear.

**The progress bar is a wrapper whose pixels must not move.** The installer links it and only it,
pinning a solid two-state bar. Change the general form freely; leave that branch alone.

**A resize is not applied until the consumer applies it.** The backend sets a flag; the reported
size follows only when the loop calls the resize and invalidate functions. Any loop that owns a
surface owns this — a surface that was always a fixed size and then starts being resized will draw
against stale dimensions and silently fail its own bounds checks.

**The caret goes to the backend when the backend has one.** `ktui_term_caret()` is the one call a
surface makes to say where it is typing, and it writes the terminal escape only when nothing else
claims the answer. The console client claims it: a surface drawn through a display server is not on
a terminal, its stdout is not the screen it appears on, and the position it knows is in its own
cells — which only the server can place on a screen. A backend that leaves the entry NULL keeps the
escape, which is what the Wayland one does, because a compositor's surfaces draw their own.

**The pointer goes the same way, and for the same reason.** `KtuiBackend.pointer` is handed the
cell the pointer is on and answers whether it drew one itself; `libkkms` is the only backend that
does, compositing an arrow into the framebuffer it already owns. Everything else leaves the entry
NULL and `ktui_draw_flush()` reverses the cell under the pointer, which is the pointer a `--tty`
view, a `--dump`, an `ssh` forward and `tty1` can show — and the one `a11y = yes` depends on, since
that setting runs the desktop on a `--tty` view so `brltty` can read `/dev/vcsa`. **An arrow cannot
be drawn in this library**: it links nothing but musl and has to keep doing so, and an arrow needs
a pixel buffer and a colour in it. **The hook is called on every flush**, with a negative `x` for
no pointer at all, because that call is the only thing that tells a backend to take the last arrow
off the screen. Over a cell marked `KT_A_GUEST` the flush reports no pointer to either path: the
guest's own compositor draws a cursor into those pixels and a second one a cell away is the one
nobody is aiming with.

**Offscreen rendering** takes a fixed size and writes the cell buffer out as plain text, with no
terminal at all. Every geometry defect this toolkit has shipped was invisible to the compiler and
to a test suite that cannot draw; this is how they get looked at.

**A widget is either an immediate-mode call or a draw-and-key pair, and which one is decided by
its callers.** `ktui_list`, the buttons, the checks and the input field read the frame's focus and
return what happened in one call, for a surface built around the frame. The page strip, the column
table, the dropdown and the text block are a `_draw`, a `_key` and a `_hit` instead, because every
surface that wanted them runs its own event loop and holds its own selection — an immediate-mode
form would have meant rebuilding each of them around the frame before it drew anything at all. A
hit test takes the same rect its draw took, so it measures what is on the screen rather than what
the widget remembered from an earlier size.

**No widget owns its selection.** The caller holds it, because the caller is what persists it,
dumps it and restores it — and because a page strip and the body under it are one selection seen
twice.

**A table's heading row says whether the selection may land on it.** Both readings are in the tree:
a network device heading is the row Enter rescans from, and a device-section caption is furniture.
The row-kind callback answers per row rather than the widget choosing for both, and a row the
selection steps over never lights.

**A sprite table entry is a borrowed pointer, so eviction is what the owner told it to do.** The
table does no pixel work and cannot free a picture; an owner registers an evictor and the table
calls it whenever the table itself stops naming a picture — a slot taken back under the byte budget,
a slot reused for a *different* picture under the same key, or a tile refused part-way through a
tiled put. **`ktui_sprite_drop` is the one way a picture stops being named without the evictor**:
dropping by key is the owner's own call, and a callback there would be a free from inside that call,
so the table hands nothing back and the owner unrefs what it dropped and clears whatever it keeps
beside the slot. Without an evictor a full table simply answers -1, which every consumer already
handles by drawing its glyph. That is right for icons, which are owned for the life of the session,
and wrong for photographs, which are megabytes each.

**A refused put is a hole, and only the owner can see it.** `ktui_sprite_put` answers -1 when the
budget cannot be made to fit: eviction skips every slot the cell grids still reference, so a table
whose pictures are all on screen has nothing it may take. A caller that stores that -1 as a slot
draws the background where a picture belongs, and the table can never repair it because it never
learned the picture existed. A consumer holding pictures that are not its own — a view showing a
session's — has to tell the side that owns them; see `KCON_OP_SPRITE_LOST` under libkcon.

**There is one evictor per process, and every owner in it shares that one.** A program that draws
photographs and icons has a single function handing back pictures built by two different libraries,
so no owner may assume the evictor is its own free. A picture given to the table must **free its own
pixels on the last unref** — a pixman image carries a destroy function for exactly that — and its
owner must **hold a reference of its own** across the handoff, or an eviction leaves that owner's
cache naming a freed image.

**Whether a sprite is still on screen is asked of the CELL BUFFER'S own size**, not of `ktui_w` and
`ktui_h`. A backend reports a new size the moment it is resized and the buffer is reallocated only
when the consumer calls the resize function, so between those two points the globals describe a
grid larger than the allocation.

**One rule for reducing a colour that came from outside the palette.** A terminal's SGR and a
picture's average tint both land on the nearest of the theme's slots by squared distance, through
one function here. A second implementation would drift, and a table saying "red means the error
slot" would be a second set of colour decisions beside the palette — one that would stop following
the accent, so `kdos theme amber` would move some colours and not others.

**Night light is a transform over the slots, not a scheme.** `ktui_theme_night()` warms whatever
`ktui_theme_set()` last loaded — green to 93%, blue to 77%, red untouched, the shape of a colour
temperature and the reason a warmed accent still reads as itself — and hands out a copy, keeping
the chosen scheme beside it: eight bits do not divide back, so turning it off returns to the table
rather than undoing the arithmetic. **The caller reads the toggle**, because this library holds no
opinion about where a desktop keeps its state, and it returns whether the palette actually moved so
a caller can skip a repaint it does not owe. **The warmed copy is one buffer, rewritten in place**,
so its address stays the same when the scheme under it changes: anything caching work per palette —
the tty backend's SGR table is the one that does — compares the eight slot colours, never the
pointer.

**A widget says what it is, because it already knows.** Every widget computes which item has focus
each frame from the same id the hit test uses, so a reader working that out again from a grid of
cells would be guessing at what the surface has in hand — and it guesses wrong first on the controls
that matter most: which cell of a table, which tab of a strip, which item of how many. `ktui_announce()`
takes a role, a label, a value and the item's **position in its set**, so "3 of 9" is a fact the
widget states rather than a count somebody has to make.

- **It lives here, not in a session.** A record composed in `kdos-con` would reach the console and
  give the graphical desktop nothing; one set in this library is set once and both desktops read it.
- **The queue is per frame, fixed, and cleared at the start of every frame.** Nothing on the draw
  path allocates — a widget that allocated to say its own name would drop frames on the link this
  desktop is sold on — and a frame with more to say than the queue holds drops the rest.
- **Silence is the failure mode, never a stale name.** A widget that says nothing announces nothing;
  a reader told the wrong control is worse off than one told nothing, and last frame's record is the
  wrong control by default.
- **A widget says what it knows and no more.** A tab strip has its names and says them; a list and a
  table take their rows from the caller's own callback, so they state the position and leave the
  name to a surface that has it. **A secret field announces that it is one and never its contents.**
- **A repeated record is the same control.** Dropping the repeat is the reader's job; the widget's
  job is to be right every frame.

**A cell's attribute byte carries six styles and the wire stays eight bytes wide.** Bold, reverse,
underline, italic, strikethrough and overline are one bit each in the byte the cell already had, so
a terminal's own text reaches a KDOS surface without the per-cell run growing for it. **All of them
but reverse are dropped on a real VT**, where an attribute bit selects a font page or a colour the
palette does not own rather than a style.

**Above the eighth bit nothing travels in that byte.** The attribute is sixteen bits wide in memory
and eight on the wire; the high half says which of the cell's three literal colours — foreground,
background, underline — mean anything, and what shape the underline is. They are set **only** by a
[negotiated colour run](#libkcon), so a consumer that was never sent a literal cannot receive a cell
claiming to have one and draw the black it never got. **One bit per colour, not one for the pair**:
a program that sets a foreground and leaves the background alone is the common case, and a single
bit would freeze the theme's background into the cell as a literal, after which a retint leaves a
rectangle of the old scheme behind.

**A literal reaches a frame only through `ktui_draw_put()`.** Every other draw call takes slots and
clears the literals, because chrome that stopped following `kdos theme` would be a second palette
nobody can change. Terminal content is what uses it: the two places that copy a terminal's grid into
a frame copy whole cells.

**A pointer event carries where in the cell it landed**, as an offset from the cell's centre in
1/256ths, and zero — what a backend with no pixel geometry leaves behind — means the centre.
Nothing drawn in cells reads it. It is what aims a pixel guest embedded in a window when the event
has no raw partner beside it: a view that reports no pixels at all, which is every view inside
somebody else's terminal, and a touch gesture's synthesised pointer on a view that does. Where
there is a partner the guest is aimed from it instead — see [libkcon](#libkcon).

**The same physical input travels twice, in two queues.** `poll_event` answers a character and a
cell; `KtuiBackend.poll_raw` answers a `KtuiRaw` — an evdev keycode with a separate press and
release, the xkb mask and group, a pointer in the backend's own pixels with the cell those pixels
were measured in and both deltas the device reported, and a scroll with a second axis, a `value120`
and what made it — and `KtuiBackend.keymap` hands over the backend's compiled layout as xkb text
with a generation counter. A backend with no device of its own leaves both NULL, which is how a
consumer knows not to claim the capability at all.

**A second queue because motion coalesces and a key must not.** A thousand-hertz mouse in the
cooked queue evicts the click that came before it, which is the oldest entry there; in the raw
queue consecutive bare motions merge into one at the newest position with their deltas **summed** —
a delta is a distance, and dropping one shortens the movement a grabbed guest sees — and nothing
else merges at all.

**The cooked event for one physical input is sent before its raw partner, and `KtuiRaw.after` is how
a caller knows which.** It counts the cooked events that must be taken before that raw one goes, so
a caller sends cooked events up to that number and only then sends the raw one — the field carries
the order, not the order the two queues were filled in. Draining one queue to empty and then the other loses it: two keys inside one poll arrive as
two cooked messages and then two raw ones, and a session that swallowed the first key's chord has
no way left to say which press it swallowed. A handler may queue the switch before the character it
resolves to, so a raw event's number is raised by any cooked event queued before the caller drains
it — the stamp is only ever raised, because one delivered early is a chord the session ate and the
guest saw.

Three rules the extraction from its original single consumer exists to keep:

- **Symbols are prefixed.** The generic names it once used collided with a consumer's own
  definitions of the same names with different semantics — two of our own programs could not be
  linked together.
- **The frame state is private.** It was a public structure that applications assigned to field by
  field; it is behind accessors now.
- **Chrome identifiers are the library's business.** Chrome registers with caller-local
  identifiers in a reserved range that never joins the focus ring and never drags the page scroll.

## libkvt

The terminal as a state machine — a hard fork of libtsm 4.7.1, kmscon's own. What a *consumer*
touches is `struct kvt_term`: a screen, a state machine and a child on a pty as one object, with a
descriptor to poll and a grid to draw.

**The child dies with the window and is collected there.** `kvt_term_close()` closes the master —
which hangs up the pty's foreground group — signals the child `SIGHUP`, waits up to 100 ms for it,
and `SIGKILL`s and blocks on whatever is left. Nothing else can do it: the non-blocking reap runs
from `kvt_term_pump()` and there is no pump after a close, so a child left behind is a zombie for
the life of the session and a child that ignores `SIGHUP` is a program still running with no
terminal. The consumer needs no `SIGCHLD` handler.

**The bytes a key produces are decided in here, never by the caller.** The escape an arrow sends
depends on application cursor mode, on keypad mode and on the modifier encoding, and all three are
state machine state. A caller hands over a libktui key and modifier set; `kvt_term_key` turns it
into a keysym and lets the machine answer. Both terminals in this tree go through it, so there is
one implementation rather than two that drift.

**A hyperlink is a 16-bit id on the screen's own cell, and the address is interned.** `OSC 8` names
an address for a run of text; the cell keeps an id into a per-terminal table, so the text scrolls
into the scrollback still knowing what it points at, and two runs of the same address are one link.
The table is capped and is **not** freed on a reset, which is what makes an id in the scrollback
safe to resolve for ever. `KtuiCell` is deliberately not widened for it: a link is a property of a
terminal's buffer, not of every surface the toolkit draws. `kvt_ui_mouse()`'s coordinates are the
**terminal's own grid** for the same reason a link lookup's are — a caller whose terminal is a
window subtracts its origin, or both the selection and the link land as far from the pointer as the
window is from the corner.

**A screen can be read out as text and written back in.** `kvt_screen_text()` gives the scrollback
and then the screen, oldest first, characters only — colour, attributes and pictures are not what a
saved session puts back, and a picture cannot be put back at all because the tiles it named belong
to a program that has exited. **The screen's empty tail is padding, not output**, so it is trimmed:
a terminal showing two lines would otherwise end with a dozen blank ones, and a caller feeding that
back scrolls the two lines it cared about off the top. `kvt_term_show()` is the other direction —
text the terminal SHOWS, into the state machine where the child's own bytes go, never to the child.

**A prompt mark is on the LINE, and the exit status is walked back to.** `OSC 133` says where a
prompt starts and what the command typed at it exited with; the mark rides the line so it survives
into the scrollback as one thing, where a mark per cell would be eighty copies of one fact and a
mark kept beside the screen would be lost the moment the line scrolled off. The status is walked
back to the nearest marked line at or above the cursor rather than remembered in a pointer — a
pointer to a line kept across a scroll is a pointer to a line the screen may have recycled.

**The render boundary is where an attribute becomes a cell, and what it cannot carry it drops.**
Bold, underline, inverse, italic, strikethrough and overline each have a bit and each is drawn by
the pixel painter; `blink` and `dim` are parsed and reach none. That is deliberate: a blink drawn as bold and a dim drawn as normal are both
a lie about the text, and the cell is the one place that can say so rather than approximate.

**A picture is written into the SCREEN as sprite cells.** `kvt_term_place` names tiles the caller
already registered in libktui's table and writes their codepoints at the cursor. In the screen
rather than in an overlay beside it, because that is what makes a picture scroll with its output,
clear with it and reach the scrollback — three behaviours an overlay would have to reimplement
against a screen already doing all three. A tile the table has since dropped becomes a blank, and
the cells carry the state machine's own default attribute, so a fallback is drawn in the terminal's
text on the terminal's background and follows `OSC 10`/`OSC 11` and the installed palette. A
literal colour index there would reduce to whatever theme slot is nearest its RGB, which for a
foreground index and a background index can be the same slot — a blank drawn on itself.

**It decodes nothing.** The three image protocols are delimited by one collector — they differ only
in how they are framed — and the payload goes to a callback the consumer set. That is what keeps
this library free of image decoders, and it has to stay free of them because `kdos-con` links it
and links no pixel code at all.

**The colour a cell reduces to is cached, and the cache is indexed by the high bits of its
multiplicative hash** — the same rule as libkchrome's literal table above, for the same reason: the
low bits of a product carry only the low bits of blue, so a low-bit index lands the whole 216-colour
xterm cube in six buckets and a 256-colour program misses on nearly every cell. The cache is thrown
away when the palette in force changes value, which is the only thing that can move an answer, and
the palette is compared by value because night light rewrites one struct in place.

**A CSI final with a private marker or an intermediate is not the plain sequence.** `r` is DECSTBM
only as `CSI Pt;Pb r`; `CSI ? Pm r` is XTRESTORE and `CSI ... $ r` is DECCARA, and either taken for
DECSTBM rewrites the scroll region and homes the cursor under a program that only asked for its
modes back. `m` is guarded the same way. A final that means different things under different
markers tests `csi_flags`, never the byte alone.

**A word selection's end is bounded by the screen, not only by the line.** A line only ever grows,
so it keeps the widest the terminal has ever been; after a shrink a word crossing the old right
edge would set `sel_end.x` past `size_x`, the renderer would never reach the index that turns the
highlight off, and every row below it would draw inverted. The copy takes the same bound, so the
text that comes back is the text that was lit.

## libkcon

A surface over a socket, both ends in one file so the two cannot drift.

**No file descriptor crosses it, ever.** That is the whole reason the view socket can be forwarded
over `ssh`: a desktop reached from another machine is the same desktop. The one descriptor anywhere
near this design goes over a different channel — a `socketpair` between the session and the
`kdos-cage` it forked — which is private, local and parent-to-child, and is not this protocol.

**A field at a time, little-endian.** A struct written whole is a struct whose padding and alignment
become protocol, and the two ends of a forwarded socket are not always the same build.

**A cell run is eight bytes a cell and stays eight bytes a cell.** The colours a terminal named
itself ride a **separate run**, sent only to a view that asked for it in its hello and only for a
run that carries any: three bytes each for the foreground, the background and the underline, then
one byte saying which of them mean anything. It repeats the position and count of the commit it
follows, so a view patches cells it already has rather than holding a frame back for a message that
may never come — and a view that declined draws the slots every cell still carries. A surface sends
the same run up to the session, which patches it over the commit it follows: without it a terminal
on the console shows every colour outside the sixteen reduced to a slot. Widening the
cell record instead would have doubled what every commit costs across the `ssh` link this desktop is
sold on, to carry colour most cells on a desktop do not have.

**A frame is cut into messages, because a frame is not a message.** A length field is an allocation
request from an untrusted peer, so it is refused at the header above a megabyte — but the grid a
frame covers may be 4096x4096, tens of megabytes of runs. The sender walks the grid and flushes
every quarter of a megabyte, cells before the colours that patch them, rather than building the
whole frame and discovering it cannot be encoded. The threshold is well under the cap on purpose: a
buffer grows by doubling, so one allowed to approach the cap reallocs to exactly the ceiling and
then refuses the run that follows.

**A fill buffer is kept, not allocated.** Every message is encoded into a `KconBuf` that belongs to
the connection — three of them, for the cells of a frame, the colour records that patch them, and
the pictures those cells reference — and `kcon_buf_retire` empties one instead of freeing it. A
sprite block at an 8x15 cell is a hundred and twenty kilobytes and a fullscreen guest is dozens of
them a frame; under a size-class allocator like musl's an allocation that large is an `mmap` and the
matching free an `munmap`, so a buffer allocated per message has the kernel fault in and zero every
page of every block on every frame. Measured on musl 1.2.5 at 1080p with a 75-block frame, a
per-message buffer costs 2400 minor faults a frame; keeping it costs under one, and with the socket
buffer below it the frame goes from 8.0 ms to 2.0 ms. **Measure this on musl, not on glibc** — glibc
keeps a 120 KiB block on its own free list, reports zero faults either way, and says the change is
worth nothing. A buffer that grew past `KCON_BUF_KEEP` (512 KiB, above both a block
and a frame chunk) is released rather than kept, so one outsized message cannot pin its memory for
the life of the connection. `KCON_BUF_KEEP` must stay at or above **twice** `KCON_CHUNK_BYTES`,
because a buffer grows by doubling and a chunk a byte over a power of two rounds its capacity up to
the next one; below that margin the frame buffers fall out of retention silently and the allocator
cost returns in full, so the relation is a `_Static_assert` rather than a convention. The cost of
keeping them is at most three buffers of `KCON_BUF_KEEP` per surface, held until that surface goes,
so the ceiling scales with the number of live surfaces rather than with how much any one sends. **The buffers belong to one connection** — on the server a field of the
`KconSurface`, in a client a field of the one connection that process holds. What breaks the rule is
a buffer *shared* between two connections, or between two senders filling at once, not the storage
class it happens to have: two surfaces are sent frames in the same loop and a shared buffer would
put one's pixels in the other's message. they are safe to refill the instant a send returns, because `kcon_send` copies the payload
into the connection's own out queue and nothing downstream holds a pointer into a `KconBuf`.

**Two ways to put bytes, and the difference is what a reader has to know.** `kcon_put_blob` writes a
length first, for a payload whose size the message does not otherwise give. `kcon_put_bytes` writes
none, for one it does — a sprite's pixels are `pw * ph * 4` and nothing else. A second length is a
second thing that can disagree with the first, and a reader computing the size from the header would
then be four bytes out for every picture on the desktop.

**A string is valid only until the next get.** The payload's bytes are not terminated where a string
ends, so one scratch buffer is shared by every call; a reader taking several strings copies each
before it reads the next, or every pointer it kept names the last one.

**A message may gain optional trailing fields**, and `kcon_rd_left` is how a reader tells a peer that
predates them from a truncated message. A view's pixel geometry and a pointer's position inside its
cell arrived that way.

**A view reports its input twice, and the two streams are different things.** `KCON_OP_KEY` and
`KCON_OP_PTR` are cooked: a character the view resolved through the person's own layout, and a cell
with an offset inside it. That is everything a cell desktop needs and nothing a pixel guest can use
— a guest holds a key down, repeats from its own keymap, reads a modifier that produces no character
at all, and is aimed at things smaller than a cell. So a view that holds a real keyboard and a real
pointing device claims `KCON_VIEW_RAW` and reports the same physical events again, unresolved:
`KCON_OP_KEY_RAW` (an evdev code, a separate press and release, the xkb mask and group),
`KCON_OP_PTR_RAW` (the view's own pixels, the cell size those pixels were measured in, libinput's
accelerated and unaccelerated deltas and the full evdev button code), `KCON_OP_AXIS_RAW` (a
continuous value, a `value120`, a horizontal axis and which device made the scroll) and
`KCON_OP_KEYMAP`. They reach the session on `view_key_raw`, `view_ptr_raw`, `view_axis_raw` and
`view_keymap`, beside the cooked hooks and never instead of them. All four are refused from a view
that did not claim the capability and from one that attached to observe, the same guard the cooked
input keeps.

**The cooked message for one physical event goes first, and that ordering is protocol.** It buys
three things: the session has already decided whether a chord ate the key, has already decided which
window the pointer is over, and has already moved its own cursor cell — so the raw arm never routes,
never chords and never touches the cursor. It only delivers, which is what keeps one answer to
"where did this click land".

**A view sends none of it until it is asked.** `kcon_view_raw()` turns the stream on and is silently
nothing on a view that did not claim the capability; the session asks only while an embedded pixel
guest holds the focus, because the raw stream is one message per device event and a pointer at a
thousand hertz on a socket that also carries a window of pixels is a socket that carries no pixels.

**Motion coalesces to the newest; a key, a button and an axis never do.** A view merges a motion into
the pending one when no button, key or axis sits between them, and sums the deltas it merges, because
a delta is a distance and a dropped one shortens the movement a grabbed guest sees. A dropped press
is a letter that never arrives, a dropped release is a key held down for ever, and a scroll of zero
is the end of a gesture — so those are sent whole however far behind the link is.

**The keymap crosses as bytes, not as a descriptor**, which is the rule this whole protocol is built
on: a layout sent as a `memfd` would make a view unforwardable for the sake of tens of kilobytes
sent once. It is xkb's text format, refused above `KCON_KEYMAP_MAX` and refused unless its last byte
is the terminator the length counts, because the consumer hands it to a compiler that reads to a NUL.
One session is one keyboard, so the last view to speak wins.

**Every raw field is bounded before a hook sees it.** A keycode and a button share evdev's number
space and are refused above `KCON_KEYCODE_MAX`, which is what sizes the set of presses a window is
owed a release for; a cell size of zero is refused because the session derives its cell by dividing
by it; an axis or a source outside its enum is refused because the far end maps both in a switch and
a scroll whose direction it guessed is a page that moves the wrong way.

**A view that claims no capability is driven by the cooked stream alone.** It is never asked for raw
input, sends no keymap, and its keys and clicks arrive cooked — which is what a view inside somebody
else's terminal, at the far end of an `ssh` link, can produce at all. The session drives a pixel
guest from that stream the way it drives a cell window: a character turned back into a keycode, a
press and a release together, no modifier on a click, no horizontal axis and no pointer lock.

**A surface's slot numbers are its own.** Two surfaces both using slot 0 is the normal case, so the
server assigns a session slot on first sight and a compositing session rewrites the slot in every
sprite cell it copies out. A session that owns a picture itself — an embedded application's frame —
takes slots from the same rotation, because a second numbering would eventually hand a view a number
a surface is already using.

**And the session's slots are a free map, not a counter.** A slot goes back when its surface goes,
when the client drops the picture, or when a session that cuts its own pictures calls
`kcon_server_free_slot`; the rotation point is only where the search for a free one starts. A
counter alone wrapped onto numbers still being drawn with, so one program's picture appeared inside
another's window on a session that had been open long enough. A session with every slot taken hands
back −1 and the caller draws the fallback mark.

**Giving a slot back tells every attached display to forget it**, as a `KCON_OP_SPRITE_DROP` in the
session→view direction — the same verb a client uses to give up one of its own numbers. A number
back in the rotation is a number the rotation will hand out again when the search comes round to it,
and until it does nobody owns the number and nothing sends a picture under it, while a display keys
its sprite table on that number alone: one that was never told holds those pixels in its own byte
budget with nothing that will ever replace them, and the eviction that eventually takes them is
reported as a loss of a slot that by then belongs to somebody else, spending that owner's repair
allowance on a picture it never lost. Best effort — a drop that cannot be queued leaves the display
holding a stale picture until the slot's next owner sends its own, which replaces it under the same
key.

**A picture is sent when its PIXELS change, not once per slot.** An animation registers a new frame
under the same key and therefore in the same slot, without touching a single cell — so a client that
remembered "slot sent" would leave the display holding the first frame for ever. The client tracks
the sprite table's put counter for each slot, never the pixel pointer: the evictor frees the
previous frame as the next one is registered and the allocator hands the same block straight back,
so a pointer comparison calls every frame after the first a repeat. **A picture also arms the frame
contract at both ends** — the sender waits for a boundary after it, and the session marks the
surface as owing one the moment a `KCON_OP_SPRITE` arrives, accepted or not, because a leg answered
only for cells leaves an animation paced by the 100 ms stall timeout.

**A picture that reached the wire has not reached a screen.** `kcon_view_sprite` answers for the
send: 1 means the bytes were queued, and the view's own sprite table — which has a byte budget of its
own — may still refuse to keep them. `KCON_OP_SPRITE_LOST` is how it says so, carrying the slots it
dropped, and the session hears them one at a time through the `view_sprite_lost` hook. It is the only
way the sender can learn of the loss: it cleared what it owed on the send's own answer, so without
this the cells naming that slot are drawn over a picture that is not there. **The count and every
slot in that message are bounded against `KCON_MAX_SPRITE_MAP` before either indexes anything**: a
slot number is an untrusted peer's index into a table of the session's own. **The view sends one
message per frame it presents, not one per loss** — a display short of budget loses a picture per
block per frame, and
a message each would spend the queue the pictures need — and the side that re-owes the slots has to
bound how often it pays them **per unit of time**, because a table too small for the window refuses
the replacement too. A total restored only when the owner next draws is not a bound but an expiry:
an owner that has finished drawing never restores it, and the first picture the display loses after
that is a hole nothing fills.

**Every connection asks the kernel for a large socket buffer.** `kcon_conn_new` requests
`KCON_SOCK_BUF` (2 MiB) for `SO_SNDBUF` and `SO_RCVBUF` on both ends. **An AF_UNIX stream write is
gated by the *sender's* own send buffer and by nothing else** — the receiver's `SO_RCVBUF` is never
consulted, measured — so the other direction is covered because the peer sets its own send buffer in
the same constructor, not because this end asked for a receive buffer. The receive request is made
so the pair is right on a transport whose flow control does read it; it is not what does the work
here. A frame that cannot be placed in one turn of the sender's loop is presented
partially: the display shows what arrived and the window fills in horizontal bands over several
frames. With the default 208 KiB buffer a 75-block 1080p frame places 11 blocks a turn and
needs seven turns; with a 4 MiB grant it places 43 and needs two. A session pacing against half
`KCON_VIEW_HIGH` rather than against the refusal mark sees the same shape one step down: 7 blocks a
turn and eleven turns, against 38 and two. **The kernel doubles the request and
clamps it to `net.core.wmem_max`** — measured, the grant is `min(2 × request, 2 × wmem_max)` — so
what is granted is never what was asked for and `kcon_conn_sndbuf` reads it back off the socket
rather than reporting the constant. On the 7.0 kernel this system ships, `net.core.wmem_max`
defaults to 4 MiB and nothing in `fs/etc/sysctl.conf` changes it, so the 2 MiB request is granted
whole as 4 MiB; on a host that lowered the ceiling the same request gets less and the desktop is
correspondingly slower. A clamp is not an error and the request never fails a
connection: a smaller buffer is a slower desktop, not a broken one.

**The backlog is the pacing signal, and it is truthful.** `kcon_conn_pending` is bytes owed to the
kernel and nothing else: `kcon_view_ready` reads it against `KCON_VIEW_HIGH`, and the connection is
marked dead above `KCON_MAX_QUEUE`, so a figure that meant anything other than "not yet written"
would break the peer-is-gone guard. `kcon_send` ends in a flush, so the backlog is zero until the
socket's own buffer fills; a caller cutting a picture into pieces asks between them and flushes
between them, which makes `kcon_view_pending` a WATERMARK on a queue that re-clears rather than a
budget for one turn. A caller pacing against it must also poll the view for writability —
`kcon_server_view_at` plus `kcon_surface_fd` and `kcon_view_pending` are the three calls that shape
the poll — or the room the display frees goes unused until the caller's next tick.

**And a display that attached late is told to start again.** `KCON_OP_SPRITE_RESEND` asks every
surface to forget what the display has: cells that name a slot do not change, so a view that arrived
after a picture was placed would show the fallback mark for the rest of its life.

**A surface with nothing to show says so.** `KCON_OP_HIDE` is not a close — the connection, the
sprites and the clipboard all survive it — and it is what an overlay needs on this desktop: a
candidate window or a stack of toasts is up for a fraction of the time its program runs, and a
surface that could not say so would park an empty box on somebody's desktop. **A different size is a
second attach**, because the session already reads a requested size out of one and a separate resize
message would be a second place for the two to disagree.

## libkxdg

Desktop entries, the MIME glob table, and the command-line splitter.

**The splitter is the single implementation** of turning a desktop entry's command into an argument
vector: it unquotes, substitutes the file-argument codes, drops the codes that carry no argument,
and — with a negative count — keeps every code verbatim for a tool that **rewrites** a line rather
than running one. Its inverse re-quotes, so a generator's output round-trips.

Every launch path in the system goes through it. See
[kdos-appbox](../04-programs/kdos-appbox.md#exec-lines).

**`kxdg_launch_read()` is the single reader of the keys that decide HOW to start something** —
`Exec`, `Name`, `Terminal`, `X-KDOS-Term`, `X-KDOS-Size`, `X-KDOS-Float` and `X-KDOS-Cells`. Four
programs carried a private copy of that list — `kdos-shell`'s application index, its desktop icons,
its *Open With* chooser and `kdos-appbox open` — and a key added to one of them was a row that
behaved differently depending on which surface it was clicked from. It is the "can this entry be
started at all" test as well, so a caller's check and its read are one call.

**`NoDisplay` and `Hidden` are deliberately not among them.** They say whether an entry belongs in a
**menu**, which is a question for whoever is drawing one — the MIME route opens a `NoDisplay` entry
on purpose, and a shared reader that refused one would break every default handler that carries it.

**The `Exec` line is copied with its field codes intact.** The splitter spends them on the documents
a launch carries and reads the same codes to decide that a line carrying none takes its documents
appended instead, so a reader that stripped them here would make every entry look like `Exec=xterm`
and open a file in the wrong argument.

**`kxdg_mime_for_arg()` says what a command-line argument is**, and it exists because the two
openers were answering that question separately and getting a URL wrong in two different ways. An
argument carrying a scheme is typed `x-scheme-handler/<scheme>`; `file:` names a path, so the path
is unwrapped and typed like any other; a name that **`stat()`s** is a path whatever it looks like.
Without it the basename decided, and `mailto:a@b.c` matched the `*.C` glob — case-insensitively —
and resolved to C++ source.

It returns **the argument the caller must pass on**, read-only and never a copy, so a long path
cannot be silently truncated on the way to the handler. The pointer is into the argument, except
for a `file:` URL naming no path at all, where it is a static `/`. Percent-escapes are
**not** decoded, and a reader sees that: `file:///home/kdos/My%20Report.pdf`, which is what a
conforming caller emits for a name with a space, resolves to a path that does not exist and opens
nothing.

**`kxdg_places()` is one reader for the whole desktop**, and it replaced two. `kdos-desk` read
`~/.config/user-dirs.dirs` for the desktop folder while `kdos-menu`'s Places list assumed six names
under `$HOME`, so on a machine where somebody had renamed one the icons were in the folder the file
named and the menu opened an empty one beside it — which reads as a broken menu rather than as two
readers. The desktop folder, the Places menu, the chooser's `Ctrl+P` list and *Add to Places* now
resolve through the same call.

There is no `xdg-user-dirs` on this system: KDOS **seeds** `user-dirs.dirs` from `/etc/skel` and it
is the user's to edit. `$HOME` is the only expansion the reader understands, because it is the only
one that file's format defines — a reader that guessed at the rest would be a shell.

**A place that is not there is not a place.** The user directories are created on demand, and every
row is checked before it is returned: a row that opens an error is worse than a row that is not
offered.

**`kxdg_recent_add()` is written from one place**, `kdos-appbox open` — the function every open on
this desktop passes through, so a recent-files store can be kept without a second copy of the rule
going stale beside it. It is the same scanner run backwards: the bookmark this URI already had is
cut out whole and a fresh one appended, so a file opened twice is one entry and it is the newest.
The oldest past the cap are dropped on the same pass, because nothing else on this system prunes
`recently-used.xbel`, and the rewrite is temp-and-rename because the store is shared with every
other program on the machine that keeps recents.

**The read half memoizes the parse on the store's size and mtime, and never the existence check.**
Find rebuilds its rows on every keystroke, so the whole-file read and the backward walk are worth
keeping; the deleted-file filter is not cacheable, because nothing about the store changes when a
file is removed and a cached row would go on offering a destination that opens nothing. A memo hit
therefore re-runs `access()` over its rows, and keeps the rows that failed so a file that comes
back returns to the list.

**`kxdg_verb_*` is one table of what can be done to a file**, read by the desktop's icons, the file
chooser and — in the one form a text file allows — `mc`'s `F2`. Three tables meant a verb landed on
one surface and not the others, which reads as a surface being incomplete rather than as three
lists. A row whose program is absent is not offered, so a verb still being built turns on when it
ships with no edit to any caller; the resolution is repeated at most once a second per verb, so a
program installed under a live surface turns its verb on within a second while a menu redrawing
its rows several times a frame does not walk `$PATH` per row. Every verb builds an **argument vector**, never a command line.

## libkpkg

The package database, the ports tree, the solver, version comparison, and the two hashes.

**Version comparison lives here rather than in either consumer**, because the package manager and
the upstream version checker ask the same question about the same strings and two implementations
would eventually disagree.

The hashes and their three states are in
[Packaging](../03-architecture/packaging.md#deciding-what-to-rebuild).

## libksig

Signing and verification, and **the one library with vendored third-party source**.

The vendored implementation is four files of public-domain C with no dependencies, no maths
library and no allocation — which is exactly the rule this set is built under. The alternatives
were each disqualified: a shared library, a library with no signing support, an unmaintained one,
and one that is the opposite of "links nothing but the C library".

It sits under its own subdirectory with its version and checksum recorded. Everything above it —
file formats, the keyring, the policy — is ours.

## libkbuild

The **deciding** half of the build orchestrator. Covered in
[The build system](build-system.md#libkbuild).

## libkproc

Every reading about the running machine, **from a root that can be moved**.

That movable root is the single most valuable property in this library: it is what makes the
resource monitor, the stutter attribution, the memory daemon and the removable-media daemon
testable against **recorded** system state. A tool whose readings cannot be replayed cannot be
tested at all.

**Elapsed time may only be computed against the system uptime.** Both a process's start time and
the uptime are seconds since boot; pairing the start time with a monotonic timestamp is a different
epoch — and under a fixture, a different machine.

**The container identity walk** turns a process id into a box name by walking the parent chain to
the supervising process and reading its command line. It is used by four separate tools, which is
why it is here rather than in any of them.

**A reading goes into a caller buffer wherever a buffer bounds the file.** A process walk on a
busy machine opens a few thousand files a tick, inside a draw loop, and a whole-file slurp pays an
fstat, a heap block and an EOF-confirming second read for each of them — three syscalls and an
allocation to carry a few hundred bytes. `kpr_read_into_proc()` and `kpr_read_into_sys()` are the
form to reach for. The slurps stay for `/proc/cpuinfo`, `/proc/stat` and a command line, none of
which any buffer bounds: a full buffer means the content **may** be cut, and a cut value parsed as
though it were whole is a wrong reading rather than a missing one, so that case is re-read on the
heap.

**What cannot change is latched, and every latch is keyed on `kpr_root_gen()`.** A CPU topology, a
model string, a disk's rotational flag: constants that cost two files per logical CPU and a 26 KB
`/proc/cpuinfo` to rederive on a tick that only wanted the busy figure. Moving the root is moving
to a different machine, so a cache that does not hold the generation it was filled at describes
this host's hardware under a recorded one. What is **not** latched is as deliberate: a device's
size, because an optical drive keeps its name across a disc change and an LVM volume across a
resize; and the existence of a sensor, because a hwmon chip appears when its module loads. That
last is also why no reader of `hwmon` or `thermal` probes an index range — the index comes from
one system-wide counter and says nothing about how many chips there are, so
`kpr_sysfs_indices()` reads the directory and every reader shares the answer.

**`KprCpu.ncpu` is the highest CPU number plus one, not the count of CPUs running.** `/proc/stat`
lists only the online CPUs and keeps their real numbers, so a machine with `cpu1` offline has
arrays four long and `online[1]` clear. A per-core chart walks `ncpu` and skips the clear slots,
which draws the gap honestly; sizing by the number of lines instead drops the highest CPU's times
altogether and leaves a phantom core reading zero for ever. Anything that wants the *count* —
rescaling a per-core percentage to percent-of-machine, say — calls `kpr_cpu_online()`, because a
CPU taken offline below the highest index leaves `ncpu` where it was.

## libkpack

The pack format. Links the base, signing and package libraries **and nothing else, so a root daemon
can take it**.

Three rules it exists to keep, each stated in
[Packs and boxes](../03-architecture/packs-and-boxes.md#three-rules-the-format-keeps): parse whole
or be absent, hash before signature, and nothing here mounts or executes.

**The solve takes an array of pointers.** A pack's metadata structure is large, and an array of
them is not something a function puts on its stack — the pack daemon overflowed its own on the
first run.

## libkchrome

The window furniture: the header band, group headings, the button bar, the list and wheel rule, the
scrollbar and its drag, and the pixel tile.

It exists so that there is **one** implementation of each. Two button bars are two button bars, and
the one nobody is looking at is the one that drifts. The rules it enforces are in
[the design language](../03-architecture/design-language.md#the-chrome-primitives).

**A tile is cut to the display's pixel cell**, because that is the one the caller laid its contents
out in — a canvas cut to anything else clips the word or mis-centres the mark. Where the display
has no pixels of its own the tile is cut to the **nominal** cell instead: the console client answers
a cell of one, its sprites cross a socket and the far end rescales them, which is the same rule
libkicon keeps for icons. A change of cell size or output scale is a resize — both canvases go and
are cut again.

**A refused sprite put keeps the picture that is up.** The table refuses on a full table or a spent
byte budget, and a frame of the previous picture is better than a flash back to the glyph layout
mid-hover — so a tile remembers what it *published* apart from what it last *tried*, or "already
showing this" would freeze it at a picture it claims is current. Attempts are capped per content
hash: an unrelenting refusal must not turn every frame into a full canvas raster, which is the
memory pressure being survived. **The cap is rearmed a second after the last refusal**, because
what refuses a put is transient while a tile's content hash — the Start button's is the label, the
hover and the cell size — need never change again, and a spent cap that never comes back leaves
that button on its glyph layout for the rest of the session.

The pixel display list is recorded while a surface draws and replayed by the backdrop libkwl
paints under its cells. **Whether the plates moved is a question asked at flush time, not a flag
set while recording.** A frame is committed on a cell diff and a plate is not a cell, so a
highlight following the pointer down a menu would never reach the screen; but a surface re-records
its whole list on every draw, so a flag set from that cannot tell a change from a redescription and
would make every draw of a backdrop surface a full-surface upload. Installing a backdrop registers
a callback libkwl asks once the list is complete, and the answer is the key of what is recorded now
against the key of the picture last painted. **`kch_px_live()` is the one answer to whether a recorded op can reach a
screen**: a backdrop has to be installed, and the surface must not be a console one, where the
session composes character cells and there is no plane under them. Neither half can be dropped — a
`--dump` installs no backdrop, and `$KDOS_CON` is the only thing separating the two displays, since
the cell size is asked of libkwl either way and libkwl answers with a fallback rather than with
nothing. A control whose only state cue is a plate has to draw the cell form of the same fact where
this is false.

**Consumed input is skipped by offset and moved down once per read, not once per message.** A
peer's backlog of *n* small messages — one cell run each, which a speckled full-screen animation
sends thousands of a frame — would otherwise cost *n²/2* bytes of copying to drain, and a drain
that slows as the backlog grows is a stall that feeds itself until the peer's queue hits its cap.

## libkicon

**One job: a name, or a file path, becomes a sprite slot — or −1.**

**Minus one is not a failure.** It is a terminal, an install with no artwork, icons switched off,
and a name nothing on this machine has a picture for. **Every caller draws its glyph tier then**,
exactly as it would without this library, which is the rule the whole icon layer is built under.

**A cell under 4x4 pixels is not a pixel backend, and `kicon_init()` refuses it.** A backend with no
pixels of its own answers a cell of one — the console client does, because there are no pixels on
its side of the socket — and rasterising at that decodes, tints and rescales a PNG per name to
produce a picture a pixel across, which is a blank cell reached the long way. Refused means
`kicon_enabled()` stays false and every lookup answers −1, so the caller draws its glyph. A consumer
that ships pictures over a wire at a **nominal** cell size passes that size instead of the
backend's, which is how a console surface keeps its icons.

**A decoded picture outlives the sprite slot naming it.** The table gives slots back under its byte
budget, so a lookup that misses it re-registers the picture this library still holds rather than
paying a PNG decode, a tint of every pixel and a bilinear rescale to rebuild something already in
memory. The pictures free their own pixels on the last unref and this library keeps a reference
across the handoff, which is what makes an eviction by somebody else's evictor safe.

**A path is memoised as a slot, so whatever frees the slots drops the memo.** `kicon_slot_for_path()`
remembers the sprite slot a file's type resolved to — the lookup is a stat and a walk of the glob
table, asked once per drawn row per frame otherwise — and a memo outliving its slot either draws
nothing or draws whichever picture reclaims the index next. The consumer clears it when it re-reads
the directory it is drawing, and `kicon_retint()` clears it too, because a retint hands back every
slot the memo can name. The name-miss and app-id tables are not cleared by a retint: neither answer
depends on the accent, and both live until the program is next started.

The test harness stubs it to exactly that, so a committed reference frame is the **character grid**.

## libkcell

The glyph cache and the cell painter: a grid of cells into a pixel buffer, the character ramp built
from it, and the pixel canvas a block of cells can be drawn as.

**The frame characters are drawn, not rasterised.** U+2500's single and double box sets and the
whole of `U+2580`–`U+259F` — the full block, the eighths, the halves, the quadrants and the three
shades — are painted as pixman rectangles derived from the cell, in the cell's own foreground, and
the face is never asked for them. Rasterising them is exact only while the face's box glyphs are
drawn to precisely the advance the cell was measured from: a face whose full block spans a hair
more than its advance leaves a hairline between two cells at some pixel sizes, and a face drawing
its box glyphs to another metric dashes a border outright. **Nothing selects the synthesis** — not
an attribute, not a caller, not a tier — so the same character is the same picture in the panel,
the terminal, the installer and the build screen, under every face and at every size. The rule is
`max(1, cell_h / 16)` thick, capped at a third of the shorter side, and a double rule is that
stroke twice with one stroke of gap; the single rule sits exactly between the double's pair, which
is what makes `├` meet `─` and `╪` meet `║` with no step. **Every block edge is
`floor(span * k / 8)` from the cell's top or left**, so an eighth, a half and a quadrant in
neighbouring cells share a pixel row and a pixel column — rounding each shape from its own fraction
is what puts a seam down a bar chart. **A synthesised character is one cell wide and puts no ink
outside its own cell**, which is what the damage report and the wide-glyph clip both assume; every
rectangle is clamped to the cell after it is computed, so a cell too small to hold three strokes
draws a thinner line rather than one that leaves the cell. **They are not cached as masks**: a
cached mask is composited OVER, per pixel, through a solid source, where a fill of at most eight
rectangles issued in one call is cheaper than the composite it replaces — a cache would spend
memory to make the draw slower. The three shades are the exception, because a quarter-tone dither
at a 16x32 cell is 128 disjoint pixels: they are one repeating `a8` tile per tone per scale, four
by two cell pixels across, **offset by the cell's absolute position** so one pattern runs
unbroken across a whole shaded area instead of changing phase at every cell boundary.

**The synthesised set is exactly what the VT tier's font carries**, so a box character is the same
picture on a screen of its own as it is on `tty1` and the two tiers cannot drift apart. That
includes the two mixed single/double junctions `╪` and `╬`, which the console font has. The heavy,
dashed and rounded variants are not in it and still reach whatever face carries them.

**A cell's style is drawn here, and two of them need a second face.** Underline, strike and
overline are one horizontal rule each, differing only in the row they land on and drawn after the
glyph so a descender crossing a strike is cut by it; a broken rule — curly, dotted, dashed — is
built as an array of rectangles and issued as **one** pixman fill, because a fill call is a region
intersect before a pixel moves and a curl is dozens of pieces. Italic and bold each ask fontconfig
for the loaded name with `:slant=italic` or `:weight=bold` and **the answer is kept only if its
advance and height match the upright face** — fontconfig never fails a match, so asking for an
italic Terminus returns a different family at a different size, and a companion that disagrees
would draw a row out of step with the one above it. There are four faces, indexed by the two style
bits, and the face is part of the glyph cache's key, because the same codepoint from two faces is
two glyphs. **Where a companion is missing the style is SYNTHESISED, and the two are synthesised
differently.** Bold with no bold face is the same mask struck twice one scaled pixel apart, so it
shares the upright glyph's cache slot — the weight belongs to the blit. Italic with no italic face
is a **shear** and therefore a different mask: the upright coverage is leaned by 7/32, a little
over twelve degrees, into a slot of its own, because a shear widens the box and moves the bearing
and neither can be done at the blit.

**The shear is about the mask's vertical middle and not about the baseline**, and it moves by whole
pixels. The painter clips a glyph to its own cell, so a baseline shear — which leans the whole
letter to the right — would cut the top off every tall one; shearing about the middle spends half
the displacement on each side, and what is still lost is a pixel at each extreme of a glyph that
already fills its cell. Whole pixels because a fractional shift needs the mask resampled, and an
alpha mask resampled at a terminal's size is a blur rather than a slant. A glyph under six pixels
tall leans by nothing at all and keeps the upright mask, which is the one case where the style is
still lost.

**A codepoint is missing only when it matches the sentinel.** fcft cannot report an absent
codepoint: its fallback search ends by rasterising glyph index 0 out of the primary face, so what
comes back for a character no font carries is a valid glyph — a tofu box, or a PCF's default
character — and `NULL` means a FreeType load error and nothing else. So the load rasterises a
permanent Unicode noncharacter, which nothing can carry, and keeps its metrics and its pixels;
`kcell_has()` reports missing when a glyph is that same picture at those same metrics. **The
pictures are compared row by row over the meaningful bytes only**: a pixman image's stride is
rounded up past the glyph's width and the rasteriser writes only each row's own bytes, so a
whole-buffer compare reads uninitialised heap and reports two copies of the same `.notdef` as
different pictures — which fails open and reports every absent codepoint present. The ascii
ramp's candidate filter depends on that answer being real, and anything else drawing a fixed glyph
set should ask once at load time rather than discover the gap on somebody's screen.

**A font load replaces everything measured against the old one.** `kcell_font_load()` may be called
repeatedly and tears the previous faces down first, taking the glyph cache, the ascii candidate
table and the tiling scratch with them — so every `KCellGlyph` handed out before it, and every
cached `kcell_w()`/`kcell_h()`/`kcell_ascent()`, is invalid once it returns. `kcell_font_free()` is
that same teardown plus fcft's own, and **is not a step in a font change**: fcft's teardown is not
refcounted, so a free is a shutdown of the library.

**The changed-span repaint steps back onto a wide glyph's lead.** A row is repainted only between
its first and last changed cell, widened one cell each way because a cell's pixels are not always
its own. That is not enough on its own: a double-width glyph is painted entirely by
its lead, so a span that began on the `KTUI_WIDE_CONT` marker beside it would fill the marker's
pixels — erasing the right half of the character — and then find nothing to redraw there. The span
therefore takes one more step left when it starts on a continuation cell.

**A run of one picture's cells composites in one call.** A sprite cell names a block and a sub-cell
coordinate inside it, so cells that are consecutive columns of the same block on the same row are
one contiguous rectangle of one image and go out as a single `pixman_image_composite32`. pixman
charges most of a small composite to its setup — choosing a combiner, building the iterators,
walking the clip — and an 8x16 cell is small enough that the setup is the whole cost: a 1080p
window of blocks is 16,080 cell-sized calls painted one at a time and 1,005 row-sized ones painted
in runs, measured at 4.90 ms and 1.72 ms a frame against pixman 0.46.4 under musl. **The run breaks
on anything unusual and the per-cell path draws it**: a different block, a sprite row that does not
advance a column at a time, a glyph, the end of the changed span, and `KT_A_REVERSE` — which is the
fill the pointer puts under the cell it is over, and is therefore a cell that has to be drawn on
its own.

**The cached foreground sources are keyed on the colours, not on the theme's address.** A glyph is
composited through a solid-fill image and those are kept per slot and per literal colour; libktui
projects night light by rewriting one table in place, so a cache keyed on the table's identity
would keep painting the old scheme's ink after a retint while backgrounds and rules, which read the
palette fresh, came up in the new one. The literal table is indexed by the **high** bits of its
multiplicative hash, since the low bits of a product carry only the low bits of blue and would land
the whole xterm colour cube in six buckets.

**The canvas is what makes a pixel tile possible** without a second renderer — a pixel image exactly
some number of cells across, with fills and text at an arbitrary pixel size, handed to the toolkit
as a sprite. See [kdos-shell](../04-programs/kdos-shell.md#the-start-button).

**fcft is reference-counted inside libkcell, so the two entry points are free of each other and of
any ordering.** `fcft_from_name()` answers `NULL` for every request until `fcft_init()` has run, so
a consumer that draws canvases and never calls `kcell_font_load()` — a console surface, whose cells
are characters on a wire — would measure every string as zero and draw none of them, with the
library's own complaint going to a stderr nothing reads. And neither of fcft's own calls is
idempotent: a second `fcft_init()` replaces FreeType's handle and orphans every face resolved
through the old one, and `fcft_fini()` destroys FreeType whether or not anything still wants it.
So `kcell_font.c` owns fcft for the whole library and counts its holders, declaring the pair in
`kcell_priv.h`; the cell font takes one reference and the canvas takes one of its own, the library
comes up on the first and goes down on the last, and **either may be used first, in either order,
and neither tears fcft down under the other.** A canvas drawing through a `kcell_font_free()` keeps
its faces, and a failed `kcell_font_load()` gives its reference back before it returns `-1` — a
caller reading that as "no cell font" has nothing left to free. The pair is private: a reference
taken outside libkcell matches no face inside it, so nothing could ever release it.

## libkkms

The cell grid on a screen: seat, connector, mode, a dumb buffer, libinput and xkb. The one library
here that opens a GPU device, which is why only the view links it.

**The grid is derived, never stored.** The backend answers its size by dividing the mode by the
cell, so `kkms_set_font()` is the whole of a font change on a screen that is already up: reload,
and every consumer of `ktui_w`/`ktui_h` sees a different answer the next time it asks. The caller
calls `ktui_draw_resize()` and tells whoever is composing for it — this library knows the pixels and
nothing about the session on top of them. **The old font comes back if the new one will not load**,
because a screen is the one thing a person cannot work around from somewhere else.

**Up to three scanout buffers a screen, plus the painter's own, and the painter never touches one
the kernel can see.** The cells are composited into a system-memory image, the rows that changed
are copied into a buffer that is neither on the screen nor named by a flip, and a page flip points
the CRTC at it. Painting into the buffer being scanned out is what tearing is, and compositing into
it is worse than slow: a dumb buffer is mapped write-combined and every `OVER` reads the
destination back.

**Every buffer is in exactly one role, and the roles are the safety argument — not arithmetic on an
index.** `front` is under the raster, `queued` is named by a flip the kernel has not reported,
`ready` holds a composed frame waiting for the queue to clear, and anything left is the painter's.
`kkms_ready()` is false when no buffer is left, `kkms_drm_fd()` is the descriptor a flip completes
on, and together they are how a view draws at the refresh rate instead of on a timer. **The third
buffer is what stops the painter waiting for the vblank**: with two, the only buffer it may touch
is the one the flip is waiting on, so a compose that overruns a refresh period costs a whole
further period — the 60-to-30 cliff. With three the next frame is composed during the flight and
the flip's own completion presents it, so the latency lost is that frame's and not the next one's.
**At most one frame waits**, which is what makes the presentation order the paint order without a
queue to keep in step. `KkmsTune.buffers` is a ceiling of 1 to 3 and a driver with no memory for
the third gets two; `front` moves only when a completion arrives, because every relight and every
wake points the CRTC at it.

**More than one buffer is for hardware and for host-scanned virtual framebuffers; a transfer-model
driver stays single-buffered.** `virtio_gpu`, `qxl` and `vmwgfx` copy the guest's buffer to the
host at the rectangle `drmModeDirtyFB` names and read it at no other time, so one buffer there is
already tear-free and a legacy page flip — no damage rectangle, so the driver takes the whole plane
— turns a few changed rows into an 8 MB upload. The decision is the driver's name, read once at
open, not "is this virtual": QEMU's stdvga is scanned continuously and tears exactly like hardware.
**A refusal is only permanent when it is about the driver.** `EINVAL`, `ENOSYS` and `EOPNOTSUPP`
give up every buffer but one, and the frame is copied into the survivor before the CRTC is pointed
at it — anything else (`EBUSY` from a CRTC the screen blank detached, `EACCES` on the way back from
another VT) costs one repainted frame and changes nothing.

**A row painted is owed to every buffer and cleared only in the one it is copied into.** The
buffers are frames apart, so a row given to one is still an older frame's in the others; with three
it stays owed to two. The strip below the last whole cell row, where the mode is not a multiple of
the cell, carries the same debt separately because no row covers it and the painter fills it only
on a full repaint — given to one buffer alone it blinks at a fraction of the flip rate.

**The mode policy and the present are the caller's, passed to `kkms_init()` as `KkmsTune`.**
`KKMS_MODE_PREFERRED` takes the monitor's EDID choice and is the default; `KKMS_MODE_FASTEST` takes
the highest refresh **at the size the monitor chose** and never another resolution, because a
scaled desktop is a blur nobody asked for. A mode already in force still outranks both, so a hotplug
for an unrelated connector does not undo a choice somebody made; that match is on the size and the
**computed** refresh, because 59.94 and 60 share a rounded `vrefresh` and a connector listing the
59.94 mode first would otherwise hand it back on every re-probe. `tearing` passes
`DRM_MODE_PAGE_FLIP_ASYNC` to the same legacy flip, which presents immediately and cuts a moving
edge across the screen; it is off unless asked for, silently off where `DRM_CAP_ASYNC_PAGE_FLIP` is
absent, and **dropped for the rest of the session on its first `EINVAL`** — a driver refusing the
flag would otherwise be read as a driver that cannot flip at all, which costs every buffer but one.

**The refresh is published so a session can pace itself to the screen.** `KkmsOutput.refresh` is
the timing in force per screen and `kkms_refresh_mhz()` is the highest among the lit ones, both in
millihertz and both computed from the clock and the totals rather than read from the kernel's
rounded `vrefresh` — 59.94 reported as 59 is two modes a picker cannot tell apart. The fastest
screen is the right one to pace to because one grid is cut across all of them: a frame slow enough
for the 60 Hz panel is a frame the 144 Hz one shows twice. It moves with a hotplug and a mode
change, so it is read again wherever those are announced.

**Nothing is painted while the screen is blanked**, and going dark retires any flip in flight,
because a detached CRTC never presents the frame a flip is waiting for and a flush skips an output
that is still waiting. Waking owes every output a full frame. **`kkms_blank()` records the state
even when it cannot program the device** — a call that lands while the session is switched away
still takes effect, and coming back from the other VT puts every CRTC in whichever state the last
call named — so one call per transition is enough and a caller that tracks its own idea of awake
never has to re-send.

**The pointing devices are configured by policy, not per device.** `kkms_set_input()` takes a
`KkmsInput` — speed, natural scroll, tap-to-click, tap-drag, disable-while-typing, left-handed,
middle emulation — and applies each field to every device on the seat that **accepts** it. A device
that will not, a mouse asked about tap-to-click, is skipped rather than treated as a failure: one
answer covers a seat made of different hardware, and a refusal there is not something a caller can
act on. `KKMS_IN_KEEP` in a field leaves libinput's own default for that device class standing,
which is not the same as `0` — tap-to-click off and tap-to-click unset differ on a touchpad whose
driver enables it. `speed` is a tenth of libinput's `-1.0..1.0` so a configuration file and a
slider can both be whole numbers, and `0` is the middle of the device's range rather than an
unaccelerated pointer.

**The open devices are held by reference.** libinput frees a device object when it is removed, so
a bare pointer kept here would outlive it; a reference of our own is what lets the policy be
re-applied to everything already open. They are tracked at `DEVICE_ADDED` and `DEVICE_REMOVED`,
**including while the seat is switched away** — the devices are suspended, not unplugged, and one
added during another session's turn would otherwise come back unconfigured and untracked for the
rest of this one.

**Key repeat is this backend's, because libinput has none.** One press and one release is all a
device reports; a compositor owns the rest, and here that is this library. The deadline is checked
from the same idle pump the long-press recogniser uses. **A repeat carries the modifiers as they
are held now, not as they were latched at the press** — taking Shift while an arrow is held extends
a selection — while the keysym stays the one that was pressed, exactly as `libkwl` does it, because
re-resolving it would turn a repeating letter into its capital mid-stream.

**The pointer is an arrow composited into the shadow after the cells, and erasing it is the harder
half.** Nothing in the cell model knows the arrow is there, so the cells it covered are unchanged
and the row diff finds nothing to repaint: this library puts those cells back into its own previous
frame, which is what makes the next paint rewrite the pixels under the old arrow. A move is also
carried past the nothing-changed exit, or the first arrow is never drawn and every later one is
drawn where it was. Getting either wrong is a trail of arrows down the screen, one per place the
hand stopped. The rows the arrow covers are marked owed **only when it moved** — a row the paint
touched is already owed, a row it did not holds the same pixels it held last frame, and `owed` is
per buffer and sticky. **The mask is drawn in code**, 11x18 scaled by a whole number of pixels to
about one cell tall, with the outline computed from the mask's own eight-neighbourhood; the body is
`KT_TEXT` and the outline `KT_BG`, so the arrow follows `kdos theme` and night light like
everything else. **Not a hardware cursor plane** — see
[known-gaps](../06-reference/known-gaps.md).

**The virtual box is a whole number of cells.** Each screen contributes its own cell width and the
trailing partial cell of its mode is padding no pointer can enter. Summing raw mode widths instead
invents a column that belongs to no screen's slice, because `floor(sum(w)/cw)` can exceed
`sum(floor(w/cw))`: the arrow vanishes in the last strip of the screen and the click lands on
nothing.

**A VT switch suspends libinput and resumes it.** The seat revokes every evdev descriptor when it
deactivates a session and hands none back by itself. Keys that arrive while switched away still
move the xkb state, or the modifiers held down when the switch fired are still held when the
session returns. A release still travels raw while switched away, because a switch that swallowed
one leaves the chord's modifier held down in a guest for ever.

**This is the backend that fills the raw queue completely.** libinput reports the unaccelerated
distance, the discrete detent count and every button's evdev code, and xkb compiles the layout the
session's own environment names — so `poll_raw` and `keymap` are both real here and a view on this
backend is the one that claims `KCON_VIEW_RAW`. The exception is touch: the gesture recogniser
synthesises a cooked pointer event and no raw partner, so a finger reaches an embedded guest at the
grid's resolution, which is the resolution a finger has.

## libkwl

The toolkit's Wayland backend. **The one library with real link dependencies beyond the cell
painter's.**

Its rules are the ones a surface author meets, and they are in
[Writing desktop software](writing-desktop-software.md). In summary, each guarding a distinct
failure:

- **A toplevel must ask for its frame**, or it gets no decoration at all.
- **Bind the layer shell at the version whose keyboard mode you want**, or an older resource
  answers on-demand with exclusive and the surface holds the seat's keyboard against every window.
- **The event queue is a ring, not one slot**, because a batch of callbacks arrives from a single
  read.
- **Key repeat is the client's job.**
- **A wheel tick is not an axis event**, and a wheel is already quantised while a touchpad is not.
- **One pointer frame is one detent** on the discrete path.
- **A motion that did not move is not a motion.**
- **A double buffer needs a shadow per buffer.**
- **A serial must be retained**, or the clipboard has nothing to present.
- **An enter carries coordinates and they are not optional.**
- **The scale and the resized buffer must land in one commit.**
- **A data source is destroyed on cancellation, never at set time**, or the copy silently does
  nothing.
- **The clipboard and the primary selection are separate stores**, and an in-flight send keeps the
  payload it started with — a terminal sets the primary selection on every mouse release, and one
  shared payload splices that text into a clipboard send that is still draining.
- **A parked send is polled for writability and its deadline counts from the last byte that
  moved.** Anything past one pipe buffer parks, and a budget counted from the first write closes
  the pipe on a receiver that is still reading — which it cannot tell from a clean EOF, so it
  accepts a truncated selection.
- **A throttled frame counts as presented**, because the stash carries its own `full` and is
  committed by the frame callback that follows. Answering otherwise makes the toolkit re-arm a
  full repaint that never clears, on every surface that draws at the display's rate.
- **A surface is clamped against the output's logical, upright box** — the mode divided by that
  output's scale, with the axes exchanged on a 90 or 270 degree transform — and never against a
  slot whose proxy is gone. Each omission lets a popup be sized for a screen that is not there.
- **A stash is content for the grid it was taken from**, so every resize drops it; publishing it
  paints the old layout into the new geometry.
- **The cell painter leaves a clip on the image it was handed**, so anything drawn into that same
  image afterwards — the panel's frame rule — must drop the clip first or pixman writes nothing.
- **A compose table that fails to build is absent, never partial.**
- **A font reload spoils every paint baseline, because it rewrites no cell.** `kwl_font_step()`
  changes what a cell LOOKS like and not what it says, so the damage diff, both buffer shadows and
  the unchanged-frame gate would all find nothing to do while every glyph on the screen is drawn at
  the old size. It also owns its own copy of the name: `KDispConfig.font` is the caller's pointer
  and the surface outlives whatever the caller built it in.
- **A lock surface must not receive the pre-configure commit**, which is a protocol error there.
- **A Ctrl chord is the letter plus `KT_MOD_CTRL`**, never the control code xkb folds it into —
  the tty decoder and libkkms both deliver the letter, and a chord table has one vocabulary.
- **A drop's offer is owned apart from the drag's**, because the leave that follows a drop arrives
  while the payload is still draining and a second drag may enter before it ends. One slot for both
  loses the first offer and destroys the second while it is live, so the next drag lands and does
  nothing.
- **A backdrop's "my pixels moved" is a flush-time question, not an announcement.**
  `kwl_pixels_dirty()` latches; a backdrop redescribes the same plates on every draw and cannot
  tell a change from a redescription, so it installs `kwl_set_pixels_dirty_fn()` instead. Latching
  from the description makes every frame a commit and removes the unchanged-frame gate for every
  surface that has a backdrop.
- **A withdrawn seat capability takes everything derived from it.** The `wl_keyboard`'s repeat and
  focus state, and the `wp_cursor_shape_device_v1` made from the `wl_pointer` — the protocol makes
  that device inert with the capability, and the re-create tests only for NULL, so a device kept
  across an unplug leaves every later shape request going to a dead proxy.
- **`kwl_init` ignores `SIGPIPE` process-wide**, because every clipboard and drag payload is
  written into a descriptor the receiver owns and the default disposition kills the surface when
  that receiver closes early. `SIG_IGN` survives `execve`, so any consumer that forks and execs
  must reset dispositions in the child or the whole launched tree inherits it.
- **The raw queue is filled, with one number this protocol cannot carry.** `wl_pointer` reports a
  position and no distance, and this client binds no relative-pointer protocol, so the delta is the
  step between two surface positions — already accelerated, already clamped to the surface, and the
  same number in both the accelerated and the unaccelerated pair. Everything else is real: evdev
  codes, the four xkb components the compositor sends, `value120` and the axis source, and the
  keymap the compositor handed over.
- **A key held when the surface loses the keyboard is released into the raw stream here.** The
  compositor sends no release for it, and the raw arm carries a switch rather than a character, so
  a press with no release is a key a pixel guest holds down for ever. Only codes this client
  reported down are released, which is what keeps the two halves symmetrical.

## Adding a library

1. **Decide what it owns**, in one sentence. If that sentence has an "and" in it, it is two
   libraries.
2. **Pick a prefix** and use it on every exported symbol.
3. **Place it in the dependency order** and confirm nothing points back up.
4. **Link nothing but the C library**, unless you are extending the one declared exception — in
   which case say so here and move every phase-1 consumer.
5. **Add its assertions** to the shared test program, especially any invariant that was established
   by comparing against something this library replaced.
6. **Add it to the consumer compile check**, so a header change that breaks a consumer fails on a
   development host rather than in a phase.
7. **Keep the frame state and any global private**, behind accessors.

## See also

- [Writing desktop software](writing-desktop-software.md) — building on the drawing libraries
- [The design language](../03-architecture/design-language.md) — the rules they enforce
- [Testing](testing.md) — the shared test program and the sanitizer runs
- [The build system](build-system.md) — where `libkbuild` fits
- [Packaging](../03-architecture/packaging.md) — where `libkpkg` and `libksig` fit
