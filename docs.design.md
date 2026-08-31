# KDOS Documentation — design

The design of record for the KDOS documentation set: a book under `docs/kdos/`,
a front-door `README.md`, and a `CLAUDE.md` reduced to the rules and conventions
an agent needs. This file is the spec the implementation plan is written from.

---

## Goal

One knowledge base for KDOS, written for a power Linux user or a developer who
intends to change the system rather than configure around it. It must be
sufficient, on its own, to install KDOS, live in it, build it from source, add a
port, write a desktop surface, and understand every subsystem well enough to
modify it.

## The rules this documentation is written under

1. **One copy of every fact.** Knowledge lives in the book. `CLAUDE.md` holds
   rules, conventions and agent workflow, and links to the book for everything
   else. Where the same subject has a user-facing and an internal half, the two
   halves are different content on different pages, never the same content told
   twice.
2. **Current state only. No history, anywhere.** No page and no comment records
   what something used to be, what bug was fixed, what "used to", "no longer",
   "now finally", or "cost a debug cycle". A warning is written as the rule and
   its consequence, never as the story of how it was learned. This applies to
   `CLAUDE.md` itself, which is rewritten under it.
3. **Every change updates its documentation in the same change.** A change in
   behaviour must update every page, code comment and shipped configuration
   file that describes that behaviour, before the change is done — and the
   update REPLACES the old description rather than appending to it or
   recording what it was. A comment or a page that contradicts its code is a
   claim the next reader will act on. This is a rule for anyone working on the
   tree, so it lands in `CLAUDE.md`'s hard rules as well as in the book.
4. **Never invent a number.** A count, a timing or a measurement appears only
   where it was measured, and says what was measured. Where a value is not
   known, the page says so.
5. **Plain markdown, relative links.** No site generator, no build step, no
   front matter. It reads in a clone, in a pager and on GitHub.
6. **No lint.** Correctness is the writer's job.
7. **One place carries the release line.** The root `README.md` names it and
   `06-reference/status.md` says what each subsystem's maturity rests on. No
   other page states a version, so a release touches two files.

## Scope

In scope: the root `README.md`, the whole of `docs/kdos/`, screenshots under
`docs/screenshots/`, and the rewrite of `CLAUDE.md`.

`src/desktop/kdos-be` and `docs/KDOS-BE.md` are removed. Both were untracked and
the directory held no files, so this is a no-op for git; nothing in the tree
references `kdos-be`, `KDOS_SESSION`, `kdos-bar` or `kdos-files`. **KDOS has one
desktop session**, and no page mentions a second.

Out of scope: any change to source, recipes, scripts or tests, except
throwaway capture scripts under `build/`; a static-site toolchain; a docs
lint; `mobile.plan.md` and `mobile.plan2.md`, which stay at the root as plans;
the empty and untracked `src/desktop/kdos-attrd` and `src/libs/libktray`.

Nothing is committed. The user commits.

---

## Structure

```
README.md                            front door, GitHub's landing page
docs/kdos/README.md                  the book's table of contents
docs/kdos/01-philosophy/             why and what
docs/kdos/02-user-guide/             install, live in it, administer it
docs/kdos/03-architecture/           how the system is put together
docs/kdos/04-programs/               a page per program KDOS itself ships
docs/kdos/05-developer/              build it, extend it, test it
docs/kdos/06-reference/              lookup tables and statements of state
docs/screenshots/                    all images, referenced by relative path
```

Part directories are numbered so a file listing reads in book order. Page files
are named for their subject, not numbered, so a link survives a reordering.

---

## The pages

Forty pages in six parts, plus two index pages: `docs/kdos/README.md` and the
root `README.md`. Each entry below states what the page carries; the plan turns
each of the forty into one task.

> Two deviations from the outline first approved. `04-programs/kdos-bb.md` is
> added: the forked AAlib demo is a shipped program whose aalib and libmikmod
> constraints bind anything else built on them, and every other program has a
> home. `06-reference/status.md` is added to carry per-subsystem maturity.
> `04-programs/kdos-be.md` is dropped — KDOS has one desktop session. The count
> is unchanged at forty.

### Front matter

| Path | Contents |
|---|---|
| `README.md` | Identity, hero screenshot, the four properties in brief, what is and is not built from source, three quick-start paths (boot a medium, build an ISO, install to disk), the headline differentiators with pictures, a **Start here** table into the book, repository layout in brief, licence. Roughly 300 lines. It is a front door, not a manual. |
| `docs/kdos/README.md` | The table of contents, every page with a one-line description, plus three reading paths: *I want to use it*, *I want to build it*, *I want to change it*. |

### Part I — Philosophy

| Page | Contents |
|---|---|
| `01-philosophy/why-kdos.md` | The four properties — built from source with named exceptions, KDOS builds KDOS, the repo builds offline, applications live in boxes. Who it is for and who it is not for. The trade the reader accepts. What is deliberately absent. |
| `01-philosophy/principles.md` | The reasoning behind the rules: no systemd, no Xorg server, no GTK or Qt on the host, one implementation per idea, absent-never-partial, never invent a number, measure rather than assume, offline by construction, reproducible by construction, honesty about what cannot be enforced. Each principle states what it costs. |
| `01-philosophy/decisions.md` | Argued choices and the alternatives that lost: a labwc hard fork over a from-scratch compositor; one pack per application over a monolithic image; Debian over Alpine inside boxes; musl and what it forecloses; KDE on the host; no application store; release assets over Git LFS; per-machine measured `-march` over tiered builds; Alpine's security database over NVD; the shell as a file beside the recipe rather than inside it. |

### Part II — User guide

| Page | Contents |
|---|---|
| `02-user-guide/getting-started.md` | Obtaining or building an ISO, writing a medium, UEFI boot, the boot splash, the tty and the login banner, the two shipped accounts, starting a session with `kdos-desktop`, what to try first. There is no published ISO: the build path is the only path, and the page says so rather than implying a download. |
| `02-user-guide/installation.md` | Running `kinstall`: every wizard page, disk selection, the filesystem table, LUKS, swap, the applications page, users and locale, the summary and the point of no return. Answer files, `--unattended`, `--dry-run`, `--save`, `--dump`. A/B slots as installed. What the installer writes where. |
| `02-user-guide/desktop.md` | The panel and every widget, the Start menu, the launcher, window management and the window menu, workspaces, the full keybinding table, notifications and the notification centre, the clipboard, screenshots, lock, idle and power, displays, removable media and devices, the file chooser and file management, tooltips. |
| `02-user-guide/applications.md` | What an alien app is. `kdos app` in full — list, search, show, install, launch, remove, update, rollback, sources. Installing from the medium and from the Start menu. MIME handling and open-with. Terminal commands that live in boxes. Boxes from a user's point of view, including `kdos-box` for a scratch environment. What a first launch costs. |
| `02-user-guide/theming.md` | `kdos theme` and the four accents. What retints live and what waits for a relaunch. The CRT knobs. Wallpaper. Style files. Fonts, console and scalable. What is themed on the host, what is themed inside a box, and why the two routes differ. How the theme is generated, and `--audit`. |
| `02-user-guide/administration.md` | Services under `ksvc`. Networking and the firewall. Storage, swap, zram and removable media. Users, groups and `wheel`. Hardware enablement: firmware, microcode, `dialout`, the udev rules, and what each unlocks. Media codecs, colour management and time. Input methods. Updates and the binhost. `kdos doctor`, `kdos clone`, `kdos rebuild`, `kdos status`. |

### Part III — Architecture and core

| Page | Contents |
|---|---|
| `03-architecture/overview.md` | The three rings. Host versus box. A process map of a running session from PID 1 down. What each root daemon owns. The data that crosses the host/box boundary and the data that does not. Where every kind of state lives. |
| `03-architecture/boot-and-init.md` | Firmware to login: rEFInd, the kernel command line, the initramfs and its generated init, the splash, LUKS unlock, A/B slot selection and boot counting, `switch_root`, `rcS`, the `init.d` convention and every shipped script, `kdos-getty`, the console font and palette, the login banner. Microcode and firmware loading. `/tmp` and `/var/run`. |
| `03-architecture/session.md` | `kdos-desktop` and `kdos-desktop-start`. The per-user session bus. PipeWire. The portal stack and backend selection. Supervised chrome and the session templates. The XDG environment a box receives. Screen capture and clipboard globals. Input-method wiring end to end. |
| `03-architecture/packaging.md` | The ports tree and the recipe model. The package format and the database. The dependency solver. Recipe and build-config hashes and what they decide. Reproducible packages and the flags that make them so. The signed binhost, its index and its three equality tests. Deltas. Vulnerability tracking. |
| `03-architecture/packs-and-boxes.md` | The `.kpack` format field by field. Signing and verification, and where each happens. `kdos-packd`'s role. Composition, reference counting and the two mount routes. Grafts and data packs. The catalogue and how it is baked. Launcher generation. One box per application, and the podman arrangement underneath. What an install carries. |
| `03-architecture/security-model.md` | The trust model stated plainly. Signing keys and the two separate keyrings. The complete setuid inventory and what each one is for. `SO_PEERCRED` as the gate on every root daemon socket. Security-context sandboxing and the global allowlist. Rootless containers and their limits. Mount options. What KDOS does **not** protect, said out loud. |
| `03-architecture/design-language.md` | The character grid as a specification: the window frame, the chrome primitives, the palette slots and their contrast, emphasis, the pointer contract, hit maps, the three glyph tiers and the console font's limits, pictures as an enhancement layer, the compositor's own decoration, and the checklist a new surface is not finished without. |

### Part IV — Programs and applications

| Page | Contents |
|---|---|
| `04-programs/README.md` | A map of every binary KDOS itself ships — name, one line, link. Grouped: compositor and chrome, root daemons, packaging, build, tools. |
| `04-programs/kdos-comp.md` | The labwc fork: what is ours and what is upstream, and how to find the grafts. `comp.conf` and its live and startup-only keys. `rc.xml` and the bindings that must not be lost. The CRT pass. The wallpaper. The frames socket. The idle policy. Decorations and the generated theme. The command socket and `kdos hey`. Xwayland. Supervised children. Shutdown order. |
| `04-programs/kdos-shell.md` | One binary under 28 names, with the name table as the index. The panel: every widget, the layout passes and how it degrades, the meters, tiles, the tray, the overflow chevron, tooltips. The Start menu, launcher, desktop, file chooser and browser, run box, notification daemon and centre, OSD, calendar, settings, the device managers, the clipboard manager, the status popup. |
| `04-programs/kdos-res.md` | Ten pages of readings. Identity by box. The renderer that serves a tty, a window and a dump. Rates, gaps and what is never invented. The detail page and its verbs. `kdos-resctl`. Fixtures and goldens. |
| `04-programs/kdos-appbox.md` | `kdos-appbox` and `kdos-box` as one binary. The launch path in order. `genlaunchers` and its four outputs. Desktop-entry handling and `Exec` parsing. Box profiles key by key, and which map onto podman and which KDOS enforces itself. Freeze, import, clone, snapshot, gc, warmup. Storage drivers. |
| `04-programs/daemons.md` | The shape every root daemon shares, then a section each: `kdos-powerd`, `kdos-energyd`, `kdos-oomd`, `kdos-mountd`, `kdos-packd`, `kdos-boxsock`, and `xdg-desktop-portal-kdos`. Each states its socket, its verbs, its authorisation, its refusals and its fixture seam. |
| `04-programs/kinstall.md` | The installer's design: the probe, the page model, the forked child and its line protocol, the answer-file schema, the filesystem table, the applications step, `--dump probe` and `--dump plan`, the terminal it owns. |
| `04-programs/kdos-command.md` | `kdos` and all nineteen subcommands, each with what it measures, what it refuses to claim, and its exit codes: `theme`, `status`, `doctor`, `app`, `version`, `why`/`explain`, `sandbox`, `restarts`, `stutter`, `march`, `rebuild`, `clone`, `cve`, `trash`, `appid`, `hey`, `oracle`, `update`, `help`. |
| `04-programs/kdos-bb.md` | The forked AAlib demo: what it is, how to run it, the audio arrangement, and the aalib and mikmod facts that constrain anything else built on them. |

### Part V — Developer guide

| Page | Contents |
|---|---|
| `05-developer/developing.md` | Clone, `make bootstrap`, `make build`, `make run`. What the build container is and why the build is offline. Every `make` target. The iteration loops that avoid a full build, per area. Where logs land. What to read when something fails. |
| `05-developer/build-system.md` | Phases and their order. `kdosbuild`: the screens, the headless and JSON modes, the preview and selftest entry points. Phase snapshots. Build plans and what they narrow. The chroot and its environment. Replay. The `fs/` manifest guard and the orphan sweep. `libkbuild`'s share of the work. |
| `05-developer/writing-ports.md` | The recipe format in full: every key, helper expansion, sources and hashes, vendoring for each language, `postinstall.sh`, the canonical `build.sh` shapes per build system. Adding a port end to end. `ports/fetch` and its container. `ports/update` and `kdos-portup`. Publishing sources. |
| `05-developer/build-troubleshooting.md` | The catalogue of recurring failures, each as symptom, cause and canonical fix. |
| `05-developer/c-libraries.md` | The `libk*` set, one section each, with the dependency direction and the rule that keeps it. What each library owns, what it may link, and the invariants it exists to protect. How to add a library, and what moving one costs. |
| `05-developer/writing-desktop-software.md` | Writing a KDOS surface: libktui's model, the widget and event protocol, libkwl's roles and input rules, libkchrome's primitives, sprites and pixel tiles, icons. `--dump`, `--dump-cells` and goldens. Adding a name to `kdos-shell`. The look checklist restated as a procedure. |
| `05-developer/testing.md` | `testing/preflight.sh` and every check it makes. `testing/selftest.sh`, its invariants, sanitizer runs and its skips. Goldens and fixtures, and the fixture-root seam the readers share. The QEMU rig: `vnc-shot.py` in full, `packlane.sh`, `install-to-disk.sh`, `appsweep.sh`. What each proves and what it cannot. |

### Part VI — Reference

| Page | Contents |
|---|---|
| `06-reference/command-index.md` | Every command the system ships, alphabetically: name, one line, link to its page. |
| `06-reference/configuration.md` | Every configuration file and every key: path, owner, keys with types and defaults, and whether a change applies live, on SIGHUP, or at next start. |
| `06-reference/filesystem-and-ipc.md` | The target filesystem layout: `/etc/kdos`, `/var/lib/kdos`, `/usr/share/kdos`, `/run`. Then the complete IPC surface — every socket, its path, its authorisation, its verbs and its reply grammar. |
| `06-reference/repository-layout.md` | The repository tree, annotated directory by directory. |
| `06-reference/known-gaps.md` | What does not exist, in present tense, so a reader stops looking. |
| `06-reference/roadmap.md` | Direction, including the aarch64 mobile target from `mobile.plan.md`, stated as intent and clearly separated from what ships. |
| `06-reference/status.md` | Maturity per subsystem — stable, in progress, or experimental — with one line each saying what that verdict rests on. The only page that carries the release line. |
| `06-reference/glossary.md` | pack, box, graft, ring, phase, tier, slot, shim, alien app, appbox, pack lane, accent, cell grid, and the rest of the vocabulary these documents use. |

---

## `CLAUDE.md` after the migration

Target 350–450 lines, containing only what is not knowledge about how the
system works:

1. **Identity and the doc map** — one line per chapter, so an agent knows which
   page to open before touching an area.
2. **Hard rules**, including two that govern documentation itself: no document
   or comment records history, and **every change updates its documentation in
   the same change, replacing the old description rather than appending to
   it**.
3. **Conventions** — the recipe format in brief, code and comment style, the
   "never do X" list that is about editing rather than about the system.
4. **Agentic workflow** — build and rebuild incantations, the rig recipes, what
   must not be done while a rebuild runs, where logs are.
5. **Working-state markers** and **"When the user says…"**.

**Acceptance: no fact is lost.** Every one of the 100 headed sections in the current
`CLAUDE.md` is accounted for by the mapping below — relocated, or deliberately
dropped as history. Some rows below cover a section together with its
subsections.

### Migration map

| `CLAUDE.md` section | Destination |
|---|---|
| What KDOS is | `01-philosophy/why-kdos.md` |
| Hard rules | **stays**, reasoning to `01-philosophy/principles.md` |
| The Three Rings | `03-architecture/overview.md` |
| The application catalogue | `03-architecture/packs-and-boxes.md` |
| Where the big artefacts live | `05-developer/developing.md`, argument to `01-philosophy/decisions.md` |
| fetch and bake run in containers | `05-developer/build-system.md` |
| genlaunchers writes four things | `04-programs/kdos-appbox.md` |
| Runtime plumbing | `04-programs/kdos-appbox.md` |
| Packs — an application is one file | `03-architecture/packs-and-boxes.md` |
| The whiteout convention | `03-architecture/packs-and-boxes.md` |
| kdos-packd | `04-programs/daemons.md` |
| The box a pack runs in | `03-architecture/packs-and-boxes.md` |
| kdos-box | `04-programs/kdos-appbox.md` |
| Boxes in the desktop | `04-programs/kdos-comp.md`, `kdos-shell.md`, `kdos-res.md` |
| kdos-appbox is a C program | `04-programs/kdos-appbox.md` |
| Session, boot and console; the session bus | `03-architecture/session.md` |
| /tmp must be mounted mode=1777 | `03-architecture/boot-and-init.md` |
| -march, measured | `04-programs/kdos-command.md` |
| kdos rebuild | `04-programs/kdos-command.md` |
| kdos clone | `04-programs/kdos-command.md` |
| A/B root slots | `03-architecture/boot-and-init.md` |
| An encrypted root | `03-architecture/boot-and-init.md` |
| switch_root | `03-architecture/boot-and-init.md` |
| Firmware | `02-user-guide/administration.md` |
| dialout and the udev rules | `02-user-guide/administration.md` |
| CPU microcode | `03-architecture/boot-and-init.md` |
| The boot splash | `03-architecture/boot-and-init.md` |
| Console font: kdos-getty | `03-architecture/boot-and-init.md` |
| The login banner | `03-architecture/boot-and-init.md` |
| The KDOS look, all nine rules | `03-architecture/design-language.md` |
| Theming — PHOSPHOR | `02-user-guide/theming.md`, palette to `03-architecture/design-language.md` |
| kdos-icons | `02-user-guide/theming.md` |
| kdos-cursors | `02-user-guide/theming.md` |
| Codecs, colour and time | `02-user-guide/administration.md`, recipe rule to `05-developer/writing-ports.md` |
| The kdos command | `04-programs/kdos-command.md` |
| The C libraries | `05-developer/c-libraries.md` |
| libktui's three glyph tiers | `05-developer/c-libraries.md`, `03-architecture/design-language.md` |
| libkcolor is the one palette | `05-developer/c-libraries.md` |
| kdos-theme — the generators | `02-user-guide/theming.md` |
| kdos-bb | `04-programs/kdos-bb.md` |
| kinstall | `04-programs/kinstall.md`, use to `02-user-guide/installation.md` |
| The compositor is a hard fork of labwc | `04-programs/kdos-comp.md` |
| Lock, idle and power | `04-programs/kdos-comp.md`, `daemons.md`, `02-user-guide/desktop.md` |
| The CRT pass | `04-programs/kdos-comp.md` |
| Stutter attribution | `04-programs/kdos-command.md`, `kdos-comp.md`, `06-reference/filesystem-and-ipc.md` |
| Ending the session | `04-programs/kdos-comp.md` |
| Screen capture, clipboard, portal | `03-architecture/session.md`, `security-model.md`, `04-programs/daemons.md` |
| Input methods — the compositor is the wire | `04-programs/kdos-comp.md` |
| The input method — fcitx5 | `03-architecture/session.md`, use to `02-user-guide/administration.md` |
| Which app is using your microphone | `04-programs/kdos-shell.md` |
| Per-app Energy Impact | `04-programs/daemons.md` |
| Memory pressure — kdos-oomd | `04-programs/daemons.md` |
| The resource monitor | `04-programs/kdos-res.md` |
| The shell answers the mouse | `04-programs/kdos-shell.md`, contract to `03-architecture/design-language.md` |
| Tiles | `04-programs/kdos-shell.md`, `05-developer/writing-desktop-software.md` |
| The notification centre | `04-programs/kdos-shell.md` |
| One taskbar, a Start menu, pictures | `04-programs/kdos-shell.md` |
| Pixel icons | `05-developer/writing-desktop-software.md`, `03-architecture/design-language.md` |
| The device managers | `04-programs/kdos-shell.md` |
| Removable media — kdos-mountd | `04-programs/daemons.md` |
| /var/run IS /run | `03-architecture/boot-and-init.md` |
| vnc-shot.py | `05-developer/testing.md` |
| The tray | `04-programs/kdos-shell.md` |
| Repo layout | `06-reference/repository-layout.md` |
| Build system, snapshots, plans, fs sync, orphan sweep | `05-developer/build-system.md` |
| libkbuild | `05-developer/c-libraries.md`, `build-system.md` |
| kdosbuild | `05-developer/build-system.md` |
| preflight.sh, selftest.sh | `05-developer/testing.md` |
| kpkg is C | `03-architecture/packaging.md` |
| One recipe format | `05-developer/writing-ports.md`, argument to `01-philosophy/decisions.md` |
| The binhost | `03-architecture/packaging.md`, `security-model.md` |
| Deltas | `03-architecture/packaging.md` |
| kdos cve | `04-programs/kdos-command.md`, `03-architecture/packaging.md` |
| Reproducible packages | `03-architecture/packaging.md` |
| kpkg and recipe conventions | `05-developer/writing-ports.md`, summary **stays** |
| Canonical build.sh shapes | `05-developer/writing-ports.md` |
| kdos-portup | `05-developer/writing-ports.md` |
| Recurring build fixes | `05-developer/build-troubleshooting.md` |
| Init scripts | `03-architecture/boot-and-init.md`, `02-user-guide/administration.md` |
| Networking | `02-user-guide/administration.md` |
| Users and login | `02-user-guide/administration.md` |
| Testing and the VM rig | `05-developer/testing.md` |
| Working-state markers | **stays** |
| Outstanding gaps / TODO | `06-reference/known-gaps.md` |
| When the user says… | **stays** |

### Content the book adds that `CLAUDE.md` does not carry

`libkicon` and `libkchrome`, which `CLAUDE.md` does not describe; the aarch64
mobile direction, from `mobile.plan.md`; the getting-started build path; the
command index; the configuration reference; the filesystem and IPC reference;
the per-subsystem status table; the glossary.

---

## House style

Present tense, second person where instructing, third person where describing.
No changelog, no "we decided", no marketing, no exclamation. Every page opens
with one paragraph saying what it covers and who needs it, and ends with a
**See also** list of relative links. Terms are used exactly as the glossary
defines them. Code blocks show real commands with real output where output was
observed. Measurements carry their conditions. Tables are used for anything
enumerable. British spelling, matching the existing prose.

Warnings are written as the rule and its consequence:

> Bind the layer shell at version 4. An older resource answers a request for
> on-demand keyboard interactivity with exclusive, which parks the seat's
> keyboard on the surface and stops every window receiving input.

---

## Screenshots

All images live in `docs/screenshots/` and are referenced by relative path.
Eleven exist. The set is completed from a **live boot of
`build/iso-build/kdos.iso`**, using `testing/vnc-shot.py` inside the
`kdos-qemu-py:latest` container, in batches of many shots per boot.

**Not from the installed disks.** Both `build/kdos-target.qcow2` and
`build/kdos-be-target.qcow2` were converted into testbeds for the removed
second desktop — the rig scripts under `build/be-*.sh` target the first by name
— so both boot a desktop that must not appear in this documentation. The ISO is
the pristine shipped system, which is the better subject anyway. Its cost is a
slower boot and that a live session cannot create a persistent box, which is a
stated gap rather than a defect.

The installer is captured by running `kinstall --dry-run` on a spare tty, which
exercises the real pages and writes nothing — rather than by running a real
install, which would destroy the disk the other shots come from. An ISO is
present at `build/iso-build/kdos.iso` and the rig attaches it alongside the
disk, so a live-medium shot is available if one is wanted.

Target set: the Start menu at its three states, the launcher, the window menu, the root menu, the desktop
menu, the notification centre and a toast, the calendar, the volume slider,
the network, bluetooth and devices popups, the clipboard manager, the status
overflow popup, the window list, display settings, the keybinding sheet,
settings at its grid and at a page, `kdos-res` on Applications, Boxes and a
detail page, and terminal captures of `kdos doctor`, `kdos stutter`,
`kdos-energy`, `kdos app list` and the tty banner. Plus the remaining two
accents and a CRT on/off pair.

A shot that cannot be captured is dropped and its page written without it. No
image is staged, composited or otherwise made to show something the system does
not do.

---

## Delivery

Nine passes, each ending at a review gate. Capture scripts written during pass 7
are throwaway and live under `build/`.

| Pass | Work |
|---|---|
| 1 | Directory scaffold and `docs/kdos/README.md`; Part I written |
| 2 | Part II — user guide |
| 3 | Part III — architecture |
| 4 | Part IV — programs |
| 5 | Part V — developer guide |
| 6 | Part VI — reference |
| 7 | Screenshots captured on the rig and placed |
| 8 | Root `README.md` rewritten |
| 9 | `CLAUDE.md` rewritten; every relative link walked, every named path checked to exist, every count re-verified |

`CLAUDE.md` is last so that every link it makes points at a page that exists.

---

## Facts as of writing

Correct at the time this design was written, and to be re-checked in pass 9
rather than trusted:

| | |
|---|---|
| ports in `ports/core` | 764 |
| packages in the built tree | 758 |
| application packs | 181 |
| shared runtimes | 7 |
| base packs | 2 |
| kernel | 7.0.10 |
| names `kdos-shell` answers to | 28 |
| `kdos` subcommands | 19 |
| KDOS-authored C libraries | 13 |
| desktop sessions | 1 |

---

## Acceptance criteria

1. Every one of the 100 headed sections of `CLAUDE.md` at HEAD is accounted for
   by the migration map, and every fact in it appears in the book or was
   dropped as history.
2. No relative link is dead. Every file path, port, binary and configuration
   key named in the book exists in the tree.
3. No page narrates history. A search for "used to", "no longer", "now
   finally", "was broken" and "cost a debug cycle" returns nothing.
4. Every count in the book matches the tree at pass 9.
5. `CLAUDE.md` is under 450 lines and carries no knowledge the book carries.
6. Every page has an opening paragraph and a **See also** list.
7. No page mentions `kdos-be`, a second desktop session, `KDOS_SESSION`,
   `kdos-bar` or `kdos-files`.
8. Nothing is committed.
