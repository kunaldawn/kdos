# KDOS Documentation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development`
> (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.
>
> **This plan contains no commit steps.** KDOS hard rule 5 — *"Do not auto-commit. The user
> commits manually, often squashing many edits into one logical commit."* — overrides the usual
> commit-per-task convention. Each task ends with a **Checkpoint**: a command whose output proves
> the task landed. Stop there and let the user commit.

**Goal:** Replace the single 1,193-line `README.md` and the 7,140-line `CLAUDE.md` knowledge dump
with a forty-page documentation book under `docs/kdos/`, a front-door `README.md`, and a
`CLAUDE.md` reduced to the rules, conventions and workflow an agent needs.

**Architecture:** Plain GitHub markdown, relative links, no build step. Six numbered part
directories under `docs/kdos/`, page files named for their subject. Knowledge lives in the book
in exactly one place; `CLAUDE.md` links into it. Content is migrated out of `CLAUDE.md` section
by section against the map in the spec, verified against the tree, and re-voiced with every
historical statement removed.

**Tech Stack:** Markdown. `testing/vnc-shot.py` inside the `kdos-qemu-py:latest` container for
screenshots. `build/docscheck.sh` (throwaway, gitignored) for link and prose verification.

**Spec:** `docs.design.md`

---

## Global Constraints

Every task's requirements implicitly include this section.

### Documentation rules

- **One copy of every fact.** Knowledge lives in the book. Where a subject has a user-facing and
  an internal half, they are different content on different pages, never the same content twice.
- **Current state only.** No page records what something used to be, what bug was fixed, or how a
  lesson was learned. A warning is written as the rule and its consequence. These exact strings
  must not appear in any page: `used to`, `no longer`, `now finally`, `was broken`, `cost a debug
  cycle`, `we decided`, `it turns out`, `previously`, `historically`, `in the past`.
- **Every change updates its documentation in the same change.** A change in behaviour updates
  every page, code comment and shipped configuration file that describes it, and the update
  REPLACES the old description rather than appending to it. This is a rule for the tree, not only
  for this task: it lands in `CLAUDE.md`'s hard rules.
- **Never invent a number.** A count, timing or measurement appears only where it was measured,
  and says what was measured. Where a value is unknown, say so.
- **Relative links only.** No absolute URLs into the repository, no site generator, no front
  matter.
- **Nothing is committed.** The user commits.
- **No source, recipe, script or test is modified.** The only writes outside `docs/`,
  `README.md` and `CLAUDE.md` are throwaway helpers under `build/`, which is gitignored.
- **KDOS has one desktop session.** No page mentions `kdos-be`, a second session, `KDOS_SESSION`,
  `kdos-bar` or `kdos-files`. `src/desktop/kdos-be` and `docs/KDOS-BE.md` are already removed;
  both were untracked and the directory held no files.
- **Two files carry the release line**: `README.md` names it, `06-reference/status.md` says what
  each subsystem's maturity rests on. No other page states a version.
- **There is no published ISO.** Every path to a running system starts with building one. No page
  implies a download.

### The page contract

Every page under `docs/kdos/` has, in this order:

1. An H1 title, sentence case, naming the subject.
2. One opening paragraph saying what the page covers and who needs it. No preamble before it.
3. The body, in H2 sections. H3 where a section genuinely subdivides.
4. A final `## See also` section: a bullet list of relative links to related pages, each with a
   short reason for following it.

### House style

Present tense. Second person when instructing, third person when describing. British spelling.
No changelog, no marketing, no exclamation marks. Tables for anything enumerable. Code blocks
show real commands, and real output only where that output was observed. Measurements carry
their conditions. Terms are used exactly as `docs/kdos/06-reference/glossary.md` defines them.

Warnings take this shape — the rule, then the consequence:

> Bind the layer shell at version 4. An older resource answers a request for on-demand keyboard
> interactivity with exclusive, which parks the seat's keyboard on the surface and stops every
> window receiving input.

### The writing cycle

Each writing task runs these five steps. They are not repeated per task; the task supplies the
files, the sources, the required sections, the facts to verify and the checkpoint.

1. **Read the sources.** The `CLAUDE.md` line ranges the task names, plus the tree paths it
   names. Read the tree, not only `CLAUDE.md` — `CLAUDE.md` is stale in places and the tree is
   the authority.
2. **Verify the facts.** Every count, path, flag, key and binary name the page will assert is
   checked against the tree with a shell command before it is written. A fact that cannot be
   verified is dropped, not softened.
3. **Write the page** to the contract above, migrating the source material into the book's voice
   and dropping every historical statement.
4. **Wire the links.** Add the `## See also` list. Add the page to `docs/kdos/README.md` if the
   scaffold task left it as a stub row.
5. **Run the checkpoint.** Fix anything it reports before moving on.

---

## File structure

```
README.md                                    front door (rewritten, task 46)
CLAUDE.md                                    rules and workflow only (rewritten, task 47)
docs.design.md                               the spec
docs.plan.md                                 this plan
docs/kdos/README.md                          table of contents
docs/kdos/01-philosophy/why-kdos.md
docs/kdos/01-philosophy/principles.md
docs/kdos/01-philosophy/decisions.md
docs/kdos/02-user-guide/getting-started.md
docs/kdos/02-user-guide/installation.md
docs/kdos/02-user-guide/desktop.md
docs/kdos/02-user-guide/applications.md
docs/kdos/02-user-guide/theming.md
docs/kdos/02-user-guide/administration.md
docs/kdos/03-architecture/overview.md
docs/kdos/03-architecture/boot-and-init.md
docs/kdos/03-architecture/session.md
docs/kdos/03-architecture/packaging.md
docs/kdos/03-architecture/packs-and-boxes.md
docs/kdos/03-architecture/security-model.md
docs/kdos/03-architecture/design-language.md
docs/kdos/04-programs/README.md
docs/kdos/04-programs/kdos-comp.md
docs/kdos/04-programs/kdos-shell.md
docs/kdos/04-programs/kdos-res.md
docs/kdos/04-programs/kdos-appbox.md
docs/kdos/04-programs/daemons.md
docs/kdos/04-programs/kinstall.md
docs/kdos/04-programs/kdos-command.md
docs/kdos/04-programs/kdos-bb.md
docs/kdos/05-developer/developing.md
docs/kdos/05-developer/build-system.md
docs/kdos/05-developer/writing-ports.md
docs/kdos/05-developer/build-troubleshooting.md
docs/kdos/05-developer/c-libraries.md
docs/kdos/05-developer/writing-desktop-software.md
docs/kdos/05-developer/testing.md
docs/kdos/06-reference/command-index.md
docs/kdos/06-reference/configuration.md
docs/kdos/06-reference/filesystem-and-ipc.md
docs/kdos/06-reference/repository-layout.md
docs/kdos/06-reference/known-gaps.md
docs/kdos/06-reference/roadmap.md
docs/kdos/06-reference/status.md
docs/kdos/06-reference/glossary.md
docs/screenshots/                            all images (11 exist, ~28 added in pass 7)
build/docscheck.sh                           throwaway verifier (gitignored)
```

---

## Source map — `CLAUDE.md` sections and their line ranges

Line ranges are as of the HEAD this plan was written against. Tasks cite section titles as well,
because a title survives an edit and a line number does not. If a range does not open on the
titled heading, find the title with `grep -n`.

| Lines | Section |
|---|---|
| 10-75 | What KDOS is |
| 76-144 | Hard rules — do not violate |
| 145-168 | The Three Rings |
| 169-215 | The application catalogue — offline alien apps |
| 216-273 | Where the big artefacts live — releases, not LFS |
| 274-318 | `fetch` and `bake` run in containers |
| 319-463 | `kdos-appbox genlaunchers` writes four things |
| 464-520 | Runtime plumbing |
| 521-601 | Packs — an application is one file |
| 602-699 | The whiteout convention is overlayfs's, not OCI's |
| 700-799 | `kdos-packd` — the sixth root daemon |
| 800-878 | The box a pack runs in |
| 879-968 | `kdos-box` — a box is a first-class object |
| 969-1100 | Boxes in the desktop |
| 1101-1228 | `kdos-appbox` is a C program |
| 1229-1246 | Session, boot and console; The session bus |
| 1247-1256 | `/tmp` must be mounted `mode=1777` |
| 1257-1303 | `-march`, measured — `kdos march` |
| 1304-1334 | The stick rebuilds the stick — `kdos rebuild` |
| 1335-1426 | The stick writes the stick — `kdos clone` |
| 1427-1472 | A/B root slots — `kdos-bootctl` |
| 1473-1502 | An encrypted root |
| 1503-1514 | initramfs must use util-linux `switch_root` |
| 1515-1552 | Firmware, and the hardware the machine cannot use without it |
| 1553-1581 | `dialout` and the udev rules |
| 1582-1623 | CPU microcode rides in front of the initramfs |
| 1624-1672 | The boot splash |
| 1673-1694 | Console font: `kdos-getty`, not `rcS` |
| 1695-1739 | The login banner |
| 1740-1855 | The KDOS look — the styling guide (and its nine numbered rules) |
| 1856-2044 | Theming — PHOSPHOR |
| 2045-2099 | kdos-icons |
| 2100-2143 | kdos-cursors |
| 2144-2187 | Codecs, colour and time on the host |
| 2188-2268 | The `kdos` command |
| 2269-2424 | The C libraries — `src/libs/` |
| 2425-2471 | libktui draws at three glyph tiers |
| 2472-2500 | libkcolor is the one palette |
| 2501-2529 | kdos-theme — the generators |
| 2530-2642 | kdos-bb — the AAlib demo, hard-forked |
| 2643-2861 | kinstall — the installer |
| 2862-3131 | The compositor is a hard fork of labwc |
| 3132-3208 | Lock, idle and power |
| 3209-3291 | The CRT pass |
| 3292-3353 | Stutter attribution — `kdos stutter` |
| 3354-3367 | Ending the session |
| 3368-3466 | Screen capture, the clipboard and the portal |
| 3467-3492 | Input methods — the compositor is the wire |
| 3493-3576 | The input method — fcitx5 |
| 3577-3640 | Which app is using your microphone |
| 3641-3732 | Per-app Energy Impact — `kdos-energyd` |
| 3733-3775 | Memory pressure — `kdos-oomd` |
| 3776-3957 | The resource monitor — `kdos-res` |
| 3958-4127 | The shell answers the mouse, everywhere |
| 4128-4644 | Tiles — a block of cells drawn as PIXELS |
| 4645-4820 | The notification centre — `kdos-notify` |
| 4821-5036 | One taskbar, a Start menu, and pictures in the grid |
| 5037-5076 | Pixel icons are an ENHANCEMENT layer |
| 5077-5132 | The device managers |
| 5133-5185 | Removable media — `kdos-mountd` |
| 5186-5199 | `/var/run` IS `/run` |
| 5200-5267 | Looking at the screen without a human — `testing/vnc-shot.py` |
| 5268-5322 | The tray — a StatusNotifierItem host |
| 5323-5396 | Repo layout |
| 5397-5566 | Build system; Phase snapshots; Build plans; `fs/` sync; PACKAGES swept |
| 5567-5619 | libkbuild — the deciding half of the orchestrator |
| 5620-5728 | kdosbuild — the orchestrator in C |
| 5729-5786 | `testing/preflight.sh` |
| 5787-5913 | `testing/selftest.sh` |
| 5914-6053 | kpkg is C |
| 6054-6125 | One recipe format, and how the tree got there |
| 6126-6207 | The binhost — a signed index, and three equality tests |
| 6208-6242 | Deltas — an update that ships the difference |
| 6243-6299 | Vulnerability tracking — `kdos cve` |
| 6300-6338 | Reproducible packages |
| 6339-6421 | kpkg and recipe conventions |
| 6422-6449 | Port recipes — the canonical `build.sh` shapes |
| 6450-6536 | kdos-portup — upstream version checker |
| 6537-6772 | Recurring build fixes |
| 6773-6858 | Init scripts |
| 6859-6870 | Networking |
| 6871-6882 | Users and login |
| 6883-6910 | Testing and the VM rig |
| 6911-6925 | Working-state markers |
| 6926-7125 | Outstanding gaps / TODO |
| 7126-7140 | When the user says… |

---
## Pass 1 — Scaffold and Part I (Philosophy)

### Task 1: Scaffold, verifier and table of contents

**Files:**
- Create: `docs/kdos/README.md`
- Create: `build/docscheck.sh` (throwaway, gitignored)
- Create directories: `docs/kdos/01-philosophy`, `02-user-guide`, `03-architecture`,
  `04-programs`, `05-developer`, `06-reference`

**Interfaces:**
- Produces: `build/docscheck.sh`, invoked by every later task's checkpoint as
  `bash build/docscheck.sh [path...]`. With no argument it checks every markdown file under
  `docs/kdos/` plus `README.md`. It reports dead relative links, forbidden historical phrases,
  and pages missing the contract's opening paragraph or `## See also`.
- Produces: `docs/kdos/README.md`, whose table every later task links its page into.

- [ ] **Step 1: Create the directory tree**

```bash
cd /home/kunaldawn/workspace/repos/kdos
mkdir -p docs/kdos/{01-philosophy,02-user-guide,03-architecture,04-programs,05-developer,06-reference}
```

- [ ] **Step 2: Write the verifier**

```bash
cat > build/docscheck.sh <<'SH'
#!/bin/bash
# Throwaway authoring check for the KDOS documentation book. Not shipped, not
# committed: /build/ is gitignored. Reports dead relative links, historical
# phrasing, and pages that break the page contract.
set -u
cd "$(git rev-parse --show-toplevel)" || exit 2

# Phrases that are almost always a record of the past rather than a statement
# of the present. This catches the common cases; it cannot catch a paragraph
# that narrates history in its own words, so read what you write.
BAD='used to be|used to have|it used to|which used to|no longer|now finally|was broken|cost a debug cycle|we decided|it turns out|previously,|historically,|in the past'

rc=0

if [ "$#" -gt 0 ]; then
    files=("$@")
else
    mapfile -t files < <(find docs/kdos -name '*.md' | sort; echo README.md)
fi

for f in "${files[@]}"; do
    if [ ! -f "$f" ]; then echo "MISSING   $f"; rc=1; continue; fi
    dir=$(dirname "$f")

    # Dead relative links. Skips URLs and pure anchors; strips any #fragment.
    dead=$(grep -oE '\]\([^)]+\)' "$f" | sed -E 's/^\]\(//; s/\)$//' | while read -r link; do
        case "$link" in http*|mailto:*|'#'*) continue ;; esac
        target=${link%%#*}
        [ -z "$target" ] && continue
        if [ ! -e "$dir/$target" ] && [ ! -e "$target" ]; then echo "  -> $link"; fi
    done)
    if [ -n "$dead" ]; then echo "DEADLINK  $f"; echo "$dead"; rc=1; fi

    hist=$(grep -niE "$BAD" "$f" | head -5)
    if [ -n "$hist" ]; then echo "HISTORY   $f"; echo "$hist" | sed 's/^/  /'; rc=1; fi

    case "$f" in
        docs/kdos/*)
            head -1 "$f" | grep -q '^# ' || { echo "NOTITLE   $f"; rc=1; }
            grep -q '^## See also' "$f" || { echo "NOSEEALSO $f"; rc=1; }
            ;;
    esac
done

# Every book page reachable from the table of contents.
if [ "$#" -eq 0 ]; then
    while read -r page; do
        rel=${page#docs/kdos/}
        [ "$rel" = "README.md" ] && continue
        grep -q "$rel" docs/kdos/README.md || { echo "UNLISTED  $page"; rc=1; }
    done < <(find docs/kdos -name '*.md' | sort)
fi

[ $rc -eq 0 ] && echo "docscheck: ok"
exit $rc
SH
chmod +x build/docscheck.sh
```

- [ ] **Step 3: Write the table of contents**

`docs/kdos/README.md` carries, in this order:

- H1 `KDOS documentation`.
- One opening paragraph: what the book is, who it is for, and the statement that it describes the
  system as it is, never as it was.
- `## Reading paths` — three short lists of links: *I want to use KDOS*
  (getting-started → installation → desktop → applications → theming → administration),
  *I want to build KDOS* (why-kdos → developing → build-system → writing-ports →
  build-troubleshooting → testing), *I want to change KDOS* (principles → overview →
  design-language → c-libraries → writing-desktop-software → the relevant program page).
- `## The book` — one H3 per part, each with a table of `| Page | What it covers |` where the
  page cell links the file. Take the forty rows and their one-line descriptions from the page
  tables in `docs.design.md`.
- `## Conventions` — that links are relative, that every page ends in **See also**, and where
  the vocabulary is defined (link the glossary).
- `## See also` — links to `../../README.md` and `../../CLAUDE.md`.

Every row links a file that does not exist yet; `docscheck.sh` will report those as `MISSING`
until the page is written. That is the intended signal of remaining work.

- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/README.md
find docs/kdos -type d | sort
grep -c '](' docs/kdos/README.md
```

Expected: seven directories; at least 40 links; and from `docscheck.sh` a `DEADLINK` block naming
every page not yet written, with **no** `HISTORY`, `NOTITLE` or `NOSEEALSO` line. The dead links
are the remaining work and clear as the pages land; the index is only clean at task 41.

---

### Task 2: `01-philosophy/why-kdos.md`

**Files:**
- Create: `docs/kdos/01-philosophy/why-kdos.md`

**Sources:**
- `CLAUDE.md` 10-75 (What KDOS is), 145-168 (The Three Rings) for the ring summary only.
- `README.md` lines 32-155 for the existing "Who this is for" and "What KDOS is" prose, which is
  good and is being relocated rather than rewritten from nothing.
- Tree: `ports/core` (count), `ports/appbox/packs.conf`, `README.md`'s exceptions table.

**Facts to verify before writing:**

```bash
ls ports/core | wc -l                                     # ports
ls build/fs/var/lib/kpkg/db 2>/dev/null | wc -l            # packages in the built tree
grep -c '^app ' ports/appbox/packs.conf                    # application packs
grep -c '^runtime ' ports/appbox/packs.conf                # runtimes
grep -c '^base ' ports/appbox/packs.conf                   # base packs
grep -h '^version' ports/core/linux/kpkgbuild              # kernel
for p in linux-firmware intel-ucode sof-firmware wireless-regdb rust go ttf-dejavu terminus-ttf; do
    [ -d "ports/core/$p" ] && echo "exception present: $p"
done
```

**Required sections:**
- `## The four properties` — built from source with named exceptions; KDOS builds KDOS; the repo
  builds offline; applications live in boxes. Each as a statement plus what it costs.
- `## What is not built from source` — the table of exceptions with the reason each is an
  exception, verified by the loop above.
- `## Who this is for` — and, explicitly, who it is not for.
- `## The trade` — what you get, what you take on.
- `## What is deliberately absent` — no systemd, no Xorg server, no GTK or Qt on the host, no
  first-boot wizard, no telemetry, no application store, no binary archive to fall back on.
- `## Scale` — the verified counts, each labelled with what it counts.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands above and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Add its row to `docs/kdos/README.md` if the scaffold left it unlinked**
- [ ] **Step 5: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/01-philosophy/why-kdos.md
```

Expected: `docscheck: ok`.

---

### Task 3: `01-philosophy/principles.md`

**Files:**
- Create: `docs/kdos/01-philosophy/principles.md`

**Sources:**
- `CLAUDE.md` 76-144 (Hard rules) — the *reasoning*; the rules themselves stay in `CLAUDE.md`.
- Principles stated throughout `CLAUDE.md` and to be gathered here: absent-never-partial
  (521-601, 1427-1472, 5567-5619), never invent a number (3641-3732, 3776-3957),
  measure rather than assume (1257-1303, 3209-3291), one implementation per idea
  (2188-2268, 3958-4127), honesty about what cannot be enforced (879-968).

**Required sections:** one H2 per principle, each stating the principle, why it holds here, and
what it costs:
- `## No systemd`
- `## No Xorg server, and one carve-out`
- `## No GTK and no Qt on the host`
- `## One implementation of an idea`
- `## A thing that does not parse whole is absent, never partial`
- `## Never invent a number`
- `## Measure rather than assume`
- `## Offline by construction`
- `## Reproducible by construction`
- `## Say what cannot be enforced`
- `## Documentation describes the present`
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Write the page to the contract**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/01-philosophy/principles.md
```

Expected: `docscheck: ok`.

---

### Task 4: `01-philosophy/decisions.md`

**Files:**
- Create: `docs/kdos/01-philosophy/decisions.md`

**Sources:** `CLAUDE.md` 169-215 (Debian over Alpine), 216-273 (release assets over LFS),
521-601 (one pack per application), 1257-1303 (measured `-march`), 2862-3131 (the labwc fork),
6054-6125 (the recipe format and the embedded-shell alternatives), 6243-6299 (Alpine secdb over
NVD and OSV), 2530-2642 (kdos-bb frozen fork).

**Required sections:** one H2 per decision, each as *the question*, *what was chosen*, *what was
rejected and why*:
- `## A hard fork of labwc, not a compositor of our own`
- `## One pack per application, not one image`
- `## Debian inside boxes, not Alpine`
- `## musl, and what it forecloses`
- `## No KDE on the host`
- `## No application store`
- `## Release assets, not Git LFS`
- `## -march measured per machine, not tiered`
- `## Alpine's security database, not NVD or OSV`
- `## The build shell lives beside the recipe, not inside it`
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Write the page to the contract**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/01-philosophy/decisions.md
bash build/docscheck.sh docs/kdos/01-philosophy/*.md
```

Expected: `docscheck: ok`. **Pass 1 review gate — stop for the user.**

---
## Pass 2 — Part II (User guide)

### Task 5: `02-user-guide/getting-started.md`

**Files:**
- Create: `docs/kdos/02-user-guide/getting-started.md`

**Sources:** `CLAUDE.md` 216-273 (`make bootstrap` and where artefacts live), 1624-1672 (boot
splash), 1673-1694 (console font), 1695-1739 (login banner), 6871-6882 (users and login).
Tree: `Makefile`, `fs/etc/inittab`, `fs/usr/local/bin/kdos-desktop`, `README.md` 951-1137.

**Facts to verify:**

```bash
grep -E '^[a-z][a-z-]*:' Makefile | cut -d: -f1 | sort   # every make target
grep -n 'autologin\|agetty\|tty[0-9]' fs/etc/inittab
sed -n '1,40p' fs/usr/local/bin/kdos-desktop
grep -n 'kdos:' fs/etc/passwd fs/etc/group
```

**Required sections:**
- `## There is no download` — one short paragraph: you build the ISO. State the cost honestly
  (hours, disk, a container runtime) before the reader starts.
- `## What you need` — host requirements: docker or podman, disk, network for `make bootstrap`
  only.
- `## Build an ISO` — `make bootstrap`, then `make build`, what each does, where the ISO lands,
  roughly how long. Point at [`05-developer/developing.md`](../05-developer/developing.md) for
  anything that goes wrong.
- `## Write the medium` — writing the image, and that `kdos clone` copies a written stick once
  you have one.
- `## Boot` — UEFI only, the splash and what its stages mean, and what a failure at each stage
  points at.
- `## First login` — the shipped account, the tty layout from `inittab`, the banner.
- `## Start the desktop` — `kdos-desktop` from tty1, why it is started by hand rather than by a
  display manager.
- `## Try these first` — five commands worth running immediately, each one line:
  `kdos help`, `kdos doctor`, `kdos status`, `kdos app list`, `kdos theme amber`.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/02-user-guide/getting-started.md
```

Expected: `docscheck: ok`.

---

### Task 6: `02-user-guide/installation.md`

**Files:**
- Create: `docs/kdos/02-user-guide/installation.md`

**Sources:** `CLAUDE.md` 2643-2861 (kinstall) — the *using* half; the design half is task 25.
1427-1472 (A/B slots), 1473-1502 (encrypted root).
Tree: `src/packages/kdos-installer/pages.c`, `conf.c`, `install.c`, `main.c`.

**Facts to verify:**

```bash
grep -n 'ki_pages\[\]\|page_index\|"[a-z]*"' src/packages/kdos-installer/pages.c | head -40
grep -n 'ki_filesystems\[\]' -A 30 src/packages/kdos-installer/conf.c
grep -oE '"--[a-z-]+"' src/packages/kdos-installer/main.c | sort -u
grep -n 'fs_passno\|mkswapfile\|fallocate' src/packages/kdos-installer/*.c | head
```

**Required sections:**
- `## Before you start` — nothing is written until the summary page; every page before it is
  reversible.
- `## Running the installer` — invocation, and giving it its own VT when something else owns the
  console.
- `## The pages` — one H3 per wizard page in the real order taken from `pages.c`, each saying
  what it asks and what the answer decides.
- `## Filesystems` — the table from `ki_filesystems[]`: name, `fs_passno`, how swap is made, and
  whether the initramfs carries the module.
- `## Encryption` — LUKS2, the two UUIDs on the kernel command line and which names what, and
  that the passphrase is refused at the questionnaire rather than at the install step.
- `## A/B root slots` — what the installer writes, what filling slot B needs, and that A/B and
  LUKS are not wired together.
- `## Applications` — the packs page, why base and runtimes are not a choice, what an install
  carries.
- `## Answer files and unattended installs` — `--save`, `--config`, `--unattended`, the exit
  codes, and that an unattended run always terminates.
- `## Inspecting without installing` — `--dry-run`, `--dump probe`, `--dump plan`, `--json`, and
  that no password appears in either dump.
- `## What the installer writes where`
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/02-user-guide/installation.md
```

Expected: `docscheck: ok`.

---

### Task 7: `02-user-guide/desktop.md`

**Files:**
- Create: `docs/kdos/02-user-guide/desktop.md`

**Sources:** `CLAUDE.md` 3132-3208 (lock, idle, power), 3958-4127 (the pointer contract as a
user sees it), 4645-4820 (notifications), 4821-5036 (taskbar and Start menu), 5077-5132 (device
managers), 5133-5185 (removable media).
Tree: `fs/etc/skel/.config/kdos-comp/rc.xml`, `menu.xml`,
`fs/etc/skel/.config/kdos/comp.conf`, `panel.conf`, `src/desktop/kdos-shell/main.c` (`TOOLS[]`).

**Facts to verify:**

```bash
grep -oE '<keybind key="[^"]+"' fs/etc/skel/.config/kdos-comp/rc.xml
grep -c '<default />' fs/etc/skel/.config/kdos-comp/rc.xml     # must be 2
sed -n '/TOOLS\[\]/,/};/p' src/desktop/kdos-shell/main.c | grep -oE '"kdos-[a-z]+"'
cat fs/etc/skel/.config/kdos/panel.conf
```

**Required sections:**
- `## The panel` — every element left to right, what each says and what clicking, middle-clicking
  and right-clicking each does.
- `## The Start menu` — the three states, pinning, search including what search reaches beyond
  applications.
- `## Windows` — focus, move, resize, maximise, fullscreen, minimise, the window menu, grouping,
  and the fact that a box's name appears in a label only when it disambiguates.
- `## Workspaces`
- `## Keybindings` — a full table generated from the verified `rc.xml`, not from memory.
- `## Notifications` — toasts, hovering to hold one, the centre, do not disturb.
- `## The clipboard`
- `## Screenshots`
- `## Lock, idle and power` — the timers, what activity ends and what it does not, inhibitors,
  and that all three timers default to zero in a virtual machine.
- `## Displays`
- `## Removable media and devices`
- `## Files` — the desktop, the chooser and the browser.
- `## When the desktop misbehaves` — the stutter chip, `kdos-res`, and what each answers.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/02-user-guide/desktop.md
grep -c '^| ' docs/kdos/02-user-guide/desktop.md
```

Expected: `docscheck: ok`; the keybinding table has one row per `<keybind>` found above.

---

### Task 8: `02-user-guide/applications.md`

**Files:**
- Create: `docs/kdos/02-user-guide/applications.md`

**Sources:** `CLAUDE.md` 169-215 (the catalogue), 464-520 (runtime plumbing as a user notices
it), 879-968 (`kdos-box` for a user), 1101-1228 (the launch path and its timings).
Tree: `ports/appbox/packs.conf`, `src/packages/kdos-appbox/main.c`, `app.c`.

**Facts to verify:**

```bash
grep -c '^app ' ports/appbox/packs.conf
grep -oE '^runtime [a-z-]+' ports/appbox/packs.conf
grep -oE '^app [a-z0-9.-]+' ports/appbox/packs.conf | awk '{print $2}' | head -30
grep -n 'recommended' ports/appbox/packs.conf | head -3
grep -oE '"(list|search|show|install|launch|remove|rollback|update|sources)"' src/packages/kdos-tools/*.c | sort -u
```

**Required sections:**
- `## What an alien app is` — one paragraph, and the boundary: KDOS builds the desktop,
  applications live in boxes.
- `## What is on the medium` — the verified counts and the shape of the catalogue, with the
  segment list rather than all 181 names.
- `## Finding and installing` — `kdos app search`, `show`, `install`, and the Start menu's
  ON THE MEDIUM rows, where the click that installs is the click that opens.
- `## Launching` — from the menu, from a prompt via the shim, and what a cold launch costs
  against a warm one.
- `## Opening files` — MIME resolution, `kdos-appbox open`, open-with, default handlers.
- `## Commands that live in boxes` — `cmd` rows, why some packs have no launcher.
- `## Updating and rolling back` — `kdos app update`, sources, `retain`, `kdos app rollback`.
- `## Removing`
- `## Boxes` — enough `kdos-box` for a user to make a scratch environment; depth is task 23.
- `## What does not work` — printing from a box, rootless-inert applications, corefonts for wine.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/02-user-guide/applications.md
```

Expected: `docscheck: ok`.

---

### Task 9: `02-user-guide/theming.md`

**Files:**
- Create: `docs/kdos/02-user-guide/theming.md`

**Sources:** `CLAUDE.md` 1856-2044 (Theming), 2045-2099 (kdos-icons), 2100-2143 (kdos-cursors),
2501-2529 (the generators), 3209-3291 (the CRT knobs as a user sets them).
Tree: `src/libs/libkcolor/kcolor.h` (`KCOL_SCHEMES`), `src/packages/kdos-theme/`,
`fs/etc/skel/.config/kdos/comp.conf`.

**Facts to verify:**

```bash
grep -n 'KCOL_SCHEMES' -A 12 src/libs/libkcolor/kcolor.h
ls src/packages/kdos-theme
grep -nE '^(crt|crt_scanlines|crt_curve|crt_fullscreen|wallpaper|chrome_font)' fs/etc/skel/.config/kdos/comp.conf
grep -rn 'theme_commit' src/packages/kdos-tools/kdos.c | head -3
```

**Required sections:**
- `## The four accents` — the names and what each is, with the screenshots.
- `## Switching` — `kdos theme <accent>`, `list`, `next`, `prev`.
- `## What changes when` — a table: artefact, who reads it, and whether it retints live, on the
  next prompt, or at next launch. This is the page's most useful thing and must be exact.
- `## The CRT pass` — `crt`, `crt_scanlines`, `crt_curve`, `crt_fullscreen`, what each does, that
  zero is an honest off, and that the pass declines on a software renderer.
- `## Wallpaper`
- `## Style files` — `kdos theme style`, the keys it may carry, and that dotted keys reach the
  compositor's theme override.
- `## Fonts` — the console font, the scalable face the decorations need, `chrome_font`.
- `## Theming inside a box` — why boxed applications are themed through `$HOME`, and the two Qt
  routes.
- `## How the theme is generated` — `kdos-theme` and its subcommands, the vendored artwork, and
  `kdos theme --audit`.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/02-user-guide/theming.md
```

Expected: `docscheck: ok`.

---

### Task 10: `02-user-guide/administration.md`

**Files:**
- Create: `docs/kdos/02-user-guide/administration.md`

**Sources:** `CLAUDE.md` 1515-1552 (firmware), 1553-1581 (dialout and udev), 2144-2187 (codecs,
colour, time), 3493-3576 (fcitx5, the using half), 6773-6858 (init scripts), 6859-6870
(networking), 6871-6882 (users and login), 1257-1303 / 1304-1334 / 1335-1426 (march, rebuild,
clone as administration).
Tree: `fs/etc/init.d/`, `fs/etc/nftables.conf`, `fs/etc/kdos/`, `fs/etc/udev/rules.d/`,
`fs/etc/group`.

**Facts to verify:**

```bash
ls fs/etc/init.d/
ls fs/etc/kdos/
ls fs/etc/udev/rules.d/
grep -n 'kdos' fs/etc/group
grep -cE '^\s*(tcp|udp|ip|icmp)' fs/etc/nftables.conf
```

**Required sections:**
- `## Services` — the `init.d` numeric convention, the full script list, `ksvc` and `service`,
  what supervision means, and which scripts are one-shots that must not be supervised.
- `## Networking` — NetworkManager, the tools, VPN, and what has no user interface.
- `## The firewall` — the shipped policy, what is open and why, and how to open a port.
- `## Storage` — filesystems, swap, zram and its two knobs, removable media.
- `## Users and groups` — the shipped account, `wheel`, and what renaming a user touches.
- `## Hardware` — firmware, microcode, the udev rules and the `dialout` group, and the fact that
  both halves are required. A table of device classes and what each unlocks.
- `## Media, colour and time` — what the host can decode, the ICC engine, the zoneinfo tree.
- `## Input methods` — enabling fcitx5, the engines, and that configuration is text files.
- `## Keeping it current` — `kdos update`, the binhost, `kdos cve`.
- `## Diagnosing` — `kdos doctor` section by section, `kdos status`, `kdos restarts`,
  `kdos why`.
- `## Copying and rebuilding the medium` — `kdos clone` and `kdos rebuild`, with their refusals.
- `## Tuning for this machine` — `kdos march`, and why a win inside the noise is not a win.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/02-user-guide/*.md
```

Expected: `docscheck: ok` for all six pages. **Pass 2 review gate — stop for the user.**

---
## Pass 3 — Part III (Architecture and core)

### Task 11: `03-architecture/overview.md`

**Files:**
- Create: `docs/kdos/03-architecture/overview.md`

**Sources:** `CLAUDE.md` 145-168 (The Three Rings), 1229-1246 (session bus), 5323-5396 (repo
layout, for the ring mapping only).
Tree: `src/desktop/`, `src/packages/`, `src/libs/`, `ports/core/`, `fs/etc/init.d/`.

**Facts to verify:**

```bash
ls src/desktop src/packages src/libs
ls fs/etc/init.d
grep -rn 'PORT_REPO' script/*.env.sh | head -3
```

**Required sections:**
- `## The three rings` — core, desktop, outer ring; what lives where and the rule that decides.
- `## Host and box` — the boundary drawn precisely: what crosses it (`$HOME`, `/run/user/1000`,
  `/tmp`, `/dev`) and what does not (`/usr/share/themes`, `/usr/share/icons`, the system bus).
- `## A running system` — an ASCII process map from PID 1 down: init, the `init.d` services, the
  root daemons, the getty, `kdos-desktop`, the compositor, its supervised chrome, and a box.
- `## The root daemons` — a table: daemon, socket, what it owns, who may talk to it.
- `## Where state lives` — a table: kind of state, path, owner, and what removes it.
- `## The two package systems` — ports and packages for the host, packs for applications, and why
  they are not one system.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/03-architecture/overview.md
```

Expected: `docscheck: ok`.

---

### Task 12: `03-architecture/boot-and-init.md`

**Files:**
- Create: `docs/kdos/03-architecture/boot-and-init.md`

**Sources:** `CLAUDE.md` 1247-1256 (`/tmp`), 1427-1472 (A/B), 1473-1502 (encrypted root),
1503-1514 (`switch_root`), 1582-1623 (microcode), 1624-1672 (splash), 1673-1694 (console font),
1695-1739 (banner), 5186-5199 (`/var/run`), 6773-6858 (init scripts), 1515-1552 (firmware
loading, the kernel side only — the administration half is task 10).
Tree: `script/06_packaging/01_initramfs.sh`, `fs/etc/init.d/rcS`, `fs/etc/inittab`,
`fs/etc/fstab`, `src/packages/kdos-splash/`.

**Facts to verify:**

```bash
ls fs/etc/init.d/
grep -n 'MODULES\|switch_root\|cryptdevice\|bootstate' script/06_packaging/01_initramfs.sh | head -20
cat fs/etc/fstab
grep -n 'kdos-getty\|autologin' fs/etc/inittab
```

**Required sections:**
- `## The path from firmware to a prompt` — an ordered list, each step naming the program that
  runs it, so a reader can find where a boot stopped.
- `## rEFInd and the kernel command line` — every parameter KDOS relies on and what reads it.
- `## The initramfs` — what it contains, the generated init, and why microcode rides in front of
  it uncompressed.
- `## The splash` — how it draws before fbcon takes over, the three constraints, and adding a
  stage.
- `## Unlocking an encrypted root` — the order, the tty the prompt uses, and why the passphrase
  travels on stdin.
- `## A/B slot selection` — where the state file lives, why counting happens in the initramfs,
  and what `mark-good` proves.
- `## switch_root` — the util-linux requirement and what a `chroot`-style switch breaks.
- `## rcS and the service scripts` — the numeric convention, what `rcS` does around each script,
  and the full ordered list with one line each.
- `## Mount points that must be right` — `/tmp` at `mode=1777` and why a remount is not enough,
  `/var/run` and `/var/lock` as symlinks.
- `## The console` — `kdos-getty`, why the font is loaded there and not in `rcS`, the palette,
  and the login banner.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/03-architecture/boot-and-init.md
```

Expected: `docscheck: ok`.

---

### Task 13: `03-architecture/session.md`

**Files:**
- Create: `docs/kdos/03-architecture/session.md`

**Sources:** `CLAUDE.md` 1229-1246 (session bus), 3368-3466 (capture, clipboard, portal),
3467-3492 (input-method wire), 3493-3576 (fcitx5), 464-520 (the environment a box receives).
Tree: `fs/usr/local/bin/kdos-desktop`, `fs/usr/local/bin/kdos-desktop-start`,
`src/desktop/kdos-comp/src/kdos-child.c` (`TEMPLATES[]`),
`fs/usr/share/xdg-desktop-portal/kdos-portals.conf`, `src/desktop/xdg-desktop-portal-kdos/`.

**Facts to verify:**

```bash
cat fs/usr/local/bin/kdos-desktop
cat fs/usr/local/bin/kdos-desktop-start 2>/dev/null
sed -n '/TEMPLATES\[\]/,/};/p' src/desktop/kdos-comp/src/kdos-child.c
cat fs/usr/share/xdg-desktop-portal/kdos-portals.conf
```

**Required sections:**
- `## Starting a session` — what `kdos-desktop` and `kdos-desktop-start` each do, in order, and
  why they are shell scripts.
- `## The session bus` — one daemon per user at a fixed address, why the address carries no guid,
  and why a pathname socket in `/tmp` cannot be used.
- `## Audio` — PipeWire on the host, and how a box reaches it.
- `## Portals` — the backend split, why `kdos-portals.conf` is not optional and why its name is
  fixed, and the startup ordering that makes ScreenCast work.
- `## Supervised chrome` — the templates, one set per output, the respawn ceiling, and why a
  child's ignored signal dispositions are reset.
- `## Screen capture and the clipboard` — the globals the compositor offers, both generations,
  and which of them a sandboxed client cannot bind.
- `## Input methods` — the three parties, which protocol carries which direction, and which half
  a sandboxed client may have.
- `## The environment a box receives` — the full variable list and what breaks without each.
- `## Ending a session`
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/03-architecture/session.md
```

Expected: `docscheck: ok`.

---

### Task 14: `03-architecture/packaging.md`

**Files:**
- Create: `docs/kdos/03-architecture/packaging.md`

**Sources:** `CLAUDE.md` 5914-6053 (kpkg is C), 6126-6207 (the binhost), 6208-6242 (deltas),
6243-6299 (`kdos cve`), 6300-6338 (reproducible packages), 6339-6421 (recipe conventions, the
architecture half — the how-to is task 30).
Tree: `src/libs/libkpkg/`, `src/packages/kdos-kpkg/`, `src/libs/libksig/`, `script/*.env.sh`.

**Facts to verify:**

```bash
ls src/libs/libkpkg src/packages/kdos-kpkg
grep -rn 'SOURCE_DATE_EPOCH\|KPKG_STRICT_RECIPE\|build-id' script/phase*.env.sh | head
grep -rn 'kp_recipe_hash\|kp_build_hash\|kp_installed_current' src/libs/libkpkg/*.h
grep -rn 'A:\|B:\|E:\|C:\|D:\|O:' src/libs/libkpkg/*.c | head -10
```

**Required sections:**
- `## The ports tree` — what a port is, the two-file recipe, and the multi-repository
  `PORT_REPO`.
- `## Packages` — the archive format, the database, the manifest, what an upgrade does about
  orphans, and file conflicts between packages versus adopted paths.
- `## The five names kpkg answers to`
- `## Deciding what to rebuild` — the recipe hash, its three states, and why a source-less port
  hashes its own directory and all of `src/libs`.
- `## Reproducible packages` — every flag and environment line that makes a package a function of
  its inputs, and what each one excludes.
- `## The binhost` — the index format key by key, the three equality tests, the exit codes, and
  what signing does and does not assert.
- `## Signing` — Ed25519, the keyring, the two separate rings, and the rules a signature scheme
  must keep. Link the security page for the trust argument.
- `## Deltas` — why the delta is taken over the uncompressed archives, and why it needs no
  signature of its own.
- `## Vulnerability tracking` — the vendored database, the comparison, and why a package the
  database does not carry is unknown rather than clean.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/03-architecture/packaging.md
```

Expected: `docscheck: ok`.

---

### Task 15: `03-architecture/packs-and-boxes.md`

**Files:**
- Create: `docs/kdos/03-architecture/packs-and-boxes.md`

**Sources:** `CLAUDE.md` 169-215 (the catalogue), 521-601 (the pack format), 602-699 (whiteouts
and the bake), 800-878 (the box a pack runs in), 274-318 (bake runs in a container).
Tree: `src/libs/libkpack/`, `src/packages/kdos-pack/`, `ports/appbox/packs.conf`,
`ports/appbox/bake`, `ports/appbox/harvest.py`, `script/06_packaging/01_packs.sh`.

**Facts to verify:**

```bash
ls src/libs/libkpack src/packages/kdos-pack
grep -n 'KPK_SIG_\|kpk_solve\|footer\|magic' src/libs/libkpack/*.h | head -20
grep -oE '^(base|runtime|app|data|cmd|env|needs|graft|boxgraft|image|snapshot|recommended|release) ' ports/appbox/packs.conf | sort | uniq -c
grep -n 'exclude-regex\|exclude-path\|force-uid\|-b 4096\|-U ' ports/appbox/bake script/06_packaging/01_packs.sh | head
```

**Required sections:**
- `## A pack is an EROFS image with parts appended` — the layout diagram, field by field, and the
  property that makes it work.
- `## Building one` — `build` against `assemble`, and why the split exists.
- `## Reproducibility` — the four flags, and why the version is deliberately not in the UUID.
- `## What makes a pack unchanged` — image hash and metadata, and why both are compared.
- `## Verification` — hash before signature, the four signature states, and where each origin is
  checked.
- `## The catalogue` — `packs.conf` row types with the verified counts, the recommended set, and
  what an install carries.
- `## Baking` — the container, overlay whiteouts against OCI whiteouts, the base row as a whole
  filesystem, and the exclusion rules that must be probed rather than trusted.
- `## Mounting` — `kdos-packd`'s role, reference counting, the two mount routes, and why a
  decompose does not unmount.
- `## Composition` — the overlay stack, one box per application, and what that costs and buys.
- `## The box` — the podman invocation and why the merged path is last, the user namespace, and
  `kdos-boxinit` as PID 1.
- `## Grafts and data packs` — the two graft namespaces, the manifest that makes ungraft exact,
  and why a data pack is mounted noexec.
- `## Launchers` — what `genlaunchers` produces and why each output is required. Depth is task 23.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/03-architecture/packs-and-boxes.md
```

Expected: `docscheck: ok`.

---

### Task 16: `03-architecture/security-model.md`

**Files:**
- Create: `docs/kdos/03-architecture/security-model.md`

**Sources:** `CLAUDE.md` 3132-3208 (setuid inventory, `kdos-checkpass`, `kdos-powerd`),
3368-3466 (the sandbox allowlist), 3641-3732 (why the energy socket is not an oracle),
6126-6207 (signing), 879-968 (what a profile cannot enforce), 5133-5185 (mountd's refusals).
Tree: `src/desktop/kdos-boxsock/`, `src/desktop/kdos-lock/`, `src/desktop/kdos-res/`,
`fs/etc/kdos/`.

**Facts to verify:**

```bash
grep -rn 'setuid\|4755\|chmod 4' script/ src/desktop/*/build.sh src/packages/*/build.sh 2>/dev/null | head -20
grep -rn 'SO_PEERCRED\|kb_user_in_group' src/desktop/*/*.c | head -20
grep -rn 'allow_for_sandbox' -A 25 src/desktop/kdos-comp/src/server.c | head -40
ls fs/etc/kdos/keys 2>/dev/null || echo "no keys dir shipped"
```

**Required sections:**
- `## What this model protects` — and, first, what it does not: this is a single-user
  workstation, not a hardened multi-tenant host.
- `## setuid binaries` — the complete inventory with what each is for, which two are ours, and
  why mode bits rather than file capabilities.
- `## Password checking` — `kdos-checkpass`: no arguments, stdin, constant time, three exit
  codes, and the consequence of losing its setuid bit.
- `## Root daemons` — `SO_PEERCRED` as the gate, why the socket mode is not the authorisation,
  and why no daemon protocol takes a path.
- `## Sandboxed clients` — the security context a box's clients carry, the global allowlist, what
  is denied and why, and the portal as the sanctioned route.
- `## Containers` — rootless podman, `newuidmap`/`newgidmap`, `keep-id`, and that ownership
  grants nothing because packs are mounted `nosuid`.
- `## Mount options` — what every pack and every removable device is mounted with, and the one
  knob that relaxes it.
- `## Signing and trust` — the two keyrings, what each attests, and why a pack key must not sit
  in the binhost ring.
- `## What is not protected` — stated plainly: no MAC, no verified boot, no per-application
  filesystem confinement beyond the box, unsigned registry content when a box names an image, and
  the asymmetry that an unsigned pack mounts where an unverifiable signed one does not.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/03-architecture/security-model.md
```

Expected: `docscheck: ok`.

---

### Task 17: `03-architecture/design-language.md`

**Files:**
- Create: `docs/kdos/03-architecture/design-language.md`

**Sources:** `CLAUDE.md` 1740-1855 (the styling guide and its nine rules), 2425-2471 (the three
glyph tiers), 2472-2500 (the palette), 3958-4127 (the pointer contract), 5037-5076 (pictures as
an enhancement layer), 1856-2044 (the two derived colours and their contrast).
Tree: `src/libs/libkcolor/kcolor.h`, `src/libs/libktui/`, `src/libs/libkchrome/`.

**Facts to verify:**

```bash
grep -n 'KT_ACCENT\|KT_WARN\|KT_TEXT\|KT_MID\|KT_DIM\|KT_SURFACE\|KT_BG' src/libs/libktui/*.h | head
grep -n 'KCOL_SCHEMES' -A 12 src/libs/libkcolor/kcolor.h
grep -oE 'kch_[a-z_]+\(' src/libs/libkchrome/*.h | sort -u
grep -n 'ktui_ramp_levels\|KTUI_CAP\|LINUXVT' src/libs/libktui/*.h | head
```

**Required sections:**
- `## Everything is a grid of character cells` — the premise and what follows from it.
- `## A window is a double-line box` — the frame, the title on the top edge, and where the body
  starts.
- `## The chrome primitives` — the `kch_*` list with what each draws, and the rule that there is
  never a second copy.
- `## Colour` — the slots, the palette table as the single source, and the two rules that have
  each shipped as a defect: a label is never the fill colour, and emphasis is a fill with swapped
  slots rather than a reverse attribute.
- `## Contrast` — the measured ratios and what they permit.
- `## The pointer contract` — the table of gestures and their meanings, applying to every surface.
- `## Hit maps` — recorded from what was drawn, coordinates handed down once.
- `## The three glyph tiers` — the table, the console font's 512 glyphs, and the rule for chrome
  that must read on tty1.
- `## Pictures` — sprites and tiles as an enhancement layer, and the requirement that every
  caller draws correctly without them.
- `## The compositor's own decoration` — why the server-side frame is part of the set.
- `## The checklist` — the numbered list a new surface is not finished without.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/03-architecture/*.md
```

Expected: `docscheck: ok` for all seven pages. **Pass 3 review gate — stop for the user.**

---
## Pass 4 — Part IV (Programs and applications)

### Task 18: `04-programs/README.md`

**Files:**
- Create: `docs/kdos/04-programs/README.md`

**Sources:** the tree. This page is an index and asserts nothing that is not verifiable.

**Facts to verify:**

```bash
ls src/desktop src/packages
sed -n '/TOOLS\[\]/,/};/p' src/desktop/kdos-shell/main.c | grep -oE '"kdos-[a-z]+"'
grep -rn 'basename' src/packages/kdos-kpkg/*.c | head -5
grep -rn 'ln -s\|install -' src/packages/kdos-tools/build.sh | head -20
```

**Required sections:**
- One opening paragraph explaining that many KDOS binaries answer to several names by dispatching
  on their own basename, so the count of programs is larger than the count of binaries.
- `## The compositor and the desktop` — table: name, what it is, page.
- `## Root daemons` — table.
- `## Packaging and boxes` — table.
- `## Build and development tools` — table, marked host-only where they never ship.
- `## Multi-name binaries` — a table of binary → every name it answers to, taken from the
  verification above, so a reader who finds `kdos-pick` on their PATH can find its page.
- `## See also`

- [ ] **Step 1: Run the verification commands and record the values**
- [ ] **Step 2: Write the page to the contract**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/04-programs/README.md
```

Expected: `docscheck: ok`.

---

### Task 19: `04-programs/kdos-comp.md`

**Files:**
- Create: `docs/kdos/04-programs/kdos-comp.md`

**Sources:** `CLAUDE.md` 2862-3131 (the fork), 3209-3291 (the CRT pass), 3292-3353 (the frames
socket half), 3354-3367 (shutdown), 3132-3208 (idle policy and lock), 3467-3492 (the
input-method wire), 969-1100 (box identity and the chip).
Tree: `src/desktop/kdos-comp/src/kdos-*.c`, `include/kdos.h`, `KDOS-FORK`,
`fs/etc/skel/.config/kdos/comp.conf`, `fs/etc/skel/.config/kdos-comp/rc.xml`, `menu.xml`.

**Facts to verify:**

```bash
ls src/desktop/kdos-comp/src/kdos-*.c
cat src/desktop/kdos-comp/KDOS-FORK 2>/dev/null | head -20
grep -c '/\* KDOS \*/' -r src/desktop/kdos-comp/src | head
cat fs/etc/skel/.config/kdos/comp.conf
grep -oE 'name="[A-Za-z]+"' fs/etc/skel/.config/kdos-comp/menu.xml | sort -u | head -20
```

**Required sections:**
- `## What it is` — labwc 0.20.0, frozen, rebranded; where the fork is recorded; how to find every
  KDOS touch point.
- `## Configuration` — `comp.conf` for the KDOS keys and `rc.xml` for everything labwc owns; the
  live and startup-only table; what an unrecognised key does.
- `## Bindings` — the `<default />` requirement stated as the rule and its consequence, and where
  overrides must go.
- `## Decorations` — the generated theme override, the frame as a KDOS box, the button glyphs,
  the hover plate's alpha, and the font requirement.
- `## The CRT pass` — the swapchain seam, why direct scanout is disabled, why the texture is not
  cached, the two fallbacks, and the magnifier bypass.
- `## The wallpaper` — a scene node rather than a client, and the three things it must get right.
- `## Idle, dim, lock and lid`
- `## The frames socket` — one object per late frame, why it must never block, and why there is
  no history.
- `## The command socket` — what `kdos hey` can ask and what each answer means.
- `## Window thumbnails`
- `## Box identity` — how a window's box is resolved, including the Xwayland fallback, and the
  chip's four rules.
- `## Supervised children` — one set per output, the reap path, and the respawn ceiling.
- `## Sandboxing` — the security context and the allowlist. Link the security page.
- `## Xwayland`
- `## Shutdown`
- `## Debugging` — `KDOS_COMP_DEBUG`, `KDOS_CRT_DUMP`, the log level.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/04-programs/kdos-comp.md
```

Expected: `docscheck: ok`.

---

### Task 20: `04-programs/kdos-shell.md`

**Files:**
- Create: `docs/kdos/04-programs/kdos-shell.md`

**Sources:** `CLAUDE.md` 3577-3640 (the privacy applet), 3958-4127 (the mouse), 4128-4644
(tiles), 4645-4820 (the notification centre), 4821-5036 (taskbar, Start menu, icons),
5077-5132 (device managers), 5268-5322 (the tray).
Tree: `src/desktop/kdos-shell/` — every `.c`, and `main.c`'s `TOOLS[]`.

**Facts to verify:**

```bash
ls src/desktop/kdos-shell/*.c
sed -n '/TOOLS\[\]/,/};/p' src/desktop/kdos-shell/main.c
cat fs/etc/skel/.config/kdos/panel.conf
grep -oE '"--[a-z-]+"' src/desktop/kdos-shell/*.c | sort -u
```

**Required sections:**
- `## One binary, many names` — the verified name table, each row linking to its H2 below, and
  the rule that a name in `TOOLS[]` with no `_main` is a link error.
- `## The panel` with H3s: layout and the four degradation passes; the Start button; quick launch
  and reordering; the window list and its three buttons; the status wing and its two rows; the
  meters strip; the tray; the overflow chevron; tooltips; `panel.conf` and reload.
- `## kdos-start` — the Start menu.
- `## kdos-launcher`
- `## kdos-menu` — the root, system and window menus.
- `## kdos-desk` — the desktop, its icons and its own context menu.
- `## kdos-pick` — the chooser and `--browse`.
- `## kdos-notifyd and kdos-notify` — the daemon, the ring, the socket verbs, do not disturb.
- `## kdos-osd` — the bezel and the slider, and why they share only the mixer.
- `## kdos-cal`
- `## kdos-settings` — the grid, the pages, and that it never writes a box profile itself.
- `## The device managers` — `kdos-net`, `kdos-bt`, `kdos-audio`, `kdos-devices`, including the
  agent bluez needs and the deferred reply that keeps the bus answering.
- `## kdos-clip`, `kdos-run`, `kdos-prompt`, `kdos-status`, `kdos-tip`, `kdos-teams`,
  `kdos-display`, `kdos-keys`, `kdos-doc`, `kdos-openwith`, `kdos-saver`, `kdos-slit`,
  `kdos-ascii` — one H3 each, short.
- `## Shared chrome` — the header band, groups, buttons, lists, and the list/wheel/scrollbar rule.
- `## Tiles` — the two-slot rule, geometry before claim, and that a tile is never required.
- `## Icons` — the atlas first, application icons untinted.
- `## Dumping a surface` — `--dump`, `--dump-cells`, `KDOS_DUMP_SIZE`, and the goldens.
- `## See also`

This is the longest page in the book. Keep each H3 to what a reader needs to understand or change
that surface; the shared rules live once under `## Shared chrome` and in
[`design-language.md`](../03-architecture/design-language.md), not repeated per tool.

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/04-programs/kdos-shell.md
sed -n '/TOOLS\[\]/,/};/p' src/desktop/kdos-shell/main.c | grep -oE '"kdos-[a-z]+"' | tr -d '"' |
  while read -r n; do grep -q "$n" docs/kdos/04-programs/kdos-shell.md || echo "UNDOCUMENTED $n"; done
```

Expected: `docscheck: ok`, and no `UNDOCUMENTED` line.

---

### Task 21: `04-programs/kdos-res.md`

**Files:**
- Create: `docs/kdos/04-programs/kdos-res.md`

**Sources:** `CLAUDE.md` 3776-3957.
Tree: `src/desktop/kdos-res/`, `testing/fixtures/res/`, `testing/goldens/res-*.txt`.

**Facts to verify:**

```bash
ls src/desktop/kdos-res/*.c
grep -n 'RES_PAGES\|enum res_page_id' -A 20 src/desktop/kdos-res/*.h | head -40
ls testing/goldens/res-*.txt
grep -rn 'resctl' src/desktop/kdos-res/*.c | head -5
```

**Required sections:**
- `## What it is for` — identity, and why that is cheap here and nowhere else.
- `## The pages` — one H3 per page id from `RES_PAGES`, each saying what it shows and where the
  numbers come from.
- `## The detail page` — why the verbs live only here, and the rings that start at open.
- `## Acting on a process` — `kdos-resctl`, its three verbs, its gate, and the confirm modal.
- `## Reading the numbers honestly` — unreadable against zero, counters that go backwards as
  gaps, engine time against utilisation, and why `starttime` may only be subtracted from uptime.
- `## One renderer, three faces` — tty, window, dump.
- `## Configuration` — `res.conf` and the page ids as the only spelling.
- `## Fixtures and goldens` — `--fixture`, `--page`, `--dump`, and why a fixture must not read
  the host.
- `## Known limits` — the two lists that do not scroll.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/04-programs/kdos-res.md
```

Expected: `docscheck: ok`.

---

### Task 22: `04-programs/kdos-appbox.md`

**Files:**
- Create: `docs/kdos/04-programs/kdos-appbox.md`

**Sources:** `CLAUDE.md` 319-463 (`genlaunchers`), 464-520 (runtime plumbing), 879-968
(`kdos-box`), 1101-1228 (the C program and its launch path).
Tree: `src/packages/kdos-appbox/*.c`, `src/packages/kdos-boxinit/`, `src/libs/libkxdg/`.

**Facts to verify:**

```bash
ls src/packages/kdos-appbox/*.c
grep -oE '"[a-z]+"' src/packages/kdos-appbox/box_cmd.c | sort -u | head -30
grep -n 'RESERVED\|RENAME\|SKIP_\|EXEC_EXTRA\|X11_FORCING\|COMMANDS' src/packages/kdos-appbox/launchers.c | head -20
grep -n 'kxdg_exec_split\|kxdg_exec_quote' src/libs/libkxdg/*.h
```

**Required sections:**
- `## Two names, one binary` — `kdos-appbox` and `kdos-box`, and what each is for.
- `## The launch path` — the ordered steps, each with what it guards against.
- `## Resolving a pack from an exec line`
- `## genlaunchers` — the four outputs and why dropping any one breaks something visible; the two
  trees and whose decision each is; the naming rules; the tables that control it.
- `## Exec lines` — quoting and field codes, the one implementation, and the round trip.
- `## The environment a box gets`
- `## Box profiles` — every key, whether podman enforces it or KDOS does, and which keys need a
  recreate.
- `## Box verbs` — one line each for the verified `kdos-box` verb list.
- `## Freeze and import` — what the artefact is, and that there is no alternative for a pack box.
- `## Snapshots and rollback` — and the cost, stated.
- `## Warmup and garbage collection`
- `## Storage drivers` — the two, the rule that decides, and why the choice must not flip.
- `## kdos-boxinit` — PID 1 inside a box, and why it is static.
- `## Tracing a launch` — the trace file and the measured timings.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/04-programs/kdos-appbox.md
```

Expected: `docscheck: ok`.

---

### Task 23: `04-programs/daemons.md`

**Files:**
- Create: `docs/kdos/04-programs/daemons.md`

**Sources:** `CLAUDE.md` 700-799 (packd), 3132-3208 (powerd), 3641-3732 (energyd), 3733-3775
(oomd), 5133-5185 (mountd), 3368-3466 (the portal backend), plus `kdos-boxsock` from
2862-3131's sandbox material.
Tree: `src/desktop/kdos-powerd/`, `kdos-energyd/`, `kdos-oomd/`, `kdos-mountd/`, `kdos-packd/`,
`kdos-boxsock/`, `xdg-desktop-portal-kdos/`, `fs/etc/init.d/5*`, `fs/etc/kdos/`.

**Facts to verify:**

```bash
for d in kdos-powerd kdos-energyd kdos-oomd kdos-mountd kdos-packd kdos-boxsock; do
    echo "== $d"; ls src/desktop/$d
    grep -rhoE '/run/[a-z.-]+\.sock' src/desktop/$d 2>/dev/null | sort -u
done
ls fs/etc/init.d/ | grep -E '5[0-9]_'
ls fs/etc/kdos/
```

**Required sections:**
- `## The shape they share` — foreground under `ksvc`, one socket in `/run`, `SO_PEERCRED` to
  root and `wheel`, one line per connection, a mode of 0666 with the credential as the real gate,
  a `--fixture` seam, and a skip check in the init script rather than a refusal under a respawn
  loop.
- One H2 per daemon: `## kdos-powerd`, `## kdos-energyd`, `## kdos-oomd`, `## kdos-mountd`,
  `## kdos-packd`, `## kdos-boxsock`, `## xdg-desktop-portal-kdos`. Each carries: what it owns,
  its socket and verbs, its authorisation, its refusals, its configuration file if it has one,
  its fixture seam, and its stated limits.
- `## Adding a root daemon` — the checklist a new one must satisfy to match the family.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/04-programs/daemons.md
for d in powerd energyd oomd mountd packd boxsock; do
    grep -q "kdos-$d" docs/kdos/04-programs/daemons.md || echo "MISSING kdos-$d"
done
```

Expected: `docscheck: ok`, and no `MISSING` line.

---

### Task 24: `04-programs/kinstall.md`

**Files:**
- Create: `docs/kdos/04-programs/kinstall.md`

**Sources:** `CLAUDE.md` 2643-2861 — the design half. The using half is task 6 and must not be
repeated here.
Tree: `src/packages/kdos-installer/{probe,pages,install,conf,dump,main}.c`,
`script/01_phase1/13_kinstall.sh`.

**Facts to verify:**

```bash
ls src/packages/kdos-installer/
grep -n 'libk' script/01_phase1/13_kinstall.sh
grep -oE "emit\('[A-Z]'" src/packages/kdos-installer/install.c | sort -u
grep -n 'ki_packs_enter\|install_plan\|page_index' src/packages/kdos-installer/*.c | head
```

**Required sections:**
- `## Why it links almost nothing` — the phase-1 constraint, the exact library list, and what
  adding a dependency would cost.
- `## The file split` — a table: file, what it owns.
- `## The probe` — what it reads and what it will not guess.
- `## The page model` — how a page fills the configuration and nothing else, and why Next on the
  summary is refused.
- `## The install child` — the fork, the line protocol letter by letter, why the work stays
  sequential and the parent stays a poll loop, and why nothing but the emitter reaches the pipe.
- `## Colour and input on a VT` — eight slots, why bold must never be emitted, the palette save
  and restore, and the evdev pointer.
- `## Chrome and hit ids` — the reserved range and what claiming real ids breaks.
- `## The filesystem table` — one row is read by the menu, the mkfs, the fstab line and the swap
  step, and every row must appear in the initramfs module list.
- `## The applications step` — why it reads the flat index rather than libkpack, why base and
  runtimes are not choices, and how a delta stanza is dropped.
- `## Answer files` — the schema, and the fallbacks that keep an unattended run from failing over
  a spelling.
- `## Dumping` — `--dump probe`, `--dump plan`, `--json`, and the guarantee that no password
  appears.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/04-programs/kinstall.md
```

Expected: `docscheck: ok`.

---

### Task 25: `04-programs/kdos-command.md`

**Files:**
- Create: `docs/kdos/04-programs/kdos-command.md`

**Sources:** `CLAUDE.md` 2188-2268 (the command), 1257-1303 (march), 1304-1334 (rebuild),
1335-1426 (clone), 3292-3353 (stutter), 6243-6299 (cve).
Tree: `src/packages/kdos-tools/kdos.c` and the other files in that port.

**Facts to verify:**

```bash
grep -oE 'strcmp\(cmd, "[a-z-]+"\)' src/packages/kdos-tools/kdos.c | sed 's/.*"\(.*\)".*/\1/' | sort -u
ls src/packages/kdos-tools/
grep -n 'usage\|Usage' src/packages/kdos-tools/kdos.c | head -5
```

**Required sections:**
- `## The front door` — that `kdos help` opens by naming the three lanes, and why one list of the
  whole system's verbs is the only place a reader learns they are three different questions.
- One H2 per subcommand, in the order the dispatch table lists them, each with: synopsis, what it
  does, what it measures where it measures anything, what it refuses to claim, and its exit
  codes. Cover every name the verification above prints.
- `## The other names in this port` — `ksvc`, `service`, `kdos-getty`, `kdos-banner`,
  `kdos-shot`, `kdos-fetch-app`, `kdos-fetch-static`, `kdos-theme`, verified against the tree.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/04-programs/kdos-command.md
grep -oE 'strcmp\(cmd, "[a-z-]+"\)' src/packages/kdos-tools/kdos.c | sed 's/.*"\(.*\)".*/\1/' | sort -u |
  while read -r c; do grep -q "^## .*\`$c\`\|^## $c" docs/kdos/04-programs/kdos-command.md || echo "UNDOCUMENTED $c"; done
```

Expected: `docscheck: ok`, and no `UNDOCUMENTED` line.

---

### Task 26: `04-programs/kdos-bb.md`

**Files:**
- Create: `docs/kdos/04-programs/kdos-bb.md`

**Sources:** `CLAUDE.md` 2530-2642.
Tree: `src/packages/kdos-bb/`, `src/packages/kdos-bb/KDOS-FORK`, `ports/core/libmikmod/`,
`ports/core/aalib/`, `fs/etc/init.d/01_udev.sh`, `50_alsa.sh`, `testing/qemu-audio.sh`.

**Facts to verify:**

```bash
ls src/packages/kdos-bb | head
cat src/packages/kdos-bb/KDOS-FORK 2>/dev/null | head -20
ls ports/core/libmikmod/*.patch 2>/dev/null
grep -n 'action=add\|alsactl' fs/etc/init.d/01_udev.sh fs/etc/init.d/50_alsa.sh
```

**Required sections:**
- `## What it is` — bb 1.3rc1, imported, rebranded, frozen; what the fork records; what was
  dropped from upstream's tree and why the build calls the compiler directly.
- `## Running it`
- `## The mixer runs on its own thread` — the rule and its consequence, and the locking rule that
  must go with it.
- `## aalib facts that outlive this program` — the driver fallback and the loader registration,
  both stated as rules for anything else built on aalib.
- `## Audio on a bare tty` — the two init-script requirements, and the ring size as a deliberate
  margin.
- `## Debugging` — `KDOS_BB_DEBUG`.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/04-programs/*.md
```

Expected: `docscheck: ok` for all nine pages. **Pass 4 review gate — stop for the user.**

---
## Pass 5 — Part V (Developer guide)

### Task 27: `05-developer/developing.md`

**Files:**
- Create: `docs/kdos/05-developer/developing.md`

**Sources:** `CLAUDE.md` 216-273 (bootstrap and release assets), 274-318 (fetch and bake
containers), 5397-5449 (the build in outline), 6883-6910 (the VM rig in outline).
Tree: `Makefile`, `Dockerfile`, `script/kdosbuild.sh`.

**Facts to verify:**

```bash
grep -E '^[a-z][a-z0-9_-]*:' Makefile | cut -d: -f1
grep -n 'BUILD_ARGS\|network none\|HOST_UID' Makefile script/kdosbuild.sh | head -20
head -20 Dockerfile
```

**Required sections:**
- `## What you need` — a container runtime and disk; nothing installed on the host.
- `## First build` — `make bootstrap` then `make build`, what each fetches or does not, and why
  the build runs with no network.
- `## Every make target` — a table from the verification above: target, what it does, roughly how
  long, what it needs.
- `## Where things land` — `build/`, the logs, the snapshots, and that `build/` is gitignored and
  root-owned in places by design.
- `## Iteration loops` — a table of *what you changed* → *the narrowest command that rebuilds it*,
  covering `fs/`, one port, a desktop program, a library, the ISO, and the initramfs.
- `## Running it` — `make run` and `make run-hw`, and which one has the CRT pass.
- `## When a build fails` — which log to read and in what order; link the troubleshooting page.
- `## Rules that apply while building` — do not edit a port's sources while its rebuild runs; do
  not re-run an early phase on a tree already ahead of it.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/05-developer/developing.md
grep -E '^[a-z][a-z0-9_-]*:' Makefile | cut -d: -f1 |
  while read -r t; do grep -q "\b$t\b" docs/kdos/05-developer/developing.md || echo "UNDOCUMENTED target $t"; done
```

Expected: `docscheck: ok`, and no undocumented target.

---

### Task 28: `05-developer/build-system.md`

**Files:**
- Create: `docs/kdos/05-developer/build-system.md`

**Sources:** `CLAUDE.md` 5397-5566 (build system, snapshots, plans, `fs/` sync, orphan sweep),
5567-5619 (libkbuild's share), 5620-5728 (kdosbuild), 274-318 (the fetch and bake containers).
Tree: `src/build/kdosbuild/`, `src/libs/libkbuild/`, `script/`, `script/*.env.sh`,
`script/util/chroot_exec.sh`.

**Facts to verify:**

```bash
ls script/
ls script/*/ | head -60
ls src/build/kdosbuild src/libs/libkbuild
grep -hoE 'KDOS_[A-Z_]+' script/*.env.sh | sort -u
grep -n 'KDOS_SNAPSHOT_PATHS\|KDOS_PHASE_TITLE' script/*.env.sh | head
```

**Required sections:**
- `## Phases` — the table of phase directories with their titles, and why `05_desktop` sorts
  before `05_phase5`.
- `## How a phase runs` — numbered scripts against a `packages.txt`, and what the orchestrator
  does with each.
- `## The phase environment` — the metadata block, that it is parsed and never sourced, the five
  honoured keys, and the `env -i` rule that any variable a chroot step reads must be named.
- `## The chroot` — what is bind-mounted where, and why anything a chroot command prints is
  parsed.
- `## Snapshots` — what is declared, what is refused, the layout on disk, and layered
  newest-wins restore.
- `## Build plans` — `--phases`, `--steps`, `--rebuild`, what narrowing suppresses, and why `-f`
  and `KDOS_REPLAY` are passed only for what the plan named.
- `## kdosbuild` — the file split, the screens and their keys, headless mode, `--json`,
  `--preview`, `--selftest`.
- `## libkbuild` — the deciding half, and the rules it keeps.
- `## Syncing `fs/`` — the manifest guard, the merge of the account files, and the two traps in
  writing the manifest.
- `## Sweeping orphaned packages`
- `## Skip-if-installed` — the recipe hash, its three states, and what it does not cover.
- `## Fetching sources and baking packs` — the two containers, why they re-exec themselves, and
  the ownership rules on the way out.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/05-developer/build-system.md
ls script/ | grep -E '^[0-9]' | while read -r p; do
    grep -q "$p" docs/kdos/05-developer/build-system.md || echo "MISSING phase $p"; done
```

Expected: `docscheck: ok`, and every phase directory named.

---

### Task 29: `05-developer/writing-ports.md`

**Files:**
- Create: `docs/kdos/05-developer/writing-ports.md`

**Sources:** `CLAUDE.md` 6339-6421 (recipe conventions), 6422-6449 (canonical shapes), 6054-6125
(the format's constraints), 6450-6536 (kdos-portup), 216-273 (publishing sources), 274-318
(`ports/fetch`).
Tree: `ports/core/zlib/`, `ports/core/fcitx5/`, `ports/core/rust/`, `ports/fetch`,
`ports/update`, `ports/Containerfile.fetch`, `src/tools/kdos-portup/`, `ports/sources`.

**Facts to verify:**

```bash
cat ports/core/zlib/kpkgbuild; echo ---; cat ports/core/zlib/build.sh
grep -rhoE '^[a-z]+ +=' ports/core/*/kpkgbuild | tr -d ' =' | sort | uniq -c | sort -rn
ls ports/core/*/postinstall.sh | wc -l
grep -rl 'vendoring' ports/core/*/kpkgbuild | wc -l
ls ports/ | head
```

**Required sections:**
- `## A port is two files` — the split and what it buys.
- `## kpkgbuild` — every key with its meaning and an example, taken from the verified key census:
  `name`, `version`, `release`, `source`, `sha256`, `description`, `homepage`, `depends`,
  `vendoring`, `vendordir`, `secdb`, `bench`, `bench_setup`, `group`, `pypackages`,
  `pyrequirements`, and any other key the census reveals. Mark any key the census shows that this
  list does not, rather than omitting it.
- `## Recipe helpers` — where they must be declared, and the exact expansion forms supported,
  with the note that command substitution is not among them.
- `## build.sh` — the variables it receives, the working directory, and that they are shell
  variables rather than exports.
- `## Canonical shapes` — meson, cmake, autotools, rust, go, python, make-only; each a complete
  block that can be copied.
- `## postinstall.sh` — when a hook is the only answer, with the shipped examples.
- `## Vendoring` — one H3 per language: what `ports/fetch` produces, where the bundle must be
  unpacked, and the offline build flags.
- `## Adding a port end to end` — a numbered walkthrough from choosing a version to a green
  preflight, naming the exact commands.
- `## Checking for new versions` — `ports/update`, `make updates`, the three outcomes, and why
  `unknown` is never folded into `current`.
- `## Publishing sources` — the sharded release assets, the append-only guarantee, and where a
  fetch may look depending on whether the hash is known.
- `## Rules a recipe must keep` — no rationale comments; no source edits with sed or awk; a
  library nobody links is a library the host does not have; `-D` flags must be options the port
  defines.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/05-developer/writing-ports.md
grep -rhoE '^[a-z]+ +=' ports/core/*/kpkgbuild | tr -d ' =' | sort -u |
  while read -r k; do grep -q "\`$k\`" docs/kdos/05-developer/writing-ports.md || echo "UNDOCUMENTED key $k"; done
```

Expected: `docscheck: ok`, and no undocumented recipe key.

---

### Task 30: `05-developer/build-troubleshooting.md`

**Files:**
- Create: `docs/kdos/05-developer/build-troubleshooting.md`

**Sources:** `CLAUDE.md` 6537-6772 in full. This page is the closest to a direct migration in the
book; the work is re-voicing each entry from a story into a symptom, a cause and a fix, and
dropping the narration of when each was discovered.

**Required sections:**
- One opening paragraph: how to use the page — find the symptom, not the cause.
- `## Symptom index` — a table of the literal error text or observable symptom → link to the H2
  that covers it. This is what makes the page usable during a failing build.
- One H2 per failure class, each with three labelled parts: **Symptom**, **Cause**, **Fix** (with
  the exact flag or code). Cover every entry in the source range, including at minimum: static
  musl with Rust and bindgen; GNU-sed extensions under toybox; coreutils applets; a build
  reaching PyPI under `--network none`; unknown meson options; `go build -o` into its own
  directory; CMake policy minimum; a Rust port pinned by this tree's rustc; a vendor bundle in
  the wrong directory; pip building metadata and resolving `cmake` from PyPI; a Python package's
  declared backend as part of its version pin; a backtick inside double quotes; a meson feature
  given a boolean; a subproject wrap, a `file(DOWNLOAD)` or a `FetchContent` git clone; a release
  archive with an empty submodule directory; `AC_PROG_CC` preferring clang; a misspelt cmake
  `-D`; incompatible pointer types under GCC 15; an upstream `-Werror`; `make VAR=` on the
  command line; "C compiler cannot create executables"; a CMakeLists with no `project()`; missing
  meson prefix and libdir; ICU split; the gstreamer PTP helper; meson option types; URL and
  version landmines.
- `## When the failure is not here` — what to read, in what order, and what to check before
  concluding it is a KDOS problem.
- `## See also`

- [ ] **Step 1: Read the source range in full**
- [ ] **Step 2: Write the page to the contract, one entry per failure class**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/05-developer/build-troubleshooting.md
grep -c '^\*\*Symptom' docs/kdos/05-developer/build-troubleshooting.md
```

Expected: `docscheck: ok`, and at least 24 symptom entries.

---

### Task 31: `05-developer/c-libraries.md`

**Files:**
- Create: `docs/kdos/05-developer/c-libraries.md`

**Sources:** `CLAUDE.md` 2269-2424 (the set), 2425-2471 (glyph tiers), 2472-2500 (the palette),
5567-5619 (libkbuild).
Tree: `src/libs/` — every directory and its public header, `src/libs/selftest.c`.

**Facts to verify:**

```bash
ls -d src/libs/libk*
for d in src/libs/libk*; do
    n=$(basename "$d"); f=$(ls "$d"/*.h 2>/dev/null | head -1)
    echo "== $n  header=$f  files=$(find "$d" -name '*.c' | wc -l)"
done
grep -rn 'libkicon\|libkchrome' CLAUDE.md | head -3   # expect libkicon absent: it is new to the book
```

**Required sections:**
- `## The constraint` — static archives linking nothing but musl, with the one declared
  exception, and what a new dependency would cost.
- `## The set` — a table: library, prefix, what it owns, what it may link. One row per directory
  found above; a directory with no files is not listed.
- `## Dependency direction` — the graph, and the rule that nothing points back up.
- One H2 per library, each with: what it owns, its public surface in outline, and the invariants
  it exists to keep, stated as rules with their consequences. `libkicon` and `libkchrome` have no
  prose in `CLAUDE.md`; read their headers and write them from the source.
- `## Three rules the extraction keeps` — prefixed symbols, private frame state, caller-owned
  chrome ids.
- `## A library does not own the exit path`
- `## Adding a library` — the checklist, including the selftest and the consumer compile.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/05-developer/c-libraries.md
for d in src/libs/libk*; do
    n=$(basename "$d"); [ "$(find "$d" -name '*.c' | wc -l)" -gt 0 ] || continue
    grep -q "$n" docs/kdos/05-developer/c-libraries.md || echo "MISSING $n"; done
```

Expected: `docscheck: ok`, and every non-empty library documented.

---

### Task 32: `05-developer/writing-desktop-software.md`

**Files:**
- Create: `docs/kdos/05-developer/writing-desktop-software.md`

**Sources:** `CLAUDE.md` 2269-2424 (libktui and libkwl rules), 4128-4644 (tiles), 5037-5076
(pixel icons), 3958-4127 (the mouse contract as an implementer meets it), 1740-1855 (the
checklist).
Tree: `src/libs/libktui/`, `libkwl/`, `libkchrome/`, `libkcell/`, `libkicon/`,
`src/desktop/kdos-shell/main.c`, `testing/fixtures/shell/dumpmain.c`, `testing/goldens/`.

**Facts to verify:**

```bash
grep -oE 'ktui_[a-z_]+\(' src/libs/libktui/*.h | sed 's/.*://' | sort -u | head -60
grep -oE 'kwl_[a-z_]+\(' src/libs/libkwl/*.h | sed 's/.*://' | sort -u
grep -oE 'KWL_ROLE_[A-Z_]+' src/libs/libkwl/*.h | sort -u
ls testing/goldens | head -20
```

**Required sections:**
- `## What a KDOS surface is` — a cell grid drawn by libktui onto one of three backends.
- `## The frame protocol` — the immediate-mode loop, consuming events, focus, the extent, and why
  the frame state is private.
- `## Choosing a role` — layer surface, toplevel, lock surface; what each costs and what each
  must ask for, including the decoration a toplevel must request and the layer-shell version.
- `## Input` — the event queue as a ring, key repeat as the client's job, wheel ticks against
  axis events, a motion that did not move, and the serial that must be retained.
- `## Drawing` — cells, the diff, damage, per-buffer shadows, scale.
- `## Chrome` — the `kch_*` primitives and the list, wheel and scrollbar rule.
- `## Pictures` — icons, sprites, the two-slot tile rule, and that none of it may be required.
- `## Colour` — slots only, and the two rules that have shipped as defects.
- `## Adding a name to kdos-shell` — the table, the `_main` symbol, the build symlink, and the
  preflight check on flags one tool passes another.
- `## Looking at a surface without a screen` — `--dump`, `--dump-cells`, `KDOS_DUMP_SIZE`,
  the two golden sizes, and how to regenerate a golden.
- `## The checklist` — the seven-point list, restated as a procedure with the command for each.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/05-developer/writing-desktop-software.md
```

Expected: `docscheck: ok`.

---

### Task 33: `05-developer/testing.md`

**Files:**
- Create: `docs/kdos/05-developer/testing.md`

**Sources:** `CLAUDE.md` 5729-5786 (preflight), 5787-5913 (selftest), 5200-5267 (vnc-shot),
6883-6910 (the rig).
Tree: `testing/` in full.

**Facts to verify:**

```bash
ls testing/
ls testing/fixtures testing/goldens | head -40
grep -oE '^\s*(check|echo "==)[^"]*' testing/preflight.sh | head -40
grep -oE "add_argument\(['\"]--[a-z-]+" testing/vnc-shot.py | sed "s/.*--/--/" | sort -u
```

**Required sections:**
- `## What each tool proves` — a table: tool, what it proves, what it cannot, roughly how long.
- `## preflight.sh` — every check it makes, one line each, and when to run it.
- `## selftest.sh` — what it compiles, what it asserts, the sanitizer invocation, the leak-check
  rule, and the cached-helper trap between a container run and a host run.
- `## Fixtures` — the movable-root seam, one H3 per fixture directory saying what it records and
  which claim it makes testable.
- `## Goldens` — the two sizes, cell goldens against text goldens, what each catches, and how to
  regenerate.
- `## The QEMU rig` — `vnc-shot.py`: every flag from the verification above, what each is for, and
  the three things it must get right.
- `## Rig recipes` — booting the installed disk, driving a session, capturing a screenshot,
  running a root script, and attaching a data disk; with the transport note that a data disk is
  virtio rather than USB unless the test needs a removable device.
- `## The other harnesses` — `packlane.sh`, `install-to-disk.sh`, `appsweep.sh`,
  `appreport.sh`, `bootcheck`, `test_runner.py`, `prepare_base.py`, `qemu-audio.sh`.
- `## Traps` — the measured harness traps, each as a rule and its consequence.
- `## See also`

- [ ] **Step 1: Read the sources**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the page to the contract**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/05-developer/*.md
```

Expected: `docscheck: ok` for all seven pages. **Pass 5 review gate — stop for the user.**

---
## Pass 6 — Part VI (Reference)

### Task 34: `06-reference/glossary.md`

Written first in this pass: every other reference page uses its vocabulary.

**Files:**
- Create: `docs/kdos/06-reference/glossary.md`

**Sources:** the vocabulary used across Parts I–V, plus `CLAUDE.md` throughout.

**Required sections:**
- `## Terms` — an alphabetical definition list. Each entry is one or two sentences and links the
  page that covers it. At minimum: accent, alien app, appbox, base pack, binhost, box, box
  profile, build-config hash, cell grid, chrome, compose, data pack, delta, fixture, golden,
  graft, glyph tier, host, medium, pack, pack lane, phase, port, recipe hash, ring, runtime,
  session, shim, slot, snapshot, sprite, tile, warmup, whiteout.
- `## Words this documentation avoids` — and what it says instead, so the vocabulary stays one
  vocabulary: no "app store", no "distro" in prose where "distribution" is meant, no "just".
- `## See also`

- [ ] **Step 1: Collect every term used in Parts I–V that a reader could misread**
- [ ] **Step 2: Write the page to the contract**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/06-reference/glossary.md
```

Expected: `docscheck: ok`.

---

### Task 35: `06-reference/command-index.md`

**Files:**
- Create: `docs/kdos/06-reference/command-index.md`

**Facts to verify:**

```bash
grep -oE 'strcmp\(cmd, "[a-z-]+"\)' src/packages/kdos-tools/kdos.c | sed 's/.*"\(.*\)".*/\1/' | sort -u
sed -n '/TOOLS\[\]/,/};/p' src/desktop/kdos-shell/main.c | grep -oE '"kdos-[a-z]+"' | tr -d '"'
grep -rhoE 'ln -sf? [^ ]+ .*bin/[a-z-]+' src/packages/*/build.sh src/desktop/*/build.sh 2>/dev/null | head -30
ls src/desktop src/packages
```

**Required sections:**
- One opening paragraph: this is an index, not a manual page; every row links the page that
  documents the command.
- `## Commands` — one alphabetical table: `| Command | What it does | Documented in |`. Every
  command the system installs, including each name a multi-name binary answers to, and every
  `kdos` subcommand as `kdos <sub>`.
- `## Host-only tools` — a second table for what runs on a build host and never ships:
  `kdosbuild`, `kdos-portup`, `ports/fetch`, `ports/update`, `ports/sources`,
  `ports/appbox/bake`, and the `testing/` harnesses.
- `## See also`

- [ ] **Step 1: Run the verification commands and record the values**
- [ ] **Step 2: Write the page to the contract**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/06-reference/command-index.md
```

Expected: `docscheck: ok`.

---

### Task 36: `06-reference/configuration.md`

**Files:**
- Create: `docs/kdos/06-reference/configuration.md`

**Facts to verify:**

```bash
find fs/etc/kdos fs/etc/skel/.config -type f | sort
cat fs/etc/skel/.config/kdos/comp.conf fs/etc/skel/.config/kdos/panel.conf
cat fs/etc/kdos/*.conf
grep -rhoE '^\s*(if|else if)?\s*!?strcmp\(key, "[a-z_]+"\)' src/desktop/kdos-comp/src/kdos-config.c | sed 's/.*"\(.*\)".*/\1/' | sort -u
grep -rhoE '"[a-z_]+"' src/desktop/kdos-shell/panel.c | sort -u | head -40
grep -rn 'mountd.conf\|packd.conf\|zram.conf\|res.conf\|pack-sources\|favorites\|session-restore\|a11y' src fs --include='*.c' --include='*.sh' -l | head
```

**Required sections:**
- One opening paragraph: every file a user or administrator may edit, what owns it, and when a
  change takes effect.
- `## Where configuration lives` — the three tiers: shipped defaults in `/etc`, per-user under
  `~/.config`, and state under `~/.local/state` and `/var/lib` that is not configuration.
- One H2 per file. Each carries a table `| Key | Type | Default | Applies |` and one line per key
  saying what it decides. Cover at minimum: `~/.config/kdos/comp.conf`,
  `~/.config/kdos/panel.conf`, `~/.config/kdos/favorites`, `~/.config/kdos/session-restore`,
  `~/.config/kdos/a11y`, `~/.config/kdos/boxes/<name>.conf`, `~/.config/kdos/res.conf`,
  `~/.config/kdos-comp/rc.xml`, `~/.config/kdos-comp/menu.xml`,
  `~/.config/kdos-comp/themerc-override`, `/etc/kdos/packd.conf`, `/etc/kdos/mountd.conf`,
  `/etc/kdos/zram.conf`, `/etc/kdos/pack-sources`, `/etc/kdos/keys/`, `/etc/nftables.conf`,
  `/etc/fstab`, `/etc/inittab`, and the generated theme artefacts.
- `## Files whose existence is the setting` — the three that ship absent, and what each enables.
- `## Generated files you should not edit` — what regenerates them.
- `## See also`

Any file found by the verification that this list does not name must be added rather than
dropped. Any key found in the sources that the shipped file does not carry must be listed with
its default from the code.

- [ ] **Step 1: Run the verification commands and record the values**
- [ ] **Step 2: Write the page to the contract**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/06-reference/configuration.md
find fs/etc/kdos fs/etc/skel/.config -type f -printf '%f\n' | sort -u |
  while read -r f; do grep -q "$f" docs/kdos/06-reference/configuration.md || echo "MISSING $f"; done
```

Expected: `docscheck: ok`, and every shipped configuration file named.

---

### Task 37: `06-reference/filesystem-and-ipc.md`

**Files:**
- Create: `docs/kdos/06-reference/filesystem-and-ipc.md`

**Facts to verify:**

```bash
grep -rhoE '/run/[a-zA-Z0-9._-]+\.sock' src/ fs/ 2>/dev/null | sort -u
grep -rhoE '\$XDG_RUNTIME_DIR/[a-zA-Z0-9._-]+' src/ fs/ 2>/dev/null | sort -u
grep -rhoE '/var/lib/kdos/[a-zA-Z0-9._/-]+' src/ fs/ script/ 2>/dev/null | sort -u
grep -rhoE '/usr/share/kdos/[a-zA-Z0-9._/-]+' src/ fs/ script/ 2>/dev/null | sort -u
grep -rhoE '/etc/kdos/[a-zA-Z0-9._/-]+' src/ fs/ script/ 2>/dev/null | sort -u
```

**Required sections:**
- `## The target filesystem` — a table of every KDOS-owned path from the verification: path, what
  it holds, who writes it, and whether it survives a reinstall.
- `## Sockets` — one H3 per socket: path, owner, permissions, authorisation, and a table of verbs
  with their arguments and replies. Cover every socket the verification finds, including the
  compositor's command and frames sockets and the notification socket.
- `## Files used as an interface` — the overflow file the panel publishes, the theme state file,
  the observed-app-id ledger, the trace file, and the pack manifest.
- `## Reply grammar` — the conventions shared by the daemon protocols: one line per connection,
  `ok`/`err`, and that a client never names a path.
- `## Environment variables` — a table of every `KDOS_*` variable, what reads it, and what it
  does. Include the debug variables.
- `## See also`

- [ ] **Step 1: Run the verification commands and record the values**
- [ ] **Step 2: Write the page to the contract**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/06-reference/filesystem-and-ipc.md
grep -rhoE '/run/[a-zA-Z0-9._-]+\.sock' src/ fs/ 2>/dev/null | sort -u |
  while read -r s; do grep -q "$s" docs/kdos/06-reference/filesystem-and-ipc.md || echo "MISSING $s"; done
```

Expected: `docscheck: ok`, and every socket documented.

---

### Task 38: `06-reference/repository-layout.md`

**Files:**
- Create: `docs/kdos/06-reference/repository-layout.md`

**Sources:** `CLAUDE.md` 5323-5396, corrected against the tree.

**Facts to verify:**

```bash
ls
ls src src/libs src/desktop src/packages src/build src/tools ports script testing fs
ls script/*/ -d
```

**Required sections:**
- One opening paragraph plus an annotated tree block, corrected against the verification: no
  directory that does not exist, and every directory that does.
- `## What lives where, and why` — the rule that decides which of `ports/core`, `src/packages`
  and `src/desktop` a thing belongs in.
- `## Directories that are generated` — `build/`, the fetch caches, and that they are gitignored.
- `## Files at the root` — one line each, including the plan and design documents.
- `## What must never exist` — `fs/etc/X11/`, and why.
- `## See also`

- [ ] **Step 1: Run the verification commands and record the values**
- [ ] **Step 2: Write the page to the contract**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/06-reference/repository-layout.md
```

Expected: `docscheck: ok`.

---

### Task 39: `06-reference/known-gaps.md`

**Files:**
- Create: `docs/kdos/06-reference/known-gaps.md`

**Sources:** `CLAUDE.md` 6926-7125 (Outstanding gaps), plus every limit stated as a gap across
Parts I–V. Entries in the source that are marked closed are **dropped**, not recorded as closed:
a closed gap is history.

**Required sections:**
- One opening paragraph: this page exists so a reader stops looking for something that is not
  there. Everything is present tense.
- `## Desktop` — drag and drop, touch, fractional scale, per-output font size, per-output window
  filtering, the dbusmenu tray gap, workspace occupancy derivation, surfaces without a dump.
- `## Applications and boxes` — per-box protocol grants, printing from a box, corefonts for wine,
  live-session box composition.
- `## Hardware and platform` — x86-64 only, no fractional-scale, what a virtual machine cannot
  answer.
- `## Security` — restated briefly with a link to the model page.
- `## Build and packaging` — anything the tree shows as unproven.
- `## Testing` — what has never fired for real.
- `## See also`

Each entry is one short paragraph: what is missing, what that means in practice, and — where one
exists — what to use instead.

- [ ] **Step 1: Read the source range and collect gaps stated elsewhere in the book**
- [ ] **Step 2: Write the page to the contract, dropping every closed entry**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/06-reference/known-gaps.md
grep -niE 'closed|~~' docs/kdos/06-reference/known-gaps.md
```

Expected: `docscheck: ok`, and no closed-gap markers.

---

### Task 40: `06-reference/roadmap.md`

**Files:**
- Create: `docs/kdos/06-reference/roadmap.md`

**Sources:** `mobile.plan.md`, `mobile.plan2.md`, and the tree's own evidence of work in
progress.

**Facts to verify:**

```bash
head -40 mobile.plan.md
ls -d script-mobile build-mobile ports/mobile 2>/dev/null || echo "no mobile trees: not started"
ls *.plan.md
```

**Required sections:**
- One opening paragraph that separates this page from the rest of the book: everything here is
  intent, nothing here ships, and a reader looking for what exists wants
  [`status.md`](status.md).
- `## aarch64 and mobile` — the target, the approach in outline, what it reuses, and a link to
  `mobile.plan.md` as the plan of record. State plainly that no mobile tree exists yet.
- `## Direction` — the themes the tree shows work heading toward, each with what would have to be
  true for it to land.
- `## Not planned` — things frequently asked for that are deliberately not on the list, with a
  one-line reason and a link to `../01-philosophy/decisions.md`.
- `## See also`

- [ ] **Step 1: Read the sources and run the verification commands**
- [ ] **Step 2: Write the page to the contract**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/06-reference/roadmap.md
```

Expected: `docscheck: ok`.

---

### Task 41: `06-reference/status.md`

**Files:**
- Create: `docs/kdos/06-reference/status.md`

**Facts to verify:**

```bash
git branch --show-current
ls ports/core | wc -l
ls build/fs/var/lib/kpkg/db 2>/dev/null | wc -l
grep -c '^app ' ports/appbox/packs.conf
ls testing/goldens | wc -l
ls testing/fixtures
```

**Required sections:**
- One opening paragraph: what the maturity words mean here — **stable** (used daily, exercised by
  the test harness, changes are additive), **in progress** (works, incomplete or unexercised in
  places), **experimental** (present, not relied upon). Each verdict must rest on something
  checkable, and the page says what.
- `## Release line` — the line this checkout is on, and what that implies for stability. This and
  the root `README.md` are the only two places a version appears.
- `## Subsystems` — one table: `| Subsystem | Status | What that rests on |`. One row per: the
  build system, the ports tree, kpkg, the binhost, packs, boxes, the compositor, the shell
  desktop, the installer, each root daemon, the C libraries, the theme system, the test harness,
  the rig. The third column names the evidence — a passing selftest, a golden, a rig pass, or
  daily use — never an opinion.
- `## Scale` — the verified counts, each labelled with what it counts and when it was counted.
- `## See also`

- [ ] **Step 1: Run the verification commands and record the values**
- [ ] **Step 2: Write the page to the contract**
- [ ] **Step 3: Checkpoint**

```bash
bash build/docscheck.sh docs/kdos/06-reference/*.md
```

Expected: `docscheck: ok` for all eight pages. **Pass 6 review gate — stop for the user.**

---
## Pass 7 — Screenshots

Three capture tasks. All shots come from a **live boot of `build/iso-build/kdos.iso`** — not from
the installed disks, both of which were converted into testbeds for the removed second desktop and
boot a desktop that must not appear here. The installer is captured from `kinstall --dry-run`, which exercises the real pages and
writes nothing — rather than from a real install, which would destroy the disk every other shot
comes from.

**The rig invocation**, used by all three tasks:

```bash
R="docker run --rm --device /dev/kvm -v $PWD:/kdos -w /kdos kdos-qemu-py:latest \
   python3 testing/vnc-shot.py --audio --size 1920x1080"
```

**The rig writes raw PPM despite the `.png` extension**, so every shot is converted before it is
placed. **And the welcome card is open on first login** — dismiss it before any shot that is not
of it.

**Rules for every capture task:**

- **Take an orientation shot before chaining clicks.** Coordinates are read off a real frame,
  never guessed. A batch that guesses produces a run of pictures of the wrong thing and costs
  another boot.
- One boot per batch. A boot is minutes; a shot is seconds.
- `--wait 14` after boot before the first shot, so the panel has settled.
- Write to `build/shots/` first, inspect, and copy only the good ones into `docs/screenshots/`.
- A shot that cannot be captured is dropped and its page written without it. Nothing is staged,
  composited or edited to show behaviour the system does not have.
- The rig runs on the pixman renderer under `-vnc`, so **the CRT pass is not in these pictures**.
  Where a page shows the phosphor shader it uses an existing `docs/screenshots/` image captured
  with virgl, and says which.

### Task 42: Desktop, menus and windows

**Files:**
- Create: `build/shots-a.sh` (throwaway), and the resulting images in `docs/screenshots/`

**Target shots:** the panel with several windows open; the Start menu at rest; the Start menu
with All Programs open; the Start menu searching, showing an `ON THE MEDIUM` row; the launcher;
the root menu; the desktop context menu; a window menu from a task button; two windows snapped;
a window with the frame and its buttons filling the shot.

- [ ] **Step 1: Boot once and take an orientation shot**

```bash
mkdir -p build/shots
$R --wait 14 --shot build/shots/orient.png > build/shots-a-orient.log 2>&1; echo "rc=$?"
```

- [ ] **Step 2: Read the panel geometry off `build/shots/orient.png`** and record the pixel
  coordinates of the Start button, a task button, the clock and the notification wing.
- [ ] **Step 3: Write `build/shots-a.sh`** chaining the shots with the recorded coordinates,
  using `--mouse` to hover, `--click` to press, `--keys` to type and `--sleep` between steps.
  Open two applications with `--root-script` before the panel shots so the taskbar is not empty.
- [ ] **Step 4: Run it and inspect every frame**

```bash
bash build/shots-a.sh; ls -la build/shots/
```

- [ ] **Step 5: Copy the good frames into `docs/screenshots/`** with descriptive names
  (`panel.png`, `start-menu.png`, `start-programs.png`, `start-search-medium.png`,
  `root-menu.png`, `desktop-menu.png`, `window-menu.png`, `window-frame.png`).
- [ ] **Step 6: Checkpoint**

```bash
ls docs/screenshots/*.png | wc -l
```

Expected: more images than the eleven that existed, and every new file non-empty.

---

### Task 43: Applets, popups and the resource monitor

**Files:**
- Create: `build/shots-b.sh` (throwaway), and the resulting images in `docs/screenshots/`

**Target shots:** the calendar popup; the volume slider; `kdos-net`; `kdos-bt`; `kdos-devices`;
`kdos-clip`; the status overflow popup; a toast; the notification centre; `kdos-teams`;
`kdos-display`; `kdos-keys`; `kdos-settings` at its grid and at its Boxes page; `kdos-res` on
Applications, on Boxes and on a detail page.

- [ ] **Step 1: Boot once and take an orientation shot** as in task 42.
- [ ] **Step 2: Write `build/shots-b.sh`.** Prefer starting each front end by name through
  `--root-script` running it as the desktop user, rather than clicking through the panel: a
  named launch is reproducible and a click depends on coordinates. Use the panel click only for
  the shots that must show a popup anchored to its applet.
- [ ] **Step 3: Run it and inspect every frame**

```bash
bash build/shots-b.sh; ls -la build/shots/
```

- [ ] **Step 4: Copy the good frames into `docs/screenshots/`**
- [ ] **Step 5: Checkpoint**

```bash
ls docs/screenshots/*.png | wc -l
```

Expected: the applet and monitor shots present and non-empty.

---

### Task 44: Console, installer and themes

**Files:**
- Create: `build/shots-c.sh` (throwaway), and the resulting images in `docs/screenshots/`

**Target shots:** the tty login banner; `kdos doctor` output on a tty; `kdos stutter` reporting a
real stall; `kdos-energy`; `kdos app list`; `kdos-res` on tty1 at the console font; the installer
at three of its pages; the two accents not yet photographed.

- [ ] **Step 1: Console shots.** Use `--console-cmd` rather than starting a session, so the
  picture is the 512-glyph console font at the vt glyph tier — which is what those pages describe.

```bash
$R --no-session --console-cmd 'kdos doctor' --sleep 3 --shot build/shots/doctor-tty.png \
   > build/shots-c1.log 2>&1; echo "rc=$?"
```

- [ ] **Step 2: The installer.** Run `kinstall --dry-run` on a spare tty and photograph its
  pages, stepping with `--keys`. It writes nothing.
- [ ] **Step 3: A real stutter.** `kdos stutter` reports nothing on a static screen, because a
  static screen produces no late frames. Start something animating — `foot -e kdos-bb` — before
  stalling the compositor, then capture the report.
- [ ] **Step 4: The accents.** For each accent not already photographed, run `kdos theme <accent>`
  and shoot the desktop; the retint is live, so one boot covers all of them.
- [ ] **Step 5: Copy the good frames into `docs/screenshots/`**
- [ ] **Step 6: Checkpoint**

```bash
ls docs/screenshots/
bash build/docscheck.sh
```

Expected: every image referenced by a page exists — `docscheck.sh` reports no `DEADLINK` for an
image path. **Pass 7 review gate — stop for the user.**

---

## Pass 8 — The front door

### Task 45: Rewrite `README.md`

**Files:**
- Modify: `README.md` (replace in full)

**Sources:** the existing `README.md` for its voice and its best passages, which are relocated
rather than rewritten from nothing; the book, which is now the authority for every fact.

**Facts to verify:** re-run every count. The current file's numbers are stale and must not be
carried over.

```bash
ls ports/core | wc -l
ls build/fs/var/lib/kpkg/db 2>/dev/null | wc -l
grep -c '^app ' ports/appbox/packs.conf
grep -h '^version' ports/core/linux/kpkgbuild
git branch --show-current
ls docs/screenshots/
```

**Required structure:**
- The centred logo, title, tagline and badge line, as now, with the counts corrected.
- The hero screenshot with its caption.
- `## What KDOS is` — the four properties in four short paragraphs, each linking the book.
- `## Who this is for` — kept from the current file, trimmed.
- `## Quick start` — three fenced blocks: build an ISO, run it in QEMU, install it to disk. Each
  block is three or four commands, followed by one line pointing at the page that covers it. It
  opens by saying there is no published ISO.
- `## What makes it different` — five or six items, each two sentences and one picture, linking
  the page: the character-grid desktop, the CRT pass, one pack per application, per-application
  identity in the monitor and the energy report, reproducible offline builds, the medium as the
  software library.
- `## Documentation` — the **Start here** table: the six parts, one line each, linking
  `docs/kdos/README.md` and the part indexes.
- `## Repository layout` — the top level only, linking
  `docs/kdos/06-reference/repository-layout.md`.
- `## Status` — one line naming the release line, linking `docs/kdos/06-reference/status.md`.
- `## License` — kept from the current file.

Target 250–350 lines. Anything that is a manual belongs in the book, not here.

- [ ] **Step 1: Read the current `README.md` in full and mark what is worth keeping**
- [ ] **Step 2: Run the verification commands and record the values**
- [ ] **Step 3: Write the new `README.md`**
- [ ] **Step 4: Checkpoint**

```bash
bash build/docscheck.sh README.md
wc -l README.md
grep -c '](docs/kdos/' README.md
```

Expected: `docscheck: ok`; between 250 and 400 lines; at least eight links into the book.
**Pass 8 review gate — stop for the user.**

---

## Pass 9 — `CLAUDE.md` and final verification

### Task 46: Rewrite `CLAUDE.md`

**Files:**
- Modify: `CLAUDE.md` (replace in full)

**Sources:** the current `CLAUDE.md` sections marked **stays** in the migration map — 76-144
(Hard rules), 6339-6421 (recipe conventions, in brief), 6883-6910 (the rig, in brief),
6911-6925 (working-state markers), 7126-7140 (When the user says…) — plus the build and rebuild
incantations from 5397-5728 and the harness traps that are about *working*, not about the system.

**Required structure:**

1. `# KDOS — agent briefing` and one paragraph: what this file is, what it is not, and that
   every fact about how the system works lives in `docs/kdos/`.
2. `## Read this before touching anything` — the documentation map: a table of `| Working on… |
   Read… |` with one row per area, each linking a book page. This replaces the seven thousand
   lines that used to be loaded on every session.
3. `## Hard rules — do not violate` — the numbered list, carried over, with **two** new rules
   added and stated as prominently as the rest:

   - *No document, comment or commit message records history. Every file describes the current
     state.*
   - *Every change updates its documentation in the same change. A change in behaviour updates
     every page under `docs/kdos/`, every code comment and every shipped configuration file that
     describes that behaviour, before the change is done — and the update **replaces** the old
     description rather than appending to it or recording what it was. A comment that contradicts
     its code is a claim the next reader will act on.*

   Drop any rule whose content is knowledge rather than instruction and link the page instead.
4. `## Conventions` — recipe format in brief with a link; comment style with the right and wrong
   example; code style; the "never do X while editing" list.
5. `## Build and iteration` — the commands, one line each: full build, phase narrowing, port
   rebuild, `fs/` resync, ISO only, selftest, preflight. Plus the rules that apply while a build
   runs.
6. `## The rig` — the invocation and the traps that cost a run, linking
   `docs/kdos/05-developer/testing.md` for everything else.
7. `## Working-state markers` — carried over.
8. `## When the user says…` — carried over.

Target 350–450 lines. Every removed section must be findable through the map in section 2.

- [ ] **Step 1: List every section of the current `CLAUDE.md` and mark each `stays` or `moved`**

```bash
awk '/^#{2,3} /{if(p)print p" :: "pl"-"NR-1; p=$0; pl=NR} END{print p" :: "pl"-"NR}' CLAUDE.md | sed 's/^#\+ //'
```

- [ ] **Step 2: For every `moved` section, confirm its content reached the book**

```bash
# Take each distinctive term from a moved section and require it somewhere in docs/kdos.
for t in packd boxsock energyd oomd mountd powerd genlaunchers kpkgbuild binhost kdelta \
         switch_root microcode initramfs ksvc fixture golden snapshot "build plan" \
         "recipe hash" "security context" SO_PEERCRED setuid graft whiteout erofs \
         "glyph tier" "hit map" wcwidth portup secdb reproducible; do
    grep -rqi -- "$t" docs/kdos || echo "NOT MIGRATED: $t"
done
```

Expected: no `NOT MIGRATED` line. Anything reported is a section whose content did not survive
the migration and must be written into its destination page before continuing.

- [ ] **Step 3: Write the new `CLAUDE.md`**
- [ ] **Step 4: Checkpoint**

```bash
wc -l CLAUDE.md
bash build/docscheck.sh
grep -niE 'used to|no longer|now finally|was broken|cost a debug cycle' CLAUDE.md
```

Expected: `CLAUDE.md` between 300 and 500 lines; `docscheck: ok`; no historical phrasing.

---

### Task 47: Final verification

**Files:** none created. This task only checks and fixes.

- [ ] **Step 1: Every link resolves and every page keeps the contract**

```bash
bash build/docscheck.sh
```

Expected: `docscheck: ok`, with no `MISSING`, `DEADLINK`, `HISTORY`, `NOTITLE`, `NOSEEALSO` or
`UNLISTED` line.

- [ ] **Step 2: Every count in the book matches the tree**

```bash
echo "ports:    $(ls ports/core | wc -l)"
echo "packages: $(ls build/fs/var/lib/kpkg/db 2>/dev/null | wc -l)"
echo "apppacks: $(grep -c '^app ' ports/appbox/packs.conf)"
echo "runtimes: $(grep -c '^runtime ' ports/appbox/packs.conf)"
echo "bases:    $(grep -c '^base ' ports/appbox/packs.conf)"
echo "kernel:   $(grep -h '^version' ports/core/linux/kpkgbuild)"
echo "shellnames: $(sed -n '/TOOLS\[\]/,/};/p' src/desktop/kdos-shell/main.c | grep -c '"kdos-')"
echo "kdossubs: $(grep -oE 'strcmp\(cmd, "[a-z-]+"\)' src/packages/kdos-tools/kdos.c | sort -u | wc -l)"
echo "libs:     $(for d in src/libs/libk*; do [ "$(find "$d" -name '*.c'|wc -l)" -gt 0 ] && echo x; done | wc -l)"
grep -rhoE '\b[0-9]{2,4}\b' docs/kdos/*/*.md README.md | sort -u | head -40
```

Cross-check every number the book asserts against these. Correct the page, never the command.

- [ ] **Step 3: Every path, binary and port the book names exists**

```bash
grep -rhoE '`[a-z][a-zA-Z0-9._/-]*\.(c|h|sh|py|conf|xml|md|txt)`' docs/kdos README.md |
  tr -d '`' | sort -u | while read -r p; do
    [ -e "$p" ] || find . -name "$(basename "$p")" -not -path './build/*' -print -quit | grep -q . ||
      echo "NOT FOUND: $p"; done
grep -rhoE '`ports/core/[a-z0-9+.-]+`' docs/kdos README.md | tr -d '`' | sort -u |
  while read -r p; do [ -d "$p" ] || echo "NO PORT: $p"; done
```

Expected: no `NOT FOUND` and no `NO PORT`.

- [ ] **Step 4: Nothing mentions the removed session**

```bash
grep -rniE 'kdos-be|kdos-bar|kdos-files|KDOS_SESSION|second desktop|two desktops' docs/kdos README.md CLAUDE.md
```

Expected: no output.

- [ ] **Step 5: No knowledge is duplicated between `CLAUDE.md` and the book**

Read `CLAUDE.md` end to end. Any paragraph explaining how a subsystem works, rather than
instructing how to work on it, belongs in the book: move it and leave a link.

- [ ] **Step 6: Report**

Print a summary for the user: pages written, screenshots added, `CLAUDE.md` before and after line
counts, and anything the acceptance criteria in `docs.design.md` do not yet satisfy.

**Nothing is committed. Stop and let the user commit.**

---

## Acceptance

The plan is complete when every acceptance criterion in `docs.design.md` holds:

1. Every one of the 100 headed sections of the original `CLAUDE.md` is accounted for by the
   migration map, and every fact appears in the book or was dropped as history.
2. No relative link is dead; every named path, port, binary and configuration key exists.
3. No page narrates history.
4. Every count matches the tree.
5. `CLAUDE.md` is under 450 lines and carries no knowledge the book carries.
6. Every page has an opening paragraph and a **See also** list.
7. No page mentions `kdos-be`, a second desktop session, `KDOS_SESSION`, `kdos-bar` or
   `kdos-files`.
8. Nothing is committed.
