# KDOS — Claude Onboarding

This file briefs a fresh Claude session on KDOS so it can be useful immediately. Read it before doing anything substantive.

---

## What KDOS is

A hand-built Linux distribution following Linux From Scratch principles. **musl libc** + **toybox** userland on the host, **no SystemD**, **Wayland-only** (no Xorg server). Desktop: **COSMIC** (System76 — Rust/smithay/iced; compositor, panel, launcher, settings, notifications), pinned at one epoch release across all components. **No GTK and no Qt on the host** — the old noctalia-era Qt 6 carve-out is gone. Everything "fat" runs in a Podman/distrobox glibc rootfs. Session entry: `kdos-session` from a tty.

Mascot lives at `kdos.png`. Default wallpaper at `fs/usr/share/backgrounds/kdos/default-wallpaper.png` (penguin, green Matrix glow).

---

## Hard rules — do not violate

1. **No SystemD.** No `systemd-*` packages on the host. Replacements: `seatd` for seat management, `basu` for sd-bus, `eudev` for udev, `dbus` (not dbus-broker), `dnsmasq` (not systemd-resolved), `wpa_supplicant`/`NetworkManager` (not systemd-networkd).
2. **No Xorg server.** No `xorg-server`, no display manager, nothing X on the login path. **Xwayland is the one carve-out** (added 2026-07-27): `xwayland`, run rootlessly by cosmic-comp itself, so X11-only alien apps work. That pulled in the client chain `xorgproto xtrans libXau libXdmcp xcb-proto libxcb libX11 libxkbfile xkbcomp libxshmfence libfontenc libXfont2 xcb-util{,-image,-renderutil,-cursor}` — these exist **only** to satisfy Xwayland/xkbcomp. A kpkgbuild that wants X for anything else still gets pushed back. Mesa stays `-Dglx=disabled -Dplatforms=wayland`, so Xwayland is built `-Dglx=false`: X clients get no OpenGL. Enabling it means rebuilding mesa with `glx=dri,platforms=wayland,x11` plus libXext/libXfixes/libXdamage/libXrandr/libXxf86vm — not done.
3. **No GTK and no Qt on the host.** GUI apps go in distrobox via `kdos-fetch-app`. COSMIC's iced toolkit needs neither; the noctalia-era Qt 6 carve-out is history.
4. **No `kpkgbuild` rationale comments.** User strips multi-line comment blocks. Keep kpkgbuilds minimal — preserve only the banner header and one-line `# description` / `# homepage` / `# depends`. No "we set X because Y" prose. Belongs in commit messages or this file, not in the kpkgbuild.
5. **Do not auto-commit.** User commits manually, often squashing many edits into one logical commit. Wait for explicit "commit this" requests.
6. **Do not run destructive `git` ops without asking.** No `reset --hard`, `clean -f`, `branch -D`, force pushes.

---

## The Three Rings

| Ring | Lives in | Purpose |
|---|---|---|
| **Core** | `ports/core/` | Hand-compiled host packages (musl + toybox + libs + WM stack) |
| **GUI sliver** | also `ports/core/` (separate `packages.txt` block) | COSMIC desktop (19 `cosmic-*`/`pop-launcher` Rust ports), foot, Wayland CLI utils |
| **Outer ring** | distrobox containers | Browsers, IDEs, Slack, GIMP — full glibc apps |

There used to be a **`src/packages/`** namespace for vendored upstream forks (Window Maker era). It's currently empty/removed but the build pipeline still supports it via `PORT_REPO="/ports/core /kdos/src/packages"`. Use it if you ever need to vendor a project to apply local patches without `.patch` files.

### The baked appbox (offline alien apps)

The outer ring ships pre-baked: `ports/appbox/Containerfile` defines a
**debian trixie** `kdos-apps` image — the best-of open-source GUI app per
segment (Awesome-Linux-Software sweep; ~90 launchers: libreoffice, calibre,
gimp, krita, blender, freecad, prusa-slicer, openscad, kicad+gtkwave+ngspice,
octave, maxima/wxmaxima, stellarium, ardour, hydrogen, lmms, kdenlive,
obs-studio, vscodium (upstream .deb — not in debian), wireshark, keepassxc,
timeshift-style backup via deja-dup/testdisk/grsync, games
(supertux/supertuxkart/wesnoth/openttd/luanti/gnome classics), emulators
(retroarch+libretro cores, dosbox, mgba, scummvm), firefox-esr, …). Debian
replaced alpine when the list outgrew it: alpine has no slicer, no VSCode
build, no calibre/gtkwave in stable. Heavy is deliberate — the image IS the
offline software library (Knoppix-style fat stick). `--no-install-recommends`
everywhere with the data packages that matter re-added explicitly
(kicad-packages3d's 5 GB stays out). `/.containersetupdone` pre-baked so
distrobox-init never apt-gets anything at first enter.
The launchers in `fs/etc/skel/.local/share/applications/` are GENERATED from
the image's own desktop entries (`ports/appbox/genlaunchers.py` parses [Desktop Entry], skips
NoDisplay/noise, renames to the ids the dock favorites reference) — regenerate
rather than hand-edit when the app set changes. Because
`make build` runs `--network none`, the image is built on the HOST with
`make fetch-apps` → `ports/appbox/appbox.tar` (gitignored, over LFS's 2G/file
limit) which `ports/appbox/pack` immediately explodes into
**`ports/appbox/image/`**: one zstd file per docker-archive member (layer
blobs split at 1.5G), all LFS-tracked, plus `INDEX.json`. That directory and
`icons/` ARE committed — the repo alone must build the full ISO. The
Containerfile is one `RUN` per segment, so a segment edit only changes that
layer's blob in git. `script/06_packaging/01_appbox.sh` loads whichever
exists: the tar directly, else `ports/appbox/assemble` streams the tar out of
the chunks straight into `podman load` (no temp file). A missing image is a
warning, the ISO builds without it. No container is created at build time:
the launchers in `fs/etc/skel/.local/share/applications/kdos-*.desktop` call
`kdos-appbox run <app>`, which creates the distrobox lazily — and
`kdos-session` backgrounds `kdos-appbox warmup` at login (flock-guarded, also
serialized against `run`'s create), so container init normally happens while
the desktop is still settling and the first app click hits a warm box. The
Containerfile pre-installs distrobox's runtime prerequisites — without them
the first `distrobox enter` apt-installs from the network, which would defeat
the point. Packaging-phase snapshots exclude
`fs/home/kdos/.local/share/containers/*`.

Appbox runtime plumbing that cost a debug cycle each: debian's games live in
`/usr/games`, which distrobox's inherited host PATH lacks — `10-wayland.sh`
appends it or every game launcher dies on "not found". Audio and OBS screen
capture come from the HOST side: `kdos-session` execs (inside the session
bus) `kdos-session-start`, which brings up pipewire + pipewire-media-session
+ pipewire-pulse and then execs cosmic-session; the xdg-desktop portals are
D-Bus-activated on demand, and OBS's capture source is portal→ScreenCast→
pipewire, with the pulse/pipewire/bus sockets reaching the box via the
shared `/run/user/1000`. The box side needs debian's `obs-plugins` package —
debian splits OBS's plugins out as a Recommends, and `linux-pipewire.so`
(the only Wayland capture path) lives there; the recommends-less install
silently dropped it, exactly like kdenlive's ffmpeg/dvdauthor chain.

Three bake-side traps, each cost a debug cycle: (1) the bake now WIPES
`$STORAGE` before loading and the uid remap is idempotent — re-baking onto
an existing store used to remap already-remapped uids (clamped to 165535)
and every `distrobox enter` died with `crun: readlink ''`; (2) the loaded
image is FLATTENED to one layer (rootful `podman create`+`export`+`import`)
— the rootful unpack records whiteout/opaque markers as `trusted.overlay.*`
which the ROOTLESS runtime mount cannot see, so multi-layer-rebuilt dirs
like `/etc/alternatives` came up empty in the box (that emptied OBS's whole
encoder list via the dangling libblas alternatives symlink); (3) the
`xdg-desktop-portal` main daemon snapshots its backends at startup —
`kdos-session-start` waits for the compositor socket, pushes
WAYLAND_DISPLAY into the D-Bus activation environment, starts
`xdg-desktop-portal-cosmic`, WAITS for it to own its bus name, and only
then (re)starts the main portal, or ScreenCast stays empty all session.

**The session bus is one daemon per user at `$XDG_RUNTIME_DIR/bus`** —
kdos-session starts (or reuses) it and exports the address itself; it does
NOT use dbus-run-session. dbus-run-session listens on `unix:tmpdir=/tmp`, a
pathname socket in the host's /tmp — which the appbox does not share, so
every alien app saw a dangling DBUS_SESSION_BUS_ADDRESS: GApplication
single-instance broke (each impatient launcher re-click spawned another FULL
instance — the "apps sometimes don't open" report), dconf/a11y stalled, and
notifications went nowhere. `/run/user/1000` IS shared with the box, so the
one fixed address works on both sides (verified: box apps reach
org.freedesktop.Notifications, second gimp click hands off in 0.3s). Two
traps encoded in kdos-session: the address must carry NO guid (a second
daemon rebinding the socket kills zbus clients — cosmic-session aborts with
"Server GUID mismatch"), and never add a second `<listen>` via dbus config
instead — multi-address envs hit the same zbus crash. `10-wayland.sh`
exports the address for ssh/tty shells when the socket exists.
`kdos-appbox run` also: waits out an in-flight login warmup (entering while
distrobox-init is mid-setup execs into a half-built user), fires a
"Starting <app>" notification on cold starts (gdbus — no libnotify on host),
launches apps with `GSETTINGS_BACKEND=keyfile NO_AT_BRIDGE=1 GTK_A11Y=none`
(no dconf-service or a11y stack is reachable in the box), and self-heals a
container wedged in "stopping" (a D-state app hang on fuse-overlayfs can
survive SIGKILL; the container then sticks in stopping and every
`distrobox enter` dies instantly with "container state improper" — reproduced;
wait → `podman kill` → `podman rm -f`, the box is stateless so recreation
loses nothing).

**Rootless storage: fuse-overlayfs on live, native overlay when installed.**
/etc/containers/storage.conf pins `mount_program = fuse-overlayfs` and the
live ISO NEEDS it: $HOME sits on the boot overlay and the kernel refuses to
stack an overlay upperdir on overlayfs — podman does NOT fall back, the
container just fails to mount `merged/` (verified). On an ext4 install the
kernel overlay is much faster than FUSE, so `kdos-appbox` writes a one-time
`~/.config/containers/storage.conf` (driver=overlay, no mount_program) when
$HOME's fs is ext4/btrfs/xfs — only while the store has no containers yet:
fuse and native write incompatible whiteout formats into container rw
layers, so the choice must never flip afterwards.

### Icons, the shell's memory, and the two QEMU flavours

- **`/usr/share/icons/hicolor/index.theme` must exist** or every icon lookup in
  Qt/Quickshell silently fails (pink missing-texture checkerboards). The
  `hicolor-icon-theme` port provides it and lives in phase-4 packages.txt — it
  sat in ports/ uninstalled for months. Appbox app icons are installed into the
  SYSTEM hicolor tree by `01_appbox.sh` (contexts flattened to `apps/`); a
  user-dir icon tree without its own index.theme is not searched.
- **`make run` (plain virtio-vga, no virgl) currently has NO desktop**: smithay
  refuses software EGL renderers ("software EGL renderers are skipped" → "no
  allocator available"), so the compositor (niri then, cosmic-comp now — same
  library) gets no outputs. Only `make run-hw` (containerized Ubuntu-qemu 10,
  virgl+blob — its machine string is "pc-i440fx-noble-v2", easy to mistake for
  the host qemu) shows the desktop.
- (Historical, noctalia era: quickshell RSS plateaued ~0.8–1.3 GB from QML JS
  heap; solved by dropping the stack. If a COSMIC component ever balloons,
  the same debug method applies.)
- Debug rig for all of the above: boot the ISO headless with `-serial unix:` +
  `-monitor unix:` sockets, sendkey `kdos-session` on tty1. **Use
  `-display egl-headless -vnc :19`**: the gtk,gl=es window path can wedge the
  guest in a soft-lockup storm once cosmic-comp starts (KDOS_QEMU_DISPLAY
  overrides it in run.sh), screendump says "no surface" under GL either way,
  and cosmic-comp has NO wlr-screencopy so grim fails — capture by reading the
  VNC framebuffer raw (RFB handshake + Raw encoding), or cosmic-screenshot
  in-session. Escape-only and space-only tty writes don't end fbcon's deferral.

### Console font: kdos-getty, not rcS

fbcon is built with deferred takeover; the takeover re-initialises every VT
with the kernel's built-in font, which lacks λ (maps it to `l`) and double
box-drawing — so a setfont in rcS is silently wiped. `/usr/local/sbin/kdos-getty`
(wrapping both gettys in `fs/etc/inittab`) forces the takeover, loads the font
and palette verified, then execs getty. Hard-won facts baked into that script:

- **Only a real glyph ends the deferral.** Escape sequences are eaten by the VT
  state machine and even *spaces* are skipped by the render path — the wrapper
  prints `K` and clears it. Verified: `' '` → nothing, `'K'` → takeover.
- The takeover is scheduled work: the wrapper polls dmesg for
  `fbcon: Taking over console`, then retries setfont until `showconsolefont -i`
  confirms 16x32.
- **The font is `ter-kdos32n`** — built in the terminus-font port: the v/xos4-2
  512-glyph charset (has λ) with six spacing diacritics (¤ ¦ ¨ ¸ ¯ ˝) swapped
  for `═ ║ ╔ ╗ ╚ ╝`, which xos4-2 lacks and the block logo needs.
- Palette (`setvtrgb`) loads before the final clear, or the screen ends up
  half pure-black, half phosphor-black.
- It traces to `/run/kdos-getty.<tty>.log` for `kdos doctor`-style debugging.

Do not move font/palette setup back into rcS.

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
│   │   ├── skel/              # ~/.config/cosmic/*, foot, btop, starship seeds
│   │   ├── kpkg.conf          # runtime kpkg config (matches src/kpkg/kpkg.conf)
│   │   └── X11/               # DOES NOT EXIST — Wayland-only
│   ├── usr/local/bin/
│   │   ├── kdos-fetch-app     # distrobox + distrobox-export wrapper
│   │   ├── kdos-fetch-static  # curl + sha256 + chmod
│   │   └── kdos-session       # user bus at $XDG_RUNTIME_DIR/bus → cosmic-session
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
from phase3 and `cross`/`mark` from phase1. Restoring a phase that has no snapshot of its
own is refused rather than silently falling back to an older one.

`[S]` during a build takes a *partial* snapshot of the running phase. Restoring one of
those re-runs that phase instead of skipping it (kpkg skips already-installed packages),
and the picker marks the row `done/total!`.

If a restore is interrupted, `build/.restore-in-progress` remains and both snapshotting
and the next build refuse to run until you restore again or `make cleanbuild`. Snapshot
outcomes — including failures — are appended to `build/logs/snapshots.log`.

Non-interactive equivalents:

```bash
make build BUILD_ARGS="--restore phase2"   # restore phase2, continue at phase3
make build BUILD_ARGS="--continue-from phase3"  # resume at phase3 on the CURRENT tree,
                                           # no restore; earlier phases are skipped and
                                           # their snapshots left untouched
make build BUILD_ARGS=--fresh              # skip the picker, run everything
make build BUILD_ARGS=--no-snapshot        # throwaway run, write no snapshots
make snapshots                             # list them (build.py --list)
make cleanbuild                            # wipe build/ but KEEP build/snapshots
```

`make clean` deletes snapshots along with everything else. Snapshots need `zstd` and GNU
`tar` in the build image (both in the Dockerfile). Budget ~2-4G per phase.

---

## Build plans — iterating on an existing build/ tree

Snapshots answer "go back"; the **build plan** answers "re-run just this". It never
restores anything: it narrows what the next run executes on the tree you already have.

Press **`[P]`** in the startup picker, or drive it from the command line:

```bash
# changed something under fs/ -> re-sync it and rebuild the ISO, nothing else
make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"

# changed a kpkgbuild -> rebuild that port and repackage, no manual kpkgdel
make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild cosmic-comp,cosmic-panel"

make build BUILD_ARGS=--plan            # interactive: phase/step checkboxes, '/' searches ports
```

In the picker: `SPACE` toggles, `←/→` expands a phase into its scripts, `/` opens a fuzzy
port search with multi-select, `A`/`N` select all/none. The choice is remembered in
`build/.devplan.json` and pre-selected next time.

Three things make this safe, and they are the reason the plumbing exists:

- **Snapshots are suppressed** whenever a plan narrows execution (override with
  `--snapshot`). Re-running phase 1 on a phase-5 tree would otherwise file that tree under
  phase 1's name — the trap documented above.
- **`kpkg install -f` genuinely forces now.** It used to be a no-op: `kpkgdepends` filters
  installed packages *before* the force check, so the package never reached the loop.
  It now resolves against an empty db and rebuilds only the packages named on the command
  line — dependencies keep the normal skip-if-installed behaviour. Because of that,
  `build.py` passes `-f` **only** for ports the plan selected; passing it blanketly (as it
  used to) would now rebuild all ~300 packages.
- **Mark-file guards stand down for a step you picked deliberately.** 17 scripts start with
  `if [ -f "$MARK/x" ] && [ "${KDOS_REPLAY:-0}" != "1" ]`. build.py exports `KDOS_REPLAY=1`
  for explicitly named steps (chroot_exec.sh forwards it), so `--steps 01_phase1:12_kpkg.sh`
  actually re-installs kpkg instead of exiting 0.

`00_file_system.sh` **merges** `/etc/{passwd,group,shadow}` instead of overwriting them:
package postinstalls add service users (polkitd, messagebus, sshd) long after phase 1 runs,
and a plain `cp` on a re-sync would silently delete them. Repo entries win; runtime-added
entries are appended back.

**Don't re-run an early phase on a tree that is already ahead of it** — its snapshot would
be overwritten with a tree containing later phases' packages, under the earlier phase's
name. That is what `--continue-from` exists for: it resumes mid-build without re-running
(and therefore without re-snapshotting) anything behind it.

**Anything a chroot command prints is parsed.** `kpkgdepends` writes the install order to
stdout and nothing else, so `chroot_exec.sh` logs its diagnostics to `build/logs/chroot.log`
rather than stdout/stderr. build.py reads stdout only and validates every token against
`^[A-Za-z0-9][A-Za-z0-9._+-]*$`; noise fails expansion loudly instead of being installed
as a package.

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
Affects: the cosmic-* ports, librsvg, anything pulling pipewire-sys, Wayland-rs, etc.

### CMake 4.x — "Compatibility with CMake < 3.5 has been removed"

KDOS ships CMake 4.2.1. Many older projects declare `cmake_minimum_required(VERSION 2.8)` or similar.

**Fix:** add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` to the `cmake` invocation. Affects: libsndfile, double-conversion, and likely anything pre-2020.

### GCC 15 — incompatible-pointer-types as error

Older C code (e.g. libndp 1.9) passes `struct sockaddr_in6 *` to `sendto()` instead of casting. Pre-GCC-14 was lenient; GCC 15 errors.

**Fix:** `export CFLAGS="$CFLAGS -Wno-incompatible-pointer-types"` at the start of `build()`.

### libxkbcommon SONAME unresolved at runtime

Symptom: `Error loading shared library libxkbcommon.so.0` when running anything that links it. Caused by meson defaulting to `--prefix=/usr/local --libdir=lib64`.

**Fix:** every meson `setup` call must include `--prefix=/usr --libdir=lib`. Audit any new meson-based port for this. The default landing path `/usr/local/lib64` is NOT in the runtime linker's search path inside the chroot.

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

### Desktop theming — PHOSPHOR on COSMIC

COSMIC reads layered RON config: `/usr/share/cosmic` (system defaults) then
`~/.config/cosmic` (user). KDOS seeds the user side from
`fs/etc/skel/.config/cosmic/`:

| File (under `~/.config/cosmic/`) | Purpose |
|---|---|
| `com.system76.CosmicBackground/v1/all` | wallpaper → the penguin |
| `com.system76.CosmicTk/v1/icon_theme` | `"KDOS"` (kdos-icons) |
| `com.system76.CosmicPanel.Panel/v1/*` | floating top panel: phosphor bg Color, opacity 0.92, radius 12, trimmed wings (no a11y/input-sources applets), `keep_style_on_maximize` true (else a maximized window snaps the panel to edge-to-edge theme-default styling) |
| `com.system76.CosmicPanel.Dock/v1/*` | dock: phosphor bg Color, size M, `keep_style_on_maximize` true, `plugins_center` without the Launcher/App buttons (workspaces + app list + minimize only) |
| `com.system76.CosmicAppList/v1/favorites` | dock pins that actually exist: foot, CosmicFiles, kdos-firefox, kdos-mousepad, kdos-gimp, CosmicSettings (stock favorites are CosmicTerm/Edit/Store → gear placeholders) |
| `com.system76.CosmicAppLibrary/v1/groups` | KDOS launcher groups: Internet, Graphics, Office, Media, Engineering, Science, System, Utilities — Categories-driven (`AppGroup` RON; field docs in upstream `app_group.rs` are swapped — `exclude` excludes, `include` force-includes) |

The applied theme itself (accent, hover states, container colors) is NOT a
skel seed: `com.system76.CosmicTheme.Dark/v2/` is generated at packaging time
by `script/06_packaging/00_theme.sh` running **kdos-theme-helper**
(src/packages — drives cosmic-theme's own ThemeBuilder; the Theme struct is
`#[version = 2]`, hand-seeded v1 files are silently ignored).

**`kdos theme <phosphor|amber|ice|bone>`** owns everything else: it reruns
kdos-theme-helper, writes the panel+dock background Color RON
(`write_panel_colors`), `~/.config/foot/themes/kdos`,
`~/.config/btop/themes/kdos.theme`, and the palette block between the
`# >>> KDOS STARSHIP PALETTE >>>` markers in starship.toml. One palette table
inside `fs/usr/local/bin/kdos` drives all generators. COSMIC and cosmic-panel
repaint live; starship on next prompt; foot and btop on next start (foot
cannot reload its config, and KDOS's pkill has no `-x`).

Known quirk: a live `kdos theme` switch makes cosmic-panel restart its applets,
and the respawned cosmic-app-list collapses all pinned favorites into its
overflow button (panel-size renegotiation race; upstream 1.4 behaviour —
restarting cosmic-panel does not heal it, the next login does; the pins are
still there, behind the overflow button).

**kdos-icons** (`src/packages/kdos-icons`): theme `KDOS`, Inherits=Cosmic.
`recolor.py` hue-shifts every non-symbolic Cosmic places/categories/devices/
mimetypes SVG (plus the two hicolor panel-button SVGs) to the phosphor hue at
build time; symbolic icons stay untouched (the toolkit tints them from the
active theme). Ships `distributor-logo-kdos` / `start-here` (256px penguin),
and the dock's app-library button is the tux: the kpkgbuild deletes the
recolored `com.system76.CosmicPanelAppButton.svg` and installs the penguin
PNG at `256x256/apps/` instead (theme-internal lookup beats hicolor's SVG).
Depends on cosmic-icons being installed in the chroot at build.

**kdos-cursors** (`src/packages/kdos-cursors`): pixel-art phosphor cursor
theme, `gencursors.py` writes Xcursor binaries directly (raw ARGB, python
stdlib only — no xcursorgen, no image libs; alpha kept 0/255 so premultiply
never matters). 22 shapes covering cosmic-comp's whole `cursor_aliases`
table (resize arrows included — without them the "default" theme has no
resize shapes and cosmic-comp falls back to its built-in arrow on window
borders), animated wait/progress, aliases as symlinks. Sizes 24–96
(4K/HiDPI-ready), upscaled through a scale2x/EPX chain so big sizes read
crisp rather than blocky, with a pale outer rim (outside-connected
transparency only — interior holes stay open) so the dark outline is
visible on dark surfaces. Installed to
`/usr/share/icons/KDOS-cursors` AND `/etc/skel/.icons/` — distrobox apps
share `$HOME` but not `/usr/share/icons`, so the home copy is what the box
sees. Selected via `XCURSOR_THEME`/`XCURSOR_SIZE` in
`fs/etc/profile.d/10-wayland.sh` (cosmic-comp/winit/Qt) and
`fs/etc/skel/.config/gtk-{3,4}.0/settings.ini` (GTK apps in the box).

The default wallpaper carries a baked CRT treatment (scanlines every 3rd row
+ vignette, imagemagick multiply — regenerate from a clean render if
replaced). The niri-era CRT window shaders are gone — cosmic-comp has no
custom-shader API. The CRT identity lives in the boot splash, the wallpaper,
the TTY, and the palette.

### The boot splash

`src/packages/kdos-splash/` — the first thing in `src/packages/`, which exists precisely
for code that is ours rather than an upstream tarball (`source=""`; `kpkg` skips the
extract loop and `build()` compiles out of `$PORT_SRC`). Static C, ~700 lines, drawing the
CRT power-on straight to `/dev/fb0` with glyphs scaled up from the shipped Terminus PSF.

**Why the screen was blank before.** `console=tty0 console=ttyS0` — the LAST `console=`
becomes `/dev/console`, so every message the initramfs prints goes to the serial port, and
the display gets nothing until agetty starts. That is also what makes the splash possible:
the kernel has `CONFIG_FRAMEBUFFER_CONSOLE_DEFERRED_TAKEOVER`, so fbcon does not claim the
framebuffer while nothing prints to tty0.

**Three things cost a debug cycle each:**

- **Deferred take-over means nothing is being scanned out.** Writing to `/dev/fb0` before
  the DRM fbdev client does its modeset paints a buffer nobody is looking at — the display
  still shows what rEFInd left in the EFI framebuffer. (Tell-tale: the screen holds a flat
  colour taken from the boot banner's corner pixel.) One byte to `/dev/tty0` ends the
  deferral and makes the fbdev buffer the live scanout; `console_claim()` writes
  `ESC[2J ESC[H ESC[?25l` to do that and hide the cursor in the same breath.
- **mmap writes need an explicit flush.** They reach the host only when fbdev's deferred-IO
  worker gets round to it, so each frame ends with `FBIOPAN_DISPLAY` to the offset it is
  already at. Without it the animation runs at the kernel's whim, not at the frame rate.
- **The process survives `switch_root` with a ghost root.** It is never chroot'ed, so
  afterwards its `/` is still the (now deleted) initramfs root whose `/dev` was moved away.
  Open fds keep working — that is the whole trick, one process spanning both halves of boot
  with the FIFO on devtmpfs — but *every path it resolves by name after that points into
  the ghost*. `unlink("/dev/.kdos-splash")` from the daemon silently fails. Hence the
  `quit` client detects the daemon by opening the FIFO for writing and watching for ENXIO
  (no reader), and does the cleanup itself from the real root.

`quit` is deliberately synchronous: rcS returns from it and init starts agetty immediately,
and agetty printing to tty1 during the power-off animation interleaves console text with
splash pixels.

Adding a stage is one line either side: `sp_step "NAME"` / `sp_ok` in the generated init in
`script/06_packaging/01_initramfs.sh`, or `splash step "NAME"` / `splash ok` in
`fs/etc/init.d/rcS` (which already wraps every `init.d` script automatically).

The layout is centered as a block on any resolution (the status column is a
fixed-width LINE_COLS+6 field — never anchor it at a screen percentage, that
only looked centered at 1280). The progress bar is fed by `kdos-splash total
N`, *additive*: each boot phase adds its own step count as soon as it knows
it (initramfs common part sends 2, the live/disk branch adds its 4/3, rcS
adds 2 + enabled-service count). Until the first total arrives the bar plays
a KITT sweep; pending stages show an amber rotor; idle adds a hum bar
drifting down the raster. When adding stages, keep the nearby `sp_total`
count in step.

Iterate on the look without booting: `kdos-splash preview 1280x800 0.35 out.ppm` renders a
single frame at a given intro time to a PPM.

### The `kdos` command

`fs/usr/local/bin/kdos` is the front door: `help` (commands + COSMIC keybind
cheat sheet), `theme`, `status`, `doctor`, `app`, `version`. `kdos-shot` and the older
`kdos-fetch-app` / `kdos-fetch-static` sit alongside it.

`kdos doctor` checks the things that have actually broken on this distro before —
including `readlink /proc/self/root`, the switch_root trap below.

### initramfs must use util-linux `switch_root`, never toybox's

toybox `switch_root` wipes the initramfs and `chroot()`s — it never does `mount(newroot, "/", MS_MOVE)`. The mount-namespace root then stays the *emptied* rootfs with the real root parked at `/newroot`, and anything that JOINS a mount namespace via `setns(CLONE_NEWNS)` (`podman exec`, `distrobox enter`, `nsenter -m`) gets that empty rootfs as `/` → every path ENOENT (`crun: executable file 'echo' not found`). `podman run` still works, because it CREATES the namespace. Tell-tale: `readlink /proc/<container-pid>/root` prints `/newroot` instead of `/`. `script/06_packaging/01_initramfs.sh` installs `/usr/sbin/switch_root` over the toybox symlink — keep it that way.

### /usr/local prefix bug — now also bites via stale .pc files

The known trap (a meson port without `--prefix=/usr` landing in `/usr/local`) has a second
edge: **the stale `/usr/local/**/pkgconfig/*.pc` shadows the fixed one.** xkeyboard-config
had this; after fixing its prefix and rebuilding, libxkbcommon still baked
`/usr/local/share/xkeyboard-config-2` because the old `.pc` was still on the pkg-config
path, and the compositor then panicked with `BadKeymap` at startup. When you fix a prefix, delete the
old files *and* the old `.pc`, then rebuild the consumers. Still outstanding:
`libinput` installs to `/usr/local/lib64` (port has no `--prefix`), it works only because
its consumers were linked against that path.

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
- NetworkManager started... actually currently no init script for NM (TODO if not added). User runs `nmcli` or the COSMIC network applet.

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
- No first-boot wizard. The system ships a fixed `kdos:kdos` user (uid 1000, `wheel`) declared in `fs/etc/{passwd,group,shadow}`; homes are materialized from `/etc/skel` by `script/06_packaging/00_user.sh`, `/run/user/<uid>` by `fs/etc/init.d/15_userdirs.sh`. Renaming/adding users is manual.

---

## When the user says...

- **"fix"** + a build log path → read its tail, identify the root cause, apply the targeted fix in the relevant kpkgbuild. Don't speculate beyond what the log shows.
- **"audit"** → grep across kpkgbuilds, packages.txt, and fs/ for residual references. Be systematic.
- **"add X"** → find the canonical upstream URL (github API works; gitlab.freedesktop.org is anubis-blocked from WebFetch), pick latest stable version, write a kpkgbuild matching the patterns above, add to depends and packages.txt as needed.
- **"why is X off"** → it's likely because deps weren't present when I added the port. User wants to know if it's a deliberate choice or a stub. Be honest: name the missing deps, offer to add them.
