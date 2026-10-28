# Preface

This chapter says what *KDOS from Scratch* is, who it is written for, what it expects you to have
and to know, how it shares the work with the other KDOS book, and how to tell which state of the
source tree a copy of it describes.

## What this book is

KDOS is a Linux distribution that builds itself from source. A build starts from a container with a
stock compiler in it, makes a cross compiler, uses that to make a minimal root filesystem, and then
compiles about two thousand packages one at a time inside that root, ending in a bootable image.
This book follows the build in the order it happens. It starts with the container every phase runs
in and the sources the build needs. It shows the two cross-toolchain scripts and the twenty-two
bootstrap scripts command by command. It explains how the new root is entered as a chroot and what
one `kpkg install` does there, and then it gives every package of every later phase a page of its
own, or a compact card for the plainer ones, in the order the build installs them.

The model is the *Linux From Scratch* book. A package page says what the package is and where its
sources come from, shows the exact commands that build it, explains the switches that are not
obvious, and lists what the package installs. The difference is that KDOS automates every one of
these steps already. You do not have to type the commands in this book to get a working system:
`make build` runs all of them, in this order. The book is there so that you can see what that one
command does, check a step against the tree, and run a step again by hand when it fails.

After the package phases, the kernel and the image each have a part: how the kernel is
configured, built and installed, and what every step of the last phase does to turn the built root
into an ISO. *System administration* then turns from building the system to running it, with one
task page per subject in the manner of a wiki: services, boot, storage, security, networking,
hardware, packages, the desktop and recovery. The appendices collect the switches that recur across
the recipes, the build shapes they share, the build times, the licences and a glossary.

## Who it is for

The book is written for anyone who wants to understand how a whole Linux system is put together and
is willing to read build scripts to find out: a developer about to change a recipe, someone looking
at a phase that failed, a reader who wants to know what is on the image and how it got there.

It assumes that you are at home on a Linux command line, can read a shell script, know roughly what
`./configure`, `make` and a compiler do, and have used Docker. It assumes no knowledge of KDOS
itself. A KDOS term is explained where it first appears, or linked to the
[glossary](../17-appendices/glossary.md).

To follow along you need a clone of the repository and a machine that runs Docker, with about
42 GB of free disk for the sources and tens of gigabytes more for the build; the full list is in
[What a development machine needs](../../kdos/05-developer/developing.md#what-a-development-machine-needs).
There is no supported way to build KDOS with your own machine's compilers and tools. The chapter on
[the os-dev container](../01-preparing/host-container.md) says what the container gives the build
that your machine cannot.

## The two books

KDOS has two books, and they divide the work between them.

[The KDOS book](../../kdos/README.md) is the authority on how the system works and why it was
designed the way it is: the boot path, the package format, the security model, the desktop. It
explains each mechanism once.

This book gives the commands. It shows what each phase and each package does to the tree, in the
order it does it, and links to the KDOS book wherever the reason behind a step matters, rather than
repeating that reason. When the two books describe the same thing differently, the KDOS book is
right and this one is wrong. You can read this book on its own and follow the links when a step
needs explaining, or read [How KDOS is built](../../kdos/05-developer/how-kdos-is-built.md) first,
which tells the whole build once in prose, and use this book as the reference it points into.

## How the book is made

Only the narrative chapters are written by hand, under `docs/kfs/` in the repository. Every package
page is generated from the port's recipe each time the book is rendered, and every command listing
in a narrative chapter is read, at the same moment, from the file it names. A page therefore cannot
show a recipe or a script other than the one in the tree it was rendered from.

Three kinds of figure come from a built tree rather than from the recipes: a package's installed
size and contents, the one-line descriptions of its programs and libraries, and its build time.
They are kept in a snapshot under `docs/kfs/data/`, which `make book-data` refreshes after a build.
When a recipe has a different version from the one the snapshot recorded, its page says so above
the figures.

`make book` renders the book into `build/kfs/`: the HTML edition, one page for each part, chapter
and package, in `html/`; the whole book as one PDF, `kfs.pdf`; and the HTML edition packed as
`kfs-html.tar.zst`. pandoc writes the HTML, and pandoc and typst together write the PDF; by default
both are the copies KDOS builds for itself, run out of the built tree that `BUILDFS` names
(`build/fs` unless you say otherwise). When that tree has a package database, the HTML edition also
lists every file each package installed; the PDF leaves the lists out. The edition published on the
project's pages is rendered without a built tree, so its package pages carry the snapshot's summary
of programs, libraries and directories and no full file lists.

## Editions

A copy of this book describes one commit of the source tree: the one it was rendered from. Every
HTML page carries an edition line in its header, and the PDF's title page carries the same, giving
the KDOS version (read from `fs/etc/os-release`), the commit, and that commit's date. The title page
also describes the snapshot in `docs/kfs/data/`: the commit the checkout stood at when it was taken,
and the date of the newest package in that built tree. Both can be older than the commit the book
describes.

Check the edition line against the tree you are building. A recipe, a script or a flag shown here
belongs to that commit. If your checkout is at another one, render the book again from it with
`make book`; nothing else needs to change.

## See also

- [Typographical conventions](conventions.md): how commands, paths, placeholders and build times are set
- [Reading a package page](reading-a-package-page.md): the layout of the generated pages
- [The KDOS book](../../kdos/README.md): the authority on how KDOS works and why
- [How KDOS is built](../../kdos/05-developer/how-kdos-is-built.md): the whole build told once, in prose

<!-- book-nav -->
---

*Front matter.* Previous: [Front matter](intro.md) · [Contents](../README.md) · Next: [Typographical conventions](conventions.md)
