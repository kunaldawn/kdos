# Reading a package page

Most of this book is package pages, one for every port in the tree, generated from the port's
recipe each time the book is rendered. This chapter walks through one section by section, says
where each part of it comes from, and explains the three tiers of page: the full page for a port the book is built around, the
full page for a port with something unusual about it, and the compact card.

## Where a page sits

Each package phase, from `20_selfhost` to `60_kernel`, has a part of the book to itself, in the
order the build runs them. A port's page is in the part of the phase that installs it. When
a port is named in the lists of two phases, it is the first that installs it; the later phase finds
it installed and current, and skips it.

Inside a part, the pages are grouped into chapters by shelf, the subject directory under
`ports/core/` the recipe is filed in (`archiver`, `cli`, `network`, and the rest of the closed list
in `ports/shelves`). The shelves come in the order of `ports/shelves`, and KDOS's own programs,
filed under `src/` rather than on a shelf, have chapters of their own at the end of the part, one
per area of `src/`, named `src-<area>`, as `src-system`. Within a chapter, the pages are in the
order the build installs them: the order `kpkgdepends` resolves for the phase, every port after
everything it depends on.

A page is numbered by its chapter and its place in it, as in `36.1`, and has a fixed anchor,
`port-<name>`, so a link to a package from anywhere in the book reaches its page whatever its
number. The recipes no phase list names are in a chapter of the appendices: those kept on purpose
in `ports/unbuilt`, such as `texlive-doc`, and KDOS's own programs that a step script compiles
without kpkg, such as `kdos-installer`, which `10_bootstrap` builds by hand.

## The three tiers

Every port gets one of three kinds of page, decided by the recipe and by a short list kept with the
book:

**Tier A** is a full page, for the ports named in `docs/kfs/data/tier-a.txt`: the sources the
bootstrap scripts build, everything the first three package phases install, the desktop, every
program of KDOS's own, and the ports the rest of the system is built around. A Tier A page draws
its *About*, its flag explanations and its *Notes* from a hand-written note beside the recipe.

**Tier B** is a full page too, for a port that has something unusual about it: its recipe carries a
patch, it vendors its dependencies, its `build.sh` is longer than 30 lines, or 20 or more ports
depend on it. Such a port carries the same note, and its page says which of the four reasons
applies. Every Tier A and Tier B port in the tree has its note; a port that gains a Tier B reason
without one renders with a line saying the note is missing, and
[`kfscheck.py`](../../kdos/05-developer/testing.md#kfscheckpy) refuses it when the port is new or
changed.

**Tier C** is a compact card, for every other port. A card holds the same facts as a full page in
less space, and instead of spelling out the five steps kpkg takes, it links to
[Anatomy of a kpkg install](../04-chroot/kpkg-install.md), where those steps are explained once.
In the PDF, several cards share a page.

The note is a file named `kfs.md` beside the recipe, in `ports/core/<shelf>/<name>/` or in the
port's directory under `src/`. It has up to four sections, each optional: `## About`, a paragraph
or two on what the port is to KDOS; `## Flags`, one bullet per switch of its `build.sh` saying why
it is set; `## Patches`, a line for any patch that carries no description of its own; and
`## Notes`, the traps and ordering constraints a reader should know. Editing it changes the book
and nothing else: it is not part of the recipe's hash, so it never makes the port rebuild.

## The header

[zlib](#port-zlib) has a full page. Its heading is the port's name and version, `zlib-1.3.2`, with
its number in front, and under the heading comes the one-line `description =` from the recipe.

When the snapshot of the built tree was taken from a different version or release of the port from
the one its recipe now names, a note follows here: the size, time and contents further down then
describe the earlier build, and the next `make book-data` after a build will bring them up to date.

A table of facts follows:

| Row | What it says | Where it comes from |
|:--|:--|:--|
| Version | The version and the release, the recipe's own revision of that version | `version =`, `release =` |
| Shelf | The shelf and its one-line description; for a port of KDOS's own, *Source tree* and its directory under `src/` instead | the port's directory, `ports/shelves` |
| Phase | The phase that installs it, its title, and the port's place in that phase's build order | the phase lists, resolved as `kpkgdepends` resolves them |
| Licence | The SPDX licence expression | `license =` |
| Homepage | The project's home page | `homepage =` |
| Build time | In SBUs, as [Typographical conventions](conventions.md#build-times-and-the-sbu) defines them, and said to be measured on another machine than the SBU, or on one it does not name, when its figure was; the row is missing when the snapshot has no measurement | `docs/kfs/data/packages.tsv` |
| Installed size | The bytes of its regular files and how many files it installed | `docs/kfs/data/packages.tsv` |
| Recipe | A link to its `kpkgbuild` | the port's directory |

A port that builds from the same source before kpkg exists, in a script of `00_cross` or
`10_bootstrap`, has a box further down headed *Differs from the bootstrap build*. It links to the
chapter on that script and lists the switches only the bootstrap build uses and those only the
port's `build.sh` uses. gcc, binutils, musl and the GNU userland have one, because the bootstrap
builds them first with the cross compiler and the package phases build them again as ports.

## About, and why it is in KDOS

*About* is the sidecar note's own section. *Why it is in KDOS* is the comment written above the
port's name in the list of the phase that installs it, quoted as it stands, with a link to the list
file. A comment above a run of several names is shown on each of their pages, and says how many
names it covers. A port with no comment above it in its phase list has no such section.

## Sources

One entry per file the recipe's `sha256 =` lines name: the file name, its size, its SHA-256, the
upstream URL from `source =` when one names that file, and the address of the copy in the KDOS
source archive when `ports/sources.idx` has one. A vendor bundle, the tarball of a Rust, Go,
Python, Haskell or Node port's dependencies made in advance so that the build needs no network, is
marked as one. A port with no sources, such as most of KDOS's own programs, says that it builds
from its own directory. [Sources and the archive](../01-preparing/sources.md) explains each of
these.

## Dependencies

*Depends on* lists the ports the recipe's `depends =` names, each linked to its page. *Required by*
lists every port whose recipe names this one, and how many there are. A dependency is a port that
must be installed before this one builds; the package phases follow these lines to put every port
after what it needs.

*Optional, off in KDOS* follows on some pages. It lists the switches in `build.sh` that name a
feature whose library is itself a port in this tree, such as a `--without-` or `-DENABLE_...=OFF`
for something KDOS builds, as found by `testing/optdeps.py`. The page cannot say whether each one
is deliberate, or a feature switched off when the port was written because its dependency did not
exist yet; it shows them so that someone can find out. A switch the page's own notes explain is
marked as explained below.

## Patches

When the port carries patches, each is listed with a link to the file and the one-line
description from its `Subject:` header, followed by the rest of the patch's own explanation. A
patch with no `Subject:` is described by the sidecar note's *Patches* section when it has a line
for it, and shown as *undescribed* otherwise. kpkg applies no patch by itself: a patch is applied
by a `patch` command in `build.sh`, so the commands further down show where.

## Installation, and what kpkg does

*Installation* gives the single command that installs the port, the one kdosbuild runs for it,
from the top of the repository inside the build container:

```bash
script/chroot/exec.sh bash -c 'source script/phases/20_selfhost/phase.env && kpkg install zlib'
```

It enters the chroot, loads the environment of the phase that installs the port, and asks kpkg to
install it. [Entering the chroot](../04-chroot/entering.md) explains the first half and the next
section of a full page explains the second.

*What kpkg does* expands that one command into the five steps kpkg takes, as commands you could run
yourself in the chroot, with the port's own file names and paths filled in:

1. **Verify** every file a `sha256 =` line names against its hash, before anything is unpacked.
2. **Unpack** the first source into `$SRC`, with its top directory stripped, and any further
   tarball beside it in `$SRC_ROOT`, after emptying the port's work directory. Old copies of
   `config.guess` and `config.sub` are then replaced with automake's.
3. **Build**: the port's `build.sh`, shown verbatim, run under `set -e` from `$SRC`. On a Tier A
   or Tier B page whose note has a *Flags* section, the explanation of each switch follows the
   listing. The common switches of each build system are explained in the
   [Flag glossary](../17-appendices/flag-glossary.md), which the step links. A `postinstall.sh`, when the port has one, is shown here too, with a line saying it runs
   on the installed system after the package is adopted.
4. **Package**: what kpkg removes from and adds to the staged tree in `$PKG`, and the `tar` command
   that rolls it into `<name>-<version>-<release>.tar.xz`. The command shown carries the
   compressor settings of a package that is kept, `xz -9`; a package deleted again as soon as it is
   installed, which is every package of an ordinary `make build`, is compressed with `xz -0`, which
   gives the same contents in a larger file.
5. **Adopt**: kpkgadd places the files over the root, refusing a path another package owns, and
   records the package in the database under `/var/lib/kpkg/db`.

Every one of these steps, and what each refuses, is explained once with a real port in
[Anatomy of a kpkg install](../04-chroot/kpkg-install.md).

When the port directory holds other files `build.sh` reads, such as a configuration file or a
desktop entry, a line after the steps names them.

## Contents

What the package installed, as recorded by the build-state snapshot: its programs, the files under
a `bin` or `sbin` directory; its libraries, the shared and static ones under `/usr/lib`; and its
directories, the topmost directories no other package shares. *Short descriptions* gives the
one-line `NAME` description of each program that has a manual page in section 1, 6 or 8, and the
`Description:` line of each `.pc` file the package installs.

The HTML edition then lists every file the package installed, read from the package database of
the tree the book was rendered with. The PDF leaves the list out.

A port that no phase installs, or that the snapshot's tree had not reached, says so in place of its
contents.

## Notes

The last section of a full page is the sidecar note's *Notes*: what breaks if a line of the recipe
changes, an ordering constraint with another port, a trap someone has already fallen into.

## A card

[pv](#port-pv), a Tier C port, has a card. Under its heading and description, one line carries the
version and release, the licence, the build time when there is one, the installed size and file
count, the phase and the port's place in its order, and a link to the port directory. Then come the
homepage, *why it is in KDOS* when its phase list has a comment for it, the dependencies, the source
files with their sizes, and the one install command. Its `build.sh` follows verbatim, then the
programs, libraries and directories it installed, and in the HTML edition its full file list.

Everything that a full page spells out step by step, the verification, the unpacking, the
packaging and the adoption, applies to a card's port in exactly the same way. The card links to
[Anatomy of a kpkg install](../04-chroot/kpkg-install.md) for those steps rather than repeating
them for each of the ports on cards.

## The index

Every package is in the book's index under its bare name. So are the programs, configuration files
and ideas explained from the preparation through the chroot, in the kernel and image parts and in
System administration: a program under its name, a file under its full path (the paths sort
together under `#`, ahead of the letters), and an idea under its usual name, such as *A/B slots* or
*recipe hash*. Spellings of a name that differ only in case are one entry, so `pipewire` and
PipeWire lead to the same pages. The HTML edition has the index as a page of its own; the PDF has it
at the end of the book.

## See also

- [Anatomy of a kpkg install](../04-chroot/kpkg-install.md): every step a page's commands stand for
- [Typographical conventions](conventions.md): paths, placeholders and the SBU
- [Writing ports](../../kdos/05-developer/writing-ports.md): the recipe format the pages are generated from
- [Packaging](../../kdos/03-architecture/packaging.md#the-ports-tree): the ports tree, shelves and repositories

<!-- book-nav -->
---

*Front matter.* Previous: [Typographical conventions](conventions.md) · [Contents](../README.md) · Next: [Part I. Preparing](../01-preparing/intro.md)
