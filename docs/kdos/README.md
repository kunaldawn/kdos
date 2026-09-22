# KDOS documentation

This is the complete documentation for KDOS, a Linux distribution compiled from
source with a desktop whose every surface is a grid of character cells. It
covers the system from firmware handoff to the panel clock: how to install it,
how to run a machine on it, how it is built, and why each hard-to-reverse
decision went the way it did.

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
| [Why KDOS](01-philosophy/why-kdos.md) | The four properties that shape the tree, who the system is for, the trade you accept, and a complete list of what is not built from source |
| [Principles](01-philosophy/principles.md) | The rules that constrain every decision, and what each one costs |
| [Decisions](01-philosophy/decisions.md) | Individual choices argued in full, with the alternatives that lost |

### Part II — User guide

Getting a machine running, and living on it.

| Page | Contents |
|---|---|
| [Getting started](02-user-guide/getting-started.md) | Building an image, writing a medium, first boot, first login, starting a session |
| [Installation](02-user-guide/installation.md) | The installer page by page: disks, filesystems, encryption, applications, unattended installs |
| [The desktop](02-user-guide/desktop.md) | Panel, menus, windows, workspaces, keybindings, notifications, lock and power, removable devices |
| [Applications](02-user-guide/applications.md) | Installing containerised applications from the catalogue, launching them, file handling, updates, rollback |
| [Theming](02-user-guide/theming.md) | The eight accents, what retints live, the phosphor controls, wallpaper, fonts, and theming inside a container |
| [Administration](02-user-guide/administration.md) | Services, networking, firewall, storage, users, hardware, codecs, updates, diagnosis |
| [Accessibility](02-user-guide/accessibility.md) | What a containerised application's own assistive stack offers, and what this desktop does not expose |

### Part III — Architecture

How the system is put together, and the models a change has to respect.

| Page | Contents |
|---|---|
| [Architecture overview](03-architecture/overview.md) | The three rings, the host/container boundary, a process map, and where state lives |
| [Boot and init](03-architecture/boot-and-init.md) | Firmware to login prompt: initramfs, splash, encryption, A/B slots, rcS, the console |
| [The session](03-architecture/session.md) | The session bus, audio, portals, supervised chrome, capture, clipboard, input methods |
| [Packaging](03-architecture/packaging.md) | Ports, packages, the dependency solver, reproducible builds, the signed binhost, binary deltas |
| [Packs and boxes](03-architecture/packs-and-boxes.md) | The pack format, the bake, mounting, composition, grafts, the catalogue, one container per application |
| [The security model](03-architecture/security-model.md) | Signing, setuid, daemon authorisation, sandboxing, containers, and what is not protected |
| [The design language](03-architecture/design-language.md) | The character grid as a specification: frame, chrome, colour slots, the pointer contract, glyph tiers |
| [The window model](03-architecture/window-model.md) | Placement, tiling, snapping, the edge search and the ring walks the compositor obeys |

### Part IV — Programs

A page for each program KDOS itself ships.

| Page | Contents |
|---|---|
| [The programs](04-programs/README.md) | Every KDOS binary, what it does, and which name it answers to |
| [kdos-comp](04-programs/kdos-comp.md) | The compositor: configuration, decorations, the phosphor pass, wallpaper, idle handling, sockets |
| [kdos-shell](04-programs/kdos-shell.md) | One binary under many names: panel, Start menu, desktop, choosers, notifications, applets |
| [kdos-res](04-programs/kdos-res.md) | The resource monitor: its pages, identity by container, and what it refuses to infer |
| [kdos-term](04-programs/kdos-term.md) | The terminal: keys and clipboards, the three image protocols, and animation |
| [kdos-appbox](04-programs/kdos-appbox.md) | Launching containerised applications, generating launchers, and container profiles |
| [The daemons](04-programs/daemons.md) | powerd, energyd, oomd, mountd, packd, boxsock, kdos-lock, and the portal backend |
| [kinstall](04-programs/kinstall.md) | The installer's design: the probe, the page model, the install child, answer files |
| [The kdos command](04-programs/kdos-command.md) | The front door, and every subcommand behind it |
| [kdos-bb](04-programs/kdos-bb.md) | The forked AAlib demo, and the audio rules it establishes |

### Part V — Developer guide

Building the distribution, extending it, and proving it still works.

| Page | Contents |
|---|---|
| [Developing](05-developer/developing.md) | The first build, every make target, and the narrow loops that avoid a full rebuild |
| [The build system](05-developer/build-system.md) | Phases, the chroot, snapshots, build plans, the filesystem manifest, the orphan sweep |
| [Writing ports](05-developer/writing-ports.md) | The recipe format, vendoring, canonical build shapes, and adding a port end to end |
| [Build troubleshooting](05-developer/build-troubleshooting.md) | Recurring failures by symptom, each with its canonical fix |
| [The C libraries](05-developer/c-libraries.md) | The `libk*` set, the dependency direction, and the invariants each library keeps |
| [Writing desktop software](05-developer/writing-desktop-software.md) | Drawing a KDOS surface: roles, input, chrome, pictures, dumps and goldens |
| [Testing](05-developer/testing.md) | preflight, the self-test, fixtures, goldens, and the QEMU rig |

### Part VI — Reference

Lookup tables and statements of state.

| Page | Contents |
|---|---|
| [Command index](06-reference/command-index.md) | Every command the system ships, and where it is documented |
| [Configuration](06-reference/configuration.md) | Every configuration file and key, with defaults and when a change takes effect |
| [Filesystem and IPC](06-reference/filesystem-and-ipc.md) | KDOS-owned paths, every socket and its verbs, and the environment variables |
| [Repository layout](06-reference/repository-layout.md) | The source tree, annotated directory by directory |
| [Known gaps](06-reference/known-gaps.md) | What does not exist, so you stop looking for it |
| [Roadmap](06-reference/roadmap.md) | Stated direction, kept clearly separate from what ships |
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
