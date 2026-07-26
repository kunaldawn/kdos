# KDOS — Claude Onboarding

This file briefs a fresh Claude session on KDOS so it can be useful immediately. Read it before doing anything substantive.

---

## What KDOS is

A hand-built Linux distribution following Linux From Scratch principles. **musl libc** + **toybox** userland on the host, **no SystemD**, **Wayland-only** (no Xorg, no XWayland by default). Compositor: **niri** (Smithay/Rust scrollable-tiling). Shell: **noctalia-shell** (QML on a Quickshell fork) — the *only* place Qt 6 is allowed on the host. Everything else "fat" runs in a Podman/distrobox glibc rootfs.

Mascot lives at `kdos.png`. Default wallpaper at `fs/usr/share/backgrounds/kdos/default-wallpaper.png` (penguin, green Matrix glow).

---

## Hard rules — do not violate

1. **No SystemD.** No `systemd-*` packages on the host. Replacements: `seatd` for seat management, `basu` for sd-bus, `eudev` for udev, `dbus` (not dbus-broker), `dnsmasq` (not systemd-resolved), `wpa_supplicant`/`NetworkManager` (not systemd-networkd).
2. **No Xorg.** Already removed: `xorg`, `xorg-server`, `libx11`, all `libx*`, `xcb-util-*`, `dwm`, `st`, `dmenu`, `nsxiv`, etc. If a kpkgbuild tries to add an X dep, push back; usually the build flag exists to disable X.
3. **No GTK on the host.** GTK apps go in distrobox via `kdos-fetch-app`. The single Qt 6 carve-out (carved for noctalia) is documented in the README rule #6.
4. **No `kpkgbuild` rationale comments.** User strips multi-line comment blocks. Keep kpkgbuilds minimal — preserve only the banner header and one-line `# description` / `# homepage` / `# depends`. No "we set X because Y" prose. Belongs in commit messages or this file, not in the kpkgbuild.
5. **Do not auto-commit.** User commits manually, often squashing many edits into one logical commit. Wait for explicit "commit this" requests.
6. **Do not run destructive `git` ops without asking.** No `reset --hard`, `clean -f`, `branch -D`, force pushes.

---

## The Three Rings

| Ring | Lives in | Purpose |
|---|---|---|
| **Core** | `ports/core/` | Hand-compiled host packages (musl + toybox + libs + WM stack) |
| **GUI sliver** | also `ports/core/` (separate `packages.txt` block) | Wayland compositor, shell, terminal, browser-via-distrobox prerequisites |
| **Outer ring** | distrobox containers | Browsers, IDEs, Slack, GIMP — full glibc apps |

There used to be a **`src/packages/`** namespace for vendored upstream forks (Window Maker era). It's currently empty/removed but the build pipeline still supports it via `PORT_REPO="/ports/core /kdos/src/packages"`. Use it if you ever need to vendor a project to apply local patches without `.patch` files.

---

## Repo layout

```
kdos/
├── ports/core/<name>/
│   ├── kpkgbuild              # the recipe (sourced bash; no shebang)
│   └── <name>-<ver>.tar.*     # cached upstream tarball (LFS-tracked)
├── src/
│   ├── kpkg/                  # the kpkg-the-tool source (bash scripts)
│   │   ├── kpkg, kpkgadd, kpkgbuild, kpkgdel, kpkgdepends, kpkg.conf
│   └── kinstall/              # ncurses-style installer (Python)
├── fs/                        # copied verbatim into the rootfs
│   ├── etc/
│   │   ├── inittab            # busybox-init style; getty on tty1
│   │   ├── profile            # sources /etc/profile.d/*.sh
│   │   ├── profile.d/         # 10-wayland.sh sets XDG_RUNTIME_DIR + Qt/Wayland env
│   │   ├── init.d/            # 01_udev → 80_cups; pattern in service_helper
│   │   ├── skel/              # ~/.config/niri/config.kdl, etc.
│   │   ├── kpkg.conf          # runtime kpkg config (matches src/kpkg/kpkg.conf)
│   │   └── X11/               # DOES NOT EXIST — Wayland-only
│   ├── usr/local/bin/
│   │   ├── kdos-fetch-app     # distrobox + distrobox-export wrapper
│   │   ├── kdos-fetch-static  # curl + sha256 + chmod
│   │   └── niri-session       # `dbus-run-session -- niri --session`
│   └── usr/share/
│       └── backgrounds/kdos/default-wallpaper.png
├── script/                    # build.py orchestrator + phase scripts
│   ├── build.py               # Python TUI; reads packages.txt per phase
│   ├── chroot_exec.sh         # bind-mounts repo paths into build/fs and chroots
│   ├── 00_toolchain/          # cross gcc/binutils/musl/etc. (extract_port_source)
│   ├── 01_phase1/             # base userland (gcc, bash, kpkg, kinstall)
│   ├── 02_phase2/, 03_phase3/, 04_phase4/, 05_phase5/  # packages.txt-driven
│   ├── 06_packaging/          # initramfs + ISO assembly
│   └── util/port.sh           # helpers: extract_port_source, kpkg_install
├── testing/
│   ├── prepare_base.py        # one-shot: mini-build a Phase 2 rootfs as Docker base image
│   ├── test_runner.py         # builds individual ports against the base image
│   └── logs/                  # per-port test logs
├── Dockerfile                 # Alpine 3.23 sandbox (build environment)
├── Makefile                   # fetch / build / run / rundisk / clean
└── ports/fetch                # downloads all `source=` URLs; handles vendoring=rust|go|node|python
```

---

## Build process — how it actually runs

1. `make build` → builds Docker image `os-dev` from `Dockerfile` (Alpine 3.23 + gcc/g++/musl-dev/python3/etc.) → runs `python3 script/build.py` inside.
2. `build.py` discovers `script/00_toolchain/`, `01_phase1/`, ... by directory; each contains either:
   - **Numbered shell scripts** (`00_*.sh`, `01_*.sh`, ...) executed in order.
   - **A `packages.txt`** — a list of port names. build.py runs `kpkgdepends <pkgs>` to resolve order, then `kpkg install -f <pkg>` for each.
3. **Phase 4/5 chroot** — `chroot_exec.sh` bind-mounts:
   - `$REPO_ROOT` → `/kdos` (so `/kdos/src`, `/kdos/script` reachable)
   - `$REPO_ROOT/ports` → `/ports`
   - `$REPO_ROOT/build` → `/kdos/build`
   - `/dev`, `/proc`, `/sys`, `/tmp`, `/run`
   Then `chroot build/fs /usr/bin/env -i ... bash -c "cd /kdos && exec ..."`.
4. Phase env files (`script/phase4.env.sh`, `phase5.env.sh`) export `CHROOT=1`, `CFLAGS`, `LDFLAGS`, `PORT_REPO="/ports/core /kdos/src/packages"`.
5. `kpkg install` builds the package via its `kpkgbuild` and stores the result at `/var/cache/kpkg/packages/<name>-<ver>-<rel>.tar.xz`. Subsequent installs reuse the cached package.

---

## Phase snapshots (restore + continue)

Every phase that completes is snapshotted to `build/snapshots/<phase-dir>/`, so a later
build can be resumed from a clean earlier state instead of re-running everything or
building on top of a polluted rootfs.

**What gets snapshotted is declared by the phase, not by build.py.** Each
`script/<phase>.env.sh` carries a metadata block that build.py *parses* (never sources —
sourcing would run those files' `rm -rf /var/cache/kpkg/work` on the build container):

```bash
# --- build-system metadata (parsed by script/build.py, never sourced) ---
export KDOS_PHASE_TITLE="Toolchain & Core Libraries"
export KDOS_PHASE_DESC="compilers, build systems, interpreters, base libraries"
export KDOS_SNAPSHOT_PATHS="fs"                 # relative to build/, dirs or files
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* ..."     # glob patterns, tar --exclude
```

Values must be literal (no `$VAR`). Paths that are absolute, empty, or contain `..` are
refused. A phase with no `KDOS_SNAPSHOT_PATHS` is never snapshotted. Adding a new phase
means adding its env metadata — nothing in build.py needs to change.

Layout: one directory per phase, overwritten in place (one snapshot per phase), one
`<path>.tar.zst` per declared path, plus `manifest.json` (git commit, duration, sizes,
file counts) and `timings.json` feeding the ETA.

**Restoring.** `make build` opens a picker listing the snapshots; pick one and the build
restores it and continues at the *next* phase. Restore is layered — each path is taken
from the newest snapshot at or below the chosen phase, so restoring phase3 pulls `fs`
from phase3 and `cross`/`mark` from phase1.

Non-interactive equivalents:

```bash
make build BUILD_ARGS="--restore phase2"   # restore phase2, continue at phase3
make build BUILD_ARGS=--fresh              # skip the picker, run everything
make build BUILD_ARGS=--no-snapshot        # throwaway run, write no snapshots
make snapshots                             # list them (build.py --list)
make cleanbuild                            # wipe build/ but KEEP build/snapshots
```

`make clean` deletes snapshots along with everything else. Snapshots need `zstd` and GNU
`tar` in the build image (both in the Dockerfile). Budget ~2-4G per phase.

---

## kpkgbuild conventions

```bash
# (banner header — keep verbatim)
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# description	: <one-line description, tab-separated>
# homepage	: <URL>
# depends	: <space-separated port names>

name=foo
version=1.2.3
release=1
source="https://upstream.example/foo-$version.tar.gz"
# Optional: vendoring=rust  (or go/node/python — runs cargo vendor / etc. during make fetch)

build() {
    ./configure --prefix=/usr --libdir=/usr/lib
    make
    make DESTDIR=$PKG install
}

# Optional: postinstall() runs at install time (in chroot)
postinstall() {
    getent group somegrp >/dev/null 2>&1 || groupadd -r somegrp
}
```

**Critical conventions:**

- **No shebang.** kpkg `source`s the file. A `#!/bin/bash` line is harmless but conventionally absent.
- **`# depends:` parsing** — kpkgdepends greps for `^# depends[[:blank:]]*:` and splits on whitespace. Tab or space after `:` both work. The list is space-separated.
- **`$PORT_SRC`, `$SRC`, `$SRC_ROOT`, `$PKG`** — set by kpkg before calling `build()`:
  - `$PORT_SRC` = the directory containing `kpkgbuild` (e.g. `ports/core/foo/`)
  - `$SRC` = `$WORK_DIR/$name/$name-$version` (the extracted source dir; per-build scratch)
  - `$SRC_ROOT` = `$WORK_DIR/$name` (parent of $SRC)
  - `$PKG` = `$WORK_DIR/$name/pkg` (DESTDIR target — the staging tree)
- **`source=` semantics** — kpkg auto-detects the tarball extension and passes `--strip-components=1` for the first source. Subsequent sources extract without strip.
- **`vendoring=rust`** — if set, `ports/fetch` extracts the source, runs `cargo vendor`, and packages `vendor/` + `.cargo/config.toml` into `<name>-vendor-<version>.tar.xz` next to the main tarball. `build()` extracts that vendor tarball into `$SRC` and uses `cargo build --frozen --offline`.

---

## Common build gotchas — recurring fixes

These are patterns we've hit multiple times. When a build fails for one of these reasons, apply the canonical fix.

### Static-musl + Rust + bindgen → "Dynamic loading not supported"

KDOS's Rust toolchain is statically linked against musl. Crates that use `bindgen` or `libloading` (typically via `*-sys` wrappers around C libs) try to `dlopen libclang.so` at build time and fail.

**Fix in the kpkgbuild's `build()`:**
```bash
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
```
Affects: niri, librsvg, anything pulling pipewire-sys, Wayland-rs, etc.

### CMake 4.x — "Compatibility with CMake < 3.5 has been removed"

KDOS ships CMake 4.2.1. Many older projects declare `cmake_minimum_required(VERSION 2.8)` or similar.

**Fix:** add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` to the `cmake` invocation. Affects: libsndfile, double-conversion, and likely anything pre-2020.

### GCC 15 — incompatible-pointer-types as error

Older C code (e.g. libndp 1.9) passes `struct sockaddr_in6 *` to `sendto()` instead of casting. Pre-GCC-14 was lenient; GCC 15 errors.

**Fix:** `export CFLAGS="$CFLAGS -Wno-incompatible-pointer-types"` at the start of `build()`.

### libxkbcommon SONAME unresolved at runtime

Symptom: `Error loading shared library libxkbcommon.so.0` when running anything that links it. Caused by meson defaulting to `--prefix=/usr/local --libdir=lib64`.

**Fix:** every meson `setup` call must include `--prefix=/usr --libdir=lib`. Audit any new meson-based port for this. The default landing path `/usr/local/lib64` is NOT in the runtime linker's search path inside the chroot.

### Qt 6 — "Unknown platform linux-g++"

Qt's `QtMkspecHelpers.cmake` derives `QT_MKSPECS_DIR` as `${cmake_dir}/../mkspecs`. cmake configs go to `/usr/lib/cmake/Qt6/`, so it expects mkspecs at `/usr/lib/cmake/mkspecs/` — but Qt's default `INSTALL_MKSPECSDIR=mkspecs` puts them at `/usr/mkspecs/`.

**Fix:** in qt6-base, `-DINSTALL_MKSPECSDIR=lib/cmake/mkspecs`. Already applied; if you see this error, check that flag.

### Qt 6 — "CMake was not built with zstd support"

KDOS's cmake doesn't have zstd archiving. Qt's archiving API needs it.

**Fix:** `-DQT_AVOID_CMAKE_ARCHIVING_API=ON`. Also `-DQT_GENERATE_SBOM=OFF` to skip downstream SBOM lookup chain.

### libunwind — undefined `__unw_getcontext` / `__libunwind_Registers_x86_64_jumpto`

KDOS's libunwind port builds without the assembly files (`UnwindRegistersSave.S`, `UnwindRegistersRestore.S`). Symbols defined in those .S files aren't in the .so. Linker (when called with `-Wl,--no-undefined`) errors when ICU's libicuuc (C++) pulls libunwind transitively.

**Workaround in consumer kpkgbuilds:** `export LDFLAGS="$LDFLAGS -Wl,--allow-shlib-undefined"`. The runtime ld.so still resolves these via gcc's other libs.

**Real fix (TODO):** rebuild `ports/core/libunwind` with the assembly enabled. Touched by gstreamer, imv, anything linking ICU-via-C++.

### ICU split — `ubrk_*` symbols missing at link

`libicuio.pc` doesn't propagate `icu-uc` as a transitive dep, so projects linking only `icu-io` miss `ubrk_*` (text break iteration, in `libicuuc`).

**Fix:** `export LDFLAGS="$LDFLAGS -licuuc"` in the consumer's `build()`.

### gstreamer — `gst-ptp-helper` rust link fails

PTP precision time protocol helper is a Rust binary that fails on static-musl rust + libunwind.

**Fix:** `-Dptp-helper=disabled` in gstreamer's meson setup.

### kpkg `install -f` doesn't actually force rebuild

**Known bug:** `kpkg install -f <pkg>` first calls `kpkgdepends`, which short-circuits installed packages BEFORE the `-f` flag kicks in. So a force-install on an already-installed package becomes a no-op.

**Workaround:** when you need to rebuild a port whose package is already installed:
```bash
sudo chroot build/fs /usr/bin/kpkgdel <pkg>
sudo rm -f build/fs/var/cache/kpkg/packages/<pkg>-*.tar.xz
make build  # will rebuild from kpkgbuild
```

### URL / version landmines

- **GNOME mirror is stale for some projects.** NetworkManager's GNOME mirror only has up to 1.5.91; current 1.56.x is on `gitlab.freedesktop.org/NetworkManager/NetworkManager/-/archive/<ver>/...`.
- **GitHub releases vs archives.** Some projects upload tarball assets to releases (use `releases/download/<tag>/<file>`); others rely on auto-generated archives (`archive/refs/tags/<tag>.tar.gz`). When `make fetch` 404s, check the actual release page or use `curl -sL | grep` on the API.
- **GitLab `/releases/<v>/downloads/` URLs require manually-attached files** (libdisplay-info hit this). Use `/-/archive/<v>/<name>-<v>.tar.bz2` for the auto-generated tarball — always works.
- **sourcehut archives via `git.sr.ht`** are sometimes blocked or rate-limited. GitHub mirrors of the same project (when available) are more reliable.
- **Tarball top-level dir naming** — GitHub `archive` produces `<repo>-<tag>/`, GitLab `archive` produces `<project>-<tag>/`, sourcehut produces variants. Some projects (libnsgif from NetSurf) use a custom `-src` suffix. Always verify `tar -tzf | head -1` if the build complains about source-not-found in `$SRC`.

### Meson option-type confusion

Meson options come in three flavors that frequently get conflated:
- **boolean** — value is `true`/`false`
- **feature** — value is `enabled`/`disabled`/`auto`
- **combo** — value is one of an explicit list (often `yes`/`no` or other domain words)
- **string** — value is a path or arbitrary text; empty string `''` typically disables

When meson errors with "Value 'foo' for option 'bar' is not one of the choices", check the upstream `meson_options.txt` for the real type. Fetch via:
```bash
curl -sL "https://api.github.com/repos/<org>/<repo>/contents/meson_options.txt?ref=<tag>" | python3 -c "import json,sys,base64;d=json.load(sys.stdin);print(base64.b64decode(d['content']).decode())"
```

---

## Adding a new port — recipes

### Standard meson port

```bash
name=foo
version=1.2.3
release=1
source="https://example.org/foo-$version.tar.xz"

build() {
    meson setup build \
        --prefix=/usr --sysconfdir=/etc --libdir=lib \
        --buildtype=release \
        -Dtests=disabled -Ddocs=disabled
    meson compile -C build
    DESTDIR=$PKG meson install --no-rebuild -C build
}
```

### Standard cmake port

```bash
build() {
    mkdir -p build && cd build
    cmake .. \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DBUILD_SHARED_LIBS=ON \
        -DBUILD_TESTING=OFF
    make
    make DESTDIR=$PKG install
}
```

### Standard autotools port

```bash
build() {
    ./configure \
        --prefix=/usr \
        --sysconfdir=/etc \
        --libdir=/usr/lib \
        --disable-static
    make
    make DESTDIR=$PKG install
}
```

### Rust port (with cargo vendor)

```bash
source="https://github.com/.../v$version.tar.gz"
vendoring=rust

build() {
    tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
    export CARGO_HOME="$SRC_ROOT/.cargo"
    export RUSTFLAGS="-C target-feature=-crt-static"
    export LIBCLANG_PATH=/usr/lib
    export CARGO_NET_OFFLINE=true
    cargo build --release --frozen --offline
    install -Dm755 target/release/$name "$PKG/usr/bin/$name"
}
```

---

## Networking architecture

**Stack from the wire up:**
- Kernel: WireGuard built-in, nftables in tree
- Userspace: `wpa_supplicant` (Wi-Fi WPA), `nftables` (firewall), `dnsmasq` (hotspot DNS), `dbus` (system bus)
- D-Bus glue without systemd: `basu` provides sd-bus
- Manager: `NetworkManager` 1.56 with `polkit=true`, `nft=/usr/sbin/nft`, `dnsmasq=/usr/sbin/dnsmasq`, `iptables=` (empty/disabled), no modemmanager, no PPP, no team driver, no OVS, internal DHCP backend, gnutls crypto
- Auth: `polkit` (with `duktape` JS engine, `authfw=shadow` since no PAM), `wheel` group has admin privileges
- VPN: `openvpn` + `networkmanager-openvpn` plugin (PKCS#11 hardware tokens off; standard cert/password/TOTP works)

**At runtime:**
- `45_seatd.sh` starts seatd at boot
- NetworkManager started... actually currently no init script for NM (TODO if not added). User runs `nmcli` or noctalia widget.

---

## Init scripts pattern

`fs/etc/init.d/` follows a numeric-prefix convention:
```
01_udev.sh   05_hostname.sh   10_sysctl.sh   20_dmesg.sh   30_network.sh
40_dbus.sh   45_seatd.sh      50_alsa.sh     60_bluetooth.sh   70_sshd.sh   80_cups.sh
rcS          service_helper
```

Each script sources `service_helper` (which provides `supervise`, `stop_service`, `check_status`):

```bash
#!/bin/bash
. /etc/init.d/service_helper

NAME="foo"
DAEMON="/usr/bin/foo-daemon"

case "$1" in
    start)
        [ ! -x "$DAEMON" ] && { echo "[SKIP] $NAME: $DAEMON not found"; exit 0; }
        echo "[KDOS] Starting $NAME..."
        supervise "$NAME" "$DAEMON --foreground"
        ;;
    stop)   stop_service "$NAME" ;;
    status) check_status "$NAME" ;;
    *)      echo "Usage: $0 {start|stop|status}"; exit 1 ;;
esac
```

`supervise` runs the daemon under a respawn loop and writes `/run/<name>.pid`. The daemon must run **in the foreground** — no daemonization.

---

## User preferences (memory)

- **Minimal kpkgbuild comments** — banner header + `# description` / `# homepage` / `# depends` only. No multi-line rationale blocks. User strips them.
- **No source edits using sed/awk** - use build flags when ever possible, use sed to patch source only when absolutely necessary.
- **Don't auto-commit.** Wait for explicit instruction.
- **Inline execution preferred** in this environment — subagents in this harness can't run bash, so dispatch-and-review patterns waste time. Just do the work.
- **Be terse in responses.** No trailing summaries unless asked. State what changed; user reads the diff.

---

## Working-state markers

When in doubt about current state, run:
```bash
ls ports/core | wc -l                # current port count
git status --short | wc -l            # tracked changes
ls build/fs/var/lib/kpkg/db/          # what's installed in the build chroot
ls build/logs/04_phase4/*.log         # which packages have build logs
tail -40 build/logs/04_phase4/<N>_<pkg>.install.log   # debug a failure
```

The user typically pastes a build log path when something fails — read its tail and respond with the targeted fix.

---

## Outstanding gaps / TODO (pre-existing)

Things known-broken or known-incomplete, not necessarily my job to fix unless asked:

- `libunwind` port lacks assembly files → undefined symbols at link time. Workaround: `--allow-shlib-undefined` per consumer.
- `kpkg install -f` doesn't truly force rebuild; user must `kpkgdel` + clear cache manually.
- No `make build`-time check that all `# depends` resolve (orphan refs only fail at runtime).
- No init script for NetworkManager itself (just the daemon binary).
- No firewall rules shipped — `nftables` is installed but `/etc/nftables.conf` empty by default.
- No proper user creation — installed system has root only; useradd works but no first-boot wizard.

---

## When the user says...

- **"fix"** + a build log path → read its tail, identify the root cause, apply the targeted fix in the relevant kpkgbuild. Don't speculate beyond what the log shows.
- **"audit"** → grep across kpkgbuilds, packages.txt, and fs/ for residual references. Be systematic.
- **"add X"** → find the canonical upstream URL (github API works; gitlab.freedesktop.org is anubis-blocked from WebFetch), pick latest stable version, write a kpkgbuild matching the patterns above, add to depends and packages.txt as needed.
- **"why is X off"** → it's likely because deps weren't present when I added the port. User wants to know if it's a deliberate choice or a stub. Be honest: name the missing deps, offer to add them.
