# Decisions

The choices in KDOS that were genuinely close, with the alternative that lost and the reason. A
decision is listed here when someone reasonable would have chosen differently, so that a reader
who disagrees can see whether their objection was already answered — and so that nobody spends a
weekend rediscovering why the obvious option does not work.

Each entry states the question, what was chosen, and what was rejected.

The last section, [Narrowings](#narrowings), holds the small ones — a decision whose whole
argument is a paragraph, and which a reader is most likely to arrive at from
[Known gaps](../06-reference/known-gaps.md) asking why something is missing.

## A hard fork of labwc, not a compositor of our own

**The question.** KDOS needs a Wayland compositor with a phosphor shader, a wallpaper it owns, a
frame-timing channel, an idle policy and per-box client identity. Write one on wlroots, or take
an existing one?

**Chosen: a frozen hard fork of labwc 0.20.0.** `src/desktop/kdos-comp` is upstream's source,
imported wholesale, rebranded, and never merged from again. `KDOS-FORK` at its root records the
tarball and its sha256. KDOS additions live in `src/kdos-*.c`; upstream files carry minimal hooks
marked `/* KDOS */`, so `grep` finds every touch point.

**Rejected: a from-scratch compositor.** The window management, session lock, screen capture,
clipboard, input-method relay, Xwayland integration and security-context filtering are not the
interesting part of this project, and each is a protocol with semantics that only show up under
real clients. Taking labwc's meant getting the parts that have been exercised for years by people
who are not us — both generations of the capture protocols, both data-control managers, the
text-input and input-method relay — and spending the effort on the parts that are actually KDOS.

**Why a fork and not patches.** A `.patch` file against a compositor is a merge conflict waiting
for the next release, and the additions here are not upstreamable: they are one distribution's
opinions. Freezing means the source in the tree is the source that builds, readable in a pager,
with no patch application step between the two.

**What it costs.** Upstream fixes do not arrive. A security fix in labwc has to be read and
applied by hand. That is accepted deliberately: the alternative was maintaining a compositor
outright.

## One pack per application, not one image

**The question.** Roughly 180 graphical applications have to reach the medium. Ship them as one
container image, or as separate artefacts?

**Chosen: one artefact per application, over a small set of shared runtimes** — an image per row
when the store builds it, a signed pack per row when a set is exported. Installing an application
disturbs nothing else, and a shared runtime's layers are stored once however many applications
name it.

**Rejected: a single container image.** It puts every application on every install whether or not
it is ever launched, and it makes adding one application a rebuild of the whole thing.

**Two walls the alternative hits that are not about size.** Overlayfs cannot add a layer to a
live mount, so installing an application would have to restart a container that other
applications are running in. And a hundred lower directories do not fit in the 4096 bytes
`mount(2)` allows for its option string. Per-application, a stack is three or four layers.

**Stated cost:** one supervisor process per *running* application.

## Debian inside boxes, not Alpine

**The question.** The application catalogue needs a base distribution. KDOS itself is musl and
would pair naturally with Alpine.

**Chosen: Debian trixie.** The catalogue is the reason. Alpine has no slicer, no VSCode build,
and no Calibre or GTKWave in stable. Debian carries the best free software in essentially every
segment the catalogue covers, packaged and patched.

**Rejected: Alpine.** The consistency would be pleasing and the images would be far smaller, and
neither buys anything for a user who wants to open a CAD file.

Alpine is nonetheless present as a *base pack* — pinned to a point release, 4.8 MB — because a
clean scratch userland that needs no network is worth one row in the catalogue.

**Stated cost:** heaviness, and it is deliberate. The medium *is* the offline software library, in
the tradition of a fat Knoppix stick. `--no-install-recommends` everywhere keeps it from being
worse.

## musl, and what it forecloses

**The question.** glibc or musl for the host C library?

**Chosen: musl**, for a small, readable, standards-focused libc that suits a system meant to be
understood in full.

**What it forecloses, stated rather than discovered later.** There is no `glibc-hwcaps`
mechanism, so the runtime-dispatch approach to CPU optimisation — building several variants of a
library and letting the loader pick — is not available; that is one of the reasons
[`kdos march`](../04-programs/kdos-command.md) measures per machine instead. Some upstream code
assumes glibc extensions and needs a flag or a patch. And a header warning that glibc does not
emit can turn an upstream `-Werror` build fatal.

The application catalogue is glibc, inside boxes, which is exactly the point of the ring
boundary.

## No KDE, GNOME or any existing desktop on the host

**The question.** A complete, mature desktop exists. Why not run one?

**Chosen: a desktop of our own, drawn as a character-cell grid.** It is the identity of the
project. It also keeps the host free of both large toolkits, which is what makes compiling the
whole host from source in one sitting tractable.

**Rejected: KDE Plasma on the host.** It would bring Qt, and with it a body of code larger than
the rest of the host combined, into the ring that is supposed to be compiled and understood here.

Note what this does *not* reject: the KDE **applications** — Dolphin, Kate, Okular, Gwenview,
Digikam and the rest — are in the catalogue, because they are the best in their segments and none
of them needs Plasma running. See [Packs and boxes](../03-architecture/packs-and-boxes.md).

## A store that builds, and a medium that carries nothing

**The question.** Roughly 180 graphical applications have to be reachable. Bake them onto the
medium, or describe them and build them on demand?

**Chosen: the medium carries a catalogue, and podman builds what somebody asks for.** A row is a
parent chain of apt packages; installing it builds an image per row, each `FROM` the one below, and
creates a box over the top one. The ISO stops carrying 23 GB of packs it may never be asked for,
adding an application is one line rather than a bake, and the shared runtime layers are stored once.
`kdos-store` and kinstall both offer it by **group**, so a selection is one tick rather than twelve.

**Rejected: baking the pack set.** Every application on every medium whether or not it is ever
launched, an hour of bake to add one row, and a release channel to push 23 GB through.

**Three costs, and they are not small.**

1. **A store install is unsigned.** It fetches content from somebody else's registry, which
   nothing in `/etc/kdos/keys` vouches for — `kdos-box create` prints exactly that about an OCI
   base, and it is true of every application built this way. *Everything here is verified and
   nothing leaves the machine* holds for an imported set and for nothing else.
2. **Installing needs a network, and minutes of apt.**
3. **A live session can install nothing.** `$HOME` is on overlayfs there, so a box's overlay upper
   has nowhere to go and `kdos-box create` refuses. Import is the only route to software on a live
   stick.

**Which is why import exists and why the pack format stays.** `kdos-appbox export` writes the
built images as signed packs with an index; `import` stages them through `kdos-packd`, which hashes
and signature-checks each one where it mounts it. An imported application is *more* verified than a
store-installed one, needs no network, and is what kinstall reads off a stick when there is no
network during an install. The pack format is how a *set is carried*, not how software is
distributed.

## The tarballs are in the tree, through Git LFS

**The question.** The upstream tarballs are 7.1 GB across 962 files, seven of them over the
100 MB a github.com push refuses. Where do they live?

**Chosen: Git LFS, in the tree.** A clone is then the whole input to a build: `git clone` and
`make build`, with no fetch step in between and nothing that can be missing. The `sha256 =` in
each recipe is what verifies an archive, and a hash with nothing to hash is a promise nobody can
check — so the thing git holds and the thing it identifies are in the same place.

**What it costs, and it is not small.** A free account provides 10 GiB of storage and 10 GiB of
monthly bandwidth, shared across every repository the account owns. 7.1 GB of that leaves under
3 GiB of margin and a month's bandwidth is a handful of clones. Exceeding the allowance does not
slow a clone down — it blocks LFS reads outright, taking the vendored art and the test fixtures
with it, so a fresh clone cannot check out at all. **A paid data pack is what keeps this
working.** `git lfs install` must also precede the clone, or the working tree holds pointer files
and the first port to unpack one fails on a corrupt archive rather than on anything that names
the cause.

**Rejected: release assets.** Two GiB per file, no total-size or bandwidth limit, and no quota to
buy — but a clone is then not enough to build, and the step that closes the gap is one more thing
to have run. The reproducibility argument won: what identifies an archive and the archive itself
belong together.

**Rejected: plain git blobs.** Seven files are over the 100 MB github.com refuses on a push, so
this does not work at all on the stated remote.

**Two properties that make this survivable.** The hash is the identity and the URL is advisory,
so a mirror can be added in ten years without invalidating a commit — a commit names contents
rather than a location. And sources are **append-only**: an asset is never deleted and never
replaced, because replacing one silently changes what an old commit builds.

**Sharded by first letter**, because a release holds a bounded number of assets and this archive
only grows. The shard is computed from the filename, so it costs no pin and no lookup.

## `-march` measured per machine, not chosen for a population

**The question.** Modern x86-64 has feature levels. Should KDOS build for one?

**Chosen: measure on the machine, keep only what wins.**
[`kdos march`](../04-programs/kdos-command.md) builds a port twice, runs that port's own
benchmark against both, and keeps the flags only where the win clears both a fixed floor and the
machine's own measured noise.

**Rejected: shipping a feature level.** The published figures for `x86-64-v3` include real wins —
FLAC, Vorbis, Zstd decompression — and real losses: bzip2 slower, Python slower, LZ4 slower *and*
drawing more power. A distribution that shipped v3 everywhere would ship those regressions and
never know.

**Rejected: runtime dispatch.** It is the right answer in principle and musl closes the door on
it, as noted above.

Since KDOS rebuilds itself on the machine it runs on, rebuild-per-machine is available here in a
way it is not elsewhere. That turns the question from "which flags" into "did they help *here*",
which is a question with an answer.

## Alpine's security database, not NVD or OSV

**The question.** `kdos cve` needs to know which pinned versions carry known vulnerabilities.

**Chosen: a vendored, pruned copy of Alpine's security database** — 264 KB, merged from a dozen
Alpine branches, committed and diffable, so the answer is offline.

**Rejected: NVD**, which is retired feeds plus an API. **Rejected: OSV**, which is the same facts
in a far larger download. Alpine is the closest distribution to KDOS that publishes machine-
readable security data: musl, the same upstream tarballs, comparable version pins.

**The honest limit** is stated with every run: a package Alpine does not carry is reported
*unknown*, never clean, and a large fraction of the tree is in that state.

## The build shell lives beside the recipe, not inside it

**The question.** A port is metadata plus a build. Where does the build script go?

**Chosen: two files.** `kpkgbuild` is declarative metadata that is *parsed* and never sourced;
`build.sh` beside it is ordinary bash. `bash -n`, shellcheck, syntax highlighting and `git diff`
all work on it, and no parser has to understand shell.

**Rejected: an argument-vector list with no shell.** It was implemented and converted most of the
tree, then refused a stubborn minority that needed heredocs, loops, redirects, globs and command
substitution.

**Rejected: embedding a shell interpreter in `kpkg`.** There is no embeddable evaluator: the
candidates are either parsers that do not execute, or programs with their own `main()` — which
would mean vendoring tens of thousands of lines of third-party C into a tree that vendors almost
none. It would also buy nothing, since bash is in the sysroot before `kpkg` is compiled and ships
on the target regardless.

**The clinching detail:** a recipe writes a configuration file whose body contains a line reading
exactly `[build]`. Any format carrying the shell inline would have had to tell that apart from
its own syntax.

## Signing the index, not every package

**The question.** A binary package host has hundreds of artefacts. What gets signed?

**Chosen: one Ed25519 signature over the index**, which carries every package's hash, so one
signature covers all of them transitively. A per-package signature sidecar exists for the
separate case of a package travelling on a USB stick with no index beside it.

**Rejected: signing every artefact as the primary mechanism**, which multiplies the work and the
number of things that can be individually wrong without improving what is proven.

**Multi-signature from day one**, because a signature file is a line per signature: during a key
rollover both keys sign and a client trusting either keeps working. Retrofitting that is brutal;
designing it in is one loop. See [Packaging](../03-architecture/packaging.md).

## Freezing a demo rather than writing one

**The question.** The system ships an ASCII-art demo. Write one, or take one?

**Chosen: a frozen hard fork of the AA-project's `bb` 1.3rc1**, recorded in `KDOS-FORK`, carrying
the sources the binary actually needs and the authors' own credits scroll. Three defects were
fixed in place, including a heap corruption that only appears on 64-bit.

**Rejected: a demo of our own.** One was written and then removed at the maintainer's request.
It is not coming back, and a stale reference to one is a leftover rather than a plan.

## Forking libtsm rather than writing a terminal

`libkvt` is a hard fork of libtsm 4.7.1, kmscon's VT100–VT520 state machine, rebranded `tsm_` →
`kvt_`. A terminal emulator is a decade of edge cases — charsets, the alternate screen, DEC private
modes, wrapping rules that differ between terminals that both claim VT100 — and none of that is a
place to be original. What is original here is the boundary, not the parser.

**Upstream's cell stays.** `kcell.h` refuses a second cell type, and that refusal is about two
libraries of the *toolkit* disagreeing — not about a terminal's private screen buffer, which nothing
outside the library ever sees. Upstream's cell earns its place: it carries 24-bit colour, a per-cell
age that drives damage tracking, and a symbol-table handle that is what makes combining characters
possible at all. Reducing it to `KtuiCell` at the boundary loses none of that until the moment the
screen is drawn.

**The conversion happens in one file.** `kvt_grid.c` is the render boundary and is where a terminal
cell becomes a `KtuiCell`. Three other files touch the toolkit and each for one narrow reason:
`kvt_term.c` maps `KT_K_*` key codes into the escape bytes a child expects, `kvt_unicode.c` asks
`ktui_wcwidth` so the library and the grid agree how wide a codepoint is, and `kvt_selection.c`
holds `kvt_ui_mouse` — what a drag over a terminal means — because every consumer of the vte needs
that decision and two copies would drift.

**`kvt_htable.c` and `kvt_grid.c` are the two files carrying no upstream copyright.** Every other
file in the library carries libtsm's; the grid is this tree's render boundary, and the hash table
was written here rather than carried.

**Colour reduces to the palette's eight slots by nearest distance** — one rule for the ANSI sixteen,
the 256 and truecolour alike. A table saying "red means the error slot" would be a second set of
colour decisions sitting beside the palette, and `kdos theme` would move one of them. The two
*default* colours are the exception and are slots outright: a terminal's default foreground is a
light grey and its background black, and reducing both by distance against eight phosphor greens
lands them on the same slot — which draws every character in the colour of the screen behind it.

## One file chooser at one width, not a wider one for the portal

The chooser a boxed application reaches through the FileChooser portal and the chooser this
desktop's own programs open are **one program at one size** — 64 columns by 22 rows — and the
portal does not get a wider one to hold a sidebar column.

**The width is not a free parameter.** The smallest screen this desktop is drawn for is 80 columns
by 24 rows, which is what the console's reference frames are cut at. A 64-column dialog leaves eight
cells of ground either side of it and one row of taskbar under it. A sidebar wide enough to read a
place name is about sixteen more, and a chooser that needed 80 columns would be a chooser with no
frame, no ground and nowhere for the bar — on the one screen every machine has.

**And the third column is what it would cost.** The chooser already spends its right-hand column on
a preview pane, so a sidebar takes its width from the names: about thirty cells for a filename, in
the window whose entire purpose is showing filenames.

**So the places are a rung and not a column.** `Ctrl+P` opens them over the file list as a declared
`Esc` rung, reaching the same list `kxdg_places()` gives the Start menu — every place, at full
width, and nothing taken from the names while it is closed. A boxed application's Open and Save get
exactly what a native one gets, which is the other half of the decision: two dialogs of two widths
would be two layouts to keep, two sets of reference frames, and two answers to how wide a chooser
is.

## Narrowings

A narrowing is a decision that reads like a gap: the thing genuinely is not there, and it is not
there because a smaller answer was chosen over a larger one. Each of these is small enough that a
section of its own would be mostly heading, and each has the same shape — what was asked for, what
is built instead, and what the larger answer would have cost.

**A drop on the desktop reaches the trash and nothing else.** Dropping a file onto a folder icon
would be a move, and a move across filesystems is a copy and an unlink that can half-succeed. A
desktop that offers the gesture has to answer what it did with the file when the second half
failed, on a surface with nowhere to say it. The trash is the one target whose failure mode is
"nothing happened".

**Screens are placed edge to edge in list order, not at coordinates.** A vertical arrangement, an
overlap and a deliberate gap cannot be expressed. What people reach a screen tool for is which
screen is left of which, and an order answers that in a list; a geometry answers it with a canvas,
a drag, a snapping rule and a validity check for the arrangements that leave a hole.

**Tabs stack; they do not tile.** `Super+Shift+s` folds one window into another as a tab, and
there are no tile groups — two windows side by side that move, size and minimise together.
`tiled` is a per-window bitmask resolved against the work area and never against a neighbour, and
the arrangements clear it afterwards precisely so that an arrangement is not a state. A group is a
relation between windows, which is the one thing the window model does not hold, so it would reuse
none of the machinery a stack reuses.

**A console screen takes a mode, and not an off, a scale or a rotation.** `kdos-display` lists
connectors and sets a mode on one; the other three verbs are Wayland's. A text grid has no scale
factor, a rotation would give the cells a different shape on one screen than on the next, and a
dark connector would leave a hole in the middle of a grid that windows are already placed across.
The buttons are drawn **disabled** rather than hidden, so the surface is the same surface in both
sessions.

**One font for every output.** The font every KDOS surface draws with is a single setting, so it
is right on a machine with one screen and wrong on two of different densities. The picker sets the
face on all of them, which keeps two screens agreeing rather than letting each be right. A
per-output font is a different design — a font per connector, a picker that asks which, and a cell
size that changes under a window when it is dragged across the boundary — not a missing call.

**No ReGIS and no Tektronix.** They are vector graphics protocols from DEC hardware, and nothing in
the catalogue emits either. The three raster protocols are what a modern program reaches for.

**Braille and speech are ports, and neither reads the desktop.** `brltty` reads `/dev/vcsa`, so it
covers `tty1` and the installer and nothing the compositor draws; `speech-dispatcher` is the API a
screen reader speaks and `espeak-ng` is the voice behind it. BrlAPI is linked by nothing here and no
KDOS surface talks to any of the three. They are on the image because they are useful to somebody at
a terminal, not because the desktop is readable — see
[Accessibility](../02-user-guide/accessibility.md).

**An invitation in a message is read and never answered.** `aerc`'s calendar filter prints the
event — summary, times, location, who was asked — and writes nothing anywhere. A filter runs every
time a message scrolls past, so one that imported would accept every meeting it was scrolled over;
and no `text/calendar` handler is registered for the same reason. Filing one is manual and it
works: `:save` the part out of the message and `khal import` it, and the day carries a mark in the
panel's calendar.

**Applications that need raw block devices are not in the catalogue** — partitioners, drive-health
tools, recovery tools. A rootless container cannot do anything useful with them, and a launcher
that opens onto a permission error teaches somebody that the machine is broken. Those jobs are
native tools on the host, which is where privilege is.

**Applications requiring a particular compositor's private protocols are out.** One catalogue
screenshot tool asks a named compositor's interface and opens an error dialog on every other.
Screenshots are the host's own tool.

**No font that has to be downloaded.** A Windows program wanting a specific proprietary font gets
a substitute. Fetching one happens at run time over the network, and nothing in the image may
depend on that.

**Editing a library rebuilds every port of ours, not only its consumers.** A recipe names which
libraries it compiles, and working out which of them a given edit actually reaches would be a
shell parser inside the package manager — reading `build.sh` to find out what it compiles. The
over-rebuild costs minutes; the parser would be a second, quieter build system.

**The initramfs carries util-linux's `switch_root` and not toybox's.** toybox's applet `chroot()`s
into the new root and never moves that root onto the root of the mount namespace, so every process
on the booted system is chrooted for ever — and `create_user_ns()` refuses a chrooted caller
outright, which is every container on the machine. The symptom is `EPERM` from `CLONE_NEWUSER` for
uid 0 with the full capability set as readily as for anybody, on a kernel reporting
`CONFIG_USER_NS=y`, no LSM, no seccomp filter and nothing on the command line;
`/proc/self/mountinfo` gives it away, with the root mount present on the right device and a
**parent id that is not in the table**. toybox owns the name `/usr/sbin/switch_root` on the
finished image and is installed after util-linux, so the copy names util-linux's own file and the
packaging step refuses to build an initramfs whose `switch_root` is toybox's.

## See also

- [Why KDOS](why-kdos.md) — the properties these decisions serve
- [Principles](principles.md) — the rules that fall out of them
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the pack decision as built
- [kdos-comp](../04-programs/kdos-comp.md) — the fork as built
- [Known gaps](../06-reference/known-gaps.md) — what these decisions leave undone
