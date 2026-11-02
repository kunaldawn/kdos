# The KDOS Book

A complete description of KDOS: what it is, how to install and run it, how it works from firmware
handoff to the Plasma desktop, how it is built from source, and how to extend it. This page is the
book's front matter: a preface, reading paths for four kinds of reader, and the full table of
contents. Every chapter it lists describes the system as it is in this source tree.

## Preface

KDOS is a Linux distribution for x86_64 machines, compiled in this repository from upstream source
with a musl C library, sysvinit and no systemd init, and no Xorg server. Its desktop is KDE Plasma
6.8 with all of KDE Gear, built unforked and made KDOS through its own settings pages, widgets,
KWin effects, Discover backend, look and updates. The graphical applications people expect are
ported natively, each with the toolkit it is written in (Qt, KDE Frameworks, GTK, wxWidgets, FLTK
or Tk), or run beside them in containers called *boxes*.

The book is written for people: anyone deciding whether to try KDOS, installing it, looking after a
machine that runs it, or building and changing it. It assumes a working knowledge of Linux (a
shell, files and permissions, what a kernel, a boot loader and a package manager are) and nothing
about KDOS itself. The chapters for developers also assume that you can read C and shell, and that
you have compiled a program from source before.

This is the first of KDOS's two books. It explains what each mechanism is and why it was chosen.
The second, [KDOS from Scratch](../kfs/README.md), gives the commands: the build phase by phase,
a page for every package, and the tasks of running a machine. Where the two meet, the second links
here rather than repeating it.

The book moves from a bird's-eye view to depth. Part I says what KDOS is, how it differs from other
distributions and why it chose differently. Part II uses the system as a person sitting at it
would. Part III opens the running system and explains each mechanism, starting with a map of the
whole. Part IV documents, one chapter each, the programs KDOS writes itself. Part V turns to the
build: it tells the story of a build from `git clone` to a bootable image once, then documents each
part of the build machinery, the recipe format, the libraries and the test harnesses. Part VI is
reference: lookup tables, statements of state and the glossary.

Each chapter opens with a paragraph that says what it covers, who it is for and what to read
first, and closes with a **See also** list. Within a chapter the order runs from concept to
mechanism to detail, so a reader can stop at the depth they need. The conventions the book follows
are listed under [Conventions](#conventions) below.

## How to read this book

Four paths through the book, depending on why you are here. Each names its chapters in the order
that suits that job; the numbers are the chapter numbers in the [Contents](#contents).

### Understanding the whole system

For a newcomer who wants to go from what KDOS is to how every part of it works, read the book
front to back. The shortest route that still covers the whole system is:

1. Ch 1, [Why KDOS](01-philosophy/why-kdos.md), and ch 2, [How KDOS differs](01-philosophy/how-kdos-differs.md)
2. Ch 3, [Principles](01-philosophy/principles.md), and ch 4, [Decisions](01-philosophy/decisions.md)
3. Ch 7, [The desktop](02-user-guide/desktop.md), and ch 8, [Applications](02-user-guide/applications.md)
4. Ch 12, [Architecture overview](03-architecture/overview.md), then chapters 13 to 19 in order
5. Ch 20, [The programs](04-programs/README.md), and any program chapter that interests you
6. Ch 30, [How KDOS is built](05-developer/how-kdos-is-built.md)
7. Ch 44, [Status](06-reference/status.md), and ch 43, [Known gaps](06-reference/known-gaps.md)

### Installing and using KDOS

For someone who wants an image on hardware and a working desktop:

1. Ch 1, [Why KDOS](01-philosophy/why-kdos.md): what the system is and what it deliberately lacks
2. Ch 5, [Getting started](02-user-guide/getting-started.md): build an image, write a medium, boot it
3. Ch 6, [Installation](02-user-guide/installation.md): the installer, page by page
4. Ch 7, [The desktop](02-user-guide/desktop.md): the desks, the panel, KRunner, windows and
   keyboard shortcuts
5. Ch 8, [Applications](02-user-guide/applications.md): which applications are native, and the
   catalogue and boxes for the rest
6. Ch 9, [Theming](02-user-guide/theming.md), and ch 11, [Accessibility](02-user-guide/accessibility.md)
7. Ch 43, [Known gaps](06-reference/known-gaps.md): what does not exist, before you depend on it

### Administering a KDOS machine

For the person who looks after an installed machine: services, hardware, updates and diagnosis.

1. Ch 10, [Administration](02-user-guide/administration.md): the tasks, each as a command or a file
2. Ch 28, [The kdos command](04-programs/kdos-command.md): status, doctor and the other subcommands
3. Ch 13, [Boot and init](03-architecture/boot-and-init.md): which program owns each step of a boot
4. Ch 15, [Packaging](03-architecture/packaging.md): what `kpkg` does, the binary host and updates
5. Ch 16, [Packs and boxes](03-architecture/packs-and-boxes.md): what installing an application does
6. Ch 17, [The security model](03-architecture/security-model.md): what is protected, and what is not
7. Ch 26, [The daemons](04-programs/daemons.md): the root daemons and what each allows
8. Ch 40, [Configuration](06-reference/configuration.md), and ch 41, [Filesystem and IPC](06-reference/filesystem-and-ipc.md)

### Building and extending KDOS

For a developer who builds the distribution, adds or changes a port, or writes desktop software:

1. Ch 3, [Principles](01-philosophy/principles.md): the rules every change is judged against
2. Ch 12, [Architecture overview](03-architecture/overview.md): how the pieces fit together
3. Ch 30, [How KDOS is built](05-developer/how-kdos-is-built.md): the build as one story
4. Ch 31, [Developing](05-developer/developing.md): the first build, and the loops that avoid one
5. Ch 32, [The build system](05-developer/build-system.md): phases, the chroot, snapshots, build plans
6. Ch 33, [Writing ports](05-developer/writing-ports.md), and ch 34, [Build troubleshooting](05-developer/build-troubleshooting.md)
7. Ch 36, [Writing desktop software](05-developer/writing-desktop-software.md), and ch 18, [The design language](03-architecture/design-language.md)
8. Ch 35, [The C libraries](05-developer/c-libraries.md): the `libk*` libraries the daemons and tools share
9. Ch 37, [Testing](05-developer/testing.md): what each harness proves, and what it cannot
10. Ch 42, [Repository layout](06-reference/repository-layout.md), and ch 38, [The ports catalogue](06-reference/ports-catalogue.md)

## Contents

Forty-five chapters in six parts.

### Part I — Introduction

What KDOS is and who it is for, how it compares with the ways other distributions solve the same
problems, the rules every part of it is built to, and the close choices argued in full. These
chapters stay at the level of ideas and link to the chapters that describe each mechanism.

1. [Why KDOS](01-philosophy/why-kdos.md): what KDOS is, the four properties that define it, who
   should run it, the trade you accept, what is deliberately absent, and what is not built from
   source
2. [How KDOS differs](01-philosophy/how-kdos-differs.md): KDOS compared point by point with other
   distributions: the C library, the userland, init, the display stack, packages, reproducibility,
   applications, updates and hardware support
3. [Principles](01-philosophy/principles.md): the rules that constrain every change, each with the
   failure it prevents and the price it charges
4. [Decisions](01-philosophy/decisions.md): the choices where an alternative was reasonable
   (Plasma as the desktop, elogind, SDDM, slot-image updates, native applications beside the
   catalogue, packs, musl, the source archive, `-march` and more), and why the alternative lost

### Part II — Using KDOS

Getting a machine running and living on it: building and booting an image, installing it, the
desktop, applications, the look of the system, administration and accessibility. These chapters
are written for the person at the keyboard and name the file behind each behaviour.

5. [Getting started](02-user-guide/getting-started.md): from a clone to a running desktop:
   building an image, trying it in a virtual machine, writing a medium, booting, logging in,
   keeping a live session's changes, and installing
6. [Installation](02-user-guide/installation.md): the installer page by page, the root filesystem,
   encryption, A/B root slots, unattended installs and every file the installer writes
7. [The desktop](02-user-guide/desktop.md): the Plasma desktop as KDOS ships it: the desks, the
   panel, the Start menu, KRunner, windows and workspaces, keyboard shortcuts, notifications, files,
   the clipboard, locking, displays, removable media and the privacy lamps
8. [Applications](02-user-guide/applications.md): which applications are ported natively, then
   finding, installing, launching, updating and removing boxed applications, carrying a set to
   another machine, and managing boxes
9. [Theming](02-user-guide/theming.md): the eight accents and what switching one changes,
   Konsole, the boot menu and consoles, Phosphor mode, wallpaper, sounds, fonts, and theming native
   applications and applications inside a box
10. [Administration](02-user-guide/administration.md): services, users, storage, networking, the
    firewall, hardware, updates and diagnosis, then each user's backups, mail, passwords and input
    methods
11. [Accessibility](02-user-guide/accessibility.md): the screen reader, braille and speech at a
    text console, the magnifier, larger text, keyboard aids, the on-screen keyboard, colour and
    motion, and boxed applications

### Part III — Architecture

How the running system is put together, from a map of the whole to each mechanism in turn: the
boot path, the session, host packaging, packs and boxes, security, the visual specification and
the window model. The overview comes first because every later chapter in the part expands one
region of it.

12. [Architecture overview](03-architecture/overview.md): the three rings, power-on to a drawn
    window, the host and a box, a running process map, the root daemons, the two packaging systems
    and where state lives
13. [Boot and init](03-architecture/boot-and-init.md): Limine, microcode, the initramfs and its
    splash, A/B slot selection, unlocking an encrypted root, `rcS`, the display manager and the
    console
14. [The session](03-architecture/session.md): what a login starts: the Plasma session, the
    user's services and the session bus, audio, portals, capture, the clipboard, input methods and
    a box's environment
15. [Packaging](03-architecture/packaging.md): the ports tree, its shelves and its phases, where
    sources come from, `kpkg`, what a build verifies, reproducible packages, the binary host,
    deltas and vulnerability tracking
16. [Packs and boxes](03-architecture/packs-and-boxes.md): the two lanes, the catalogue, the pack
    format, installing and mounting, composition, the box, grafts and data packs
17. [The security model](03-architecture/security-model.md): setuid binaries, system accounts,
    root daemons, the firewall, polkit, containers, signing, untrusted bytes, and what is not
    protected
18. [The design language](03-architecture/design-language.md): KDOS's identity on Plasma: colour,
    type, the pieces of the look, one look from power-on to the lock screen, motion, Phosphor mode,
    the desks, and the text tools' glyph tiers
19. [The window model](03-architecture/window-model.md): KWin as KDOS sets it: focus, placement,
    window memory, snapping and tiles, auto-tiling, workspaces, the window rules KDOS writes and
    more than one screen

### Part IV — Programs

What KDOS adds to the KDE programs it extends most, and a chapter for each program KDOS writes
itself, from the box tools and the root daemons to the installer and the command-line front door.
Software built from upstream ports is documented by its upstream; these chapters cover only what
this repository writes or changes.

20. [The programs](04-programs/README.md): every KDOS program and desktop component, what it does,
    and every name it answers to
21. [KWin](04-programs/kwin.md): the compositor as KDOS ships it: its defaults, the carried patches,
    shortcuts, the key card, window memory, tiling, boxes at the compositor, Phosphor mode, idle and
    lock, and debugging
22. [The Plasma shell](04-programs/plasma-shell.md): the three desks, the Start menu, the tray and
    its privacy lamps, the KDOS runner in KRunner, notices, the kded module, the welcome tour, System
    Monitor's KDOS sensors and the offline library
23. [KDOS settings pages](04-programs/kdos-settings.md): the Boxes, Firewall, Health, KDOS Look,
    System Versions and Backups pages, and the daemon behind KDE's date, time and language pages
24. [Discover](04-programs/discover.md): the KDOS backend for ports, packs and boxes, what keeps
    Discover offline, system updates through `kdos-sysupdated`, the update notifier and firmware
25. [kdos-appbox](04-programs/kdos-appbox.md): `kdos-appbox`, `kdos-box` and `xdg-open`: the launch
    path, launcher generation, the open path, box profiles, warmup and storage drivers
26. [The daemons](04-programs/daemons.md): the six root daemons, the per-box Wayland socket, the
    other services `ksvc` runs, and how to add a root daemon
27. [kinstall](04-programs/kinstall.md): the installer's reference: keys, pages, install steps,
    answer files, dry-run dumps, LVM, encryption, the applications step and its design
28. [The kdos command](04-programs/kdos-command.md): the front door to a KDOS machine, every
    subcommand, and the eleven other names the same binary answers to
29. [kdos-bb](04-programs/kdos-bb.md): the forked AAlib demo: running it, and what it establishes
    about frame pacing, synchronized output, timing to music and audio

### Part V — Building and developing

How KDOS comes into existence and how to change it. The part opens with the whole build told as
one story, then documents the everyday commands, the build orchestrator, the recipe format, the
failures that recur, the C libraries, desktop software and the test harnesses.

30. [How KDOS is built](05-developer/how-kdos-is-built.md): the build as one continuous story, from
    `git clone` through fetching, the cross toolchain, the phases and the chroot, to a bootable ISO
31. [Developing](05-developer/developing.md): what a development machine needs, the first build,
    every make target, rebuilding one thing, working without a build, and cutting a release
32. [The build system](05-developer/build-system.md): the thirteen phases and their package
    lists, where `kpkg` looks for a port, the phase environment, the chroot, syncing `fs/`, the
    orphan sweep, the packaging steps, snapshots, build plans and `kdosbuild`
33. [Writing ports](05-developer/writing-ports.md): the recipe format, canonical build shapes, a
    worked example, adding a port end to end, vendoring, checking for new versions and publishing
    sources
34. [Build troubleshooting](05-developer/build-troubleshooting.md): recurring fetch, build and push
    failures, indexed by the message you see, each with its cause and fix
35. [The C libraries](05-developer/c-libraries.md): the nine `libk*` libraries, the
    dependency direction, and the invariants each one keeps
36. [Writing desktop software](05-developer/writing-desktop-software.md): KDOS's Plasma code:
    settings modules, widgets, the runner, KWin effects and scripts, names, the toolkit rule,
    conventions, and the Qt test suite
37. [Testing](05-developer/testing.md): preflight, the self-test, the Qt suite, fixtures, the QEMU
    rig and its Plasma harnesses, AT-SPI goldens, checking the sources, the book checks, measuring
    frame rate, and what is not tested

### Part VI — Reference

Lookup tables and statements of state: every port, command, configuration key, path and socket,
the layout of the source tree, what does not exist, how mature each part is, and the vocabulary of
the book.

38. [The ports catalogue](06-reference/ports-catalogue.md): every port KDOS can build, by shelf
    and by `src/` area, with the phase that builds it, the libraries shipped as the
    application-developer platform, and the ports named in more than one list
39. [Command index](06-reference/command-index.md): every command the tree installs, where it lives
    and which chapter documents it
40. [Configuration](06-reference/configuration.md): every configuration file and key, with its
    default and when a change takes effect
41. [Filesystem and IPC](06-reference/filesystem-and-ipc.md): KDOS-owned paths, every socket and
    its verbs, the D-Bus names KDOS serves, files used as an interface, and environment variables
42. [Repository layout](06-reference/repository-layout.md): the source tree directory by
    directory: the port repositories, the shelves of `ports/core` and the areas of `src/`, the
    `script/` directory, the library rule, where upstream sources are, and what git ignores
43. [Known gaps](06-reference/known-gaps.md): what KDOS does not do, or has not been shown to do,
    and what to do instead
44. [Status](06-reference/status.md): the maturity of each subsystem and the evidence behind each
    verdict
45. [Glossary](06-reference/glossary.md): the vocabulary the book uses, defined once, and the words
    it avoids

## Conventions

Links between chapters are relative, so the book reads the same in a clone, in a pager and on a
web forge. Every chapter opens with a paragraph saying what it covers, who it is for and what to
read first, and closes with a **See also** list. Terms carry the meaning the
[glossary](06-reference/glossary.md) gives them; a chapter defines a term at first use or links
the glossary entry.

Commands are shown in fenced code blocks, and paths, keys, flags and program names in `code`.
Tables hold lookup data; prose holds explanation. Where a chapter states a count or a measurement,
it says what was counted or measured, and every such number was taken from the tree at the time of
writing. The book describes the system as it is: it records no history, and a behaviour that
changes changes its page in the same commit.

This book states the KDOS version in two places: the [repository README](../../README.md), which
names the release line, and [Status](06-reference/status.md#release-line), which says what that
line delivers and what it does not promise. The running system reads it from `fs/etc/os-release`. Any other
version number in the book belongs to an upstream component, except a release tag shown as an
example in a command.

## See also

- [Repository README](../../README.md): the front door, and the shortest path to a running system
- [KDOS from Scratch](../kfs/README.md): the companion book: the build phase by phase, a page for
  every package, and the tasks of running a machine
- [Why KDOS](01-philosophy/why-kdos.md): the first chapter, for a reader starting at the beginning
- [Glossary](06-reference/glossary.md): every term the book uses, defined once
