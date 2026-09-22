# Decisions

This page records the choices in KDOS that were genuinely close: the ones where a reasonable
engineer would have gone the other way. Each entry states the conclusion first, then the question
behind it, then the alternatives and why they lost, then what the choice costs.

It exists so that a reader who disagrees can see whether their objection was already answered, and
so that nobody spends a weekend rediscovering why the obvious option does not work.

## Index

| Decision | Conclusion |
|---|---|
| [The compositor](#the-compositor-is-a-frozen-fork-of-labwc) | A frozen hard fork of labwc 0.20.0, never merged from again |
| [Application packaging](#one-pack-per-application-not-one-image) | One artefact per application over shared runtimes, never one image |
| [The base distribution in boxes](#debian-inside-boxes-not-alpine) | Debian trixie, with Alpine carried as a scratch base |
| [The host C library](#musl-as-the-host-c-library) | musl, and runtime CPU dispatch is foreclosed by it |
| [The host desktop](#no-kde-gnome-or-any-existing-desktop-on-the-host) | A desktop written for this system; KDE's applications, never Plasma |
| [Application delivery](#a-store-that-builds-and-a-medium-that-carries-nothing) | The medium carries a catalogue; podman builds what is asked for |
| [Where upstream archives live](#the-tarballs-are-in-the-tree-through-git-lfs) | Git LFS, in the tree, so a clone is the whole input to a build |
| [CPU optimisation](#march-measured-per-machine) | Measured per machine by `kdos march`, never a shipped feature level |
| [The vulnerability database](#alpines-security-database-not-nvd-or-osv) | A vendored, pruned copy of Alpine's secdb, offline |
| [Recipe format](#the-build-shell-lives-beside-the-recipe) | Two files: parsed metadata, plus ordinary bash |
| [Binhost signing](#signing-the-index-not-every-package) | One signature over the index, multi-signature from the start |
| [The ASCII demo](#freezing-a-demo-rather-than-writing-one) | A frozen hard fork of the AA-project's `bb` 1.3rc1 |
| [The terminal state machine](#forking-libtsm-rather-than-writing-a-terminal) | `libkvt`, a hard fork of libtsm 4.7.1 |
| [The file chooser](#one-file-chooser-at-one-width) | One program at 64×22 for portal and native callers alike |
| [Narrowings](#narrowings) | Small decisions that read like gaps |

## The compositor is a frozen fork of labwc

KDOS runs a frozen hard fork of labwc 0.20.0. `src/desktop/kdos-comp` is upstream's source,
imported wholesale, rebranded, and never merged from again. `KDOS-FORK` at its root records the
tarball and its sha256. KDOS additions live in `src/kdos-*.c`, and upstream files carry minimal
hooks marked `/* KDOS */`, so `grep` finds every touch point.

The question was what to do about a compositor that needs a phosphor shader, a wallpaper it owns,
a frame-timing channel, an idle policy and per-box client identity: write one on wlroots, or take
an existing one.

A from-scratch compositor was rejected because window management, session lock, screen capture,
clipboard, the input-method relay, Xwayland integration and security-context filtering are not the
interesting part of this project, and each is a protocol whose semantics only surface under real
clients. Taking labwc's implementation meant getting parts that have been exercised for years by
people outside this project — both generations of the capture protocols, both data-control
managers, the text-input and input-method relay — and spending the effort on the parts that are
actually KDOS.

A fork rather than a patch series, because a `.patch` file against a compositor is a merge conflict
waiting for the next release, and these additions are not upstreamable: they are one
distribution's opinions. Freezing means the source in the tree is the source that builds, readable
in a pager, with no patch application step between the two.

The cost is that upstream fixes do not arrive. A security fix in labwc has to be read and applied
by hand. That is accepted deliberately; the alternative was maintaining a compositor outright.

See [kdos-comp](../04-programs/kdos-comp.md) for the fork as built.

## One pack per application, not one image

Each application is one artefact over a small set of shared runtimes: an image per catalogue row
when the store builds it, a signed pack per row when a set is exported. Installing an application
disturbs nothing else, and a shared runtime's layers are stored once however many applications
name it.

The question was how 183 graphical applications reach the medium — as one container image, or as
separate artefacts.

A single container image was rejected because it puts every application on every install whether
or not it is ever launched, and it makes adding one application a rebuild of the whole thing.

Two walls the alternative hits have nothing to do with size. Overlayfs cannot add a layer to a live
mount, so installing an application would have to restart a container that other applications are
running in. And a hundred lower directories do not fit in the 4096 bytes `mount(2)` allows for its
option string. Per-application, a stack is three or four layers.

The cost is one supervisor process per running application.

## Debian inside boxes, not Alpine

The application catalogue is built on `debian:trixie-slim`.

The question was which base distribution the catalogue should use. KDOS itself is musl and would
pair naturally with Alpine.

The catalogue is the reason Debian won. Alpine has no slicer, no VSCode build, and no Calibre or
GTKWave in stable. Debian carries the best free software in essentially every segment the
catalogue covers, packaged and patched. The consistency of an all-musl system would be pleasing
and the images would be far smaller, and neither buys anything for a user who wants to open a CAD
file.

Alpine is present all the same, as a base pack pinned to `alpine:3.24.1` — about 3 MB of busybox
and musl. A clean scratch userland that needs no network is worth one row in the catalogue, and it
is the one non-Debian rootfs small enough that carrying it is free.

The cost is heaviness, and it is deliberate. The medium is the offline software library, in the
tradition of a fat Knoppix stick. `--no-install-recommends` everywhere keeps it from being worse.

## musl as the host C library

The host C library is musl: small, readable and standards-focused, which suits a system meant to
be understood in full.

What musl forecloses is worth stating rather than discovering later. There is no `glibc-hwcaps`
mechanism, so runtime dispatch for CPU optimisation — building several variants of a library and
letting the loader pick — is not available, which is one reason
[`kdos march`](../04-programs/kdos-command.md) measures per machine instead. Some upstream code
assumes glibc extensions and needs a flag or a patch. And a header warning that glibc does not
emit can turn an upstream `-Werror` build fatal.

The application catalogue is glibc, inside boxes, which is exactly the point of the ring boundary.

## No KDE, GNOME or any existing desktop on the host

The host runs a desktop written for it, in which every surface KDOS paints is a character-cell
grid — the panel and all its surfaces, the file chooser, the resource monitor, the terminal, the
lock screen, the installer, the boot splash and `tty1`. The compositor is the one place with
pixels of its own: it links `cairo` and `pangocairo` and draws titlebars, the root menu and the
window-switcher OSD with pango, at a size matched to the grid. An application in a box draws
whatever its toolkit draws.

The question is fair: a complete, mature desktop exists, so why not run one? Two answers. The
cell grid is the identity of the project, and keeping it means the host stays free of both large
toolkits, which is what makes compiling the whole host from source in one sitting tractable.

KDE Plasma on the host was the serious alternative and was rejected because it would bring Qt, and
with it a body of code larger than the rest of the host combined, into the ring that is supposed
to be compiled and understood here.

Note what this does not reject. The KDE applications — Dolphin, Kate, Okular, Gwenview, Digikam
and the rest — are in the catalogue, because they are the best in their segments and none of them
needs Plasma running. See [Packs and boxes](../03-architecture/packs-and-boxes.md).

## A store that builds, and a medium that carries nothing

The medium carries a catalogue, and podman builds what somebody asks for. A catalogue row is a
parent chain of apt packages; installing it builds an image per row, each `FROM` the one below,
and creates a box over the top one. Adding an application to KDOS is one line in a text file
rather than a bake, and the shared runtime layers are stored once. `kdos-store` and kinstall both
offer the catalogue by group — seven of them, from `essential` at five applications to `games` at
thirteen — so a selection is one tick rather than thirteen.

Baking the pack set onto the ISO was rejected: every application on every medium whether or not it
is ever launched, an hour of bake to add one row, and a release channel to push the whole set
through.

Three costs follow, and they are not small.

1. A store install is unsigned. It fetches content from somebody else's registry, which nothing in
   `/etc/kdos/keys` vouches for. `kdos-box create` prints exactly that about an OCI base, and it
   is true of every application built this way. The claim that everything is verified and nothing
   leaves the machine holds for an imported set and for nothing else.
2. Installing needs a network, and minutes of apt.
3. A live session can install nothing. `$HOME` is on overlayfs there, so a box's overlay upper has
   nowhere to go and `kdos-box create` refuses. Import is the only route to software on a live
   stick.

Which is why import exists and why the pack format stays. `kdos-appbox export` writes the built
images as signed packs with an index; `import` stages them through `kdos-packd`, which hashes and
signature-checks each one where it mounts it. An imported application is more verified than a
store-installed one, needs no network, and is what kinstall reads off a stick when there is no
network during an install. The pack format is how a set is carried, not how software is
distributed.

## The tarballs are in the tree, through Git LFS

Upstream archives live in the repository, held by Git LFS: 1,025 objects, about 8.3 GB, nine of
them over the 100 MiB a github.com push refuses.

The reason is that a clone is then the whole input to a build — `git clone` followed by
`make build`, with no fetch step between and nothing that can be missing. The `sha256 =` in each
recipe is what verifies an archive, and a hash with nothing to hash is a promise nobody can check,
so the thing git holds and the thing it identifies are in the same place.

Release assets were the strongest alternative: two GiB per file, no total-size or bandwidth limit,
and no quota to buy. They lost because a clone is then not enough to build, and the step that
closes the gap is one more thing to have run. Plain git blobs do not work at all on the stated
remote, since nine files exceed the 100 MiB push limit.

What LFS costs is not small either. A free account provides 10 GiB of storage and 10 GiB of
monthly bandwidth, shared across every repository the account owns. The archives are 7.7 GiB of
that, which leaves about 2.3 GiB of margin, and a month's bandwidth is a handful of clones. Exceeding the allowance does not
slow a clone down — it blocks LFS reads outright, taking the vendored art and the test fixtures
with it, so a fresh clone cannot check out at all. A paid data pack is what keeps this working.
`git lfs install` must also precede the clone, or the working tree holds pointer files and the
first port to unpack one fails on a corrupt archive rather than on anything naming the cause.

Two properties make the arrangement survivable. The hash is the identity and the URL is advisory,
so a mirror can be added in ten years without invalidating a commit — a commit names contents
rather than a location. And sources are append-only: an asset is never deleted and never replaced,
because replacing one silently changes what an old commit builds. Assets are sharded by first
letter, since a release holds a bounded number of them and this archive only grows; the shard is
computed from the filename, so it costs no pin and no lookup.

## `-march` measured per machine

CPU optimisation is measured on the machine that will run the result.
[`kdos march`](../04-programs/kdos-command.md) builds a port twice, runs that port's own benchmark
against both, and keeps the flags only where the win clears both a fixed floor and the machine's
own measured noise.

Shipping a feature level was rejected on the published figures. `x86-64-v3` brings real wins —
FLAC, Vorbis, Zstd decompression — and real losses: bzip2 slower, Python slower, LZ4 slower and
drawing more power. A distribution that shipped v3 everywhere would ship those regressions and
never know.

Runtime dispatch is the right answer in principle, and musl closes the door on it.

Because KDOS rebuilds itself on the machine it runs on, rebuild-per-machine is available here in a
way it is not elsewhere. That turns "which flags" into "did they help here", which is a question
with an answer.

## Alpine's security database, not NVD or OSV

[`kdos cve`](../04-programs/kdos-command.md) answers from a vendored, pruned copy of Alpine's
security database — 262 KB, merged from six Alpine branches (v3.19 through v3.24), committed and
diffable, so the answer is offline.

NVD was rejected as retired feeds plus an API; OSV as the same facts in a far larger download.
Alpine is the closest distribution to KDOS that publishes machine-readable security data: musl,
the same upstream tarballs, comparable version pins.

The honest limit is stated with every run. A package Alpine does not carry is reported unknown,
never clean, and a large fraction of the tree is in that state.

## The build shell lives beside the recipe

A port is two files. `kpkgbuild` is declarative metadata that is parsed and never sourced;
`build.sh` beside it is ordinary bash. `bash -n`, shellcheck, syntax highlighting and `git diff`
all work on it, and no parser has to understand shell.

An argument-vector list with no shell at all was implemented and converted most of the tree, then
ran into a stubborn minority of ports needing heredocs, loops, redirects, globs and command
substitution.

Embedding a shell interpreter in `kpkg` was rejected because there is no embeddable evaluator: the
candidates are either parsers that do not execute, or programs with their own `main()`, which
would mean vendoring tens of thousands of lines of third-party C into a tree that vendors almost
none. It would buy nothing either, since bash is in the sysroot before `kpkg` is compiled and
ships on the target regardless.

The clinching detail is smaller and sharper. A recipe writes a configuration file whose body
contains a line reading exactly `[build]`. Any format carrying the shell inline would have had to
tell that apart from its own syntax.

See [Writing ports](../05-developer/writing-ports.md) for the format as built.

## Signing the index, not every package

A binhost is protected by one Ed25519 signature over the index, which carries every package's
hash, so one signature covers all of them transitively. A per-package signature sidecar exists for
the separate case of a package travelling on a USB stick with no index beside it.

Signing every artefact as the primary mechanism was rejected because it multiplies the work and
the number of things that can be individually wrong without improving what is proven.

Multi-signature is in from day one, because a signature file is a line per signature: during a key
rollover both keys sign and a client trusting either keeps working. Retrofitting that is brutal;
designing it in is one loop. See [Packaging](../03-architecture/packaging.md).

## Freezing a demo rather than writing one

The ASCII-art demo is a frozen hard fork of the AA-project's `bb` 1.3rc1, recorded in `KDOS-FORK`,
carrying the sources the binary actually needs and the authors' own credits scroll.

Three defects are fixed in place: `clear_zbuff()` cleared `sizeof(long)` per cell against a
`sizeof(int)` allocation, which is the same size on the hardware it was written for and double on
x86-64, so the heap corrupts; the message scroll used `memcpy` on overlapping ranges, which musl is
free not to survive; and `REGISTERS(n)` expanded to an x86-32-only `regparm` attribute that warns
on every declaration.

A demo written from scratch is not on the roadmap. `bb` is a set of scenes paced against three
tracker modules it ships with, and reaching that from nothing is a project of its own; the frozen
fork is the whole of the plan.

## Forking libtsm rather than writing a terminal

`libkvt` is a hard fork of libtsm 4.7.1, kmscon's VT100–VT520 state machine, rebranded `tsm_` to
`kvt_`. A terminal emulator is a decade of edge cases — charsets, the alternate screen, DEC
private modes, wrapping rules that differ between terminals that both claim VT100 — and none of it
is a place to be original. What is original here is the boundary, not the parser.

Upstream's cell stays. `kcell.h` refuses a second cell type, and that refusal is about two
libraries of the toolkit disagreeing, not about a terminal's private screen buffer, which nothing
outside the library ever sees. Upstream's cell earns its place: it carries 24-bit colour, a
per-cell age that drives damage tracking, and a symbol-table handle that is what makes combining
characters possible at all. Reducing it to a `KtuiCell` at the boundary loses none of that until
the moment the screen is drawn.

The conversion happens in one file. `kvt_grid.c` is the render boundary and is where a terminal
cell becomes a `KtuiCell`. Three other files touch the toolkit, each for one narrow reason:
`kvt_term.c` maps `KT_K_*` key codes into the escape bytes a child expects; `kvt_unicode.c` asks
`ktui_wcwidth` so the library and the grid agree how wide a codepoint is; and `kvt_selection.c`
holds `kvt_ui_mouse`, which decides what a drag over a terminal means, because every consumer of
the vte needs that decision and two copies would drift.

`kvt_grid.c`, `kvt_term.c` and `kvt_htable.c` (with its header) are the files carrying no upstream
copyright. Every other file in the library carries libtsm's: the grid is this tree's render
boundary, `kvt_term` is the screen-plus-state-machine-plus-child object upstream never had, and the
hash table was written here rather than carried.

Colour reduces to the palette's eight slots by nearest distance — one rule for the ANSI sixteen,
the 256 and truecolour alike. A table saying "red means the error slot" would be a second set of
colour decisions sitting beside the palette, and `kdos theme` would move only one of them. The two
default colours are the exception and are slots outright: a terminal's default foreground is a
light grey and its background black, and reducing both by distance against eight phosphor greens
lands them on the same slot, which draws every character in the colour of the screen behind it.

## One file chooser at one width

The chooser a boxed application reaches through the FileChooser portal and the chooser this
desktop's own programs open are one program at one size, 64 columns by 22 rows. The portal does
not get a wider one to hold a sidebar column.

The width is not a free parameter. The smallest screen this desktop is drawn for is 80 columns by
24 rows, which is what the reference frames are cut at. A 64-column dialog leaves eight cells of
ground either side of it and one row of taskbar under it. A sidebar wide enough to read a place
name is about sixteen more, and a chooser needing 80 columns would be a chooser with no frame, no
ground and nowhere for the bar, on the one screen every machine has.

The third column is what a sidebar would actually cost. The chooser already spends its right-hand
column on a preview pane, so a sidebar takes its width from the names: about thirty cells for a
filename, in the window whose entire purpose is showing filenames.

So the places are a rung and not a column. `Ctrl+P` opens them over the file list as a declared
`Esc` rung, reaching the same list `kxdg_places()` gives the Start menu — every place, at full
width, and nothing taken from the names while it is closed. A boxed application's Open and Save
get exactly what a native one gets, which is the other half of the decision: two dialogs of two
widths would be two layouts to keep, two sets of reference frames, and two answers to how wide a
chooser is.

## Narrowings

A narrowing is a decision that reads like a gap. The thing genuinely is not there, and it is not
there because a smaller answer was chosen over a larger one. Each of these is small enough that a
section of its own would be mostly heading, and each has the same shape: what was asked for, what
is built instead, and what the larger answer would have cost.

**A drop on the desktop reaches the trash and nothing else.** Dropping a file onto a folder icon
would be a move, and a move across filesystems is a copy and an unlink that can half-succeed. A
desktop offering the gesture has to answer what it did with the file when the second half failed,
on a surface with nowhere to say it. The trash is the one target whose failure mode is "nothing
happened".

**Screens are placed edge to edge in list order, not at coordinates.** A vertical arrangement, an
overlap and a deliberate gap cannot be expressed. What people reach a screen tool for is which
screen is left of which, and an order answers that in a list; a geometry answers it with a canvas,
a drag, a snapping rule and a validity check for the arrangements that leave a hole.

**Tabs stack; they do not tile.** The compositor's `AddToTabGroup` action folds the focused window
onto the one behind it, and there are no tile groups — two windows side by side that move, size
and minimise together. `tiled` is a per-window bitmask resolved against the work area and never
against a neighbour, and the arrangements clear it afterwards precisely so that an arrangement is
not a state. A group is a relation between windows, which is the one thing the window model does
not hold, so it would reuse none of the machinery a stack reuses. See
[the window model](../03-architecture/window-model.md).

**One font for every output.** `comp.conf` holds `chrome_font` and `panel_font`, and both are
machine-wide, so they are right on a machine with one screen and wrong on two of different
densities. `kdos-style`'s font page offers fontconfig's monospace families and writes those two
keys, setting the face on every output, which keeps two screens agreeing rather than letting each
be right. A per-output font is a different design — a font per connector, a picker that asks which,
and a cell size that changes under a window when it is dragged across the boundary — not a missing
call.

**No ReGIS and no Tektronix.** They are vector graphics protocols from DEC hardware, and nothing
in the catalogue emits either. The three raster protocols the terminal implements are what a
modern program reaches for. See [kdos-term](../04-programs/kdos-term.md).

**Braille and speech are ports, and neither reads the desktop.** `brltty` reads `/dev/vcsa`, so it
covers `tty1` and the installer and nothing the compositor draws; `speech-dispatcher` is the API a
screen reader speaks and `espeak-ng` is the voice behind it. BrlAPI is linked by nothing here and
no KDOS surface talks to any of the three. They are on the image because they are useful to
somebody at a terminal, not because the desktop is readable — see
[Accessibility](../02-user-guide/accessibility.md).

**An invitation in a message is read and never answered.** `aerc`'s calendar filter prints the
event — summary, times, location, who was asked — and writes nothing anywhere. A filter runs every
time a message scrolls past, so one that imported would accept every meeting it was scrolled over,
and no `text/calendar` handler is registered for the same reason. Filing one is manual and it
works: `:save` the part out of the message and `khal import` it, and the day carries a mark in the
panel's calendar.

**Applications that need raw block devices are not in the catalogue** — partitioners, drive-health
tools, recovery tools. A rootless container cannot do anything useful with them, and a launcher
that opens onto a permission error teaches somebody that the machine is broken. Those jobs are
native tools on the host, which is where privilege is.

**Applications requiring a particular compositor's private protocols are out.** One catalogue
candidate screenshot tool asks a named compositor's interface and opens an error dialog on every
other. Screenshots are the host's own tool.

**No font that has to be downloaded.** A Windows program wanting a specific proprietary font gets
a substitute. Fetching one happens at run time over the network, and nothing in the image may
depend on that.

**Editing a library rebuilds every port in this tree, not only its consumers.** A recipe names which
libraries it compiles, and working out which of them a given edit actually reaches would be a
shell parser inside the package manager, reading `build.sh` to find out what it compiles. The
over-rebuild costs minutes; the parser would be a second, quieter build system.

**The initramfs carries util-linux's `switch_root` and not toybox's.** toybox's applet `chroot()`s
into the new root and never moves that root onto the root of the mount namespace, so every process
on the booted system is chrooted for ever — and `create_user_ns()` refuses a chrooted caller
outright, which is every container on the machine. A process joining the namespace with `setns()`
(podman exec, distrobox enter, `nsenter -m`) gets the empty initramfs rootfs as `/` and every path
is `ENOENT`. The symptom is `EPERM` from `CLONE_NEWUSER` for uid 0 with the full capability set as
readily as for anybody, on a kernel reporting `CONFIG_USER_NS=y`, no LSM, no seccomp filter and
nothing on the command line; `/proc/self/mountinfo` gives it away, with the root mount present on
the right device and a parent id that is not in the table. toybox owns the name
`/usr/sbin/switch_root` on the finished image and is installed after util-linux, so the packaging
step copies util-linux's own file by its real name and then refuses to build an initramfs whose
`switch_root` is toybox's.

## See also

- [Why KDOS](why-kdos.md) — the properties these decisions serve
- [Principles](principles.md) — the rules that fall out of them
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the pack decision as built
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor fork as built
- [Known gaps](../06-reference/known-gaps.md) — what these decisions leave undone
