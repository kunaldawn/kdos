# Decisions

The choices in KDOS that were genuinely close, with the alternative that lost and the reason. A
decision is listed here when someone reasonable would have chosen differently, so that a reader
who disagrees can see whether their objection was already answered — and so that nobody spends a
weekend rediscovering why the obvious option does not work.

Each entry states the question, what was chosen, and what was rejected.

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

## A second fork for the kiosk, not a mode of the first

**The question.** The console desktop composites character cells, and a Wayland client's surface is
pixels. A graphical application launched there needs *something* holding a display for it — an
output in memory it renders into, or a VT of its own. `kdos-comp` is already a compositor this
project owns — give it a kiosk mode, or take a second one?

**Chosen: a hard fork of cage 0.3.1**, MIT, in `src/desktop/kdos-cage`. Seven `.c` files, built on
`wlroots-0.20` — the branch this tree already pins for the labwc fork, so there is one wlroots to
keep current and not two.

**Rejected: a `--kiosk` flag on `kdos-comp`.** The compositor is a *desktop*: window management,
workspaces, tiling, the panel's foreign-toplevel feed, the phosphor pass, per-box identity, the
session lock. A kiosk is the negation of nearly all of it, and a flag that turns most of a program
off is a second program sharing a binary — with every code path in it now answering "and what does
this do in kiosk mode?". The parts a guest on a VT actually needs are the parts cage already is.

**Rejected: writing one.** It is the same argument the labwc fork made and it holds harder here,
because the job is smaller: a kiosk compositor is roughly three thousand lines of somebody else's
tested XWayland integration, output layout, seat handling and idle inhibition.

**And the compositing happens in a SEPARATE PROCESS, which is what makes the whole thing safe to
have.** One `kdos-cage --embed` per embedded window renders into a shared mapping and the session
puts the bytes in its cells. So `kdos-con` links no wlroots, no mesa and no pixel library at all: a
machine whose GPU driver is broken still boots into its desktop, and a graphical toolkit that
crashes takes one window with it rather than the session. A compositor built into the session would
have traded exactly that away for one fewer process.

**What the fork changed.** The name, in what a person sees. `security-context-v1`, so
`kdos-boxsock` can tag a box's socket exactly as it does under the compositor — one launch path for
a boxed application rather than two. And a background in the palette's deep colour, because a guest
that has not painted yet is otherwise a black rectangle in the middle of a phosphor screen.

**Upstream's internal names are left alone**, which is the one place this fork differs in style
from the labwc one: `cg_server` and `CAGE_HAS_XWAYLAND` still say cage. A fork whose identifiers
stop matching upstream's is a fork nobody can read a security fix against, and this one is small
enough that reading upstream's diffs by hand is the maintenance plan.

**What it costs.** A second wlroots consumer to move whenever wlroots breaks API, which it does
every release. The self-test compiles all seven files wherever wlroots exists, so that breakage is
a failed check rather than a four-hour build that ends in an error.

## One pack per application, not one image

**The question.** Roughly 180 graphical applications have to reach the medium. Ship them as one
container image, or as separate artefacts?

**Chosen: one signed image per application, over a small set of shared runtimes.** An install
carries only what was ticked. Installing an application disturbs nothing else, and a shared
runtime's pages are shared because the pack is mounted once.

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

## No application store

**The question.** Users need to find and install applications.

**Chosen: the Start menu lists what the medium already carries.** Every category shows the
application packs the medium would put there under an `ON THE MEDIUM` rule, search matches them
by name and summary, and a row is *open this* — the pack is installed if it is not, and the
application opens. The click that installs is the click that opens.

**Rejected: a store front end.** On a distribution whose medium *is* the software library, "where
do I get this" is not a question anyone has. What remains is disposal, and that belongs where the
readings already are.

## Release assets, not Git LFS

**The question.** The tarballs, the vendored bundles and the pack set are tens of gigabytes.
Where do they live?

**Chosen: GitHub release assets, with git holding only what identifies them.** Each artefact
class already carries its own content-addressed index — `sha256 =` in a recipe, a content hash
per pack in the signed `PACKAGES` — and no second manifest was added, because a second copy of a
hash is a second thing to drift. `make bootstrap` fetches them.

**Rejected: Git LFS.** A free account provides 10 GiB of storage and 10 GiB of monthly bandwidth,
shared across every repository the account owns. The tarballs alone exceed that, and the pack set
is 24 GB. Release assets have a per-file limit and no total-size or bandwidth limit.

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

## See also

- [Why KDOS](why-kdos.md) — the properties these decisions serve
- [Principles](principles.md) — the rules that fall out of them
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the pack decision as built
- [kdos-comp](../04-programs/kdos-comp.md) — the fork as built
- [Known gaps](../06-reference/known-gaps.md) — what these decisions leave undone
