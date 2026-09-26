# Decisions

This page covers the choices in KDOS that were genuinely close: the ones where a reasonable
engineer could have gone the other way. It is for readers who want to know why KDOS is built the
way it is, and for contributors about to propose a different approach. If you disagree with
something in KDOS, look here first: your objection may already have an answer, and the obvious
alternative may have a reason it does not work.

Each entry states the conclusion first, then the question behind it, then the alternatives and
why they lost, then what the choice costs. [Why KDOS](why-kdos.md) and
[Principles](principles.md) give the background the entries assume. The index below lists every
decision; the last section, [Narrowings](#narrowings), collects smaller choices that look like
missing features.

## Index

| Decision | Conclusion |
|---|---|
| [The compositor](#the-compositor-is-a-frozen-fork-of-labwc) | A frozen hard fork of labwc 0.20.0, never merged from again |
| [Application packaging](#one-pack-per-application-not-one-image) | One artefact per application over shared runtimes, never one image |
| [The base distribution in boxes](#debian-inside-boxes-not-alpine) | Debian trixie, with Alpine carried as a scratch base |
| [The host C library](#musl-as-the-host-c-library) | musl, and runtime CPU dispatch is foreclosed by it |
| [The host desktop](#no-kde-gnome-or-any-existing-desktop-on-the-host) | A desktop written for this system; KDE's applications, never Plasma |
| [Application delivery](#a-store-that-builds-and-a-medium-that-carries-nothing) | The medium carries a catalogue; podman builds what is asked for |
| [Where upstream sources live](#upstream-archives-are-content-addressed-release-assets) | Release assets named by their sha256, fetched by `make fetch` |
| [CPU optimisation](#-march-measured-per-machine-not-chosen-for-a-population) | Measured per machine by `kdos march`, never a shipped feature level |
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

The question was what to do about a compositor that needs a phosphor shader (the pass that
renders the screen as a green CRT), a wallpaper it owns,
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
by hand. That is accepted deliberately; the alternative is maintaining a compositor outright.

See [kdos-comp](../04-programs/kdos-comp.md) for the fork as built.

## One pack per application, not one image

Each application is one artefact over a small set of shared runtimes: an image per catalogue row
when the store builds it, and a [pack](../06-reference/glossary.md) per row when a set is
exported (signed when a signing key is
available). Installing an application
disturbs nothing else, and a shared runtime's layers are stored once however many applications
name it.

The question was how 180 graphical applications reach a machine: as one container image, or as
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

The question was which base distribution the catalogue should use. KDOS itself uses musl as its C
library and would pair naturally with Alpine.

The catalogue is the reason Debian won. Alpine has no slicer, no VSCode build, and no Calibre or
GTKWave in stable. Debian carries the best free software in essentially every segment the
catalogue covers, packaged and patched. The consistency of an all-musl system would be pleasing
and the images would be far smaller, and neither buys anything for a user who wants to open a CAD
file.

Alpine is present all the same, as a base pack pinned to `alpine:3.24.1` — about 3 MB of busybox
and musl. A clean scratch userland that needs no network is worth one row in the catalogue, and it
is the one non-Debian rootfs small enough that carrying it is free.

The cost is heaviness, and it is deliberate: a Debian box is far larger than the same program on
Alpine would be. Installing with `--no-install-recommends` everywhere keeps it from being worse.

## musl as the host C library

The host C library is musl: small, readable and standards-focused, which suits a system meant to
be understood in full.

What musl forecloses is worth stating rather than discovering later. There is no `glibc-hwcaps`
mechanism, so runtime dispatch for CPU optimisation — building several variants of a library and
letting the loader pick — is not available, which is one reason
[`kdos march`](../04-programs/kdos-command.md) measures per machine instead. Some upstream code
assumes glibc extensions and needs a flag or a patch. And a header warning that glibc does not
emit can turn an upstream `-Werror` build fatal.

The application catalogue is glibc, inside boxes, which is exactly the point of the
[ring](../06-reference/glossary.md) boundary: the host and the boxes need not share a C library.

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

Which is why import exists and why the pack format stays. `kdos-appbox export <file.ktar>
<id|group>...` writes the built images as packs with an index, signed when a key is readable
through `KDOS_PACK_KEY` and saying plainly that it is unsigned otherwise. `kdos-appbox import
<file.ktar>` stages them through `kdos-packd`, which hashes and signature-checks each one where it
mounts it. An imported application is more verified than a
store-installed one, needs no network, and is what kinstall reads off a stick when there is no
network during an install. The pack format is how a set is carried, not how software is
distributed.

## Upstream archives are content-addressed release assets

Upstream source archives are not kept in git. A few small upstream files a recipe hashes — patch
levels, IANA registries, `certdata.txt`, a language model — are, and they are never archived. Every
other source file is a release asset in the GitHub repository
`kunaldawn/kdos-sources`, named by its own sha256 (the same string as the `sha256 =` line in the
recipe that uses it), in one of 256 releases named `sha256-00` to `sha256-ff` after the hash's
first byte. A file's address is therefore

```
https://github.com/kunaldawn/kdos-sources/releases/download/sha256-<first two hex digits>/<hash>
```

The recipes name 1,232 distinct files, about 8.3 GiB. Twenty-four files in the port directories
are over the 100 MiB a github.com push refuses (twelve distinct files, since the LLVM source
tarball serves eight ports), and the largest, `linux-firmware`, is 632 MiB. `KDOS_SOURCES_REPO`
names a different archive repository, and `KDOS_SOURCES_BASE` a different download base; setting
`KDOS_SOURCES_BASE` empty makes `make fetch` use upstream URLs only.

The hash is the identity and the URL is advisory. A recipe names contents, not a location, so a
file that verifies is the file the recipe meant whether it came from the archive, from upstream or
from a mirror added in ten years, and none of those invalidates a commit. The name being the hash
also means a URL needs no index, no manifest and no lookup, and that two different upstream
releases under one filename cannot collide. Hashes spread evenly over the 256 shards, so a shard
reaches the 1,000-asset limit of a release only somewhere past 200,000 archives; a filename-keyed
shard would skew by first letter and fill within years. GitHub also rewrites asset names containing
characters outside `[A-Za-z0-9._-]`, which a bare hash never has.

The archive is append-only. An asset whose digest matches its name is never replaced or deleted,
because replacing one would silently change what an old commit builds; a checkout from five years
ago finds the bytes it was written against after upstream has moved or gone. For each KDOS
release, `ports/publish --freeze <tag>` attaches `sources.sha256`, the list of every hash that
tag's recipes name, to the release of that tag on the main repository, creating it as a draft when
it does not exist. Published with GitHub's immutable releases enabled, that list cannot change, so
a rewrite of the archive is detectable from outside it.

What it costs is that a clone alone does not build. `make fetch` has to run once after a clone
and again after a recipe changes, and it is the only step that uses the network. For each file it
takes the first copy whose hash matches, looking in this order:

1. the port directory;
2. the local cache, `ports/.srccache/` (or wherever `KDOS_SRCCACHE` points), which each port
   directory hard-links into;
3. the archive;
4. the upstream URL in the recipe's `source =` line;
5. for a port's own vendor bundle only, regenerating it reproducibly.

It is safe to re-run. The cache means a branch switch downloads nothing, and so does a second
checkout on the same machine once `KDOS_SRCCACHE` points both at one cache. A file that is missing
from the archive, or an archive that cannot be reached, costs a download from upstream rather than
a failed build, as long as upstream still has it.

The other cost is on the publishing side. A new or bumped source has to reach the archive before
the commit naming it is pushed, or that commit builds on the machine that wrote it and nowhere
else. `ports/publish` uploads missing sources (it needs a token in `$KDOS_SOURCES_TOKEN` or
`~/.config/kdos/sources-token`, mode 600). The pre-push hook in `script/hooks/` refuses a push
whose recipes name a hash the archive lacks. It also refuses when it cannot reach the archive, or
the archive answers with anything but found or not found, because absence cannot then be ruled
out: a push made offline is refused even when nothing is unpublished. The hook is enabled per clone
with `git config core.hooksPath script/hooks`, and `KDOS_SKIP_PUBLISH_CHECK=1` bypasses it. See
[Writing ports](../05-developer/writing-ports.md#publishing-sources) for the procedure.

Git LFS would make a clone the whole input to a build, and it is not used because the sources do
not fit it. A free account has 10 GiB of LFS storage and 10 GiB of monthly bandwidth, shared across
every repository the account owns. The current sources are 8.3 GiB of that storage on their own,
and every version the repository's history names comes to about 17 GiB, well past it; a single
clone uses most of a month's bandwidth. Past the allowance LFS reads
are blocked outright, not slowed, so a repository that depends on it stops checking out. Release
assets carry no total-size or bandwidth limit and allow 2 GiB per file. Plain git blobs are not
possible at all, since 24 files exceed the push limit.

## `-march` measured per machine, not chosen for a population

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

An argument-vector list with no shell at all fits most ports, but a stubborn minority need
heredocs, loops, redirects, globs and command substitution, and a format that cannot express
those cannot build them.

Embedding a shell interpreter in `kpkg` was rejected because there is no embeddable evaluator: the
candidates are either parsers that do not execute, or programs with their own `main()`, which
would mean vendoring tens of thousands of lines of third-party C into a tree that vendors almost
none. It would buy nothing either, since bash is in the sysroot before `kpkg` is compiled and
ships on the target regardless.

The clinching detail is smaller and sharper. The `rust` port's `build.sh` writes a `config.toml`
whose body contains a line reading exactly `[build]`, the start of a TOML section. A recipe format
carrying the shell inline under INI-style section headers, with `[build]` as one of them, would
read that line as the start of its own section.

See [Writing ports](../05-developer/writing-ports.md) for the format as built.

## Signing the index, not every package

A binhost is protected by one Ed25519 signature over the index, which carries every package's
hash, so one signature covers all of them transitively. A per-package signature sidecar exists for
the separate case of a package travelling on a USB stick with no index beside it.

Signing every artefact as the primary mechanism was rejected because it multiplies the work and
the number of things that can be individually wrong without improving what is proven.

The signature file holds any number of signatures, one per line, so during a key rollover both
keys sign and a client trusting either keeps working. Adding that to a format that holds only one
signature would be a format change every client has to understand. See [Packaging](../03-architecture/packaging.md).

## Freezing a demo rather than writing one

The ASCII-art demo is a frozen hard fork of the AA-project's `bb` 1.3rc1, recorded in `KDOS-FORK`,
carrying the sources the binary actually needs and the authors' own credits scroll.

`KDOS-FORK` lists every change the fork makes. The changes fall into two groups:

- **What it needs to run here.** `clear_zbuff()` clears exactly the `sizeof(int)` per cell it
  allocates; clearing `sizeof(long)`, double on x86-64, corrupts the heap. The message scroll
  moves overlapping ranges with `memmove`, because `memcpy` on them is undefined and musl does not
  tolerate it. `REGISTERS(n)` expands to nothing, because the x86-32-only `regparm` attribute warns
  on every declaration here. The drawing is paced to one frame per 16 ms, the mixer runs on its own
  thread so the music does not starve while the window is being drawn, and the scene clock follows
  the music player so the two stay in step. Each frame is wrapped in synchronized output (DECSET
  2026), so a terminal that supports it, `kdos-term` included, draws whole frames rather than half
  of two.
- **What would otherwise be false.** The demo starts by itself, with `-nosound` and `-mixer` as
  flags in place of upstream's two start-up questions; the flash words, the closing text and two
  logo beats carry KDOS's mark; and the closing text turns its own pages in time with the last
  module. Every contributor line and every Special Thanks in the credits is upstream's, unchanged.

A demo written from scratch is not on the roadmap. `bb` is a set of scenes paced against three
tracker modules it ships with, and reaching that from nothing is a project of its own; the frozen
fork is the whole of the plan.

## Forking libtsm rather than writing a terminal

`libkvt` is a hard fork of libtsm 4.7.1, kmscon's VT100–VT520 state machine, rebranded `tsm_` to
`kvt_`. A terminal emulator is a decade of edge cases — charsets, the alternate screen, DEC
private modes, wrapping rules that differ between terminals that both claim VT100 — and none of it
is a place to be original. What is original here is the boundary, not the parser.

A *cell* is the record for one character position on the screen: the character and its colours
and attributes. `libkvt` keeps libtsm's own cell record internally, and produces the toolkit's
shared cell type — `KtuiCell`, defined in `libkcell`'s `kcell.h` (see
[C libraries](../05-developer/c-libraries.md)) — only when the screen is drawn. `kcell.h` refuses
a second cell type, but that refusal is about two libraries of the toolkit disagreeing, not about
a terminal's private screen buffer, which nothing outside the library ever sees. Upstream's cell
earns its place: it carries 24-bit colour, a
per-cell age that drives damage tracking, and a symbol-table handle that is what makes combining
characters possible at all. Reducing it to a `KtuiCell` at the boundary loses none of that until
the moment the screen is drawn.

The conversion happens in one file. `kvt_grid.c` is the render boundary and is where a terminal
cell becomes a `KtuiCell`. Three other files touch the toolkit, each for one narrow reason:
`kvt_term.c` maps `KT_K_*` key codes into the escape bytes a child expects; `kvt_unicode.c` asks
`ktui_wcwidth` so the library and the grid agree how wide a codepoint is; and `kvt_selection.c`
holds `kvt_ui_mouse`, which decides what a drag over a terminal means, because every program that
embeds the terminal state machine needs that decision and two copies would drift.

`kvt_grid.c`, `kvt_term.c` and `kvt_htable.c` (with its header) are the files carrying no upstream
copyright. Every other file in the library carries libtsm's: the grid is this tree's render
boundary, `kvt_term` is the screen-plus-state-machine-plus-child object upstream never had, and the
hash table was written here rather than carried.

Colour reduces to the palette's eight [slots](../06-reference/glossary.md) (named colour roles)
by nearest distance — one rule for the ANSI sixteen,
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

So the places are an overlay and not a column. `Ctrl+P` opens them over the file list as an
overlay that `Esc` closes, reaching the same list `kxdg_places()` gives the Start menu — every place, at full
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
covers `tty1` and the installer; its other screen drivers read a `tmux` session or a terminal run
under `brltty-pty`, and none of them reads anything the compositor draws. It carries a driver for
nearly every braille display, loaded by name from `/etc/brltty.conf`. `speech-dispatcher` is the
API a screen reader speaks and `espeak-ng` is the voice behind it. BrlAPI is linked by nothing here
and no KDOS surface talks to `brltty`, `speech-dispatcher` or `espeak-ng`. They are on the image
because they are useful to somebody at a terminal, not because the desktop is readable — see
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

**Editing a library rebuilds every port of KDOS's own software, not only its consumers.** The 24
ports under `src/` compile the `libk*` libraries into themselves, and each port's `build.sh` names
the libraries it uses as a glob under `$LIBS`. Working out which ports an edit reaches would need a
shell parser inside the package manager, so the whole of `src/libs` goes into each of those ports'
recipe hash instead. Upstream ports under `ports/core` are never affected. Each rebuild takes
seconds; the parser would be a second, quieter build system.

**The initramfs carries util-linux's `switch_root` and not toybox's.** toybox's applet
`chroot()`s into the new root rather than moving it onto the root of the mount namespace, which
leaves every process on the booted system chrooted, and the kernel refuses a chrooted caller a new
user namespace, so every rootless container would be refused. The toybox recipe compiles the applet
out, and the packaging step refuses to build an initramfs whose `switch_root` is toybox's. See
[Boot and init](../03-architecture/boot-and-init.md#switch_root).

## See also

- [Why KDOS](why-kdos.md) — the properties these decisions serve
- [Principles](principles.md) — the rules that fall out of them
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the pack decision as built
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor fork as built
- [Known gaps](../06-reference/known-gaps.md) — what these decisions leave undone
- [Glossary](../06-reference/glossary.md) — the terms these entries use
