<p align="center">
  <img src="kdos.png" alt="KDOS Mascot" width="300"/>
</p>

<p align="center"><b>KDOS</b> — a handmade, artisan-crafted Linux distribution built from the absolute ground up, following the sacred texts of <b>Linux From Scratch</b>.</p>

<p align="center"><i>Powered by <b>musl libc</b> and <b>toybox</b>. Wayland-only. No SystemD.<br>The digital equivalent of a katana: minimal, sharp, and lightweight.</i></p>

---

## The Philosophy

Most distros give you a house. KDOS gives you a pile of bricks, a trowel, and a blueprint written in Bash.

1.  **LFS Roots** — every byte of the OS is hand-compiled. If we didn't compile it, we don't trust it.
2.  **Musl Libc** — Glibc is a metropolitan city. Musl is a Zen garden. We choose the garden.
3.  **Toybox** — one binary that rules them all.
4.  **No SystemD** — we init like our ancestors did. `inittab`, serial `init.d`, simple service helpers.
5.  **No X11** — Wayland-only. No Xorg server, no XWayland by default. Last century's display protocol stays in last century.
6.  **No GTK / GNOME / KDE / XFCE on the host** — fat GUI apps live in a Podman container alongside their glibc. *Qt 6 is the single carved-out exception, present only because the shell layer (Quickshell + noctalia) is QML.*
7.  **KPKG** — our own package manager. For fun, and because we earned it.

---

## The Three Rings

KDOS organizes itself into three concentric rings, each with a distinct philosophy:

| Ring | What lives here | Backed by |
|------|-----------------|-----------|
| **Core** | musl, toybox, kpkg, kernel, init scripts | hand-compiled, in `ports/core/` |
| **GUI sliver** | niri compositor, noctalia shell, Wayland CLI utils | hand-compiled, Wayland-native |
| **Outer ring** | Browsers, IDEs, Slack, GIMP, Steam — the fat stuff | distrobox + glibc rootfs (Debian by default) |

---

## The Core: musl + toybox + kpkg

The base system is hand-built phase-by-phase via `script/build.py`, which orchestrates a Docker container that bootstraps a cross toolchain (Phase 0), builds the musl userland (Phase 1), then walks through expansion phases until the rootfs is complete. Every package is a `kpkgbuild` recipe under `ports/core/<name>/`.

Package management is **kpkg** — a small Bash-based pacman/pkgsrc analog:

```sh
kpkg install foo       # build + install from ports
kpkgadd  foo.tar.xz    # install a pre-built package
kpkgdel  foo           # remove
kpkgdepends foo        # show resolved dependency tree
```

`kpkgbuild` recipes are sourced shell — they declare `name`, `version`, `release`, `source`, and a `build()` function. No DSL, no manifests, no opinions you can't override with a `vim`.

---

## The GUI Sliver: niri + noctalia (Wayland)

To start a graphical session from a tty login:

```sh
niri-session
```

That wraps `niri --session` in `dbus-run-session`. The skel config (`~/.config/niri/config.kdl`) auto-spawns the shell on session start.

**The compositor stack:**

| Layer | What it is |
|-------|-----------|
| `niri` | Wayland compositor — Smithay-based, scrollable-tiling, Rust |
| `noctalia-shell` | Desktop shell — bars, panels, notifications, lock screen, widgets (QML) |
| `noctalia-qs` | Quickshell fork that runs noctalia's QML (Qt 6) |
| `seatd` | Seat manager — auto-started by `/etc/init.d/45_seatd.sh` |
| `xdg-desktop-portal` + `-wlr` | Portal layer — screencast, file picker, no GTK |
| `basu` | sd-bus library extracted from systemd (no-systemd D-Bus glue for portals) |
| `/etc/profile.d/10-wayland.sh` | Sets `XDG_RUNTIME_DIR`, `QT_QPA_PLATFORM=wayland`, etc. on every login |

**Companion CLI utils** (kept tiny, all Wayland-native):

- `foot` — terminal emulator (the daily driver; `Mod+Return`)
- `fuzzel` — keyboard launcher (`Mod+D`)
- `grim` + `slurp` — screenshot + region selector
- `wl-clipboard` — `wl-copy` / `wl-paste`
- `imv` — image viewer with SVG and animated GIF support

**Multimedia:** PipeWire 1.6 with Bluetooth (`bluez5`) and ffmpeg-backed file playback; PulseAudio compat shim built-in (apps that link against libpulse work transparently). Full GStreamer 1.28 stack (base/good/bad/ugly/libav).

**Networking + auth:** NetworkManager 1.56 + `wpa_supplicant` for Wi-Fi, `polkit` for non-root connection management, `nftables` for firewall, `dnsmasq` for hotspot mode, `openvpn` + NM-openvpn plugin for VPN. UPower for battery/power widgets.

Anything else (file manager, editor, calculator) is done in a terminal — `lf`, `nvim`, `bc`. Anything fatter is one `kdos-fetch-app` away.

---

## The Outer Ring: Alien Apps via Distrobox

KDOS deliberately does not native-port browsers, IDEs, office suites, video editors, or chat clients. Those are the things upstream `apt`, `dnf`, and `pacman` already package well — we don't need to relitigate that.

Instead, KDOS ships **Podman + distrobox**. On first boot you get a default `kdos-debian` distrobox (Debian glibc rootfs), and the helper:

```sh
kdos-fetch-app firefox        # apt-installs firefox in the box, exports it as a host launcher
kdos-fetch-app gimp           # same for gimp
kdos-fetch-app --remove gimp  # remove the export and uninstall in the box
```

A `.desktop` file lands in `~/.local/share/applications/` — `fuzzel` and `noctalia` pick it up automatically. The host musl tree never sees a single glibc dep.

For single-binary tools (Zig, Go binaries, single-binary Rust apps), there's also:

```sh
kdos-fetch-static <name> <url> <sha256>
```

— curl + sha256-verify + chmod into `/usr/local/bin`. The most suckless answer for "I just want this one tool."

---

## Building KDOS

The full build runs inside a sandboxed Docker container — no contamination of the host, fully reproducible from clean.

```sh
make fetch    # download all upstream tarballs (LFS storage)
make build    # bootstrap toolchain → build everything → produce ISO
make run      # boot the resulting ISO in QEMU/KVM (KVM-accelerated)
make rundisk  # boot from the persisted disk image instead of the ISO
make clean    # nuke the build/ tree
```

**Build artifacts:**
- `build/iso-build/kdos.iso` — bootable installer ISO
- `build/fs/` — the populated rootfs (chroot-able for inspection)
- `build/kdos.qcow2` — persistent disk image used by `make rundisk`

A clean build takes 2–4 hours depending on hardware (Qt 6 alone is ~30 min on 8 cores; gstreamer adds ~15 min). Incremental rebuilds are fast — the build system marks completion of each phase and skips re-doing finished work.

---

## Repository Layout

```
kdos/
├── ports/core/<name>/        # upstream packages — kpkgbuild + tarball
├── src/                      # KDOS-authored tools (kpkg, kinstall)
├── fs/                       # files copied verbatim into the rootfs
│   ├── etc/                  # inittab, init.d/, profile, profile.d/, skel/
│   ├── usr/local/bin/        # kdos-fetch-app, kdos-fetch-static, niri-session
│   └── usr/share/            # backgrounds, branding
├── script/                   # phase-by-phase build orchestrator (Python TUI)
├── testing/                  # standalone per-port build tests
├── Dockerfile                # Alpine sandbox the build runs inside
└── Makefile                  # entry point — fetch / build / run
```

---

## Hardware

- **CPU:** x86_64 with KVM virtualization for `make run`
- **RAM:** 4 GB minimum to build comfortably (8 GB recommended for parallel cargo)
- **Disk:** ~30 GB free for build artifacts; final ISO is ~2 GB

QEMU virtio-vga is the default tested guest target — KDOS boots cleanly under QEMU/KVM with `make run` straight from the ISO. Bare-metal install is supported via the `kinstall` curses installer, but the QEMU path is the well-trodden one.

---

## License

MIT. Go forth and segfault.
