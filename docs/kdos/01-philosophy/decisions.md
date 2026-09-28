# Decisions

This chapter records the choices in KDOS where a reasonable engineer could have gone the other
way: the compositor, the application catalogue, the C library, where upstream sources are kept,
how CPU optimisation is chosen, and about twenty smaller ones. It is written for readers who want to
know why the system has the shape it has, and for contributors about to propose a different
approach, whose objection may already have an answer here. Read [Why KDOS](why-kdos.md),
[How KDOS differs](how-kdos-differs.md) and [Principles](principles.md) first; the entries assume
the goals and rules set out there.

Each entry gives the conclusion first. Most then give the question it answers, the alternatives and
why they were not taken, and what the choice costs; an entry leaves a part out where there is
nothing more to say about it than the conclusion already does. The entries link to the chapters that
describe the mechanism as built. The final section, [Narrowings](#narrowings), collects smaller
decisions that look like missing features.

## Index

| Decision | Conclusion |
|---|---|
| [The compositor](#the-compositor-is-a-frozen-fork-of-labwc) | A frozen hard fork of labwc 0.20.0, never merged from again |
| [Application delivery](#native-applications-on-the-medium-a-store-that-builds-the-rest) | The medium carries native ports; podman builds catalogue applications on request |
| [Application packaging](#one-pack-per-application-not-one-image) | One artefact per application over shared runtimes, never one image |
| [The base distribution in boxes](#debian-inside-boxes-not-alpine) | Debian trixie, with Alpine carried as a scratch base |
| [The host C library](#musl-as-the-host-c-library) | musl, which forecloses runtime CPU dispatch |
| [The host desktop](#no-kde-gnome-or-any-existing-desktop-on-the-host) | A desktop written for this system; KDE's and GNOME's applications, never their shells |
| [Where upstream sources live](#upstream-archives-are-content-addressed-release-assets) | Release assets named by their sha256, fetched by `make fetch` |
| [CPU optimisation](#-march-measured-per-machine-not-chosen-for-a-population) | Measured per machine by `kdos march`, never a shipped feature level |
| [The vulnerability database](#alpines-security-database-not-nvd-or-osv) | A vendored, pruned copy of Alpine's secdb, answered offline |
| [Recipe format](#the-build-shell-lives-beside-the-recipe) | Two files: parsed metadata, plus ordinary bash |
| [Binhost signing](#signing-the-index-not-every-package) | One signature over the index; a signature file holds several |
| [The ASCII demo](#freezing-a-demo-rather-than-writing-one) | A frozen hard fork of the AA-project's `bb` 1.3rc1 |
| [The terminal state machine](#forking-libtsm-rather-than-writing-a-terminal) | `libkvt`, a hard fork of libtsm 4.7.1 |
| [The file chooser](#one-file-chooser-at-one-width) | One program at 64×22 for portal and native callers alike |
| [Narrowings](#narrowings) | Small decisions that read like gaps |

## The compositor is a frozen fork of labwc

KDOS runs a frozen hard fork of the labwc 0.20.0 Wayland compositor. `src/desktop/kdos-comp` is
upstream's source, imported whole, renamed, and never merged from again. `KDOS-FORK` at its root
records the upstream tarball and its sha256. KDOS's additions live in eighteen files named
`src/kdos-*.c`, and the upstream files carry small hooks marked `/* KDOS */` (or `# KDOS` in a
`meson.build`), so `grep` finds every point where the fork touches upstream code.

The question was what to do about a compositor that needs things no existing one offers: the
phosphor shader (the pass that renders the screen as a CRT in the current accent), a wallpaper the
compositor owns, a frame-timing channel, an idle policy and per-box client identity. The choice was
to write one on wlroots or to take an existing one.

A compositor written from scratch was rejected because window management, session lock, screen
capture, the clipboard, the input-method relay, Xwayland integration and security-context
filtering are not what this project is about, and each is a protocol whose semantics only show
under real clients. Taking labwc's implementation brings parts that people outside this project
have exercised for years — both generations of the capture protocols (`wlr-screencopy` and
`ext-image-copy-capture`), both data-control managers, the text-input and input-method relay — and
leaves the effort for the parts that are specific to KDOS.

A fork was chosen over a patch series because a `.patch` file against a compositor becomes a merge
conflict at the next upstream release, and these additions would not be accepted upstream: they
are one distribution's opinions. Freezing means the source in the tree is the source that builds,
readable in a pager, with no patch step between the two.

The cost is that upstream fixes do not arrive. A security fix in labwc has to be read and applied
by hand. That is accepted; the alternative is maintaining a compositor outright.

See [kdos-comp](../04-programs/kdos-comp.md) for the fork as built.

## Native applications on the medium, a store that builds the rest

The installation medium carries native ports of the applications a machine needs with no network: a
browser, an office suite, media and graphics tools, maps, an offline library, and specialist tools
for CAD, electronics, software radio, science and amateur radio. A machine that never sees a network
has only what its media carry, so these are compiled like every other port and ship in the root
filesystem; [The ports catalogue](../06-reference/ports-catalogue.md) lists them by group. For
everything else, the medium carries a catalogue, and podman builds what somebody asks for. A
catalogue row is a parent chain of apt packages: installing a row builds a podman image per row in
its chain, each `FROM` the one below, and creates a [box](../06-reference/glossary.md) (a rootless
podman container one application runs in) over the top one. The catalogue file installs to
`/usr/share/kdos/appstore/catalogue`, and it is what the store, kinstall (the installer) and `kdos
app` all read, so adding an application to KDOS is one line in a text file. `kdos-store`, the
panel's graphical catalogue, and kinstall both offer the catalogue by group — seven of them, from
one application each in `essential`, `creative`, `make` and `games` to six in `science` — so
choosing the six science applications is one choice rather than six.

Carrying every catalogue application prebuilt on the ISO, as
[packs](../06-reference/glossary.md#pack) (the application images described below), was rejected: it
puts every application on every medium whether or not it is ever launched, costs an hour of building
to add one row, and needs a release channel to push the whole set through.

The choice has three costs.

1. A store install is unsigned. It pulls its base image from a public registry and its packages
   from Debian's archive, and nothing in `/etc/kdos/keys` vouches for either. `kdos-box create`
   prints exactly that about an OCI base, and it is true of every application built this way. The
   claim that everything is verified holds for an imported set and for nothing else.
2. Installing needs a network, and minutes of apt.
3. A live session keeps less of what it installs. `$HOME` is on the boot overlay there, so the
   container store runs through fuse-overlayfs, and what a store install builds is written under
   `$HOME`, held in memory and lost at power-off unless the session has a persistence store (see
   [Getting started](../02-user-guide/getting-started.md#keep-what-you-change)). A pack box's
   writable layer is always a tmpfs there, persistence store or not, because the kernel refuses an
   overlay upper directory on overlayfs, so whatever a box writes outside the home directory is gone
   at shutdown. See
   [Known gaps](../06-reference/known-gaps.md#a-live-session-cannot-create-a-persistent-box).

That is why import exists and why the pack format stays. `kdos-appbox export <file.ktar>
<id|group>...` writes the built images as packs with an index, signed when `KDOS_PACK_KEY` names a
readable key and saying plainly that the index is unsigned otherwise. `kdos-appbox import
<file.ktar> [<id>...]` stages each pack through `kdos-packd`, the root daemon that mounts packs,
which hashes and signature-checks it where it mounts it; with no names it imports the whole set the
archive carries. An imported application is better verified than a store-installed one and needs no
network. kinstall imports a set it finds on a stick, but into the live session rather than onto the
new disk (see [Installation](../02-user-guide/installation.md#8-applications)). The pack format is
how a set is carried, not how software is distributed.

## One pack per application, not one image

Each application is its own artefact over a small set of shared
[runtimes](../06-reference/glossary.md) (layers holding a toolkit or a family of libraries that many
applications sit on): a podman image per
catalogue row when the store builds it, and a [pack](../06-reference/glossary.md) per row when a
set is exported. Installing an application disturbs nothing else, and a shared runtime's layers are
stored once however many applications sit on it.

The question was how the catalogue's 73 applications (the `app` rows in
`src/packages/kdos-appbox/catalogue`) reach a machine: as one container image, or as separate
artefacts.

A single image was rejected because it puts every application on every install whether or not it
is ever launched, and it turns adding one application into a rebuild of the whole image.

The alternative also hits two limits that have nothing to do with size. Overlayfs cannot add a layer
to a mounted overlay, so installing an application into a shared box would mean restarting a
container other applications are running in. And a hundred lower directories do not fit in the 4096
bytes `mount(2)` allows for its option string. Per application, a stack is two to four layers — the
base, none, one or two runtimes, and the application — and the pages of a shared runtime are shared
between boxes because each pack is mounted once.

The cost is one `conmon`, podman's container monitor, per running application rather than one for
all of them, and only for applications that are open. See
[Packs and boxes](../03-architecture/packs-and-boxes.md) for the stack as built.

## Debian inside boxes, not Alpine

The application catalogue is built on `debian:trixie-slim`, with every row installed from a pinned
Debian snapshot archive.

The question was which base distribution the catalogue should use. KDOS itself uses musl as its C
library and would pair naturally with Alpine.

The catalogue decided it. Its purpose is to offer the strongest free application in each segment
it covers — office, creative work, development, science, CAD and 3D printing, games — and Debian
packages and patches nearly all of them, where Alpine's repositories lack a number of them. The one
application the catalogue carries that Debian does not package, VSCodium, is fetched as a `.deb`
from its own releases by a `deb` line in the catalogue, and apt resolves its dependencies. An
all-musl system would be more consistent and its images far smaller, and neither buys anything for
a user who wants to open a CAD file.

The catalogue's `snapshot = auto` line pins the Debian archive to the date the base image itself
was built from, so the packages installed on top cannot disagree with the root filesystem under
them. A literal timestamp pins it explicitly, and `off` reads the live archive and gives up
reproducibility.

Alpine is present all the same, as a second base pinned to `alpine:3.24.1`: about 3 MB of busybox
and musl, with no packages added. A clean scratch userland that needs no network is worth one row,
and Alpine is the one non-Debian root filesystem small enough that carrying it costs little.

The cost is size, and it is accepted: a Debian box is much larger than the same program would be
on Alpine. Every generated `apt-get install` passes `--no-install-recommends` to keep it from
growing further.

## musl as the host C library

The host C library is musl.

The question was which C library the host is compiled against, and the alternative was glibc, which
the Debian base of the boxes uses. musl was chosen because it is small, readable and close to the
standards, which suits a system meant to be understood in full.

musl closes some doors, and they are stated here rather than discovered later. It has no
`glibc-hwcaps` mechanism, so runtime dispatch for CPU optimisation — building several variants of
a library and letting the loader pick one — is not available, which is one reason
[`kdos march`](../04-programs/kdos-command.md#kdos-march) measures per machine instead. Some
upstream code assumes glibc extensions and needs a build flag or a patch. And a header warning that
glibc does not emit can turn an upstream `-Werror` build fatal.

The application catalogue is glibc, inside boxes built on the Debian base. That is the purpose of
the [ring](../06-reference/glossary.md) boundary: the host and the boxes need not share a C library.

## No KDE, GNOME or any existing desktop on the host

The host runs a desktop written for it, in which every surface KDOS paints is a character-cell
grid: the panel and all its surfaces, the file chooser, the resource monitor, the terminal, the
lock screen, the installer, the boot splash and `tty1`. The compositor is the one place with pixels
of its own. It links `cairo` and `pangocairo` and draws titlebars, the root menu and the
window-switcher display with pango, at a size matched to the grid. An application, native or in a
box, draws whatever its toolkit draws.

The question was why KDOS does not run one of the complete desktops that exist. It has two
reasons: the cell grid is the project's identity, and keeping it keeps every large toolkit out of
the desktop itself, which is what makes compiling and reading the desktop in one sitting tractable.

KDE Plasma on the host was the serious alternative. It was rejected because it would put Qt and
KDE Frameworks, a body of code larger than the rest of the host combined, under the desktop's own
surfaces, and bring a second session with its own daemons, portals and lock screen.

This does not reject KDE's or GNOME's applications. Dolphin, Kate, Okular, Kdenlive, GIMP and
others are ported natively with their toolkits and run as ordinary clients of `kdos-comp`, and the
catalogue carries more on the shared `rt-kde` and `rt-gtk` runtimes. None of them needs Plasma or
GNOME Shell running. See [Principles](principles.md#toolkits-are-for-applications-not-the-desktop)
and [Packs and boxes](../03-architecture/packs-and-boxes.md).

## Upstream archives are content-addressed release assets

Upstream source archives are not kept in git. Every source file a recipe names by a `sha256 =`
line is a release asset in the GitHub repository `kunaldawn/kdos`, and the asset's name is the
file's own sha256 — the same string as the recipe line. The exception is a small set of upstream
files that git does carry, such as bash's and readline's patch levels, the IANA registries,
`certdata.txt` and a Tesseract language model; those are never archived. The assets fill releases
in order: `sources-001`, then `sources-002` once the first holds 1,000 files, and so on. The
committed file `ports/sources.idx` records which release holds each hash, so a file's address is

```
https://github.com/kunaldawn/kdos/releases/download/sources-<NNN>/<hash>
```

The current recipes name 2,489 distinct files, 38.6 GiB in total: 39 that git carries, and 2,450
that belong in the archive, of which the index names 1,191. Sixty of them exceed the 100 MiB a push
to github.com refuses; they appear 74 times across the port directories, because a file such as the
LLVM source tarball serves several ports. The largest, `texlive`'s texmf tree, is 4.6 GiB. In all
the index names 1,678 files. A file enters it when `ports/publish` uploads it, and no line is ever removed, so
the index also names the older versions that earlier commits use. `KDOS_SOURCES_REPO` names a
different archive repository, and `KDOS_SOURCES_BASE` a different download base; setting
`KDOS_SOURCES_BASE` empty makes `make fetch` skip the archive and go from the local cache straight
to upstream.

The hash is the identity and the URL is advisory. A recipe names contents, not a location, so a
file that verifies is the file the recipe meant whether it came from the archive, from upstream or
from a mirror added in ten years, and none of those changes a commit. Two different upstream
releases published under one filename cannot collide, and GitHub, which rewrites asset names
containing characters outside `[A-Za-z0-9._-]`, never has to rewrite a bare hash.

Filling releases in order keeps their number as small as the file count allows. A GitHub release
holds at most 1,000 assets and has no limit on their total size, so 1,700 files need two releases
and 50,000 need fifty. Deriving the release from the hash instead would need no index, but hashes
are uniformly random, so every release such a scheme divides into exists from the first upload, and
a scheme with few of them fills within years. The index is what the ordering costs: one committed
line per file, `<hash> <NNN> <port>/<file>`, which `ports/publish` appends only after GitHub reports
the uploaded asset's digest equal to its name. The index is append-only like the archive, so the
newest index names every file ever archived, and an old checkout can be fetched with it
(`ports/fetch --tree <dir>`).

The archive is append-only. An asset whose digest matches its name is never replaced or deleted,
because replacing one would silently change what an old commit builds; a checkout from five years
ago finds the bytes it was written against after upstream has moved or gone. For each KDOS
release, `ports/publish --freeze <tag>` attaches `sources.sha256`, the list of every hash that
tag's recipes name, to the release of that tag, creating it as a draft when it does not exist. The
list is a convenience, one file that says what the release needs. What pins the hashes is the tag
itself: its recipes carry them, and git cannot change those without changing the tag.

The archive's releases share the repository's release page with the KDOS releases. Each one's
notes list every file it holds, as `<hash>  <port>/<file>` lines, and they are created with
`make_latest` off, so "latest" always means a KDOS release. GitHub's immutable releases stay off
on the repository: the setting applies to every release in it, and it would freeze an archive
release at its first publication, after which no source could be added to it.

The first cost is that a clone alone does not build. `make fetch` has to run once after a clone
and again after a recipe changes, and it is the only build step that uses the network. It takes
each file from the local cache or the archive, and falls back to the upstream URL in the recipe's
`source =` line, so a file missing from the archive, or an archive that cannot be reached, costs a
download from upstream rather than a failed build, as long as upstream still has the file. See
[Where sources come from](../05-developer/developing.md#where-sources-come-from) for the order it
looks in, and [How KDOS is built](../05-developer/how-kdos-is-built.md) for where `make fetch`
sits in the build as a whole.

The second cost is on the publishing side. A new or bumped source has to reach the archive, by
`ports/publish`, before the commit naming it is pushed, or that commit builds on the machine that
wrote it and nowhere else. The pre-push hook in `script/hooks/` enforces this by refusing a push
whose recipes name a hash the archive cannot be shown to hold. See
[Writing ports](../05-developer/writing-ports.md#publishing-sources) for the procedure.

Git LFS would make a clone the whole input to a build, and it is not used because the sources do not
fit in it. A free account has 10 GiB of LFS storage and 10 GiB of monthly bandwidth, shared across
every repository the account owns. The current sources take 38.6 GiB, nearly four times that
storage, on their own, before any older version the history names, and a single clone uses most of a
month's bandwidth. Past the allowance LFS reads are blocked outright, not slowed, so a repository
that depends on LFS stops checking out. Release assets carry no total-size or bandwidth limit and
allow 2 GiB per file. Plain git objects are not possible at all, since sixty of the files exceed the
push limit.

## `-march` measured per machine, not chosen for a population

CPU optimisation is measured on the machine that will run the result.
[`kdos march`](../04-programs/kdos-command.md#kdos-march) builds a port twice, once with the
highest x86-64 feature level the CPU's `/proc/cpuinfo` flags allow, runs that port's own benchmark
against both builds, and keeps the flags only where the median win exceeds 3 per cent plus the
machine's own measured noise. The verdicts go to a ledger,
`/var/lib/kdos/march.ledger`, which `kdos march report` prints: kept, reverted and unmeasurable.

A port with no `bench =` line in its recipe is unmeasurable, never a winner; six ports under
`ports/core` carry one. Each measurement takes the median of five runs by default, because a single
run measures the scheduler and a mean measures the worst outlier. The noise floor is the larger
spread of the two builds' own runs, so a busy or thermally throttled machine has to see a larger win
before it keeps the flags.

Shipping a feature level was rejected because published benchmarks of `x86-64-v3` show both gains
and regressions: some programs run faster and others slower, and some draw more power for the same
work. A distribution that shipped v3 everywhere would ship those regressions and never learn of
them. Runtime dispatch, which would choose between variants at load time, is not available on musl
(see [musl as the host C library](#musl-as-the-host-c-library)).

Because KDOS rebuilds itself on the machine it runs on, rebuilding per machine is available here in
a way it is not for a binary distribution. That turns "which flags" into "did they help on this
machine", which is a question with a measurable answer.

## Alpine's security database, not NVD or OSV

[`kdos cve`](../04-programs/kdos-command.md#kdos-cve) answers from a vendored, pruned copy of
Alpine's security database, `src/packages/kdos-tools/secdb/secdb.txt`, installed to
`/usr/share/kdos/secdb.txt`. It is about 260 KiB: 4,099 fix records for 798 packages, merged from
twelve Alpine branches (`main` and `community` for v3.19 to v3.24) by `secdb/vendor.py`. It is
committed and diffable, so the answer needs no network.

The question it answers is narrow and exact: is the version KDOS pins older than the version
Alpine records as fixing a CVE. That is a version comparison with libkpkg's `kp_vercmp`, the same
comparator `kdos-portup`, the port-version checker, uses, not a scan of binaries.

NVD was rejected because its data feeds are retired in favour of an API, which needs a network;
OSV because it carries the same facts in a far larger download. Alpine is the closest
distribution to KDOS that publishes machine-readable security data: musl, the same upstream
tarballs, comparable version pins.

The limit is stated with every run. The database is keyed by Alpine package names, so a port
named differently needs a `secdb = <alpine-name>` line in its recipe, and a CVE fixed in a version
Alpine never shipped is invisible. A package with no entry is reported unknown, never clean. The
data ages, so `kdos cve` warns once it is more than 180 days old, and `ports/update --cve` is the
separate online cross-check.

## The build shell lives beside the recipe

A port is two files. `kpkgbuild` is declarative metadata that is parsed and never sourced;
`build.sh` beside it is ordinary bash. `bash -n`, shellcheck, syntax highlighting and `git diff`
all work on it, and no parser has to understand shell.

An argument-vector list with no shell at all would fit most ports, but a minority need heredocs,
loops, redirects, globs and command substitution, and a format that cannot express those cannot
build them.

Embedding a shell interpreter in `kpkg`, the host package manager, was rejected because there is no
embeddable evaluator: the candidates are either parsers that do not execute, or programs with their
own `main()`, which would mean vendoring tens of thousands of lines of third-party C into a tree
that vendors almost none. It would gain nothing either, since bash is in the sysroot (the tree the
host is compiled into) before `kpkg` is compiled and ships on the target regardless.

A single file with the shell carried inline under INI-style section headers was rejected on a
concrete case. The `rust` port's `build.sh` writes a `config.toml` through a heredoc whose body
contains a line reading exactly `[build]`, the start of a TOML section. A recipe format with
`[build]` as one of its own section headers would read that line as the start of its own section.

See [Writing ports](../05-developer/writing-ports.md) for the format as built.

## Signing the index, not every package

A [binhost](../06-reference/glossary.md) (a directory of prebuilt host packages with a signed
index) is protected by one Ed25519 signature over its index, `PACKAGES.sig` beside `PACKAGES`.
The index carries every package's SHA-256, so one signature covers all of them transitively. A
per-package `<file>.sig` sidecar exists for the separate case of a package travelling on a USB
stick with no index beside it.

Signing every artefact as the primary mechanism was rejected because it multiplies the work and
the number of things that can be individually wrong without improving what is proven.

A signature file holds any number of signatures, one per line, and verifies when at least one line
verifies against a key in the local keyring. During a key rollover both keys sign and a client
trusting either keeps working. Adding that to a format that holds only one signature would be a
format change every client has to understand. The key id inside a signature is a label, not a
selector: every line is tried against every key in the local keyring and against nothing outside
it, so the id can never supply a key. See [Packaging](../03-architecture/packaging.md).

## Freezing a demo rather than writing one

The ASCII-art demo, `kdos-bb`, is a frozen hard fork of the AA-project's `bb` 1.3rc1, recorded in
`src/packages/kdos-bb/KDOS-FORK`. Of the 81 files upstream ships, it keeps the sources and headers
the binary is built from, the three `.s3m` tracker modules, and the authors' own credits scroll; the
autotools apparatus is replaced by a static `aconfig.h` and a `build.sh` that calls the compiler.
The fork's `src/` holds 44 C sources, one of them `kdostux.c`, an image generated by `genimg.py`,
and 16 headers, one of them `aconfig.h`.

`KDOS-FORK` lists every change the fork makes, and they fall into two groups.

Some changes are needed for the demo to run here. `clear_zbuff()` clears exactly the `sizeof(int)`
per cell it allocates; clearing `sizeof(long)`, double that on x86-64, corrupts the heap. The
message scroll moves overlapping ranges with `memmove`, because `memcpy` on them is undefined and
musl does not tolerate it. `REGISTERS(n)` expands to nothing, because the x86-32-only `regparm`
attribute warns on every declaration here. The drawing is paced on a deadline grid of one frame per
16 ms, the mixer runs on its own thread so the music does not starve while a frame is drawn, and
the scene clock follows the music player so the two stay in step. Each frame is wrapped in
synchronized output (DECSET 2026), so a terminal that supports it, `kdos-term` included, draws
whole frames rather than halves of two.

The rest change what the demo shows and how it starts. It starts by itself, with `-nosound` and
`-mixer` as flags in place of upstream's two start-up questions; the flash words, the closing text
and two logo beats carry KDOS's mark; and the closing text turns its own pages in time with the
last module. Every contributor line and every Special Thanks in the credits is upstream's,
unchanged.

A demo written from scratch was the alternative. `bb` is a set of scenes paced against the three
tracker modules it ships with, and reaching that from nothing would be a project of its own, where
the fork needed only the changes listed above. See [kdos-bb](../04-programs/kdos-bb.md).

## Forking libtsm rather than writing a terminal

`libkvt` is a hard fork of libtsm 4.7.1, kmscon's VT100–VT520 state machine, with its `tsm_` and
`shl_` prefixes renamed to `kvt_` and `kvt_shl_`. A terminal emulator is a decade of edge cases —
character sets, the alternate screen, DEC private modes, wrapping rules that differ between
terminals that both claim VT100 — and none of it is a place to be original. What is original here
is the boundary to the toolkit, not the parser. Like the compositor, the fork is pinned and does
not rebase onto upstream.

A *cell* is the record for one character position on the screen: the character, its colours and its
attributes. `libkvt` keeps libtsm's own cell record internally and produces the toolkit's shared
cell type — `KtuiCell`, defined in `libktui`'s `ktui.h` — only when the screen is drawn. `libkcell`
paints `KtuiCell`s directly rather than defining a cell type of its own, but that is about the
toolkit's libraries agreeing with each other, not about a terminal's private screen buffer, which
nothing outside the library sees. Upstream's cell is kept because it carries 24-bit colour, a
per-cell age that drives damage tracking, and a symbol-table handle that makes combining characters
possible. The conversion to a `KtuiCell` happens at one render boundary, once per frame, over the
runs the screen reports as changed, so none of that is lost before the screen is drawn. The library
reaches the toolkit only through narrow calls: key codes, character width, mouse selection, and
libkbase's base64 decoder for OSC 52 clipboard writes.

libtsm borrows an LGPL hash table from CCAN, and the fork replaces it with one written here,
because `libkvt` is MIT and a library that put relinking obligations on every binary linking it
could not be carried.

The terminal's colours reduce onto the palette's [slots](../06-reference/glossary.md) (named colour
roles) by the same rule as every other surface, so `kdos theme` recolours a terminal too; the rule
is described with the library in [The C libraries](../05-developer/c-libraries.md#libkvt).

The cost is the compositor's: upstream fixes do not arrive, and are read and applied by hand. See
[The C libraries](../05-developer/c-libraries.md#libkvt) for the library as built.

## One file chooser at one width

The chooser a boxed application reaches through the FileChooser portal and the chooser this
desktop's own programs open are one program, `kdos-pick`, at one size: 64 columns by 22 rows. The
portal does not get a wider one to hold a sidebar column.

The width is not a free parameter. Laid out inside a classic 80-column by 24-row terminal, a
64-column dialog leaves eight cells of ground either side of it and two rows below it for the
taskbar. A sidebar wide enough to read a place name is about sixteen more columns, and a chooser
needing 80 columns would be a chooser with no frame, no ground and no room for the taskbar at that
size. The chooser's [goldens](../06-reference/glossary.md) (committed reference frames of a
surface, compared byte for byte by the tests) are also taken at 56 by 24, where the dialog shrinks
to the screen.

A third column is what a sidebar would cost. When the selected row is an image, the chooser
already carves a preview pane from the right-hand third of the list, so a sidebar would take its
width from the names: about thirty cells for a filename, in the window whose purpose is showing
filenames.

So the places are an overlay and not a column. `Ctrl+P` opens them over the file list, and `Esc`
closes them. The list starts with the places `kxdg_places()` gives the Start menu and adds the
recently visited directories that `zoxide` reports, every place at full width, and nothing is taken
from the names while it is closed. A boxed application's Open and Save get exactly what a native one
gets, which is the other half of the decision: two dialogs of two widths would be two layouts to
maintain, two sets of reference frames, and two answers to how wide a chooser is.

## Narrowings

A narrowing is a decision that reads like a gap. The thing is not there, and it is not there because
a smaller answer was chosen over a larger one. Each of these is too small for a full entry of its
own, and each has the same shape: what was asked for, what is built instead, and what the larger
answer would have cost.

### Drops reach only the trash

A drop on the desktop reaches the trash and nothing else. Dropping a file onto a folder icon
would be a move, and a move across filesystems is a copy and an unlink that can half-succeed. A
desktop offering the gesture has to say what it did with the file when the second half failed, on
a surface with nowhere to say it. The trash is the one target whose failure mode is that nothing
happened.

### Screens are ordered, not placed

Screens are placed edge to edge in list order, not at coordinates. `kdos-display` lays the
outputs out from `x = 0` with their tops aligned, in the order of its list, and `[` and `]` move a
screen along it. A vertical arrangement, an overlap and a deliberate gap cannot be expressed. What
people reach a screen tool for is which screen is left of which, and an order answers that in a
list; a geometry answers it with a canvas, a drag, a snapping rule and a validity check for the
arrangements that leave a hole.

### Tabs stack; they do not tile

The compositor's `AddToTabGroup` action folds the focused window onto the one behind it, and there
are no tile groups — two windows side by side that move, resize and minimise together. A window's
`tiled` state is a set of edges resolved against the work area, never against a neighbour, and
`libkwm`, which computes window rectangles, has no notion of a neighbour at all. A group is a
relation between windows, which is the one thing the window model does not hold, so a tile group
would reuse none of the machinery a tab stack reuses. See
[The window model](../03-architecture/window-model.md).

### One font for every output

The desktop uses one font for every output. `~/.config/kdos/comp.conf` holds `chrome_font` and
`panel_font`, and each is one value for every output, so they are right on a machine with one screen
and wrong on two of different densities. `kdos-style`'s font page offers fontconfig's monospace
families and writes those two keys, which the desktop reads at the next login; setting the face on
every output keeps two screens agreeing rather than letting each be right. A per-output font is a
different design — a font per connector, a picker that asks which, and a cell size that changes
under a window as it is dragged across the boundary — not a missing call.

### No ReGIS or Tektronix

The terminal implements neither ReGIS nor Tektronix graphics. They are vector graphics protocols
from DEC and Tektronix hardware, and nothing in the catalogue emits either. The three raster
protocols the terminal implements (sixel, the kitty graphics protocol and iTerm2's inline images)
are what current programs use. See [kdos-term](../04-programs/kdos-term.md).

### Braille and speech do not read the desktop

Braille and speech are ports, and neither reads the desktop. `brltty` reads `/dev/vcsa` by
default, so it covers `tty1` and the installer; its other screen drivers read a `tmux` session or a
terminal run under `brltty-pty`, and none of them reads anything the compositor draws. It carries a
driver for nearly every braille display, loaded by name from `/etc/brltty.conf`.
`speech-dispatcher` is the interface a screen reader speaks to and `espeak-ng` is the voice behind
it. No KDOS program links BrlAPI; the API server stays on for BRLTTY's own clients, such as
`brltty-clip`. No KDOS surface talks to `brltty`, `speech-dispatcher` or
`espeak-ng`. They are on the image because they are useful to somebody at a terminal, not because
the desktop is readable; see [Accessibility](../02-user-guide/accessibility.md).

### A tray menu is read once

A tray icon's menu is read when it opens and not watched while it is open. `kdos-shell`'s tray menu
listens to neither `LayoutUpdated` nor `ItemsPropertiesUpdated`, the two signals an application
sends when its menu changes (`src/desktop/kdos-shell/traymenu.c`). A menu is open for as long as
somebody is looking at it, and a menu whose rows move under the pointer activates the wrong row.
`AboutToShow` is sent first, which is where an application fills a submenu in; the layout is read
once its reply arrives, and that read is the only one taken. Following the signals would mean a
policy for every row that moves, appears or disappears under a pointer that is already on its way to
it. See [kdos-shell](../04-programs/kdos-shell.md).

### Invitations are read, not answered

An invitation in a message is read and never answered. The shipped `aerc` configuration sends
`text/calendar` parts through `kdos-part ics`, which runs aerc's own calendar filter: it prints the
event — summary, times, location, who was asked — and writes nothing anywhere. A filter runs every
time a message scrolls past, so one that imported would accept every meeting it was scrolled over,
and no `text/calendar` handler is registered for the same reason. Filing an invitation is manual
and works: `:save` the part out of the message and `khal import` it, and the day carries a mark in
the panel's calendar.

### No raw-block-device applications

Applications that need raw block devices are not in the catalogue: partitioners, drive-health
tools, recovery tools. A rootless container cannot do anything useful with a block device, and a
launcher that opens onto a permission error teaches somebody that the machine is broken. Those jobs
are native tools on the host, where the privilege is: `parted`, `gptfdisk`, `smartmontools`,
`testdisk` and `ddrescue` are ports.

### No compositor-private protocols

Applications requiring a particular compositor's private protocols are out. Spectacle, KDE's
screenshot tool, asks for KWin's own screenshot interface on Wayland and opens an error dialog on
any other compositor, so it is not in the catalogue. Screenshots are the host's `kdos-shot`, which
runs where the capture protocols are.

### Nothing downloads at run time

Nothing downloads a component at run time. `winetricks` is not in the catalogue, because it
fetches Windows runtimes from the network when it runs, and for the same reason Wine gets no
Microsoft core fonts: a Windows program that wants a specific proprietary font gets a substitute.
Once installed, nothing may depend on a download an offline machine cannot make.

### A library edit rebuilds every in-tree port

Editing a library rebuilds every port of KDOS's own software that builds from the tree, not only its
consumers. A port under `src/` with no `source =` line builds from the tree, and seventeen of the 24
ports there compile `libk*` libraries into themselves, each `build.sh` naming the ones it uses as a
glob under `$LIBS`. Working out which ports an edit reaches would need a shell parser inside the
package manager, so `kp_hash.c` puts the whole of `src/libs` into the recipe hash of every such port
instead. The test is the recipe and where it sits: the hash walks `src/libs` for every port with no
source (no `source =` line, or an empty one) whose directory has a `../../libs` beside it, which is
all 24 ports under `src/packages` and `src/desktop`. The two source-less ports under `ports/core`,
`musl-ldd` and `containers-common`, hash their own directory alone, and every other port under
`ports/core` is unaffected. Each rebuild takes seconds, while a parser would be a second build
system whose mistakes show only as missed rebuilds.

### util-linux switch_root in the initramfs

The initramfs carries util-linux's `switch_root` and not toybox's. Toybox's applet `chroot()`s
into the new root rather than moving it onto the root of the mount namespace, which leaves every
process on the booted system chrooted. The kernel refuses a chrooted caller a new user namespace,
so every rootless container would be refused, and the machine boots normally either way. The
toybox recipe compiles the applet out, and the initramfs step refuses to build an initramfs whose
`switch_root`, `mount`, `umount`, `losetup`, `dmesg` or `blkid` is toybox's. See
[Boot and init](../03-architecture/boot-and-init.md#switch_root).

## See also

- [Why KDOS](why-kdos.md) — the properties these decisions serve
- [How KDOS differs](how-kdos-differs.md) — how other distributions answer the same questions
- [Principles](principles.md) — the rules that follow from them
- [Packs and boxes](../03-architecture/packs-and-boxes.md) — the pack and store decisions as built
- [kdos-comp](../04-programs/kdos-comp.md) — the compositor fork as built
- [Writing ports](../05-developer/writing-ports.md) — the recipe format and publishing sources
- [How KDOS is built](../05-developer/how-kdos-is-built.md) — the build from a clone to an ISO,
  where the source archive and the recipe hash come into play
- [The ports catalogue](../06-reference/ports-catalogue.md) — every port the build and security
  decisions apply to, by group
- [Known gaps](../06-reference/known-gaps.md) — what these decisions leave undone
- [Glossary](../06-reference/glossary.md) — the terms these entries use

<!-- book-nav -->
---

*Part I — Introduction, chapter 4.* Previous: [3. Principles](principles.md) · [Contents](../README.md) · Next: [5. Getting started](../02-user-guide/getting-started.md)
