# KDOS documentation

The complete documentation for KDOS: a Linux distribution compiled from source, with a desktop
that is a grid of character cells all the way down. It is written for a reader who is comfortable
with a build log, a package recipe and the C that draws the panel — someone who intends to change
this system rather than configure around it. Every page describes KDOS as it is now. Nothing here
records what it was.

## Reading paths

**I want to use KDOS**

1. [Getting started](02-user-guide/getting-started.md) — build an image and boot it
2. [Installation](02-user-guide/installation.md) — put it on a disk
3. [The desktop](02-user-guide/desktop.md) — live in it
4. [Applications](02-user-guide/applications.md) — the software library on the medium
5. [Theming](02-user-guide/theming.md) — the seven accents and the CRT pass
6. [Administration](02-user-guide/administration.md) — services, network, hardware, updates

**I want to build KDOS**

1. [Why KDOS](01-philosophy/why-kdos.md) — what you are signing up for
2. [Developing](05-developer/developing.md) — the first build, and the loops that avoid one
3. [The build system](05-developer/build-system.md) — phases, snapshots, plans
4. [Writing ports](05-developer/writing-ports.md) — the recipe format end to end
5. [Build troubleshooting](05-developer/build-troubleshooting.md) — when it fails
6. [Testing](05-developer/testing.md) — what each harness proves

**I want to change KDOS**

1. [Principles](01-philosophy/principles.md) — the rules every change is judged against
2. [Architecture overview](03-architecture/overview.md) — how the pieces fit
3. [The design language](03-architecture/design-language.md) — what a KDOS surface must look like
4. [The C libraries](05-developer/c-libraries.md) — what you build on
5. [Writing desktop software](05-developer/writing-desktop-software.md) — how to draw one
6. The [program page](04-programs/README.md) for whatever you are touching

## The book

### I — Philosophy

Why this distribution exists, what it refuses to do, and the arguments behind the choices that
are hardest to reverse.

| Page | What it covers |
|---|---|
| [Why KDOS](01-philosophy/why-kdos.md) | The four properties, who it is for, the trade you accept, and what is deliberately absent |
| [Principles](01-philosophy/principles.md) | The rules that constrain every decision, and what each one costs |
| [Decisions](01-philosophy/decisions.md) | Choices argued in full, with the alternatives that lost |

### II — User guide

Getting a system running and living in it.

| Page | What it covers |
|---|---|
| [Getting started](02-user-guide/getting-started.md) | Building an image, writing a medium, first boot, first login, starting a session |
| [Installation](02-user-guide/installation.md) | The installer page by page: disks, filesystems, encryption, applications, unattended installs |
| [The desktop](02-user-guide/desktop.md) | Panel, menus, windows, workspaces, keybindings, notifications, lock and power, devices |
| [Applications](02-user-guide/applications.md) | Alien apps: installing from the medium, launching, file handling, updates, rollback |
| [Theming](02-user-guide/theming.md) | The seven accents, what retints live, the CRT knobs, wallpaper, fonts, boxed applications |
| [Administration](02-user-guide/administration.md) | Services, networking, firewall, storage, users, hardware, codecs, updates, diagnosis |

### III — Architecture and core

How the system is put together, and the models a change has to respect.

| Page | What it covers |
|---|---|
| [Overview](03-architecture/overview.md) | The three rings, the host/box boundary, a process map, where state lives |
| [Boot and init](03-architecture/boot-and-init.md) | Firmware to login prompt: initramfs, splash, encryption, A/B slots, rcS, the console |
| [The session](03-architecture/session.md) | The session bus, audio, portals, supervised chrome, capture, clipboard, input methods |
| [Packaging](03-architecture/packaging.md) | Ports, packages, the solver, reproducible builds, the signed binhost, deltas |
| [Packs and boxes](03-architecture/packs-and-boxes.md) | The pack format, mounting, composition, grafts, the catalogue, one box per application |
| [Security model](03-architecture/security-model.md) | Signing, setuid, daemon authorisation, sandboxing, containers — and what is not protected |
| [Design language](03-architecture/design-language.md) | The character grid as a specification: frame, chrome, colour, pointer, glyph tiers |
| [The window model](03-architecture/window-model.md) | Placement, tiling, the edge search and the ring walks — one implementation, obeyed by both desktops |

### IV — Programs and applications

A page per program KDOS itself ships.

| Page | What it covers |
|---|---|
| [Program map](04-programs/README.md) | Every KDOS binary, what it is, and which name it answers to |
| [kdos-comp](04-programs/kdos-comp.md) | The compositor: configuration, decorations, the CRT pass, wallpaper, idle, sockets |
| [kdos-con](04-programs/kdos-con.md) | The console desktop: the default session, the session/view split, both sockets, the greeter, a graphical application as a window, and recording one |
| [kdos-shell](04-programs/kdos-shell.md) | One binary under many names: the panel, Start menu, desktop, chooser, notifications, applets |
| [kdos-res](04-programs/kdos-res.md) | The resource monitor: its pages, identity by box, and what it refuses to invent |
| [kdos-term](04-programs/kdos-term.md) | The terminal: one binary on both desktops, its keys and clipboards, the three image protocols and animation |
| [kdos-cage](04-programs/kdos-cage.md) | The kiosk: one application embedded in the cell desktop or full screen on a VT, and what the fork of cage changed |
| [kdos-appbox](04-programs/kdos-appbox.md) | Launching alien apps, generating launchers, and boxes as first-class objects |
| [The root daemons](04-programs/daemons.md) | powerd, energyd, oomd, mountd, packd, boxsock, and the portal backend |
| [kinstall](04-programs/kinstall.md) | The installer's design: the probe, the page model, the install child, answer files |
| [The kdos command](04-programs/kdos-command.md) | The front door and every subcommand behind it |
| [kdos-bb](04-programs/kdos-bb.md) | The forked AAlib demo, and the audio rules it establishes |

### V — Developer guide

Building the distribution, extending it, and proving it still works.

| Page | What it covers |
|---|---|
| [Developing](05-developer/developing.md) | First build, every make target, and the narrow loops that avoid a full one |
| [The build system](05-developer/build-system.md) | Phases, the chroot, snapshots, build plans, the fs manifest, the orphan sweep |
| [Writing ports](05-developer/writing-ports.md) | The recipe format, vendoring, canonical build shapes, adding a port end to end |
| [Build troubleshooting](05-developer/build-troubleshooting.md) | Recurring failures by symptom, with the canonical fix for each |
| [The C libraries](05-developer/c-libraries.md) | The libk set, the dependency direction, and the invariants each one keeps |
| [Writing desktop software](05-developer/writing-desktop-software.md) | Drawing a KDOS surface: roles, input, chrome, pictures, dumps and goldens |
| [Testing](05-developer/testing.md) | preflight, selftest, fixtures, goldens, and the QEMU rig |

### VI — Reference

Lookup tables and statements of state.

| Page | What it covers |
|---|---|
| [Command index](06-reference/command-index.md) | Every command the system ships, and where it is documented |
| [Configuration](06-reference/configuration.md) | Every configuration file and key, with defaults and when a change applies |
| [Filesystem and IPC](06-reference/filesystem-and-ipc.md) | KDOS-owned paths, every socket and its verbs, and the environment variables |
| [Repository layout](06-reference/repository-layout.md) | The source tree, annotated |
| [Known gaps](06-reference/known-gaps.md) | What does not exist, so you stop looking for it |
| [Roadmap](06-reference/roadmap.md) | Stated direction, clearly separated from what ships |
| [Status](06-reference/status.md) | Maturity per subsystem, and the evidence behind each verdict |
| [Glossary](06-reference/glossary.md) | The vocabulary these pages use, defined once |

## Conventions

Links between pages are relative, so the book works in a clone, in a pager and on GitHub. Every
page opens with a paragraph saying what it covers, and ends with a **See also** list. Terms are
used exactly as the [glossary](06-reference/glossary.md) defines them. Where a page states a
count or a measurement, it says what was counted or measured.

Two files carry a version: the [repository README](../../README.md) names the release line, and
[Status](06-reference/status.md) says what each subsystem's maturity rests on. No other page
states a version.

## See also

- [Repository README](../../README.md) — the front door, and the quickest path to a running system
- [CLAUDE.md](../../CLAUDE.md) — the rules and conventions for working on this tree
