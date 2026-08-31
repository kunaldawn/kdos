# The C libraries

Thirteen static libraries under `src/libs/`, the constraint they are built under, the dependency
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
| `libktui` | `ktui_` | Terminal ownership, the cell buffer and its diff, key and mouse decoding, character width, paste, immediate-mode widgets, modals, the three glyph tiers, charts, offscreen rendering | Nothing |
| `libkxdg` | `kxdg_` | Desktop entries, the MIME glob table, and **the one correct way to turn a command line into an argument vector** | `libkbase` |
| `libkpkg` | `kp_` | The package database, the ports tree, dependency parsing and solving, version comparison, the recipe and build-config hashes | `libkbase` |
| `libksig` | `ksig_` | Signing and verification, key files, keyrings. **The one library with vendored third-party source** | `libkbase` |
| `libkbuild` | `kbuild_`, `kj_` | Phase discovery, the phase metadata block, the build plan, the snapshot inventory, a read-only structured-data scanner | `libkbase` |
| `libkproc` | `kpr_` | Every reading about the running machine, from a **movable root**: processes, uptime, container identity, processor, memory, block devices, network, power, graphics — and the sample ring | `libkbase` |
| `libkpack` | `kpk_` | The pack format: the footer, the metadata blob, the requirement solve, the payload hash, the signature block, and the index | `libkbase`, `libksig`, `libkpkg` |
| `libkchrome` | `kch_` | The window furniture: the header band, group headings, the button bar, the list and scrollbar rule, the pixel tile | `libktui` |
| `libkicon` | `kicon_` | **A name becomes a sprite slot, or −1** | `libktui` |
| `libkcell` | `kcell_` | The glyph cache and the cell painter — a grid of cells into a pixel buffer, the character ramp built from it, and the pixel canvas | A font renderer, a pixel library |
| `libkwl` | `kwl_` | The toolkit's **Wayland backend**: surface roles, buffers, scale, input, clipboard, compose, cursors, frame throttling | `libkcell`, plus Wayland client libraries |

## Dependency direction

```
libkwl → libkcell → libktui → libkcolor → libkbase
libkchrome → libktui
libkicon   → libktui
libkxdg    → libkbase
libkpkg    → libkbase
libksig    → libkbase
libkbuild  → libkbase
libkproc   → libkbase
libkpack   → libksig, libkpkg, libkbase
```

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
- **The freedesktop trash lives here**, so a prompt and the desktop's delete key are one
  implementation. See [The kdos command](../04-programs/kdos-command.md#trash).

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

**The progress bar is a wrapper whose pixels must not move.** The installer links it and only it,
pinning a solid two-state bar. Change the general form freely; leave that branch alone.

**A resize is not applied until the consumer applies it.** The backend sets a flag; the reported
size follows only when the loop calls the resize and invalidate functions. Any loop that owns a
surface owns this — a surface that was always a fixed size and then starts being resized will draw
against stale dimensions and silently fail its own bounds checks.

**Offscreen rendering** takes a fixed size and writes the cell buffer out as plain text, with no
terminal at all. Every geometry defect this toolkit has shipped was invisible to the compiler and
to a test suite that cannot draw; this is how they get looked at.

Three rules the extraction from its original single consumer exists to keep:

- **Symbols are prefixed.** The generic names it once used collided with a consumer's own
  definitions of the same names with different semantics — two of our own programs could not be
  linked together.
- **The frame state is private.** It was a public structure that applications assigned to field by
  field; it is behind accessors now.
- **Chrome identifiers are the library's business.** Chrome registers with caller-local
  identifiers in a reserved range that never joins the focus ring and never drags the page scroll.

## libkxdg

Desktop entries, the MIME glob table, and the command-line splitter.

**The splitter is the single implementation** of turning a desktop entry's command into an argument
vector: it unquotes, substitutes the file-argument codes, drops the codes that carry no argument,
and — with a negative count — keeps every code verbatim for a tool that **rewrites** a line rather
than running one. Its inverse re-quotes, so a generator's output round-trips.

Every launch path in the system goes through it. See
[kdos-appbox](../04-programs/kdos-appbox.md#exec-lines).

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

## libkicon

**One job: a name, or a file path, becomes a sprite slot — or −1.**

**Minus one is not a failure.** It is a terminal, an install with no artwork, icons switched off,
and a name nothing on this machine has a picture for. **Every caller draws its glyph tier then**,
exactly as it would without this library, which is the rule the whole icon layer is built under.

The test harness stubs it to exactly that, so a committed reference frame is the **character grid**.

## libkcell

The glyph cache and the cell painter: a grid of cells into a pixel buffer, the character ramp built
from it, and the pixel canvas a block of cells can be drawn as.

**The canvas is what makes a pixel tile possible** without a second renderer — a pixel image exactly
some number of cells across, with fills and text at an arbitrary pixel size, handed to the toolkit
as a sprite. See [kdos-shell](../04-programs/kdos-shell.md#the-start-button).

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
- **A compose table that fails to build is absent, never partial.**
- **A lock surface must not receive the pre-configure commit**, which is a protocol error there.

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
