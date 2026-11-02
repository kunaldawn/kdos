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
| Anything at all, first time | [`docs/kdos/README.md`](docs/kdos/README.md) — the book's title page, reading paths and table of contents |
| Why the system is shaped this way | [`01-philosophy/why-kdos.md`](docs/kdos/01-philosophy/why-kdos.md), [`principles.md`](docs/kdos/01-philosophy/principles.md), [`decisions.md`](docs/kdos/01-philosophy/decisions.md) |
| How KDOS differs from other distributions, and why | [`01-philosophy/how-kdos-differs.md`](docs/kdos/01-philosophy/how-kdos-differs.md) |
| The boot path, initramfs, splash, A/B slots, the login | [`03-architecture/boot-and-init.md`](docs/kdos/03-architecture/boot-and-init.md) |
| The login, elogind, per-user services (bus, PipeWire), portals, the box environment | [`03-architecture/session.md`](docs/kdos/03-architecture/session.md) |
| Ports, packages, the solver, reproducibility, the binhost, deltas | [`03-architecture/packaging.md`](docs/kdos/03-architecture/packaging.md) |
| The pack format, the bake, mounting, composition, grafts | [`03-architecture/packs-and-boxes.md`](docs/kdos/03-architecture/packs-and-boxes.md) |
| Signing, setuid, daemon authorisation, the sandbox | [`03-architecture/security-model.md`](docs/kdos/03-architecture/security-model.md) |
| Where a window goes, tiling, snapping, workspaces | [`03-architecture/window-model.md`](docs/kdos/03-architecture/window-model.md) |
| **Drawing anything** — the KDOS look kit: accents and colour schemes, Plasma style, decoration, fonts, Phosphor mode | [`03-architecture/design-language.md`](docs/kdos/03-architecture/design-language.md) |
| KWin, its effects and the KDOS decoration | [`04-programs/kwin.md`](docs/kdos/04-programs/kwin.md) |
| The Plasma shell, layouts, KDOS plasmoids and the runner | [`04-programs/plasma-shell.md`](docs/kdos/04-programs/plasma-shell.md) |
| KDOS settings pages (KCMs) | [`04-programs/kdos-settings.md`](docs/kdos/04-programs/kdos-settings.md) |
| Discover, updates and the A/B slots | [`04-programs/discover.md`](docs/kdos/04-programs/discover.md) |
| Launching boxed apps, launcher generation, box profiles | [`04-programs/kdos-appbox.md`](docs/kdos/04-programs/kdos-appbox.md) |
| Any root daemon | [`04-programs/daemons.md`](docs/kdos/04-programs/daemons.md) |
| The installer | [`04-programs/kinstall.md`](docs/kdos/04-programs/kinstall.md) |
| The `kdos` command and its subcommands | [`04-programs/kdos-command.md`](docs/kdos/04-programs/kdos-command.md) |
| The build end to end, from clone to ISO | [`05-developer/how-kdos-is-built.md`](docs/kdos/05-developer/how-kdos-is-built.md) |
| Build targets and the fast iteration loops | [`05-developer/developing.md`](docs/kdos/05-developer/developing.md) |
| Phases, the chroot, snapshots, build plans | [`05-developer/build-system.md`](docs/kdos/05-developer/build-system.md) |
| **Writing or changing a recipe** | [`05-developer/writing-ports.md`](docs/kdos/05-developer/writing-ports.md) |
| Choosing a port's shelf (`ports/core/<shelf>/<name>/`, the closed list in `ports/shelves`) | [`05-developer/writing-ports.md`](docs/kdos/05-developer/writing-ports.md) |
| **A build that failed** | [`05-developer/build-troubleshooting.md`](docs/kdos/05-developer/build-troubleshooting.md) |
| The `libk*` libraries | [`05-developer/c-libraries.md`](docs/kdos/05-developer/c-libraries.md) |
| **Writing KDOS desktop code** — a KCM, plasmoid, runner or KWin effect | [`05-developer/writing-desktop-software.md`](docs/kdos/05-developer/writing-desktop-software.md) |
| Tests, fixtures, AT-SPI and QtTest, the rig | [`05-developer/testing.md`](docs/kdos/05-developer/testing.md) |
| Every command / config key / socket / path | [`06-reference/`](docs/kdos/06-reference/command-index.md) |
| Every port, by shelf and phase | [`06-reference/ports-catalogue.md`](docs/kdos/06-reference/ports-catalogue.md) |
| What does not exist | [`06-reference/known-gaps.md`](docs/kdos/06-reference/known-gaps.md) |
| *KDOS from Scratch* — the second book, phase by phase and port by port, rendered by `script/book/` | [`docs/kfs/README.md`](docs/kfs/README.md); its renderer, data snapshot and targets in [`developing.md`](docs/kdos/05-developer/developing.md#make-targets) |
| **Writing a port's `kfs.md`**, or a patch's `Subject:` header | [`05-developer/writing-ports.md`](docs/kdos/05-developer/writing-ports.md#the-ports-note-kfsmd) |
| The book checks: `docscheck --book`, `anchorcheck`, `booknav`, `kfscheck`, the patch gate | [`05-developer/testing.md`](docs/kdos/05-developer/testing.md#kfscheckpy) |

`docs/kdos/` stays the authority on mechanism and rationale. `docs/kfs/` owns
build steps, per-package pages and task procedures, and links to `docs/kdos`
for everything else rather than restating it.

**If a page and the tree disagree, measure the tree and fix the page in the same
change.** A stale-optimistic page sends the next reader past a bug; a
stale-pessimistic one makes them re-verify something that already works.

---

## Hard rules — do not violate

1. **Every change updates its documentation, in the same change.** A change in
   behaviour updates every page under `docs/kdos/` and `docs/kfs/`, every code comment and every
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

3. **No systemd init, unit files or service manager.** PID 1 is sysvinit;
   services are `rcS` scripts supervised by `ksvc`, and per-user services run
   under turnstile's `ksvc` backend. `elogind` is the one logind provider and
   `basu` the one sd-bus library. A KDOS daemon may serve a documented subset
   of a freedesktop D-Bus API that systemd also serves (`sysupdate1`,
   `hostname1`, `timedate1`, `locale1`); `kpkg` refuses `usr/lib/systemd`, so
   no recipe installs a unit. See
   [principles](docs/kdos/01-philosophy/principles.md#no-systemd-init-units-or-service-manager)
   for each replacement and what the choice costs.

4. **No Xorg server.** No `xorg-server` and nothing X on the login path. SDDM
   is the display manager, built against `elogind`, and runs its greeter on
   `kwin_wayland` (`DisplayServer=wayland`); there is no X11 session.
   `fs/etc/X11/` must never exist and `kpkg` refuses `etc/X11`. Xwayland is
   the one X server: KWin starts it for boxed applications and host
   applications that have no Wayland path. X client libraries may be linked
   by any application that needs them.

5. **The desktop is KDE Plasma; KDOS's desktop code is Qt 6 and KDE
   Frameworks.** Plasma, KWin and KDE Gear run Wayland-only. KDOS code that
   is part of the desktop (KCMs, plasmoids, the KRunner runner, KWin effects
   and scripts, the window decoration, Discover backends, the SDDM theme and
   the graphical installer front end) lives under `src/desktop/` and may link
   Qt 6, KDE Frameworks 6 and Plasma's libraries, and no other toolkit. Code
   under `src/system/`, `src/daemons/` and `src/art/`, every `libk*` library
   and every program that runs as root or before the login links no toolkit.
   Applications may be ported natively with GTK 3/4, libadwaita, Qt 5/6, KDE
   Frameworks, wxWidgets, FLTK or Tk, built with their Wayland backend as the
   run-time default. A `.patch` against a KDE module is allowed under rule 9
   when no flag exists; it is offered upstream at the same time, and its
   `kfs.md` names the merge request. Why, and what is ported, is in
   [`principles.md`](docs/kdos/01-philosophy/principles.md#the-desktop-is-kde-plasma)
   and [`applications.md`](docs/kdos/02-user-guide/applications.md).

6. **No rationale comments in a recipe.** Banner header plus the metadata keys
   or the commands, in `kpkgbuild` and `build.sh` alike. Why a port passes a
   flag or carries a patch, and the traps in its build, go in its `kfs.md`
   beside the recipe — never in `kpkgbuild` or `build.sh`. `kfs.md` is not
   hashed, so editing it rebuilds nothing, and `kfscheck` fails when a flag it
   explains is gone from `build.sh`. A new or changed port with a patch, a
   vendor bundle, a `build.sh` over 30 lines or 20 reverse dependencies needs
   one. Other reasoning belongs in a commit message or in the book. Every new
   or changed `.patch` opens with a `Subject:` line saying what it changes;
   preflight and the pre-push hook refuse one without. See
   [`writing-ports.md`](docs/kdos/05-developer/writing-ports.md#the-ports-note-kfsmd).

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
to the unpacked source. An optional third, `kfs.md`, holds the port's About,
Flags, Patches and Notes sections for *KDOS from Scratch* (hard rule 6).

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
  programs unlinkable together. KDOS Qt code is in the C++ namespace `Kdos`;
  its D-Bus names and polkit actions are `io.kdos.*`, its KCM ids `kcm_kdos_*`,
  its plugin ids `org.kdos.*` and its translation domains `kdos-<name>`.
- **No `system()` and no shell** in a program that handles names from desktop
  entries or from a command line. Everything executes through the argument-vector
  builder, and Qt code through `QProcess` with a program and an argument list,
  never `sh -c` or a command string.
- **Colour comes from a slot or a role, never a literal.** In `libktui` code
  `KT_MID` is for labels and `KT_DIM` is a fill. In Qt code QML reads
  `Kirigami.Theme` roles and C++ reads `KColorScheme`, and a shader takes its
  colours as uniforms; the KDOS accents reach Plasma only as the `.colors`
  files generated from `libkcolor`'s palette, and a colour literal in
  `src/desktop` QML or C++ fails preflight.
- **Match the surrounding code** — its comment density, its naming, its idiom.

### Comments

State the rule and its consequence. Never the story. See hard rule 2.

---

## Build and iteration

```sh
git config core.hooksPath script/hooks           # once per clone: the pre-push layout, patch-header, book and source checks
make fetch                                       # every source; networked, as is the first builder-image build
make fetch-check                                 # offline: what is missing or wrong
make build                                       # everything — no network
make build BUILD_ARGS=--fresh                    # skip the picker
make build BUILD_ARGS="--continue-from 41_system"
make run            # plain graphics: KWin on llvmpipe, Phosphor mode cannot load
make run-hw         # virgl: Phosphor mode and the motion effects run
make book BUILDFS=build/fs     # KDOS from Scratch into build/kfs: html/, kfs.pdf, kfs-html.tar.zst (~2 min)
make book-data BUILDFS=build/fs  # rewrite docs/kfs/data from a built tree; after a build
make book-stamp                  # rewrite docs/kfs/data/recipes.stamp alone; no built tree
```

`make book` renders with the pandoc and typst inside `BUILDFS`, run through
that tree's own loader; `KFS_PANDOC` and `KFS_TYPST` replace them. Package
pages are generated from the recipes on every render and never committed;
only `docs/kfs/data/` is, and its `recipes.stamp` goes stale on every edit
to a recipe, a `kfs.md`, a phase list or a file a page includes, which
`kfscheck` and the pre-push hook refuse until `make book-stamp` (or
`book-data`) has run. An include range moved by an edit above it is fixed
with `python3 testing/kfscheck.py --repin`.

**Narrow it.** A full build is hours and almost nothing needs one:

| Changed | Run |
|---|---|
| Something under `fs/` | `make build BUILD_ARGS="--phases 10_bootstrap,70_image --steps 10_bootstrap:000_file_system.sh"` |
| One port | `make build BUILD_ARGS="--phases <phase>,70_image --rebuild <port>"` — `<phase>` is the one whose list names the port (`grep -rlx <port> script/phases/*/packages*`), e.g. `41_system` |
| KDOS desktop code (`src/desktop`) | `make build BUILD_ARGS="--phases 50_desktop --rebuild <port>"` |
| A KDE module | `make build BUILD_ARGS="--phases <phase>,70_image --rebuild <port>"`; a Plasma, KDE Frameworks or Gear version bump moves the whole release set, so it rebuilds every port of that family |
| Only packaging | `make build BUILD_ARGS="--phases 70_image"` |

**Rules while building:**

- **Never edit a port's sources while its rebuild is running.** The recipe hash
  is taken at install time.
- **Never re-run an early phase on a tree already ahead of it** — its snapshot
  would be overwritten with a later tree under the earlier name. That is what
  `--continue-from` is for.
- **A build started inside a backgrounded call dies with it.**
- **Editing a library rebuilds every port of ours**, not only its consumers.
- **Clear `ports/.portup`, `.portup-tools`, `.kpkgbin` when
  switching between a container run and a host run** — a binary built against one
  C library cannot execute under the other, and the failure does not say so. A
  container run as root leaves them root-owned, so remove them **from a
  container** rather than reaching for sudo. **`ports/.srccache` is not on that
  list**: it is plain data — every fetched source, by hash — that either C
  library reads, and the port directories are hard links into it. Deleting it
  is safe but costs a full refetch.
- **Upstream sources are not in git.** A recipe names each by its `sha256 =`;
  `make fetch` resolves it from `ports/.srccache`, the `kunaldawn/kdos`
  archive (releases `sources-NNN`, located through the committed
  `ports/sources.idx`) or upstream. A recipe that names a new hash is not done
  until `ports/publish <port>` has put the file in the archive and its index
  line is committed with the recipe — the pre-push hook refuses the push
  otherwise. See
  [`writing-ports.md`](docs/kdos/05-developer/writing-ports.md).
- **A new opt-in packaging flag is two edits, not one.** The chroot is entered
  with a cleared environment, so a variable must be named in `script/chroot/exec.sh`
  as well as passed by the `Makefile`, or it reaches every host step and no
  chroot one.
- **A Plasma, KDE Frameworks or Gear port at a version other than its release
  set's fails preflight.** Bump the whole family in one change; Plasma's
  modules require each other at one version, and Gear's PIM modules pin each
  other exactly.

**Before believing a change:**

```sh
testing/preflight.sh                     # the wiring, about 2.5 minutes
testing/selftest.sh                      # libraries and consumers, about 2.5 minutes
CC="cc -fsanitize=address,undefined -g" testing/selftest.sh   # when you touch a parser
python3 testing/kfscheck.py              # a recipe, patch or kfs.md: sidecar drift, notes, Subject: headers, the recipe stamp, a full render
bash testing/docscheck.sh                # either book's pages; --book docs/kfs for one book
python3 testing/anchorcheck.py           # every #fragment in both books lands on a heading
python3 testing/booknav.py               # chapter footers and docs/kfs contents; --write after adding or moving a page
make check                               # every check above but the ASan selftest, plus phaseclosure
make plasmatest                          # KDOS Qt code: QtTest, qmllint, package installs; the devdeps container only
```

A new or changed `.patch` without a `Subject:` line fails preflight and the
pre-push hook. "New or changed" means on no remote yet; `--base REF` or
`KDOS_CHECK_BASE=REF` measures against a ref instead.

---

## The rig

```sh
testing/rig-image.sh          # builds kdos-qemu-py:latest; network, once
R="docker run --rm --device /dev/kvm -v $PWD:/kdos -w /kdos kdos-qemu-py:latest \
   python3 testing/vnc-shot.py --audio --size 1920x1080"
$R --wait <boot> --cmd 'plasma-systemmonitor' --sleep 4 --shot build/shots/x.png
$R --gl --wait <boot> --cmd 'kdos phosphor on' --sleep 4 --shot build/shots/x.png
```

`<boot>` is the seconds from power-on to a settled Plasma session, measured on the image you
boot.

**Do not rebuild the ISO to look at a changed program.** Packaging is most of an image build, to
carry a binary of a few hundred kilobytes:

```sh
testing/quick.sh kcm-kdos-boxes -- --cmd 'systemsettings kcm_kdos_boxes' --sleep 3 --shot build/shots/x.png
KDOS_QUICK_KEEP=1 KDOS_QUICK_NOBUILD=1 testing/quick.sh kcm-kdos-boxes -- ...
```

It builds without packaging, tars what the port owns, patches a booted ISO's RAM overlay and
restarts whatever loaded the old copy: `plasmashell` for a plasmoid, `kwin_wayland --replace` for
a KWin plugin, a D-Bus unload and load for an effect or a KWin script; a KCM, Discover or the
installer is simply started again. It cannot carry anything under `fs/`, a new port, an SDDM
theme or a daemon that owns a D-Bus name, and it is not evidence about the shipped image — see
[`testing.md`](docs/kdos/05-developer/testing.md).

Boot the **ISO** (`build/iso-build/kdos.iso`), not the installed disk images
under `build/`: both are leftover testbeds and boot software that is not in this
tree. The ISO is the pristine shipped system.

**Traps, each a rule with its consequence:**

- **The rig writes raw PPM despite the `.png` extension.** Convert before use.
- **plasma-welcome, with the desk picker, opens on first login.** Close it
  before any other shot.
- **A pattern kill matching a monitoring loop's own search term kills the
  monitor.**
- **An exact-name kill cannot match a name past 15 characters**, so a restart
  using one restarts nothing and the stale program keeps answering.
  `xdg-desktop-portal-kde` and `polkit-kde-authentication-agent-1` are two of
  them.
- **The harness kills the emulator without a shutdown**, so a root script must
  `sync` after writing to the disk or the filesystem leaves the rewritten files
  zero-length on the next boot.
- **A supervised daemon's output is not on the pipe that started it**: `service
  start x | cat` returns when the script does, and the daemon's lines are in
  `/run/kdos-svc.<name>.log` and syslog; a user service's are under
  `$XDG_RUNTIME_DIR/ksvc`.
- **A static screen produces no frame events**, so anything about dropped frames
  needs something animating first.
- **A plain virtual display puts KWin on llvmpipe**, where Phosphor mode and
  the CRT motion refuse to load; photograph them with `vnc-shot.py --gl`.
- **Driving the session means a *login* shell** (`su - kdos -c`). It sets the
  home directory, without which the container tooling resolves it to `/` and
  every box call fails, and `/etc/profile` points it at the session bus
  (`/run/user/<uid>/bus`), without which `qdbus6` and `plasma-apply-*` reach
  no session.
- **A port's autostart file is part of every session.** Plasma runs each entry
  in `/etc/xdg/autostart` at login, and the session bus starts any service a
  `.service` file names on its first call, so a port that installs either
  changes what every login runs; a recipe deletes the ones KDOS does not want.
- **AT-SPI goldens and `plasmatest.sh` run only in the development image**
  (`testing/devdeps-image.sh`), not on a bare host.

Everything else about the harnesses is in
[`testing.md`](docs/kdos/05-developer/testing.md).

---

## Working-state markers

```bash
find ports/core -name kpkgbuild | wc -l                # ports
ls build/fs/var/lib/kpkg/db/ | wc -l                   # installed packages
git status --short | wc -l                             # tracked changes
ls build/logs/41_system/*.log                          # which packages have logs
tail -40 build/logs/41_system/<N>_<pkg>.install.log    # debug a failure
bash testing/docscheck.sh                              # both books: links, prose, page contract
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
  pick its shelf from `ports/shelves`, write a recipe at
  `ports/core/<shelf>/<name>/` matching
  [`writing-ports.md`](docs/kdos/05-developer/writing-ports.md), and wire it into
  `depends` and the list of the phase its dependencies put it in
  (`packages.d/<shelf>.txt` in `40_lang`…`44_apps`, `packages.txt` elsewhere);
  `python3 testing/phaseclosure.py` says when the phase is wrong. A KDE module
  joins its family's release set at the set's version (KDE Frameworks on the
  `kf6` shelf, Plasma's modules at the Plasma version, Gear's at the Gear
  version), and a Gear application is shelved by what it is for, like any
  application. A program of our own goes at `src/<area>/<name>/` in one of the
  four areas that hold ports (`system`, `art`, `desktop`, `daemons`; `desktop`
  holds the Qt code of rule 5), listed in `packages.d/src-<area>.txt` of its
  phase, or in `50_desktop`'s `packages.txt` for `desktop` and `daemons`. Put
  the reasons for its flags and patches in a `kfs.md` beside the recipe, and
  run `make book-stamp` (`make book-data` once it is built) so the book's
  recipe stamp matches.
- **"why is X off"** → usually because dependencies weren't present when the port
  was added. Name the missing dependencies and offer to add them; don't pretend
  it was a deliberate choice.

**And whatever the request:** if it changes behaviour, it changes documentation
in the same breath. Hard rule 1.

Subagents in this harness can't run bash, so dispatch-and-review wastes time.
Just do the work inline.
