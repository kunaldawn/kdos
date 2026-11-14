# KDOS v0.2 release — a 2 GB live ISO, every port prebuilt beside it

The end-to-end plan for the first public KDOS release: what ships, how it is built, every task
with its status and its proof, how it is verified, and how it is published. Measurements are
from the build tree of 2026-11-12 unless a row says otherwise.

Status marks: **done** (in the working tree, its checks green), **in progress** (being written
now), **todo**, **blocked** (with the reason). Nothing here is committed; the user commits.

---

## 1. What a user gets

- **One ISO of at most 1,900 MiB** on the release page: a single GitHub asset, nothing to join.
  It boots (UEFI or BIOS, Secure Boot off) into the full KDE Plasma desktop as a live session,
  and installs to disk from the same session.
- **On the ISO:** Plasma (KWin, the shell, SDDM, System Settings, Discover, the portal, KRunner),
  the KDOS look, desks, accents, Phosphor mode, settings pages, widgets and daemons; Firefox ESR;
  Konsole, Dolphin, Kate, Okular, Gwenview, Ark, Spectacle, KCalc, Elisa, KDE Connect,
  Partition Manager, Yakuake; NetworkManager, Bluetooth, PipeWire, CUPS and scanning, the box
  stack, the installer, `kpkg` and the `kdos` tools.
- **One step away, prebuilt:** every other of about 2,400 native ports, from the release's
  signed binhost — one at a time or in groups (KDE games, education, PIM, multimedia, graphics,
  utilities, development; office, graphics, internet, multimedia, games, science, development,
  Windows, build tools, Chinese/Japanese/Korean input).
- **Or built on the machine** from the same recipes, with what a build needs downloaded or built
  too, by choice.
- **Or boxed:** 70 Debian applications from the catalogue, each in a rootless container, for
  what KDOS does not port or ports at an older version.
- **Updates** arrive as signed whole-slot images (or packages between releases) written into
  the inactive A/B slot; everything the user installed follows, by the route it came.

## 2. Design

### 2.1 The base image

- `ports/base` names the seeds; `script/lib/imageset.py` computes their closure over each
  recipe's first `depends =` line, not following into `ports/buildonly` (compilers and
  generators that are only build edges).
- `70_image/017_stage.sh` trims an overlay over `build/fs` (`script/chroot/exec.sh` roots every
  step from 017 on in it) to that closure; `build/fs` keeps every package for the next build
  and the binhost. `KDOS_IMAGE=full` keeps everything (the offline medium).
- `087_gates.sh` (linkcheck, entrycheck) is the proof the closure is whole: a `DT_NEEDED` or a
  `#!` that resolves to nothing left fails the build.

### 2.2 Facets: run-time files only on the ISO and installed machines

An install-time filter over the one signed package file. `libkpkg` classifies each path as
run time, development (headers, static libraries, CMake and pkg-config files, unversioned `.so`
links, man 2 and 3) or documentation (`share/doc`, `info`, `gtk-doc`). With `FACETS=run` (the
image's `kpkg.conf`; the build chroot keeps `all`) the non-run files are not installed, not in
the manifest or hash sidecar, and recorded in `<db>/.facets/<name>`. `kpkg facets --strip`
applies it to installed packages; 017 runs it over the stage. `kdos update install --dev <port>`
reinstalls a package prebuilt with `FACETS=all`; a source build does that for each dependency it
compiles against, after installing `build-tools`. No new package names: signatures, recipe
hashes, the binhost index and every name check are unchanged.

### 2.3 The binhost is the release's package repository

`010_binhost.sh` (`KDOS_MAKE_BINHOST=1`) moves every kept package out of `build/fs`'s cache into
`build/binhost`, indexes with every repository (so KDOS's own packages carry their recipe hash),
publishes only current packages, and fails on a package larger than one asset. A machine takes
the binhost of the release its `/ports/TREE` names, so packages always match their base.

### 2.4 Three ways to install, remembered

`kdos update install [--prebuilt | --source [--build-deps=prebuilt|source]] [--dev] [--root DIR]
[--pending] <port|group>`, defaults from `/etc/kdos/update.conf` (`install =`, `build-deps =`,
both `prebuilt`), set from System Settings › System Versions › Installing software through
`io.kdos.Slots1.SetInstallRoute`. `/var/lib/kdos/origin` records each package's route; updates
build source-origin packages again and download the rest. Discover offers "KDOS system"
(prebuilt, `kdos-ports-helper`) and "KDOS, built on this computer" (`kdos-ports-builder`, polkit
`io.kdos.ports.build`, asked every time) as two origins of one application.

### 2.5 Updates keep what the user installed

The image lane writes the base, then installs the world set into the slot by origin from the
same release's binhost, refusing before it writes anything when the binhost cannot cover it.
`origin`, `world`, `apps-pending` and `groups-pending` are carried into the slot at the arm.

### 2.6 Groups

`ports/groups` (`group <id> <title>`, then member ports), staged at `/ports/groups` and carried in
the signed tree. `kdos update groups [--json]` lists them with installed state and download size.
The installer's Applications page lists native groups above the boxed catalogue (`groups =` in
the answer file); chosen ones are installed into the new disk during the install from the
medium's own release (`kdos update fetch --binhost-only`, cache in the target), or recorded in
`groups-pending` for `sudo kdos update install --pending` when offline.

### 2.7 The budget is a gate

`088_ledger.sh` writes `build/image-ledger`: each package on the ISO, its bytes and why it is
there (`imageset.py --why`). `110_iso.sh` fails a release build (`KDOS_IMAGE=base`,
`KDOS_ISO_GATE=1`) whose ISO passes `KDOS_ISO_MAX` (1,900 MiB) and prints the ledger's top
twenty. The release notes carry the ledger summary.

## 3. The 2 GB budget

Today's base: 1,004 packages, **9.53 GB of files** (hardlinks once) → about 3.9 GB of ISO at the
whole-tree ratio (0.41). The target is about **4.8 GB of files**.

| Cut | Files saved | Task |
|---|---|---|
| Development and documentation payload (facets): 1,103 MB static libraries, 493 MB headers, 320 MB docs and info, 28 MB CMake and pkg-config | 1.9 GB | T3.1 |
| clang, libclc, spirv-llvm-translator (Mesa without OpenCL) | 0.7 GB | T3.3 |
| The toolchain (gcc's compiler, cmake, doxygen, sphinx, docbook, ruby, scons) | 0.5 GB | T3.3 |
| `plasma-workspace-wallpapers` (309 MB), `oxygen` (123 MB) | 0.43 GB | T3.3 |
| QtWebEngine and its consumers (ktorrent, khelpcenter, online accounts) | 0.3 GB | T3.3 |
| Firmware for less common hardware | 0.35 GB | T4.1 |
| CJK input engines, boost and numpy through them | 0.25 GB | T3.3 |
| `kdos-kfs` (121 MB), the Qt 5 IM stack, Samba file sharing | 0.25 GB | T3.3 |
| **Total** | **≈ 4.7 GB** | ≈ 1.9 GB at 0.40; less with the measured codec (T3.4) |

With the firmware split (T4.1) the base keeps 262 MB of firmware instead of 596 MB. The current
estimate: 855 packages of today's tree are 4.91 GB of files without any firmware, plus 0.26 GB
of the four vendors' firmware, less about 0.95 GB of facets: **about 4.2 GB of files, an ISO of
about 1,750 MiB at zstd:19**, 150 MiB under the gate. It is an estimate until T6.3's build runs the
ledger and the gate.

### Measured codecs

Measured 2026-11-12 on the base of the current build tree (1,004 packages, 9,535,467,742 bytes of
package files, hard links once), with squashfs-tools 4.7.5 on 16 threads in a root container over
a hard-link copy of exactly those files. *Read* is `unsquashfs -p 1 -pf /dev/null` (every block
decompressed on one core, image in the page cache), the stand-in for boot cost; *read N* is the
same on 16 threads. Write times are on a shared machine and vary by about a fifth.

| Codec | Block | Tree | `system.sfs` bytes | MiB | Write | Read (1 core) | Read N |
|---|---|---|---|---|---|---|---|
| zstd:15 | 1M | base | 3,441,373,184 | 3,282 | 80 s | 7.2 s | 2.6 s |
| zstd:19 | 1M | base | 3,256,926,208 | 3,106 | 229 s | 8.7 s | 2.5 s |
| zstd:22 | 1M | base | 3,256,471,552 | 3,106 | 286 s | 8.7 s | 2.5 s |
| xz | 1M | base | 3,122,028,544 | 2,977 | 237 s | 58.1 s | 8.3 s |
| xz-bcj | 1M | base | 3,052,171,264 | 2,911 | 484 s | 61.6 s | 8.7 s |
| zstd:19 | 128K | base | 3,482,406,912 | 3,321 | 357 s | 11.0 s | 3.7 s |
| xz-bcj | 128K | base | 3,272,548,352 | 3,121 | 418 s | 69.1 s | 11.5 s |
| zstd:19 | 1M | base − facets | 2,965,721,088 | 2,828 | 205 s | 7.3 s | 2.2 s |
| xz-bcj | 1M | base − facets | 2,774,777,856 | 2,646 | 425 s | 52.0 s | 6.7 s |
| zstd:19 | 1M | **slim base, run facets** | 1,867,137,024 | **1,781** | 99 s | 4.2 s | 1.3 s |
| xz-bcj | 1M | **slim base, run facets** | 1,761,239,040 | 1,680 | 212 s | 29.1 s | 3.9 s |

*Slim base, run facets* is T3.3's base (853 packages) with T3.1's rules applied, gcc's
`facet_dev` line included: 4,506,990,417 bytes of files. With about 90 MiB beside `system.sfs`
(kernel, initramfs, Limine, the `/ports` append), the ISO is **about 1,870 MiB at zstd:19**, under
the 1,900 MiB gate by about 30 MiB, and about 1,770 MiB at xz-bcj. zstd:19 stays the default;
the firmware split (T4.1, about 0.35 GB of files) is what makes the margin comfortable, and
xz-bcj is the fallback if a build lands over the gate before it.

*Base − facets* leaves out `usr/include/**`, `*.a`, `usr/lib/cmake/**`, `usr/lib/pkgconfig/**`,
`usr/share/pkgconfig/**`, `usr/share/aclocal/**`, `usr/share/doc/**`, `usr/share/info/**`,
`usr/share/gtk-doc/**` and `usr/share/man/man3/**`: 1.86 GB of files (7,676,338,312 bytes remain),
which compress to only 278 MiB of zstd:19 — the dev payload squashes at about 0.16, not the 0.40
the budget above assumed for it.

**Chosen: zstd:19, 1 MiB blocks** (`110_iso.sh`'s default). zstd:22 is no smaller (a 1 MiB block
caps the window). xz-bcj is 6.3% smaller than zstd:19 but reads 7x slower: 53 s more of one core
for the whole image, against 1.5 s for zstd:19 over zstd:15, so zstd:19 meets the 10% rule on any
boot and xz-bcj only if a boot reads little of the image. **Open:** a boot to Plasma timed under
QEMU for zstd:19 and xz-bcj; switch the default to xz-bcj if it is within 10%. 128 KiB blocks cost
7% on both codecs.

**The ISO this predicts** (system.sfs + about 60 MB of kernel, initramfs and Limine outside it +
the `/ports` append and the files no package owns, a few tens of MB; call it 90 MiB): the base today
≈ **3,200 MiB** with zstd:19 (≈ 3,000 MiB with xz-bcj); with the facet cut ≈ **2,920 MiB**
(≈ 2,740 MiB with xz-bcj). All are well over 1,900 MiB: the facet cut saves about 280 MiB and
xz-bcj another 180, so T3.3's cuts must take roughly another 1,020 MiB of `system.sfs` under
zstd:19 (840 under xz-bcj), about 3 GB of files at the base's measured 0.34, before the gate
passes.

## 4. Tasks

### Phase 1 — the base image and the release binhost

| Task | Status | Proof |
|---|---|---|
| T1.1 `ports/base`, `ports/buildonly`, `script/lib/imageset.py` (`--keep --drop --sizes --check`) | done | preflight's image-base check; closure simulated: every `DT_NEEDED` and `#!` resolves (clang kept off the build-only list for Mesa) |
| T1.2 The stage: `exec.sh` overlay from 017 on, `017_stage.sh`, `KDOS_IMAGE`, stage dirs kept root's | done | preflight; never run in a build |
| T1.3 `020_cleanup.sh` cleans the chroot's own root, never `build/fs` through the stage | done | read; never run in a build |
| T1.4 Binhost: all repositories indexed, current packages only, size gate, packages moved out of the cache | done | read; never run in a build |
| T1.5 `release-image.sh --upper`: the slot image of a base build | done | read; never run |
| T1.6 `ports/groups` in the signed tree (`ports/publish`) and in `/ports` (`105_ports.sh`) | done | read |
| T1.7 `thin-provisioning-tools` in the base (090's initramfs requires it with lvm2) | done | `imageset.py --check` |

### Phase 2 — installing, updating, groups

| Task | Status | Proof |
|---|---|---|
| T2.1 Routes: `update.conf` keys, `--prebuilt --source --build-deps`, origin file, dry-run format | done | selftest |
| T2.2 Per-origin installs in the binhost lane; `builds` in the update document | done | selftest |
| T2.3 The image lane's world step and its coverage refusal | done | selftest (host and container, as root) |
| T2.4 `kdos update groups`, group install, `--root`, `--pending`, `fetch --binhost-only` | done | selftest |
| T2.5 Discover's built-here origin, `kdos-ports-builder`, `io.kdos.ports.build` | done | QtTest; selftest (the helper's door) |
| T2.6 `SetInstallRoute`, `InstallRoute`, `BuildDeps`, polkit `io.kdos.slots.install-route`; System Versions › Installing software | done | QtTest; container selftest (sysupdated, polkit table) |
| T2.7 Installer: native groups (core, TUI, GUI), `groups =`, the Packs step, `groups-pending` | done | selftest (dumps, answer file); QtTest (GUI answers) |
| T2.8 `origin` and `groups-pending` carried at the arm | done | selftest (carry) |
| T2.9 Discover shows the build count before the password: the plan runs `kdos update install --dry-run` as the user and asks through `Transaction::proceedRequest` | done | QtTest (parser over fixtures, the backend's ask, cancel and refusal) |
| T2.10 Discover's default origin follows `install =`: lists carry only the preferred twin (Discover's dedup keeps whichever of two equal rows its sort leaves first), a link answers both, preferred first | done | QtTest |
| T2.11 An image update keeps `--dev` restorations: the running root's database is the list (no `.facets` record under `FACETS=run`, a dev or doc file in the manifest), coverage is checked before the slot is written, and the world step installs them whole in the slot | done (the install into a slot unrun) | selftest (image dry run: refusal, plan, a record, run-only files and `FACETS=all` list nothing; host and container) |

### Phase 3 — under 2 GB (this round)

| Task | Status | Proof |
|---|---|---|
| T3.1 Facets: `kp_facet_of`, `FACETS`, the install filter, `.facets/` sidecar, `kpkg facets --strip --list`, 017 strips the stage; recipe key `facet_dev` (gcc's compiler: 270 MB) | done | selftest (host and container); library self-test 1,127 checks; 949 MB of files freed on the slim base |
| T3.2 `kdos update install --dev`; the source route installs any missing `build-tools` member and restores the dev facets it compiles against; plain `kpkg install` refuses with the command to run | done | selftest |
| T3.3 Slim `ports/base`: explicit application list in place of `kde-gear-core`; `kde-plasma` seeds without the wallpaper and Oxygen collections; toolchain to `ports/buildonly`; CJK engines, `kdos-kfs`, file sharing off; QtWebEngine consumers off; `mesa` without OpenCL and a new `mesa-opencl` port; groups `build-tools`, `lang-zh`, `lang-ja`, `lang-ko`, `kde-network`, `kde-system`, `kde-artwork`; `kdeplasma-addons` without WebEngine; khelpcenter callers re-pointed to `xdg-open` | done (not built) | 853 packages, 5.46 GB of files (from 1,004 and 9.54 GB); DT_NEEDED, `#!`, QML and Python simulation over the kept set; phaseclosure; both Mesa configurations pass `meson setup` |
| T3.4 Codec measured on the base, with and without facets; `xz-bcj`, `KDOS_ISO_BLOCK`; default chosen | done (boot timing open) | the table in §3 "Measured codecs"; default zstd:19 in `110_iso.sh` |
| T3.5 `imageset.py --why --ledger`, `088_ledger.sh`, the 1,900 MiB gate in `110_iso.sh` (`KDOS_ISO_MAX`, `KDOS_ISO_GATE`) | done (a run in a real 70_image open) | preflight clean; `--ledger --root build/fs` total of the base equals `du -sb` of a hard-link copy of its files (9,535,467,742); the gate block run on a sparse 2,000,000,000-byte ISO fails with the ledger's top twenty and passes under `KDOS_ISO_GATE=0`, `KDOS_IMAGE=full` and a raised `KDOS_ISO_MAX`, and refuses malformed values |

### Phase 4 — the lean ISO made pleasant

| Task | Status |
|---|---|
| T4.1 `linux-firmware` split by vendor; the ISO carries Intel and AMD graphics and Wi-Fi, Realtek and SOF; `kdos firmware sync` installs what the machine's modaliases and the kernel's firmware-load failures name; the installer and a live-session notification run it | done — eight vendor ports and `firmware-map`; the ISO keeps Intel, AMD, Realtek, MediaTek (262 MB of 596); `kdos firmware sync`; a Firmware install step; a kdos-notice notice; selftest and preflight |
| T4.2 Measured run-time dependencies: `kpkg` records `DT_NEEDED`, `#!`, QML and Python imports and a recipe's `runtime =`; the closure follows them; `ports/buildonly` shrinks; the QML gate | done — `.needs` records (so, exe, qml, py, need), `kpkg needs --scan --resolve`, `runtime =`, `imageset.py --verify` as 087's fourth gate; measures nothing until packages are reinstalled |
| T4.3 The installer ticks `lang-*` from the chosen layout and locale | done — layout and time zone tick `lang-*` when the answer file names no groups; selftest and QtTest |
| T4.4 "Make it yours": the Welcome Center's last page installs groups prebuilt, with sizes | done — Welcome Center page 95, `pkexec kdos-ports-helper install <group>`; QML test; not seen on a rig |

### Phase 5 — documentation (with each change, hard rule 1)

| Task | Status |
|---|---|
| T5.1 README from the user's side; the name, *Kool Desktop Operating System* | done |
| T5.2 Both books and the port notes for phases 1–2 | done (docscheck, anchorcheck, booknav, kfscheck green) |
| T5.3 Both books for phase 3 (facets, the slim base and app list, the ledger and gate, codecs) | in progress |
| T5.4 The README and user guide's "what is on the ISO" list after T3.3 settles it | in progress |

### Phase 6 — build, prove, release

| Task | Status |
|---|---|
| T6.1 Free disk: a `KDOS_MAKE_BINHOST=1` build keeps every package (tens of GB) beside the snapshots; today 24 GB are free | **blocked** — the user's disk |
| T6.2 Release key: `kpkg keygen build/keys/kdos-release`, backed up offline; its key id published in the README | todo |
| T6.3 Rebuild what the recipes changed (mesa, mesa-opencl, kdos-tools, kdos-kpkg, kdos-discover, kcm-kdos-system, kdos-sysupdated, kinstall, kdos-installer-gui), then `make build KDOS_MAKE_BINHOST=1` (base image) | blocked on T6.1 |
| T6.4 The ISO is ≤ 1,900 MiB (the gate); the ledger reviewed | blocked on T6.3 |
| T6.5 Rig proof (§5) | blocked on T6.3 |
| T6.6 Tag `v0.2`, build from the tag, `release-image.sh --root build/fs --upper build/image-upper`, `make release` (draft), check the draft from another machine, `make release-publish` | blocked on T6.5 |

## 5. Verification

**Every change:** `testing/preflight.sh`, `testing/selftest.sh` (host and `testing/devdeps-image.sh`),
`make plasmatest` for Qt code, `docscheck`, `anchorcheck`, `booknav`, `kfscheck`, `make book-stamp`.
The `kdos rebuild` self-test block needs 25 GB free and fails on today's disk for that reason
alone. Three QML lint findings fail on the committed tree too and are the development image's, not
the code's: `kdos-installer-gui/qml/AccountPage.qml`, and `kdos-welcome`'s `00-Accessibility.qml`
and `60-Keys.qml` (plasma-welcome's `Utils` there has no `globalShortcutTexts`). A full `make plasmatest` adds more of the same kind, in components this plan did not change:
`kdos-sddm-theme`'s build (the image has no Python `PIL`), and the QML lint of
`plasmoid-kdos-privacy`, `kwin-effect-kdos-keys` and the two KWin scripts (KWin and Plasma
types the image's lint cannot resolve).

**The rig proof (T6.5), offline, on the built ISO**, with `build/binhost` served as `binhost =`
from a second drive:

1. The ISO boots to Plasma under QEMU (UEFI, then BIOS); its size is under the gate.
2. Live session: `kdos update groups`; `sudo kdos update install office` lasts until restart.
3. Install with the `games` native group and the `essential` boxed group ticked; the group is on
   the installed disk, `groups-pending` is empty; repeat offline: `groups-pending` holds it and
   `sudo kdos update install --pending` installs it.
4. Installed: Discover installs Kdenlive prebuilt and one port built here; `kdos update install
   --source bat` with `build-deps = prebuilt` installs `build-tools` and the dev facets it needs;
   `--dev zlib` restores headers.
5. Build a mock next release (`--prev`), apply the image lane: every installed program survives
   the slot switch, the built-here one rebuilt, the rest downloaded.
6. `kpkg check --all` passes on the slot with facets left out.

## 6. Risks

- **The budget is estimated**, from file sizes and the whole tree's ratio; static libraries
  compress better than what stays. The gate and the measured codecs (T3.4) settle it; the
  firmware split (T4.1) is the next lever, then Mesa's LLVM.
- **Closure by `depends =`** cannot see QML and Python imports; T3.3 simulates them on the
  current tree, T4.2 makes it a gate.
- **Facets and source builds:** a build that reads a file the rules call documentation (a
  template under `share/doc`, say) fails; the restore puts the whole package back, and the rules
  are measured on the current tree before they ship.
- **No real-hardware matrix:** QEMU only; the release says so.
- **The whole plan is unbuilt** until T6.1 frees the disk.

## 7. Decisions

- The ISO is at most 1,900 MiB: one asset.
- The base is an explicit list; the toolchain is not in it: the first source build installs it.
- Facets are an install-time filter over one package, not separate packages.
- Groups are lists in `ports/groups`, not meta ports: no recipe, shelf or phase slot per group.
- `groups =` is its own installer key: ids such as `office` exist in the boxed catalogue too.
- Default route `prebuilt`, `build-deps = prebuilt`; an install never rebuilds an installed
  package; a package keeps the route it came by until it is installed again by the other.
- `ports/buildonly` is a closed list until T4.2 replaces it with measured edges.
- The docs describe v0.2 as published; they go out with the release, not before.
