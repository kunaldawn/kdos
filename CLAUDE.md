# KDOS — agent briefing

Rules, conventions and workflow for working on this tree. **This file is not a
description of how KDOS works** — that is the book under `docs/kdos/`, and it is
the authority. This file tells you which page to open, what you may not do, and
how to build and test what you change.

Read the map below before touching anything.

---

## Read this before touching anything

| Working on… | Read |
|---|---|
| Anything at all, first time | [`docs/kdos/README.md`](docs/kdos/README.md) — the book's index and three reading paths |
| Why the system is shaped this way | [`01-philosophy/why-kdos.md`](docs/kdos/01-philosophy/why-kdos.md), [`principles.md`](docs/kdos/01-philosophy/principles.md), [`decisions.md`](docs/kdos/01-philosophy/decisions.md) |
| The boot path, initramfs, splash, A/B slots, the console | [`03-architecture/boot-and-init.md`](docs/kdos/03-architecture/boot-and-init.md) |
| The session bus, portals, audio, supervised chrome, the box environment | [`03-architecture/session.md`](docs/kdos/03-architecture/session.md) |
| Ports, packages, the solver, reproducibility, the binhost, deltas | [`03-architecture/packaging.md`](docs/kdos/03-architecture/packaging.md) |
| The pack format, the bake, mounting, composition, grafts | [`03-architecture/packs-and-boxes.md`](docs/kdos/03-architecture/packs-and-boxes.md) |
| Signing, setuid, daemon authorisation, the sandbox | [`03-architecture/security-model.md`](docs/kdos/03-architecture/security-model.md) |
| Where a window goes, tiling, snapping, workspaces | [`03-architecture/window-model.md`](docs/kdos/03-architecture/window-model.md) |
| **Drawing anything** — colour, chrome, the pointer contract, glyph tiers | [`03-architecture/design-language.md`](docs/kdos/03-architecture/design-language.md) |
| The compositor and its grafts | [`04-programs/kdos-comp.md`](docs/kdos/04-programs/kdos-comp.md) |
| The console desktop, its two sockets, the greeter | [`04-programs/kdos-con.md`](docs/kdos/04-programs/kdos-con.md) |
| The panel and its 29 surfaces | [`04-programs/kdos-shell.md`](docs/kdos/04-programs/kdos-shell.md) |
| The resource monitor | [`04-programs/kdos-res.md`](docs/kdos/04-programs/kdos-res.md) |
| The terminal, its keys and clipboards, and pictures in one | [`04-programs/kdos-term.md`](docs/kdos/04-programs/kdos-term.md) |
| A graphical application on the console, and the kiosk that holds it | [`04-programs/kdos-cage.md`](docs/kdos/04-programs/kdos-cage.md) |
| Launching boxed apps, launcher generation, box profiles | [`04-programs/kdos-appbox.md`](docs/kdos/04-programs/kdos-appbox.md) |
| Any root daemon | [`04-programs/daemons.md`](docs/kdos/04-programs/daemons.md) |
| The installer | [`04-programs/kinstall.md`](docs/kdos/04-programs/kinstall.md) |
| The `kdos` command and its subcommands | [`04-programs/kdos-command.md`](docs/kdos/04-programs/kdos-command.md) |
| Build targets and the fast iteration loops | [`05-developer/developing.md`](docs/kdos/05-developer/developing.md) |
| Phases, the chroot, snapshots, build plans | [`05-developer/build-system.md`](docs/kdos/05-developer/build-system.md) |
| **Writing or changing a recipe** | [`05-developer/writing-ports.md`](docs/kdos/05-developer/writing-ports.md) |
| **A build that failed** | [`05-developer/build-troubleshooting.md`](docs/kdos/05-developer/build-troubleshooting.md) |
| The `libk*` libraries | [`05-developer/c-libraries.md`](docs/kdos/05-developer/c-libraries.md) |
| **Writing a new surface** | [`05-developer/writing-desktop-software.md`](docs/kdos/05-developer/writing-desktop-software.md) |
| Tests, fixtures, goldens, the rig | [`05-developer/testing.md`](docs/kdos/05-developer/testing.md) |
| Every command / config key / socket / path | [`06-reference/`](docs/kdos/06-reference/command-index.md) |
| What does not exist | [`06-reference/known-gaps.md`](docs/kdos/06-reference/known-gaps.md) |

**If a page and the tree disagree, measure the tree and fix the page in the same
change.** A stale-optimistic page sends the next reader past a bug; a
stale-pessimistic one makes them re-verify something that already works.

---

## Hard rules — do not violate

1. **Every change updates its documentation, in the same change.** A change in
   behaviour updates every page under `docs/kdos/`, every code comment and every
   shipped configuration file that describes that behaviour — before the change
   is done, not afterwards and not in a follow-up. **The update REPLACES the old
   description.** It does not append to it, annotate it, or record what the
   description was. A comment that contradicts its code is a claim the next
   reader will act on.

2. **No document, comment or shipped file records history.** Not a previous
   state, not which bug a line fixed, not how a lesson was learned. Write the
   **constraint** — what the code does and what breaks if it changes — never the
   **changelog**.

   ```c
   /* WRONG — narrates a past defect */
   /* This read the buffer size before the offset, which overflowed on a
    * full buffer and reported EOF. Fixed by clamping first. */

   /* RIGHT — states the rule and its consequence */
   /* Clamp before the read: a full buffer would otherwise ask for a
    * zero-length read and take the result for EOF. */
   ```

   Both carry the same warning. Only the second is still true in five years, and
   only the second survives the surrounding code being rewritten.

3. **No systemd.** No `systemd-*` on the host. See
   [principles](docs/kdos/01-philosophy/principles.md#no-systemd) for the
   replacements and what the choice costs.

4. **No Xorg server.** No `xorg-server`, no display manager, nothing X on the
   login path, and `fs/etc/X11/` must never exist. Xwayland is the single
   carve-out; a recipe that wants the X client libraries for anything else gets
   pushed back.

5. **No GTK and no Qt on the host.** Graphical applications go in a box.

6. **No rationale comments in a recipe.** Banner header plus the metadata keys.
   Reasoning belongs in a commit message or in the book.

7. **Do not auto-commit.** The user commits manually, often squashing many edits
   into one logical commit.

8. **Do not run destructive git operations without asking.** No `reset --hard`,
   `clean -f`, `branch -D`, force pushes.

9. **No source edits with stream editors.** Use build flags. Patch only when
   there is genuinely no flag, and then ship a real `.patch` beside the recipe.

10. **Be terse in responses.** State what changed; the user reads the diff.

---

## Conventions

### Recipes

A port is two files. `kpkgbuild` is declarative metadata that is **parsed, never
sourced**; `build.sh` beside it is ordinary bash with the working directory set
to the unpacked source.

```
name        = foo
version     = 1.2.3
release     = 1
source      = https://upstream.example/$name-$version.tar.gz
sha256      = <hash>  foo-1.2.3.tar.gz
description = <one line>
homepage    = <URL>
depends     = zlib openssl
```

Helpers go **between `release` and `source`** — a helper may read `$version`,
and `source` may read the helper. Command substitution is not available; use the
pattern operators.

Every key, the canonical build shapes per build system, vendoring, and the
end-to-end walkthrough are in
[`writing-ports.md`](docs/kdos/05-developer/writing-ports.md).

### Code

- **Prefixed symbols.** Every exported symbol in a `libk*` library carries its
  library's prefix. Unprefixed generic names have already made two of our own
  programs unlinkable together.
- **No `system()` and no shell** in a program that handles names from desktop
  entries or from a command line. Everything executes through the argument-vector
  builder.
- **Colour comes from a slot, never a literal.** `KT_MID` for labels; `KT_DIM`
  is a fill.
- **Match the surrounding code** — its comment density, its naming, its idiom.

### Comments

State the rule and its consequence. Never the story. See hard rule 2.

---

## Build and iteration

```sh
make bootstrap                                   # fetch sources — network, once
make build                                       # everything — no network
make build BUILD_ARGS=--fresh                    # skip the picker
make build BUILD_ARGS="--continue-from 04_phase4"
make run            # plain graphics: no phosphor pass
make run-hw         # accelerated: the pass is on
```

**Narrow it.** A full build is hours and almost nothing needs one:

| Changed | Run |
|---|---|
| Something under `fs/` | `make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"` |
| One port | `make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild <port>"` |
| A desktop program | `make build BUILD_ARGS="--phases 05_desktop --rebuild <port>"` |
| Only packaging | `make build BUILD_ARGS="--phases 06_packaging"` |

**Rules while building:**

- **Never edit a port's sources while its rebuild is running.** The recipe hash
  is taken at install time.
- **Never re-run an early phase on a tree already ahead of it** — its snapshot
  would be overwritten with a later tree under the earlier name. That is what
  `--continue-from` is for.
- **A build started inside a backgrounded call dies with it.**
- **Editing a library rebuilds every port of ours**, not only its consumers.
- **Clear `ports/.kpkg-meta`, `.portup`, `.portup-tools`, `.kpkgbin` when
  switching between a container run and a host run** — a binary built against one
  C library cannot execute under the other, and the failure does not say so. A
  container run as root leaves them root-owned, so remove them **from a
  container** rather than reaching for sudo.
- **A new opt-in packaging flag is two edits, not one.** The chroot is entered
  with a cleared environment, so a variable must be named in `script/chroot_exec.sh`
  as well as passed by the `Makefile`, or it reaches every host step and no
  chroot one.

**Before believing a change:**

```sh
testing/preflight.sh                     # the wiring, in seconds
testing/selftest.sh                      # libraries and consumers, ~30s
CC="cc -fsanitize=address,undefined -g" testing/selftest.sh   # when you touch a parser
```

---

## The rig

```sh
testing/rig-image.sh          # builds kdos-qemu-py:latest; network, once
R="docker run --rm --device /dev/kvm -v $PWD:/kdos -w /kdos kdos-qemu-py:latest \
   python3 testing/vnc-shot.py --audio --size 1920x1080"
$R --wait 24 --cmd 'kdos-res' --sleep 4 --shot build/shots/x.png
```

Boot the **ISO** (`build/iso-build/kdos.iso`), not the installed disk images
under `build/`: both are leftover testbeds and boot software that is not in this
tree. The ISO is the pristine shipped system.

**Traps, each a rule with its consequence:**

- **The rig writes raw PPM despite the `.png` extension.** Convert before use.
- **The welcome card is open on first login.** Dismiss it before any other shot.
- **A pattern kill matching a monitoring loop's own search term kills the
  monitor.**
- **An exact-name kill cannot match a name past 15 characters**, so a restart
  using one restarts nothing and the stale program keeps answering.
- **The harness kills the emulator without a shutdown**, so a root script must
  `sync` after writing to the disk or the filesystem leaves the rewritten files
  zero-length on the next boot.
- **A pipeline reading a supervised service's output never returns**, because the
  supervisor keeps the pipe open.
- **A static screen produces no frame events**, so anything about dropped frames
  needs something animating first.
- **A plain virtual display puts the compositor on software rendering**, so the
  phosphor pass is never in a rig photograph.
- **Driving the session means a *login* shell** (`su - kdos -c`), or the
  container tooling resolves the home directory to `/` and every call fails.
- **Goldens regenerate only where the Wayland dependencies exist** — a build
  container, not a bare host.

Everything else about the harnesses is in
[`testing.md`](docs/kdos/05-developer/testing.md).

---

## Working-state markers

```bash
ls ports/core | wc -l                                  # ports
ls build/fs/var/lib/kpkg/db/ | wc -l                   # installed packages
git status --short | wc -l                             # tracked changes
ls build/logs/04_phase4/*.log                          # which packages have logs
tail -40 build/logs/04_phase4/<N>_<pkg>.install.log    # debug a failure
bash testing/docscheck.sh                              # the book: links, prose, page contract
```

---

## When the user says…

- **"fix"** + a build log path → read its tail, identify the root cause, apply
  the targeted fix in the relevant recipe. Don't speculate past the log. Check
  [`build-troubleshooting.md`](docs/kdos/05-developer/build-troubleshooting.md)
  first — most failures here are one of those.
- **"audit"** → grep across recipes, phase lists and `fs/` for residual
  references. Be systematic.
- **"add X"** → find the canonical upstream URL, pick the latest stable version,
  write a recipe matching
  [`writing-ports.md`](docs/kdos/05-developer/writing-ports.md), and wire it into
  `depends` and the right `packages.txt`.
- **"why is X off"** → usually because dependencies weren't present when the port
  was added. Name the missing dependencies and offer to add them; don't pretend
  it was a deliberate choice.

**And whatever the request:** if it changes behaviour, it changes documentation
in the same breath. Hard rule 1.

Subagents in this harness can't run bash, so dispatch-and-review wastes time.
Just do the work inline.
