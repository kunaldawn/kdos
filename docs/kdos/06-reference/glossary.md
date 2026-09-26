# Glossary

The vocabulary this book uses, each term defined once, in alphabetical order. It is for any reader
who meets a word elsewhere in the book and wants its exact meaning here. Where a term has a general
meaning elsewhere and a narrower one in KDOS, the KDOS meaning is the one these pages use.

The last section lists words the book deliberately avoids, and what it says instead.

## Terms

**accent** — One of the eight palettes: phosphor, amber, ice, bone, norton, borland, perfect and
paper. Not a single colour but a small set of related values that every drawn surface resolves its
*slots* against. Stored as one word in `$XDG_CACHE_HOME/kdos/theme` (normally
`~/.cache/kdos/theme`), which is the entire theme state the desktop reads. See
[Theming](../02-user-guide/theming.md).

**alien app** — A graphical application that is not compiled by this repository. It is built on
your machine from a *catalogue* row, or imported as *packs*, and runs in a
*box*. See [Applications](../02-user-guide/applications.md).

**appbox** — The program that installs, launches and exports boxed applications, `kdos-appbox`.
Also, loosely, the whole mechanism by which alien apps run. See
[kdos-appbox](../04-programs/kdos-appbox.md).

**base** — The bottom of an application's image chain: a whole root filesystem rather than a
difference over another. The catalogue names two, `alpine` and `base` (Debian). A **base pack** is
a base carried as a pack; building with `KDOS_PACK_KDOS=1` also packs the KDOS root filesystem
itself as the base pack `kdos`.

**binhost** — A directory of prebuilt host packages with a signed index: a USB stick, an NFS mount
or any local path, never a URL. Optional, and one you make yourself; there is no public one. See [Packaging](../03-architecture/packaging.md).

**box** — A rootless container that one application, or one working environment, runs in. A
**store box** is created from the container image the store built; a **pack box** is composed from
a stack of mounted packs plus a writable layer. A box is a packaging and desktop boundary, not a
security boundary against you: it shares your home directory. See
[Packs and boxes](../03-architecture/packs-and-boxes.md#the-box) and
[The security model](../03-architecture/security-model.md).

**box profile** — `~/.config/kdos/boxes/<name>.conf`. Every key maps onto a container-engine flag
or onto something KDOS enforces itself, and the profile printer says which.

**build-config hash** — One of the two hashes that decide whether a prebuilt package is usable
here: the flags, the target, the C library and the compiler. Written `B:` in a package index. See
also *recipe hash*.

**catalogue** — The shipped list of every application KDOS knows how to build, as Debian packages:
`src/packages/kdos-appbox/catalogue`, installed as `/usr/share/kdos/appstore/catalogue`. Its rows
are bases, *runtimes*, applications, data sets and groups. See
[Packs and boxes](../03-architecture/packs-and-boxes.md#the-catalogue).

**cell grid** — The model every KDOS surface is drawn in: a two-dimensional buffer of character
cells, each with a character, a foreground slot, a background slot and attributes. Every surface
this project paints is one — the panel and all its surfaces, the resource monitor, the terminal,
the lock screen, the installer, the boot splash, `tty1` — drawn by `libktui` and handed to the
compositor as an ordinary Wayland surface. Two things on screen are not: the compositor's own
window chrome, drawn with pango at a size matched to the cell, and whatever an application in a box
draws for itself. See [the design language](../03-architecture/design-language.md).

**chrome** — The furniture around content *inside* a KDOS surface: the frame, the header band,
group headings, the button bar, scrollbars. Drawn in cells by `libkchrome`, so there is one
implementation of each. Distinct from *window chrome* — the titlebar, the root menu and the
window-switcher display — which the compositor draws itself, with pango. Also, *supervised
chrome*: the desktop programs the compositor starts and restarts.

**compose** — To build a pack box's overlay from its pack stack. Safe to repeat, reference counted,
and redone before every start because the overlay lives on a temporary filesystem.

**data pack** — A pack carrying a dataset rather than a program. Mounted read-only with execution
disabled, never composed into a box root. It reaches its consumers through *grafts*.

**delta** — A binary difference between two packages or two packs, taken over the uncompressed
form. Never trusted on its own: the rebuilt file is verified against the hash the signed index
already carries.

**dump** — One frame of a surface printed as text instead of drawn on a display, with `--dump`
(the characters) or `--dump-cells` (every cell's character, colours and attributes). What
*goldens* are made of. See [Testing](../05-developer/testing.md#goldens).

**fixture** — Recorded system state that a program can be pointed at instead of the live machine,
which is what makes readings and decisions testable. There are 38 fixture directories under
`testing/fixtures/`. See [Testing](../05-developer/testing.md#fixtures).

**glyph tier** — Which set of drawing characters a surface may use, chosen from the terminal's
capabilities. The middle tier exists because the console font has 512 glyphs, and a character it
lacks renders as a blank.

**golden** — A committed reference frame: a surface's *dump* compared byte for byte by the
self-test. Text frames catch geometry; cell frames catch colour as well. There are 193, under
`testing/goldens/`.

**graft** — A declared placement of a data pack's contents somewhere a consumer will look. There are
two namespaces, because the host's data directories are invisible inside a box. Recorded in a
manifest so removal is exact.

**host** — The KDOS system itself: everything compiled from this repository, as opposed to a box.
In a build context, also the machine you are building on.

**lane** — One of the two routes an application takes to a box. The **store** lane builds container
images from the catalogue on your machine, over the network; the **import** (or pack) lane installs
signed packs someone exported. Both are separate from host packaging. See
[Packs and boxes](../03-architecture/packs-and-boxes.md#two-lanes-one-box).

**medium** — The USB stick or disc image KDOS boots from. It carries the system and the catalogue;
applications are built on the machine that asks for them, or imported from packs.

**pack** — One application, runtime, base or dataset as a single signed file: an EROFS filesystem
image with a metadata blob, an icon, a signature block and a footer appended. Extension `.kpack`.
See [Packs and boxes](../03-architecture/packs-and-boxes.md).

**phase** — One stage of the build, a directory under `script/`. There are eight, run in sorted
order, each holding either numbered scripts or a package list (`packages.txt`). See
[The build system](../05-developer/build-system.md).

**phosphor pass** — The compositor's CRT-imitating shader over the whole desktop: a horizontal
bleed, a vignette, a faint floor so black is never quite black, and optional scanlines and
curvature. It runs only under the GLES2 renderer, so a virtual machine with plain graphics never
shows it. Not to be confused with *phosphor*, one of the eight accents. See
[kdos-comp](../04-programs/kdos-comp.md#the-phosphor-pass).

**port** — One piece of host software as this repository describes it: a directory holding a
`kpkgbuild` (the metadata, parsed and never run) and a `build.sh` (the build). There are three port
repositories, all in one format, holding 1,038 recipes: 1,014 upstream ports in `ports/core/`, 11
of ours in `src/packages/` and 13 in `src/desktop/`. See
[Writing ports](../05-developer/writing-ports.md).

**preflight** — `testing/preflight.sh`, the check that the tree's wiring is consistent — every
package resolves, every recipe parses, every script is valid — without building anything. See
[Testing](../05-developer/testing.md#preflightsh).

**recipe hash** — The other of the two package hashes: a hash over a port's recipe files
(`kpkgbuild`, `build.sh`, `postinstall.sh` and every patch), which for one of our own ports also
covers `src/libs`. Written `E:` in a package index. It decides what the build rebuilds. Not to be
confused with a *source hash*.

**rig** — The virtual-machine harness that boots a real KDOS image in QEMU, drives it with keys,
pointer and commands, and photographs the screen: `testing/vnc-shot.py` and the scripts around it.
See [Testing](../05-developer/testing.md#the-qemu-rig).

**ring** — Which of the three tiers a piece of software belongs to: core, desktop or outer. What
decides it is build cost. See [Architecture overview](../03-architecture/overview.md).

**root slot** — One of the two root filesystems, A and B, on an installed disk. One is live; an
update is written into the other and tried on the next boot, and `kdos-bootctl` selects and
confirms slots. See [Boot and init](../03-architecture/boot-and-init.md#ab-slot-selection).

**route** — A stable name for a place in the system, `verb.noun`, defined in `menu.conf` and opened
with `kdos menu summon`. It keeps working when the chord is rebound or the menu row moves.

**runtime** — A layer shared by many applications, holding a toolkit or a family of libraries, such
as `rt-gtk` or `rt-qt`. The catalogue names seven. An application row names a runtime or a base as
its parent.

**session** — A running desktop: the compositor, its supervised chrome, and the per-user services
under it. `tty1`'s login shell starts one through `kdos-desktop`, and no other tty does; a session
that fails to come up falls back to that shell's prompt rather than taking the terminal with it.

**shim** — A symlink in `/usr/local/bin` named after an application and pointing at `kdos-appbox`,
which looks at the name it was started by. It is what makes a boxed application an ordinary
command.

**slot** — A named colour role — accent, warning, error, text, mid, dim, surface, background — that
resolves against the current accent. Everything drawn takes its colour from a slot rather than
from a literal value. (For the A/B root filesystems, see *root slot*.)

**source archive** — The content-addressed store of every upstream source file the recipes name:
release assets of the GitHub repository `kunaldawn/kdos`, each named by its own sha256, in
numbered releases `sources-001`, `sources-002`, … filled 1,000 files at a time. The committed file
`ports/sources.idx` says which release holds each hash. Append-only. `make fetch`
reads it; `ports/publish` adds to it. See
[Where sources come from](../05-developer/developing.md#where-sources-come-from).

**source cache** — `ports/.srccache`, or wherever `KDOS_SRCCACHE` names: every source file `make fetch`
has verified, in the archive's layout, hard-linked into the port directories. It means a branch
switch or a second checkout downloads nothing twice.

**source hash** — The `sha256 =` line in a recipe, naming one source file by the SHA-256 of its
contents. It is the file's identity: a copy that matches is the right file wherever it came from.

**sprite** — A picture occupying whole cells, with its slot and sub-cell position encoded in the
cell itself, so the ordinary row diff is already its redraw mechanism. Always optional: every
caller draws a character fallback when none is available.

**surface** — One window or popup of the desktop drawn by KDOS: the panel, the Start menu, the
control centre and so on. `kdos-shell` answers to 53 names, which reach 52 programs: most of them
surfaces, and a few session helpers with no window of their own, such as the notification daemon
`kdos-notifyd`, the media watcher `kdos-mediad` and the network secret agent `kdos-netagent`. See
[kdos-shell](../04-programs/kdos-shell.md).

**tile** — A block of cells drawn as pixels, for content that genuinely cannot be a row of text.
Limited to a modest number of cells. It owns two slots and alternates between them, or its content
would never be presented.

**vendor bundle** — A tarball of a port's language dependencies (Rust crates, Go modules, Python
wheels, Haskell packages), generated by `make fetch` for ports with a `vendoring =` key so the build
needs no network. Generated reproducibly, so it has a stable *source hash*. See
[Writing ports](../05-developer/writing-ports.md).

**warmup** — Starting the boxes behind your pinned applications in the background at login, so the
first launcher click does not wait for a container to start.

**whiteout** — How a filesystem layer records a deletion. The overlay filesystem and a container
image archive use different conventions, and a pack built from the wrong one merges with the
deleted file still present.

## Words this book avoids

Keeping the vocabulary to one vocabulary.

| Not used | Instead |
|---|---|
| "app store" | *The store*, or `kdos-store`: it builds from the catalogue on your machine and sells nothing |
| "distro" in prose | Distribution |
| "just" | Say the thing without it. "Just run X" hides how much X is |
| "simply", "obviously", "of course" | If it were obvious the sentence would not be needed |
| "should work" | Say what was measured, or say it is untested |
| First-person decision narration | State the decision and its reason in the present |
| Past-tense narration of any kind | These pages describe the present. See [Principles](../01-philosophy/principles.md#documentation-describes-the-present) |
| "sandbox" for a box, unqualified | A box limits what an application can do to the *desktop*, not to your data |

## See also

- [Architecture overview](../03-architecture/overview.md) — where most of these terms live
- [Principles](../01-philosophy/principles.md) — the rules behind the vocabulary
- [Command index](command-index.md) — every command by name
- [Configuration](configuration.md) — every setting by name
