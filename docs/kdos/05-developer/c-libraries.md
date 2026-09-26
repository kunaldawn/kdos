# The C libraries

This page describes the seventeen `libk*` libraries under `src/libs/` that every program KDOS
writes is built from: what each one owns, which of them may depend on which, which external
libraries they are allowed to pull in, and the rules each library exists to enforce.

It is for people changing or extending KDOS's own C code. If you are writing a new window, menu or
settings page, read [Writing desktop software](writing-desktop-software.md) first; it covers the
same libraries from the point of view of someone building a surface on them, and sends you back
here for the detail. The libraries' tests are described in [Testing](testing.md).

After reading this page you should be able to find the library that owns a piece of behaviour, add
to it without breaking the program that can least afford a new dependency, and add a library of
your own.

## How the libraries are built

A `libk*` library is a directory of C sources and one public header (some also carry private
headers of their own), not an installed archive or a shared object. Each program that uses a library compiles that library's `.c` files straight into its own
binary, from its own `build.sh`, and names the external libraries it needs on its own link line.
For example, `kinstall`'s build compiles `src/libs/libkbase/*.c`, `src/libs/libktui/*.c` and
`src/libs/libkcolor/*.c` together with its own sources. `kdos-comp` is the one exception in shape
only: its build compiles `libkbase`, `libkcolor` and `libkwm` into a local `libkdos.a` because
meson takes its extra objects that way.

Two consequences follow:

- Nothing is installed under `/usr/lib` for these libraries, and nothing outside this tree can link
  them.
- A program's recipe has no upstream tarball, so its recipe hash covers the whole of `src/libs/`.
  Editing any library therefore rebuilds every KDOS program on the next build, not only the ones
  that use it. See [Developing](developing.md) for the narrow rebuild commands.

## The constraint

The libraries a terminal program needs link nothing but the C library.

The reason is phase 1 of the build (see [The build system](build-system.md)). The installer,
`kinstall`, and the package manager, `kpkg`, are both compiled there, before any other library
exists to link against: `kinstall` uses `libkbase`, `libktui` and `libkcolor`, and `kpkg` uses
`libkbase`, `libkpkg` and `libksig`. If any of those five ever needed a real `-l` flag, both
programs would have to move to a later phase with it, and the first bootable image would lose its
installer.

Twelve libraries keep that rule outright: `libkbase`, `libkcolor`, `libktui`, `libkxdg`, `libkpkg`,
`libksig`, `libkbuild`, `libkproc`, `libkpack`, `libkvt`, `libkwm` and `libkdisp`. `libksig` carries
its cryptography as vendored source rather than linking it, and `libkproc` opens NVIDIA's
management library at run time only when one is installed, so neither adds a link flag.

The other five draw pixels, and pixels need real libraries. Each is a separate directory precisely
so that the twelve above stay clean, and so that a program only pays for what it draws:

| Library | External libraries it needs |
|---|---|
| `libkcell` | fcft (font rasterising) and pixman |
| `libkicon` | pixman and libpng |
| `libkimg` | pixman, plus libpng, libjpeg, libwebp, libsixel and libnsgif — each optional, switched on by a `KIMG_HAVE_*` define |
| `libkwl` | wayland-client, xkbcommon, fontconfig, fcft and pixman |
| `libkchrome` | pixman directly (the pixel tile), plus everything `libkcell`, `libkicon` and `libkwl` need, since it is built on them |

`libkcell` and `libkwl` are split for the same reason one level down: a program that wants the
cell painter is not made to link a Wayland client library to get it.

## The set

The prefix column is the one every exported symbol of that library carries (a second prefix marked
"internal" is linker-visible but declared only in a private header); the "Built on" column
lists the other `libk*` libraries it calls into.

| Library | Prefix | Owns | Built on | Used by |
|---|---|---|---|---|
| `libkbase` | `kb_` | Allocation and its failure hook, fatal and warning output, strings, files, paths, locking, monotonic time, SHA-256 and MD5, base64, `file://` URIs, group membership and the root-daemon authorisation gate, the argument-vector builder and process helpers, Landlock self-sandboxing, and the freedesktop trash | — | Every KDOS program except `kdos-splash` and `kdos-bb`, plus `kpkg` and the build tools |
| `libkcolor` | `kcol_` | The palette table, colour-space conversion, mixing, the readable muted colour, the hue-family classifier, remapping and retinting | `libkbase` | Every drawing program, `kdos-comp`, `kdos-theme`, `kdos-tools`, `kdos-powerd` |
| `libktui` | `ktui_` | Terminal ownership, the cell buffer and its diff, key and mouse decoding, the touch-gesture recogniser, character width, paste, immediate-mode widgets, menus, modals, the keys contract, the selection rule every surface draws its rows with, the three glyph tiers, charts, offscreen rendering | `libkcolor`, `libkbase` | `kinstall`, `kdosbuild`, `kdos-appbox`, `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-lock` |
| `libkxdg` | `kxdg_` | Desktop entries, the MIME glob table, the one correct way to turn a command line into an argument vector, places, recent files and file verbs | `libkbase` | `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-appbox`, `kdos-tools` |
| `libkpkg` | `kp_` | The package database, the ports tree, dependency parsing and solving, version comparison, the recipe and build-config hashes | `libkbase` | `kpkg` (also compiled on the build host as the recipe reader `ports/fetch` uses), `kdos-portup`, `kdos-pack`, `kdos-packd`, `kdos-tools` |
| `libksig` | `ksig_` | Ed25519 signing and verification, key files, keyrings. The one library with vendored third-party source | `libkbase` | `kpkg`, `kdos-pack`, `kdos-packd`, `kdos-tools` |
| `libkbuild` | `kbuild_`, `kj_` | Phase discovery, the phase metadata block, the build plan, the snapshot inventory, a read-only structured-data scanner | `libkbase` | `kdosbuild`, `kdos-portup` |
| `libkproc` | `kpr_` | Every reading about the running machine, from a movable root: processes, uptime, box identity, processor, memory, block devices, network, power, graphics, sound PCMs — and the sample ring | `libkbase` | `kdos-res`, `kdos-shell`, `kdos-tools`, `kdos-oomd`, `kdos-energyd` |
| `libkpack` | `kpk_` | The pack format: the footer, the metadata blob, the requirement solve, the payload hash, the signature block, and the index | `libkbase`, `libksig`, `libkpkg` | `kdos-pack`, `kdos-packd`, `kdos-tools` |
| `libkvt` | `kvt_` | The terminal: the VT100–VT520 state machine, the screen, scrollback, selection, the pty, and one render boundary that turns it all into cells. A hard fork of libtsm 4.7.1 | `libktui`, `libkbase` | `kdos-term` |
| `libkimg` | `kimg_` | The only place untrusted image bytes are decoded. Two entry points — one picture, or every frame of the one format that has more than one — five optional decoders, and a budget enforced from the header the format declares *before* any allocation | — | `kdos-shell`, `kdos-term` |
| `libkwm` | `kwm_` | The window model the compositor obeys: placement, the tiled-state transition and its geometry, the neighbour-edge arithmetic, the nearest occupied workspace, and the drag threshold | — | `kdos-comp`, `kdos-shell` |
| `libkdisp` | `kdisp_` | Which display server, decided once: the surface config, the seven roles, the lifecycle every surface asks for, the font list, and the window list a panel manages | `libktui` | `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-lock` |
| `libkchrome` | `kch_` | The window furniture: the header band, group headings, the button bar, the list and scrollbar rule, the pixel tile | `libktui`, `libkcolor`, `libkicon`, `libkcell`, `libkdisp`, `libkwl` | `kdos-shell`, `kdos-res` |
| `libkicon` | `kicon_`, `ki_` (internal) | A name becomes a sprite slot, or −1 | `libktui`, `libkcolor`, `libkxdg` | `kdos-shell`, `kdos-res` |
| `libkcell` | `kcell_` | The glyph cache and the cell painter — a grid of cells into a pixel buffer, the character ramp built from it, the pixel canvas, and the one scale-and-cut of a decoded picture into sprite tiles | `libktui`, `libkcolor` | `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-lock` |
| `libkwl` | `kwl_` | The toolkit's Wayland backend: surface roles, buffers, scale, the font in force, input, touch, clipboard, compose, cursors, frame throttling | `libkcell`, `libkdisp`, `libktui`, `libkbase` | `kdos-shell`, `kdos-res`, `kdos-term`, `kdos-lock` |

## Dependency direction

Each arrow reads "calls into". The graph has no cycles, and it must stay that way: nothing lower
down may call anything higher up.

```
libkwl     → libkcell, libkdisp, libktui, libkbase
libkchrome → libkicon, libkcell, libkdisp, libkwl, libktui, libkcolor
libkicon   → libkxdg, libktui, libkcolor
libkcell   → libktui, libkcolor
libkdisp   → libktui
libkvt     → libktui, libkbase
libktui    → libkcolor, libkbase
libkcolor  → libkbase
libkxdg    → libkbase
libkpkg    → libkbase
libksig    → libkbase
libkbuild  → libkbase
libkproc   → libkbase
libkpack   → libksig, libkpkg, libkbase

libkwm     (nothing)
libkimg    (nothing of ours)
```

Three edges are worth stating explicitly:

- `libkvt` is a terminal's private screen and reaches `libktui` only at its render boundary.
- `libkimg` decodes untrusted bytes and calls no other `libk*` library, so a decoder bug cannot
  reach into the toolkit.
- `libkwm` calls nothing at all, which is what lets the compositor take it without taking anything
  else, and lets the self-test replay it against a fixture with no display.

## libkbase

The floor. Everything else is built on it: allocation, strings, files, paths, locking, time,
hashing, process helpers.

A library does not own the exit path. The allocator calls whatever failure handler was registered
rather than knowing that a terminal exists and that the program is called something specific.
`kb_set_progname()` supplies the program name for that message and for the fatal and warning
output.

Several members are worth knowing about specifically.

`kb_copy_file` streams. It is what moves application packs, and those run to hundreds of megabytes,
with the pack daemon copying one into the store as root on every install. Reading a file whole to
write it whole asks for its size in anonymous memory for no reason.

The process helpers send a child's error output to nothing unless `kb_proc_verbose` is set, so
anything whose *failure* is diagnosed by the child's own message has to turn that on.

A secret reaches a child on a descriptor and never in `argv`. `kb_run_feed` writes to the child's
stdin; `kb_run_feed_env` does the same and additionally sets names in the child's environment and
captures its stderr, which is what a mount helper needs — `mount.cifs` reads a password from the
descriptor `$PASSWD_FD` names and reports its refusal on stderr, and a caller without both would
either put the secret in an argument list or report a status with no reason. The input must fit in
one pipe buffer in the feeding-and-reading forms: nothing is read back until the whole input has
been written.

The freedesktop trash lives here (`kb_trash_put`, `kb_trash_list` and their neighbours), so
`kdos trash` at a prompt and the desktop's delete key are one implementation. See
[The kdos command](../04-programs/kdos-command.md#kdos-trash).

Two hashes live here, for two different jobs. `kb_sha256_*` checks the `sha256 =` line of a recipe
before an archive is unpacked. It lives in the base library because `libkpkg`, `libksig`,
`libkpack` and the programs above them all hash, and none of them may link a crypto library. It
proves the bytes are the ones the recipe named and says nothing about who named them —
signatures are `libksig`'s. `kb_md5_*` names thumbnail cache files, because the freedesktop
thumbnail standard names them by the MD5 of the source URI and a stronger hash would produce a
cache no other program could share. It is a file name, never a security check.

### Landlock

`kb_landlock_*` is unprivileged self-sandboxing through the kernel's Landlock interface: three
system calls and no library. A ruleset starts with nothing reachable, each `kb_landlock_allow()`
opens one directory tree back up (read-only or read-write), and `kb_landlock_enforce()` makes it
permanent for the process and everything it starts. The order is always new, allow, enforce, exec.
`kdos sandbox` is built on it; see [The kdos command](../04-programs/kdos-command.md#kdos-sandbox).

What the running kernel can police depends on its Landlock ABI version. Denying TCP needs ABI 4,
and scoping abstract sockets and signals needs ABI 6. Below those versions the request is silently
not applied, so a caller that must not run without it checks `kb_landlock_abi()` first and refuses.
An ABI of `-EOPNOTSUPP` means Landlock is compiled into the kernel but not enabled in `CONFIG_LSM`
or `lsm=`, which is the quiet failure worth reporting: without the check, everything runs with no
sandbox and nothing says so.

### The root-daemon gate

`kb_uid_allowed(uid, group)` is the whole authorisation boundary of the five root daemons: is this
uid root, or a member of that group. Each daemon reads the peer's uid from `SO_PEERCRED` — which
the kernel fills in and the peer cannot forge — and asks this one function before acting as root on
the peer's behalf.

It is one function because five copies are a rule that gets tightened on one socket and stays loose
on the other four, with nothing in the tree to show which is which. A daemon that needs a different
rule states the difference beside its own call and still asks this for the rest: `kdos-mountd`'s
`--fixture-serve` mode is the only one that does, and it grants nothing, because its paths are a
scratch directory and every child it would spawn is printed instead of run.

A uid with no passwd entry is refused. Membership itself is `kb_user_in_group`, which counts the
group's own gid as well as the member list — a user whose *primary* group is `wheel` never appears
in that list, and that is exactly the account an installer creates.

### kb_fuzzy

`kb_fuzzy` is the desktop's only answer to "does this row match what was typed". It matches a
subsequence rather than a substring — so `sm` finds `System Monitor`, which no substring search can
— scored so a prefix beats an acronym, an acronym beats a run, and a run beats a scatter. Higher is
better and zero is no match.

Each needle character prefers a word start over an earlier occurrence inside a word, which is what
makes the acronym score reachable. Because that preference is greedy it can consume a character the
rest of the needle still needs, so a preferring pass that fails is redone taking the leftmost
occurrence of each character. Any needle that is a subsequence of the row therefore matches, and a
row never loses to a prefix of its own name.

It lives here rather than in a surface because three surfaces search the same applications: the
palette, the launcher and the Start menu. One query ranked three ways teaches a person that the
desktop's search cannot be relied on — somebody finds a thing in one surface and nothing in
another.

A caller that sorts on it must sort descending. A sort written for a lower-is-better matcher ranks
a correct list backwards, which reads as bad ranking rather than as a bug.

### The argument-vector builder

The builder stores the pointer and does not copy. Several arguments built one after another in a
single reused buffer therefore all point at the same bytes, and the program is handed the last
value repeatedly. Use the formatting variant, which writes into the vector itself. This is a
property of the builder, not of any one caller.

## libkcolor

The palette table is an X-macro, so every consumer expands the same literal values into its own
table at compile time. The toolkit projects them onto its colour slots; the theme generators expand
them into stylesheets, vector artwork and cursor images. Nobody keeps a second copy of the numbers.

The colour-space conversions reproduce a specific reference implementation exactly, including its
unusual intermediate expression, its handling of a negative hue, and its round-half-to-even
rounding. That is not pedantry. The vendored icon and cursor artwork is committed, so a generator
that rounds differently produces a diff against files already in version control. The equivalence
is verified across thousands of colours in every accent.

The library stays off the maths library for the same reason the toolkit does — a phase-1 consumer
cannot link one — so the modulo is done in a loop and the rounding by hand.

The two mixing functions are not interchangeable. The integer and floating-point forms disagree by
a unit on some inputs, and each generated file was written against exactly one of them. The test
suite asserts that they disagree, so nobody "fixes" it.

Two derived colours exist, and confusing them is a legibility bug. The dim value is a fill and
measures below any text floor against the background; the derived muted colour is the readable one,
and every text role goes through it. See
[the design language](../03-architecture/design-language.md#colour).

## libktui

The toolkit: terminal ownership, the cell buffer, the diff, input decoding, widgets and charts.

### Capabilities and the frame bracket

Three glyph tiers are chosen from the terminal's capabilities, because the VT font holds 512 glyphs
and a character it lacks renders as a blank. The table is in
[the design language](../03-architecture/design-language.md#the-glyph-tiers).

What a terminal can do is asked for once, in one write, on entering the screen. Two facts follow
from no `TERM` value and no capability database entry: the kitty keyboard flags (`CSI ? u`), which
is what makes `Super` arrive at all, and synchronized output (`CSI ? 2026 $ p`), which is what lets
a frame be bracketed. Both queries go out together and their replies are told apart by scanning the
buffer — asking in turn pays the timeout twice on a terminal that answers neither, and the second
wait would swallow a slow answer to the first as if it were its own. A DECRQM value of `0` means
"not recognised" and `4` means "can never be set", so only `1`, `2` and `3` set `KT_CAP_SYNC`. The
replies are consumed there or they are typed into the desktop, and the Linux VT, which answers
neither, is skipped rather than waited on.

A frame is bracketed where that bit is set: `CSI ?2026h` before the diff and `CSI ?2026l` after it.
A diff frame is a scatter of cursor moves and single cells, and a terminal drawing as they arrive
shows a menu before the one under it is erased. The close is written again when a bounded flush
dropped the frame, and once more when the terminal is handed back, because a terminal left inside
the block draws nothing further — an unclosed bracket is a frozen screen rather than a tear.

The progress bar is a wrapper whose pixels must not move. The installer links it and only it,
pinning a solid two-state bar. Change the general form freely; leave that branch alone.

### Resizing, the caret and the pointer

A resize is not applied until the consumer applies it. The backend sets `ktui_resized`; the cell
buffer follows only when the loop calls `ktui_draw_resize()` and `ktui_draw_invalidate()`. Any loop that owns a surface
owns this — a surface that was always a fixed size and then starts being resized will draw against
stale dimensions and silently fail its own bounds checks.

The caret goes to the backend when the backend has one. `ktui_term_caret()` is the one call a
surface makes to say where it is typing, and it writes the terminal escape only when nothing else
claims the answer. A client drawing through a display server claims it: its stdout is not the
screen it appears on, and the position it knows is in its own cells. A backend that leaves the
entry NULL keeps the escape, which is what the Wayland one does, because a compositor's surfaces
draw their own.

The pointer goes the same way and for the same reason. `KtuiBackend.pointer` is handed the cell the
pointer is on and answers whether it drew one itself; a backend that composites an arrow into a
framebuffer it owns says so. Everything else leaves the entry NULL and `ktui_draw_flush()` reverses
the cell under the pointer, which is the pointer a `--tty` run, a `--dump` and `tty1` can show. An
arrow cannot be drawn in this library: it links nothing but musl and has to keep doing so, and an
arrow needs a pixel buffer and a colour in it. The hook is called on every flush, with a negative
`x` for no pointer at all, because that call is the only thing that tells a backend to take the
last arrow off the screen. Over a cell marked `KT_A_GUEST` the flush reports no pointer to either
path: the guest's own compositor draws a cursor into those pixels, and a second one a cell away is
the one nobody is aiming with.

Offscreen rendering takes a fixed size and writes the cell buffer out as plain text, with no
terminal at all. A geometry defect in this toolkit is invisible to the compiler and to a test suite
that cannot draw, so this is the only way one gets looked at.

### Widgets

A widget is either an immediate-mode call or a draw-and-key pair, and which one is decided by its
callers. `ktui_list`, the buttons, the checks and the input field read the frame's focus and return
what happened in one call, for a surface built around the frame. The page strip, the column table,
the dropdown and the text block are a `_draw`, a `_key` and a `_hit` instead, because every surface
that wanted them runs its own event loop and holds its own selection — an immediate-mode form would
have meant rebuilding each of them around the frame before it drew anything at all. A hit test
takes the same rect its draw took, so it measures what is on the screen rather than what the widget
remembered from an earlier size.

No widget owns its selection. The caller holds it, because the caller is what persists it, dumps it
and restores it — and because a page strip and the body under it are one selection seen twice.

A table's heading row says whether the selection may land on it. Both readings are in the tree: a
network device heading is the row `Enter` rescans from, and a device-section caption is furniture.
The row-kind callback answers per row rather than the widget choosing for both, and a row the
selection steps over never lights.

Three rules hold about the library's own state, each guarding a link or focus failure:

- Symbols are prefixed. Generic names collide with a consumer's own definitions of the same names
  with different semantics, which is enough to make two of our own programs unlinkable together.
- The frame state is private, behind accessors. A public structure that applications assign to
  field by field is a second API nobody can change.
- Chrome identifiers are the library's business. Chrome registers with caller-local identifiers in
  a reserved range that never joins the focus ring and never drags the page scroll.

### The sprite table

A sprite table entry is a borrowed pointer, so eviction is what the owner told it to do. The table
does no pixel work and cannot free a picture; an owner registers an evictor and the table calls it
whenever the table itself stops naming a picture — a slot taken back under the byte budget, a slot
reused for a *different* picture under the same key, or a tile refused part-way through a tiled
put.

`ktui_sprite_drop` is the one way a picture stops being named without the evictor. Dropping by key
is the owner's own call, and a callback there would be a free from inside that call, so the table
hands nothing back and the owner unrefs what it dropped and clears whatever it keeps beside the
slot.

Without an evictor a full table answers −1, which every consumer already handles by drawing
its glyph. That is right for icons, which are owned for the life of the session, and wrong for
photographs, which are megabytes each.

A refused put is a hole, and only the owner can see it. `ktui_sprite_put` answers −1 when the
budget cannot be made to fit: eviction skips every slot the cell grids still reference, so a table
whose pictures are all on screen has nothing it may take. A caller that stores that −1 as a slot
draws the background where a picture belongs, and the table can never repair it because it never
learned the picture existed. A consumer holding pictures that are not its own has to tell the side
that owns them.

There is one evictor per process, and every owner in it shares that one. A program that draws
photographs and icons has a single function handing back pictures built by two different libraries,
so no owner may assume the evictor is its own free. A picture given to the table must free its own
pixels on the last unref — a pixman image carries a destroy function for exactly that — and its
owner must hold a reference of its own across the handoff, or an eviction leaves that owner's cache
naming a freed image.

Whether a sprite is still on screen is asked of the cell buffer's own size, not of `ktui_w` and
`ktui_h`. A backend reports a new size the moment it is resized and the buffer is reallocated only
when the consumer calls the resize function, so between those two points the globals describe a
grid larger than the allocation.

### Colour

One rule governs reducing a colour that came from outside the palette. A terminal's SGR and a
picture's average tint both land on the nearest of the theme's slots by squared distance, through
one function here. A second implementation would drift, and a table saying "red means the error
slot" would be a second set of colour decisions beside the palette — one that would stop following
the accent, so `kdos theme amber` would move some colours and not others.

Night light is a transform over the slots, not a scheme. `ktui_theme_night()` warms whatever
`ktui_theme_set()` last loaded — green to 93%, blue to 77%, red untouched, the shape of a colour
temperature and the reason a warmed accent still reads as itself — and hands out a copy, keeping
the chosen scheme beside it: eight bits do not divide back, so turning it off returns to the table
rather than undoing the arithmetic. The caller reads the toggle, because this library holds no
opinion about where a desktop keeps its state, and the call returns whether the palette actually
moved so a caller can skip a repaint it does not owe. The warmed copy is one buffer, rewritten in
place, so its address stays the same when the scheme under it changes: anything caching work per
palette — the tty backend's SGR table is the one that does — compares the eight slot colours, never
the pointer.

### Announcements

A widget says what it is, because it already knows. Every widget computes which item has focus each
frame from the same id the hit test uses, so a reader working that out again from a grid of cells
would be guessing at what the surface has in hand — and it guesses wrong first on the controls that
matter most: which cell of a table, which tab of a strip, which item of how many.
`ktui_announce()` takes a role, a label, a value and the item's position in its set, so "3 of 9" is
a fact the widget states rather than a count somebody has to make.

- It lives here, not in a surface. A record composed per surface would be composed once per
  surface; one set in this library is set once and every consumer reads it.
- The queue is per frame, fixed, and cleared at the start of every frame. Nothing on the draw path
  allocates — a widget that allocated to say its own name would drop frames on the link this
  desktop is sold on — and a frame with more to say than the queue holds drops the rest.
- Silence is the failure mode, never a stale name. A widget that says nothing announces nothing; a
  reader told the wrong control is worse off than one told nothing, and last frame's record is the
  wrong control by default.
- A widget says what it knows and no more. A tab strip has its names and says them, and so does a
  menu, which holds its own rows; a list and a table take their rows from the caller's own
  callback, so they state the position and leave the name to a surface that has it. A secret field
  announces that it is one and never its contents.
- A position counts what a caret can reach. A menu's separators and its rows hidden by the scope
  rules are drawn and cannot be selected, so neither the ordinal nor the total counts them: a
  person hearing "3 of 4" can count to the same row.
- A repeated record is the same control. Dropping the repeat is the reader's job; the widget's job
  is to be right every frame.

### The cell's attribute byte

A cell's attribute byte carries six styles in its low half. Bold, reverse, underline, italic,
strikethrough and overline are one bit each in the byte the cell already had, so a terminal's own
text reaches a KDOS surface without the per-cell run growing for it. All of them but reverse are
dropped on a real VT, where an attribute bit selects a font page or a colour the palette does not
own rather than a style. A seventh low bit, `KT_A_GUEST`, is not a style at all: it says the cell
holds an embedded guest's pixels with the guest's own cursor already composited into them, and the
flush is its only reader.

Above the eighth bit nothing is part of the portable attribute. The attribute is sixteen bits wide
and the low byte is what a consumer drawing in slots alone honours; the high half says which of the
cell's three literal colours — foreground, background, underline — mean anything, and what shape
the underline is. A literal reaches a cell only from the terminal's render boundary and from the
theme picker's swatches, so a consumer reading slots alone cannot receive a cell claiming a colour
it never got and draw the black it was never given. There is one bit per colour, not one for the pair: a
program that sets a foreground and leaves the background alone is the common case, and a single bit
would freeze the theme's background into the cell as a literal, after which a retint leaves a
rectangle of the old scheme behind.

A literal reaches a frame only through `ktui_draw_put()`. Every other draw call takes slots and
clears the literals, because chrome that stopped following `kdos theme` would be a second palette
nobody can change. Terminal content is what uses it: the two places that copy a terminal's grid
into a frame copy whole cells.

### Two input queues

A *guest* here is another program's graphical output shown inside a surface's cells — an embedded
client whose pixels the surface displays and whose input it forwards. The *session* is the program
hosting the guest and deciding which input it keeps for itself (a desktop chord, for example) and
which it passes on; a *grabbed* guest is one that currently receives all pointer input. The raw
queue exists for that forwarding: a guest needs real key codes and pointer distances, not
characters and cells.

The same physical input travels twice. `poll_event` answers a character and a cell.
`KtuiBackend.poll_raw` answers a `KtuiRaw` — an evdev keycode with a separate press and release,
the xkb mask and group, a pointer in the backend's own pixels with the cell those pixels were
measured in and both deltas the device reported, and a scroll with a second axis, a `value120` and
what made it. `KtuiBackend.keymap` hands over the backend's compiled layout as xkb text with a
generation counter. A backend with no device of its own leaves both NULL, which is how a consumer
knows not to claim the capability at all.

There are two queues because motion coalesces and a key must not. A thousand-hertz mouse in the
cooked queue evicts the click that came before it, which is the oldest entry there. In the raw
queue consecutive bare motions merge into one at the newest position with their deltas summed — a
delta is a distance, and dropping one shortens the movement a grabbed guest sees — and nothing else
merges at all.

The cooked event for one physical input is sent before its raw partner, and `KtuiRaw.after` is how
a caller knows which. It counts the cooked events that must be taken before that raw one goes, so a
caller sends cooked events up to that number and only then sends the raw one; the field carries the
order, not the order the two queues were filled in. Draining one queue to empty and then the other
loses it: two keys inside one poll arrive as two cooked messages and then two raw ones, and a
session that swallowed the first key's chord has no way left to say which press it swallowed. A
handler may queue the switch before the character it resolves to, so a raw event's number is raised
by any cooked event queued before the caller drains it. The stamp is only ever raised, because one
delivered early is a chord the session ate and the guest saw.

## libkwm

The window model, and only the model. Placement, tiling and the workspace walk live here and
nowhere else, out of the compositor that obeys them — which is what lets every one of them be
asserted against a fixture with no display anywhere.

`kdos-comp` calls eight entry points: `kwm_place`, `kwm_tile_geom`, `kwm_tile_next`,
`kwm_ws_adjacent`, and the four pieces of the neighbour-edge search. Of that search only the
arithmetic is shared — `kwm_clip_add`, `kwm_clip_sub`, `kwm_edge_best` and `kwm_edge_check` —
while the walk that *finds* the candidate edges is the compositor's, across its scene graph. The
desktop icons in `kdos-shell` call a ninth, `kwm_drag_threshold`, which starts a drag once the
pointer leaves the cell it went down in; the threshold is in cells because every other geometry in
this model is.

Every entry point a consumer is meant to call has a caller in a shipped program;
`kwm_edge_between`, the between-test `kwm_edge_check` applies to each candidate, is exported only so the
self-test can pin it. A rule kept in this library that
nothing calls is a second answer to a question the compositor already answers, and the contract
cannot arbitrate between two copies when only one of them ships — so a rule with no caller belongs
in the one place that runs.

The library is handed rectangles and told what is being asked. What a window *is*, which output it
is on, whether a client accepted its size and whether it is maximised all stay with the caller,
which is what lets the compositor hand it rectangles and nothing else.

The contract is `testing/fixtures/wm/geometry.txt`. Each section of it cites the function and
lines of `kdos-comp` its rows were derived from, and the self-test replays the file rather than
asserting anything of its own. Adding a case means adding a row, under a section that cites where
the behaviour lives.

Three things that file pins, each of which reads as a bug and is not:

- A quarter snapped towards the edge it already occupies collapses to a half. The parallel component
  is then neither the inverse of the request nor absent, so no branch of the transition matches and
  the orthogonal component is discarded.
- The two halves of an axis come from different expressions — `(size + gap) / 2` and `(size - gap)
  / 2` — which is what puts a whole gap between two tiled windows rather than half a gap each. An
  odd dimension therefore gives the right or bottom half one extra pixel.
- Occupancy is an input, not a derivation. The compositor counts views that are not omnipresent;
  the panel counts windows that are not minimised, because the workspace protocol has active,
  urgent and hidden but no "there is something here". Two rules, two right answers, and this
  library picks neither.

There is no maths library here, the constraint `libkcolor` and `kcell_ascii.c` are already written
under. The placement grid's interval search compares doubles, which is plain arithmetic and calls
nothing.

## libkdisp

Which display server a surface reaches, decided in one place.

`kdos-shell` alone opens a surface from fifty-five call sites, and each then asks whether it should
close, resizes itself, or hides its panel. Branching on the server at every one of those is the
same decision written fifty-five times in one program and again in the next.

The consumer decides what it links. This library names no implementation and pulls in none; a
caller hands over the ones it compiled, in preference order, so a program that links no display
server still compiles:

```c
extern const KDispImpl kwl_impl;   /* libkwl — the Wayland compositor */
static const KDispImpl *const have[] = { &kwl_impl };
kdisp_init(&cfg, have, 1);
```

Each program states that list once, and it is the single line that changes when a second server is
added.

### Fonts

The screen's font is a display's to answer, not a surface's to load. A surface draws cells and
something else turns them into pixels, so `kdisp_font_count` / `_at` / `_current` / `_set` take an
index into the display's list and never a name — no fontconfig syntax crosses to a display that has
never seen it.

A display lists only faces it can wear. A cell grid gives every glyph one advance, so `libkwl`
lists fontconfig's monospaced families — `FC_SPACING == FC_MONO`, deduplicated by family — and
nothing else, because a proportional family offered here is a screen of smeared columns.

A count of 0 is a display with no face to offer: a machine carrying no monospace family, or a run
with no display installed at all, where `libktui`'s terminal backend draws and the font is the
terminal's. A current index of −1 is not an error — it says the screen is wearing something no row
names, which is what an alias like `monospace` and a `chrome_font` naming a face this machine does
not have both come to.

`keep` is whether a choice survives the logout, so a picker's arrows pass 0 and only its `Enter`
passes 1. Each surface is its own process with its own face, so `keep` is the only thing that
reaches the others, and it reaches them at their next start.

A caller re-reads the list each turn, the rule the window list keeps. `kdisp_font_ask()` starts a
gathering, and a backend that collects its faces over a socket answers some pumps later, so a
caller that believed the first answer would draw an empty list for ever. A backend whose list is
local leaves `font_ask` NULL — fontconfig answers `libkwl` in this process, so its first
`font_count` is already the whole list — and an empty slot rather than a stub is the point: a slot
that did nothing would read as one that had started something.

A server that cannot answer an entry leaves it NULL, and the forwarder returns the neutral answer
rather than crashing. A `--tty` run has no server-side decoration to report and nothing to hand out
in place of a Wayland handle. `kwl_display()` is deliberately *not* in the vtable for that reason:
it hands out the Wayland connection itself, for a program such as `kdos-shell` that binds protocols
of its own on it, and nothing else can stand in for one.

### Somebody else's windows

Five vtable entries cover them, and they are asked for. `win_count`, `win_at`, `win_activate`,
`win_close` and `win_set_state` are what a panel, a task switcher and a window menu need — enough to
draw a row and act on the one that was clicked. A caller reaches them as `kdisp_win_count()`,
`kdisp_win_at()`, `kdisp_win_activate()`, `kdisp_win_close()`, and `kdisp_win_minimise()`,
`kdisp_win_maximise()` and `kdisp_win_fullscreen()`, which all go through `win_set_state`.
`kdisp_win_supported()` says whether the display offers a window list at all, which is how a caller
tells a desktop with no windows open from a display that cannot say.

`manage` on the configuration is a declaration and not a gate. A compositor hands the list and the
verbs to whatever binds `wlr-foreign-toplevel-management` and cannot tell one client from another,
so what keeps a launcher out of somebody's editor is never calling `kdisp_win_*` and nothing else.

An id crosses the interface, never a handle. A caller draws a list in one frame and acts on a row
in a later one, and a stale handle is a request to a destroyed proxy that kills the connection.

Announcement order is list order, and a row is only offered once it is settled. A panel draws task
N at position N, so a removal closes the gap rather than filling it from the end — an unordered
swap teleports the last button into the middle of the bar. A handle whose property batch has not
been closed by `done` is left out of both the count and the walk, compacted rather than holed,
because a hole would hide every settled window behind it.

The Wayland side binds foreign-toplevel on first use, not at start-up. A compositor announces every
window to whoever binds it, and a terminal, a lock screen and a resource monitor all link this
library and want none of that traffic.

### Two edge vocabularies

`KDISP_EDGE_*` is a sequence naming which edge a panel is anchored to. `KWM_EDGE_*` is a bitmask
whose values match the compositor's own enum, so that corners are combinations. They are different
questions and must not be conflated.

Nothing points back up.

## libkxdg

Desktop entries, the MIME glob table, and the command-line splitter.

The splitter is the single implementation of turning a desktop entry's command into an argument
vector: it unquotes, substitutes the file-argument codes, drops the codes that carry no argument,
and — with a negative count — keeps every code verbatim for a tool that *rewrites* a line rather
than running one. Its inverse re-quotes, so a generator's output round-trips. Every launch path in
the system goes through it. See [kdos-appbox](../04-programs/kdos-appbox.md#exec-lines).

`kxdg_launch_read()` is the single reader of the keys that decide *how* to start something:
`Exec`, `Name`, `Terminal`, `X-KDOS-Term`, `X-KDOS-Size` and `X-KDOS-Float`. Four programs would
otherwise each hold a private copy of that list — `kdos-shell`'s application index, its desktop
icons, its *Open With* chooser and `kdos-appbox open` — and a key added to one of them is a row
that behaves differently depending on which surface it was clicked from. It is the "can this entry
be started at all" test as well, so a caller's check and its read are one call.

`NoDisplay` and `Hidden` are deliberately not among them. They say whether an entry belongs in a
*menu*, which is a question for whoever is drawing one: the MIME route opens a `NoDisplay` entry on
purpose, and a shared reader that refused one would break every default handler that carries it.

The `Exec` line is copied with its field codes intact. The splitter spends them on the documents a
launch carries, and reads the same codes to decide that a line carrying none takes its documents
appended instead — so a reader that stripped them here would make every entry look like
`Exec=xterm` and open a file in the wrong argument.

### Typing an argument

`kxdg_mime_for_arg()` says what a command-line argument is, and it is one implementation because
two openers answering that question separately get a URL wrong in two different ways.

An argument carrying a scheme is typed `x-scheme-handler/<scheme>`; `file:` names a path, so the
path is unwrapped and typed like any other; a name that `stat()`s is a path whatever it looks like.
Deciding on the basename instead matches `mailto:a@b.c` against the `*.C` glob —
case-insensitively — and resolves it to C++ source.

It returns the argument the caller must pass on, read-only and never a copy, so a long path cannot
be silently truncated on the way to the handler. The pointer is into the argument, except for a
`file:` URL naming no path at all, where it is a static `/`. Percent-escapes are not decoded, and a
reader sees that: `file:///home/kdos/My%20Report.pdf`, which is what a conforming caller emits for
a name with a space, resolves to a path that does not exist and opens nothing.

### Places, recents and verbs

`kxdg_places()` is one reader for the whole desktop. The desktop folder, the Places menu, the
chooser's `Ctrl+P` list and *Add to Places* all resolve through the same call, because two readers
disagreeing about where a user directory is puts icons in the folder one names and opens an empty
one beside it — which reads as a broken menu rather than as two readers.

There is no `xdg-user-dirs` on this system. KDOS seeds `user-dirs.dirs` from `/etc/skel` and it is
the user's to edit. `$HOME` is the only expansion the reader understands, because it is the only
one that file's format defines; a reader that guessed at the rest would be a shell.

A place that is not there is not a place. The user directories are created on demand, and every row
is checked before it is returned: a row that opens an error is worse than a row that is not
offered.

`kxdg_recent_add()` is written from one place, `kdos-appbox open` — the function every open on this
desktop passes through, so a recent-files store can be kept without a second copy of the rule going
stale beside it. It is the same scanner run backwards: the bookmark this URI already had is cut out
whole and a fresh one appended, so a file opened twice is one entry and it is the newest. The
oldest past the cap are dropped on the same pass, because nothing else on this system prunes
`recently-used.xbel`, and the rewrite is temp-and-rename because the store is shared with every
other program on the machine that keeps recents.

The read half memoizes the parse on the store's size and mtime, and never the existence check. Find
rebuilds its rows on every keystroke, so the whole-file read and the backward walk are worth
keeping; the deleted-file filter is not cacheable, because nothing about the store changes when a
file is removed and a cached row would go on offering a destination that opens nothing. A memo hit
therefore re-runs `access()` over its rows, and keeps the rows that failed so a file that comes
back returns to the list.

`kxdg_verb_*` is one table of what can be done to a file, read by the desktop's icons, the file
chooser and — in the one form a text file allows — `mc`'s `F2`. Three tables would mean a verb
landing on one surface and not the others, which reads as a surface being incomplete rather than as
three lists. A row whose program is absent is not offered, so a verb still being built turns on
when it ships with no edit to any caller. The resolution is repeated at most once a second per
verb, so a program installed under a live surface turns its verb on within a second while a menu
redrawing its rows several times a frame does not walk `$PATH` per row. Every verb builds an
argument vector, never a command line.

## libkpkg

The package database, the ports tree, the solver, version comparison, and the two hashes.

Version comparison (`kp_vercmp`) and the version-shape filter (`kp_vershape`) live here rather than
in either consumer, because the package manager and the upstream version checker ask the same
questions about the same strings and two implementations would eventually disagree. Both carry
libkpkg's `kp_` prefix even though `kdos-portup` is the shape filter's only caller: a symbol
exported under a consumer's prefix is one the next reader hunts for in the wrong library, and two
libraries that each define a generic name cannot be linked into one program.

The hashes and their three states are in
[Packaging](../03-architecture/packaging.md#deciding-what-to-rebuild).

## libksig

Signing and verification, and the one library with vendored third-party source.

The vendored implementation is Monocypher 4.0.3: four files of public-domain C with no
dependencies, no maths library and no allocation — which is exactly the rule this set is built
under. The alternatives were each disqualified: a shared library, a library with no signing
support, an unmaintained one, and one that is the opposite of "links nothing but the C library".

It sits under its own subdirectory with its version and checksum recorded in `UPSTREAM`.
Everything above it — file formats, the keyring, the policy — is ours.

## libkbuild

The deciding half of the build orchestrator. Covered in
[The build system](build-system.md#libkbuild).

## libkproc

Every reading about the running machine, from a root that can be moved.

That movable root is the single most valuable property in this library: it is what makes the
resource monitor, the stutter attribution (`kdos stutter`), the memory daemon and the energy daemon
testable against recorded system state. A tool whose readings cannot be replayed cannot be tested
at all.

Elapsed time may only be computed against the system uptime. Both a process's start time and the
uptime are seconds since boot; pairing the start time with a monotonic timestamp is a different
epoch — and under a fixture, a different machine.

The box identity walk (`kpr_box_of()`, `kpr_box_of_pid()`) turns a process id into a box name by
walking the parent chain up to podman's per-container supervisor, `conmon`, and reading its command
line. It is used by four separate programs — `kdos-shell`, `kdos stutter`, `kdos-oomd` and
`kdos-energyd` — which is why it is here rather than in any of them, with one climb limit for all
of them.

Graphics readings come from the kernel's DRM interface (`kpr_drm_list()`). Only amdgpu and NVIDIA's
management library publish a utilisation figure; everywhere else `busy_percent` is −1 and a
renderer must show engine time and label it as such. The NVIDIA library is opened with `dlopen` at
run time when `libnvidia-ml.so.1` is present, which it is not on a stock KDOS, so it adds no link
dependency.

A reading goes into a caller buffer wherever a buffer bounds the file. A process walk on a busy
machine opens a few thousand files a tick, inside a draw loop, and a whole-file slurp pays an
fstat, a heap block and an EOF-confirming second read for each of them — three syscalls and an
allocation to carry a few hundred bytes. `kpr_read_into_proc()` and `kpr_read_into_sys()` are the
form to reach for. The slurps stay for `/proc/cpuinfo`, `/proc/stat` and a command line, none of
which any buffer bounds: a full buffer means the content *may* be cut, and a cut value parsed as
though it were whole is a wrong reading rather than a missing one, so that case is re-read on the
heap.

What cannot change is latched, and every latch is keyed on `kpr_root_gen()`. A CPU topology, a
model string, a disk's rotational flag: constants that cost two files per logical CPU and a 26 KB
`/proc/cpuinfo` to rederive on a tick that only wanted the busy figure. Moving the root is moving
to a different machine, so a cache that does not hold the generation it was filled at describes
this host's hardware under a recorded one.

What is *not* latched is as deliberate: a device's size, because an optical drive keeps its name
across a disc change and an LVM volume across a resize; and the existence of a sensor, because a
hwmon chip appears when its module loads. That last is also why no reader of `hwmon` or `thermal`
probes an index range — the index comes from one system-wide counter and says nothing about how
many chips there are, so `kpr_sysfs_indices()` reads the directory and every reader shares the
answer.

`KprCpu.ncpu` is the highest CPU number plus one, not the count of CPUs running. `/proc/stat` lists
only the online CPUs and keeps their real numbers, so a machine with `cpu1` offline has arrays four
long and `online[1]` clear. A per-core chart walks `ncpu` and skips the clear slots, which draws
the gap honestly; sizing by the number of lines instead drops the highest CPU's times altogether
and leaves a phantom core reading zero for ever. Anything that wants the *count* — rescaling a
per-core percentage to percent-of-machine, say — calls `kpr_cpu_online()`, because a CPU taken
offline below the highest index leaves `ncpu` where it was.

`libkproc` links `libkbase` and nothing else, and that is what lets a root daemon take it. Two root
daemons do — `kdos-oomd` and `kdos-energyd` — and every library they link is code running as
root.

## libkpack

The pack format. It links the base, signing and package libraries and nothing else, so a root
daemon — `kdos-packd` — can take it.

The rules it exists to keep are stated in
[Packs and boxes](../03-architecture/packs-and-boxes.md#rules-the-format-keeps): a pack that does
not parse whole is absent rather than partial, each section has its own size limit, the signature
block ends exactly where the footer begins, and the payload hash is checked before the signature
means anything. The format number in the footer says which byte span the payload digest covers
(`KPK_FORMAT_MIN` to `KPK_FORMAT`, both read). Changing that span without raising the format makes
every released pack fail its hash check and stop mounting. Nothing in this library mounts or executes a pack; that is the daemon's job.

The solve takes an array of pointers. A pack's metadata structure is large, and an array of them is
not something a function puts on its stack.

## libkvt

The terminal as a state machine — a hard fork of libtsm 4.7.1, kmscon's own, pinned rather than
tracked. What a consumer touches is `struct kvt_term`: a screen, a state machine and a child on a
pty as one object, with a descriptor to poll and a grid to draw.

The child dies with the window and is collected there. `kvt_term_close()` closes the master — which
hangs up the pty's foreground group — signals the child `SIGHUP`, waits up to 100 ms for it, and
`SIGKILL`s and blocks on whatever is left. Nothing else can do it: the non-blocking reap runs from
`kvt_term_pump()` and there is no pump after a close, so a child left behind is a zombie for the
life of the session and a child that ignores `SIGHUP` is a program still running with no terminal.
The consumer needs no `SIGCHLD` handler.

The bytes a key produces are decided in here, never by the caller. The escape an arrow sends
depends on application cursor mode, on keypad mode and on the modifier encoding, and all three are
state machine state. A caller hands over a libktui key and modifier set; `kvt_term_key` turns it
into a keysym and lets the machine answer. `kdos-term` goes through it, and so must any other
terminal built here, so that there is one implementation rather than two that drift.

### Links and prompts

A hyperlink is a 16-bit id on the screen's own cell, and the address is interned. `OSC 8` names an
address for a run of text; the cell keeps an id into a per-terminal table, so the text scrolls into
the scrollback still knowing what it points at, and two runs of the same address are one link. The
table is capped and is *not* freed on a reset, which is what makes an id in the scrollback safe to
resolve for ever. `KtuiCell` is deliberately not widened for it: a link is a property of a
terminal's buffer, not of every surface the toolkit draws. `kvt_ui_mouse()`'s coordinates are the
terminal's own grid for the same reason a link lookup's are — a caller whose terminal is a window
subtracts its origin, or both the selection and the link land as far from the pointer as the window
is from the corner.

A prompt mark is on the line, and the exit status is walked back to. `OSC 133` says where a prompt
starts and what the command typed at it exited with; the mark rides the line so it survives into
the scrollback as one thing, where a mark per cell would be eighty copies of one fact and a mark
kept beside the screen would be lost the moment the line scrolled off. The status is walked back to
the nearest marked line at or above the cursor rather than remembered in a pointer, because a
pointer to a line kept across a scroll is a pointer to a line the screen may have recycled.

### The render boundary

The render boundary is where an attribute becomes a cell, and what it cannot carry it drops. Bold,
underline, inverse, italic, strikethrough and overline each have a bit and each is drawn by the
pixel painter; `blink` and `dim` are parsed and reach none. That is deliberate: a blink drawn as
bold and a dim drawn as normal are both a lie about the text, and the cell is the one place that
can say so rather than approximate.

A picture is written into the screen as sprite cells. `kvt_term_place` names tiles the caller
already registered in libktui's table and writes their codepoints at the cursor — in the screen
rather than in an overlay beside it, because that is what makes a picture scroll with its output,
clear with it and reach the scrollback, three behaviours an overlay would have to reimplement
against a screen already doing all three. A tile the table has since dropped becomes a blank, and
the cells carry the state machine's own default attribute, so a fallback is drawn in the terminal's
text on the terminal's background and follows `OSC 10`/`OSC 11` and the installed palette. A
literal colour index there would reduce to whatever theme slot is nearest its RGB, which for a
foreground index and a background index can be the same slot — a blank drawn on itself.

This library decodes nothing. The three image protocols are delimited by one collector — they
differ only in how they are framed — and the payload goes to a callback the consumer set. That is
what keeps this library free of image decoders, and it has to stay free of them because a consumer
that links no pixel code at all still links this.

The colour a cell reduces to is cached, and the cache is indexed by the high bits of its
multiplicative hash — the same rule `libkchrome`'s literal table keeps, for the same reason: the
low bits of a product carry only the low bits of blue, so a low-bit index lands the whole
216-colour xterm cube in six buckets and a 256-colour program misses on nearly every cell. The
cache is thrown away when the palette in force changes value, which is the only thing that can move
an answer, and the palette is compared by value because night light rewrites one struct in place.

A CSI final with a private marker or an intermediate is not the plain sequence. `r` is DECSTBM only
as `CSI Pt;Pb r`; `CSI ? Pm r` is XTRESTORE and `CSI ... $ r` is DECCARA, and either taken for
DECSTBM rewrites the scroll region and homes the cursor under a program that only asked for its
modes back. `m` is guarded the same way. A final that means different things under different
markers tests `csi_flags`, never the byte alone.

A word selection's end is bounded by the screen, not only by the line. A line only ever grows, so
it keeps the widest the terminal has ever been; after a shrink a word crossing the old right edge
would set `sel_end.x` past `size_x`, the renderer would never reach the index that turns the
highlight off, and every row below it would draw inverted. The copy takes the same bound, so the
text that comes back is the text that was lit.

## libkimg

The only place untrusted image bytes are decoded, and reachable by anything that can write to a
terminal.

Two entry points: one picture, or every frame of the one format that has more than one. Five
decoders are optional — PNG, JPEG, WebP, sixel and GIF — and the byte budget is enforced from the
header the format declares, before any allocation.

GIF goes through libnsgif's `nsgif_*` interface, and the byte stream is walked for its block
structure before the library scans it. That walk is what refuses a stream that ends before its
trailer — the library alone reports a GIF cut off after a whole frame as a success with the frames
that arrived — and what caps the file at 4097 frames, because the library keeps a record for every
image it scans and the budget is charged only for frames actually decoded. Each frame returned is
the whole canvas with disposal and transparency already applied, read out of libnsgif's bitmap as
byte-order RGBA. The canvas is the logical screen grown to cover the first frame only, so a later
frame that reaches past it is clipped; a logical screen of zero, of one of the common monitor
sizes such as 640x480, or of more than 2048 pixels on a side is taken as unset, and the first
frame's extent becomes the canvas. The delay after it is the file's own centiseconds times ten, with a delay of zero
read as 100 ms; a frame with no graphic control extension carries libnsgif's default of ten
centiseconds. The file's loop count is not returned.

A decoder must answer NULL or an image for any bytes at all, and must never read past the end of
them. `testing/fixtures/img/fuzz.c` is the committed check on both.

## libkchrome

The window furniture: the header band, group headings, the button bar, the list and wheel rule, the
scrollbar and its drag, and the pixel tile.

It exists so that there is one implementation of each. Two button bars are two button bars, and the
one nobody is looking at is the one that drifts. The rules it enforces are in
[the design language](../03-architecture/design-language.md#the-chrome-primitives).

A tile is cut to the display's pixel cell, because that is the one the caller laid its contents out
in — a canvas cut to anything else clips the word or mis-centres the mark. Where the display has no
pixels of its own the tile is cut to the nominal cell instead: such a display answers a cell of one
and whatever presents the sprite rescales it, which is the same rule `libkicon` keeps for icons. A
change of cell size or output scale is a resize; both canvases go and are cut again.

A refused sprite put keeps the picture that is up. The table refuses on a full table or a spent
byte budget, and a frame of the previous picture is better than a flash back to the glyph layout
mid-hover — so a tile remembers what it *published* apart from what it last *tried*, or "already
showing this" would freeze it at a picture it claims is current. Attempts are capped per content
hash: an unrelenting refusal must not turn every frame into a full canvas raster, which is the
memory pressure being survived. The cap is rearmed a second after the last refusal, because what
refuses a put is transient while a tile's content hash — the Start button's is the label, the hover
and the cell size — need never change again, and a spent cap that never comes back leaves that
button on its glyph layout for the rest of the session.

The pixel display list is recorded while a surface draws and replayed by the backdrop `libkwl`
paints under its cells. Whether the plates moved is a question asked at flush time, not a flag set
while recording. A frame is committed on a cell diff and a plate is not a cell, so a highlight
following the pointer down a menu would never reach the screen; but a surface re-records its whole
list on every draw, so a flag set from that cannot tell a change from a redescription and would
make every draw of a backdrop surface a full-surface upload. Installing a backdrop registers a
callback `libkwl` asks once the list is complete, and the answer is the key of what is recorded now
against the key of the picture last painted.

`kch_px_live()` is the one answer to whether a recorded op can reach a screen. The list is replayed
by a backdrop and a backdrop is painted by `libkwl`, so it reaches a screen only where one is
installed — which a `--dump` run never does. The cell size cannot stand in for the test: it is
asked of `libkwl` either way and `libkwl` answers with a fallback rather than with nothing. A
control whose only state cue is a plate has to draw the cell form of the same fact where this is
false.

## libkicon

One job: a name, or a file path, becomes a sprite slot — or −1.

Minus one is not a failure. It is a terminal, an install with no artwork, icons switched off, and a
name nothing on this machine has a picture for. Every caller draws its glyph tier then, exactly as
it would without this library, which is the rule the whole icon layer is built under.

A cell under 4x4 pixels is not a pixel backend, and `kicon_init()` refuses it. A backend with no
pixels of its own answers a cell of one, and rasterising at that decodes, tints and rescales a PNG
per name to produce a picture a pixel across, which is a blank cell reached the long way. Refused
means `kicon_enabled()` stays false and every lookup answers −1, so the caller draws its glyph. A
consumer that ships pictures over a wire at a nominal cell size passes that size instead of the
backend's, which is how such a consumer keeps its icons.

The KDOS theme's own icons come from one memory-mapped atlas file holding every icon at every
size it was rasterised at, sorted by name and size so a lookup is a binary search and the pager
reads only the pages that are drawn. Every header field, directory entry and blob extent is checked
against the mapped length before use, so a damaged atlas is absent rather than partial. The
atlas is tinted into the accent like every other piece of KDOS artwork; an application's own icon,
found as a PNG under `/usr/share/icons/hicolor`, is drawn untinted. The split is by where the
picture came from, never by guessing from its name.

A decoded picture outlives the sprite slot naming it. The table gives slots back under its byte
budget, so a lookup that misses it re-registers the picture this library still holds rather than
paying a PNG decode, a tint of every pixel and a bilinear rescale to rebuild something already in
memory. The pictures free their own pixels on the last unref and this library keeps a reference
across the handoff, which is what makes an eviction by somebody else's evictor safe.

A path is memoised as a slot, so whatever frees the slots drops the memo.
`kicon_slot_for_path()` remembers the sprite slot a file's type resolved to — the lookup is a stat
and a walk of the glob table, asked once per drawn row per frame otherwise — and a memo outliving
its slot either draws nothing or draws whichever picture reclaims the index next. The consumer
clears it when it re-reads the directory it is drawing, and `kicon_retint()` clears it too, because
a retint hands back every slot the memo can name. The name-miss and app-id tables are not cleared
by a retint: neither answer depends on the accent, and both live until the program is next started.

The dump harness stubs this library to exactly "no picture", so a committed reference frame is the
character grid.

## libkcell

The glyph cache and the cell painter: a grid of cells into a pixel buffer, the character ramp built
from it, and the pixel canvas a block of cells can be drawn as.

### Synthesised frame characters

The frame characters are drawn, not rasterised. U+2500's single and double box sets and the whole
of `U+2580`–`U+259F` — the full block, the eighths, the halves, the quadrants and the three shades
— are painted as pixman rectangles derived from the cell, in the cell's own foreground, and the
face is never asked for them.

Rasterising them is exact only while the face's box glyphs are drawn to precisely the advance the
cell was measured from. A face whose full block spans a hair more than its advance leaves a
hairline between two cells at some pixel sizes, and a face drawing its box glyphs to another metric
dashes a border outright.

Nothing selects the synthesis — not an attribute, not a caller, not a tier — so the same character
is the same picture in the panel, the terminal, the installer and the build screen, under every
face and at every size.

The rule is `max(1, cell_h / 16)` thick, capped at a third of the shorter side, and a double rule
is that stroke twice with one stroke of gap; the single rule sits exactly between the double's
pair, which is what makes `├` meet `─` and `╪` meet `║` with no step. Every block edge is
`floor(span * k / 8)` from the cell's top or left, so an eighth, a half and a quadrant in
neighbouring cells share a pixel row and a pixel column — rounding each shape from its own fraction
is what puts a seam down a bar chart. A synthesised character is one cell wide and puts no ink
outside its own cell, which is what the damage report and the wide-glyph clip both assume; every
rectangle is clamped to the cell after it is computed, so a cell too small to hold three strokes
draws a thinner line rather than one that leaves the cell.

They are not cached as masks. A cached mask is composited OVER, per pixel, through a solid source,
where a fill of at most eight rectangles issued in one call is cheaper than the composite it
replaces — a cache would spend memory to make the draw slower. The three shades are the exception,
because a quarter-tone dither at a 16x32 cell is 128 disjoint pixels: they are one repeating `a8`
tile per tone per scale, four by two cell pixels across, offset by the cell's absolute position so
one pattern runs unbroken across a whole shaded area instead of changing phase at every cell
boundary.

The synthesised set is exactly what the VT tier's font carries, so a box character is the same
picture on a screen of its own as it is on `tty1` and the two tiers cannot drift apart. That
includes the two mixed single/double junctions `╪` and `╬`, which the VT font has. The heavy,
dashed and rounded variants are not in it and still reach whatever face carries them.

### Styles and companion faces

A cell's style is drawn here, and two of them need a second face. Underline, strike and overline
are one horizontal rule each, differing only in the row they land on and drawn after the glyph so a
descender crossing a strike is cut by it; a broken rule — curly, dotted, dashed — is built as an
array of rectangles and issued as one pixman fill, because a fill call is a region intersect before
a pixel moves and a curl is dozens of pieces.

Italic and bold each ask fontconfig for the loaded name with `:slant=italic` or `:weight=bold`, and
the answer is kept only if its advance and height match the upright face. fontconfig never fails a
match, so asking for an italic Terminus returns a different family at a different size, and a
companion that disagrees would draw a row out of step with the one above it. There are four faces,
indexed by the two style bits, and the face is part of the glyph cache's key, because the same
codepoint from two faces is two glyphs.

Where a companion is missing the style is synthesised, and the two are synthesised differently.
Bold with no bold face is the same mask struck twice one scaled pixel apart, so it shares the
upright glyph's cache slot — the weight belongs to the blit. Italic with no italic face is a shear
and therefore a different mask: the upright coverage is leaned by 7/32, a little over twelve
degrees, into a slot of its own, because a shear widens the box and moves the bearing and neither
can be done at the blit.

The shear is about the mask's vertical middle and not about the baseline, and it moves by whole
pixels. The painter clips a glyph to its own cell, so a baseline shear — which leans the whole
letter to the right — would cut the top off every tall one; shearing about the middle spends half
the displacement on each side, and what is still lost is a pixel at each extreme of a glyph that
already fills its cell. Whole pixels, because a fractional shift needs the mask resampled, and an
alpha mask resampled at a terminal's size is a blur rather than a slant. A glyph under six pixels
tall leans by nothing at all and keeps the upright mask, which is the one case where the style is
still lost.

### Missing codepoints

A codepoint is missing only when it matches the sentinel. fcft cannot report an absent codepoint:
its fallback search ends by rasterising glyph index 0 out of the primary face, so what comes back
for a character no font carries is a valid glyph — a tofu box, or a PCF's default character — and
`NULL` means a FreeType load error and nothing else.

The load therefore rasterises a permanent Unicode noncharacter, which nothing can carry, and keeps
its metrics and its pixels; `kcell_has()` reports missing when a glyph is that same picture at
those same metrics.

The pictures are compared row by row over the meaningful bytes only. A pixman image's stride is
rounded up past the glyph's width and the rasteriser writes only each row's own bytes, so a
whole-buffer compare reads uninitialised heap and reports two copies of the same `.notdef` as
different pictures — which fails open and reports every absent codepoint present. The ascii ramp's
candidate filter depends on that answer being real, and anything else drawing a fixed glyph set
should ask once at load time rather than discover the gap on somebody's screen.

### Font lifetime

A font load replaces everything measured against the old one. `kcell_font_load()` may be called
repeatedly and tears the previous faces down first, taking the glyph cache, the ascii candidate
table and the tiling scratch with them — so every `KCellGlyph` handed out before it, and every
cached `kcell_w()`/`kcell_h()`/`kcell_ascent()`, is invalid once it returns. `kcell_font_free()` is
that same teardown plus fcft's own, and is not a step in a font change: fcft's teardown is not
refcounted, so a free is a shutdown of the library.

fcft is reference-counted inside `libkcell`, so the two entry points are free of each other and of
any ordering. `fcft_from_name()` answers NULL for every request until `fcft_init()` has run, so a
consumer that draws canvases and never calls `kcell_font_load()` would measure every string as zero
and draw none of them, with the library's own complaint going to a stderr nothing reads. And
neither of fcft's own calls is idempotent: a second `fcft_init()` replaces FreeType's handle and
orphans every face resolved through the old one, and `fcft_fini()` destroys FreeType whether or not
anything still wants it.

`kcell_font.c` therefore owns fcft for the whole library and counts its holders, declaring the pair
in `kcell_priv.h`. The cell font takes one reference and the canvas takes one of its own, the
library comes up on the first and goes down on the last, and either may be used first, in either
order, with neither tearing fcft down under the other. A canvas drawing through a
`kcell_font_free()` keeps its faces, and a failed `kcell_font_load()` gives its reference back
before it returns −1 — a caller reading that as "no cell font" has nothing left to free. The pair
is private: a reference taken outside `libkcell` matches no face inside it, so nothing could ever
release it.

### Repainting

The changed-span repaint steps back onto a wide glyph's lead. A row is repainted only between its
first and last changed cell, widened one cell each way because a cell's pixels are not always its
own. That is not enough on its own: a double-width glyph is painted entirely by its lead, so a span
that began on the `KTUI_WIDE_CONT` marker beside it would fill the marker's pixels — erasing the
right half of the character — and then find nothing to redraw there. The span therefore takes one
more step left when it starts on a continuation cell.

A run of one picture's cells composites in one call. A sprite cell names a block and a sub-cell
coordinate inside it, so cells that are consecutive columns of the same block on the same row are
one contiguous rectangle of one image and go out as a single `pixman_image_composite32`. pixman
charges most of a small composite to its setup — choosing a combiner, building the iterators,
walking the clip — and an 8x16 cell is small enough that the setup is the whole cost: a 1080p
window of blocks is 16,080 cell-sized calls painted one at a time and 1,005 row-sized ones painted
in runs, measured at 4.90 ms and 1.72 ms a frame against pixman 0.46.4 under musl.

The run breaks on anything unusual and the per-cell path draws it: a different block, a sprite row
that does not advance a column at a time, a glyph, the end of the changed span, and `KT_A_REVERSE`
— which is the fill the pointer puts under the cell it is over, and is therefore a cell that has to
be drawn on its own.

The cached foreground sources are keyed on the colours, not on the theme's address. A glyph is
composited through a solid-fill image and those are kept per slot and per literal colour; libktui
projects night light by rewriting one table in place, so a cache keyed on the table's identity
would keep painting the old scheme's ink after a retint while backgrounds and rules, which read the
palette fresh, came up in the new one. The literal table is indexed by the high bits of its
multiplicative hash, since the low bits of a product carry only the low bits of blue and would land
the whole xterm colour cube in six buckets.

The canvas is what makes a pixel tile possible without a second renderer: a pixel image exactly
some number of cells across, with fills and text at an arbitrary pixel size, handed to the toolkit
as a sprite. See [kdos-shell](../04-programs/kdos-shell.md#the-start-button).

## libkwl

The toolkit's Wayland backend, and the library with the most external dependencies: the Wayland
client library, xkbcommon, fontconfig, fcft and pixman. It also carries touch input: `wl_touch`
feeds `libktui`'s gesture recogniser, which turns a touch into a gesture and into the ordinary mouse
events every widget already handles.

Two kinds of rule live here. The first set is what a surface author relies on or must do; each is
written up from the author's side in [Writing desktop software](writing-desktop-software.md). The
second set is for anyone changing `libkwl` itself.

Three terms recur below. A *stash* is the saved frame a throttled commit leaves behind, published by
the next frame callback. A *backdrop* is the pixel display list a surface records while it draws,
which `libkwl` paints under its cells (see [libkchrome](#libkchrome)); its pieces are *plates*. A
*pixel guest* is an embedded client's pixels shown in cells, as defined under
[Two input queues](#two-input-queues).

### What a surface author relies on

- A toplevel must ask for its frame, or it gets no decoration at all.
- Bind the layer shell at the version whose keyboard mode you want, or an older resource answers
  on-demand with exclusive and the surface holds the seat's keyboard against every window.
- A Ctrl chord is the letter plus `KT_MOD_CTRL`, never the control code xkb folds it into — the tty
  decoder delivers the letter, and a chord table has one vocabulary.
- The cell painter leaves a clip on the image it was handed, so anything drawn into that same image
  afterwards — the panel's frame rule — must drop the clip first or pixman writes nothing.
- A surface with a backdrop installs `kwl_set_pixels_dirty_fn()` so `libkwl` can ask at flush time
  whether its pixels moved. A latched dirty flag would not do: a backdrop redescribes the same
  plates on every draw and cannot tell a change from a redescription, so latching from the
  description makes every frame a commit and removes the unchanged-frame gate (the check that skips
  committing a frame identical to the last) for every surface that has a backdrop.
- `kwl_font_step()` owns its own copy of the font name: `KDispConfig.font` is the caller's pointer,
  and the surface outlives whatever the caller built it in.
- `kwl_init` ignores `SIGPIPE` process-wide, because every clipboard and drag payload is written
  into a descriptor the receiver owns and the default disposition kills the surface when that
  receiver closes early. `SIG_IGN` survives `execve`, so any consumer that forks and execs must
  reset dispositions in the child or the whole launched tree inherits it.

### Rules for anyone changing libkwl

Each guards a distinct failure.

- The event queue is a ring, not one slot, because a batch of callbacks arrives from a single read.
- Key repeat is the client's job.
- A wheel tick is not an axis event, and a wheel is already quantised while a touchpad is not.
- One pointer frame is one detent on the discrete path.
- A motion that did not move is not a motion.
- A double buffer needs a shadow per buffer.
- A serial must be retained, or the clipboard has nothing to present.
- An enter carries coordinates and they are not optional.
- The scale and the resized buffer must land in one commit.
- A data source is destroyed on cancellation, never at set time, or the copy silently does nothing.
- The clipboard and the primary selection are separate stores, and an in-flight send keeps the
  payload it started with — a terminal sets the primary selection on every mouse release, and one
  shared payload splices that text into a clipboard send that is still draining.
- A parked send is polled for writability and its deadline counts from the last byte that moved.
  Anything past one pipe buffer parks, and a budget counted from the first write closes the pipe on
  a receiver that is still reading — which it cannot tell from a clean EOF, so it accepts a
  truncated selection.
- A throttled frame counts as presented, because the stash carries its own `full` and is committed
  by the frame callback that follows. Answering otherwise makes the toolkit re-arm a full repaint
  that never clears, on every surface that draws at the display's rate.
- A surface is clamped against the output's logical, upright box — the mode divided by that
  output's scale, with the axes exchanged on a 90 or 270 degree transform — and never against a
  slot whose proxy is gone. Each omission lets a popup be sized for a screen that is not there.
- A stash is content for the grid it was taken from, so every resize drops it; publishing it paints
  the old layout into the new geometry.
- A compose table that fails to build is absent, never partial.
- A font reload spoils every paint baseline, because it rewrites no cell. `kwl_font_step()` changes
  what a cell *looks* like and not what it says, so the damage diff, both buffer shadows and the
  unchanged-frame gate would all find nothing to do while every glyph on the screen is drawn at the
  old size.
- A lock surface must not receive the pre-configure commit, which is a protocol error there.
- A drop's offer is owned apart from the drag's, because the leave that follows a drop arrives
  while the payload is still draining and a second drag may enter before it ends. One slot for both
  loses the first offer and destroys the second while it is live, so the next drag lands and does
  nothing.
- A withdrawn seat capability takes everything derived from it: the `wl_keyboard`'s repeat and
  focus state, and the `wp_cursor_shape_device_v1` made from the `wl_pointer`. The protocol makes
  that device inert with the capability, and the re-create tests only for NULL, so a device kept
  across an unplug leaves every later shape request going to a dead proxy.
- The raw queue is filled, with one number this protocol cannot carry. `wl_pointer` reports a
  position and no distance, and this client binds no relative-pointer protocol, so the delta is the
  step between two surface positions — already accelerated, already clamped to the surface, and the
  same number in both the accelerated and the unaccelerated pair. Everything else is real: evdev
  codes, the four xkb components the compositor sends, `value120` and the axis source, and the
  keymap the compositor handed over.
- A key held when the surface loses the keyboard is released into the raw stream here. The
  compositor sends no release for it, and the raw queue carries a key switch rather than a
  character, so a press with no release is a key a pixel guest holds down for ever. Only codes this
  client reported down are released, which is what keeps the two halves symmetrical.

## Adding a library

1. Decide what it owns, in one sentence. If that sentence has an "and" in it, it is two libraries.
2. Pick a prefix and use it on every exported symbol.
3. Place it in the dependency order and confirm nothing points back up.
4. Link nothing but the C library if a terminal program or a root daemon could ever want it. If it
   genuinely needs an external library, keep it out of the phase-1 set (`libkbase`, `libkcolor`,
   `libktui`, `libkpkg`, `libksig`) and out of everything they call, and add it to the table under
   [The constraint](#the-constraint).
5. Add its sources and include path to the `build.sh` of every program that uses it. There is no
   archive to link; a program compiles the library's `.c` files itself. Some consumers are built
   outside a recipe, and each names its libraries explicitly:
   - `script/01_phase1/12_kpkg.sh` (`kpkg`) and `script/01_phase1/13_kinstall.sh` (`kinstall`),
     the phase-1 builds. A library added under either program and missing here breaks the
     bootstrap.
   - `script/kdosbuild.sh`, which builds `kdosbuild` on the host.
   - `src_kpkg_ensure` in `ports/srclib.sh`, the host recipe reader. Its source list,
     `src/tools/kdos-portup/main.c`'s and `testing/selftest.sh`'s must agree.
   - The `libkdos.a` line in `src/desktop/kdos-comp/build.sh`, which meson links into the
     compositor.
6. Add its assertions to `src/libs/selftest.c`, especially any invariant established by comparing
   against something this library replaced.
7. Add it to the consumer compile block in `testing/selftest.sh` ("every consumer still compiles
   against the libraries"), so a header change that breaks a consumer fails on a development host
   rather than hours into a build.
8. Keep the frame state and any global private, behind accessors.
9. Add a row to [The set](#the-set) and a section to this page.

## See also

- [Writing desktop software](writing-desktop-software.md) — building on the drawing libraries
- [The design language](../03-architecture/design-language.md) — the rules they enforce
- [Testing](testing.md) — the shared test program and the sanitizer runs
- [The build system](build-system.md) — where `libkbuild` fits
- [Packaging](../03-architecture/packaging.md) — where `libkpkg` and `libksig` fit
