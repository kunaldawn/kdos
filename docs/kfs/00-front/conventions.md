# Typographical conventions

This chapter sets out how the book writes commands and their output, where each command is meant
to run, how paths and placeholders are spelled, how listings taken from the tree are marked, and
what a build time given in SBU means.

## Commands and output

A command is set in a box of its own, with no prompt in front of it, so that it can be copied as it
stands. This one, like every `kpkg` command outside System administration, runs in the chroot
described below:

```bash
kpkg info pv
```

What a command prints, when the book shows it, is in a separate box directly after the command, so
that output is never mistaken for something to type:

```text
Package: pv
Version: 1.12.0-1
License: GPL-3.0-or-later
Files: 16
```

File names, commands, variables and switches in running text are set in a fixed-width face, as in
`build.sh`, `kpkg install` or `--disable-static`. A term is set in **bold** where it is defined.

## Where a command runs

Every command in this book runs in one of four places, and the sentence before the box always
says which:

On **your machine**, the host, from the top of your clone of the repository. Nothing in this book
asks you to run a command there as root. `make fetch`, `make build` and `make book` are typed here.

In **the container**, `os-dev`, which `make build` starts and which is described in
[the os-dev container](../01-preparing/host-container.md). The repository is at `/workspace` inside
it, and the container runs as its own root. The first two phases, `00_cross` and `10_bootstrap`, run
here.

In **the chroot**: the root filesystem the build is assembling, `build/fs`, entered as `/` with
`script/chroot/exec.sh` from inside the container. Every phase from `20_selfhost` on runs here, and
every `kpkg` command outside System administration means a `kpkg` running inside the chroot, as
root, from the directory `/kdos`. [Entering the chroot](../04-chroot/entering.md) shows how to get
there by hand.

On **a KDOS machine**, a system installed or booted from the finished image. Every command in
[System administration](../16-wiki/intro.md) is typed there, and the page says which of them need
root, usually by setting `sudo` in front.

The same file has a different path in each of the first three. The book writes a path the way the place
running the command sees it, and this table translates between them:

| On your machine | In the container | In the chroot |
|:--|:--|:--|
| the top of the clone | `/workspace` | `/kdos` |
| `ports/` | `/workspace/ports` | `/ports`, and again at `/kdos/ports` |
| `build/` | `/workspace/build` | `/kdos/build` |
| `build/fs` | `/workspace/build/fs` | `/` |
| `build/fs/var/lib/kpkg/db` | `/workspace/build/fs/var/lib/kpkg/db` | `/var/lib/kpkg/db` |

An absolute path such as `/var/lib/kpkg/db` or `/usr/share/licenses` always means a path inside the
root being built, which is `build/fs/...` on your machine. A path with no leading slash, such as
`script/phases/41_system/phase.env`, is relative to the top of the repository, and means the same
file in all three places because each runs its commands from there.

## Placeholders

A word in angle brackets is a placeholder for something you choose, and the brackets are not typed:
`<port>` is the bare name of a port, such as `pv`; `<phase>` is a phase directory, such as
`41_system`; `<shelf>` is the shelf a port is filed on, such as `cli`. In

```bash
make build BUILD_ARGS="--phases <phase>,70_image --rebuild <port>"
```

you would type `--phases 41_system,70_image --rebuild pv`.

## Listings read from the tree

Most of the commands in this book are not copied into it. They are read from the script or recipe
that runs them each time the book is rendered, and set as a listing with a caption under it naming
the file, and the line range when the listing is part of a file. The caption links to the file. A
listing of a whole file leaves out its `#!` line and its banner, the comment block that draws the
KDOS logo in box characters at the top of the tree's scripts and recipes, and nothing else. A
listing is always the text the build runs; the prose around it is what explains it.

## Names and links

A port is always named by its bare name, the name in its recipe's `name =` line, never with its
shelf: `pv`, not `cli/pv`. KDOS finds a port by that name wherever it is filed. A port named in
running text links to its package page, as in [pv](#port-pv) or [zlib](#port-zlib).

A link into [the KDOS book](../../kdos/README.md) leads to the chapter that explains a mechanism.
This book follows such a link with no more than a sentence of its own about the same subject. In
the rendered book, such a link, and a link to a file of the tree, opens the file on GitHub.

## Notes

Three kinds of note are set in a box on a package page. One says that the build-state snapshot was
taken from a different version of the port from the recipe shown, so that the size, the time and
the contents on that page describe the earlier build. Another appears on the page of a package
that is also built before kpkg exists, by a script of `00_cross` or `10_bootstrap`, and compares the
switches the two builds use; see [Reading a package page](reading-a-package-page.md#the-header).
The third appears on a full page whose port has no hand-written note to explain what makes it
unusual; see [The three tiers](reading-a-package-page.md#the-three-tiers).

## Sizes

A size is in binary units: 1 KiB is 1,024 bytes and 1 MiB is 1,024 KiB. A package's installed size
is the total of the regular files its database entry lists, as they are on disk in the built tree.
A source's download size is the size of the file in the source cache.

## Build times and the SBU

Build times are given in **SBUs**, standard build units, the way the *Linux From Scratch* book gives
them, because a time in seconds is only true of the machine that measured it. One SBU is the time
the first step of the whole build took: the cross binutils, `00_cross/binutils`. Every other figure
is a step's own time divided by that one, so a port shown as `2.5 SBU` took two and a half times as
long as the cross binutils did on the same machine.

The figures come from `build/snapshots/timings.json`, where kdosbuild records the seconds each step
took when it did work, keyed by phase and step: `00_cross/binutils`, `10_bootstrap/gcc`,
`41_system/pv`; see [Step timings](../../kdos/05-developer/build-system.md#step-timings).
`make book-data` divides each by the SBU and writes the result into the snapshot. The snapshot's
stamp names the commit the checkout stood at when the snapshot was taken, the date of the newest
package in the built tree, how many packages that tree had installed, and, as `sbu-seconds`, how
long one SBU was. `sbu-machine` is the machine that measured it, as kdosbuild recorded it beside
the figure, and each `machine-<n>` line describes one machine a figure names: its processor, the
CPUs the build could run on, its memory and the `KDOS_JOBS` it was given, then, as `machine-<n>-ports`, how many port figures it measured.
`unrecorded-ports` counts the figures that name no machine, recorded by a kdosbuild that filed
none, and `-` for `sbu-machine` means the SBU is one of those. `measured-phases` and
`unmeasured-phases` divide the package phases by whether any of their ports has a figure.
[Build times](../17-appendices/sbu.md) lists them:

<!-- kfs:include docs/kfs/data/stamp -->

Read an SBU figure as a proportion, not a promise. Four things move it:

- **The job count.** Every phase runs make with `-j$KDOS_JOBS`, and the cross binutils is built in
  parallel too, so the figures hold roughly across job counts; a port whose build is mostly one
  long serial step costs more SBUs on a machine with many threads than on one with few.
- **The compiler cache.** Once `ccache` is installed, a CMake port whose objects are already in
  `build/ccache` builds in a fraction of the time it took the first time; see
  [The compiler cache](../../kdos/05-developer/how-kdos-is-built.md#the-compiler-cache).
- **What a step did.** A step records a time only when it did its own work. A port skipped because
  it was installed and current, one installed from the package store, and a script whose "already
  done" marker made it exit at once record nothing, and the figure from the last run that did the
  work stays.
- **Which run measured it.** The figures of one snapshot come from as many runs as built its
  ports, each on the machine that run had, and every one is divided by the one SBU there is. A
  figure measured on another machine than the SBU, or on one it does not name, is a ratio across
  two machines or may be, and its page and the appendix say so.

A figure below 0.1 SBU is shown as 0.1. A package page with no build time on it has no measurement
in the snapshot. [Build times](../17-appendices/sbu.md), in the appendices, lists every figure the
snapshot holds, phase by phase.

## See also

- [Preface](preface.md): what the book covers and what it assumes
- [Reading a package page](reading-a-package-page.md): how the generated pages are laid out
- [The os-dev container](../01-preparing/host-container.md): the second of the three places a command runs
- [Entering the chroot](../04-chroot/entering.md): the third

<!-- book-nav -->
---

*Front matter.* Previous: [Preface](preface.md) · [Contents](../README.md) · Next: [Reading a package page](reading-a-package-page.md)
