# KDOS documentation

This is the complete documentation for KDOS, a Linux distribution compiled from
source with a desktop whose every surface is a grid of character cells. It
covers the system from firmware handoff to the panel clock: how to install it,
how to run a machine on it, how it is built, and why each hard-to-reverse
decision went the way it did.

It is written for people: anyone trying KDOS, installing it, running a machine on it, or
contributing to it. Pick the reading path below that matches why you are here; each lists the pages
in the order that makes sense for that job, and each page says in its opening paragraph who it is
for and what to read first.

Forty-three pages, in six parts. Every page describes the system as it is.

## Reading paths

Three routes through the book, depending on why you are here.

### Evaluating or installing KDOS

Start with what the project is for, then get an image onto hardware.

1. [Why KDOS](01-philosophy/why-kdos.md) — what the distribution is, and what it deliberately lacks
2. [Getting started](02-user-guide/getting-started.md) — build an image, write a medium, boot it
3. [Installation](02-user-guide/installation.md) — the installer, page by page
4. [The desktop](02-user-guide/desktop.md) — panel, menus, windows, keybindings
5. [Applications](02-user-guide/applications.md) — the catalogue and the containers behind it
6. [Known gaps](06-reference/known-gaps.md) — what does not exist, before you depend on it

### Administering a KDOS machine

Day-to-day operation: services, hardware, updates and diagnosis.

1. [Administration](02-user-guide/administration.md) — services, network, storage, users, updates
2. [Theming](02-user-guide/theming.md) — accents, the phosphor pass, wallpaper, fonts
3. [Accessibility](02-user-guide/accessibility.md) — what assistive routes exist, and what they cannot reach
4. [The kdos command](04-programs/kdos-command.md) — the front door for status, doctor and diagnosis
5. [Configuration](06-reference/configuration.md) — every file and key, with defaults
6. [The security model](03-architecture/security-model.md) — what is protected, and what is not

### Working on the source

Building the distribution, extending it, and proving it still works.

1. [Principles](01-philosophy/principles.md) — the rules every change is judged against
2. [Architecture overview](03-architecture/overview.md) — how the pieces fit together
3. [Developing](05-developer/developing.md) — the first build, and the loops that avoid one
4. [The build system](05-developer/build-system.md) — phases, the chroot, snapshots, build plans
5. [Writing ports](05-developer/writing-ports.md) — the recipe format, end to end
6. [The C libraries](05-developer/c-libraries.md) — what desktop software is built on
7. [Writing desktop software](05-developer/writing-desktop-software.md) — drawing a KDOS surface
8. [Testing](05-developer/testing.md) — what each harness proves, and what it cannot

## Contents

### Part I — Philosophy

The reasoning behind the distribution: why it exists, what it refuses to do, and
the arguments behind the choices that are hardest to reverse.

| Page | Contents |
|---|---|
| [Why KDOS](01-philosophy/why-kdos.md) | What KDOS is, the four properties that shape the tree, who should run it, the trade you accept, what is deliberately absent, a complete list of what is not built from source, and the system in numbers |
| [Principles](01-philosophy/principles.md) | The rules that constrain every decision, and what each one costs |
| [Decisions](01-philosophy/decisions.md) | The close choices argued in full — the compositor fork, packs, Debian in boxes, musl, the source archive, `-march`, the file chooser and more — with the alternatives that lost |

### Part II — User guide

Getting a machine running, and living on it.

| Page | Contents |
|---|---|
| [Getting started](02-user-guide/getting-started.md) | What a first build costs, fetching and building an image, trying it in a virtual machine, writing a medium, booting, logging in, keeping changes, and installing |
| [Installation](02-user-guide/installation.md) | The installer page by page: disks, the root filesystem, encryption, A/B root slots, unattended installs, and every file it writes |
| [The desktop](02-user-guide/desktop.md) | Panel, Start menu, windows, keyboard shortcuts, notifications, files, the clipboard, lock and power, displays, removable devices, and who is using the camera and microphone |
| [Applications](02-user-guide/applications.md) | Finding and installing applications from the catalogue, carrying a set to another machine, launching, opening files, boxed commands, updating, removing, and boxes |
| [Theming](02-user-guide/theming.md) | The eight accents, choosing a font, what changes when, the boot menu and consoles, the phosphor pass, wallpaper, style files, and theming inside a box |
| [Administration](02-user-guide/administration.md) | Services, periodic jobs, backups, networking, the firewall, file search, mail, passwords, storage, users, hardware, input methods, updates, diagnosis, and copying the medium |
| [Accessibility](02-user-guide/accessibility.md) | What exists — braille and speech at a text console, the magnifier, larger text, a boxed application's own assistive stack — and what the desktop does not expose |

### Part III — Architecture

How the system is put together, and the models a change has to respect.

| Page | Contents |
|---|---|
| [Architecture overview](03-architecture/overview.md) | From power-on to a drawn window, the three rings, the host and a box, a process map, the root daemons, the two packaging systems, and where state lives |
| [Boot and init](03-architecture/boot-and-init.md) | Firmware to login prompt: Limine, microcode, the initramfs, the splash, unlocking an encrypted root, A/B slot selection, rcS, the console and the login banner |
| [The session](03-architecture/session.md) | The session bus, audio, portals, supervised chrome, capture, clipboard, input methods |
| [Packaging](03-architecture/packaging.md) | The ports tree, where sources come from, `kpkg`, what a build verifies, reproducible packages, the binary host, deltas, updating a machine, vulnerability tracking |
| [Packs and boxes](03-architecture/packs-and-boxes.md) | The two lanes, the pack format, building a pack, the catalogue, mounting, composition, the box, grafts and data packs, and how a box reaches the desktop |
| [The security model](03-architecture/security-model.md) | setuid binaries, system accounts, root daemons, polkit, sandboxed clients, containers, signing, untrusted image bytes, and what is not protected |
| [The design language](03-architecture/design-language.md) | The character grid as a specification: the frame, chrome, shared keys, colour, the pointer contract, touch, the wheel, hit maps, glyph tiers, pictures |
| [The window model](03-architecture/window-model.md) | The contract `libkwm` keeps with the compositor: tiling, placement, windows that belong to other windows, the edge search, and occupancy |

### Part IV — Programs

A page for each program KDOS itself ships.

| Page | Contents |
|---|---|
| [The programs](04-programs/README.md) | Every KDOS binary, what it does, and which name it answers to |
| [kdos-comp](04-programs/kdos-comp.md) | The compositor: configuration, bindings, decorations, frame pacing, the phosphor pass, wallpaper, idle and lock, sockets, window groups, box identity, and working on its code |
| [kdos-shell](04-programs/kdos-shell.md) | One binary under 53 names and 52 surfaces: the panel, Start menu, launcher, desktop, file chooser, settings, device managers, notifications, the store, and the small surfaces |
| [kdos-res](04-programs/kdos-res.md) | The resource monitor: its eleven pages, keys, acting on a process, identity by box, and what it refuses to infer |
| [kdos-term](04-programs/kdos-term.md) | The terminal: the escape layer, pasting and the paste guard, the two clipboards, per-window fonts, hyperlinks, prompt marks, and the three picture protocols |
| [kdos-appbox](04-programs/kdos-appbox.md) | `kdos-appbox`, `kdos-box` and `xdg-open`: the launch path, launcher generation, the open path, box profiles, warmup, and storage drivers |
| [The daemons](04-programs/daemons.md) | powerd, energyd, oomd, mountd, packd, boxsock, kdos-lock, and the portal backend |
| [kinstall](04-programs/kinstall.md) | The installer's reference: keys, pages, the install steps, answer files, dry-run dumps, LVM, the applications step, and its design |
| [The kdos command](04-programs/kdos-command.md) | The front door, and every subcommand behind it |
| [kdos-bb](04-programs/kdos-bb.md) | The forked AAlib demo: running it, and what it establishes about frame rate, synchronized output, timing to music, and audio |

### Part V — Developer guide

Building the distribution, extending it, and proving it still works.

| Page | Contents |
|---|---|
| [Developing](05-developer/developing.md) | What a machine needs, getting and fetching the sources, the first build, every make target, rebuilding one thing, and cutting a release |
| [The build system](05-developer/build-system.md) | Phases, the chroot, snapshots, build plans, `kdosbuild`, syncing `fs/`, the orphan sweep, the packaging steps, and fetching in a container |
| [Writing ports](05-developer/writing-ports.md) | The recipe format, canonical build shapes, vendoring, adding a port end to end, checking for new versions, and publishing sources |
| [Build troubleshooting](05-developer/build-troubleshooting.md) | Recurring build, fetch and push failures by symptom, each with its fix |
| [The C libraries](05-developer/c-libraries.md) | The `libk*` set, the dependency direction, and the invariants each library keeps |
| [Writing desktop software](05-developer/writing-desktop-software.md) | Drawing a KDOS surface: roles, input, chrome, pictures, dumps and goldens |
| [Testing](05-developer/testing.md) | preflight, the self-test, goldens, fixtures, the QEMU rig, checking the sources, docscheck, and what is not tested |

### Part VI — Reference

Lookup tables and statements of state.

| Page | Contents |
|---|---|
| [Command index](06-reference/command-index.md) | Every command the system ships, and where it is documented |
| [Configuration](06-reference/configuration.md) | Every configuration file and key, with defaults and when a change takes effect |
| [Filesystem and IPC](06-reference/filesystem-and-ipc.md) | KDOS-owned paths, every socket and its verbs, and the environment variables |
| [Repository layout](06-reference/repository-layout.md) | The source tree, annotated directory by directory: the port repositories, the library rule, where upstream sources are, and what git ignores |
| [Known gaps](06-reference/known-gaps.md) | What does not exist, so you stop looking for it |
| [Roadmap](06-reference/roadmap.md) | aarch64 and mobile, stated direction, and what is not planned, kept clearly separate from what ships |
| [Status](06-reference/status.md) | Maturity per subsystem, and the evidence behind each verdict |
| [Glossary](06-reference/glossary.md) | The vocabulary these pages use, defined once |

## Conventions

Links between pages are relative, so the book reads in a clone, in a pager and
on a web forge. Every page opens with a paragraph saying what it covers and
closes with a **See also** list. Terms carry the meaning the
[glossary](06-reference/glossary.md) gives them. Where a page states a count or
a measurement, it says what was counted or measured.

The KDOS version string appears in three places and nowhere else: the
[repository README](../../README.md), which names the release line;
[Status](06-reference/status.md), which says what each subsystem's maturity
rests on; and `fs/etc/os-release`. A version number anywhere else in the book
belongs to an upstream component, not to the system.

## See also

- [Repository README](../../README.md) — the front door, and the shortest path to a running system
