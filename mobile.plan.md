# KDOS Mobile — aarch64 / OnePlus 6T Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` or
> `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`)
> syntax for tracking.
>
> **This plan does NOT contain commit steps.** KDOS hard rule 5 — *"Do not auto-commit. The user
> commits manually, often squashing many edits into one logical commit."* — overrides the usual
> commit-per-task convention. Each task ends with a **Checkpoint**: a command whose output proves
> the task landed. Stop there and let the user commit.

**Goal:** Give KDOS a second build target — `aarch64-kdos-linux-musl` on the OnePlus 6T
(`fajita`, Qualcomm SDM845) and on QEMU `-M virt` — reaching a login prompt over USB ethernet
and SSH, using KDOS's own toolchain (`kdosbuild`, `kpkg`, the ports tree) and no Buildroot.

**Architecture:** A sibling phase tree (`script-mobile/`) and build root (`build-mobile/`) drive
the existing `kdosbuild` orchestrator through `--script-dir` / `--build-dir`, which are already
CLI flags. Phases 00–01 cross-compile a base userland with a genuine aarch64 cross toolchain;
phases 02–05 run inside an aarch64 chroot under `qemu-user` binfmt, so every existing
`ports/core/*/build.sh` recipe runs unmodified and native-shaped. `PORT_REPO` puts
`ports/mobile` ahead of `ports/core`, giving all 405 existing recipes for free with per-port
override. Two boards (`fajita`, `qemu-aarch64`) share phases 00–04 and their snapshots; only
the kernel phase and packaging differ.

**Tech Stack:** kdosbuild + libkbuild, kpkg + libkpkg/libkbase/libksig, musl, toybox, bash,
GCC cross toolchain, qemu-user-static + binfmt_misc, AOSP `mkbootimg.py`, fastboot.

**Spec:** This document. It supersedes the archived kmobile repo
(`/home/kunaldawn/workspace/repos/kmobile`), whose design spec and M0/M1 plan live at
`docs/superpowers/specs/2026-08-12-kmobile-design.md` and
`docs/superpowers/plans/2026-08-12-kmobile-m0-m1.md` in that repo. **Every device fact that repo
established is reproduced verbatim in Global Constraints below** — after this plan lands, the
kmobile repo is reference-only and may be archived.

---

## Global Constraints

### Provenance rule

Boot image offsets, firmware paths, kernel versions and the DTB name are **verified against
upstream, never recalled**. Each value below carries its source. Changing any of them means
re-reading that source.

### Device facts — OnePlus 6T (`fajita`, SDM845)

| Thing | Value | Source |
|---|---|---|
| Target triplet | `aarch64-kdos-linux-musl` | this plan |
| Kernel tarball | `https://gitlab.com/sdm845-mainline/linux/-/archive/sdm845-7.1-rc1-r0/linux-sdm845-7.1-rc1-r0.tar.gz` | pmaports `linux-postmarketos-qcom-sdm845/APKBUILD` |
| Kernel sha512 | `94da173aaf74dd33ef8ff9015e92759481cd0da5bd8a1e52c8664fd372d48a1155655001ad2e9d26557733c71ae8bc448e4ebdf1372b8f32159907a35f0306a9` | same APKBUILD |
| Kernel image | `Image.gz` (arch/arm64/boot) | — |
| DTB | `qcom/sdm845-oneplus-fajita.dtb` | pmaports `device-oneplus-fajita/deviceinfo` |
| boot.img base | `0x00000000` | same deviceinfo |
| kernel_offset | `0x00008000` | same deviceinfo |
| ramdisk_offset | `0x01000000` | same deviceinfo |
| second_offset | `0x00f00000` | same deviceinfo |
| tags_offset | `0x00000100` | same deviceinfo |
| pagesize | `4096` | same deviceinfo |
| header_version | `0` | same deviceinfo |
| Kernel cmdline | `console=ttyMSM0,115200 root=PARTLABEL=userdata rw rootwait` | deviceinfo + kernel-cmdline.conf |
| Console | `ttyMSM0` @ 115200 | same |
| Firmware commit | `3e31a0c3e5a061645c09f805387b49fa9d35acbf` | pmaports `firmware-oneplus-sdm845/APKBUILD` |
| Firmware tarball sha512 | `56604c46ad0f30f1f715af4b83bd2686d96723024063c7ef6baa5c73cd52b375478e3d65b886ff08efc95ce129fc789b8509efcdf9f0d46cf7229f4ec1afa513` | same APKBUILD |
| Firmware URL | `https://gitlab.com/sdm845-mainline/firmware-oneplus-sdm845/-/archive/<commit>/firmware-oneplus-sdm845-<commit>.tar.gz` | same |
| USB gadget IDs | `idVendor 0x1d6b`, `idProduct 0x0104` | Linux Foundation Multifunction Composite Gadget |
| Device IP | `172.16.42.1/24` (host takes `.2`) | this plan |

### Hard rules

1. **Never commit firmware blobs.** They are proprietary Qualcomm/OnePlus files fetched for
   this device's own use. `build-mobile/firmware/` is gitignored.
2. **Never write partitions other than `boot` and `userdata`.** `aboot`, `xbl`, `modem` and
   `persist` are off limits — writing them can hard-brick the device.
3. **Flashing runs on the host, never in the container.** No USB passthrough, no privileged
   container for flashing.
4. **Builds are offline** (`--network none`), exactly as the desktop target already is. Only
   `make fetch` and the firmware fetch touch the network.
5. **`make` is the only human entrypoint.** Do not invoke `docker`, `kdosbuild` or `kpkg` by
   hand in documentation.
6. **Every source file carries the KDOS ASCII banner**, comment-prefixed for the file's
   language, exactly as every existing file in this repo does.
7. **Do not auto-commit** (KDOS hard rule 5). Do not run destructive `git` operations
   (KDOS hard rule 6).
8. **No `kpkgbuild` rationale comments** (KDOS hard rule 4) — banner plus one-line
   `description` / `homepage` / `depends` keys. Reasoning goes in this file or a commit message.
9. **No source edits with `sed`/`awk`** (KDOS hard rule 7). Use build flags; ship a real
   `.patch` beside the recipe when there is genuinely no flag.

### Facts that are easy to get wrong

- **Firmware lives under `enchilada`, not `fajita`.** Both the OnePlus 6 and 6T use
  `qcom/sdm845/OnePlus/enchilada/` — this is what the fajita device tree asks for via
  `sdm845-oneplus-common.dtsi`. **Do not "fix" it.**
- **arm64 has no appended-DTB boot protocol.** `Image.gz` and the DTB are concatenated
  explicitly because the Qualcomm bootloader scans the blob for the FDT magic (`d0 0d fe ed`)
  after the gzip payload.
- **`mkbootimg.py` needs its `gki` sibling module.** Vendor both or the import fails.
- **The pmOS kernel config is clang-generated.** KDOS builds it with GCC; kconfig drops the
  clang-only options (CFI, shadow call stack) during `olddefconfig`. `CONFIG_MODULE_SIG` and
  `CONFIG_EFI_ZBOOT` must be turned off explicitly.
- **There is no initramfs.** The USB gadget must exist the moment userspace starts, so configfs
  and its ECM function are built in rather than modular, and root is mounted directly from
  `PARTLABEL=userdata`.

### Verified findings this plan rests on

Each was **measured in this repo**, not assumed. If one turns out false during execution, stop
and re-plan rather than working around it.

1. **`kdosbuild` already accepts `--script-dir DIR` and `--build-dir DIR`**
   (`src/build/kdosbuild/main.c:427-430`, `usage()` at `:48-49`; `KDOS_BUILD_DIR` env also
   honoured at `:420`). A second target needs **no orchestrator change**.
2. **`mgr_init` derives `repo_root` by stripping the last path component of `script_dir`**
   (`src/build/kdosbuild/manager.c:208-211`). Therefore the mobile phase tree **must be a direct
   child of the repo root** — `script-mobile/`, not `targets/mobile/script/`.
3. **`chroot_exec.sh` is looked up at `<script_dir>/chroot_exec.sh`**
   (`manager.c:216`). The mobile tree supplies its own, which it needs anyway.
4. **Phase env files are found by convention `<script_dir>/<phase-name>.env.sh`**, where the
   phase name is the directory name after its numeric prefix (`kb_phase.c:216-224`). A sibling
   tree therefore picks up its own env files automatically.
5. **`kb_is_dir` uses `stat()`, which follows symlinks** (`src/libs/libkbase/kb_fs.c:91-95`), so
   symlinked phase directories would resolve — kept in reserve, not used in this plan.
6. **`PORT_REPO` is an ordered search path and the first repo wins on a duplicate**
   (`kp_port_dir` at `src/libs/libkpkg/kp_conf.c:141-155`, and the same precedence in
   `kp_all_ports`). `PORT_REPO="/kdos/ports/mobile /ports/core"` gives all 405 core recipes with
   per-port override.
7. **The cross toolchain scripts are already fully arch-neutral.**
   `script/00_toolchain/00_binutils.sh` and `01_gcc.sh` derive everything from `$KDOS_TARGET`;
   there is no x86 string in either. Retargeting phase 00 is a one-variable change.
8. **aarch64 binaries execute inside Docker on this host with no qemu binary in the image.**
   `/proc/sys/fs/binfmt_misc/qemu-aarch64` is registered with flags `POF` — the `F` flag pins the
   interpreter at registration, so it survives into containers and chroots. Verified with
   `docker run --rm --platform linux/arm64 debian:bookworm-slim uname -m` → `aarch64`.
9. **Only 7 ports in `ports/core` carry an arch assumption**: `go`, `rust`, `libgmp`, `librsvg`,
   `musl-ldd`, `refind` (an x86 EFI bootloader — mobile never builds it), and `gcc`'s recipe.
   `fs/` is arch-clean.
10. **`ports/fetch` hardcodes `PORT_REPO="$SCRIPT_DIR/core"`** (`ports/fetch:26`) and must be
    taught about `ports/mobile`.

### What is explicitly NOT in this arc

Named so nobody adds them opportunistically. Each is a later arc.

- No desktop: no `kdos-comp`, `kdos-shell`, wlroots, mesa, Wayland, display or touch.
- No appbox / distrobox / podman.
- No `kinstall` — the phone is flashed with fastboot, not installed.
- No Wi-Fi: `qrtr`, `pd-mapper`, `rmtfs` and `ath10k` bring-up are a later arc.
- No modem, audio, GPU or camera.
- No ISO, no `refind`, no EFI, no microcode.
- No initramfs, no A/B slots, no encrypted root.

---

## Target Tree

Everything below is **new**. The only pre-existing files this plan modifies are `ports/fetch`,
`Makefile`, `testing/preflight.sh`, `.gitignore` and `CLAUDE.md` — the desktop build path is
otherwise untouched, and `make build` for x86_64 must keep working at every checkpoint.

```
kdos/
├── mobile.plan.md                      # this file
├── script-mobile/                      # the mobile phase tree (sibling of script/)
│   ├── chroot_exec.sh                  # qemu-user aware; CHROOT_DIR=build-mobile/fs
│   ├── kdosbuild.sh                    # thin wrapper: --script-dir/--build-dir
│   ├── toolchain.env.sh                # KDOS_TARGET=aarch64-kdos-linux-musl
│   ├── phase1.env.sh
│   ├── phase2.env.sh                   # CHROOT=1
│   ├── phase3.env.sh                   # CHROOT=1
│   ├── phase4.env.sh                   # CHROOT=1
│   ├── kernel.env.sh                   # host-side cross kernel build
│   ├── packaging.env.sh                # host-side; rootfs.ext4 + boot.img
│   ├── util/port.sh                    # copy of script/util/port.sh
│   ├── host/                           # host-side helpers, never run in the container
│   │   ├── fetch-firmware.sh
│   │   ├── flash.sh
│   │   ├── boot.sh
│   │   ├── usbnet-host.sh
│   │   ├── mkbootimg.py                # vendored AOSP, Apache-2.0
│   │   └── gki/
│   │       ├── __init__.py
│   │       └── generate_gki_certificate.py
│   ├── 00_toolchain/
│   │   ├── 00_binutils.sh
│   │   └── 01_gcc.sh
│   ├── 01_phase1/
│   │   ├── 00_file_system.sh
│   │   ├── 01_linux_headers.sh
│   │   ├── 02_musl_libc.sh
│   │   ├── 03_libstdc++.sh
│   │   ├── 04_ncurses.sh
│   │   ├── 05_xz.sh
│   │   ├── 06_gzip.sh
│   │   ├── 06_tar.sh
│   │   ├── 06_toybox.sh
│   │   ├── 07_readline.sh
│   │   ├── 08_bash.sh
│   │   ├── 09_binutils.sh
│   │   ├── 10_gcc.sh
│   │   ├── 11_make.sh
│   │   └── 12_kpkg.sh
│   ├── 02_phase2/packages.txt
│   ├── 03_phase3/packages.txt
│   ├── 04_phase4/packages.txt
│   ├── 05_kernel/00_kernel.sh          # shell, not packages.txt — board-selected
│   └── 06_packaging/
│       ├── 00_cleanup.sh
│       ├── 01_firmware.sh
│       ├── 02_rootfs.sh
│       └── 03_bootimg.sh
├── ports/mobile/                       # overlay repo, searched BEFORE ports/core
│   ├── linux-fajita/{kpkgbuild,build.sh,linux-base.config,linux.config}
│   ├── linux-qemu-aarch64/{kpkgbuild,build.sh,linux.config}
│   └── kdos-mobile-init/{kpkgbuild,build.sh,src/{kdos-usbnet,S30usbnet}}
├── board/
│   ├── fajita/
│   │   ├── deviceinfo                  # boot.img geometry, cmdline, dtb, partitions
│   │   └── fs/etc/inittab              # getty on ttyMSM0
│   └── qemu-aarch64/
│       ├── deviceinfo
│       └── fs/etc/inittab              # getty on ttyAMA0
├── fs-mobile/                          # rootfs overlay shared by both boards
│   └── etc/{os-release,hostname,fstab,init.d/S30usbnet}
├── testing/
│   ├── mobile-preflight.sh             # wiring gate for the mobile tree
│   └── mobile-bootimg.sh               # parses the produced boot.img header
├── build-mobile/                       # gitignored build root
└── docs/KDOS-MOBILE.md                 # the plan of record + hardware runbook
```

---

## Task 1: Scaffold the mobile phase tree and prove the orchestrator drives it

Nothing is built here. This task proves the orchestrator discovers a second phase tree, with the
right titles and chroot flags, and that the desktop target is unaffected.

**Files:**
- Create: `script-mobile/toolchain.env.sh`, `phase1.env.sh`, `phase2.env.sh`, `phase3.env.sh`,
  `phase4.env.sh`, `kernel.env.sh`, `packaging.env.sh`
- Create: `script-mobile/00_toolchain/.gitkeep`, `01_phase1/.gitkeep`, `02_phase2/.gitkeep`,
  `03_phase3/.gitkeep`, `04_phase4/.gitkeep`, `05_kernel/.gitkeep`, `06_packaging/.gitkeep`
- Create: `script-mobile/util/port.sh` (copy of `script/util/port.sh`, unmodified)
- Create: `script-mobile/kdosbuild.sh`
- Modify: `.gitignore`

**Interfaces:**
- Produces: the phase names `toolchain`, `phase1`, `phase2`, `phase3`, `phase4`, `kernel`,
  `packaging`; the build root `build-mobile/`; the shell variables `$KDOS_TARGET`, `$WORKSPACE`,
  `$BUILD_DIR`, `$SYSROOT`, `$CROSS_SYSROOT`, `$MARK` that every later phase script consumes.

- [ ] **Step 1: Create the toolchain phase env**

`script-mobile/toolchain.env.sh` — this is `script/toolchain.env.sh` with the target changed and
the build dir moved. Note `KDOS_SNAPSHOT_PATHS` is relative to `$BUILD_DIR`, and `MAKEFLAGS`
matches the host's core count.

```bash
#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro — mobile
# ---------------------------------

# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Cross Toolchain"
export KDOS_PHASE_DESC="cross binutils + gcc targeting aarch64-kdos-linux-musl"
export KDOS_SNAPSHOT_PATHS="cross fs mark"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

export KDOS_TARGET=aarch64-kdos-linux-musl
export KDOS_BOARD=${KDOS_BOARD:-fajita}

export WORKSPACE=/workspace
export BUILD_DIR=$WORKSPACE/build-mobile
export SYSROOT=$BUILD_DIR/fs
export CROSS_SYSROOT=$BUILD_DIR/cross
export MARK=$BUILD_DIR/mark/toolchain

mkdir -p $BUILD_DIR $SYSROOT $CROSS_SYSROOT $MARK
rm -rf $BUILD_DIR/tmp
mkdir -p $BUILD_DIR/tmp

export PKG_CONFIG_PATH=""
export PKG_CONFIG_LIBDIR=$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=$SYSROOT

export PATH=$CROSS_SYSROOT/bin:$CROSS_SYSROOT/usr/bin:$PATH

export CFLAGS="-O2 -pipe -std=gnu99"
export CXXFLAGS="-O2 -pipe"
export LDFLAGS=""

# Reproducible packages: same five lines as the desktop target, same reasons.
export SOURCE_DATE_EPOCH=1735689600
export TZ=UTC
export LC_ALL=C
export CFLAGS="$CFLAGS -ffile-prefix-map=/var/cache/kpkg/work=/build"
export CXXFLAGS="$CXXFLAGS -ffile-prefix-map=/var/cache/kpkg/work=/build"
export LDFLAGS="$LDFLAGS -Wl,--build-id=sha1"
export MAKEFLAGS="-j12"
```

- [ ] **Step 2: Create `phase1.env.sh`**

Identical to Step 1's file except for the metadata block and `MARK`. Copy the whole file, then
replace the metadata block and the `MARK` line with:

```bash
export KDOS_PHASE_TITLE="Base Userland"
export KDOS_PHASE_DESC="musl, toybox, bash, native gcc and kpkg, cross-compiled into the sysroot"
export KDOS_SNAPSHOT_PATHS="cross fs mark"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

export MARK=$BUILD_DIR/mark/phase1
```

- [ ] **Step 3: Create the three chroot phase envs**

`script-mobile/phase2.env.sh`. `CHROOT=1` is what makes the orchestrator run this phase through
`chroot_exec.sh`. Inside the chroot the repo is bind-mounted at `/kdos` and ports at `/ports`, so
`PORT_REPO` uses those paths — and `ports/mobile` comes first, which is what makes the overlay
work.

```bash
#!/bin/bash

# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro — mobile
# ---------------------------------

# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Self-Hosting Bootstrap"
export KDOS_PHASE_DESC="rebuild tar, musl, zlib, binutils and gcc inside the aarch64 chroot"
export KDOS_SNAPSHOT_PATHS="fs"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

export CHROOT=1

export PORT_REPO="/kdos/ports/mobile /ports/core"
export CFLAGS="-O2 -pipe"
export CXXFLAGS="-O2 -pipe"
export LDFLAGS=""
export SOURCE_DATE_EPOCH=1735689600
export TZ=UTC
export LC_ALL=C
export MAKEFLAGS="-j12"
```

`phase3.env.sh` and `phase4.env.sh` are the same file with only these two lines differing:

```bash
# phase3.env.sh
export KDOS_PHASE_TITLE="Toolchain & Core Libraries"
export KDOS_PHASE_DESC="build systems, interpreters, base libraries"

# phase4.env.sh
export KDOS_PHASE_TITLE="Userland"
export KDOS_PHASE_DESC="init, udev, networking, ssh — everything the phone needs to answer"
```

- [ ] **Step 4: Create `kernel.env.sh` and `packaging.env.sh`**

Both are **host-side** — no `CHROOT=1`. The kernel cross-compiles far faster on the host than it
would under emulation, and `mkbootimg.py` needs the host's python3.

```bash
# script-mobile/kernel.env.sh  (banner header as above)
export KDOS_PHASE_TITLE="Kernel"
export KDOS_PHASE_DESC="cross-build the board's kernel, DTB and modules"
export KDOS_SNAPSHOT_PATHS="fs mark"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

export KDOS_TARGET=aarch64-kdos-linux-musl
export KDOS_BOARD=${KDOS_BOARD:-fajita}
export WORKSPACE=/workspace
export BUILD_DIR=$WORKSPACE/build-mobile
export SYSROOT=$BUILD_DIR/fs
export CROSS_SYSROOT=$BUILD_DIR/cross
export MARK=$BUILD_DIR/mark/kernel
mkdir -p $MARK
export PATH=$CROSS_SYSROOT/bin:$CROSS_SYSROOT/usr/bin:$PATH
export SOURCE_DATE_EPOCH=1735689600
export TZ=UTC
export LC_ALL=C
export MAKEFLAGS="-j12"
```

```bash
# script-mobile/packaging.env.sh  (banner header as above)
export KDOS_PHASE_TITLE="Packaging"
export KDOS_PHASE_DESC="trim the rootfs, install firmware, roll rootfs.ext4 and boot.img"
export KDOS_SNAPSHOT_PATHS="fs images"
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* fs/var/cache/kpkg/work/* fs/dev/* fs/proc/* fs/sys/* fs/run/* fs/kdos/* fs/ports/*"

export KDOS_TARGET=aarch64-kdos-linux-musl
export KDOS_BOARD=${KDOS_BOARD:-fajita}
export WORKSPACE=/workspace
export BUILD_DIR=$WORKSPACE/build-mobile
export SYSROOT=$BUILD_DIR/fs
export IMAGES=$BUILD_DIR/images
export MARK=$BUILD_DIR/mark/packaging
mkdir -p $MARK $IMAGES
export SOURCE_DATE_EPOCH=1735689600
export TZ=UTC
export LC_ALL=C
```

- [ ] **Step 5: Create the phase directories**

`kbuild_discover` only accepts a directory whose name starts with digits followed by `_`
(`numeric_prefix`, `kb_phase.c:186-195`), and derives the env-file name from the part after the
first `_`. Create each with a `.gitkeep` so git carries it.

```bash
cd /home/kunaldawn/workspace/repos/kdos
for d in 00_toolchain 01_phase1 02_phase2 03_phase3 04_phase4 05_kernel 06_packaging; do
    mkdir -p "script-mobile/$d"
    touch "script-mobile/$d/.gitkeep"
done
mkdir -p script-mobile/util script-mobile/host
cp script/util/port.sh script-mobile/util/port.sh
```

- [ ] **Step 6: Create `script-mobile/kdosbuild.sh`**

Same compile line as `script/kdosbuild.sh`, different `--script-dir` / `--build-dir`.

```bash
#!/bin/bash
# (banner header)
#   script-mobile/kdosbuild.sh — compile the orchestrator, then run it for aarch64

set -e
cd "$(dirname "$0")/.."

OUT=${KDOSBUILD_BIN:-build-mobile/.kdosbuild}
mkdir -p "$(dirname "$OUT")"

${CC:-cc} -O2 -std=gnu11 -D_GNU_SOURCE -Wall -Wextra \
    -Isrc/libs/libkbase -Isrc/libs/libkbuild -Isrc/libs/libktui \
    -Isrc/libs/libkcolor -Isrc/build/kdosbuild \
    -o "$OUT" \
    src/build/kdosbuild/*.c \
    src/libs/libkbase/*.c src/libs/libkbuild/*.c \
    src/libs/libktui/*.c src/libs/libkcolor/*.c

exec "$OUT" --script-dir script-mobile --build-dir build-mobile "$@"
```

Then `chmod +x script-mobile/kdosbuild.sh`.

- [ ] **Step 7: Ignore the mobile build root**

Append to `.gitignore`:

```
build-mobile/
```

- [ ] **Step 8: Verify the orchestrator discovers the tree**

`--plain` forces plain lines instead of the TUI; a non-tty stdout implies it anyway.

```bash
cd /home/kunaldawn/workspace/repos/kdos
bash script-mobile/kdosbuild.sh --list --plain
```

Expected: it compiles `build-mobile/.kdosbuild` and exits 0 reporting no snapshots. It must not
error on phase discovery.

- [ ] **Step 9: Verify phase titles and chroot flags came from the mobile env files**

```bash
cd /home/kunaldawn/workspace/repos/kdos
build-mobile/.kdosbuild --script-dir script-mobile --build-dir build-mobile --json --list
```

Expected: valid JSON, exit 0.

If the orchestrator offers no direct phase dump, assert the parse the same way `kbuild_discover`
does — the env file must be found by convention:

```bash
for p in toolchain phase1 phase2 phase3 phase4 kernel packaging; do
    test -f "script-mobile/$p.env.sh" || { echo "FAIL: missing $p.env.sh"; exit 1; }
done
grep -q 'CHROOT=1' script-mobile/phase2.env.sh || { echo "FAIL: phase2 not chroot"; exit 1; }
grep -q 'CHROOT=1' script-mobile/phase3.env.sh || { echo "FAIL: phase3 not chroot"; exit 1; }
grep -q 'CHROOT=1' script-mobile/phase4.env.sh || { echo "FAIL: phase4 not chroot"; exit 1; }
grep -q 'CHROOT=1' script-mobile/kernel.env.sh    && { echo "FAIL: kernel must be host-side"; exit 1; }
grep -q 'CHROOT=1' script-mobile/packaging.env.sh && { echo "FAIL: packaging must be host-side"; exit 1; }
echo "PASS: mobile phase tree"
```

- [ ] **Step 10: Verify the desktop target still resolves**

This must pass unchanged after every task in this plan.

```bash
cd /home/kunaldawn/workspace/repos/kdos
bash testing/preflight.sh
```

Expected: PASS, exactly as before this task.

**Checkpoint:** `script-mobile/kdosbuild.sh --list --plain` exits 0, the seven env files assert
clean, and `testing/preflight.sh` still passes. Stop for the user to commit.

---

## Task 2: Retarget the cross toolchain to aarch64

Finding 7: both toolchain scripts are already arch-neutral and flow from `$KDOS_TARGET`. The only
edits are the `source` line (it must pick up the *mobile* env) and, for GCC, the arch flags.

**Files:**
- Create: `script-mobile/00_toolchain/00_binutils.sh` (from `script/00_toolchain/00_binutils.sh`)
- Create: `script-mobile/00_toolchain/01_gcc.sh` (from `script/00_toolchain/01_gcc.sh`)

**Interfaces:**
- Consumes: `$KDOS_TARGET`, `$CROSS_SYSROOT`, `$SYSROOT`, `$MARK` from `toolchain.env.sh`;
  `extract_port_source` from `script-mobile/util/port.sh`.
- Produces: `build-mobile/cross/bin/aarch64-kdos-linux-musl-{gcc,cc,ld,as,ar}` on `$PATH` for
  every later phase.

- [ ] **Step 1: Copy both scripts and repoint their `source` lines**

The originals begin `source script/toolchain.env.sh` — a repo-root-relative path that would pull
in the **desktop** target. This is the single most important edit in the task.

```bash
cd /home/kunaldawn/workspace/repos/kdos
cp script/00_toolchain/00_binutils.sh script-mobile/00_toolchain/00_binutils.sh
cp script/00_toolchain/01_gcc.sh      script-mobile/00_toolchain/01_gcc.sh
```

Then in **both** copies replace:

```bash
source script/toolchain.env.sh
source script/util/port.sh
```

with:

```bash
source script-mobile/toolchain.env.sh
source script-mobile/util/port.sh
```

- [ ] **Step 2: Add the aarch64 arch flags to GCC's configure**

In `script-mobile/00_toolchain/01_gcc.sh`, add these two flags to the `../configure` invocation,
immediately after `--disable-multilib`. `armv8-a` is the SDM845's baseline and is also what QEMU
`-M virt` accepts, so one toolchain serves both boards.

```
    --with-arch=armv8-a \
    --with-abi=lp64 \
```

Leave every other flag exactly as the desktop's — `--without-headers --with-newlib
--disable-shared --disable-threads` and the rest are the standard stage-1 cross-compiler shape
and are arch-independent.

- [ ] **Step 3: Confirm no x86 assumption survived either copy**

```bash
cd /home/kunaldawn/workspace/repos/kdos
grep -n 'x86\|i386\|amd64' script-mobile/00_toolchain/*.sh && { echo "FAIL: x86 leaked in"; exit 1; }
grep -q 'script-mobile/toolchain.env.sh' script-mobile/00_toolchain/00_binutils.sh || { echo "FAIL: binutils sources the wrong env"; exit 1; }
grep -q 'script-mobile/toolchain.env.sh' script-mobile/00_toolchain/01_gcc.sh || { echo "FAIL: gcc sources the wrong env"; exit 1; }
bash -n script-mobile/00_toolchain/00_binutils.sh && bash -n script-mobile/00_toolchain/01_gcc.sh
echo "PASS: toolchain scripts"
```

- [ ] **Step 4: Build the toolchain phase**

This is the first real build and takes roughly 20–40 minutes. `--phases toolchain` narrows the
plan, which suppresses snapshot writes by design — that is expected and correct.

```bash
cd /home/kunaldawn/workspace/repos/kdos
make build-mobile BUILD_ARGS="--fresh --phases toolchain --plain" 2>&1 | tee /tmp/mobile-toolchain.log
```

> `make build-mobile` does not exist until Task 10. Until then, run it directly with the same
> container invocation the desktop uses — see Task 10 Step 2 for the exact `docker run` line, or
> run `bash script-mobile/kdosbuild.sh --fresh --phases toolchain --plain` on the host if the
> host has `gcc`, `bison`, `flex` and `texinfo`.

Expected: exit 0, `>>> Building Binutils...` then `>>> Building GCC...`, no errors.

- [ ] **Step 5: Verify the compiler exists and emits aarch64**

```bash
cd /home/kunaldawn/workspace/repos/kdos
export PATH=$PWD/build-mobile/cross/bin:$PATH
aarch64-kdos-linux-musl-gcc --version | head -1
echo 'int main(void){return 0;}' > /tmp/t.c
aarch64-kdos-linux-musl-gcc -c /tmp/t.c -o /tmp/t.o
file /tmp/t.o
```

Expected: `file` reports `ELF 64-bit LSB relocatable, ARM aarch64`. Anything reporting `x86-64`
means the env file was not the mobile one — go back to Step 1.

**Checkpoint:** `file /tmp/t.o` says `ARM aarch64`, and `bash testing/preflight.sh` still passes.
Stop for the user to commit.

---

## Task 3: Cross-build the base userland into the aarch64 sysroot

Phase 1 is the 15 scripts that populate `build-mobile/fs` far enough to chroot into it. They are
copied rather than symlinked: these are the arch-sensitive bootstrap scripts, and the desktop's
copies are frozen and working.

`13_kinstall.sh` is deliberately **not** copied — the phone is flashed, not installed.

**Files:**
- Create: `script-mobile/01_phase1/{00_file_system,01_linux_headers,02_musl_libc,03_libstdc++,04_ncurses,05_xz,06_gzip,06_tar,06_toybox,07_readline,08_bash,09_binutils,10_gcc,11_make,12_kpkg}.sh`

**Interfaces:**
- Consumes: the cross toolchain on `$PATH` from Task 2; `$SYSROOT`, `$MARK` from `phase1.env.sh`.
- Produces: a populated `build-mobile/fs` containing `/bin/bash`, `/bin/toybox`, `/usr/bin/kpkg`
  and a native aarch64 `gcc` — everything `chroot_exec.sh` needs in Task 4.

- [ ] **Step 1: Copy the fifteen scripts and repoint their `source` lines**

```bash
cd /home/kunaldawn/workspace/repos/kdos
for f in 00_file_system 01_linux_headers 02_musl_libc 03_libstdc++ 04_ncurses \
         05_xz 06_gzip 06_tar 06_toybox 07_readline 08_bash 09_binutils \
         10_gcc 11_make 12_kpkg; do
    cp "script/01_phase1/$f.sh" "script-mobile/01_phase1/$f.sh"
done
```

In every copy replace `source script/phase1.env.sh` with `source script-mobile/phase1.env.sh`,
and `source script/util/port.sh` with `source script-mobile/util/port.sh`.

- [ ] **Step 2: Fix the one hardcoded architecture — the kernel headers**

`script-mobile/01_phase1/01_linux_headers.sh` contains `make ARCH=x86_64 headers_install ...`.
Change `ARCH=x86_64` to `ARCH=arm64`. The kernel's arch name is `arm64`, **not** `aarch64` — a
wrong value here fails with `Makefile: arch/aarch64: No such file or directory`.

- [ ] **Step 3: Verify no other x86 assumption survived**

```bash
cd /home/kunaldawn/workspace/repos/kdos
grep -rn 'x86\|i386\|amd64' script-mobile/01_phase1/ && { echo "FAIL: x86 leaked in"; exit 1; }
grep -rLn 'script-mobile/phase1.env.sh' script-mobile/01_phase1/*.sh && { echo "FAIL: a script sources the wrong env"; exit 1; }
for f in script-mobile/01_phase1/*.sh; do bash -n "$f" || exit 1; done
test ! -f script-mobile/01_phase1/13_kinstall.sh || { echo "FAIL: kinstall must not be in the mobile tree"; exit 1; }
echo "PASS: phase1 scripts"
```

- [ ] **Step 4: Build phase 1**

Roughly 40–70 minutes; the native aarch64 GCC in `10_gcc.sh` dominates.

```bash
cd /home/kunaldawn/workspace/repos/kdos
make build-mobile BUILD_ARGS="--continue-from phase1 --phases phase1 --plain" 2>&1 | tee /tmp/mobile-phase1.log
```

Expected: exit 0.

- [ ] **Step 5: Verify the sysroot is aarch64 and self-sufficient**

```bash
cd /home/kunaldawn/workspace/repos/kdos
for b in bin/bash bin/toybox usr/bin/kpkg lib/libc.so; do
    test -e "build-mobile/fs/$b" || { echo "FAIL: missing $b"; exit 1; }
done
file build-mobile/fs/bin/bash | grep -q 'ARM aarch64' || { echo "FAIL: bash is not aarch64"; exit 1; }
file build-mobile/fs/usr/bin/kpkg | grep -q 'ARM aarch64' || { echo "FAIL: kpkg is not aarch64"; exit 1; }
echo "PASS: aarch64 sysroot"
```

- [ ] **Step 6: Verify an aarch64 binary from the sysroot actually runs on this host**

This is finding 8 applied to a binary we built ourselves, and it is the gate for Task 4.

```bash
cd /home/kunaldawn/workspace/repos/kdos
build-mobile/fs/bin/toybox uname -m
```

Expected: `aarch64`. If instead you get `Exec format error`, binfmt_misc is not reaching this
context — stop and resolve that before Task 4 (see Task 4 Step 1).

**Checkpoint:** `file build-mobile/fs/bin/bash` says `ARM aarch64`, that binary runs and prints
`aarch64`, and `bash testing/preflight.sh` still passes. Stop for the user to commit.

---

## Task 4: Bring up the qemu-user chroot

The mobile `chroot_exec.sh` differs from the desktop's in exactly three ways: the chroot
directory is `build-mobile/fs`, it refuses to run if binfmt cannot execute an aarch64 binary, and
it passes `KDOS_BOARD` through. Everything else — the unmount discipline, the diagnostics-to-a-
logfile rule, the `env -i` isolation — is copied verbatim because each of those is a bug that
already cost a debug cycle on the desktop side.

**Files:**
- Create: `script-mobile/chroot_exec.sh` (from `script/chroot_exec.sh`)

**Interfaces:**
- Consumes: `build-mobile/fs` from Task 3.
- Produces: the ability for phases 2–4 to run `kpkg` inside the target rootfs. The orchestrator
  finds this file at `<script_dir>/chroot_exec.sh` (finding 3) — the name and location are fixed.

- [ ] **Step 1: Confirm binfmt_misc is registered with the `F` flag**

Run on the **host**, before anything else. The `F` flag is what makes the interpreter survive
into a chroot that contains no qemu binary.

```bash
test -r /proc/sys/fs/binfmt_misc/qemu-aarch64 || {
    echo "FAIL: no qemu-aarch64 binfmt handler."
    echo "      Install it:  sudo apt install qemu-user-static binfmt-support"
    exit 1
}
grep -q '^enabled' /proc/sys/fs/binfmt_misc/qemu-aarch64 || { echo "FAIL: handler disabled"; exit 1; }
grep -q '^flags:.*F' /proc/sys/fs/binfmt_misc/qemu-aarch64 || {
    echo "FAIL: handler lacks the F flag — it will not work inside a chroot."
    echo "      Re-register with 'F' (debian: sudo systemctl restart systemd-binfmt, or"
    echo "      docker run --rm --privileged multiarch/qemu-user-static --reset -p yes)"
    exit 1
}
echo "PASS: binfmt aarch64 with F flag"
```

- [ ] **Step 2: Copy `chroot_exec.sh` and repoint the chroot directory**

```bash
cd /home/kunaldawn/workspace/repos/kdos
cp script/chroot_exec.sh script-mobile/chroot_exec.sh
```

In the copy, change:

```bash
CHROOT_DIR="$REPO_ROOT/build/fs"
```

to:

```bash
CHROOT_DIR="$REPO_ROOT/build-mobile/fs"
```

and change the two `build/logs` references to `build-mobile/logs`:

```bash
MOUNT_LOG="$REPO_ROOT/build-mobile/logs/chroot.log"
```

- [ ] **Step 3: Add the binfmt guard**

Insert immediately after the existing `if [ ! -d "$CHROOT_DIR" ]` check. Without this, a missing
handler surfaces as `chroot: failed to run command '/bin/bash': Exec format error`, which reads
like a broken rootfs rather than a missing host package.

```bash
# The rootfs is aarch64 and the host is not. Without a binfmt_misc handler
# registered with the F flag, chroot(2) succeeds and the first exec fails with
# "Exec format error" — which looks like a corrupt sysroot and is not. The F
# flag matters specifically: it pins the interpreter at registration time, so
# no qemu binary has to exist inside the chroot.
if [ ! -r /proc/sys/fs/binfmt_misc/qemu-aarch64 ] ||
   ! grep -q '^enabled' /proc/sys/fs/binfmt_misc/qemu-aarch64 ||
   ! grep -q '^flags:.*F' /proc/sys/fs/binfmt_misc/qemu-aarch64; then
    echo "Error: no usable qemu-aarch64 binfmt_misc handler (need one registered with the F flag)." >&2
    echo "       Install qemu-user-static + binfmt-support on the HOST, then retry." >&2
    exit 1
fi
```

- [ ] **Step 4: Forward `KDOS_BOARD` into the chroot**

The final `chroot ... /usr/bin/env -i` invocation clears the environment. Add `KDOS_BOARD` beside
the existing `KDOS_REPLAY` line so board-conditional recipes can see it:

```bash
    KDOS_BOARD="${KDOS_BOARD:-fajita}" \
```

- [ ] **Step 5: Verify the chroot works**

```bash
cd /home/kunaldawn/workspace/repos/kdos
bash -n script-mobile/chroot_exec.sh || exit 1
sudo bash script-mobile/chroot_exec.sh /bin/bash -c 'uname -m; echo $KDOS_BOARD'
```

Expected:

```
aarch64
fajita
```

- [ ] **Step 6: Verify `kpkg` runs inside the chroot and sees the overlay**

`kpkgdepends` prints one bare space-separated line and nothing else — that contract is what the
orchestrator parses, so a stray diagnostic here becomes a bogus package name.

```bash
cd /home/kunaldawn/workspace/repos/kdos
sudo KDOS_BOARD=fajita bash script-mobile/chroot_exec.sh \
    /bin/bash -c 'PORT_REPO="/kdos/ports/mobile /ports/core" kpkgdepends zlib'
```

Expected: a single line naming `zlib` and its dependencies, nothing else on stdout.

- [ ] **Step 7: Verify the mounts were cleaned up**

A leaked bind mount makes the next phase snapshot refuse to archive the tree.

```bash
cut -d' ' -f2 /proc/self/mounts | grep "$PWD/build-mobile/fs" && { echo "FAIL: leaked mounts"; exit 1; }
echo "PASS: chroot clean"
```

**Checkpoint:** `uname -m` inside the chroot says `aarch64`, `kpkgdepends zlib` prints one clean
line, no mounts leak, and `bash testing/preflight.sh` still passes. Stop for the user to commit.

---

## Task 5: Define the mobile package set for phases 2–4

Three text files. This is where the mobile target's smallness is decided: no Wayland, no mesa, no
desktop, no appbox. Every name must resolve in `ports/mobile` or `ports/core`.

**Files:**
- Create: `script-mobile/02_phase2/packages.txt`
- Create: `script-mobile/03_phase3/packages.txt`
- Create: `script-mobile/04_phase4/packages.txt`
- Delete: the three `.gitkeep` files those directories carried from Task 1

**Interfaces:**
- Consumes: the chroot from Task 4; `PORT_REPO` from the phase env files.
- Produces: a rootfs with a shell, `udev`, `ip`, `sshd` and an ext4-capable userland — the
  functional target of this arc.

- [ ] **Step 1: Write phase 2 — the self-hosting bootstrap**

Copy the desktop's list verbatim; it is the standard LFS rebuild-inside-the-chroot set and has no
arch content.

```bash
cd /home/kunaldawn/workspace/repos/kdos
cp script/02_phase2/packages.txt script-mobile/02_phase2/packages.txt
rm -f script-mobile/02_phase2/.gitkeep
```

- [ ] **Step 2: Write phase 3 — core libraries and build systems**

`script-mobile/03_phase3/packages.txt`, banner header then:

```
zlib
xz
zstd
bzip2
openssl
ncurses
readline
libffi
expat
pcre2
python3
pkgconf
autoconf
automake
libtool
m4
gperf
bison
flex
perl
patch
findutils
diffutils
gawk
grep
sed
which
file
```

Before running anything, confirm every name resolves — several of these may live under a
different port name in `ports/core`. Step 5 is that check, and it is cheap; run it first.

- [ ] **Step 3: Write phase 4 — the userland that makes the phone answer**

`script-mobile/04_phase4/packages.txt`, banner header then:

```
util-linux
e2fsprogs
kmod
eudev
iproute2
iputils
openssh
shadow
linux-pam
libcap
libcap-ng
tzdata
kdos-mobile-init
```

`kdos-mobile-init` is created in Task 8. Until then it will fail to resolve — that is expected,
and Step 5 will say so by name.

- [ ] **Step 4: Remove the remaining placeholders**

```bash
cd /home/kunaldawn/workspace/repos/kdos
rm -f script-mobile/03_phase3/.gitkeep script-mobile/04_phase4/.gitkeep
```

- [ ] **Step 5: Verify every package name resolves**

This is the check `testing/preflight.sh` performs for the desktop, applied to the mobile tree.
Run it on the host with the on-demand `kpkg` that `ports/fetch` already knows how to build.

```bash
cd /home/kunaldawn/workspace/repos/kdos
KPKG=ports/.kpkgbin/kpkg
if [ ! -x "$KPKG" ]; then
    mkdir -p "$(dirname "$KPKG")"
    ${CC:-cc} -O2 -std=gnu11 -D_GNU_SOURCE \
        -Isrc/libs/libkbase -Isrc/libs/libkpkg -Isrc/packages/kdos-kpkg \
        -o "$KPKG" src/packages/kdos-kpkg/*.c \
        src/libs/libkbase/*.c src/libs/libkpkg/*.c
fi

fail=0
for f in script-mobile/0[234]_*/packages.txt; do
    while read -r pkg; do
        case "$pkg" in ''|\#*) continue;; esac
        PORT_REPO="$PWD/ports/mobile $PWD/ports/core" "$KPKG" meta "$pkg" >/dev/null 2>&1 \
            || { echo "MISSING: $pkg (in $f)"; fail=1; }
    done < "$f"
done
[ "$fail" -eq 0 ] && echo "PASS: all mobile packages resolve"
```

Expected at this point: everything resolves **except** `kdos-mobile-init`. Fix any other name by
looking up the real port (`ls ports/core | grep <thing>`) and correcting the list — do not invent
a port to satisfy the check.

- [ ] **Step 6: Verify each list produces a clean dependency order**

`kpkgdepends` output is parsed by the orchestrator and validated against
`^[A-Za-z0-9][A-Za-z0-9._+-]*$`, so noise here fails the build loudly later.

```bash
cd /home/kunaldawn/workspace/repos/kdos
for f in script-mobile/02_phase2/packages.txt script-mobile/03_phase3/packages.txt; do
    pkgs=$(grep -v '^#' "$f" | grep -v '^$' | tr '\n' ' ')
    out=$(PORT_REPO="$PWD/ports/mobile $PWD/ports/core" ports/.kpkgbin/kpkg 2>/dev/null; true)
    order=$(PORT_REPO="$PWD/ports/mobile $PWD/ports/core" \
        sh -c "exec -a kpkgdepends ports/.kpkgbin/kpkg $pkgs" 2>/dev/null)
    test -n "$order" || { echo "FAIL: empty order for $f"; exit 1; }
    echo "$order" | tr ' ' '\n' | grep -vE '^[A-Za-z0-9][A-Za-z0-9._+-]*$' | grep -q . \
        && { echo "FAIL: bad token in order for $f"; exit 1; }
done
echo "PASS: dependency orders clean"
```

> `kpkg` dispatches on `argv[0]`'s basename. If the `exec -a` form above misbehaves, symlink it
> instead: `ln -sf kpkg ports/.kpkgbin/kpkgdepends` and call that.

**Checkpoint:** every package except `kdos-mobile-init` resolves, both dependency orders are
non-empty and clean, and `bash testing/preflight.sh` still passes. Stop for the user to commit.

---

## Task 6: The QEMU aarch64 board — first boot to a login prompt

The QEMU board comes before the phone deliberately: it exercises phases 00–05 and packaging with
a two-second boot loop and no hardware, and it is the M0 parity milestone. Both boards share the
same triplet, so phases 00–04 and their snapshots are reused unchanged.

**Files:**
- Create: `board/qemu-aarch64/deviceinfo`
- Create: `board/qemu-aarch64/fs/etc/inittab`
- Create: `ports/mobile/linux-qemu-aarch64/{kpkgbuild,build.sh,linux.config}`
- Create: `script-mobile/05_kernel/00_kernel.sh`
- Create: `fs-mobile/etc/{os-release,hostname,fstab}`
- Create: `script-mobile/06_packaging/{00_cleanup.sh,02_rootfs.sh}`
- Delete: `script-mobile/05_kernel/.gitkeep`, `script-mobile/06_packaging/.gitkeep`

**Interfaces:**
- Consumes: the rootfs from Task 5; the cross toolchain from Task 2.
- Produces: `board/<board>/deviceinfo` as the board contract — a `KEY=value` file that Task 7 and
  Task 9 both read; `build-mobile/images/rootfs.ext4`; `build-mobile/images/Image.gz`.

- [ ] **Step 1: Write the board contract**

`board/qemu-aarch64/deviceinfo`. This file is **parsed, never sourced** — same discipline as the
phase env metadata, and for the same reason.

```sh
# (banner header)
# KDOS mobile — board contract for QEMU aarch64 (-M virt).
# Parsed as KEY=value. Values are literal; no expansion, no command substitution.

board=qemu-aarch64
arch=arm64
kernel_port=linux-qemu-aarch64
kernel_image=Image.gz
dtb=
console=ttyAMA0
baudrate=115200
cmdline=console=ttyAMA0,115200 root=/dev/vda rw rootwait
rootfs_size=4G
rootfs_label=kdos-root
bootimg=no
```

- [ ] **Step 2: Write the board inittab**

`board/qemu-aarch64/fs/etc/inittab`. Note there is no `kdos-getty` and no autologin — those are
desktop concerns. Root login on the console is what this arc needs.

```
# (banner header)
::sysinit:/etc/init.d/rcS

# QEMU virt exposes a PL011 UART as ttyAMA0. This is the only console.
ttyAMA0::respawn:/sbin/getty -L ttyAMA0 115200 vt100

::restart:/sbin/init
::ctrlaltdel:/sbin/reboot
::shutdown:/sbin/swapoff -a
::shutdown:/bin/umount -a -r
```

- [ ] **Step 3: Write the QEMU kernel port**

`ports/mobile/linux-qemu-aarch64/kpkgbuild`. Reuse the version and checksum already pinned by
`ports/core/linux` so no new tarball is fetched — read them out of that file rather than copying
the values from here:

```bash
grep -E '^(version|sha256|source)' ports/core/linux/kpkgbuild
```

Then write, substituting the version and sha256 you just read:

```
# (banner header)

name        = linux-qemu-aarch64
version     = <the version from ports/core/linux>
release     = 1
source      = https://cdn.kernel.org/pub/linux/kernel/v${version:0:1}.x/linux-$version.tar.xz
sha256      = <the sha256 from ports/core/linux>
description = Linux kernel for the QEMU aarch64 virt machine
secdb       = linux-lts
```

> `source` names `linux-$version.tar.xz` explicitly rather than `$name-$version` because the
> upstream tarball is called `linux-`, not `linux-qemu-aarch64-`. The first `source` gets
> `--strip-components=1`, so the extracted directory name does not matter.

`ports/mobile/linux-qemu-aarch64/build.sh`:

```bash
#!/bin/bash
# (banner header)

export ARCH=arm64
export CROSS_COMPILE=aarch64-kdos-linux-musl-

make mrproper
make defconfig

cat $PORT_SRC/linux.config >> .config
make olddefconfig

make HOSTCFLAGS="-D__attribute_const__= -D__always_inline=inline" Image.gz modules
make INSTALL_MOD_PATH=$PKG DEPMOD=/bin/true modules_install

rm -f "$PKG/lib/modules/$version/build"
rm -f "$PKG/lib/modules/$version/source"

mkdir -p $PKG/boot
cp arch/arm64/boot/Image.gz $PKG/boot/Image.gz
cp System.map $PKG/boot/System.map-$version
```

`ports/mobile/linux-qemu-aarch64/linux.config`:

```
# Serial console on the PL011 UART that -M virt provides.
CONFIG_SERIAL_AMBA_PL011=y
CONFIG_SERIAL_AMBA_PL011_CONSOLE=y

# Root on a virtio-blk disk, no initramfs.
CONFIG_VIRTIO=y
CONFIG_VIRTIO_PCI=y
CONFIG_VIRTIO_MMIO=y
CONFIG_VIRTIO_BLK=y
CONFIG_VIRTIO_NET=y
CONFIG_EXT4_FS=y
CONFIG_DEVTMPFS=y
CONFIG_DEVTMPFS_MOUNT=y

# Not signing modules; nothing here has keys.
# CONFIG_MODULE_SIG is not set
# CONFIG_EFI_ZBOOT is not set
```

- [ ] **Step 4: Write the kernel phase**

`script-mobile/05_kernel/00_kernel.sh` — a shell step rather than a `packages.txt` precisely so
the board can select which kernel port is built.

```bash
#!/bin/bash
# (banner header)
#
# The kernel is cross-compiled on the host, not in the chroot: it is the one
# large build that gains nothing from running under emulation, and mkbootimg
# in the packaging phase needs the host's python3 anyway.

set -e
source script-mobile/kernel.env.sh
source script-mobile/util/port.sh

BOARD_DIR="$WORKSPACE/board/$KDOS_BOARD"
test -f "$BOARD_DIR/deviceinfo" || { echo "ERROR: no deviceinfo for board '$KDOS_BOARD'"; exit 1; }

# deviceinfo is PARSED, never sourced.
kernel_port=$(sed -n 's/^kernel_port=//p' "$BOARD_DIR/deviceinfo")
test -n "$kernel_port" || { echo "ERROR: deviceinfo declares no kernel_port"; exit 1; }

if [ -f "$MARK/$kernel_port" ] && [ "${KDOS_REPLAY:-0}" != "1" ]; then
    exit 0
fi

echo ">>> Building kernel port $kernel_port for board $KDOS_BOARD..."

PORT_REPO="$WORKSPACE/ports/mobile $WORKSPACE/ports/core" \
KPKG_ROOT="$SYSROOT" \
    kpkg install -f "$kernel_port"

touch "$MARK/$kernel_port"
```

> If the host has no `kpkg` on `$PATH` at this point, build it the way `ports/fetch` does — the
> same six-line `cc` invocation used in Task 5 Step 5 — and call that binary instead.

- [ ] **Step 5: Write the shared rootfs overlay**

`fs-mobile/etc/os-release` — the identity of this variant. `NAME` stays KDOS; this is a variant,
not a different distro.

```
NAME="KDOS"
PRETTY_NAME="KDOS mobile"
ID=kdos
VARIANT="mobile"
VARIANT_ID=mobile
HOME_URL="https://github.com/kunaldawn/kdos"
```

`fs-mobile/etc/hostname`:

```
kdos-mobile
```

`fs-mobile/etc/fstab` — root is already mounted by the kernel; these are the pseudo-filesystems.

```
proc      /proc      proc     defaults          0 0
sysfs     /sys       sysfs    defaults          0 0
devtmpfs  /dev       devtmpfs defaults          0 0
devpts    /dev/pts   devpts   gid=5,mode=620    0 0
tmpfs     /dev/shm   tmpfs    mode=1777         0 0
tmpfs     /tmp       tmpfs    mode=1777         0 0
tmpfs     /run       tmpfs    mode=0755         0 0
configfs  /sys/kernel/config configfs defaults  0 0
```

- [ ] **Step 6: Write the rootfs packaging steps**

`script-mobile/06_packaging/00_cleanup.sh`:

```bash
#!/bin/bash
# (banner header)
set -e
source script-mobile/packaging.env.sh

echo ">>> Trimming the rootfs and applying overlays..."

# Shared overlay first, then the board's, so a board can override any file.
cp -a "$WORKSPACE/fs-mobile/." "$SYSROOT/"
if [ -d "$WORKSPACE/board/$KDOS_BOARD/fs" ]; then
    cp -a "$WORKSPACE/board/$KDOS_BOARD/fs/." "$SYSROOT/"
fi

# Build-time scaffolding that must not ship.
rm -rf "$SYSROOT/var/cache/kpkg/work"/*
rm -rf "$SYSROOT/tmp"/*
rm -rf "$SYSROOT/usr/share/man" "$SYSROOT/usr/share/doc" "$SYSROOT/usr/share/info"

# The chroot's bind-mount points, empty but present.
mkdir -p "$SYSROOT"/{dev,proc,sys,run,tmp}
rmdir "$SYSROOT/kdos" "$SYSROOT/ports" 2>/dev/null || true

echo ">>> Rootfs trimmed."
```

`script-mobile/06_packaging/02_rootfs.sh`:

```bash
#!/bin/bash
# (banner header)
#
# mkfs.ext4 with -d builds the image from a directory without needing root or
# a loop mount, which is what lets this run in an unprivileged container.
set -e
source script-mobile/packaging.env.sh

BOARD_DIR="$WORKSPACE/board/$KDOS_BOARD"
size=$(sed -n 's/^rootfs_size=//p' "$BOARD_DIR/deviceinfo")
label=$(sed -n 's/^rootfs_label=//p' "$BOARD_DIR/deviceinfo")

OUT="$IMAGES/rootfs.ext4"
rm -f "$OUT"

echo ">>> Building $OUT ($size, label $label)..."
truncate -s "$size" "$OUT"
mkfs.ext4 -F -L "$label" -d "$SYSROOT" -U random "$OUT"

echo ">>> Wrote $OUT ($(stat -c%s "$OUT") bytes)"
```

Then remove the placeholders:

```bash
cd /home/kunaldawn/workspace/repos/kdos
rm -f script-mobile/05_kernel/.gitkeep script-mobile/06_packaging/.gitkeep
chmod +x script-mobile/05_kernel/00_kernel.sh script-mobile/06_packaging/*.sh
```

- [ ] **Step 7: Build the QEMU board end to end**

```bash
cd /home/kunaldawn/workspace/repos/kdos
KDOS_BOARD=qemu-aarch64 make build-mobile BUILD_ARGS="--plain" 2>&1 | tee /tmp/mobile-qemu.log
```

Expected: exit 0, and `build-mobile/images/` containing `rootfs.ext4` and the kernel installed
into `build-mobile/fs/boot/Image.gz`.

- [ ] **Step 8: Verify the artifacts**

```bash
cd /home/kunaldawn/workspace/repos/kdos
test -f build-mobile/images/rootfs.ext4 || { echo "FAIL: no rootfs"; exit 1; }
test -f build-mobile/fs/boot/Image.gz   || { echo "FAIL: no kernel"; exit 1; }
file build-mobile/fs/boot/Image.gz | grep -q gzip || { echo "FAIL: kernel not gzip"; exit 1; }
/sbin/tune2fs -l build-mobile/images/rootfs.ext4 | grep -q 'kdos-root' || { echo "FAIL: bad label"; exit 1; }
echo "PASS: qemu artifacts"
```

- [ ] **Step 9: Boot it and reach a login prompt**

This is the milestone. Write `testing/mobile-qemu-boot.sh`:

```bash
#!/usr/bin/env bash
# (banner header)
# Boots the qemu-aarch64 board headless and waits for a login prompt.
set -euo pipefail
cd "$(dirname "$0")/.."

KERNEL=build-mobile/fs/boot/Image.gz
ROOTFS=build-mobile/images/rootfs.ext4
for f in "$KERNEL" "$ROOTFS"; do
    [ -f "$f" ] || { echo "FAIL: $f missing — run: KDOS_BOARD=qemu-aarch64 make build-mobile"; exit 1; }
done

LOG=$(mktemp)
trap 'rm -f "$LOG"' EXIT

timeout 180 qemu-system-aarch64 \
    -M virt -cpu cortex-a72 -smp 2 -m 1G -nographic \
    -kernel "$KERNEL" \
    -drive file="$ROOTFS",format=raw,if=virtio \
    -append "console=ttyAMA0,115200 root=/dev/vda rw rootwait" \
    > "$LOG" 2>&1 || true

if grep -qi 'login:' "$LOG"; then
    echo "PASS: qemu boot reached a login prompt"
    exit 0
fi

echo "FAIL: no login prompt. Last 40 lines:"
tail -40 "$LOG"
exit 1
```

Then:

```bash
chmod +x testing/mobile-qemu-boot.sh
bash testing/mobile-qemu-boot.sh
```

Expected: `PASS: qemu boot reached a login prompt`.

**Checkpoint:** `testing/mobile-qemu-boot.sh` passes — an aarch64 KDOS built entirely by
`kdosbuild` and `kpkg` boots to a login prompt. `bash testing/preflight.sh` still passes. Stop
for the user to commit.

---

## Task 7: The fajita kernel port

**Files:**
- Create: `board/fajita/deviceinfo`
- Create: `board/fajita/fs/etc/inittab`
- Create: `ports/mobile/linux-fajita/{kpkgbuild,build.sh,linux.config}`
- Copy in: `ports/mobile/linux-fajita/linux-base.config` (9167 lines, from the archived kmobile
  repo at `board/kmobile/fajita/linux-base.config`)

**Interfaces:**
- Consumes: the kernel phase from Task 6 Step 4 unchanged — it reads `kernel_port` from
  `deviceinfo`, so adding a board adds no code.
- Produces: `build-mobile/fs/boot/Image.gz` and `build-mobile/fs/boot/dtbs/qcom/sdm845-oneplus-fajita.dtb`
  for Task 9's boot image.

- [ ] **Step 1: Write the board contract**

`board/fajita/deviceinfo`. Every value here is from Global Constraints and traceable to pmaports.

```sh
# (banner header)
# KDOS mobile — board contract for the OnePlus 6T (fajita, Qualcomm SDM845).
# Parsed as KEY=value. Values are literal; no expansion, no command substitution.
# Sources: pmaports device-oneplus-fajita/deviceinfo and the sdm845-mainline tree.

board=fajita
arch=arm64
kernel_port=linux-fajita
kernel_image=Image.gz
dtb=qcom/sdm845-oneplus-fajita.dtb
console=ttyMSM0
baudrate=115200
cmdline=console=ttyMSM0,115200 root=PARTLABEL=userdata rw rootwait
rootfs_size=4G
rootfs_label=kdos-root
bootimg=yes
bootimg_base=0x00000000
bootimg_kernel_offset=0x00008000
bootimg_ramdisk_offset=0x01000000
bootimg_second_offset=0x00f00000
bootimg_tags_offset=0x00000100
bootimg_pagesize=4096
bootimg_header_version=0
flash_boot_partition=boot
flash_rootfs_partition=userdata
```

- [ ] **Step 2: Write the board inittab**

`board/fajita/fs/etc/inittab`:

```
# (banner header)
::sysinit:/etc/init.d/rcS

# The SDM845's UART, exposed on the USB-C SBU pins. Reachable only with an
# SDM845 debug cable, but it is the console the kernel cmdline names.
ttyMSM0::respawn:/sbin/getty -L ttyMSM0 115200 vt100

::restart:/sbin/init
::ctrlaltdel:/sbin/reboot
::shutdown:/sbin/swapoff -a
::shutdown:/bin/umount -a -r
```

- [ ] **Step 3: Bring the base kernel config across**

The 9167-line config is the pmOS clang-generated config for this device. Copy it verbatim; do not
regenerate or hand-edit it.

```bash
cd /home/kunaldawn/workspace/repos/kdos
mkdir -p ports/mobile/linux-fajita
cp /home/kunaldawn/workspace/repos/kmobile/board/kmobile/fajita/linux-base.config \
   ports/mobile/linux-fajita/linux-base.config
wc -l ports/mobile/linux-fajita/linux-base.config   # expect 9167
```

- [ ] **Step 4: Write the KDOS config fragment**

`ports/mobile/linux-fajita/linux.config` — carried over verbatim from the archived kmobile repo,
including its comments, because each line is a decision with a reason.

```
# Built with GCC, not clang: the pmOS base config is clang-generated, and
# kconfig drops the clang-only hardening options during olddefconfig.
# CONFIG_MODULE_SIG is not set
# CONFIG_EFI_ZBOOT is not set

# No initramfs: USB gadget has to be available the moment userspace starts,
# so configfs and its ECM function are built in rather than modular.
CONFIG_USB_CONFIGFS=y
CONFIG_USB_CONFIGFS_ECM=y
CONFIG_USB_GADGET=y
CONFIG_USB_LIBCOMPOSITE=y

# Root lives on a GPT-labelled UFS partition, mounted without an initramfs.
CONFIG_EXT4_FS=y
CONFIG_EFI_PARTITION=y
CONFIG_SCSI_UFS_QCOM=y
CONFIG_DEVTMPFS_MOUNT=y

# Crash forensics: with no serial cable, pstore is the only post-mortem source.
CONFIG_PSTORE=y
CONFIG_PSTORE_RAM=y
CONFIG_PSTORE_CONSOLE=y
```

- [ ] **Step 5: Write the recipe**

`ports/mobile/linux-fajita/kpkgbuild`. The version string is the upstream tag; the sha512 is from
Global Constraints.

```
# (banner header)

name        = linux-fajita
version     = sdm845-7.1-rc1-r0
release     = 1
source      = https://gitlab.com/sdm845-mainline/linux/-/archive/$version/linux-$version.tar.gz
sha512      = 94da173aaf74dd33ef8ff9015e92759481cd0da5bd8a1e52c8664fd372d48a1155655001ad2e9d26557733c71ae8bc448e4ebdf1372b8f32159907a35f0306a9  linux-sdm845-7.1-rc1-r0.tar.gz
description = Mainline SDM845 kernel for the OnePlus 6T
homepage    = https://gitlab.com/sdm845-mainline/linux
secdb       = linux-lts
```

> Confirm `sha512` is a key `kpkg meta` accepts. `ports/core/*` use `sha256`. If `sha512` is not
> supported, compute the sha256 of the same tarball and use that instead — **do not drop the
> checksum**. Verify with:
> `ports/.kpkgbin/kpkg meta linux-fajita` (with `PORT_REPO` set) and check the field appears.

`ports/mobile/linux-fajita/build.sh`:

```bash
#!/bin/bash
# (banner header)

export ARCH=arm64
export CROSS_COMPILE=aarch64-kdos-linux-musl-

make mrproper

# The pmOS device config is the base; the KDOS fragment is appended on top and
# olddefconfig resolves the rest. The base is clang-generated, so the
# clang-only options simply drop out here — that is expected, not an error.
cp $PORT_SRC/linux-base.config .config
cat $PORT_SRC/linux.config >> .config
make olddefconfig

make HOSTCFLAGS="-D__attribute_const__= -D__always_inline=inline" Image.gz dtbs modules
make INSTALL_MOD_PATH=$PKG DEPMOD=/bin/true modules_install

rm -f "$PKG/lib/modules/"*/build
rm -f "$PKG/lib/modules/"*/source

mkdir -p $PKG/boot/dtbs/qcom
cp arch/arm64/boot/Image.gz $PKG/boot/Image.gz
cp arch/arm64/boot/dts/qcom/sdm845-oneplus-fajita.dtb $PKG/boot/dtbs/qcom/
cp System.map $PKG/boot/System.map-$version
```

- [ ] **Step 6: Fetch the kernel tarball and verify its checksum**

```bash
cd /home/kunaldawn/workspace/repos/kdos
bash ports/fetch 2>&1 | tail -20
(cd ports/mobile/linux-fajita && sha512sum -c <<'EOF'
94da173aaf74dd33ef8ff9015e92759481cd0da5bd8a1e52c8664fd372d48a1155655001ad2e9d26557733c71ae8bc448e4ebdf1372b8f32159907a35f0306a9  linux-sdm845-7.1-rc1-r0.tar.gz
EOF
)
```

Expected: `linux-sdm845-7.1-rc1-r0.tar.gz: OK`.

> `ports/fetch` does not know about `ports/mobile` until Task 10 Step 1. Either do that step now,
> or fetch this one tarball by hand into `ports/mobile/linux-fajita/` and checksum it as above.

- [ ] **Step 7: Build the fajita kernel**

```bash
cd /home/kunaldawn/workspace/repos/kdos
KDOS_BOARD=fajita make build-mobile BUILD_ARGS="--continue-from kernel --phases kernel --plain" \
    2>&1 | tee /tmp/mobile-fajita-kernel.log
```

Expected: exit 0.

> **If the kernel fails to compile with GCC, capture the first error and stop — do not switch
> toolchains silently.** The documented fallback is to build it with clang by adding
> `LLVM=1` to the `make` lines in `build.sh` and installing `clang lld llvm` in the Dockerfile.
> That is a deliberate decision, not a workaround to apply mid-task.

- [ ] **Step 8: Verify the kernel and DTB**

```bash
cd /home/kunaldawn/workspace/repos/kdos
K=build-mobile/fs/boot/Image.gz
D=build-mobile/fs/boot/dtbs/qcom/sdm845-oneplus-fajita.dtb
test -f "$K" || { echo "FAIL: no Image.gz"; exit 1; }
test -f "$D" || { echo "FAIL: no fajita dtb"; exit 1; }
file "$K" | grep -q gzip || { echo "FAIL: Image.gz is not gzip"; exit 1; }
head -c4 "$D" | od -An -tx1 | tr -d ' \n' | grep -qi 'd00dfeed' || { echo "FAIL: dtb lacks FDT magic"; exit 1; }
echo "PASS: fajita kernel"
```

**Checkpoint:** `Image.gz` is gzip and the DTB carries the FDT magic `d00dfeed`.
`bash testing/preflight.sh` still passes. Stop for the user to commit.

---

## Task 8: USB ECM gadget, SSH and the mobile init package

**Files:**
- Create: `ports/mobile/kdos-mobile-init/{kpkgbuild,build.sh}`
- Create: `ports/mobile/kdos-mobile-init/src/kdos-usbnet`
- Create: `ports/mobile/kdos-mobile-init/src/S30usbnet`
- Create: `script-mobile/host/usbnet-host.sh`

**Interfaces:**
- Consumes: `CONFIG_USB_CONFIGFS_ECM=y` from Task 7's fragment; `iproute2` and `openssh` from
  Task 5's phase-4 list.
- Produces: `/usr/bin/kdos-usbnet` and `/etc/init.d/S30usbnet` in the rootfs; the phone at
  `172.16.42.1`, the host at `172.16.42.2`.

- [ ] **Step 1: Write the gadget script**

`ports/mobile/kdos-mobile-init/src/kdos-usbnet` — carried over from the archived kmobile repo.
ECM rather than RNDIS because the development host is Linux, where ECM needs no driver coaxing.

```sh
#!/bin/sh
# (banner header)
#
# Brings up a USB ethernet gadget via configfs. ECM rather than RNDIS: the
# development host is Linux, where ECM needs no driver coaxing.
set -eu

GADGET=/sys/kernel/config/usb_gadget/kdos
DEV_IP=172.16.42.1

mount -t configfs none /sys/kernel/config 2>/dev/null || true

[ -d "$GADGET" ] && exit 0

mkdir -p "$GADGET"
cd "$GADGET"

# 0x1d6b/0x0104 is Linux Foundation / Multifunction Composite Gadget.
echo 0x1d6b > idVendor
echo 0x0104 > idProduct
echo 0x0100 > bcdDevice
echo 0x0200 > bcdUSB

mkdir -p strings/0x409
echo "KDOS"          > strings/0x409/manufacturer
echo "OnePlus 6T"    > strings/0x409/product
echo "0123456789"    > strings/0x409/serialnumber

mkdir -p configs/c.1/strings/0x409
echo "ECM" > configs/c.1/strings/0x409/configuration
echo 250   > configs/c.1/MaxPower

mkdir -p functions/ecm.usb0
ln -sf functions/ecm.usb0 configs/c.1/

# The single UDC the SoC exposes; its name is the dwc3 node address.
udc=$(ls /sys/class/udc | head -n1)
[ -n "$udc" ] || { echo "kdos-usbnet: no UDC found" >&2; exit 1; }
echo "$udc" > UDC

# The interface appears only once the gadget is bound.
for _ in 1 2 3 4 5 6 7 8 9 10; do
    [ -d /sys/class/net/usb0 ] && break
    sleep 1
done

ip addr add "$DEV_IP/24" dev usb0 2>/dev/null || true
ip link set usb0 up
```

- [ ] **Step 2: Write the init script**

`ports/mobile/kdos-mobile-init/src/S30usbnet`:

```sh
#!/bin/sh
# (banner header)
case "$1" in
    start)
        printf 'Starting USB ethernet gadget: '
        /usr/bin/kdos-usbnet && echo OK || echo FAIL
        ;;
    stop)
        ;;
    restart|reload)
        "$0" start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac
```

- [ ] **Step 3: Write the recipe**

`ports/mobile/kdos-mobile-init/kpkgbuild`:

```
# (banner header)

name        = kdos-mobile-init
version     = 1
release     = 1
description = USB ECM gadget bring-up and SSH host key generation for KDOS mobile
depends     = iproute2 openssh
```

`ports/mobile/kdos-mobile-init/build.sh` — there is no upstream tarball, so the build installs
from `$PORT_SRC` directly.

```bash
#!/bin/bash
# (banner header)

install -Dm755 $PORT_SRC/src/kdos-usbnet $PKG/usr/bin/kdos-usbnet
install -Dm755 $PORT_SRC/src/S30usbnet   $PKG/etc/init.d/S30usbnet

# Key-only root login over the gadget. Password auth is off because the only
# network this listens on is a USB cable, and a default password on it would
# be a default password on the device.
install -dm700 $PKG/root/.ssh
install -Dm644 /dev/null $PKG/etc/ssh/sshd_config
cat > $PKG/etc/ssh/sshd_config <<'EOF'
Port 22
PermitRootLogin prohibit-password
PasswordAuthentication no
PubkeyAuthentication yes
AuthorizedKeysFile .ssh/authorized_keys
Subsystem sftp /usr/libexec/sftp-server
EOF
```

> A recipe with no `source` must still extract cleanly. If `kpkg` requires at least one source,
> confirm against `src/packages/kdos-*/kpkgbuild` — those are sourceless too — and follow whatever
> shape they use.

- [ ] **Step 4: Install the builder's public key at packaging time**

Add to `script-mobile/06_packaging/00_cleanup.sh`, before the trimming block:

```bash
# The key baked into the image is the builder's. KDOS_SSH_KEY names one
# explicitly; otherwise the first ~/.ssh/id_*.pub is used. No key means no way
# in over the cable, so this says so loudly rather than shipping a locked image.
KEY="${KDOS_SSH_KEY:-}"
if [ -z "$KEY" ]; then
    for k in "$HOME"/.ssh/id_*.pub; do [ -f "$k" ] && { KEY=$k; break; }; done
fi
if [ -n "$KEY" ] && [ -f "$KEY" ]; then
    install -dm700 "$SYSROOT/root/.ssh"
    install -m600 "$KEY" "$SYSROOT/root/.ssh/authorized_keys"
    echo ">>> Baked in $(basename "$KEY")"
else
    echo "WARNING: no SSH public key found — the image will have no way in over USB." >&2
    echo "         Set KDOS_SSH_KEY=/path/to/key.pub and rebuild the packaging phase." >&2
fi
```

- [ ] **Step 5: Write the host-side helper**

`script-mobile/host/usbnet-host.sh`:

```bash
#!/usr/bin/env bash
# (banner header)
# Configures the host end of the USB ethernet link. Runs on the HOST.
set -euo pipefail

iface=$(ip -o link show | awk -F': ' '/enp.*u|usb/ {print $2; exit}')
[ -n "$iface" ] || { echo "ERROR: no USB ethernet interface found. Is the phone booted?"; exit 1; }

echo "Using $iface"
sudo ip addr replace 172.16.42.2/24 dev "$iface"
sudo ip link set "$iface" up
echo "Host ready. Try: ssh root@172.16.42.1"
```

```bash
chmod +x script-mobile/host/usbnet-host.sh
```

- [ ] **Step 6: Rebuild phase 4 and packaging**

```bash
cd /home/kunaldawn/workspace/repos/kdos
KDOS_BOARD=fajita make build-mobile \
    BUILD_ARGS="--continue-from phase4 --phases phase4,packaging --plain" 2>&1 | tail -30
```

- [ ] **Step 7: Verify what landed in the rootfs**

```bash
cd /home/kunaldawn/workspace/repos/kdos
R=build-mobile/fs
test -x "$R/usr/bin/kdos-usbnet"  || { echo "FAIL: kdos-usbnet missing or not executable"; exit 1; }
test -x "$R/etc/init.d/S30usbnet" || { echo "FAIL: S30usbnet missing or not executable"; exit 1; }
test -x "$R/usr/sbin/sshd"        || { echo "FAIL: sshd missing"; exit 1; }
grep -q 'PasswordAuthentication no' "$R/etc/ssh/sshd_config" || { echo "FAIL: password auth not disabled"; exit 1; }
grep -q '^172.16.42.1$' <(sed -n 's/^DEV_IP=//p' "$R/usr/bin/kdos-usbnet") || { echo "FAIL: wrong device IP"; exit 1; }
test -s "$R/root/.ssh/authorized_keys" || { echo "FAIL: no authorized_keys baked in"; exit 1; }
test "$(stat -c%a "$R/root/.ssh")" = 700 || { echo "FAIL: .ssh not 0700"; exit 1; }
test "$(stat -c%a "$R/root/.ssh/authorized_keys")" = 600 || { echo "FAIL: authorized_keys not 0600"; exit 1; }
grep -q 'ttyMSM0' "$R/etc/inittab" || { echo "FAIL: getty not on ttyMSM0"; exit 1; }
echo "PASS: usbnet + ssh"
```

**Checkpoint:** all assertions above pass, and `bash testing/preflight.sh` still passes. Stop for
the user to commit.

---

## Task 9: Firmware and the boot image

**Files:**
- Create: `script-mobile/host/fetch-firmware.sh`
- Create: `script-mobile/host/mkbootimg.py`, `script-mobile/host/gki/__init__.py`,
  `script-mobile/host/gki/generate_gki_certificate.py` (vendored AOSP, Apache-2.0)
- Create: `board/fajita/firmware.files`
- Create: `script-mobile/06_packaging/01_firmware.sh`
- Create: `script-mobile/06_packaging/03_bootimg.sh`
- Create: `script-mobile/host/{flash.sh,boot.sh}`
- Create: `testing/mobile-bootimg.sh`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: `board/fajita/deviceinfo` from Task 7; `Image.gz` and the DTB from Task 7.
- Produces: `build-mobile/images/boot.img`.

- [ ] **Step 1: Record the firmware file list**

`board/fajita/firmware.files` — 17 paths, verbatim. **They are under `enchilada`, not `fajita`.**
Both the OnePlus 6 and 6T use that path because it is what `sdm845-oneplus-common.dtsi` asks for.

```
qcom/sdm845/OnePlus/enchilada/a630_zap.mbn
qcom/sdm845/OnePlus/enchilada/adsp.mbn
qcom/sdm845/OnePlus/enchilada/adspr.jsn
qcom/sdm845/OnePlus/enchilada/adspua.jsn
qcom/sdm845/OnePlus/enchilada/cdsp.mbn
qcom/sdm845/OnePlus/enchilada/cdspr.jsn
qcom/sdm845/OnePlus/enchilada/ipa_fws.mbn
qcom/sdm845/OnePlus/enchilada/mba.mbn
qcom/sdm845/OnePlus/enchilada/modem.mbn
qcom/sdm845/OnePlus/enchilada/modemr.jsn
qcom/sdm845/OnePlus/enchilada/modemuw.jsn
qcom/sdm845/OnePlus/enchilada/slpi.mbn
qcom/sdm845/OnePlus/enchilada/slpir.jsn
qcom/sdm845/OnePlus/enchilada/slpius.jsn
qcom/sdm845/OnePlus/enchilada/venus.mbn
qcom/sdm845/OnePlus/enchilada/wlanmdsp.mbn
qca/OnePlus/enchilada/crnv21.bin
```

- [ ] **Step 2: Write the firmware fetcher**

`script-mobile/host/fetch-firmware.sh`. Network step, host-side, checksum-verified.

```bash
#!/usr/bin/env bash
# (banner header)
#
# Downloads the proprietary SDM845 firmware. HOST-side and the only network
# step besides `make fetch`. The blobs are NEVER committed.
set -euo pipefail
cd "$(dirname "$0")/../.."

COMMIT="3e31a0c3e5a061645c09f805387b49fa9d35acbf"
URL="https://gitlab.com/sdm845-mainline/firmware-oneplus-sdm845/-/archive/$COMMIT/firmware-oneplus-sdm845-$COMMIT.tar.gz"
SHA512="56604c46ad0f30f1f715af4b83bd2686d96723024063c7ef6baa5c73cd52b375478e3d65b886ff08efc95ce129fc789b8509efcdf9f0d46cf7229f4ec1afa513"

mkdir -p build-mobile/dl build-mobile/firmware
TARBALL="build-mobile/dl/firmware-oneplus-sdm845-$COMMIT.tar.gz"

if [ ! -f "$TARBALL" ]; then
    echo "==> Downloading firmware at $COMMIT..."
    curl -fL "$URL" -o "$TARBALL.tmp"
    mv "$TARBALL.tmp" "$TARBALL"
fi

echo "$SHA512  $TARBALL" | sha512sum -c - || {
    echo "ERROR: firmware checksum mismatch — refusing to unpack." >&2
    exit 1
}

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
tar xf "$TARBALL" -C "$tmp" --strip-components=1
rsync -a --delete "$tmp/lib/firmware/" build-mobile/firmware/

echo "==> $(find build-mobile/firmware -type f | wc -l) firmware files in build-mobile/firmware/"
```

```bash
chmod +x script-mobile/host/fetch-firmware.sh
```

- [ ] **Step 3: Keep the blobs out of git**

Append to `.gitignore`:

```
build-mobile/firmware/
```

`build-mobile/` is already ignored from Task 1; this line is belt-and-braces so the intent is
explicit and survives anyone narrowing that rule.

- [ ] **Step 4: Write the firmware packaging step**

`script-mobile/06_packaging/01_firmware.sh`:

```bash
#!/bin/bash
# (banner header)
set -e
source script-mobile/packaging.env.sh

BOARD_DIR="$WORKSPACE/board/$KDOS_BOARD"
LIST="$BOARD_DIR/firmware.files"
SRC="$BUILD_DIR/firmware"

# A board with no firmware list needs none — qemu-aarch64 is that board.
[ -f "$LIST" ] || { echo ">>> No firmware list for $KDOS_BOARD, skipping."; exit 0; }

test -d "$SRC" || {
    echo "ERROR: $SRC missing — run 'make fetch-firmware' on the host first." >&2
    exit 1
}

n=0
while read -r f; do
    case "$f" in ''|\#*) continue;; esac
    test -f "$SRC/$f" || {
        echo "ERROR: firmware blob missing: $f — run 'make fetch-firmware'." >&2
        exit 1
    }
    install -Dm644 "$SRC/$f" "$SYSROOT/lib/firmware/$f"
    n=$((n + 1))
done < "$LIST"

echo ">>> Installed $n firmware blobs."
```

- [ ] **Step 5: Vendor `mkbootimg` and its `gki` sibling**

`mkbootimg.py` imports a sibling `gki` module; the script alone will not import.

```bash
cd /home/kunaldawn/workspace/repos/kdos
mkdir -p script-mobile/host/gki
cp /home/kunaldawn/workspace/repos/kmobile/scripts/mkbootimg.py script-mobile/host/mkbootimg.py
cp /home/kunaldawn/workspace/repos/kmobile/scripts/gki/*.py     script-mobile/host/gki/
python3 script-mobile/host/mkbootimg.py --help >/dev/null && echo "PASS: mkbootimg imports"
```

Expected: `PASS: mkbootimg imports`. A `ModuleNotFoundError: No module named 'gki'` means the
sibling module or its `__init__.py` did not come across.

- [ ] **Step 6: Write the boot image step**

`script-mobile/06_packaging/03_bootimg.sh`:

```bash
#!/bin/bash
# (banner header)
#
# arm64 Linux has no appended-DTB boot protocol. This works because the
# Qualcomm bootloader scans the kernel blob for the FDT magic after the gzip
# payload, so Image.gz and the DTB are concatenated explicitly here.
set -e
source script-mobile/packaging.env.sh

BOARD_DIR="$WORKSPACE/board/$KDOS_BOARD"
want=$(sed -n 's/^bootimg=//p' "$BOARD_DIR/deviceinfo")
[ "$want" = "yes" ] || { echo ">>> Board $KDOS_BOARD needs no boot.img, skipping."; exit 0; }

get() { sed -n "s/^$1=//p" "$BOARD_DIR/deviceinfo"; }

KERNEL="$SYSROOT/boot/$(get kernel_image)"
DTB="$SYSROOT/boot/dtbs/$(get dtb)"
KERNEL_DTB="$IMAGES/Image.gz-dtb"

test -f "$KERNEL" || { echo "ERROR: $KERNEL missing"; exit 1; }
test -f "$DTB"    || { echo "ERROR: $DTB missing"; exit 1; }

cat "$KERNEL" "$DTB" > "$KERNEL_DTB"

python3 "$WORKSPACE/script-mobile/host/mkbootimg.py" \
    --kernel "$KERNEL_DTB" \
    --base "$(get bootimg_base)" \
    --kernel_offset "$(get bootimg_kernel_offset)" \
    --ramdisk_offset "$(get bootimg_ramdisk_offset)" \
    --second_offset "$(get bootimg_second_offset)" \
    --tags_offset "$(get bootimg_tags_offset)" \
    --pagesize "$(get bootimg_pagesize)" \
    --header_version "$(get bootimg_header_version)" \
    --cmdline "$(get cmdline)" \
    --output "$IMAGES/boot.img"

echo ">>> Wrote $IMAGES/boot.img ($(stat -c%s "$IMAGES/boot.img") bytes)"
```

```bash
chmod +x script-mobile/06_packaging/01_firmware.sh script-mobile/06_packaging/03_bootimg.sh
```

- [ ] **Step 7: Write the fastboot helpers**

`script-mobile/host/boot.sh` — writes nothing to the phone.

```bash
#!/usr/bin/env bash
# (banner header)
# fastboot boot — loads the image into RAM and jumps to it. Writes NOTHING.
set -euo pipefail
cd "$(dirname "$0")/../.."

IMG=build-mobile/images/boot.img
[ -f "$IMG" ] || { echo "ERROR: $IMG missing — run 'make build-mobile' first"; exit 1; }
command -v fastboot >/dev/null || { echo "ERROR: fastboot not installed on the host"; exit 1; }

echo "Waiting for a fastboot device (hold Volume Up + Power, then plug in USB)..."
fastboot getvar product 2>&1 | head -1
fastboot boot "$IMG"
```

`script-mobile/host/flash.sh` — writes exactly two partitions, after an explicit `yes`.

```bash
#!/usr/bin/env bash
# (banner header)
#
# Writes ONLY boot and userdata. aboot, xbl, modem and persist are off limits:
# writing them can hard-brick the device.
set -euo pipefail
cd "$(dirname "$0")/../.."

BOOT=build-mobile/images/boot.img
ROOT=build-mobile/images/rootfs.ext4
for f in "$BOOT" "$ROOT"; do
    [ -f "$f" ] || { echo "ERROR: $f missing — run 'make build-mobile' first"; exit 1; }
done
command -v fastboot >/dev/null || { echo "ERROR: fastboot not installed on the host"; exit 1; }

echo "This will overwrite the 'boot' and 'userdata' partitions on the connected device."
echo "Everything currently on userdata will be destroyed."
printf 'Type "yes" to continue: '
read -r reply
[ "$reply" = "yes" ] || { echo "Aborted."; exit 1; }

fastboot flash boot "$BOOT"
fastboot flash userdata "$ROOT"
echo "Done. Reboot with: fastboot reboot"
```

```bash
chmod +x script-mobile/host/boot.sh script-mobile/host/flash.sh
```

- [ ] **Step 8: Write the boot image test**

`testing/mobile-bootimg.sh` parses the header and checks every field the bootloader reads.

```bash
#!/usr/bin/env bash
# (banner header)
set -euo pipefail
cd "$(dirname "$0")/.."

IMG=build-mobile/images/boot.img
[ -f "$IMG" ] || { echo "FAIL: boot.img missing — run: KDOS_BOARD=fajita make build-mobile"; exit 1; }

python3 - "$IMG" <<'PY'
import struct, sys
data = open(sys.argv[1], 'rb').read()
assert data[:8] == b'ANDROID!', f"bad magic {data[:8]!r}"
(kernel_size, kernel_addr, ramdisk_size, ramdisk_addr,
 second_size, second_addr, tags_addr, page_size,
 header_version) = struct.unpack('<9I', data[8:44])
assert page_size == 4096, f"pagesize {page_size}"
assert header_version == 0, f"header_version {header_version}"
assert kernel_addr == 0x00008000, f"kernel_addr {kernel_addr:#x}"
assert ramdisk_addr == 0x01000000, f"ramdisk_addr {ramdisk_addr:#x}"
assert second_addr == 0x00f00000, f"second_addr {second_addr:#x}"
assert tags_addr == 0x00000100, f"tags_addr {tags_addr:#x}"
assert kernel_size > 4 * 1024 * 1024, f"kernel suspiciously small: {kernel_size}"
cmdline = data[64:64+512].split(b'\x00')[0].decode()
for token in ("console=ttyMSM0,115200", "root=PARTLABEL=userdata", "rw", "rootwait"):
    assert token in cmdline, f"cmdline missing {token!r}: {cmdline!r}"
kernel = data[page_size:page_size + kernel_size]
assert kernel[:2] == b'\x1f\x8b', "kernel payload is not gzip"
assert b'\xd0\x0d\xfe\xed' in kernel[-4*1024*1024:], "no appended DTB found in kernel payload"
print("boot.img header OK")
PY

echo "PASS: bootimg"
```

```bash
chmod +x testing/mobile-bootimg.sh
```

- [ ] **Step 9: Fetch firmware and build the full fajita image**

```bash
cd /home/kunaldawn/workspace/repos/kdos
bash script-mobile/host/fetch-firmware.sh
KDOS_BOARD=fajita make build-mobile BUILD_ARGS="--continue-from packaging --phases packaging --plain"
```

Expected: the fetcher reports 115 files; packaging reports `Installed 17 firmware blobs.` and
writes `boot.img`.

- [ ] **Step 10: Verify**

```bash
cd /home/kunaldawn/workspace/repos/kdos
bash testing/mobile-bootimg.sh
for f in $(grep -v '^#' board/fajita/firmware.files); do
    test -f "build-mobile/fs/lib/firmware/$f" || { echo "FAIL: $f not in rootfs"; exit 1; }
done
echo "PASS: firmware in rootfs"
git status --porcelain | grep -q 'build-mobile' && { echo "FAIL: build-mobile is not ignored"; exit 1; }
echo "PASS: no blobs staged"
```

**Checkpoint:** `testing/mobile-bootimg.sh` passes, all 17 blobs are in the rootfs, git sees
nothing under `build-mobile/`, and `bash testing/preflight.sh` still passes. Stop for the user to
commit.

---

## Task 10: Wire it into `make`, `ports/fetch` and the test rig

**Files:**
- Modify: `ports/fetch` (teach it `ports/mobile`)
- Modify: `Makefile` (mobile targets)
- Modify: `testing/preflight.sh` (make it cover the mobile tree)
- Create: `testing/mobile-preflight.sh`

**Interfaces:**
- Produces: `make fetch`, `make build-mobile`, `make fetch-firmware`, `make run-mobile`,
  `make boot-mobile`, `make flash-mobile`, `make test-mobile`.

- [ ] **Step 1: Teach `ports/fetch` about the mobile repo**

`ports/fetch:26` reads `PORT_REPO="$SCRIPT_DIR/core"`. Change it to:

```bash
PORT_REPO="$SCRIPT_DIR/core $SCRIPT_DIR/mobile"
```

Then confirm the loop that walks port directories iterates every entry of `$PORT_REPO`, not just
the first. If it globs `"$PORT_REPO"/*`, change that to a loop over the words:

```bash
for repo in $PORT_REPO; do
    for portdir in "$repo"/*/; do
        fetch_port "$portdir" || true
    done
done
```

- [ ] **Step 2: Add the mobile targets to the Makefile**

Append. `--network none` and the `HOST_UID`/`HOST_GID` handling mirror the desktop `build` target
exactly; the mobile build additionally needs `--privileged` for the chroot's bind mounts.

```make
# ── mobile: aarch64, OnePlus 6T (fajita) and QEMU virt ────────────────────
#
# KDOS_BOARD selects the board; everything else is shared with the desktop
# image. --privileged is for the chroot's bind mounts, exactly as the desktop
# build already needs. Flashing NEVER happens in here — see flash-mobile.
KDOS_BOARD ?= fajita

build-mobile:
	mkdir -p build-mobile
	docker build -t os-dev .
	docker run --network none --cpus="10" --rm --privileged \
		-e HOST_UID=$$(id -u) -e HOST_GID=$$(id -g) \
		-e KDOS_BOARD="$(KDOS_BOARD)" \
		-e KDOS_SSH_KEY="$(KDOS_SSH_KEY)" \
		-e KDOS_GIT_COMMIT="$$(git rev-parse --short HEAD 2>/dev/null)" \
		-v $$(pwd)/build-mobile:/workspace/build-mobile \
		-v $$(pwd)/src:/workspace/src:ro \
		-v $$(pwd)/fs-mobile:/workspace/fs-mobile:ro \
		-v $$(pwd)/board:/workspace/board:ro \
		-v $$(pwd)/script-mobile:/workspace/script-mobile:ro \
		-v $$(pwd)/ports:/workspace/ports:ro \
		$(DOCKER_TTY) os-dev script-mobile/kdosbuild.sh $(BUILD_ARGS)

# The only mobile target that touches the network besides `make fetch`.
fetch-firmware:
	bash script-mobile/host/fetch-firmware.sh

run-mobile:
	bash testing/mobile-qemu-boot.sh

# fastboot boot — RAM only, writes nothing to the phone.
boot-mobile:
	bash script-mobile/host/boot.sh

# Writes boot and userdata. Asks for confirmation first.
flash-mobile:
	bash script-mobile/host/flash.sh

test-mobile:
	bash testing/mobile-preflight.sh

clean-mobile:
	test -d build-mobile && find build-mobile -mindepth 1 -maxdepth 1 ! -name snapshots -exec rm -rf {} + || true

.PHONY: build-mobile fetch-firmware run-mobile boot-mobile flash-mobile test-mobile clean-mobile
```

Add the same names to the existing `.PHONY` line at the bottom of the file if you prefer one
list; do not remove any existing target.

- [ ] **Step 3: Write the mobile preflight**

`testing/mobile-preflight.sh` — everything a full build would catch, minus the build. Same spirit
as `testing/preflight.sh`, scoped to the mobile tree.

```bash
#!/usr/bin/env bash
# (banner header)
# Wiring gate for the mobile target. Seconds, no container, no network.
set -uo pipefail
cd "$(dirname "$0")/.."

fail=0
say() { echo "  $*"; }
bad() { echo "FAIL: $*"; fail=1; }

# 1. Every phase directory has the env file kbuild_discover will look for.
for p in toolchain phase1 phase2 phase3 phase4 kernel packaging; do
    [ -f "script-mobile/$p.env.sh" ] || bad "missing script-mobile/$p.env.sh"
done

# 2. Chroot flags are on exactly the phases that need them.
for p in phase2 phase3 phase4; do
    grep -q '^export CHROOT=1' "script-mobile/$p.env.sh" || bad "$p should be a chroot phase"
done
for p in toolchain phase1 kernel packaging; do
    grep -q '^export CHROOT=1' "script-mobile/$p.env.sh" && bad "$p must be host-side"
done

# 3. Nothing in the mobile tree still targets x86.
grep -rn 'x86_64\|i386\|amd64' script-mobile/ 2>/dev/null | grep -v Binary && bad "x86 reference in script-mobile/"

# 4. Every shipped and build shell script parses.
for f in $(find script-mobile ports/mobile -name '*.sh' -o -name 'kdos-usbnet' -o -name 'S30usbnet'); do
    bash -n "$f" 2>/dev/null || bad "syntax error: $f"
done

# 5. Every board declares the keys the packaging steps read.
for b in board/*/; do
    d="$b/deviceinfo"
    [ -f "$d" ] || continue
    for k in board arch kernel_port kernel_image cmdline rootfs_size rootfs_label bootimg; do
        grep -q "^$k=" "$d" || bad "$d declares no $k"
    done
    if grep -q '^bootimg=yes' "$d"; then
        for k in bootimg_base bootimg_kernel_offset bootimg_ramdisk_offset \
                 bootimg_second_offset bootimg_tags_offset bootimg_pagesize \
                 bootimg_header_version dtb; do
            grep -q "^$k=" "$d" || bad "$d says bootimg=yes but declares no $k"
        done
    fi
    kp=$(sed -n 's/^kernel_port=//p' "$d")
    [ -f "ports/mobile/$kp/kpkgbuild" ] || bad "$d names kernel_port '$kp' with no recipe"
done

# 6. Every package named in a mobile packages.txt has a port.
for f in script-mobile/0[234]_*/packages.txt; do
    [ -f "$f" ] || continue
    while read -r pkg; do
        case "$pkg" in ''|\#*) continue;; esac
        [ -f "ports/mobile/$pkg/kpkgbuild" ] || [ -f "ports/core/$pkg/kpkgbuild" ] \
            || bad "$f names '$pkg' with no port"
    done < "$f"
done

# 7. Every recipe under ports/mobile declares the required keys.
for r in ports/mobile/*/kpkgbuild; do
    [ -f "$r" ] || continue
    for k in name version release description; do
        grep -q "^$k *=" "$r" || bad "$r declares no $k"
    done
done

# 8. mkbootimg can import its gki sibling.
python3 script-mobile/host/mkbootimg.py --help >/dev/null 2>&1 || bad "mkbootimg.py does not import (missing gki/?)"

# 9. The flash script writes only boot and userdata.
parts=$(grep -o 'fastboot flash [a-z]*' script-mobile/host/flash.sh | awk '{print $3}' | sort -u | tr '\n' ' ')
[ "$parts" = "boot userdata " ] || bad "flash.sh writes partitions other than boot+userdata: $parts"

# 10. No firmware blob is tracked by git.
git ls-files | grep -E '\.(mbn|jsn)$' && bad "firmware blobs are tracked by git"

[ "$fail" -eq 0 ] && echo "PASS: mobile preflight"
exit "$fail"
```

```bash
chmod +x testing/mobile-preflight.sh
```

- [ ] **Step 4: Run both preflights**

```bash
cd /home/kunaldawn/workspace/repos/kdos
bash testing/preflight.sh
bash testing/mobile-preflight.sh
```

Expected: both PASS. The desktop one is the regression gate — it must be as green as it was
before Task 1.

- [ ] **Step 5: Verify the Makefile targets resolve**

```bash
cd /home/kunaldawn/workspace/repos/kdos
for t in build-mobile fetch-firmware run-mobile boot-mobile flash-mobile test-mobile clean-mobile; do
    make -n "$t" >/dev/null 2>&1 || { echo "FAIL: make $t does not resolve"; exit 1; }
done
make -n build-mobile | grep -q -- '--network none' || { echo "FAIL: mobile build is not offline"; exit 1; }
make -n build-mobile | grep -q 'script-mobile/kdosbuild.sh' || { echo "FAIL: wrong orchestrator"; exit 1; }
make -n flash-mobile | grep -q 'docker' && { echo "FAIL: flashing must not run in a container"; exit 1; }
echo "PASS: makefile"
```

- [ ] **Step 6: Verify `ports/fetch` sees the mobile repo**

```bash
cd /home/kunaldawn/workspace/repos/kdos
bash ports/fetch 2>&1 | grep -q 'linux-fajita' || { echo "FAIL: fetch does not walk ports/mobile"; exit 1; }
echo "PASS: fetch covers ports/mobile"
```

**Checkpoint:** both preflights pass, every `make` target resolves, the mobile build is offline
and flashing is not containerised. Stop for the user to commit.

---

## Task 11: Documentation

**Files:**
- Create: `docs/KDOS-MOBILE.md`
- Modify: `CLAUDE.md`
- Modify: `README.md`

**Interfaces:**
- Produces: the plan of record for the mobile target and the hardware runbook Task 12 follows.

- [ ] **Step 1: Write `docs/KDOS-MOBILE.md`**

Cover, in this order:

1. **What the mobile target is** — a second build target, not a fork: `aarch64-kdos-linux-musl`
   on `fajita` and `qemu-aarch64`, same `kdosbuild`, same `kpkg`, same ports tree.
2. **The device-facts table** — copy the Global Constraints table from this plan verbatim,
   including the Source column. This becomes the canonical copy.
3. **The build model** — cross toolchain for phases 00–01, qemu-user chroot for 02–04, host-side
   kernel and packaging; why (recipe reuse, finding 8); the escape hatch (cross-build a port that
   qemu chokes on).
4. **Boards** — how `board/<name>/deviceinfo` works, that it is parsed and never sourced, and the
   full key list.
5. **The "easy to get wrong" list** — reproduce the Global Constraints section of the same name
   verbatim: `enchilada` not `fajita`, no appended-DTB protocol on arm64, `mkbootimg` needs `gki`,
   the clang-generated base config, no initramfs.
6. **Milestone status** — M1 done/not done, and the explicitly-out-of-scope list from this plan.
7. **The hardware runbook** — the whole of Task 12, written out as numbered steps someone
   follows with a phone in hand, including the failure branches.

The runbook section must contain, at minimum, these headings, because
`testing/mobile-preflight.sh` can be extended to assert them and because they are the branches
that actually get hit: **Back up the phone**, **Unlock the bootloader**, **First boot**,
**Connect**, **If nothing enumerates**, **Reading pstore**, **Making it persistent**,
**Recovery**.

Key content the runbook must carry:

- Unlocking **erases everything**; there is no way to unlock without wiping, and that is enforced
  by the bootloader, not by KDOS.
- `adb reboot bootloader` → `fastboot flashing unlock` → confirm on-device with the volume keys.
- Re-enable USB debugging afterwards; later steps need `adb reboot bootloader`.
- **The screen stays dark.** Display is a later arc. A dark screen is not a failure.
- After 10–30 seconds a USB ethernet interface appears on the host: watch `dmesg -w` and
  `ip link`.
- `bash script-mobile/host/usbnet-host.sh` then `ssh root@172.16.42.1`; confirm with `uname -a`.
- If nothing enumerates: (a) does the host see *any* USB device, (b) some ABL builds reject a
  boot image with no ramdisk — add a dummy one with
  `printf '' | cpio -o -H newc 2>/dev/null | gzip > dummy-ramdisk.gz` and pass
  `--ramdisk` to `mkbootimg.py`, (c) `bash testing/mobile-bootimg.sh` to confirm the DTB is really
  appended, (d) read `ttyMSM0` at 115200 with an SDM845 debug cable.
- pstore: `CONFIG_PSTORE_RAM` with console capture means the previous boot's log survives in RAM
  at `/sys/fs/pstore/console-ramoops-0`.
- `make flash-mobile` writes exactly `boot` and `userdata`; `aboot`, `xbl`, `modem` and `persist`
  are never written.
- Recovery: Volume Up + Power always reaches the bootloader, whatever is on `boot`.

- [ ] **Step 2: Add a mobile section to `CLAUDE.md`**

Insert after the "Build system" section. Keep it terse — CLAUDE.md records *current state and the
reasoning that constrains it*, and every "never do X" there is a bug that already cost a debug
cycle. Cover:

- The mobile target exists, what it targets, and that `docs/KDOS-MOBILE.md` is its plan of record.
- `script-mobile/` + `build-mobile/` + `ports/mobile/` + `board/` + `fs-mobile/`, one line each.
- **The four findings that are load-bearing**, because a future session will otherwise re-derive
  them: `--script-dir`/`--build-dir` already exist; `repo_root` is the parent of `script_dir`, so
  the phase tree must be a direct child of the repo root; `PORT_REPO` is first-match-wins, which
  is what makes `ports/mobile` an overlay; the chroot needs a binfmt handler registered with the
  `F` flag.
- The hard rules from this plan's Global Constraints: never commit firmware, never write a
  partition other than `boot`/`userdata`, flashing is host-side only.
- The "easy to get wrong" list.

Add to `CLAUDE.md`'s hard-rules list:

> **Never write partitions other than `boot` and `userdata`.** `aboot`, `xbl`, `modem` and
> `persist` are off limits — writing them can hard-brick the device. Flashing runs on the host,
> never in a container.

- [ ] **Step 3: Add the mobile targets to `README.md`**

A short section with the target table (`make build-mobile`, `make fetch-firmware`,
`make run-mobile`, `make boot-mobile`, `make flash-mobile`, `make test-mobile`), the
`KDOS_BOARD=qemu-aarch64|fajita` switch, and a pointer to `docs/KDOS-MOBILE.md`.

- [ ] **Step 4: Verify the docs are complete**

```bash
cd /home/kunaldawn/workspace/repos/kdos
for f in docs/KDOS-MOBILE.md; do
    [ -s "$f" ] || { echo "FAIL: $f missing or empty"; exit 1; }
done
for s in "Unlock the bootloader" "First boot" "If nothing enumerates" "Reading pstore" \
         "Making it persistent" "Recovery"; do
    grep -qi "$s" docs/KDOS-MOBILE.md || { echo "FAIL: runbook missing section: $s"; exit 1; }
done
for t in build-mobile fetch-firmware run-mobile boot-mobile flash-mobile test-mobile; do
    grep -q "make $t" README.md || { echo "FAIL: README does not document 'make $t'"; exit 1; }
done
grep -q 'enchilada' docs/KDOS-MOBILE.md || { echo "FAIL: the enchilada trap is not documented"; exit 1; }
grep -q 'script-mobile' CLAUDE.md || { echo "FAIL: CLAUDE.md does not mention the mobile tree"; exit 1; }
echo "PASS: docs"
```

**Checkpoint:** the docs assertions pass and both preflights still pass. Stop for the user to
commit.

---

## Task 12: Hardware bring-up — MANUAL, needs the phone

**This task cannot be executed by an agent.** It needs the physical OnePlus 6T and a bootloader
unlock, which **wipes the device**. Do not start it without the user present and their explicit
go-ahead on the wipe.

Everything up to Task 11 is verifiable without hardware. This task is the one that closes the arc.

- [ ] **Step 1: Back up the phone.** Unlocking erases everything. There is no way around it.

- [ ] **Step 2: Unlock the bootloader.** Developer options → OEM unlocking + USB debugging, then
  `adb reboot bootloader`, `fastboot flashing unlock`, confirm with the volume keys. Re-enable USB
  debugging after the wipe.

- [ ] **Step 3: Build and verify before touching the phone.**

```bash
cd /home/kunaldawn/workspace/repos/kdos
bash ports/fetch
bash script-mobile/host/fetch-firmware.sh
KDOS_BOARD=fajita make build-mobile
bash testing/mobile-bootimg.sh
bash testing/mobile-preflight.sh
```

- [ ] **Step 4: First boot, RAM only.**

```bash
adb reboot bootloader
make boot-mobile
```

`fastboot boot` loads into RAM and jumps; nothing is written to storage, and holding the power
button undoes it. **Expect a dark screen** — display is a later arc.

- [ ] **Step 5: Connect.**

```bash
bash script-mobile/host/usbnet-host.sh
ssh root@172.16.42.1
uname -a
```

**The arc is complete when this returns a shell and reports the sdm845 kernel.**

- [ ] **Step 6: Only then, make it persistent.**

```bash
make flash-mobile      # asks for an explicit "yes"; writes boot + userdata only
fastboot reboot
```

- [ ] **Step 7: Record the outcome** in `docs/KDOS-MOBILE.md`'s milestone block — what booted,
  what did not, and any deviation from the runbook. That block is the next session's starting
  point.

---

## Self-Review

**Spec coverage.** Every decision made during design maps to a task: the sibling phase tree
(Task 1), the aarch64 retarget (Tasks 2–3), the qemu-user chroot (Task 4), the mobile package set
(Task 5), the QEMU board and M0 parity (Task 6), the fajita kernel (Task 7), USB ECM + SSH
(Task 8), firmware + boot.img (Task 9), `make`/fetch/test wiring (Task 10), docs (Task 11), and
the hardware step (Task 12). Every device fact from the archived kmobile repo is reproduced in
Global Constraints and consumed by a named task.

**Known gaps, called out rather than hidden.**

1. **Task 5's phase-3 package list is a proposal, not a verified list.** Several names
   (`bzip2`, `grep`, `sed`, `which`, `iputils`, `tzdata`) may not exist under those names in
   `ports/core`. Step 5 of that task is the check that finds out, and it runs in seconds before
   anything is built. Correct the list from `ls ports/core`; do not invent ports.
2. **`sha512` may not be a key `kpkg meta` accepts** — every `ports/core` recipe uses `sha256`.
   Task 7 Step 5 flags this inline with the fallback (compute the sha256 of the same tarball;
   never drop the checksum).
3. **A recipe with no `source` may need a particular shape.** `kdos-mobile-init` (Task 8) has no
   upstream tarball. `src/packages/kdos-*` are the working examples to copy from.
4. **`kpkgdepends` invocation via `exec -a`** in Task 5 Step 6 depends on `kpkg` dispatching on
   `argv[0]`. The symlink fallback is given inline.
5. **`ports/fetch`'s directory walk** may or may not already iterate `$PORT_REPO` word by word.
   Task 10 Step 1 says to check and gives the fix.
6. **Emulated build time is unmeasured.** Phases 02–04 run under qemu-user; expect roughly
   5–10x native. Phase snapshots are what make this survivable — never run `--fresh` casually
   after Task 3.
7. **`mkfs.ext4 -d` must be available in the build image.** The Dockerfile installs Alpine's
   `e2fsprogs`; confirm `mkfs.ext4 -d` is supported there at Task 6 Step 7, and add the package
   to the Dockerfile if the build reports `invalid option -- 'd'`.

**Type and name consistency.** `KDOS_TARGET` is `aarch64-kdos-linux-musl` everywhere.
`KDOS_BOARD` is `fajita` or `qemu-aarch64` everywhere, defaulted in `toolchain.env.sh`,
`kernel.env.sh`, `packaging.env.sh`, `chroot_exec.sh` and the Makefile. `$BUILD_DIR` is
`/workspace/build-mobile`, `$SYSROOT` is `$BUILD_DIR/fs`, `$IMAGES` is `$BUILD_DIR/images` —
consistent across Tasks 1, 6 and 9. The `deviceinfo` keys written in Task 6 Step 1 and Task 7
Step 1 are exactly the keys read in Task 6 Step 6, Task 9 Step 6 and asserted in Task 10 Step 3.
The gadget script is `kdos-usbnet` at `/usr/bin/kdos-usbnet` in Tasks 8 and 10 alike (renamed
from kmobile's `kmobile-usbnet`, consistently, because the distro is KDOS).

**Regression gate.** `bash testing/preflight.sh` is run at the end of every task. The desktop
x86_64 build must stay green throughout; the only pre-existing files this plan touches are
`ports/fetch`, `Makefile`, `testing/preflight.sh`, `.gitignore`, `CLAUDE.md` and `README.md`.
