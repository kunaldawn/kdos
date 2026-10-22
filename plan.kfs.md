# plan.kfs.md — *KDOS from Scratch*, the second book

A Linux From Scratch + Arch Wiki book for KDOS: phase by phase and package by
package, how the system is built, and topic by topic, how it is run. For human
readers. `docs/kdos/` stays the authority on mechanism and rationale; this book
owns build steps, per-package pages and task procedures, and links to
`docs/kdos` for everything else.

Worktree: `~/workspace/repos/kdos-kfs`, branch `kfs` (from `v0.2` @ 69095ba0).

---

## Decisions (2026-10-18)

| # | Question | Decision |
|---|---|---|
| 1 | Coverage | **Tiered.** A: ~150 hand-written LFS-depth pages (bootstrap, 20/30/31, the KDOS spine, every `src/` port, 70_image). B: ~400 generated pages with a required note (patched, vendored, `build.sh` > 30 lines, ≥ 20 reverse deps). C: ~1,500 generated cards. |
| 2 | Where per-port "why" lives | **Sidecar beside the recipe**: `ports/core/<shelf>/<name>/kfs.md` (and `src/<area>/<name>/kfs.md`). Drift-checked against `build.sh`. Needs a hard-rule-6 carve-out and a recipe-hash exclusion. |
| 3 | Relation to `docs/kdos` | **Link, never restate.** Mechanism and rationale are linked by anchor; a tracked anchor checker keeps the links alive. Wiki pages carry numbered procedures, not explanation. |
| 4 | Toolchain | **Markdown + Python generator + pandoc (chunked HTML) + pandoc → typst (PDF).** Every tool is a KDOS port. |
| 5 | Build-state data | **Committed snapshot**: `make book-data` writes `docs/kfs/data/` from `build/fs`; a check marks a port stale when the snapshot's version differs from its recipe. |
| 6 | Publishing | **All four**: GitHub Pages via Actions, PDF + HTML tarball in Releases, shipped in the image, `make book` locally. |
| 7 | PDF | **One single PDF.** |
| 8 | Name and place | **"KDOS from Scratch"**, `docs/kfs/`. |
| 9 | Package order | **Phase → shelf → build order** (`kpkgdepends`). |
| 10 | Commands | **Both views**: the `kpkg install` one-liner, then "What kpkg does", expanded LFS-style with every flag explained from the sidecar. |
| 11 | Two builds of one package | **Separate pages per stage**; the port page carries a "differs from the bootstrap build" box. |
| 12 | Wiki pages | **Fixed skeleton**, config pulled from the tree at render time, enforced by a check. |
| 13 | SBU | **Instrument kdosbuild**: per-step seconds for every phase; 1 SBU = `00_cross/binutils`. |
| 14 | Patches | **`Subject:` header in every patch**, gated on new or changed patches only; old ones show "undescribed" until their port is next touched. |
| 15 | Host | **The os-dev container, documented honestly.** No bare-host path. |
| 16 | Tooling | **Shared with `docs/kdos`**: parameterised `docscheck.sh`, tracked `booknav.py` and `anchorcheck.py`, run over both books. `docs/kdos` content untouched. |
| 17 | First milestone | **Spine + generator + Part I.** |
| 18 | In-image | **A separate port, `kdos-kfs`.** |
| 19 | Tier C in the PDF | **Compact cards**, 3–4 per page; full file lists only in HTML. |
| 20 | Edition | **Tracks the tree.** Pages = development edition from `main`; each Release attaches a frozen PDF + HTML. |
| 21 | Patch gate start | **New/changed patches only.** |
| 22 | `kdos-doc` symlink | **Narrowed to `docs/kdos` + `docs/screenshots`**; `kdos-kfs` links `docs/kfs`. |
| 23 | Pages CI | **One workflow, both books.** CI pins upstream pandoc 3.11 / typst 0.15.1 by sha256 (the port versions) and lowdown for `docs/kdos`. `_config.yml` retires. |

---

## Shape of the book

```
docs/kfs/
  README.md                      title page, edition line, contents (booknav writes the contents)
  00-front/                      preface, conventions, typography, SBU, how to read a package page
  01-preparing/                  Part I  — Preparing: the os-dev container, sources and the archive,
                                           the fs/ overlay, the build tree, kdosbuild
  02-cross/                      Part II — The cross toolchain (00_cross): binutils, gcc pass 1
  03-bootstrap/                  Part III — The bootstrap (10_bootstrap): one page per step script
  04-chroot/                     Part IV — Entering the chroot, anatomy of a `kpkg install`
  05-selfhost/ … 14-kernel/      Parts V… — one part per phase: 20_selfhost, 30_foundation,
                                           31_compilers, 40_lang, 41_system, 42_graphics,
                                           43_toolkits, 44_apps, 50_desktop, 60_kernel
  15-image/                      70_image: one page per step; 090 initramfs and 110 iso as chapters
  16-wiki/                       System administration: 64 topic pages in nine groups, fixed skeleton
  17-appendices/                 flag glossary, canonical build shapes, SBU table, licences, glossary
  data/                          committed snapshot (decision 5)
```

Phase parts hold only their **narrative** (`intro.md`, one `<shelf>.md` intro
per shelf when it has something to say). **Package pages are generated** at
render time into `build/kfs/src/`, not committed, from:

| Section | Source |
|---|---|
| header table: version, release, shelf, phase, licence, homepage, SBU, installed size | `kpkgbuild`, `data/` |
| description | `kpkgbuild` |
| "Why it is in KDOS" | the comment above the port in its phase list |
| sources, sha256, download size | `kpkgbuild`, `ports/sources.idx` |
| depends / reverse depends | `depends =`, inverted |
| optional, off in KDOS | `testing/optdeps.py` |
| patches | patch filenames + `Subject:` and body |
| install: the one-liner | phase + name |
| what kpkg does: expanded commands | `build.sh` verbatim, banner stripped, with the sidecar's flag notes |
| contents: programs, libraries, directories | `data/` manifest, classified by path |
| short descriptions | `data/` whatis lines; `.pc` `Description:` for libraries |
| notes, traps | sidecar `kfs.md` |

### The sidecar `kfs.md`

```markdown
## About
One or two paragraphs: what this is to KDOS (Tier A/B only).

## Flags
- `--disable-static` — <why>
- `-DBUILD_TESTING=OFF` — <why>

## Patches
- `0001-foo.patch` — only when the patch has no `Subject:` header.

## Notes
Traps, ordering constraints, what breaks if it changes.
```

- **Drift check**: every flag a `## Flags` bullet names must still occur in
  `build.sh`; every patch it names must exist.
- **Recipe hash**: `kp_recipe_hash` excludes `kfs.md`, so editing a note
  never rebuilds the port (`src/libs/libkpkg/kp_hash.c`, documented in
  `packaging.md`'s E: section).
- **Hard rule 6** gains the carve-out: rationale lives in `kfs.md`, never in
  `kpkgbuild` or `build.sh`.
- **Tier B requirement**: a port meeting a Tier B criterion needs a `kfs.md`,
  gated like patches, on new or changed ports only.

### Bootstrap stage pages

The 24 scripts in `00_cross` and `10_bootstrap` are not ports. Each gets a
hand-written page in `02-cross/` or `03-bootstrap/` whose commands are pulled
verbatim from the script at render time, with the flags explained in the page.
Its port counterpart's generated page carries "differs from the bootstrap
build" with the two flag sets compared.

### Wiki skeleton

Every page under `16-wiki/` has, in order: `## Installation`,
`## Configuration`, `## Usage`, `## Tips`, `## Troubleshooting`,
`## Known gaps`, `## See also`. A configuration excerpt is written as a
directive the renderer fills:

```markdown
<!-- kfs:include fs/etc/nftables.conf -->
<!-- kfs:include ports/core/auth/pam/build.sh lines=40-72 -->
```

so it is read from the tree on every render and cannot drift.

---

## Data snapshot (`docs/kfs/data/`)

`make book-data` (host, reads `build/fs` and `build/snapshots/timings.json`):

- `packages.tsv` — name, version, release, installed bytes, file count, SBU
- `manifest/<name>` — the kpkg db file list, one per installed package
- `whatis.tsv` — program, section, one-line description, owning package
- `pc.tsv` — library `.pc` name, description, owning package
- `stamp` — the snapshot's tree commit and date

A port whose `packages.tsv` version differs from its recipe renders with a
"snapshot predates this version" note; the check counts them and fails only
in `--strict`.

### SBU instrumentation

`kdosbuild` writes per-step seconds for **every** phase into
`build/snapshots/timings.json`, merged rather than replaced, so a
`--continue-from` run keeps the earlier phases' figures. A step served from
`build/pkgstore` records no time, and the earlier figure survives.

---

## Rendering (`make book`)

`script/book/` (host and chroot alike, Python 3 standard library only):

- `kfsgen.py` — reads the tree and `data/`, writes generated pages plus the
  hand-written ones into `build/kfs/src/`, expands `kfs:include`, numbers
  chapters, assigns stable anchors `port-<name>`, escapes recipe text for
  Markdown.
- `template.html`, `kfs.css` — chunked HTML, one page per section, KDOS
  palette, light and dark.
- `kfs.typ` — the typst template: title page with edition, TOC, running
  heads, compact cards for Tier C, index.
- `index.lua` — a pandoc Lua filter that turns `{.idx}` spans into typst index
  entries, plus an HTML index page.

Outputs: `build/kfs/html/`, `build/kfs/kfs.pdf`, `build/kfs/kfs-html.tar.zst`.

Tools: pandoc and typst from `build/fs`, through its own loader
(`build/fs/usr/lib/libc.so --library-path build/fs/usr/lib`), so the book is
built by KDOS's own binaries. `KFS_PANDOC` / `KFS_TYPST` override.

---

## Checks

- `testing/docscheck.sh [--book docs/kdos|docs/kfs]` — both books by default;
  the page contract per book (kfs also requires its wiki skeleton).
- `testing/booknav.py` — writes or checks the nav footers and the contents
  of both books.
- `testing/anchorcheck.py` — every `#fragment` in both books resolves, with
  GitHub's heading-id rule.
- `testing/kfscheck.py` — sidecar drift, Tier B note requirement (new or
  changed), snapshot staleness count, that every recipe renders, and that
  every bootstrap page's include still resolves.
- Patch headers: `testing/preflight.sh` and `script/hooks/pre-push` refuse a
  new or changed `.patch` without `Subject:`.
- `make check` runs them all; pre-push runs docscheck and kfscheck.

---

## Publishing

- **`kdos-kfs` port** (`src/system/kdos-kfs`, 41_system `src-system.txt`,
  `depends = pandoc typst python3`): `docs -> ../../../docs/kfs`, plus
  `script/book/` copied in. It installs `/usr/share/doc/kfs/` (HTML + PDF).
  The renderer reads recipes live from `/kdos/ports`, which no recipe hash
  covers, so `docs/kfs/data/recipes.stamp` holds one aggregate hash over every
  recipe file. `kfscheck` fails when it is stale, and `make book-data` rewrites
  it. A recipe edit therefore changes `docs/kfs` and rebuilds `kdos-kfs`.
- **`kdos-doc`**: its `docs` symlink narrows to `docs/kdos`, with screenshots
  linked separately; `mkbook.py` arguments follow.
- **`.github/workflows/books.yml`**: on push to `main`, render `docs/kdos`
  with `mkbook.py`'s HTML half (lowdown from apt) and `docs/kfs` with
  `make book` (pinned pandoc/typst release binaries), then deploy to Pages.
  On a release tag, attach `kfs.pdf` and `kfs-html.tar.zst`. `_config.yml`
  is deleted.

---

## Documentation this changes (hard rule 1)

`CLAUDE.md` (rule 6 carve-out, the map row for the new book),
`writing-ports.md` (the sidecar, patch headers), `packaging.md` (E: hash
excludes `kfs.md`), `testing.md` (the new checks), `developing.md`
(`make book`, `make book-data`), `repository-layout.md` (`docs/kfs`,
`script/book`, `.github`), `04-programs/README.md` or wherever `kdos-doc` is
described (the narrowed symlink), `ports-catalogue.md` (the new port),
`README.md` (the second book).

---

## Milestones

All five are done (2026-10-18). Measured at the end of M5: 2107 ports render
(Tier A 228, Tier B 641, Tier C 1238); every Tier A and Tier B port has a
`kfs.md` (955 notes, 86 of them on Tier C ports); the render is 2558 HTML
pages and a 3917-page, 34 MB PDF in about 80 s.

**M1 — spine + generator + Part I** — done.
1. Generator, templates, `make book`, `make book-data`, first snapshot.
2. Every port renders as at least a Tier C card; HTML and PDF build.
3. Shared checks, the sidecar drift check, the patch-header gate.
4. `kp_recipe_hash` exclusion, kdosbuild timing merge.
5. `kdos-kfs` port, `kdos-doc` narrowing, the Actions workflow.
6. Front matter and Parts I–IV written in full.
7. Documentation updates above.

**M2** — done. 20_selfhost, 30_foundation, 31_compilers: part intros and
sidecars for every port of the three phases.
**M3** — done. The KDOS spine sidecars; Parts XIII (desktop), XIV (kernel:
configuring, building, installing, a 120-row option table checked against
`kdos.config` by `kfscheck`'s `kernel` check) and XV (one chapter per
`70_image` step, the initramfs and its init, the ISO).
**M4** — done. Part XVI, 64 wiki pages in nine groups (System, Boot,
Storage, Security, Networking, Hardware, Packages and applications,
Desktop, Troubleshooting), the groups written as `[Label]` tokens in the
part's `kfs:order` and as `####` headings in README.md's contents.
**M5** — done. A note for every Tier B port, the flag glossary, the build
shapes, build times, licences and glossary appendices. Note links are
mapped like a page's (relative on GitHub, anchors in the book).

---

## Open items

What remains needs a build by this worktree, an installed disk, hardware,
or a decision of the user's. Measurements behind each are under
~/.cache/kdos-portwork/<item>/evidence.md.

- **A build by the worktree.** The ISO and build/fs are main's c4fc3bf5.
  Not in them, so seen only by syntax check, host build or a patched guest:
  `fs/etc/mtab`, `fs/etc/resolv.conf`'s signature line, `35_chrony.sh`'s
  0750 run directory, the sshd/syslogd pid files, containers-common's
  storage.conf, `accept4(SOCK_CLOEXEC)` in kdos-powerd, bootctl's deploy
  pipe fix (host and static-binary guest only), kdos-shell's `disks.c`,
  `print.c`, `audio.c`, panel quick-launch and run-or-raise, `kb_proc.c`,
  the `service <verb> <name>` hints, the installer's legacy-BIOS
  warning on its Welcome page (red; says the disk starts on UEFI only,
  never drawn on a BIOS boot in the rig), and the `kdos-kfs` port. A
  `KDOS_GOLDEN_UPDATE=1` selftest in kdos-devdeps reproduces every golden
  in the worktree byte for byte.
  Build times exist for 20/30/31 only: c4fc3bf5's kdosbuild recorded no
  port time after 31_compilers; a worktree build then `make book-data`
  fills 40_lang..60_kernel. `data/stamp` names no machine.
- **An installed disk.** kdos-oomd kills kinstall's rsync in a 4 GB VM
  (known gap); one install succeeded with oomd stopped and booted (LVM,
  UEFI: A/B trial, menu trial, rollback, single-user, emergency). Still
  unbooted: an encrypted root, thin/cached LVM, BIOS (the installer's GPT
  has no BIOS boot partition, a known gap the user guide, kinstall,
  boot-and-init and the wizard now all state; whether to add a BIOS boot
  partition or refuse a legacy install is the user's call), the installer's fstab swap
  lines, keymap, serial login, CUPS-off, a box warmed from the desktop.
  A full install needs ~48 GB of host disk.
- **Hardware.** Radios, printers, scanners, SMART/NVMe, backlight, lid,
  battery, Thunderbolt, fingerprint/FIDO/smart card, suspend, capsule
  updates, a physical firmware's BootNext, GPUs other than virtio-gpu
  (whether the live splash shows before fbcon takes over).
- **Tree defects reported, not fixed (the user's call).**
  - kdos-oomd kills the installer's copy (protected list lacks rsync).
  - A persistence store on a second USB stick is not used: the initramfs
    runs `blkid -L KDOS_PERSIST` once, before usb-storage has settled.
  - `service restart networkmanager` in a session: kdos-netagent
    bus-activates an unsupervised NetworkManager in the gap.
  - The supervised ModemManager never holds its bus name (NetworkManager
    activates another), while `service status` says OK.
  - smbd has no /var/log/samba or /var/cache/samba (needs a recipe mkdir).
  - The pack daemon picks a kept `<id>-<ver>.kpack` over `<id>.kpack`.
  - `kdos-box remove` ignores podman's result, with or without --force.
  - `kdos-record` writes an empty file on the software renderer.
  - BRLTTY with `speech-driver en` crash-loops while status says OK.
  - `who -r` prints nothing: the records hold no RUN_LVL entry (cause
    not traced; the pages say so).
  - kdos-oomd, kdos-packd, kdos-energyd, kdos-mountd accept without
    SOCK_CLOEXEC, as kdos-powerd did.
  - The installer's font lookup under /mnt/iso is hidden by its own /mnt
    target (no `font.bin` on an installed menu); its `--dump plan` says
    "all of the group" while it makes `-l 50%VG`.
  - `kdos-backup --once` discards restic's output; `kpkg check --all`
    exits 1 on a pristine image (the emptied /var/log, /var/cache dirs).
  - On musl (kdos-devdeps) selftest.c's "a partial table names the owner
    the full one names" fails, on c4fc3bf5 as in the worktree: for a path
    two packages claim, `kp_owned_load_some` and `kp_owned_load` pick
    different owners. The glibc host run passes.
  - syslogd half-escapes the em dash in oomd's kill line, so plain grep
    calls /var/log/messages binary (the wiki uses `grep -a`).
  - polkitd's `--no-debug` discards `polkit.log()` output; the ghc
    `CONF_GCC_LINKER_OPTS_STAGE1` line changes no binary; sysklogd's
    "1M x 5" comment is ambiguous; pcscd's reader filters are not wired.
- **Tree text the measurements contradict, left for the user.**
  CLAUDE.md, testing.md, `store.c` say a pipeline reading a supervised
  service's output never returns; `service start x | cat` returned (the
  rcS loop's own comment, about a script run directly, still holds).
  `ngircd/build.sh` says musl has no getent (it ships one; editing it
  changes the recipe hash). `mpctl.c`'s comment on the watcher's exit.
- The ten in-app help pages in `fs/usr/share/kdos/doc/` stay as they are; the
  wiki links to them and does not replace them.
- `port.fix.plan.md` becomes moot for Pages once `_config.yml` retires.
