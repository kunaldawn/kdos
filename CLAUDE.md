# KDOS — Claude Onboarding

Briefing for a fresh session. Read before doing anything substantive.
This file records the **current** state and the reasoning that constrains it —
not the project's history. If something here reads like a rule, it is one:
almost every "never do X" is a bug that already cost a debug cycle.

---

## What KDOS is

A hand-built Linux distribution following Linux From Scratch principles, for
experienced Linux users. Tagline: **"I use KDOS btw."**

**musl libc** + **toybox** userland, **no systemd**, **Wayland-only** (no Xorg
server). **No GTK and no Qt on the host.** Everything fat runs in a
Podman/distrobox glibc rootfs. Session entry: `kdos-desktop` from a tty.

> **The desktop is ours; the compositor is a hard fork.** COSMIC was removed
> and replaced by `kdos-comp` — since 2026-08-14 a **frozen, rebranded hard
> fork of labwc 0.20.0** carrying the KDOS features as `src/kdos-*.c` grafts
> (CRT pass, wallpaper, frames socket, idle policy, supervised chrome) — plus
> `kdos-shell`, a character-cell-grid shell drawn by libktui, with
> `kdos-lock`, `kdos-powerd` and `kdos-boxsock` beside them.
> **`docs/KDOS-DESKTOP.md` is the plan of record and carries a status block
> per milestone**, `docs/KDOS-ROADMAP.md` the full menu of work,
> `docs/KDE-ON-HOST-REJECTED.md` the alternative that lost.

Four properties define the project. Everything else follows from them:

1. **Built from scratch.** Every host byte is compiled here from an upstream
   tarball by a `kpkgbuild` recipe in `ports/`. 405 ports.
2. **KDOS can build KDOS.** Phase 2 is a self-hosting bootstrap — the chroot
   rebuilds tar/musl/zlib/binutils/gcc with itself. The shipped system carries
   gcc, binutils, rust, cmake, meson, ninja, python3, make and `kpkg`, so a
   running KDOS can rebuild every port in the tree.
3. **The repo builds offline.** `make build` runs `--network none`. Every
   upstream tarball, every `cargo vendor` bundle and the whole ~4 GB alien-app
   container image live in-tree under Git LFS. Clone, `make build`, get an ISO,
   no network.
4. **Alien apps.** **KDOS builds the desktop; applications live in boxes.**
   No native-porting of browsers, office suites or IDEs — ~105 GUI apps ship
   pre-baked in a Debian container and behave like ordinary system apps
   (launchers, MIME handlers, terminal commands). The boundary is now in the
   same place as the build cost, with no per-app arguments.

Mascot: `kdos.png`. Wallpaper: `fs/usr/share/backgrounds/kdos/default-wallpaper.png`.

---

## Hard rules — do not violate

1. **No systemd.** No `systemd-*` on the host. Replacements: `seatd` (seat
   management), `basu` (sd-bus), `eudev` (udev), `dbus` (not dbus-broker),
   `dnsmasq` (not systemd-resolved), `wpa_supplicant`/`NetworkManager` (not
   systemd-networkd).
2. **No Xorg server.** No `xorg-server`, no display manager, nothing X on the
   login path. **Xwayland is the one carve-out**, run rootlessly by kdos-comp
   itself so X11-only alien apps work. It pulled in the client chain
   `xorgproto xtrans libXau libXdmcp xcb-proto libxcb libX11 libxkbfile xkbcomp
   libxshmfence libfontenc libXfont2 xcb-util{,-image,-renderutil,-cursor}` —
   those exist **only** to satisfy Xwayland/xkbcomp. A kpkgbuild that wants X
   for anything else gets pushed back. Mesa is `-Dglx=disabled
   -Dplatforms=wayland`, so Xwayland is built `-Dglx=false`: X clients get no
   OpenGL. Enabling it means rebuilding mesa with `glx=dri,platforms=wayland,x11`
   plus libXext/libXfixes/libXdamage/libXrandr/libXxf86vm — not done.
3. **No GTK and no Qt on the host.** GUI apps go in a container. The desktop
   is `kdos-comp` + `kdos-shell`, drawn by libktui, which needs neither.
   `libcanberra`, if it ever lands, builds `--disable-gtk`.
4. **No `kpkgbuild` rationale comments.** Banner header plus one-line
   `# description` / `# homepage` / `# depends`. No "we set X because Y" prose —
   that belongs in a commit message or in this file.
5. **Do not auto-commit.** The user commits manually, often squashing many
   edits into one logical commit.
6. **Do not run destructive `git` ops without asking.** No `reset --hard`,
   `clean -f`, `branch -D`, force pushes.
7. **No source edits with sed/awk.** Use build flags. Patch only when there is
   genuinely no flag, and then ship a real `.patch` beside the kpkgbuild.
8. **Be terse in responses.** State what changed; the user reads the diff.

---

## The Three Rings

| Ring | Lives in | Purpose |
|---|---|---|
| **Core** | `ports/core/` | Hand-compiled host packages (musl + toybox + libs + toolchain) |
| **Desktop** | `ports/core/` (`wlroots`) + `src/desktop/` (ours) | `kdos-comp`, `kdos-shell`, `kdos-lock` — see `docs/KDOS-DESKTOP.md`. Plus foot and the Wayland CLI utils |
| **Outer ring** | the baked appbox container | Browsers, IDEs, office, media, CAD, games — full glibc apps |

**`src/packages/`** is the second port repo (`PORT_REPO="/ports/core
/kdos/src/packages"`) and holds what is OURS rather than an upstream tarball:
`kdos-splash`, `kdos-appbox`, `kdos-installer`, `kdos-kpkg`, `kdos-theme`,
`kdos-tools`, and the three vendored-and-remade art packages `kdos-cursors`
(Bibata), `kdos-icons` (Papirus), `kdos-gtk-theme` (adw-gtk3). The desktop's own
packages live one directory over, in `src/desktop/`.

The three art packages share one shape, and new vendoring should copy it: a
host-only `vendor.py` that prunes upstream into a committed `art/`/`theme/`
tree, plus a `gen*.py` that the build **and** `kdos theme` run to recolour it.
That keeps the palette a KDOS decision and avoids carrying `.patch` files
against artwork.

---

## The appbox — offline alien apps

`ports/appbox/Containerfile` defines a **debian trixie** `kdos-apps` image:
the best open-source GUI app per segment, ~105 launchers (libreoffice, calibre,
gimp, krita, blender, freecad, prusa-slicer, openscad, kicad+gtkwave+ngspice,
octave, maxima, stellarium, ardour, hydrogen, lmms, kdenlive, obs-studio,
vscodium, wireshark, keepassxc, backup tools, games, emulators, wine,
firefox-esr, …),
plus a **KDE segment** — dolphin, konsole, kate, okular, gwenview, ark, kcalc,
spectacle, digikam, elisa, kdeconnect, kdevelop, k3b, filelight, kwalletmanager.
Not `kde-full`: that is Plasma itself plus everything and would roughly double a
~4 G image. The applications are the half with no equal elsewhere; none of them
needs Plasma running. `plasma-integration` comes with them, which is what makes
`QT_QPA_PLATFORMTHEME=kde` work at all, and `kio-extras`/`ffmpegthumbs`/
`kdegraphics-thumbnailers` are what make dolphin show thumbnails rather than
generic icons.
Debian rather than alpine because alpine has no slicer, no VSCode build, no
calibre/gtkwave in stable. Heavy is deliberate — **the image IS the offline
software library** (Knoppix-style fat stick). `--no-install-recommends`
everywhere, with the data packages that matter re-added explicitly
(kicad-packages3d's 5 GB stays out). `/.containersetupdone` is pre-baked so
distrobox-init never apt-gets anything on first enter.

### How the image ships in-tree

`make build` runs `--network none`, so the image is built on the HOST with
`make fetch-apps` → `ports/appbox/appbox.tar` (gitignored, over LFS's 2G/file
limit), which `kdos-appbox image pack` immediately explodes into
**`ports/appbox/image/`**: one zstd file per docker-archive member (layer blobs
split at 1.5 G), all LFS-tracked, plus `INDEX.json`. That directory and
`icons/` ARE committed — the repo alone must build the full ISO. The
Containerfile is one `RUN` per segment, so editing a segment only rewrites that
layer's blob in git. `script/06_packaging/01_appbox.sh` loads the tar directly
if present, else `kdos-appbox image assemble` streams it out of the chunks into
`podman load`. A missing image is a warning; the ISO still builds.

No container is created at build time. Launchers call `kdos-appbox run <app>`,
which creates the distrobox lazily, and `kdos-desktop` backgrounds
`kdos-appbox warmup` at login (flock-guarded, serialized against `run`'s
create), so container init normally happens while the desktop is still settling.

### Bake-time traps (each cost a debug cycle)

- **Drop root's runtime paths from the user's libpod database.** The bake runs
  rootful podman with `--runroot /tmp/appbox-runroot`, and podman RECORDS the
  runroot and tmpdir. Rootless podman then honours them, dies on
  `mkdir /run/libpod: permission denied`, and EVERY podman call fails — silently,
  because the launcher swallows stderr. Tell-tale: `podman images` as kdos fails
  while the same command as root works. `01_appbox.sh` removes `$STORAGE/libpod`
  and `db.sql` after the load; the first rootless call recreates them.
- **Wipe `$STORAGE` before loading**, and keep the uid remap idempotent.
  Re-baking onto an existing store used to remap already-remapped uids (clamped
  at 165535) and every `distrobox enter` died with `crun: readlink ''`.
- **Flatten the loaded image to one layer** (rootful `podman create` + `export`
  + `import`). The rootful unpack records whiteout/opaque markers as
  `trusted.overlay.*`, which the ROOTLESS runtime mount cannot see, so
  multi-layer-rebuilt dirs like `/etc/alternatives` come up EMPTY in the box.
- **Packaging snapshots exclude** `fs/home/kdos/.local/share/containers/*`.

### `kdos-appbox genlaunchers` writes four things

`kdos-appbox genlaunchers <desktop-dir> <fs-root>` parses the image's own
desktop entries. Dropping any one output breaks something visible:

| Output | Why |
|---|---|
| `etc/skel/.local/share/applications/<upstream-id>.desktop` | the launcher |
| `mimeinfo.cache` beside them | without it the `MimeType` lines are never consulted |
| `usr/share/kdos/alien-apps` | name → in-box command line |
| `usr/local/bin/<name>` → `kdos-appbox` | every alien app is also a normal command |

- **Some alien software is a COMMAND, not an application.** wine is the case:
  what you want is `wine setup.exe` at a prompt, and Debian's own entries for it
  are `NoDisplay=true`, which the parse correctly drops — so the box would carry
  wine and the host would have no way to reach it. The `COMMANDS` table gets an
  `alien-apps` row and a shim and deliberately NO `.desktop`, because a launcher
  for `wine` with no arguments opens nothing. Emitted only when the image really
  carries the binary, so an older bake does not gain a shim that dies on "not
  found".

- **Carry `MimeType` through from upstream.** Without it no alien app appears
  in any "Open with" dialog and none can be a default handler. The cache is
  written here rather than by `update-desktop-database` because the host has no
  desktop-file-utils.
- **The launcher FILENAME must be upstream's own desktop id** — not
  `kdos-<name>`, and not `StartupWMClass`. A dock matches a running toplevel to
  a desktop entry by the entry's FILE ID; COSMIC's app-list ignored
  `StartupWMClass` entirely and a mismatch showed a second grey cog beside the
  pinned icon. `kdos-shell` matches `app_id` first, then the desktop id, then
  `StartupWMClass` — but upstream's id is still the right filename, because it
  is the one every other tool agrees on.
  And **a Wayland `app_id` is not the X11 `WM_CLASS`**: GIMP's entry says
  `StartupWMClass=gimp-3.0` but its toplevel calls `set_app_id("gimp")` —
  measured with `WAYLAND_DEBUG=1`, not guessed. So dock favorites reference
  upstream ids too (`firefox-esr`, `org.xfce.mousepad`, `gimp`).
  `StartupWMClass` is still written — it costs nothing and is what an X11 app
  under Xwayland matches by.
- **Strip X11-forcing env prefixes** (`X11_FORCING`): debian ships audacity as
  `env GDK_BACKEND=x11 audacity`, which kills it under a compositor whose
  Xwayland the app cannot reach. Those apps run fine on pure Wayland.
- **`EXEC_EXTRA`** carries argv an app needs only because it is containerised.
  VSCodium is there because Electron's chrome-sandbox wants a setuid helper and
  CLONE_NEWUSER, gets neither as a non-root user in an unprivileged podman
  container, and exits rather than falling back:
  `--no-sandbox --ozone-platform-hint=auto --disable-gpu-compositing`.

Regenerating needs the image's `/usr/share/applications`. After an ISO build
that is already on disk (the bake flattens the appbox to one layer):
`build/fs/home/kdos/.local/share/containers/storage/overlay/*/diff/usr/share/applications`
— no `make fetch-apps` required.

### Runtime plumbing

- Debian's games live in `/usr/games`, which distrobox's inherited host PATH
  lacks — `10-wayland.sh` appends it or every game launcher dies on "not found".
- **X11-only apps need `DISPLAY` pushed in explicitly.** `kdos-comp` runs
  Xwayland rootlessly but exports DISPLAY only to what IT spawned;
  `kdos-appbox` probes `/tmp/.X11-unix/X*` (distrobox shares the host /tmp) and
  adds `DISPLAY=` to BOXENV.
- **Qt theming has two routes and BOTH are gated on an image label.** With the
  KDE segment the image carries `plasma-integration`, so `kdos-appbox` exports
  `QT_QPA_PLATFORMTHEME=kde` and every Qt app reads `~/.config/kdeglobals` —
  which `kdos theme` writes into the home the box already shares. That is the
  direct route and it needs no style override.
  Without it: `QT_QPA_PLATFORMTHEME=gtk3`, which is inert without debian's
  `qt{5,6}-gtk-platformtheme` (an appbox baked before those were added leaves
  every Qt app grey — a re-bake, not a config bug), plus
  `QT_STYLE_OVERRIDE=Fusion`, because the Breeze style kdenlive and shotcut pull
  in paints from its own colour scheme and ignores what qgtk3 hands it. Fusion
  with NO platform theme falls back to Qt's built-in LIGHT palette — worse than
  nothing — so both are gated on `kdos.qt-kde-theme` / `kdos.qt-gtk-theme`,
  which the Containerfile declares in the same layer that installs each. One
  `podman image inspect` per label per boot, cached in `$XDG_RUNTIME_DIR`
  (150 ms inspect vs a 300 ms warm launch; the image cannot change without a
  reboot).
- Audio and OBS screen capture come from the HOST: `kdos-desktop` execs
  `kdos-desktop-start` inside the session bus, which brings up pipewire +
  pipewire-media-session + pipewire-pulse and then execs `kdos-comp`. Portals
  are D-Bus-activated on demand; OBS captures via portal→ScreenCast→pipewire,
  with the sockets reaching the box through the shared `/run/user/1000`. The box
  needs debian's `obs-plugins` package — debian splits OBS's plugins out as a
  Recommends and `linux-pipewire.so` (the only Wayland capture path) lives there.
- **The `xdg-desktop-portal` main daemon snapshots its backends at startup.**
  `kdos-desktop-start` waits for the compositor socket, pushes WAYLAND_DISPLAY
  into the D-Bus activation environment, starts `xdg-desktop-portal-wlr`,
  WAITS for it to own its bus name, and only then (re)starts the main portal —
  or ScreenCast stays empty all session. Backend-independent, so the fix
  survived the move off COSMIC verbatim.
- **The zero-streams trap was COSMIC's, and the lesson outlives it.** OBS 30.2's
  `on_start_response_received_cb` does `if (n_streams != 1) for (size_t i = 0;
  i < n_streams - 1; i++)`: with `n_streams == 0` the unsigned subtraction wraps
  and it spins on an exhausted iterator at ~5 MB/s of log with the UI wedged
  (on the live ISO that log is tmpfs, so it eats RAM). A ScreenCast `Start` that
  answers Success with no streams must never happen; `Cancelled` is the answer.
  Debug notes that still apply: OBS's stdout is block-buffered when redirected
  (use `stdbuf -oL` or read `~/.config/obs-studio/logs/*.txt`), and the host has
  no `dbus-monitor` — run debian's from inside the appbox, it shares the session
  bus.

### `kdos-appbox` is a C program (`src/packages/kdos-appbox`)

`/usr/local/bin/kdos-appbox` is C: `main.c` (CLI + launch path), `box.c`
(boxes and profiles), `app.c` (app table, install/refresh), `tui.c` (the
front end), `launchers.c` (`genlaunchers`), `image.c` (`image
pack|assemble|remap-uids`), `open.c` (`open`), `util.c` (the trace file and the
notification).
It links
**libkbase + libktui + libkcolor + libkxdg and nothing else** — the process/path/lock
helpers `util.c` used to carry are libkbase's now, and `tui.c` is libktui, so
the `# depends : ncurses` line is gone. The TUI reads
`$XDG_CACHE_HOME/kdos/theme` and adopts the accent the desktop is wearing.

Two rules it exists to keep:

- **The launch path is a straight port, ordering included.** 91 launchers and
  the login warmup depend on its exact behaviour: the stuck-in-`stopping`
  recovery, the `container_setup_done` readiness wait that only runs when
  someone ELSE started the box, the fire-and-forget notification, the one-time
  storage-driver choice. Every one is a fix for something that broke. Comments
  marked *cost a debug cycle* mark them.
- **There is no `system()` and no shell anywhere in the program.** App names,
  package names and file arguments all arrive from .desktop files and argv; a
  shell in the middle turns any of them into an injection point. Everything
  execs through libkbase's `KbArgv` builder.

Invoked through a symlink named after an app, it dispatches on its own basename
busybox-style, so `gimp photo.png` works from a terminal with no shell wrapper.
The name → command table is `/usr/share/kdos/alien-apps` (baked) plus
`~/.local/share/kdos/alien-apps` (runtime); user entries win.

**`shared-mime-info` ships a database that has to be COMPILED on the target.**
The port builds `--disable-update-mimedb`, so the package contains
`packages/freedesktop.org.xml` and nothing else — no `globs`, no `types`, no
`mime.cache`. Measured on a booted ISO: `/usr/share/mime` held one directory,
and every consumer asking "what type is this file" got no answer at all.
`postinstall.sh` runs `update-mime-database /usr/share/mime`, which is a target
binary and therefore something only an install-time hook can do. Verified live:
before it, `kdos-appbox open --print /tmp/a.txt` fell through to `xdg-open`;
after it, the same file resolves to `nvim.desktop`.

**`kdos-appbox open <path>` is the MIME route, and it is here rather than in
xdg-open because this is already the program that knows what "open with GIMP"
means on this machine.** `kdos-desk` called it for a release before it existed,
so every double-click on the desktop died on "unknown". The resolution is the
freedesktop one and nothing clever: `/usr/share/mime/globs` for the type
(**longest matching suffix wins**, or every `.tar.gz` opens in a decompressor),
then `mimeapps.list` `[Default Applications]`, `[Added Associations]` and each
`applications/mimeinfo.cache` — which is the file `genlaunchers` already writes
beside the box's own launchers, so a boxed app is found by exactly the same
lookup as a host one. Field codes are SUBSTITUTED rather than stripped: `%f` IS
the file, and dropping it opens the application with an empty document.
`--print` resolves and prints instead of executing, which is what
`testing/selftest.sh` asserts on; `xdg-open` is the last resort, because it
knows about URL schemes this does not read.

**Sandbox profiles** live in `~/.config/kdos/boxes/<name>.conf` and every key
maps 1:1 onto a distrobox flag — `network`/`ipc`/`devices`/`processes` to
`--unshare-{netns,ipc,devsys,process}`, `home` to `--home`. That mapping is the
point: **KDOS does not offer confinement it cannot enforce.** Defaults are
exactly what a plain `distrobox create` does. A profile applies at CREATE time —
namespaces cannot be re-flagged on a live container, so changing one tells you
to run `kdos-appbox recreate <box>` rather than silently doing nothing.

**Rootless storage: fuse-overlayfs on live, native overlay when installed.**
`/etc/containers/storage.conf` pins `mount_program = fuse-overlayfs` and the
live ISO NEEDS it: $HOME sits on the boot overlay and the kernel refuses to
stack an overlay upperdir on overlayfs — podman does NOT fall back, the
container just fails to mount `merged/`. On an ext4 install the kernel overlay
is much faster, so `kdos-appbox` writes a one-time
`~/.config/containers/storage.conf` (driver=overlay, no mount_program) when
$HOME's fs is ext4/btrfs/xfs — **only while the store has no containers yet**:
fuse and native write incompatible whiteout formats into container rw layers,
so the choice must never flip afterwards.

Other launch-path facts: `kdos-appbox` waits out an in-flight login warmup
(entering while distrobox-init is mid-setup execs into a half-built user), but
**only** when the box is already running and `container_setup_done` has not yet
appeared in `podman logs` — a blind `flock -w 120` on the warmup lock made a
click wait for the entire container init, up to two minutes of dead-looking
desktop. The warmup runs at nice 10 so it actually finishes. `notify()` uses
`gdbus --timeout 2` and is backgrounded — gdbus's default reply timeout is **25
seconds**, and a notification must never gate a launch. Apps are launched with
`GSETTINGS_BACKEND=keyfile NO_AT_BRIDGE=1 GTK_A11Y=none` (no dconf-service or
a11y stack is reachable in the box).

**`GTK_USE_PORTAL=1` is what makes the KDOS portal reachable at all**, with
`XDG_CURRENT_DESKTOP=KDOS` beside it. A GTK app routes through the portal only
when it believes it is sandboxed, which it decides from `/.flatpak-info` or from
that variable — and a distrobox is neither. So FileChooser, Settings and OpenURI
all existed, all answered, and **nothing ever called them**: every boxed app
went on drawing its own GtkFileChooser, which is the exact rounded antialiased
dialog `kdos-pick` was written to replace. Firefox's
`widget.use-xdg-desktop-portal.file-picker` default of `auto` reads the same
variable, so it is covered too. Stated cost: GTK's Print dialog goes through
the portal as well and KDOS has no Print backend — but printing from a boxed app
did not work before either (there is no route to the host's cups socket), so
nothing that worked stops working.

Stage timings are appended to
`$XDG_RUNTIME_DIR/kdos-appbox.trace`. Measured: cold launch (no container at
all) **18.3 s**, warm **0.3 s**, second click **0.55 s**.

---

## Session, boot and console

### The session bus

**One daemon per user at `$XDG_RUNTIME_DIR/bus`.** `kdos-desktop` starts (or
reuses) it and exports the address itself; it does **not** use
`dbus-run-session`, which listens on `unix:tmpdir=/tmp` — a pathname socket in
the host's /tmp, which the appbox does not share. Every alien app then saw a
dangling `DBUS_SESSION_BUS_ADDRESS`: GApplication single-instance broke (each
impatient re-click spawned another FULL instance), dconf/a11y stalled, and
notifications went nowhere. `/run/user/1000` IS shared with the box, so one
fixed address works on both sides. Two traps encoded in `kdos-desktop`: the
address must carry **no guid** (a second daemon rebinding the socket kills zbus
clients — COSMIC's session aborted with "Server GUID mismatch"), and never add a
second `<listen>` via dbus config instead (multi-address envs hit the same zbus
crash). `10-wayland.sh` exports the address for ssh/tty shells when the socket
exists.

### `/tmp` must be mounted `mode=1777`

`defaults` gives a 0755 root tmpfs that HIDES the 1777 `/tmp` baked into the
image, so no non-root user can write to `/tmp` at all. Every alien app depends
on it (GTK/Wayland lock files, GIMP scratch, LibreOffice, fontconfig), and the
failure looks exactly like "the app is slow / never opens". Fixed in `fstab`
**and** with an explicit `chmod 1777 /tmp` after `mount -a` in `rcS`, because a
tmpfs that is already mounted ignores a mode change on remount —
`mount -o remount,mode=1777 /tmp` silently does nothing, only `chmod` works.

### `-march`, measured — `kdos march`

```
$ kdos march probe
  x86-64-v2    yes
  x86-64-v3    yes
  x86-64-v4    missing avx512f ...
highest usable: x86-64-v3
nothing is built with it until `kdos march run <port>` measures a win

$ kdos march run lz4
lz4  x86-64-v3  baseline 0.321s  -march=x86-64-v3 0.302s  +5.9% (noise 16.7%) -> reverted
       the win is inside the noise; that is not a win
```

**Gentoo optimises blind; CachyOS optimises by tier for a population; Clear
Linux, which did runtime dispatch properly, was shut down in July 2025 — and
musl closes that route anyway** (no glibc-hwcaps, IFUNC contested upstream).
That leaves rebuild-per-machine, which makes the question not "which flags" but
**"did they help HERE"**.

The published v3 numbers are why: flac +20%, vorbis +21%, zstd decompress +16%
— and bzip2 **−7%**, python **−3%**, lz4 **−2.9% with over 10% more power**. A
distro that shipped v3 everywhere would ship those regressions and never know.

So the tool builds each port twice on this machine, runs the port's own
benchmark against both, and keeps the flags only where the win clears BOTH a
fixed 3% floor and the machine's own measured noise. Four rules:

- **A port with no `bench =` line is UNMEASURABLE, never a winner.** Most ports
  have no meaningful benchmark, and assuming a win for them is the blind
  optimisation this replaces.
- **The median of several runs**, not the mean (which the worst outlier owns)
  and not one run (which measures the scheduler). `KDOS_MARCH_RUNS` raises it.
- **The noise floor is measured, not assumed** — it is the spread of the samples
  themselves, and a "win" smaller than it is the machine breathing. Both
  measurements here landed inside it and were reverted, which is the correct
  answer on a busy machine and the tool says so.
- **`bench_setup =` runs once and is not timed.** The first bench line written
  measured `dd if=/dev/urandom` more than it measured lz4 — 64 MiB out of the
  kernel RNG pushed the noise floor to 13.5%. A fixture belongs outside the
  stopwatch.

`kdos march report` is the ledger the roadmap asks for by name — kept, reverted,
unmeasurable, with the summary line. A report that listed only winners would be
a sales pitch; the reverts are the evidence that the measuring is real.

### The stick rebuilds the stick — `kdos rebuild`

```
make build KDOS_ISO_SOURCES=1     # a developer stick: ~2.7 G larger
...boot it...
kdos rebuild /mnt/disk/work       # no network at any point
```

Every leg of this is old — the LFS LiveCD shipped its sources in 2005, FreeBSD
has shipped `/usr/src` for thirty years — and what none of them does is **rebuild
the medium from the medium**. KDOS can because of three properties it already
had: the repo builds offline (every tarball is in `ports/`), KDOS can build KDOS
(the shipped system carries gcc, binutils, make, meson, ninja, python3 and kpkg),
and packages are reproducible, so a rebuild can be COMPARED to what it was built
from rather than merely produced.

**The sources go on the ISO9660 filesystem beside `system.sfs`, not inside it**,
so they cost the installed system nothing and are readable from `/mnt/iso` the
moment the live image is up. Opt-in, because `ports/` is 2.7 G of tarballs that
are already compressed and squashing them again buys nothing.

**`kdos rebuild` is checks plus `kdosbuild`, and the checks are the valuable
half.** A live stick's `/` is an overlay whose upper layer is tmpfs: a rebuild
started there reports gigabytes free, eats memory, and dies hours in with the
machine unusable. So the work directory is refused when it is on `tmpfs`,
`ramfs` or `overlay` — a free-space check cannot see that — and again when it has
less than 25 GB or when a build tool is missing. Everything after the checks is
the same orchestrator `make build` runs, compiled on demand out of the tree being
built (the `ports/fetch` shape) so the build is driven by the sources on the
machine rather than by a binary from somewhere else.

### A/B root slots — `kdos-bootctl`

Two root partitions, a state file on the ESP, and a boot that can change its
mind. The shape is RAUC's state machine with none of its dependencies:

```
slot_a   = <root uuid>
slot_b   = <root uuid>
active   = a          the slot known to work
try      = b          a candidate, or empty
attempts = 3          how many boots it gets
```

**The counting is ours and it lives in the INITRAMFS.** rEFInd has no boot
counting — that is a systemd-boot feature — and `rcS` is the wrong place
regardless: a kernel that boots into a wedged userland must still spend an
attempt, and the `rcS` in that userland never runs to say so. `kdos-bootctl
select` decides and decrements in one step, before anything is mounted, and
prints the UUID to boot. `kdos-bootctl mark-good` at the END of `rcS` — after
every service that was going to fail has had its chance — promotes a candidate
to active.

So a bad update boots three times and rolls itself back with no help from
anything, which is measurable: `testing/selftest.sh` runs `select` four times
without ever calling `mark-good` and requires the fourth to hand back the old
slot.

**The state file is on the ESP, which is FAT and has no journal.** A torn write
here does not fail an update, it bricks the machine — the initramfs cannot tell
which slot to boot. So every write is temp file, `fsync` the FILE, `fsync` the
DIRECTORY, then `rename`. The directory fsync is the step people leave out, and
without it the rename can be lost while the data survives.

**A state file that does not parse is ABSENT, never partial.** Half a file that
looked complete is exactly how a machine boots a slot that was never installed;
absent means "use the `root=` the command line already carries", which is what a
single-root machine does anyway. `try` pointing at a slot with no root, or at the
active slot, is refused rather than recorded.

kinstall writes the initial state (slot A active, slot B empty) and adds
`bootstate=UUID=<esp>` to the kernel options. Filling slot B is an updater's job
and the state machine is already complete for it. **A/B and LUKS are not wired
together**: the slot select yields a filesystem UUID and an encrypted slot's
filesystem lives inside a container, so combining them needs a per-slot
`cryptdevice=` and is not done.

### An encrypted root

`cryptdevice=UUID=<luks-uuid>:<name>` on the kernel command line, Arch's syntax
because it is the one already in people's heads. The generated init unlocks
before it looks for a filesystem, because the filesystem inside the container
does not exist until then — `root=UUID=` names the INNER filesystem and
`cryptdevice=` the container, and confusing the two is the whole trap.

Three things the prompt gets right, and each is a way this usually goes wrong:

- **It goes through the SPLASH, not `/dev/console`.** `console=` is ttyS0 on this
  kernel command line (the last one wins), so a plain `read -p` prompts a serial
  port nobody is looking at while the screen shows a splash that appears to have
  frozen.
- **The keystrokes come from `/dev/tty1`**, which is where the keyboard is.
- **The passphrase is fed to `cryptsetup --key-file=-` on stdin**, never as an
  argument: `/proc/<pid>/cmdline` is world-readable for the life of the process.

Three attempts, then a shell rather than a reboot loop. There is no
per-keystroke feedback: the splash owns the framebuffer and bash owns the
terminal, and a masked field with dots would mean moving the read into the
splash — stated rather than hidden.

`01_initramfs.sh` carries cryptsetup and its libraries only when it is installed,
and says so when it is not; a half-carried cryptsetup fails at the passphrase
prompt instead of at build time. `PASS_TTY` and `CRYPT_MAPPER_DIR` default to the
real thing and exist so `testing/selftest.sh` can run the unlock function against
stub tools with no LUKS volume and no root — the same trick
`kdos stutter --fixture` uses for `/proc`.

### initramfs must use util-linux `switch_root`, never toybox's

toybox `switch_root` wipes the initramfs and `chroot()`s — it never does
`mount(newroot, "/", MS_MOVE)`. The mount-namespace root then stays the
*emptied* rootfs with the real root parked at `/newroot`, and anything that
JOINS a mount namespace via `setns(CLONE_NEWNS)` (`podman exec`,
`distrobox enter`, `nsenter -m`) gets that empty rootfs as `/` → every path
ENOENT (`crun: executable file 'echo' not found`). `podman run` still works,
because it CREATES the namespace. Tell-tale: `readlink /proc/<pid>/root` prints
`/newroot`. `script/06_packaging/01_initramfs.sh` installs
`/usr/sbin/switch_root` over the toybox symlink — keep it that way.

### CPU microcode rides in front of the initramfs

`CONFIG_MICROCODE=y`, `CONFIG_MICROCODE_LATE_LOADING` **off** — so the early
loader is the only path there is, and it runs before any filesystem exists:
it scans the raw initrd for the literal paths
`kernel/x86/microcode/{GenuineIntel,AuthenticAMD}.bin`. Hence the shape:
`01_initramfs.sh` builds a plain cpio of those two files and the ISO's
`initramfs.cpio.gz` is that cpio **concatenated in front of** the gzipped
initramfs. Three properties it must keep:

- **Nothing in the microcode cpio may be compressed** — not the cpio and not
  the blobs. `linux-firmware` ships `amd-ucode/*.bin.zst` because the *runtime*
  firmware loader decompresses; the early loader cannot, so they are expanded
  on the way in.
- **`intel-ucode` is not curated.** It is upstream's whole `intel-ucode/`
  concatenated into one bundle by the port (17.7 MB), minus
  `intel-ucode-with-caveats/`, which upstream ships separately because it needs
  coordinated firmware support. A per-family prune boots on the machines it
  covers and leaves the rest silently unpatched, which is the state the errata
  are written about.
- **The Intel bundle must consume to exactly its last byte.** `scan_microcode()`
  ends `return size ? NULL : patch;` — one stray file in `intel-ucode/` means
  the kernel loads **nothing at all**, not "everything before the bad record".
  So `build.sh` walks the records the same way and fails the build instead;
  verified against a bundle with a README appended.

**16-byte alignment is a myth worth not chasing.** `iucode_tool --write-earlyfw`
inserts a dummy `.enuineIntel.align.0123456789abc` directory to pad the blob to
16 bytes, which looks load-bearing and is not: `lib/earlycpio.c` in the kernel
KDOS ships aligns headers to **4** and nothing else. Measured — our own cpio
lands the blob at offset 636 (`% 16 == 12`) and that is fine. With alignment
off the table the tool's only remaining gain was 2.5 MB of deduplicated
revisions — which `scan_microcode()` skips anyway, keeping the highest revision
it finds — so it is not a port.

`kdos doctor` runs the kernel's own search against `/boot/initramfs.cpio.gz`
and reports the running revision from
`/sys/devices/system/cpu/cpu0/microcode/version`. That is the check that
matters, because an initramfs rebuilt without the ucode step has no symptom —
the CPU simply keeps whatever the firmware loaded. `KDOS_INITRD` moves the
image so `testing/selftest.sh` can assert both answers.

### The boot splash

`src/packages/kdos-splash/` — static C, ~700 lines, drawing the CRT power-on
straight to `/dev/fb0` with glyphs scaled up from the shipped Terminus PSF.
(`source=""`; kpkg skips the extract loop and `build()` compiles out of
`$PORT_SRC`.)

Why the screen was blank before: `console=tty0 console=ttyS0` — the LAST
`console=` becomes `/dev/console`, so initramfs messages go to serial and the
display gets nothing until agetty. That is also what makes the splash possible:
the kernel has `CONFIG_FRAMEBUFFER_CONSOLE_DEFERRED_TAKEOVER`, so fbcon does not
claim the framebuffer while nothing prints to tty0.

Three facts, one debug cycle each:

- **Deferred take-over means nothing is being scanned out.** Writing to
  `/dev/fb0` before the DRM fbdev client does its modeset paints a buffer nobody
  is looking at (tell-tale: the screen holds a flat colour from rEFInd's
  framebuffer). One byte to `/dev/tty0` ends the deferral; `console_claim()`
  writes `ESC[2J ESC[H ESC[?25l` to do that and hide the cursor at once.
- **mmap writes need an explicit flush.** They reach the host only when fbdev's
  deferred-IO worker gets round to it, so each frame ends with `FBIOPAN_DISPLAY`
  to the offset it is already at.
- **The process survives `switch_root` with a ghost root.** It is never
  chroot'ed, so afterwards its `/` is the deleted initramfs root whose `/dev`
  was moved away. Open fds keep working — that is the whole trick, one process
  spanning both halves of boot with the FIFO on devtmpfs — but *every path it
  resolves by name after that points into the ghost*. Hence the `quit` client
  detects the daemon by opening the FIFO for writing and watching for ENXIO, and
  does the cleanup itself from the real root.

`quit` is deliberately synchronous: rcS returns from it and init starts agetty
immediately, and agetty printing to tty1 during the power-off animation would
interleave console text with splash pixels.

Adding a stage is one line either side: `sp_step "NAME"` / `sp_ok` in the
generated init in `01_initramfs.sh`, or `splash step "NAME"` / `splash ok` in
`fs/etc/init.d/rcS` (which already wraps every `init.d` script). The layout is
centered as a block on any resolution (the status column is a fixed-width
LINE_COLS+6 field — never anchor it at a screen percentage). The progress bar is
fed by `kdos-splash total N`, **additive**: each phase adds its own step count
as soon as it knows it (initramfs common part 2, live/disk branch 4/3, rcS 2 +
enabled services). Because it is additive, `done == total` happens at every
phase boundary, so the splash **clamps to 99% / one segment short** until `quit`
arrives, and the initramfs announces `BOOT MEDIA` *before* its 2 s device
settle. Keep both: a boot that shows 100% before it is finished is a bug report
waiting to happen. Iterate without booting:
`kdos-splash preview 1280x800 0.35 out.ppm`.

### Console font: `kdos-getty`, not `rcS`

fbcon is built with deferred takeover; the takeover re-initialises every VT with
the kernel's built-in font, which lacks λ and double box-drawing — so a setfont
in rcS is silently wiped. `/usr/local/sbin/kdos-getty` (wrapping both gettys in
`fs/etc/inittab`) forces the takeover, loads the font and palette verified, then
execs getty:

- **Only a real glyph ends the deferral.** Escape sequences are eaten by the VT
  state machine and even *spaces* are skipped by the render path. Verified:
  `' '` → nothing, `'K'` → takeover. The wrapper prints `K` and clears it.
- The takeover is scheduled work: poll dmesg for `fbcon: Taking over console`,
  then retry setfont until `showconsolefont -i` confirms 16x32.
- **The font is `ter-kdos32n`** — built in the terminus-font port: the v/xos4-2
  512-glyph charset (has λ) with six spacing diacritics swapped for
  `═ ║ ╔ ╗ ╚ ╝`, which xos4-2 lacks and the block logo needs.
- `setvtrgb` loads the palette before the final clear, or the screen ends up
  half pure-black, half phosphor-black.
- Traces to `/run/kdos-getty.<tty>.log`.

Do not move font/palette setup back into rcS.

### The login banner

`fs/usr/local/bin/kdos-banner` paints the banner one raster line at a time with
a bright beam leading the fill, then one frame of reverse video
(`ESC[?5h`/`ESC[?5l`) for the CRT thump. It bails to a plain print when stdout
is not a tty, `TERM` is dumb, `KDOS_NO_ANIM` is set, or the banner is taller
than the terminal; any keypress skips the rest. Delays come from bash's
`read -t`, not `sleep`, so a login costs zero extra forks per frame, and the
beam line is stripped of SGR sequences in pure bash (`tr -d '\033'` removes the
escape byte but leaves `[1;32m` visible as text).

**kdos-banner composes the banner itself; fastfetch is run with `--logo none`.**
Not a style choice. Asked to draw a logo, fastfetch prints the logo block, moves
the CURSOR BACK UP over it (`ESC[<n>A`) and writes each info line with an
absolute column jump. That output is not a sequence of raster lines, so
replaying it a line at a time — which is what any animation does — drifts one
row per line and draws the whole block TWICE, offset. So the script reads
`/usr/share/kdos/logo.txt` itself, measures each line's visible width with the
same pure-bash `strip_ansi`, and pastes the two columns together. Consequences
worth keeping: the logo can never come out missing, the info column no longer
has to be TALLER than the logo, and when `logo + gap + info` exceeds `$COLUMNS`
the script stacks them instead (a 1280x800 TTY with the 16x32 font is 80
columns). The config still declares a `logo` for a bare `fastfetch` typed at the
prompt; `--logo none` on the command line overrides it.

**The fastfetch keys are padded to 10 characters BY HAND.** `display.key.width`
looks like the right knob and is not: fastfetch places values at an ABSOLUTE
column, so a padded key runs past it and the value overwrites the separator
(`Packages 351`, `Alien app94`).

`fs/usr/share/kdos/logo.txt` is GENERATED by
`src/packages/kdos-splash/genlogo.py`, which decodes the mascot straight out of
`penguin.h` — the same quantised crop of `kdos.png` the boot splash draws — so
banner, splash and mascot cannot drift apart. Do not hand-edit it. Three
constraints are baked into that script: the TTY font has FULL BLOCK and the
double-line box characters but **no half blocks** (and ░ ▒ but no ▓ — grep
`uni/xos4-2.uni` before using any glyph), so one cell is one
solid block; character cells are twice as tall as wide, so the sampling grid
must be ~2× wider than tall or the penguin stretches; and the banner must stay
under ~30 lines or it scrolls off a 33-row TTY. Amber, the bright rim and the
eyes win their cell on a minority of pixels (`RIM_BIAS`) — a plain majority vote
erases every small feature.

---

## Theming — PHOSPHOR

**The desktop needs no theme file at all.** `kdos-comp` and `kdos-shell` link
libkcolor, so they carry the same `KCOL_SCHEMES` table `kdos theme` expands, and
they read one word — the accent NAME — from `$XDG_CACHE_HOME/kdos/theme`. No
colours are written for the desktop, and a running session is retinted by a
SIGHUP rather than by being handed a palette: the shell repaints its chrome, the
compositor re-reads the accent its CRT shader tints with. The state file is
written BEFORE the signal, or the session re-reads the accent it already had.

That is why `kdos-theme-helper` — Rust, and a dependency on cosmic-theme's
ThemeBuilder — could be deleted rather than ported: there was nothing left for it
to generate.

Everything else `kdos theme` writes exists for software that is NOT ours and
cannot be told: GTK and Qt apps in the appbox, foot, btop, starship.

**Alien apps are themed through `$HOME`** — the appbox shares the home directory
and nothing else, so `/usr/share/themes` and `/usr/share/icons` are invisible
inside the box:

| Path | Written by | Read by |
|---|---|---|
| `~/.themes/KDOS/` | `write_gtk` → `kdos-theme gtk` | GTK3 and non-libadwaita GTK4 apps |
| `~/.icons/KDOS/` | `write_icons` → `kdos-theme icons` | every toolkit, host and box |
| `~/.config/gtk-{3,4}.0/gtk.css` | `write_gtk` | libadwaita (which ignores themes entirely) |
| `~/.config/kdeglobals` | `write_kde` | every Qt app under `QT_QPA_PLATFORMTHEME=kde` |
| `~/.local/share/color-schemes/KDOS.colors` | `write_kde` | a KDE app's own colour picker |
| `~/.icons/KDOS-cursors/` | kdos-cursors' `/etc/skel` copy | cursor lookup in the box |

**`kdeglobals` is MERGED, never overwritten**, and that is the one thing about
it worth remembering. KDE apps write their own settings into that file —
dolphin's view modes, kate's session state — so `kdos theme` replaces only what
it owns (the `[Colors:*]` and `[WM]` sections outright, plus `ColorScheme`,
`Name`, `widgetStyle` and `Icons/Theme`) and keeps every other section verbatim.
The merge NORMALISES the file (sections in their original order, foreign keys
first, ours after, blank lines and comments dropped) because the first version
patched in place and moved `[KDE]` on every second run — `kdos theme` twice in a
row produced two different files and `--audit` had a permanent complaint. A
section that would hold only our keys goes at the end, in generator order,
wherever it sat before. Sections we own are replaced WHOLE: leaving a foreign
key inside `[Colors:Window]` made the file grow by one stale colour per run.

`--audit` copies the live `kdeglobals` into its scratch home before
regenerating, exactly as it already does for `starship.toml` — both are edited
rather than written, and auditing a merge against an empty home would call every
user's `[Dolphin]` section drift.

The packages install only the SYSTEM copies plus their generators; the home
copies come from `06_packaging/00_theme.sh` running `kdos theme phosphor` with
`HOME=/etc/skel`. One generator per artefact, and a live `kdos theme amber`
retints host and box together.

**The theme is a recoloured `adw-gtk3`, and choosing that specifically is the
fix.** Redefining `theme_*` in the user's gtk.css is enough for
GTK4/libadwaita, which genuinely resolves named colours, but **stock GTK3
Adwaita is compiled from SASS with literal hex in every rule**: the names reach
only the widgets that reference them. Hand-written CSS rules over Adwaita never
cover enough widgets, and blunt `*:selected` rules turn GIMP's Opacity/Size/
Spacing labels into solid green bars. `adw-gtk3` is the libadwaita stylesheet
ported to GTK3 and is written against named colours end to end (~125
`@define-color` at the top, almost nothing below), so
`kdos-theme gtk` rewrites the palette and every widget
follows — GTK3 apps end up genuinely identical to GTK4 ones. `vendor.py` prunes
the upstream release (dark variant only; `gtk.css` dropped as byte-identical to
`gtk-dark.css`; unreferenced assets dropped — the rest are neutral grey or
symbolic masks, so **no asset needs recolouring**). Fill accents use the DIM
accent (`pdark`), never full intensity: a 100 %-filled GIMP opacity slider in
`#39ff14` is an unreadable neon block.

Qt apps in the box follow via `QT_QPA_PLATFORMTHEME=gtk3` and `GTK_THEME=KDOS`
in `kdos-appbox`'s BOXENV plus debian's platform themes. GIMP needs
`(theme "System")` in `~/.config/GIMP/3.0/gimprc` (seeded in skel) or it keeps
its own grey theme. GTK reloads neither theme nor icons on a file change, so
alien apps pick up an accent switch on their next launch.

**`kdos theme <phosphor|amber|ice|bone>`** owns everything else: it regenerates
`~/.themes/KDOS`, `~/.icons/KDOS` and `~/.icons/KDOS-cursors`, writes the two
`gtk.css` files, `~/.config/foot/themes/kdos`, `~/.config/btop/themes/kdos.theme`
and the palette block between the `# >>> KDOS STARSHIP PALETTE >>>` markers in
starship.toml, then writes the accent NAME to `$XDG_CACHE_HOME/kdos/theme` and
SIGHUPs `kdos-shell` and `kdos-comp`. The state file is written BEFORE the signal,
or the session re-reads the accent it already had. Ours repaint live; starship on
the next prompt; foot and btop on next start (foot cannot reload its config at
all); GTK apps on their next launch, because GTK re-reads neither theme nor icons
on a file change.

**The SIGHUP goes to FOUR names, and `pkill -x` is load-bearing.**
`reload_session()` in `kdos.c` signals `kdos-shell`, `kdos-desk`,
`kdos-notifyd` and `kdos-comp`. The first three are argv[0]s of one binary, so
signalling `kdos-shell` alone retinted the panel and left the desktop icons and
any live toast in the old accent — photographed on a booted ISO. And the match
must be EXACT: `kdos-desk` is a substring of `kdos-desktop` and
`kdos-desktop-start`, the two `/bin/sh` scripts that own the session, and an
unhandled SIGHUP kills a shell. Verified live — `pgrep -fc kdos-desktop-start`
is 3 before and after two accent changes.

**Emphasis on a cell grid is a FILL plus swapped slots, never `KT_A_REVERSE`
over a label.** The attribute inverts only the cells the text covers, so the
focused taskbar entry came out as one lit block per WORD —
`▓GNU▓ ▓Image▓ ▓Mani▓` — and looked correct for every window whose name had no
space in it. The bottom panel always filled first; the top panel and the privacy
indicator do now.

**That SIGHUP only started arriving when the compositor stopped blocking it** —
see **The compositor is a hard fork of labwc** (signal lessons). In kdos-comp
itself, SIGHUP is labwc's Reconfigure and additionally calls
`kdos_crt_reload()`, and that half has worked since the fork.

**The panel half was a CRASH, not a repaint, and it took a measurement to see
it.** `kdos-shell` installed no SIGHUP handler at all (`SigCgt: 0` on a running
panel), so the signal did what an unhandled SIGHUP does: it KILLED the panel,
the compositor's supervisor respawned it, the new process read the state file,
and the desktop came up in the new accent — indistinguishable from a live
retint until you look at the pid. What made it a real bug rather than an ugly
one is `RESPAWN_MAX`: **five deaths in thirty seconds stops the respawn**, and
one `kdos theme` takes about a second, so trying the four accents to pick one
loses the panel for the rest of the session. Measured, on the live ISO — six
changes in twelve seconds and `kdos-shell died 6 times in 30s — not restarting
it again`. `sh_theme_watch()` (shell.c) now catches SIGHUP for the two
long-lived surfaces, the panel and the notification daemon, and each re-reads
the accent and invalidates at the top of its loop; the short-lived front ends
read it when they start, which is already after the change.

**A supervised child gets every ignored disposition back, not just SIGPIPE.**
`SIG_IGN` survives exec exactly as the mask does, so a session started under
`nohup` handed its ignored SIGHUP all the way down and the new handler would
never have fired — measured as `SigIgn: 0x1` on a running panel.
`child_reset_signals()` in `kdos-child.c` walks the 31 standard signals and
restores the default for any it finds ignored. labwc's own `spawn.c` resets
SIGPIPE only, which is right for a process that inherited nothing.

**The foot theme is `[colors-dark]`, never `[colors]`.** foot deprecated the old
section name and warns ONCE PER KEY on stderr, so every terminal on this desktop
opened with 24 lines of `deprecated: foot: [colors]: use [colors-dark] instead`
above the first prompt. `initial-color-theme` defaults to `dark`, so one section
is all foot reads and there is no light KDOS palette to write.

**`kdos theme --audit` is the palette claim, checked** (`src/packages/kdos-tools/
themeaudit.c`). It does not try to recognise "palette colours" in the installed
files — that test would have to know which mixes are legal and would drift from
the generators. It runs the SAME generators with `$HOME` and the XDG variables
pointed at a scratch directory (the trick `06_packaging/00_theme.sh` already uses
for `/etc/skel`) and compares byte for byte, symlinks included. Anything that
differs, differs from what this machine's own palette produces right now. It
writes nothing outside the scratch directory and signals nothing — an audit that
repaired what it found would be a `kdos theme` with a misleading name. Exit 0
clean, 1 on drift, 2 if the audit could not run. `kdos theme --audit amber` asks
what an accent switch WOULD change: measured, 10 605 of 23 279 icon entries, the
rest being symlinks that do not move. A `starship.toml` is copied in first,
because that artefact is edited between markers rather than written.

Verified by `testing/selftest.sh` against a real generated `$HOME`: clean after
generation, and all four kinds of drift caught — an edited stylesheet, a deleted
icon, a stray file, and a cursor alias re-pointed at another shape.

### kdos-icons

Theme `KDOS`, a **vendored, pruned, recoloured Papirus**, `Inherits=hicolor`
(the COSMIC and Pop fallbacks went with the desktop that needed them). Two
scripts, same split as kdos-cursors:

- **`vendor.py`** is the maintenance tool, run by hand on the host with network:
  `vendor.py papirus-icon-theme-YYYYMMDD.tar.gz` rewrites `art/`. Papirus over
  Tela/Colloid/Qogir purely for coverage — the only free set with a real icon
  for essentially every mimetype, device and place the desktop or the ~105
  Debian apps will ask for, and flat single-fill SVG, so a palette remap is a substitution
  rather than a redraw. The prune is 90 MB → 13.7 MB: **`apps/` is not vendored
  at all** (the alien apps ship their own icons, overriding Firefox's and GIMP's
  marks makes the launcher harder to read, and it is 76 MB of the 90), six sizes
  of upstream's fourteen, and only the DEFAULT folder colourway — blue, which
  the unsuffixed names already alias to (`folder-cd.svg → folder-blue-cd.svg`,
  so dropping blue strands every base name). Papirus-Dark is merged over
  Papirus. Aliases stay symlinks; any left dangling by the prune is dropped
  iteratively (links point at links).
- **`kdos-theme icons`** is the build step, and `kdos theme` re-runs it per accent.

Colours are mapped by FAMILY, not flattened: blue/green/purple → accent,
yellow/orange/brown → secondary, red → urgent, near-greys → faintly tinted
neutral, each keeping its own lightness and saturation. Deliberate — Papirus
colour-codes mimetypes, and collapsing every hue onto one accent turns a folder
of files into a wall of identical green lozenges. A PDF stays red and an audio
file stays amber while folders, devices and places go phosphor. The same mapping
is in libkcolor's `kcol_remap`, so icons and widgets agree about what "green"
means.

Because Papirus's `apps/` is not vendored, the theme would have NO Applications
context at all and every host app icon would come through untinted — which is
what "the KDOS icon theme is not being used" looks like. So `kdos-theme icons`
also recolours `hicolor/<size>/apps` into `scalable/apps`, but **only names with
the `kdos.` prefix**: `06_packaging` installs the ALIEN apps' icons into that
same tree, and a phosphor Firefox logo is vandalism, not theming.
**Sweep every size directory, not just `scalable/`** — the rule was learned
against COSMIC, whose icons were SVGs filed under fixed sizes and nowhere else,
so a scalable-only pass left exactly the buttons the user looks at unthemed. It
still holds for anything that ships an SVG under a numeric directory. Largest
variant per name wins.

The KDOS marks (`distributor-logo-kdos` / `start-here`, and the tux over
`kdos-launcher`) are installed by the GENERATOR, not the kpkgbuild —
`kdos theme <accent>` re-runs it against `$HOME` and has to produce a complete
theme on its own. The PNGs themselves are committed under `marks/`; the
host-only `genmarks.py` is what cuts them from `kdos.png` when they need
recutting, and nothing on the target or in the build ever runs it. **Crop at
the GLOW alpha threshold, not the SOLID one**, or the outer glow is clipped.

**`/usr/share/icons/hicolor/index.theme` must exist** or every icon lookup
silently fails. The `hicolor-icon-theme` port provides it. Appbox app icons are
installed into the SYSTEM hicolor tree by `01_appbox.sh` (contexts flattened to
`apps/`); a user-dir icon tree without its own index.theme is not searched.

### kdos-cursors

A **vendored, pruned, recoloured fork of Bibata-Modern-Ice**. Same two-script
split:

- **`vendor.py`** keeps only what KDOS uses — 32 shapes (upstream's X11 relics
  dropped), five sizes (24 32 48 64 96 of fourteen), and every third animation
  frame with the delay tripled. `wait` and `progress` ship 54 frames at 9 MB
  each upstream — over two thirds of the theme — so that decimation is most of
  the 27 MB → 4.4 MB cut. `art/` is LFS-tracked; `art/UPSTREAM` records the
  release.
- **`kdos-theme cursors`** recolours `art/` into the palette. Luminance maps onto a
  dark-green→accent ramp, except `wait`/`progress`, which ramp to **amber** —
  the same "working on it" colour the boot splash uses. No SVG toolchain, no
  clickgen, no network at build time.

**Xcursor ARGB is premultiplied** — divide alpha out before the ramp and
multiply back after, or every anti-aliased edge turns into a dark halo.

**Alias groups are re-rooted on their CSS names** (`default` is the real file,
`left_ptr` the symlink). Not cosmetic: Bibata ships the opposite direction, and
installing it over the old theme produced `default -> left_ptr -> default`, so
kpkg aborted with `realpath: Symbolic link loop`. If a theme swap ever fails
that way: `kpkgdel kdos-cursors`, delete `/usr/share/icons/<theme>` plus the
`/etc/skel/.icons` copy and the cached package, then rebuild.

Upstream is GPL-3.0; `LICENSE.notice` records every deviation. Installed to
`/usr/share/icons/KDOS-cursors` AND `/etc/skel/.icons/` — the box shares `$HOME`
but not `/usr/share/icons`. Selected via `XCURSOR_THEME`/`XCURSOR_SIZE` in
`fs/etc/profile.d/10-wayland.sh` and `fs/etc/skel/.config/gtk-{3,4}.0/settings.ini`.

**The wallpaper's baked scanlines are gone** — `kdos-comp` renders them now (see
**The CRT pass** below), and two sets beating against each other is moiré, not
identity. They were an exact `×0.779` multiply on every third row, so removing
them was an exact divide: measured over the whole image (period-3 row means
`10.59 / 10.59 / 8.26` before, `10.586 / 10.586 / 10.599` after) with **zero**
clipped pixels, since the brightest affected pixel was 193. The **vignette is
still baked in**: it is a radial function nobody recorded, so it is not exactly
invertible, and the shader's own vignette is mild enough to live with it. If the
wallpaper is ever re-rendered from a clean source, drop it there.

---

## The `kdos` command

`kdos` is the front door, and by now it is fifteen subcommands: `help`
(commands + the keybind cheat sheet), `theme`, `status`, `doctor`, `app`,
`version`, plus `why` / `explain`, `sandbox`, `appid`, `restarts`, `stutter`,
`march`, `rebuild` and `cve` — each documented in its own section here. `kdos
doctor` checks the things that have actually broken on this distro — including
`readlink /proc/self/root`, the switch_root trap above.

It is C now, in **`src/packages/kdos-tools`**, along with `kdos-banner`,
`kdos-shot`, `kdos-fetch-app`, `kdos-fetch-static`, `ksvc`/`service` and
`kdos-getty` — one binary dispatched on its own basename, installed under
every name. Nothing of it is left in `fs/`.

Two things that came out of the move:

- **The palette is libkcolor's.** `kdos` used to carry its own copy of the
  four-scheme table and a `mix_hex` to go with it; that copy and the
  installer's were edited separately.
- **`kdos-fetch-app` no longer interpolates an app name into a shell string.**
  It built `bash -c "... sudo apt install -y '$APP' ..."`, which the OUTER
  shell expanded before the inner one parsed it — a name containing a quote
  broke out and ran as the box's root. The in-box package-manager fallback is
  still shell, because that is what it is, but the name arrives as `$1`.

Verified against the bash version: `kdos theme` produces byte-identical
`gtk.css` ×2, foot, btop, starship, both panel RON files and the state file
for **all four accents**, and `kdos help` is byte-identical. `kdos-banner
--plain` is byte-identical too, multi-byte column alignment included.

`kdos-desktop` and `kdos-desktop-start` are **deliberately still `/bin/sh`**.
Every line in them is a fix for something that broke — the guid-less bus
address, the flock, the portal restart ordering — they take no untrusted
input, and rewriting them buys nothing but risk.

---

## The C libraries — `src/libs/`

Static archives, `libk*`, **linking nothing but musl — with exactly one
declared exception**. That is the constraint the whole set exists under:
libktui has to be usable in phase 1, before any library exists to link against.
If a lib ever needs a real `-l`, every phase-1 consumer moves to phase 4 with
it.

**`libkwl` is the exception, and being a separate archive is how the rule
survives it.** It is libktui's *Wayland backend* — the cell grid painted into a
layer-shell or session-lock surface instead of into a terminal — so it needs
`fcft`, `pixman`, `xkbcommon` and `wayland-client`, which phase 1 does not have.
Splitting it out rather than folding it into libktui is what keeps kinstall
linking zero libraries on the first bootable image; that split IS milestone M3.
`kdos-shell` and `kdos-lock` link it, and nothing in phase 1 does.

**Bind `zwlr_layer_shell_v1` at 4, and never below it without meaning to.**
`KEYBOARD_INTERACTIVITY_ON_DEMAND` is a version-4 request, and wlroots answers
it on an older resource with `!!interactive` — so a client bound at 1 asking for
ON_DEMAND is granted **EXCLUSIVE**. That is not cosmetic: labwc parks the seat's
keyboard on an exclusive layer surface and `seat_focus()` then refuses every
window focus, so `kdos-desk` (background layer, on-demand keyboard) held the
keyboard against every window on the screen and NOTHING TYPED REACHED ANY WINDOW
until an overlay took the focus and gave it back. Shipped, and invisible to
everything: the request was right, the protocol was right, the number was 1.

**Three input rules libkwl now keeps, each one a bug that was already there:**

- **The event queue is a RING, not one slot.** libwayland delivers a whole batch
  of callbacks from a single read, so a button PRESS followed in that batch by
  the motion that accompanies it was overwritten before any consumer saw it —
  every click under a moving hand was a coin toss. Motion still collapses onto
  motion; a button or a key never overwrites anything.
- **Key repeat is the CLIENT's job.** Wayland has none: the compositor sends
  `repeat_info` and every client repeats for itself. Without it, holding an
  arrow in the launcher, the menu, the chooser, the run box or the desktop did
  exactly one thing. The poll timeout is shortened to the next repeat, or it
  fires at whatever cadence the consumer happened to poll at — one second, in
  every one of those loops.
- **A wheel tick is not an axis event.** `wl_pointer.axis` is continuous and a
  touchpad sends a stream of small ones; each as a tick scrolled a list by
  hundreds of rows. Ticks come from `axis_discrete` where the compositor
  measured them, from a ten-unit accumulator otherwise, and both are spent in
  `frame`. `pt_leave` also has to report — hover state with no leave stays lit
  for the rest of the session.

| Lib | Prefix | Owns |
|---|---|---|
| `libkbase` | `kb_` | alloc + OOM hook, `die`/`warn`, strings, files, paths, flock, monotonic time, group membership, **the `KbArgv` builder and `kb_run`/`kb_run_capture`/`kb_run_detach`** |
| `libkcolor` | `kcol_` | **the palette table**, hex/HLS, `kcol_mix`, the hue-family classifier, `kcol_remap`, `kcol_retint_text` |
| `libktui` | `ktui_`, `KT_`, `KRect`/`KRgb`/`KtuiEvent` | terminal ownership, cell buffer + diff flush, key/mouse decoding, immediate-mode widgets, modals, text furniture, **the three glyph tiers and the charts drawn out of them**, offscreen rendering |
| `libkxdg` | `kxdg_` | desktop entries, matching what `RawConfigParser(strict=False)` did with them |
| `libkpkg` | `kp_` | the package database, the ports tree, `# depends` parsing, the dependency solver, `kp_vercmp`, the recipe/build-config hashes |
| `libksig` | `ksig_` | Ed25519 signing and verification, key files, keyrings — the one library with vendored third-party source (Monocypher) |
| `libkbuild` | `kbuild_`, `kj_` | phase discovery, the phase-env metadata block, the build plan, the snapshot inventory, a read-only JSON scanner |
| `libkcell` | `kcell_` | the fcft glyph cache and the cell painter — a grid of cells into a pixel buffer, and the ASCII ramp built out of it. Needs fcft and pixman |
| `libkwl` | `kwl_`, `KWL_` | libktui's Wayland backend — surface roles (layer-shell, xdg, session-lock), shm buffers, output naming, input (queue, key repeat, wheel accumulation), `kwl_input_cells`. **The one library with real `-l` dependencies beyond libkcell's** |

Dependency direction is `libktui → libkcolor → libkbase` and `libkxdg →
libkbase` and `libkbuild → libkbase` and `libksig → libkbase` and `libkwl →
libkcell → libktui`, and nothing points back up. libkcell is a SEPARATE
archive from libkwl so that a consumer wanting the cell painter is not made to
link a Wayland client library to get it.

Three rules the extraction exists to keep, each one a bug that was already
there:

- **Symbols are prefixed.** `Rect`, `Event`, `Ui`, `xstrdup`, `read_file` were
  all unprefixed, and `kdos-appbox` had already declared its *own* `xstrdup`
  and a `read_file` with different length semantics. Two of our own programs
  could not be linked together.
- **`ui` is private.** The frame state used to be a public struct that
  applications assigned to field by field — `ui.consumed = 1` in two dozen
  places in the installer alone. It is `static` in `ktui_widget.c` now, behind
  `ktui_consume()` / `ktui_focus_get()` / `ktui_wheel_take()` /
  `ktui_focus_rect()`. Same for the draw extent: `draw_maxy` was a global the
  caller wrote to, and is `ktui_extent_reset()` / `ktui_extent()`.
- **Chrome ids are the library's business.** `ui.c` used to hardcode the
  installer's `UI_ID_SIDEBAR`. Chrome now registers through
  `ktui_hit_chrome(rect, id)` with caller-local ids and reads back with
  `ktui_chrome_clicked(id)`; the reserved range and the "never joins the Tab
  ring, never drags the page scroll" rule stay inside libktui.

And one libkbase rule: **a library does not own the exit path.** `kb_calloc`
calls whatever `kb_set_oom_handler()` was given (the installer hands it
`ktui_term_shutdown`) instead of knowing that a `term_shutdown` exists and
that the program is called "kinstall". `kb_set_progname()` supplies the
prefix for that message and for `kb_die`/`kb_warn`.

### libktui draws at three glyph tiers, and the middle one is the reason

A ramp cell is picked from one of three tables, chosen by `ktui_ramp_init()`
from `ktui_caps`:

| Tier | Caps | Ramp | Levels |
|---|---|---|---|
| rich | `UTF8` | `▏▎▍▌▋▊▉█` / `▁▂▃▄▅▆▇█` | 8 |
| vt | `UTF8 \| LINUXVT` | `░▒█` | 3 |
| ascii | neither | `.:#` | 3 |

**The vt tier exists because `ter-kdos32n` is 512 glyphs and eighth blocks are
not among them.** kinstall runs on that console and shares these widgets with
kdosbuild, which runs on the host in a modern terminal; a glyph the font does
not carry renders as a BLANK on tty1, so an eighth-block bar there is not ugly,
it is invisible. Three levels is the honest resolution of that font. Grep
`uni/xos4-2.uni` in the terminus-font tarball before using any glyph — the
`.uni` file is a plain list of 512 codepoints, and `ports/core/terminus-font/build.sh`
adds exactly six more (the double box-drawing set). It has `░ ▒ █` and the box
sets and `· • ■ … ° ↑ ↓ ◀ ▶`; it has **no eighth blocks, no half blocks, no
braille, and no `← →`** (that pair is why `ktui_glyph` carries `◀ ▶` instead).

**`ktui_progress` is a wrapper whose pixels must not move.** kinstall links it
and only it, never `ktui_progress_ex` directly, and it pins `KT_BAR_SOLID` plus
`KT_BG` so the installer keeps drawing exactly the two-state bar it always did:
in the SOLID branch `tip` stays 0, the fractional-tip cell is unreachable, and
`fill` is still `(int)(frac * r.w + 0.5)` under the same `frac > 1` clamp. The
`frac < 0` indeterminate scanner is untouched. Change `ktui_progress_ex` freely;
leave that branch alone.

**A resize is not applied until the CONSUMER applies it.** The backend sets
`ktui_resized`; `ktui_w`/`ktui_h` follow only when the loop calls
`ktui_draw_resize()` and `ktui_draw_invalidate()`, which `panel.c` and
`launcher.c` do and `notifyd.c` did not — it had a fixed-size surface and never
needed to. The moment that surface started being resized, `draw_toasts()` went
on believing it had the old three rows, a toast WITH A BODY failed its
`y + rows > h` guard, and the daemon painted an empty box in the corner. Any
loop that owns a surface owns this.

**`ktui_offscreen_init(w, h)` + `ktui_draw_dump()` render with no terminal at
all** — the cell buffer at a fixed size, written out as plain text instead of
escapes. Every geometry defect this toolkit has shipped (text over a box
border at 80 columns, a heat strip past its rect, a column drifted out from
under its own header, a gauge invisible on a selected row) was invisible to the
compiler and to `testing/selftest.sh`, which has no terminal and cannot draw.
This is how they get looked at. `kdosbuild --preview` is the consumer.

### libkcolor is the one palette

`KCOL_SCHEMES(X)` in `kcolor.h` is an **X-macro**, so every consumer expands
the same literals into its own table at compile time — libktui projects them
onto its eight VT slots (`deep` is the one colour a terminal has no use for),
and the theme generators expand them into CSS, SVG and Xcursor. Nobody keeps
a second copy of the numbers.

**`kcol_to_hls` / `kcol_from_hls` / `kcol_remap` reproduce python's `colorsys`
exactly**, including the odd `2.0 - maxc - minc`, the modulo on a negative
hue, and `round()`'s half-to-even. That is not pedantry: the vendored Papirus
and Bibata artwork is *committed*, and a C generator that rounds differently
diffs against files already in git. Verified over 8476 colours × 4 schemes
against CPython. Do not "clean up" the operation order.

libkcolor stays off libm for the same reason libktui does — a phase-1
consumer cannot link one. `pymod1` subtracts in a loop and the rounding is
done by hand rather than with `fmod`/`nearbyint`.

`kcol_mix` (integer, percent) and `kcol_mixf` (float, 0..1) are **not
interchangeable** — they disagree by a unit on some inputs, and each generated
file was written against exactly one of them. `kcol_retint_text` is the whole
of KDOS's "SVG" and "CSS" recolouring: a `#rrggbb`/`#rgb` token scanner,
matching python's `#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b` including the trailing
word boundary, so `#39ff14ff` is left alone rather than half-rewritten. There
is no SVG parser and no CSS parser anywhere in the tree.

---

## kdos-theme — the generators

`src/packages/kdos-theme/` is one binary with three subcommands, replacing
`gengtk.py`, `genicons.py` and `gencursors.py`:

```sh
kdos-theme gtk     <out> [accent] [--src DIR]
kdos-theme icons   <out> [accent] [--src DIR] [--marks DIR]
kdos-theme cursors <out> [accent] [--src DIR]
kdos-theme accents
```

`--src`/`--marks` beat `$KDOS_GTK_SRC` / `$KDOS_ICON_ART` / `$KDOS_ICON_MARKS`
/ `$KDOS_CURSOR_ART`, which beat the installed `/usr/share/kdos/…`. The three
art packages call it from their `build()` against `$PORT_SRC`; `kdos theme`
calls it against `$HOME`; `06_packaging/00_theme.sh` gets it via
`kdos theme phosphor` with `HOME=/etc/skel`.

**This is what took python3 off the target.** `gengtk.py` and `genicons.py`
were installed into `/usr/share/kdos` and run by `kdos theme`; nothing on the
shipped system runs python now.

Output was verified **byte-identical to the python generators for all four
accents** — 10792 icons, 12462 symlinks, 6 stylesheets, 32 cursor shapes,
`diff -r` clean. Two things had to be reproduced exactly to get there, and
both are noted in the source: python's `round()` is half-to-**even**, and
`re.sub`'s greedy `\s*$` **swallows the file's trailing newline** after the
last `@define-color`.

### The appbox image tools

`kdos-appbox image` replaces `ports/appbox/pack`, `ports/appbox/assemble` and
the python heredoc that used to sit inside `01_appbox.sh`:

| Subcommand | Runs where | Was |
|---|---|---|
| `image pack <tar> <dir>` | host, from `ports/appbox/fetch` | `ports/appbox/pack` |
| `image assemble <dir>` | **inside the chroot**, from `01_appbox.sh` | `ports/appbox/assemble` |
| `image remap-uids <store>` | inside the chroot | the inline `python3 - <<EOF` |

`assemble` is why this mattered: it ran in the chroot, so python3 was a
*packaging* dependency as well as a runtime one. `libkbase`'s `kb_tar_*` is a
minimal ustar reader/writer — the only archives it ever sees are `podman save`
output — and `zstd` is still exec'd rather than linked, so kdos-appbox keeps
its zero-`-l` property.

The INDEX.json format is unchanged and was verified **both ways**: the C
`assemble` reads a python-written index, and the python `assemble` reads a
C-written one. The committed `ports/appbox/image/` chunks do not need
regenerating.

`ports/appbox/fetch` runs on the host, where no target binary exists, so it
compiles kdos-appbox itself for the one subcommand it needs. That is a
two-second `cc` of a program that links nothing.

**The cursors follow an accent switch now.** That was the one artefact
`kdos theme <accent>` could not reach — the generator was always parameterised,
but the art it needs was not on the target. `kdos-cursors` installs its `art/` to
`/usr/share/kdos/cursors/art` (4.4 MB, the same shape kdos-icons already used),
which is exactly `CURSOR_ART_DEFAULT`, and `kdos theme` regenerates
`~/.icons/KDOS-cursors` with the rest. An install without the art keeps the
cursors it has rather than getting an empty theme.

---

## bb — the AAlib demo

**There is no KDOS demoscene.** One was written (`src/packages/kd`, a
from-scratch aalib demo with a 3D pipeline, audio-reactive channel taps and
music-as-clock) and then removed at the user's request. Do not resurrect it,
and do not reintroduce a `kd`/`kk` port on the strength of a stale reference:
if one turns up, it is a leftover.

What ships is `ports/core/bb` — the 1997 AAlib demo, **upstream and
unmodified** apart from two musl/x86_64 safety patches. Both defects were
found with ASan rather than by reading:

- **`tex.c`'s `clear_zbuff()` cleared twice its allocation** — `set_zbuff()`
  allocates `sizeof(int)` per cell and the clear used `sizeof(long)`. Same
  size in 1997, double on x86_64: the heap corrupts and stage 2 aborts with
  `malloc(): corrupted top size`. (`zbuff-int-not-long.patch`)
- **`messager.c` scrolled the text buffer with `memcpy`**, source and
  destination overlapping by every row but one. glibc survived it; musl is
  free not to. (`messager-overlapping-copy.patch`)

Two aalib facts that cost a debug cycle each and outlive the demo that found
them:

- **aalib's `linux` driver writes cells into `/dev/vcsa<n>`**, which a
  non-root user on KDOS cannot open. `aa_autoinit` answers a failed
  *recommended* driver by sweeping its own `aa_drivers[]` — landing on
  **`stdout`**, which scrolls a fresh block of text up the terminal every
  frame. Anything on aalib must recommend curses as well as linux.
- **`MikMod_RegisterAllLoaders()`, not just `load_s3m`.** ModArchive's
  public-domain shelf is `.xm`/`.it`/`.mod`; with the S3M loader alone a
  track fails inside `Player_Load` and the program plays silent.

Sound needs `libmikmod` (ALSA only, `--disable-dl` so a missing ALSA is a link
error rather than a silent runtime failure), with two patches of its own:
`alsa-null-close.patch` guards the `END:` label against closing a NULL pcm
(otherwise a machine with no card aborts inside `MikMod_Init` on
`Assertion failed: pcm`), and `alsa-nonblocking-update.patch` stops
`ALSA_Update` blocking its caller's frame loop.

**Audio on a bare TTY took two stacked fixes**, both in `fs/etc/init.d/`:

- `01_udev.sh` triggers coldplug with **`--action=add`**. The default replays
  every device as `change`, and `80-drivers.rules` — the rule that modprobes
  from MODALIAS — opens with `ACTION!="add", GOTO="drivers_end"`. A plain
  trigger therefore loads no module at all: the HDA controller stayed
  unclaimed and alsa-lib answered "Unknown PCM default".
- `50_alsa.sh` falls back to **`alsactl init`** when `alsactl restore` fails.
  A live ISO has no saved `asound.state`, and a failed restore leaves HDA
  exactly as the kernel did — Master MUTED at 0%/-74dB. `init` answers 99 when
  it matched a generic rule, which is a success here, so its status is
  deliberately ignored.

There is no pipewire on a TTY and none is wanted: `kdos` is already in the
`audio` group. QEMU had no audio device at all until `testing/qemu-audio.sh` —
every `make run*` target and the containerized `run.sh` source their
`-audiodev` from it, probed rather than hardcoded because QEMU aborts at
startup on a backend its build lacks.

---

## kinstall — the installer

`src/packages/kdos-installer/` is built by `script/01_phase1/13_kinstall.sh`
with the **cross** compiler, alongside `src/libs/libkbase` and
`src/libs/libktui` on the same command line. The whole set links against
**nothing** — not even ncurses — which is why it can live in phase 1 and
exists on every tree from the first bootable image onward. Do not give any of
the three a library dependency without moving this build to phase 4. The
binary is still `/usr/bin/kinstall`; a `kpkgbuild` sits beside the sources so
a running KDOS can rebuild it natively.

| File | What it owns |
|---|---|
| `probe.c` | /sys + superblock + GPT reader; a small blkid |
| `pages.c` | the ten wizard pages |
| `install.c` | the forked install child and its line protocol |
| `conf.c` | the answer file |
| `dump.c` | `--dump probe` / `--dump plan`, text or `--json` |
| `main.c` | chrome, the poll loop, CLI |

Everything else it used to own — the terminal, the cell buffer, the input
layer, the widgets, the modals, the palette — is libktui (see **The C
libraries**).

**libkcolor is on that build line too**, because libktui's `ktui_theme.c`
includes `kcolor.h` — the palette numbers live there and nowhere else.
Leaving it out builds fine on the host, where `testing/selftest.sh` passes it,
and fails only in the cross build.

Five decisions carry the design:

- **Nothing is written before the summary.** Every page fills `cfg` and only
  `cfg`; the install step is the single point of no return. The Python
  installer it replaces partitioned in the middle of the questionnaire, so
  "back" did not exist. `Next` on the summary page is deliberately refused —
  the install starts from the button and only from the button.
- **The whole UI is eight colours.** A 512-glyph console font makes the VT
  steal the foreground intensity bit for the 9th glyph bit, so colours 8-15 are
  unreachable as foreground and `A_BOLD` changes the FONT PAGE rather than the
  weight — never emit it on a VT. Eight slots mean the tty and a truecolor
  `foot` window render the same picture. On a VT the installer saves the
  palette with `GIO_CMAP`, installs its own with `PIO_CMAP` and restores
  exactly what `kdos-getty`'s setvtrgb loaded; elsewhere the same slots go out
  as 24-bit or 256-colour SGR.
- **The mouse works on tty1 with no gpm.** The Linux console has no mouse
  reporting at all, so `input.c` opens `/dev/input/event*`, keeps its own
  pointer (relative mice scaled by the real cell size read from
  `/sys/class/graphics/fb0/virtual_size`, absolute devices — QEMU's usb-tablet
  — mapped directly) and draws it as an inverted cell. Under a terminal
  emulator it uses SGR-1006 instead and never touches evdev.
- **The install runs in a forked child** speaking `S/K/P/N/L/F/D` lines back
  over a pipe, so the work stays sequential code and the parent stays a
  single-threaded poll loop that never blocks on a 4 GB rsync. The child
  redirects its own stdio to `/dev/null` and keeps the protocol on its own fd;
  nothing but `emit()` can reach the pipe. **No `system()`, no shell** — device
  paths and user names all arrive from menus.
- **Chrome takes hit ids from a reserved range** (`UI_ID_SIDEBAR`), never from
  `ui_id()`. The sidebar draws before the page, so claiming real focus ids
  there pushed every control on every page ten places down the Tab ring and
  left the caret parked on a decoration — typing did nothing at all.

Things the installer needed from the rest of the tree, all now present:

- `kdos-getty` loads `/etc/keymap` with `loadkeys` (the installer writes it).
- `rcS` runs `swapon -a` after `mount -a`; nothing else ever did, so the swap
  option would otherwise have been decorative.
- **fstab is appended to, never replaced.** The shipped file carries the
  tmpfs `/tmp` with `mode=1777`; overwriting it is the /tmp bug above, arriving
  a week later and looking like "GIMP does not start".
- Renaming the user rewrites `passwd`, `group` (membership lists *and* the
  primary group's own name), `shadow`, the home directory, and the
  `--autologin kdos` in `inittab`. Miss the last one and the installed system
  logs nobody in.
- Kernel and initramfs are copied **onto the ESP** and the generated
  `refind.conf` points at those FAT paths. rEFInd can read the ext4 root only
  through its filesystem driver, and a boot that depends on a driver load is a
  boot that fails silently after a kernel update.

**LUKS2 on the root partition** is a checkbox on the layout page. The installer
runs `cryptsetup luksFormat` then `open`, feeding the passphrase on **stdin**
both times — an argument would publish it through `/proc/<pid>/cmdline` to every
process for as long as cryptsetup runs — and everything after that talks to
`/dev/mapper/kdosroot`, so one name means "where the root filesystem is" for the
mkfs, the mount, the fstab UUID and the rsync. The boot options carry **two**
UUIDs and confusing them is the trap: `cryptdevice=UUID=<container>:kdosroot`
names the LUKS volume, `root=UUID=<filesystem>` names what is inside it, and the
second does not exist until the first is open. The passphrase is refused at the
QUESTIONNAIRE when the image has no cryptsetup, not at the install step — after
the point of no return is the wrong place to discover it.

**The root filesystem is a table, not a branch** (`ki_filesystems[]` in
`conf.c`): ext4, btrfs or xfs, and every consumer reads the same row — the
menu, the mkfs argv, the fstab line and the swapfile step. Three things it
encodes that are each a way this goes wrong later:

- **`fs_passno` is 1 for ext4 and 0 for everything else.** A non-zero pass is
  an instruction to run a checker; there is no `fsck.btrfs` worth running and
  no `fsck.xfs` on this image at all.
- **A swapfile is made differently on each.** `fallocate` leaves unwritten
  extents and `swapon` refuses those on xfs (*"swapfile has holes"*), and btrfs
  needs the file NOCOW and uncompressed, which is what `btrfs filesystem
  mkswapfile` is for. The failure is at the NEXT boot's `swapon -a`, with no
  swap and nothing saying why — so ext4 gets `fallocate`, xfs gets `dd`, btrfs
  gets `mkswapfile`.
- **`CONFIG_XFS_FS=m`**, so `01_initramfs.sh` carries the `xfs` module. ext4 and
  btrfs are built in. An xfs root the initramfs cannot mount installs perfectly
  and never boots again.

A filesystem whose mkfs is missing is still listed and still selectable, with
the row saying so and `do_prepare` refusing it *before* anything is written —
a control that snaps back under the cursor is worse than one that explains
itself. An answer file naming something else (`fstype = zfs`) falls back to
ext4 rather than failing at the mkfs. `--dump plan` prints the resulting mkfs
command and fstab line, because "fs btrfs" alone does not say what will run.

Known gaps, each for a missing port rather than a missing feature: no f2fs
(`CONFIG_F2FS_FS=m` with no mkfs), and the time zone is written as a **POSIX TZ
string** into `/etc/profile.d/20-timezone.sh` because there is no `tzdata` —
musl parses those directly, DST rules included.

Iterate without booting: `kinstall --dry-run` logs every command and executes
none, and `--save`/`--config`/`--unattended` give it an answer file.

**`kinstall --dump probe|plan [--json]` needs neither a terminal nor a run**
(`dump.c`). `probe` is the machine as `probe.c` sees it — firmware, cpu, memory,
every disk and partition — and is the right thing to paste into a bug report.
`plan` calls the same `install_plan()` the wizard does, so the step list and its
SKIPS are the real ones: `plan = reuse` skips Partition, a non-default accent
un-skips Theme. `--json` is a rendering of the same traversal, not a second
walk. **No password reaches either form** — `cfg` holds them in the clear
because `crypt()` is next, and a dump is what ends up in a CI log; a sentinel in
`testing/selftest.sh` proves it. `--dump probe` is not in the selftest: it walks
the rootfs to measure the payload, seconds on a live ISO and far more on a
developer's disk.

---

## The compositor is a hard fork of labwc

`src/desktop/kdos-comp/` is **labwc 0.20.0, imported wholesale, rebranded and
FROZEN** — no upstream merges, no `.patch` files; it is our source now and is
edited directly. `KDOS-FORK` at its root records the upstream tarball and
sha256. GPL-2.0 and upstream copyright headers are kept. The binary is
`kdos-comp`; config dirs are `kdos-comp` (`~/.config/kdos-comp/rc.xml` — labwc's
own format, and labwc's documentation applies verbatim). The port builds with
meson out of `$PORT_SRC` (`source =` empty), compiling libkbase + libkcolor
into a small static archive first because the grafts need the palette.

**KDOS code lives in `src/kdos-*.c` plus `include/kdos.h`; upstream files carry
minimal hooks marked `/* KDOS */`.** Grep for that marker to find every
touch-point. The graft files: `kdos-config.c` (comp.conf), `kdos-child.c`
(supervised chrome), `kdos-wallpaper.c`, `kdos-frames.c` (stutter socket),
`kdos-idle.c` (dim → lock → off policy), `kdos-crt.c` (the CRT pass). What the
old from-scratch compositor carried beyond these — window management,
session-lock, capture and clipboard globals, the input-method wire, Xwayland,
security-context filtering — is labwc upstream code and needs no graft.

**`~/.config/kdos/comp.conf` holds ONLY the KDOS keys now**: `wallpaper`,
`crt` / `crt_scanlines` / `crt_curve`, `idle_dim` / `idle_lock` / `idle_off`.
Keybinds, mouse, workspaces and window behaviour are rc.xml's business; an old
`bind`/`startup`/`workspaces`/`mouse` line is ignored **and says so**, naming
rc.xml — as does any other unrecognised key, because the skel file promises
that a line which does not take effect is reported, and a typo that produced
silence is indistinguishable from a setting that does nothing. It also carries
`panel_bottom` and `desktop_icons`, the two pieces of supervised chrome that
are optional, and `chrome_font`, which `kdos-child.c` passes to every one of
them as `--font`. All three are read once at startup. `chrome_font` exists
because libkwl's default is a PIXEL size and libkwl does no HiDPI: on a 4K panel
the chrome comes out half the height it should be and nothing could say so
(`Terminus:pixelsize=64` is the doubled cell). The compositor's own titlebars
are the matching knob on the labwc side — `<theme><font>` in rc.xml, in POINTS.

**`<default />` MUST be the first child of `<keyboard>` and of `<mouse>` in
rc.xml, and it is the most load-bearing line in this distro's configuration.**
labwc loads its built-in bindings only when the user's file defines NONE of
that kind (`post_processing()`), so a file that binds one key throws every
default away. KDOS shipped exactly that: no `Client Left Press → Focus/Raise`
(so **clicking a window did not focus it** — `focus_follow_mouse` is false),
no titlebar drag, no working Close/Iconify/Maximize buttons, no border resize,
no root menu, no A-Tab, no A-F4, no snap arrows. On a booted ISO the symptom is
"the mouse does not work", and it is invisible to a compile, to the recipe
parser and to XML validation. Overrides go AFTER `<default />`, because
`deduplicate_{key,mouse}_bindings()` keeps the LATER of a duplicate pair.
`testing/preflight.sh` fails a skel rc.xml that gets this wrong — with the
comments stripped first, or it reports on the file's own explanation of the
trap.

The skel rc.xml keeps the old KDOS bindings (W-Return foot, W-d launcher,
W-q close, W-Tab cycle, W-1..4 workspaces, W-l lock, media keys → kdos-osd),
adds what the fork gained (`W-m` ToggleMaximize, `W-f` ToggleFullscreen) and
adds the three front ends that had no way in: `A-F2` kdos-run, `W-e` mc,
`Print`/`S-Print` kdos-shot. `W-Escape` goes through the prompt below. Three
more since: `W-n` Iconify (minimise had a click and no key), `W-space` ShowMenu
(the root menu is a right-click on a desktop a maximised window covers), and
`W-p`/`XF86Display` for `kdos-display`. The `<libinput>` block is shipped
COMMENTED — labwc's own defaults are already right, tap-to-click included
(`libinput_category_create()` enables it), and the comment is there so the
knobs can be found without reading labwc's manual.

**What the fork closed outright** (each was a documented gap of the old
compositor): `set_maximized`/`set_fullscreen` work; middle-click paste works
(primary selection); cursor-shape-v1, fractional-scale-v1, xdg-activation-v1
and xdg-toplevel-icon-v1 exist; SIGHUP is labwc's Reconfigure and reaches the
CRT tint reload (`kdos_crt_reload()` hook in `handle_sighup`).

**Window frames: `themerc-override` is GENERATED, and foot is told to use
them.** labwc reads `~/.config/kdos-comp/themerc-override` over its built-in
theme, and `kdos theme` writes it (`write_themerc()` in `kdos.c`) from the same
palette as everything else — the SIGHUP it already sends is labwc's
Reconfigure, so the frames retint live with the panel and the shader. It used
to ship from `fs/` as a fixed neutral grey, chosen so it would read acceptably
under all four accents without regeneration; the cost was the one artefact an
accent switch could not reach, and a bar across the top of every window that
looked like somebody else's desktop. `kdos theme --audit` covers it.

Three traps found live: a `*.title.bg.color` alone changes nothing — the
default texture is a gradient, so `window.*.title.bg: flat solid` must be set
too; the grey bar foot showed was foot's own CSD, not labwc's SSD at all (skel
`foot.ini` carries `[csd] preferred=server`); and **the SSD font is pango's,
not the cell grid's** — its default is `sans` 10, which put a 13-pixel
antialiased title on top of every 32-pixel window. `<theme><font>` in rc.xml
is Terminus at 24 POINTS, which is 32 pixels at 96dpi and therefore exactly the
`ter-u32n` bitmap the rest of the desktop is drawn in. The same block covers
MenuItem, MenuHeader and OnScreenDisplay, which are labwc's root menu and its
window switcher.

**`<core><promptCommand>` is `kdos-prompt`, because labnag is not built.**
labwc's `<action name="If"><prompt>` spawns the prompt command and dispatches
on its EXIT STATUS — 0 takes `then`, 254 is "cancelled, do nothing", anything
else takes `else`. Upstream's program for that is labnag, which is
`-Dlabnag=disabled` here, so the facility existed with nothing on the other end
of it. `kdos-prompt` is kdos-shell under another name and answers with those
codes; it is what lets `W-Escape`, Log Out, Restart and Shut Down ask before
they act. Its one shell was `spawn_piped()`, upstream's only `/bin/sh -c`, and
it is `g_shell_parse_argv` + `execvp` now like every other spawn in the fork —
the string it runs interpolates a message that came out of rc.xml.

**The root menu is `menu.xml`, and it deliberately lists no applications.**
labwc's built-in default is Terminal / Reconfigure / Exit, a compositor's menu
rather than a desktop's, and that is what a KDOS user got. Applications, Places
and System are `kdos-menu`, which reads the same desktop entries the launcher
and the panel do; a second application menu built at compositor startup would
be the one that went stale.

**The fork logs at INFO by default** (labwc's default is ERROR — with it, the
graft layer's decisions were invisible and an empty session log looked like a
hung session). `KDOS_COMP_DEBUG=1` maps to DEBUG, keeping the contract the
`kdos stutter` section documents.

**The `WLR_RENDERER=pixman` fallback in `kdos-desktop` reads sysfs, not
dmesg.** `dmesg` is root-only under dmesg_restrict, so the old grep forced
software rendering for every normal user — which silently disabled the CRT
pass (it declines non-GLES2). `/sys/bus/virtio/devices/*/features` is
world-readable, one character per feature bit, VIRGL is bit 0. The old check
also demanded RESOURCE_BLOB; that was a smithay-era lesson — wlroots' GLES2
renderer runs fine on virgl without blob, measured on the fork with the pass
on (and this host's qemu cannot even combine virgl with blob).

**Signal lessons, now upstream's.** The old compositor's hardest-won rule — a
spawned child must get its signal mask and SIG_IGN dispositions back before
`execvp`, because both survive exec and `wl_event_loop_add_signal` blocks what
it watches — is encoded in labwc's own `spawn.c` (`reset_signals_and_limits()`)
on every spawn path, and `kdos-child.c` does the same for the supervised pair.
The live-retint claim in **Theming** depends on it; it was verified against the
fork (`kdos theme amber` retints the running panel and shader).

**The startup children are supervised; a rc.xml Execute child is not.**
`kdos-child.c`: `kdos-shell`, `kdos-shell --bottom`, `kdos-desk` and
`kdos-notifyd` are spawned with ONE fork,
their pid kept, and the reap arrives through labwc's existing SIGCHLD handler
(`kdos_child_reap()` hook — a second signalfd on SIGCHLD would race the first).
signalfd COALESCES the signal and labwc's handler reaps one zombie per event,
so the handler also runs `kdos_children_poll()` — targeted `waitpid(WNOHANG)`
per supervised pid, which can never steal a zombie that is not ours — or two
chrome children dying together leave one a zombie that never respawns.
Five deaths in thirty seconds stops the respawn, because a crash loop hides
the log line that explains it. labwc's own spawns keep their double fork: a
terminal a keybinding opened is not the desktop's chrome.

**ONE SET OF CHROME PER OUTPUT**, spawned from labwc's output-created hook and
SIGTERMed from output-destroyed (the slot is marked `stopping` so the reap frees
it instead of respawning it — a panel whose output is gone would otherwise
exit, respawn and exit until the five-in-thirty rule stopped trusting it).
An unnamed layer surface is placed by the compositor on ONE output, so a
two-monitor machine used to get a panel, a window list and desktop icons on the
first screen and nothing on the second. libktui has a single cell buffer, so a
second screen cannot be a second surface of the same process — it has to be a
second PROCESS, which is what the two panels already were. `libkwl`'s
`KwlConfig.output` names it (wl_output version **4**, for the `name` event: a
registry id cannot be written on a command line), and `kdos-shell`/`kdos-desk`
take `--output`. `kdos-notifyd` stays one per session because it owns a bus
name and a second instance would simply fail to take it.

The bottom panel and the desktop icons were, for a release, code that **nothing
started** — `panel.c`'s own comment claimed `kdos-desktop-start` launched the
bottom panel and it did not. If a feature here has no line in `TEMPLATES[]`, it
does not run, whatever any comment says.

**Windows land in the usable area because labwc computes one.** Placement,
snapping and maximise all honour layer-shell exclusive zones upstream — the
old `kc_usable_at()` lesson (a maximised window must not cover the panel) is
now somebody else's regression test.

**The wallpaper is the compositor's, not a client's.** `kdos-wallpaper.c`
decodes the PNG with libpng and puts one `wlr_scene_buffer` per output at the
bottom of the scene tree, rebuilt from the `output_update_for_layout_change()`
hook. The usual answer is a layer-shell client (swaybg) and it is wrong here:
libkwl paints CELLS, so a wallpaper client would be the one program in this
desktop that is not a character grid. Three things it has to get right — there
is no public "wlr_buffer from memory", so the file carries the smallest
`wlr_buffer_impl` that works (data-ptr access only, which is what both
renderers use to upload a texture); libpng hands back R,G,B,A in memory order
while `DRM_FORMAT_ARGB8888` wants B,G,R,A; and libpng reports a bad file by
`longjmp`ing back **after** the allocation, so only an initialised buffer may
leave `decode_png` (its locals are `volatile` for exactly that path). Scaled
to COVER and centred via the source box, `wallpaper = none` in comp.conf an
honest off.

**A view on another workspace is re-reported as MINIMIZED** over
wlr-foreign-toplevel (`kdos_foreign_workspace_sync()`, hooks in
`workspaces_switch_to` and `view_move_to_workspace`, plus
`view_toggle_visible_on_all_workspaces` — omnipresence is an input to the
predicate, so it resyncs there too, though labwc moves omnipresent views to
the current workspace on every switch and the toggle is normally a no-op).
The pre-fork compositor
did this and kdos-shell is built on it twice over: the panel's workspace-
occupancy derivation counts non-minimized tasks, and the window list dims
minimized ones. Stock labwc reports them unminimized, which made every visited
workspace look occupied while any window existed anywhere.

## Lock, idle and power

`ext-session-lock-v1`, `ext-idle-notify-v1` and `idle-inhibit` are labwc
upstream code now (`src/session-lock.c`, `src/idle.c` in the fork), plus three
binaries of ours: `kdos-lock`, `kdos-checkpass` and `kdos-powerd`/`kdos-power`.

**The compositor owns `locked`, not the client — and labwc agrees.** Its
`manager->locked` stays true when the lock client dies without unlocking
("abandoned lock"): the lock output trees keep covering every screen, and a
NEW lock client may replace the abandoned one — which is exactly the recovery
`kdos-lock` needs after a crash. A lock screen that unlocks when it crashes is
the failure mode the protocol exists to remove; verified semantics, not
assumed. The old per-surface lessons (locked sent exactly once, count mapped
surfaces, no null-buffer commit on a lock surface) are upstream's code paths
now — but the libkwl half of the last one is still OURS: `kwl_init` skips the
pre-configure commit for `KWL_ROLE_LOCK`, because that empty commit is a
protocol error on a lock surface.

**The idle POLICY is a graft** (`kdos-crt.c`'s sibling, `kdos-idle.c`): one
timer, dim → lock → outputs off, each stage measured from the LAST ACTIVITY,
never from the previous stage. Activity ends the dim and powers outputs back
on; it never unlocks. An inhibitor stops the policy dead. The hook is labwc's
`idle_manager_notify_activity()` — the single funnel every input path already
goes through — plus one call in each idle-inhibitor handler. The dim is a
translucent scene rect raised to the top (lock trees re-raised above it), not
a gamma change. All three timers default to 0 in a VM (DMI `sys_vendor`, or no
`wlr_session`) unless any `idle_*` key is set in comp.conf — a blanked screen
over VNC is indistinguishable from a crashed compositor.

**libkwl's lock role covers every output** — the protocol will not report the
session locked until they all have a surface. libktui has ONE cell buffer, so
the prompt is on the first output and the rest are filled with `KT_BG`. That is
a libktui limitation, said out loud, not a protocol one.

**`kdos-checkpass` is the only setuid binary KDOS ships**, and it takes NO
ARGUMENTS — not even a user name. The account checked is the caller's real uid,
so there is nothing to aim at root and nothing an attacker can vary; the
password arrives on **stdin** (`kb_run_feed`, new in libkbase) because argv is
world-readable through `/proc/<pid>/cmdline`. Privilege is dropped as soon as
the hash is read, the compare is constant time, and `!`, `*` or an empty hash
always fail. Exit codes are 0 / 1 / 2 and the caller must distinguish them: a
"wrong password" for a machine with a broken `/etc/shadow` sends the user
looking in the wrong place.

**`kdos-powerd` is a root daemon on `/run/kdos-powerd.sock`, gated by
`SO_PEERCRED`** — root and `wheel`, checked on the socket rather than taken
from the message, so there is nothing to forge. One word per connection
(`suspend`, `poweroff`, `reboot`, `ping`); poweroff and reboot signal pid 1
first (toybox init: SIGUSR2 poweroff, SIGTERM reboot) and only then call
`reboot(2)`. `kdos-powerd --explain <user>` answers "would this user be allowed,
and why" and is what a dead power key gets diagnosed with.
`KDOS_POWERD_SOCKET` moves the socket for testing and grants nothing —
authorisation never depended on the path.

**`kdos doctor` checks the setuid bit**, because losing it is the worst failure
in the system and a silent one: `kdos-checkpass` without it refuses every
password and locks the user OUT of their own session. An `rsync` without `-p`
is all it takes.

---

## The CRT pass

`src/desktop/kdos-comp/src/kdos-crt.c` — ported whole onto the labwc fork, and
**ON by default** (`crt = 55`, scanlines 60, curve 0). The CRT stopped being
four separate imitations and became one system property: the compositor
renders the desktop through a phosphor shader, so the boot splash, the TTY and
the session are finally the same machine. **The wallpaper's baked scanlines
were deleted** in the same change. Fork hook points: `kdos_crt_early_init()`
before `wlr_scene_create()` (main.c), the commit routed through
`kdos_crt_frame()` in labwc's `handle_output_frame` (output.c, falling back to
`lab_wlr_scene_output_commit` when the pass declines), and `kdos_crt_reload()`
in `handle_sighup` so `kdos theme <accent>` retints the shader live.

**wlroots has no shader API** — `wlr/render/pass.h` is `add_texture` and
`add_rect`, and wlr_scene has three node types and no callback node. It does have
a documented seam: `wlr_scene_output_build_state()` takes an options struct whose
`.swapchain` field is *"Allows use of a custom swapchain."* So the scene
composites into a buffer of ours, and our own GLES2 program blits that buffer
into the output's real buffer with the effect applied. Both swapchains come from
`wlr_output_configure_primary_swapchain()`, so neither needs format or modifier
guesswork of ours.

**Direct scanout is turned OFF for the whole session while the pass is on**, and
that is correctness, not tuning. When wlr_scene takes the scanout path it hands
the commit a *client's* buffer plus a `buffer_dst_box` — not a picture of the
desktop — and the first version of this file stretched a 13-pixel-tall panel over
the whole screen. wlroots exposes exactly one switch, an env var read by
`wlr_scene_create()`, so `kc_crt_init()` sets it and is called before the scene
is created. A foreign buffer arriving anyway is committed unprocessed rather than
mangled.

**The texture is imported per frame and destroyed after the pass.** Caching it
per swapchain slot is the obvious optimisation and it deadlocks the swapchain:
`wlr_texture_from_buffer()` LOCKS the buffer, a slot is only reused once its last
lock goes, and four cached textures means `[ERROR] render/swapchain.c:95: No free
output buffer slot` and a scene that stops rendering. Measured, not reasoned
about.

**Two fallbacks, and neither can produce a black screen.** A renderer that is not
GLES2 gets no pass at all — pixman is software, `make run` has only that, and a
fullscreen post-process there is a slideshow — reported at startup. Anything that
fails at runtime marks that *output* broken and returns to
`wlr_scene_output_commit()` for good, because 60 identical error lines a second
is worse than missing scanlines.

**The magnifier takes the frame instead, whole.** labwc draws its magnified
inset inside `lab_wlr_scene_output_commit()` — the call this pass replaces — so
with the pass on, a magnified frame lost the inset whenever the scene needed
redrawing and kept it whenever the scene was static (a hardware cursor damages
nothing): a flicker between two different pictures. `kdos_crt_frame()` declines
while `magnifier_is_enabled()`, which is also the right answer on its own — an
accessibility zoom read through scanlines and a three-tap bleed is harder to
read, not easier. Nothing else in `output->pending` is lost by the bypass:
every writer of that struct commits it itself (`output_state_commit()`, or
`configure_new_output()`'s own scene commit), and the only field the frame
handler leaves there is `tearing_page_flip`, which is meaningless under a
fullscreen post-process.

The shader is scanlines every third **physical** row (the splash's and the old
wallpaper's period), a three-tap horizontal bleed, a vignette, optional barrel
distortion and a faint phosphor floor so black is never quite black. **No
persistence and no real bloom** — a history buffer and two blur passes
respectively, and the fill rate is not free. The curvature is normalised by the
corner displacement so no value crops the desktop; without that divisor,
`crt_curve = 100` blacked out the corners and ate the panel's top row. Colours
come from libkcolor's `KCOL_SCHEMES`, and the accent is read from
`$XDG_CACHE_HOME/kdos/theme` — the same one-word file kdos-shell reads — at
startup and again on SIGHUP: `kdos theme amber` retints the running shader in
the same signal that repaints the panel (verified on the fork).

`crt` / `crt_scanlines` / `crt_curve` in comp.conf, percentages, `crt = 0` an
honest off. **`KDOS_CRT_DUMP=<prefix>`** writes `<prefix>-in.ppm` and
`-out.ppm` once — the composite and the result — with `KDOS_CRT_DUMP_FRAME=<n>`
to wait past the empty first frame. That is how the pass gets looked at without a
screen, the same reason kdosbuild has `--preview`. The input is read back through
the texture the shader sampled, not the buffer, because that buffer is not always
renderable.

Sync is the one part only hardware can confirm: the composite and the pass share
wlroots' single GLES2 context, so ordering is free, but the commit relies on
implicit dmabuf fencing after `glFlush()` — a hand-built state cannot carry the
scene's wait timeline. Getting it wrong shows up as a torn frame, not an error.

## Stutter attribution — `kdos stutter`

Three sources, none of them an answer alone, joined:

| Knows | Does not know | Where |
|---|---|---|
| a frame was late, by how much, and what the compositor's own render cost | who did it | `kdos-comp/src/kdos-frames.c` |
| the machine was starved, and of what | by whom | `/proc/pressure/*` |
| who burned CPU and who sat in D state | that anyone cared | `/proc/<pid>/stat` |

The closest prior art (Latency Lens) reads PSI and says outright it *"cannot
identify which specific process caused a frame miss"*. That sentence is what this
finishes: **"7 frames dropped on eDP-1 (133 ms) — the busiest just then:
calibre-idx (appbox kdos-apps) waiting on the disk, calibre (appbox kdos-apps) at
92% of a core."**

**The compositor reports on `$XDG_RUNTIME_DIR/kdos-frames.sock`**, one NDJSON
object per late frame. Presentation events are the truth where the backend has
them (`wlr_output.events.present` carries when content turned into light, plus
the refresh interval); headless and nested backends do not present, so the
fallback is the frame clock, and the two are reported with a `source` field
rather than averaged — a present gap is what the user SAW, a frame gap is what
the compositor was GIVEN. The threshold is 1.5 refresh intervals.

**The socket must never slow the frame loop.** Non-blocking both ends; a consumer
that cannot keep up loses lines. No history either: a consumer that connects late
has missed what happened, and a ring buffer would hide that. Not a Wayland
protocol on purpose — it is one distro's channel between two of its own programs,
and `presentation-time` already exists for clients that want their own numbers.

**`render_ms` is what separates the two explanations.** Over 60% of the frame
budget and `kdos stutter` says *the desktop itself was late* — the one causal
claim it makes, because there it has both halves. Otherwise it reports what it
measured and names who was busy, and it **never says "X caused this"**:
attribution from a 500 ms sample window is circumstantial, and a tool that
claimed otherwise would be wrong the first time two things were busy at once.

Two details that are not obvious:

- **Blocked before busy.** A process asleep in `D` shows almost no CPU while it
  is the thing holding the disk, so sorting on CPU alone hides exactly the case
  the io half exists for.
- **Container names come from `conmon`'s argv, not from cgroups.** No systemd
  means rootless podman gets no cgroup delegation and the box frequently sits in
  `0::/`, which says nothing. Walking the ppid chain to the `conmon` that
  supervises it and reading `-n <name>` costs a few file reads and no podman
  call — which matters, because this runs while the machine is already
  struggling.

**`--fixture` is what makes an attribution engine testable.** The whole sampler
reads from a directory that defaults to `/proc` and does not have to be one:
`testing/fixtures/stutter` is two snapshots 500 ms apart plus the events
kdos-comp would have sent between them, and `testing/selftest.sh` asserts the app
is named with its box, the disk waiter is first, PSI is quoted, and exactly one
of the two events is blamed on the compositor. Proved live as well, on a real
compositor: three `SIGSTOP`/`SIGCONT` stalls produced three reports of 23 dropped
frames each (~410 ms at 60 Hz) with the spinning process named first.

`KDOS_COMP_DEBUG=1` puts kdos-comp's log at DEBUG, where every miss is also
logged — a session log with a stutter in it is worth something even when nothing
was listening. `kdos doctor` reports whether the socket is there at all.

### Ending the session

labwc handles `SIGTERM`/`SIGINT` through `wl_event_loop_add_signal` and its
teardown removes its own listeners — the "wlroots asserts nothing is still
listening at destroy" lesson is upstream's to keep now. What is OURS on that
path: `main.c`'s shutdown hooks run the graft teardown in order
(`kdos_wallpaper_finish` → `kdos_frames_finish` → `kdos_idle_finish` →
`kdos_crt_finish`) before the server dies, and the shell/notifyd notice the
dead compositor themselves — libkwl's `kwl_pump` uses the documented
prepare_read/read_events sequence, so a closed socket is seen instead of spun
on (see **the shell**'s notifyd notes).

---

## Screen capture, the clipboard and the portal

**All the capture and clipboard globals are labwc upstream code now** — both
generations of each: `zwlr_screencopy_manager_v1` (released grim 1.4.1),
`zwlr_export_dmabuf_manager_v1`, `ext_image_copy_capture_manager_v1` with the
output and foreign-toplevel capture sources (what `xdg-desktop-portal-wlr`
prefers, and per-WINDOW capture), and both data-control managers (wl-clipboard
2.2 and 2.3). Implementing only one generation strands either the shipped
grim or every client written after 2024; labwc carries both, which is part of
why the fork won.

**A boxed app is kept off them by labwc's security-context filter**
(`allow_for_sandbox()` in `src/server.c`): a client tagged through
`kdos-boxsock` gets a static allowlist — surfaces, seat, dmabuf, text-input,
primary selection — and none of the capture, data-control or input-method
globals. Verified against the fork: a tagged `grim` cannot bind screencopy
while an untagged one shoots the screen. **That is what makes the portal the
sanctioned route rather than a convenience**: a boxed OBS cannot bind
screencopy at all, so it asks the portal, which runs on the host and asks the
user which output to share. (The legacy per-box `wayland.*` grant keys in box
profiles have no labwc equivalent — a box either is sandboxed or is not. Gap
stated, not hidden.)

**A screenshot of a phosphor desktop looks like the desktop.** Output capture
copies the output's committed buffer, which under the CRT pass is the processed
one. Per-window capture renders the scene node and is untinted. Both are the
honest answer to what was asked.

**The portal is `ports/core/xdg-desktop-portal-wlr` 0.8.4** — sd-bus from basu,
`--libexecdir=/usr/lib` so both daemons sit where `kdos-desktop-start` looks.
`wayland-protocols` needed no bump: 1.48 already carries every staging protocol
it and wlroots ask for.

**`fs/usr/share/xdg-desktop-portal/kdos-portals.conf` is not optional and its
NAME is not free.** xdg-desktop-portal looks for `<desktop>-portals.conf` with
`XDG_CURRENT_DESKTOP` lowercased, so `KDOS` → `kdos`. Without the file the only
thing selecting a backend is `UseIn=` inside `wlr.portal`, which lists sway,
river and Hyprland and has never heard of us. It reads `default=none` — honest
rather than lazy, because the usual second backend is xdg-desktop-portal-gtk and
**there is no GTK on the host**, so Print, Email and Wallpaper genuinely have
nobody — plus ScreenCast and Screenshot to `wlr` and FileChooser and Settings
to `kdos`.

**FileChooser, Settings and OpenURI are ours:
`src/desktop/xdg-desktop-portal-kdos`.**
It is a bus adapter and nothing more — it spawns `kdos-pick` and reads its
stdout, so the chooser stays a normal program that can be run by hand,
scripted or replaced. Before it, every boxed application's Open and Save fell
back to whatever dialog its own toolkit shipped, which put a rounded
antialiased GTK window in the middle of a text-mode desktop every time anybody
opened a file. Three rules it exists to keep: **every request is answered** (0
success, 1 cancelled, 2 error — an unanswered request leaves the application
blocked forever, the same lesson as the ScreenCast zero-streams trap on a
different interface); `parent_window` is ignored and says so; and the chooser
is exec'd with argv, never a command string.

**And the bus loop does not block on the dialog.** The first version forked the
picker and sat in `waitpid()` inside the method handler, so for as long as
anybody had a file dialog open this backend answered nothing at all — a second
application's Open queued behind the first, and a boxed app asking Settings for
the colour scheme (which happens on every launch) hung until the dialog was
dismissed. The fork happens in the handler, the `sd_bus_message` is REFFED and
the handler returns "handled" without replying, the pipe joins the main loop's
poll set, and the reply is built at EOF. A portal backend is a server, and a
server that stops serving while it thinks is a server that is down.

**`OpenURI` is "open this on the host for me", and it had no backend at all.**
That is the request every containerised application makes when a link or a
downloaded file is clicked, `default=none` answered not-supported, and the click
therefore did nothing — silently. There is nothing to write beyond the
plumbing: **`kdos-appbox open` IS the implementation** of what opens a thing on
this machine, so the method decodes the URI's percent-escapes and forwards a
path to it, and hands anything that is a scheme rather than a path to
`xdg-open`. `OpenFile`/`OpenDirectory` take an FD because the application may
not be able to NAME the file in terms the host would recognise — it is in a
container with its own mounts — and `/proc/self/fd` is the only translation
there is; a link ending in ` (deleted)` is refused rather than opened as a file
of that literal name. No chooser and no prompt: the front end has already
decided the application may ask, and a second dialog here would be a permission
question asked by the half of the stack that does not know the answer.

**And none of that is reached unless the app thinks it is sandboxed** — which
is why `kdos-appbox` exports `GTK_USE_PORTAL=1`; see the appbox section.

`~/.config/xdg-desktop-portal-wlr/config` (skel) uses `chooser_type=simple` with
slurp. The alternative, `none`, silently captures the first output — right on a
laptop, wrong the moment a second screen is plugged in, and wrong in the
direction where the user never finds out.

**The `captest` proof was of the PRE-FORK compositor** — a client that is its
own subject: window of a known colour, captured both ways, pixels checked,
28800/28800 under both renderers. The binary was never committed, so it was
not rerun against the fork; what was verified on the fork is `grim` shooting
the screen and the sandbox denial above. One fact of captest's outlives it:
the headless GLES2 path advertises **BGR888, three bytes per pixel**, and a
client that assumes four gets `Invalid stride`.

## Input methods — the compositor is the wire

**The wire is labwc upstream code now** (`src/input/ime.c` in the fork):
text-input-v3 for the application, input-method-v2 for the engine,
virtual-keyboard-v1 for the route back, all three globals created and relayed
by code that sway and labwc users have exercised for years. The semantic rules
the old `textinput.c` was written around — one IM per seat, enter only for the
focused surface's client, compositor bindings before the grab, a virtual
keyboard's keys never fed back to the grab, nothing crossing a lock — are the
protocol's rules, and labwc's implementation is the one the ecosystem tests
against. The pre-fork `imtest` proof (15 assertions, both parties in one
client) was of our from-scratch wire; its binary was never committed and was
not rerun — what was verified on the fork is fcitx5 itself binding
`zwp_input_method_manager_v2` in the session.

**Sandboxed clients cannot BE an input method** — `zwp_input_method_manager_v2`
and `zwp_virtual_keyboard_manager_v1` are outside labwc's `allow_for_sandbox`
allowlist (a keylogger by design, since the grab delivers every keystroke on
the seat), while `zwp_text_input_manager_v3` is deliberately inside it: that
is the application half, and denying it would deny input methods to the boxed
apps that need one most.

**The engine is `fcitx5`, ported** — see **The input method** below for the
build and where each half lives. What this section describes is the wire; test
it with any input-method-v2 client.

## The input method — fcitx5, and where each half lives

Three parties that never speak to each other, and the compositor is the wire
between them (see **Input methods — the compositor is the wire** above for the
protocol side). `fcitx5` is the party on the other end of `input-method-v2`.

```
fcitx5  ──input-method-v2──▶  kdos-comp  ──text-input-v3──▶  the application
      ◀──virtual-keyboard-v1──                                (host or boxed)
```

**The build is Wayland-only, and every flag that says so is load-bearing:**

| | |
|---|---|
| `ENABLE_X11=Off` | the hard rule. X11 here would pull xcb-imdkit, cairo-xcb, xkbfile and seven xcb components onto the host for an XIM frontend nothing can use. X11 apps reach fcitx5 through Xwayland and text-input-v3 like everything else |
| `EVENT_LOOP_BACKEND=libuv` | the alternative is systemd. `USE_SYSTEMD=Off` alone still lets `auto` find one |
| `BUILD_SPELL_DICT=Off` | it `file(DOWNLOAD)`s a dictionary at build time — the one thing in this tree that would reach the network under `--network none` |
| `ENABLE_XDGAUTOSTART=Off` | KDOS runs no autostart agent; `kdos-desktop-start` launches fcitx5 by name |
| `ENABLE_ENCHANT=Off` | word prediction for Latin scripts, and a port that exists for nothing else |
| **`ENABLE_TESTING_ADDONS=On`** | **not an oversight.** Every engine port does an unconditional `find_package(Fcitx5Module REQUIRED COMPONENTS TestFrontend)`, so turning the testing addons off makes fcitx5-hangul, -anthy and -chinese-addons all fail to CONFIGURE. Three small addons |

**One language per engine, and the Chinese one costs a Boost.**
`fcitx5-chinese-addons` (pinyin, shuangpin, and the table methods — cangjie,
wubi, erbi, zhengma…) sits on `libime`, which needs Boost's headers *and*
`Boost::iostreams`; that is a 188 MB tarball in LFS for one compiled component,
and the recipe builds `--with-libraries=iostreams` and nothing else. It buys the
input method for the largest language population there is, and there is no
smaller road to pinyin — librime wants Boost too. `fcitx5-anthy` over
`anthy-unicode` is Japanese, `fcitx5-hangul` over `libhangul` is Korean, and
neither needs it.

**Two decisions in the engine builds are policy, not packaging:**

- **`ENABLE_CLOUDPINYIN=Off`.** Cloud pinyin sends what you are typing to a
  remote service to be completed. That is keystrokes leaving the machine, on a
  distro that builds offline and boxes its applications so nothing has to phone
  home.
- **`ENABLE_GUI=Off` / `ENABLE_BROWSER=Off`.** The configuration tool is Qt
  Widgets, and with the browser also QtWebEngine. There is no Qt on the host.
  fcitx5's config is text under `~/.config/fcitx5/`.

**libime's data is 49 MB fetched at build time upstream**, in three tarballs —
the language model, the dictionary and the table set. `Fcitx5Download.cmake`
skips the fetch when the file is already there and resolves the name against the
SOURCE directory, so the port carries all three as extra `source =` lines and
copies them into `data/` before cmake runs. That is the whole of making it
offline, and it was verified by watching `sc.dict` and eight table dictionaries
come out of a build with no network in it.

**A boxed app reaches the input method through the compositor and never
directly.** `kdos-appbox` exports `QT_IM_MODULE=wayland` and — deliberately —
sets **no** `GTK_IM_MODULE` at all: GTK on Wayland picks the right context by
itself when the variable is unset, and setting it is how a working GTK app stops
accepting CJK. Neither is ever `fcitx`: that is the X11-era route where each
toolkit talks to the IM daemon itself, and inside the container that daemon does
not exist.

**Proved by running it**, against a headless `kdos-comp` on the pixman backend:
fcitx5 loads its `wayland` and `waylandim` addons, binds
`zwp_input_method_manager_v2`, creates a `zwp_input_method_v2` and a
`zwp_virtual_keyboard_v1` — the forwarding route the compositor half was built
for — and loads the `pinyin` engine against the dictionary generated offline.
All four engines register their input-method metadata (`pinyin`, `shuangpin`,
`anthy`, `hangul`, plus eight table methods).

**Known gaps, stated rather than hidden:**

- **No `fcitx5-configtool`** — it is Qt. Configuration is the text files.
- **The candidate window is fcitx5's own**, drawn with cairo and pango through
  `get_input_popup_surface`. It is the one thing on this desktop that is not a
  character grid, and making it one would mean writing an input method.
- **`musl-locales` needs GCC 14 or newer** (`-std=c23`); KDOS ships 15, so it
  builds there and not on an older host. It installs a `locale` command and
  message catalogues — musl implements `LC_MESSAGES` and nothing else, so
  `LANG=zh_CN.UTF-8` starts giving translated program output and still has no
  collation and no localised `strftime`.

## Which app is using your microphone

`src/desktop/kdos-shell/privacy.c`. The panel names it:

```
 KDOS │ 1 2 3 4 │ foot  firefox │            ●MIC pw-cat  ●CAM firefox +1  41%  23:31
```

Every phone OS answers this and no Linux desktop does, and the reason is not
difficulty — PipeWire knows the capture node, the node knows the client, the
client knows its own name, the panel knows how to draw. It is **four owners for
one indicator and nobody owns all four**. KDOS owns all four.

**Two sources, because there are two ways to record and only one goes through
PipeWire.**

| | Where it comes from | Why not the other way |
|---|---|---|
| microphone | a PipeWire node with `media.class` = `Stream/Input/Audio`, counted only while its state is RUNNING | `/proc` says *pipewire* holds `/dev/snd/pcmC0D0c`, which is the non-answer every other desktop gives |
| camera | a process holding an fd on `/dev/video*`, found by walking `/proc` | almost nothing takes the camera through the portal; PipeWire would report nothing at all |

**A node that EXISTS is not a node that is RECORDING.** An app that opened the
mic and went idle must not light the lamp — an indicator that cannot tell those
apart is one nobody believes twice. The camera half is counted the other way
round on purpose: an open fd on a camera *is* use, there being no other reason
to hold one.

The name is the app's own — `application.name` ("Google Chrome", not "chrome"),
then `node.name`, then `/proc/<pid>/comm` for the camera. One app is named and
the rest are counted (`●MIC firefox +2`): the panel is one row, and three
truncated names say less than one name and a number.

Drawn in the SECONDARY colour, and the camera additionally reversed — the one
thing on this panel that is a warning rather than a fact, and a camera is more
consequential than a mic.

Three details worth keeping:

- **`--dump` waits, the live panel does not.** Connect, registry, bind and info
  are four roundtrips; a one-shot dump that asked once would report "nothing is
  recording" because it asked too early, so `sh_priv_settle()` pumps up to
  800 ms for it. The live panel pumps the loop FOUR times per tick instead of
  once — measured, that is what puts a stream that just started on the panel in
  one tick rather than two.
- **The node record lives in the proxy's user data**, so `pw_registry_bind`
  allocated it and the destroy handler must unlink it and NOT free it.
- **`KDOS_PRIVACY_PROC` moves the `/proc` walk**, the same trick
  `kdos stutter --fixture` uses, which is what makes the camera half testable on
  a machine with no camera. `testing/fixtures/privacy/proc` is three processes:
  one holding `/dev/video0` twice (named once), one holding `/dev/video1`, and
  one holding an ALSA capture device that must be ignored.

Cost: `kdos-shell` links `libpipewire-0.3` and the port depends on `pipewire`.
The camera walk runs every two seconds — a readdir per process, and there is
nothing to subscribe to.

## Per-app Energy Impact — `kdos-energyd`

Windows 11, macOS and Android all ship per-app battery attribution. No Linux
desktop does, and the reason is not the measurement — RAPL and the cycle-share
model are twenty years old. It is **identity**: "Firefox" is forty processes in
scattered cgroups and nothing on a normal desktop owns enough of the system to
name them. On KDOS every fat application already runs inside its own container
whose supervisor knows its name, so the expensive half is free here and only
here.

```
KDOS energy  —  2.1 h of samples, RAPL package-0

  firefox-esr (appbox kdos-apps)          75.5%  ███████████████       gpu 75.0%
  kdos-comp                               15.4%  ███                   gpu 25.0%
  short-lived and exited processes          8.7%

  shares are of ATTRIBUTABLE energy — 57% of the package total; the rest is the idle floor
  idle floor 15.00 W, the lowest average power seen in 3 samples
```

**`src/desktop/kdos-energyd/`** is a root daemon on `/run/kdos-energyd.sock`
(`rapl.c` the counter, `attrib.c` the attribution, `report.c` what the numbers
may say) plus a `kdos-energy` client, basename-dispatched — the kdos-powerd
shape, including the `SO_PEERCRED` gate, which is **libkbase's
`kb_user_in_group` now** rather than a copy in each daemon.

**Relative, never watt-hours.** RAPL measures the CPU package. It cannot see the
panel — the largest single draw on a laptop — nor the radio, the SSD, or a
discrete GPU. "GIMP was 41% of attributable CPU energy today" is a measurement;
"GIMP used 12% of your battery" is a guess wearing a unit.

Six decisions, each of which changes the answer:

- **Nested RAPL domains are dropped.** `/sys/class/powercap` lists
  `intel-rapl:0` (a package) flat beside `intel-rapl:0:0` (that package's core
  domain), and summing the listing counts the cores twice — measured on the
  fixture: 15 W becomes 26.25 W. A domain is a subdomain exactly when it appears
  *inside* another's directory. `psys` goes the other way: it contains the
  packages, so where it exists it replaces them.
- **The counter wraps**, about every 36 minutes at 30 W, and a naive subtraction
  produces one enormous negative per wrap with nothing in the output saying so.
- **The idle floor is subtracted before anything is attributed.** A package
  burns watts with nothing running, and a cycle-share model that skips this
  reports a machine at a login prompt as 90% one process. The floor is the
  lowest average power seen — a measurement, printed with the answer.
- **The floor is applied at REPORT time, not per window.** It can only fall, so
  charging each window the floor as it stood then throws away the first window
  entirely — which is usually the busiest, because something was just launched.
  Each app instead carries two weighted sums (`we`, `wt`) and the report computes
  `we − floor·wt` once, with the floor as it finally stands.
- **The denominator is `/proc/stat`, not the sum of the surviving pids.** A build
  that starts and exits inside one window is gone by the second sample, and
  dividing by the survivors would hand its energy to them. The difference is a
  real quantity and gets its own line.
- **The GPU column is engine TIME from `drm-engine-*` in fdinfo, never energy.**
  Nothing on the machine says what that time cost in joules. On a part with
  integrated graphics it is already inside the package number (detected by an
  `uncore` subdomain); on a discrete card it is outside RAPL entirely and the
  report says so. A driver with no fdinfo stats — the proprietary nvidia one —
  gets **no column**, not a column of zeroes.

**Why a daemon, and why the socket is not an oracle.** RAPL is a free-running
counter, so a one-shot tool can only report what happened while it was watching;
and it has been root-only since Linux 5.10 closed PLATYPUS (CVE-2020-8694),
where fine-grained unprivileged reads recover AES keys. What leaves this process
is a per-app percentage over minutes — the raw counter and the interval are
never republished, and the interval is **fixed at 10 s by the daemon rather than
requested by a client**, so it cannot be driven toward being one. It is a
sampler only: no write path into powercap at all, and no argument from any
client. Answers go to root and `wheel` and to nobody else — on a multi-user
machine this list is what everyone else is running. The gate is `SO_PEERCRED`,
**not the socket's mode**, which is 0666 exactly as kdos-powerd's is: anyone may
connect and anyone unauthorised gets `err not permitted`. A mode that looked
like the authorisation is a mode somebody eventually loosens.

**`56_energyd.sh` checks for a readable energy domain before `supervise` sees
the daemon.** Most VMs have no RAPL; the daemon refuses to start there rather
than report a machine that uses no energy at all, and a refusing daemon under a
respawn loop is a boot that never settles.

**`--fixture` is what makes it testable**, the same seam as `kdos stutter` and
`KDOS_PRIVACY_PROC`: `testing/fixtures/energy/{0..3}/` are recorded `proc` and
`powercap` trees, and `testing/selftest.sh` asserts the floor (the nesting), the
attributable fraction (the wrap), that `Web Content` is rolled onto
`firefox-esr (appbox kdos-apps)` and appears nowhere itself, and that the
short-lived residue is its own line. Both traps were confirmed to bite by
building the daemon with each one disabled.

**Not built: a panel indicator.** The milestone's deliverable is the sentence,
and `kdos-shell` drawing a share strip is its own piece of work.

## The shell answers the mouse, everywhere

`kdos-shell` is one binary under eleven names: `kdos-shell` (the panel, and
`--bottom` the second one), `kdos-launcher`, `kdos-menu`, `kdos-desk`,
`kdos-pick`, `kdos-run`, `kdos-prompt`, `kdos-notifyd`, `kdos-osd`,
`kdos-cal` and `kdos-display`. Four of
them shipped with no mouse handling at all — `grep -c KT_EVT_MOUSE` was 0 for
launcher, pick, run and notifyd — and one of those is the file dialog **every
boxed application reaches through the portal**. The one dialog on the system
that other people's software puts in front of you was the one that could not be
clicked.

One contract, so a hand that learns it in the menu already knows the launcher:

| | |
|---|---|
| motion | selects (it arrives as `KT_MP_DRAG` — libkwl's spelling for plain movement, and testing `ev.press` for truth made every mouse MOVE a click) |
| left press | activates |
| wheel | scrolls the selection |
| right press | backs out one level, then closes |
| click away | closes — see below |

**A keyboard overlay that loses focus closes itself** (`kb_leave` in libkwl,
gated on having seen an ENTER first, because the compositor decides when an
ON_DEMAND layer surface gets the keyboard and a leave before any enter would
close the surface during its own map). Before that, clicking on a window while
a menu was open left the menu floating over it until somebody found Escape.
There is no "unfocused menu" state worth having.

**A menu opens under the word that was clicked.** Layer-shell has no
coordinates, so "at x" is an anchor plus a margin: `KWL_CORNER_TOP_LEFT` with
`margin_x`/`margin_y` in pixels, which the panel passes as `--at X Y`. Without
it every menu opened in the CENTRE of the screen and read as a dialog.

**The panel lights the word the pointer is over.** `menu_open` is never set by
anything — the menu is a separate process and does not report back — so hover
is what the bar actually knows, and it is what makes three words read as three
buttons.

**The window list answers all three buttons**, as taskbars have since Windows
95: left toggles (minimise the window you are in, restore the one you are not),
middle closes politely so an editor still gets to ask, right minimises. The
workspace strip answers the wheel, which is also the only way to reach
workspace 5 and up — the strip has room for four digits and does not scroll.

**The taskbar shows the desktop entry's `Name`.** `Name` > title > `app_id`,
resolved once when the app_id arrives rather than per frame. An app_id is a
reverse-DNS identifier chosen so it cannot collide, and `org.gnome.Meld` was
sitting in the one place a human name belongs. The entry is found by its ID
first and then by **`StartupWMClass`**, which is what an X11 client under
Xwayland needs: it reports its WM_CLASS as its app_id, and the entry that owns
that names it in `StartupWMClass` rather than in its file name (GIMP's says
`gimp-3.0`; the toplevel says `gimp`). The second pass is a directory scan and
runs only when the cheap lookup missed, once per window.

**The right wing of the panel is CONTROLS, not a picture.** Clock and battery
open `kdos-cal`; `VOL 62%` mutes on a click and takes ±5% from the wheel;
`NET wlan0` opens `foot -e nmtui`; the restart mark opens `kdos restarts`. Each
records the span the last frame drew it in, and an applet with no room records
an EMPTY one — a hit map that outlives what it describes is how a narrow screen
mutes itself when somebody aims at the clock. The volume is `kdos-osd`'s ALSA
mixer, cached open and re-read through `snd_mixer_handle_events` (a panel asks
once a second; open+attach+load per tick is a mixer rebuilt sixty times a
minute), and the network is `/sys/class/net` for the same reason the battery is.

**The workspace strip records where it drew each digit.** A two-cell stride is
right only while every label is one digit wide, and `<desktops number="12"/>`
is a supported thing to write — from the tenth on, the strip activated the
wrong workspace.

**`kdos-desk` claims only the cells its icons occupy** (`kwl_input_cells`).
It covers the whole output, so with the default input region it ate every click
on bare wallpaper — and labwc's root-menu mousebind therefore never fired for
anybody running with desktop icons on, which is the shipped default. The region
is recomputed only when the layout changes, because setting one is a surface
commit and doing it per frame puts back the cadence the flicker fix removed.
Its right-click context menu is drawn INTO its own grid (it owns the screen; a
popup here is not a second surface), and while that is up the whole surface is
claimed so a click-away lands on it rather than on the compositor.

**A toast dismisses on click**, with the spec's reason 2. The surface takes no
keyboard — a toast must never steal focus — so the pointer is the only way to
make one go away early, and a notification that cannot be dismissed is one
people learn to work around by not looking at that corner of the screen.

**`kdos-prompt` is the yes/no dialog**, and its interface is its EXIT STATUS
because that is what kdos-comp already reads: 0 yes, 1 no, 254 cancelled. It is
what `W-Escape` and the System menu's Log Out / Restart / Shut Down go through.

## The tray — a StatusNotifierItem host

`src/desktop/kdos-shell/tray.c`. The tray is the one part of a desktop that is
pure D-Bus: an app publishes an ITEM, a WATCHER keeps the list, a HOST displays
it. KDOS is all three, because nothing else here is — and it matters more here
than elsewhere, since **a boxed app that minimises to a tray which does not
exist has minimised to nowhere** (kdeconnect, keepassxc, nextcloud, syncthing).

An item is **one cell**: the first letter of its `Id`, coloured by `Status` —
dim for Passive, the text colour for Active, the accent AND reversed for
NeedsAttention. `Id` rather than `IconName` because a letter from a name a human
chose beats a letter from a theme lookup that will never happen on a character
grid. Left click sends `Activate`, middle `SecondaryActivate`, right
`ContextMenu`.

Three rules, each one a bug that was already there:

- **Nothing blocks the panel.** Property reads have a 300 ms ceiling and use one
  `GetAll` rather than five `Get`s (five against a wedged app is a second and a
  half of dead panel); every method call to an item is fire-and-forget.
- **Properties are never read from inside a bus callback.** A synchronous
  `sd_bus_call` there does not get its reply — sd-bus is already processing a
  message — so registration marks the item `needs_props` and the next dispatch
  reads it. The first version drew items with no name at all.
- **The interface spelling is only recorded when the OTHER one answered.** KDE
  apps publish `org.kde.StatusNotifierItem`, a few publish the freedesktop
  spelling. Flipping on the first failure sent every subsequent click to an
  interface the app did not implement — and a fire-and-forget click reports
  nothing, so it failed in total silence.

**When something else owns the watcher, we adopt its list.** waybar might, and
so does the live panel when `kdos-shell --dump` runs beside it — without the
adopt path a dump drew an empty tray while the panel above it showed four items,
which is exactly the quietly-wrong output a dump exists not to produce.

**Known gap, stated rather than hidden:** `com.canonical.dbusmenu` is not
rendered. It is a second protocol with a nested-variant layout tree, and drawing
it as cells is its own piece of work. Apps with their own menu window answer
ContextMenu correctly; apps that set `ItemIsMenu` expect the host to draw the
menu and will do nothing.

**`testing/fixtures/tray/traycheck.c` is the test, and it is a second PROCESS.**
SNI is a conversation between two peers on a bus, and a mock of either side
would have passed on both silent bugs above. It forks a Qt-shaped item, drives
`tray.c` against it on a private `dbus-daemon`, and asserts the item is
registered, its Id and Status are read, all three buttons arrive, and killing
the app removes the cell. `testing/selftest.sh` runs it wherever there is an
sd-bus and a dbus-daemon.

---

## Repo layout

```
kdos/
├── ports/
│   ├── core/<name>/
│   │   ├── kpkgbuild            # declarative metadata — parsed, never sourced
│   │   ├── build.sh             # the build; bash, cwd = $SRC
│   │   ├── postinstall.sh       # optional install-time hook (4 ports)
│   │   └── <name>-<ver>.tar.*   # cached upstream tarball (LFS)
│   ├── appbox/                  # Containerfile, fetch, image/ chunks
│   └── fetch                    # downloads all source= URLs; vendoring=rust|go|node|python
├── src/
│   ├── libs/                    # our C libraries, static, no external deps
│   │   ├── libkbase/            # kb_*  alloc/string/file/proc/lock helpers
│   │   ├── libkcolor/           # kcol_* the palette table + colour maths
│   │   ├── libktui/             # ktui_* terminal, widgets, mouse, modals
│   │   ├── libkcell/            # kcell_* the glyph cache and cell painter
│   │   ├── libkxdg/             # kxdg_* desktop entries
│   │   ├── libkpkg/             # kp_*   db, ports tree, solver, version + hashes
│   │   ├── libksig/             # ksig_* Ed25519 (vendored Monocypher)
│   │   ├── libkbuild/           # kbuild_* phases, plans, snapshot inventory
│   │   └── libkwl/              # kwl_*  libktui's Wayland backend — the one
│   │                            #   library with real -l dependencies
│   ├── desktop/                 # the desktop, ours (see docs/KDOS-DESKTOP.md)
│   │   ├── kdos-comp/           # hard fork of labwc 0.20.0; KDOS grafts in
│   │   │                        #   src/kdos-*.c (CRT, wallpaper, frames, idle)
│   │   ├── kdos-shell/          # panel x2, launcher, menu, desk, pick, run,
│   │   │                        #   prompt, notifyd, osd, tray (SNI), privacy
│   │   ├── kdos-lock/           # the lock screen + setuid kdos-checkpass
│   │   ├── kdos-powerd/         # suspend/poweroff/reboot over a unix socket
│   │   ├── kdos-energyd/        # per-app Energy Impact from RAPL, relative
│   │   ├── kdos-boxsock/        # the security-context-v1 sandbox engine
│   │   └── xdg-desktop-portal-kdos/  # FileChooser + Settings + OpenURI
│   ├── build/
│   │   └── kdosbuild/           # the build orchestrator (C, host-only)
│   ├── tools/
│   │   └── kdos-portup/         # the upstream version checker (C, host-only)
│   └── packages/                # ports that are OURS (see Three Rings)
│       ├── kdos-installer/      # the installer (C, zero libraries)
│       ├── kdos-kpkg/           # kpkg + the four names it answers to
│       ├── kdos-theme/          # the GTK/icon/cursor generators
│       └── kdos-tools/          # kdos (+ theme --audit), ksvc/service,
│                                #   kdos-getty, banner,
│                                #   shot, fetch-app, fetch-static
├── fs/                          # copied verbatim into the rootfs
│   ├── etc/{inittab,fstab,profile,profile.d,init.d,skel,kpkg.conf}
│   ├── usr/local/bin/           # kdos-desktop, alien-app shims
│   └── usr/share/{kdos,backgrounds}
├── script/                      # phase dirs + util/ + kdosbuild.sh
├── testing/                     # per-port build tests, qemu-hw runner
├── docs/screenshots/            # README images (captured from QEMU)
├── Dockerfile                   # Alpine build sandbox
└── Makefile
```

`fs/etc/X11/` does not exist and must not.

---

## Build system

`make build` → builds the `os-dev` Alpine image → runs `script/kdosbuild.sh`
inside it with `--network none`, which compiles `src/build/kdosbuild` (a
two-second `cc` of a program that links nothing but libc) and runs it.

The orchestrator discovers phase directories and runs each as either **numbered
shell scripts** (`00_*.sh`, `01_*.sh`, …) or **a `packages.txt`** (port names;
`kpkgdepends` resolves order, then `kpkg install -f <pkg>` per package).

**`script/build.py` and `script/buildlib/` are gone.** kdosbuild is the
orchestrator; there is no second driver and no `DRIVER` switch. It also has a
headless mode, which the python one never did — `curses.wrapper` died with
`setupterm: could not find terminal` the moment stdout was not a tty, so a
build could not be run from anything but an interactive shell.

| Phase dir | Title |
|---|---|
| `00_toolchain` | Cross Toolchain — cross binutils + gcc targeting `x86_64-kdos-linux-musl` |
| `01_phase1` | Base Userland — musl, toybox, bash, native gcc, kpkg, kinstall |
| `02_phase2` | Self-Hosting Bootstrap — rebuild tar/musl/zlib/binutils/gcc inside the chroot |
| `03_phase3` | Toolchain & Core Libraries |
| `04_phase4` | Userland & Wayland base |
| `05_desktop` | Desktop — wlroots, then `kdos-comp`, `kdos-shell` and the rest of `src/desktop/` |
| `05_phase5` | Kernel |
| `06_packaging` | trim rootfs, theme, user, appbox, initramfs, ISO |

`05_desktop` sorts before `05_phase5` and that ordering is deliberate, not an
accident of the names: the desktop is ordinary userland and the kernel is the
last thing built before packaging.

**Phase 4/5 chroot** — `chroot_exec.sh` bind-mounts `$REPO_ROOT` → `/kdos`,
`$REPO_ROOT/ports` → `/ports`, `$REPO_ROOT/build` → `/kdos/build`, plus
`/dev /proc /sys /tmp /run`, then chroots with `env -i`. Phase env files
(`script/phaseN.env.sh`) export `CHROOT=1`, `CFLAGS`, `LDFLAGS`,
`PORT_REPO="/ports/core /kdos/src/packages"`.

**Anything a chroot command prints is parsed.** `kpkgdepends` writes the install
order to stdout and nothing else, so `chroot_exec.sh` logs diagnostics to
`build/logs/chroot.log`. kdosbuild reads stdout only and validates every token
against `^[A-Za-z0-9][A-Za-z0-9._+-]*$`; noise fails loudly instead of being
installed as a package.

### Phase snapshots

Every completed phase is snapshotted to `build/snapshots/<phase>/`. **What gets
snapshotted is declared by the phase**, in a metadata block the orchestrator
*parses* (never sources — sourcing would run those files' `rm -rf
/var/cache/kpkg/work` on the build container):

```bash
# --- build-system metadata (PARSED by the orchestrator, never sourced) ---
export KDOS_PHASE_TITLE="Toolchain & Core Libraries"
export KDOS_PHASE_DESC="compilers, build systems, interpreters, base libraries"
export KDOS_SNAPSHOT_PATHS="fs"                 # relative to build/
export KDOS_SNAPSHOT_EXCLUDE="fs/tmp/* ..."     # tar --exclude globs
```

Values must be literal. Absolute/empty/`..` paths are refused. A phase with no
`KDOS_SNAPSHOT_PATHS` is never snapshotted. One directory per phase, overwritten
in place, one `<path>.tar.zst` per declared path plus `manifest.json` and
`timings.json`.

`make build` opens a picker; pick a snapshot and the build restores it and
continues at the *next* phase. Restore is layered — each path comes from the
newest snapshot at or below the chosen phase. Restoring a phase with no snapshot
of its own is refused. `[S]` during a build takes a *partial* snapshot; restoring
one re-runs that phase (kpkg skips installed packages) and the picker marks it
`done/total!`. An interrupted restore leaves `build/.restore-in-progress` and
both snapshotting and the next build refuse to run until it is resolved.

```bash
make build BUILD_ARGS="--restore phase2"        # restore, continue at phase3
make build BUILD_ARGS="--continue-from phase3"  # resume on the CURRENT tree, no restore
make build BUILD_ARGS=--fresh                   # skip the picker, run everything
make build BUILD_ARGS=--no-snapshot             # throwaway run
make snapshots                                  # list them
make cleanbuild                                 # wipe build/ but KEEP snapshots
```

Budget ~2–4 G per phase. `make clean` deletes snapshots too.

### Build plans

Snapshots answer "go back"; the **build plan** answers "re-run just this". It
restores nothing — it narrows what the next run executes on the tree you have.
Press `[P]` in the picker, or:

```bash
# changed something under fs/ -> re-sync it and rebuild the ISO, nothing else
make build BUILD_ARGS="--phases 01_phase1,06_packaging --steps 01_phase1:00_file_system.sh"

# changed a kpkgbuild -> rebuild that port and repackage, no manual kpkgdel
make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild mesa,networkmanager"

make build BUILD_ARGS=--plan     # interactive; '/' fuzzy-searches ports
```

Three things make that safe:

- **Snapshots are suppressed** whenever a plan narrows execution (override with
  `--snapshot`). Re-running phase 1 on a phase-5 tree would otherwise file that
  tree under phase 1's name.
- **`kpkg install -f` genuinely forces.** It resolves against an empty db and
  rebuilds only the packages named on the command line; dependencies keep the
  normal skip-if-installed behaviour. Because of that, kdosbuild passes `-f`
  **only** for ports the plan selected — passing it blanketly would rebuild all
  ~350 packages.
- **Mark-file guards stand down for a step you picked.** 17 scripts start with
  `if [ -f "$MARK/x" ] && [ "${KDOS_REPLAY:-0}" != "1" ]`; kdosbuild exports
  `KDOS_REPLAY=1` for explicitly named steps and `chroot_exec.sh` forwards it.

**Don't re-run an early phase on a tree already ahead of it** — its snapshot
would be overwritten with a later tree under the earlier name. That is what
`--continue-from` is for.

### `fs/` sync is manifest-guarded

**A file deleted from `fs/` must disappear from the tree.** `cp -r` overwrites
but never removes, so a dropped path lingers forever — that is how a stale
shell `kdos-appbox` blocked its C replacement with a kpkg file conflict, how a
stale icon rode three ISO rebuilds, and how 94 old `kdos-*.desktop` launchers
sat beside their 91 replacements. `00_file_system.sh` records every path `fs/`
provided in `/var/lib/kdos/fs-manifest` and deletes, on the next sync, the ones
it no longer provides. Two traps: the manifest is written **after** the copy (so
a package that later owns the same path is not the manifest's to remove), and it
must **not** use `find -printf` — a GNU extension the build image's busybox
`find` rejects, which wrote an EMPTY manifest that silently protected nothing.

`00_file_system.sh` **merges** `/etc/{passwd,group,shadow}` instead of
overwriting: package postinstalls add service users (polkitd, messagebus, sshd)
long after phase 1 runs, and a plain `cp` on a re-sync would delete them. Repo
entries win; runtime-added entries are appended back.

`00_user.sh` needs the same treatment for the trees it copies into the home
(`.icons`, `.themes`, `.local/share/applications`) — it clears them before
copying skel.

### And PACKAGES are swept the same way

**A port deleted from `ports/` used to leave its package installed forever.**
`fs/` has had a manifest guard since the stale `kdos-appbox` above; packages had
none, and the build tree is incremental. Measured on this branch: the ISO still
carried all sixteen `cosmic-*` packages, `pop-launcher`, `kdos-theme-helper` and
`xdg-desktop-portal-cosmic` — **529 MB of a desktop removed a milestone
earlier**, plus `kd`, the demo the user asked to be deleted. Nothing was wrong
with the recipes; there were no recipes.
`06_packaging/00_orphans.sh` `kpkgdel`s every installed package with no
`kpkgbuild` in `ports/core`, `src/packages` or `src/desktop`, and
`testing/preflight.sh` says so in seconds instead of at the end of a two-hour
build. Neither is fatal on failure: an orphan with a damaged manifest must not
stop the ISO from being rolled.

**Skip-if-installed still compares the VERSION, not the recipe** (see **kpkg is
C**), and the same session found the other half of that: `fastfetch`'s
`config.jsonc` is a KDOS file installed by the port, its banner footer was
changed from `cosmic` to `wlroots`, no `release` was bumped, and the shipped
`/etc/xdg/fastfetch/config.jsonc` still said `cosmic` for every user with no
home config — root, on tty2. The fix was `release = 2`. Wiring `E:` into the
skip decision is the real answer and is still not done.

### libkbuild — the deciding half of the orchestrator

`src/libs/libkbuild/` is phase discovery, the phase-env metadata block, the
build plan and the snapshot inventory: the part of the orchestrator that
**decides**. It reads and it chooses; creating archives and running phases is
kdosbuild's.

**The env files are PARSED, never sourced** — several end with
`rm -rf /var/cache/kpkg/work`, which at source time hits the build
CONTAINER's filesystem, not the target's. So only `export NAME=VALUE` lines
are read, only five keys are honoured (`CHROOT` plus the four `KDOS_*`), and a
value must be a literal: no expansion, and an unterminated quote reads as
empty.

A declared snapshot path is either kept or **reported as rejected** — never
quietly repaired. Snapshot and restore delete and re-extract those paths as
root, so absolute, empty, `.` and anything with a `..` component is refused.
(`..foo` is a real name and is allowed; only a whole `..` component counts.)

The **build plan** (`kb_plan.c`) is the narrowing `--phases` / `--steps` /
`--rebuild` do, plus the port discovery behind the picker. A plan that narrows
anything **suppresses snapshot writes** (`--rebuild` alone is *custom* but does
not narrow, so it keeps them) — a snapshot taken from a partially re-run tree
would be filed under a phase name it no longer represents.

The **snapshot inventory** (`kb_snap.c`) is `load` / `list` / `plan_restore` /
the restore marker / `mounts_under`. Restore selection is layered and
newest-wins — each declared path comes from the newest snapshot at or below
the target — and a manifest that does not parse, carries no `entries`, or
names an archive that is not on disk reads as **absent, never as partial**. A
half-read manifest that looks complete is what loses a tree.

`kb_json.c` is a read-only JSON scanner for exactly those files. It refuses
anything that does not parse whole — truncated, trailing junk, trailing comma
— because every caller treats a parse failure as "absent", and a lenient
parser would turn a corrupt manifest into a confident wrong answer.

Two bugs the port surfaced, both silent:

- **`kb_read_all` stopped at the stat'ed size.** Every file under `/proc`
  reports `st_size` 0, so `/proc/mounts` read back as an empty string and
  `mounts_under()` answered "nothing is mounted" — the answer that lets a
  snapshot run over a live bind mount. It reads to real EOF now.
- **A `{}` restore marker is not a marker.** python returned the dict and
  every caller tested it for truth, so an empty object means no interrupted
  restore.

These were all verified against `script/buildlib` line by line while both
existed — phases, steps, ports, the package index, 16 build-plan shapes, the
snapshot inventory, layered restore at 8 targets. buildlib is gone now, so
that differential is gone with it; what remains is libkbuild's own assertions
in `src/libs/selftest.c` and the end-to-end run below.

### kdosbuild — the orchestrator in C

`src/build/kdosbuild/` is `script/build.py` plus `script/buildlib/*.py`:
`main.c` (the CLI), `manager.c` (the execution order and the step runner),
`snapshot.c` (writing and extracting archives), `stats.c` (timing history, the
ETA, the telemetry sampler), `tui.c` (the four screens), `view.c` (the parts
of the screens that are DECISIONS rather than drawing — the layout, the log
classifier — plus the preview fixture) and `report.c` (the headless output, text
and NDJSON, from one traversal). It sits on libkbuild for everything that only
INSPECTS the tree.

Drawing on libktui is what kills the **third TUI toolkit** — kinstall's,
kdos-appbox's ncurses one and `buildlib/tui.py`'s python curses one were three
implementations of one idea, and the build screen was the last consumer.

**One structural change from the python, and it is a simplification.**
build.py ran the build on a worker THREAD because curses' `getch()` blocks.
libktui's input has a timeout, so here the build IS the main loop: pump the
running child, pump the sampler, draw, wait for a key with a deadline. No
threads, no locks, no "never let the worker die silently" wrapper, and the
progress callbacks that had to be careful never to draw concurrently with the
caller now cannot be.

Two behaviours are new rather than ported:

- **A headless mode.** build.py went straight through `curses.wrapper`, so a
  build with its output redirected drew a full-screen UI into a log file and a
  build with no terminal could not start at all. kdosbuild answers a non-tty
  stdout or `TERM=dumb` with plain lines, the same rule kpkg already follows —
  and that is also what makes the engine testable without a pty.
- **A phase's snapshot is deferred by one pump turn.** Nothing runs in
  between; it exists so a front end can print a step's RESULT before the
  snapshot's progress starts arriving underneath it.

Everything else is behaviour-preserving, including the parts that only exist
because something broke: the layered newest-wins restore, refusing to restore
a phase whose earlier snapshots have gone missing (the fallback would build
against a rootfs that never had the target's packages), the skip ceiling taken
from the phase that actually CONTRIBUTED data, `KDOS_REPLAY=1` only for a step
the plan named by name, `-f` only for ports the plan named, and reporting a
`--rebuild` that no selected phase ever reached.

**`kb_argv_add` stores the pointer; it does not copy.** Three call sites here
handed it a `KbBuf` that was freed immediately after, or a stack buffer that
went out of scope at `return` — `bash -c ' ;'`, which is what a corrupted
command line looks like from the outside. The command strings are owned by
the step (`BStep.cmd_line`) or by the manager (`Manager.chroot_exec`) now.
Worth remembering before adding a fourth.

**The build screen's keys.** `↑↓`/`jk` select, `SPACE` folds a group, `F`
toggles follow, `S` queues a partial snapshot, `Q` stops (twice to force). Five
are newer and are the ones nothing else documents: **`/`** opens a search over
the selected step's log — marks rather than filters, because a build log is
read for the context AROUND the hit; **`n`/`N`** walk the marks; **`E`** jumps
to the first line `log_severity()` calls an error; **`O`** opens the step's log
file in `$PAGER` (argv, never a shell); **`T`** cycles the four accents live,
which needs `ktui_term_repalette()` AND `ktui_draw_invalidate()` — repalette
alone leaves every untouched cell wearing the old colours.

**Three diagnostic entry points, none of which needs a build.**

- **`kdosbuild --selftest`** runs `view.c`'s assertions: `layout_compute` over
  every size from 40x10 to 300x100 with no two regions overlapping and none
  leaving the screen, the region drop order, and the log classifier including
  the trap that "checking for error_at_line" is not an error. It is what
  `testing/selftest.sh` calls; it prints `view: ok`.
- **`kdosbuild --preview <screen> <WxH> <tier>`** draws one screen offscreen
  and dumps the cell buffer as plain text. `<screen>` is
  `build activity startup plan packages`, `<tier>` is `rich vt ascii` (see the
  libktui tier table). It is the only way to SEE a layout without a two-hour
  build and a terminal, and it exists because six geometry defects in this TUI
  were found by hand arithmetic and none of them was visible to the compiler.
  Read the `vt` output specifically for glyphs the console font lacks.

  It is what forced each screen's drawing half out of its event loop into a
  `draw_*_frame()`. The loop calls that function and nothing else — a second
  drawing path would be a second thing to keep in agreement with the one
  people look at. The fixture in `view.c` is chosen to break layouts rather
  than to look plausible: a multi-terabyte total, a nine-digit file count, an
  hour-scale ETA, a port name longer than any pane. The one thing that is not
  reproducible between runs is the spinner glyph, which is picked from the
  wall clock.

- **`kdosbuild --json`** replaces the plain lines with NDJSON, one object per
  event: `build phase step snapshot notice restore result`. `--list --json`
  prints the snapshot inventory as one object. Both are the SAME traversal —
  `run_plain()` calls a `Reporter` (`report.c`) and does not know which of the
  two it has, so the text and JSON views cannot disagree about what ran. NDJSON
  rather than one document because a build can be killed at any moment and the
  reason to have a machine-readable log is reading the tail of one that died.
  There is no total step count in the `build` event on purpose: a
  `packages.txt` phase expands only when it is entered. This is what lets
  `testing/selftest.sh` assert on the ENGINE — the failing synthetic tree's
  `rc`, which step failed, and that the step after it never ran — instead of
  grepping for `BUILD COMPLETE`.

**What is proved.** `testing/selftest.sh` compiles it, then runs it against a
synthetic two-phase tree: a build with snapshots and logs, a restore that puts
the tree back and resumes, plan narrowing with snapshot suppression and
`KDOS_REPLAY`, and a failing step that stops the build without snapshotting
the phase. Its CLI surface was diffed against build.py over 16 argument shapes
including every refusal, while build.py still existed.

And it has now driven real phases: **00_toolchain** (the cross toolchain,
8m27s), **01_phase1** (musl through native gcc, kpkg and kinstall) and
**02_phase2** (the self-hosting bootstrap, 14m12s, 11 packages through
`kpkgdepends` + `kpkg install` inside the chroot). Phases 3–5 and packaging
are the remaining unproven ground.

### `testing/preflight.sh`

Everything a full build would catch, minus the build. `make build` takes hours
and needs a container; this checks the WIRING in seconds:

- every package named in a `packages.txt` has a port
- every `packages.txt` resolves to a dependency order, with no stderr, no
  empty result, and every token matching what `phases.py` will accept
- every `# depends` names a port that exists — the gap CLAUDE.md's TODO list
  called "no build-time check that all `# depends` resolve"
- every recipe declares name, version and release
- all shipped and build shell is syntactically valid
- nothing still invokes a tool the C consolidation removed
- the rootfs carries no script whose interpreter is gone
- the shipped `rc.xml` keeps `<default />` in `<keyboard>` and `<mouse>`
- **every command named in the shipped `rc.xml` and `menu.xml` is provided by
  the tree** — the check `<promptCommand>labnag</promptCommand>` needed, and the
  one `kdos-desk` needed when it called a `kdos-appbox` subcommand that did not
  exist yet. A command counts when a port of that name exists, when a build.sh
  installs or links it into a bin directory, or when it is named in a `for … in`
  list. That last form is restricted to those lines ON PURPOSE: a bare word
  search over a recipe matches a COMMENT (`-Dlabnag=disabled`) and passes on a
  desktop that does not work

It cannot prove the build works. It can prove the build will not fail for one
of the dull reasons. Run it after touching a `packages.txt`, a recipe, or
anything under `script/`.

### `testing/selftest.sh`

Host-only regression net for `src/libs/`. Compiles every library with the host
compiler under `-Werror`, runs `src/libs/selftest.c` against them, then
compiles all five consumers to prove the headers still agree, and resolves a
port to prove the ports tree still parses, and audits a generated theme. No
container, no network, half a minute — most of it the ~50 MB of icons and
cursors the theme audit generates three times, which is the price of testing
that claim with the real artwork.

Its assertions are the INVARIANTS that were established by diffing against the
implementations these libraries replaced — python's `colorsys`, the shell
kpkg, the shell `kdos`. Each is a claim that was measured once; this is what
notices when it stops being true. Worth extending whenever a new one is found;
worth running before trusting any change under `src/libs/`.

Two of them are worth knowing about because they are counter-intuitive:
`kcol_mix` and `kcol_mixf` are asserted to DISAGREE (0x7f vs 0x80 halfway
between black and white — a truncating divide against python's round-half-to-
even), and the ustar writer is checked by handing its output to real GNU tar.

It also runs kdosbuild end to end against a synthetic two-phase tree — a real
build, snapshot, restore, plan narrowing and a deliberate failure. That is the
only test that exercises the ENGINE rather than a decision, and it is the one
to extend when the orchestrator grows.

**And it LOOKS at the shell's layouts.** `kdos-cal`, `kdos-menu`,
`kdos-launcher` and `kdos-pick` each render one frame offscreen (`--dump`) with
libkwl stubbed out by `testing/fixtures/shell/dumpmain.c` — the dump path
touches no Wayland and no fcft, which is what makes it runnable on a host that
has neither. The assertions are shape, not content: every row of a box the same
width, and the chooser's hint text and its two buttons both on their row without
colliding. Six geometry defects have shipped in this toolkit and not one was
visible to a compiler; this is how they get seen. Verified by breaking it —
widening the hint row makes the check say so.

**Run it sanitized when you touch a parser.** `CC` is the seam and it takes
more than one word:

```sh
CC="cc -fsanitize=address,undefined -g" testing/selftest.sh
```

The suite is clean that way and stays that way. It found two real defects the
plain run could not see: `kb_tar`'s base-256 size field overflowed a `long
long` (signed overflow, and the negative it produced became a `size_t` length —
the GNU-long-name branch asked `read()` for 2^63 bytes into a 512-byte stack
buffer, and only the kernel refusing an address range that large stood in the
way), and `kxdg_entry.c` called `memcpy` with a NULL source on every entry's
first key. **Leak checking is off by default and on for `src/libs/selftest.c`
alone** — every program here owns its parsed state until it exits, which
LeakSanitizer reports as a leak and turns into a false test failure; the
library suite is the one binary whose subject is code called repeatedly.

A `libFuzzer` driver over `kj_parse`, `kxdg_load`, `kp_recipe_key`,
`kb_tar_next`, `kcol_retint_text` and `kp_vercmp` is what found both, in ten
minutes across four workers, with no crash and no leak besides. It is not
committed — the corpus is worth more than the driver, and neither has a home in
a tree that ships no test binaries — but it is twenty minutes to rewrite and
worth doing after any change to a parser.

**What the gate does NOT cover.** `kdos-comp` and `kdos-shell` compile here
only where wlroots 0.20 and fcft exist, which a bare host does not have, so on
most machines the blocks that would catch a break in them print `skipped`.
`kdos-boxsock` was in that category for a worse reason — an unguarded `#define
_GNU_SOURCE` collided with the `-D_GNU_SOURCE` libkbase needs, under `-Werror`
— and now compiles wherever wayland-client does, which is nearly everywhere.

There used to be a **libkbuild ↔ buildlib differential** here: `pdump.c` and
`pdump.py` printed the same lines from the C and python implementations of the
same decision, and `diff` was the whole test. It did its job — the port was
verified against the original line by line — and went when `script/buildlib`
did. Nothing is left to diff against.

---

## kpkg is C

`src/packages/kdos-kpkg/` is one binary answering to five names — `kpkg`,
`kpkgadd`, `kpkgbuild`, `kpkgdel`, `kpkgdepends` — dispatched on its own
basename, cross-compiled in phase 1 by `12_kpkg.sh` the same way kinstall is.
It links libkbase, libkpkg and libksig and nothing else. There are no shell scripts in
`src/` any more.

**Recipes are unchanged and still bash.** kpkgbuild does the source
extraction and rolls the package itself; bash is exec'd to do exactly one
thing — run `build()` (and `postinstall()`) with `PKGNAME` / `PORT_SRC` /
`SRC_ROOT` / `SRC` / `PKG` set as shell variables, cwd `$SRC`, inside
`( set -e )`, stdio inherited so the interleaved output is still the per-port
log. The recipe format a port uses is decided by looking at the file: a
`build()` function means legacy. That is what lets ports convert one at a
time.

Contracts that were reproduced deliberately, each verified rather than
assumed:

- **`kpkgdepends` prints one bare space-separated line and nothing else.**
  Verified identical to the shell version over all 397 ports individually,
  over every phase's whole `packages.txt`, and with a populated database.
- **`PKGDB_DIR=/dev/null` means an empty database.** `kp_installed` only asks
  whether `<dir>/<name>` reads as a file and never stats the directory —
  stating it would answer "character device" and silently break `kpkg install
  -f`, the orchestrator and `mini_build.py` together.
- **`kpkg install -f` forces only what was NAMED**; dependencies keep
  skip-if-installed, because a blanket force rebuilds all ~350 packages.
- The db line-1 format, the `./`-prefixed manifest with trailing slashes on
  directories, `<name>-<version>-<release>.tar.xz`, `--root`, the whitespace
  `PORT_REPO` list, env-over-config, and exit codes 0/1.

**Skip-if-installed compares NOTHING — it asks whether a database entry
exists** (`kp_installed()` is `kb_read_all(<db>/<name>) != NULL`), so an
installed package is skipped whatever its version, its release or its recipe
now say. Bumping `version` is therefore worth doing for the binhost and for a
fresh tree, and does nothing at all for an incremental local build: **the only
thing that rebuilds a changed port on a built tree is `--rebuild <port>`** (or
`kpkg install -f`). Get that wrong and a build "succeeds" carrying the old
binary, which is indistinguishable from a change that did not work.

This is not theoretical and it cost a build: `ports/core/wlroots/build.sh` gained
`install -m644 protocol/*.xml "$PKG/usr/share/wlroots/protocols/"` because
upstream does not install its own protocol XML and three of our recipes read it
from there. The installed wlroots on the tree predated that line, its manifest
carried zero `.xml`, and `kdos-shell` — the first genuinely new package of
`05_desktop` — died at once on wayland-scanner's `Could not open input file`.
Nothing detects it: the version was unchanged, so kpkg correctly skipped a
package that no longer matched its own recipe.

KDOS already computes the answer — `kp_recipe_hash` is the binhost's `E:`, a
SHA-256 over kpkgbuild, build.sh, postinstall.sh and every patch — and the
local install path does not consult it. Until it does, **the fix is
`--rebuild <port>`**, and the smell is a package whose behaviour disagrees with
a recipe you just read. Wiring `E:` into the skip decision would rebuild every
port whose recipe was touched, which is right and is also a mass rebuild, so it
is a deliberate change rather than an obvious one.

**A file conflict is between PACKAGES.** A path that exists but that no
installed package claims is adopted, not refused. That is not a loosening for
its own sake — it is what phase 2 IS: phases 0 and 1 install tar, musl,
binutils and gcc by hand with `make DESTDIR=$SYSROOT install`, leaving files
no database entry owns, and the self-hosting bootstrap then rebuilds exactly
those packages with kpkg. It used to work because the phase passed a blanket
`kpkg install -f`, which skipped the conflict scan altogether; `-f` now
genuinely forces a REBUILD, so it cannot be handed out just to get an
overwrite, and narrowing it to plan-selected ports broke the bootstrap
silently — phase 2 died on `tar`. A path another package owns is still a
conflict, and `src/libs/selftest.c` asserts both halves.

**`--overwrite` is the other half of what `-f` used to mean, split out.** The
userland genuinely overlaps: toybox ships 242 paths and `awk` is gawk's,
`readelf`/`strings` are binutils', `find`/`xargs` are findutils', and
`mount`/`blkid`/`losetup` are util-linux's — about 70 collisions in all. The
build's rule has always been that whoever comes last in the dependency order
wins, and it worked only because the phase passed a blanket `-f` that skipped
the scan. `--overwrite` does that deliberately and nothing else: no rebuild, and
the path **changes hands in the database** — `kp_db_drop_paths` removes it from
the old owner's manifest — instead of being claimed twice, which is what made
`kpkgdel <old>` delete a file the new owner installed. kdosbuild passes it for
every package a `packages.txt` phase installs; `-f` still means rebuild and is
still only passed for ports a plan selected.

**`kb_run_capture` used to deadlock on output past its buffer.** `spawn()` left
the pipe's read end open in the child, so when the parent filled its ceiling and
closed its own copy, the child blocked in `write()` forever instead of taking
EPIPE, and the parent blocked in `waitpid()`. `kpkg install zig` hit it exactly:
a 1.1 MB `tar -tf` listing against a 1 MB buffer. The read end is `FD_CLOEXEC`
now, and the manifest readers use **`kb_run_capture_buf`**, which grows — a
short manifest is a database entry that owns fewer files than the package
installed. The upgrade orphan sweep had a matching `char *paths[8192]` ceiling
against zig's 20831 paths; it grows too.

Bugs the rewrite fixed, all of them silent:

- A failed `mv` ran inside `find | while read`, so its `exit 1` left only the
  SUBSHELL — the script carried on and wrote a database entry for a package
  that was half on disk.
- The conflict scan used `for f in $(find …)` while the install loop used
  `while read -r`; the two disagreed about any path with a space in it.
- An upgrade never removed orphans. A file present in v1 and absent from v2
  stayed on disk forever, owned by nothing. (Demonstrated, not inferred.)
- `./.POSTINSTALL` was recorded in the manifest though it is never installed,
  so removing such a package tried to `rm -f /./.POSTINSTALL`.
- `realpath -m` was called only to pretty-print a log line and aborted the
  install under `set -e` — the documented "Symbolic link loop" failure.
- `kpkgdel a bogus c` removed `a`, then exited at `bogus` and never touched
  `c`.
- `rm -rf "$WORK_DIR/$name"` ran before `WORK_DIR` was validated.
- `kpkg info` printed the package NAME as its description; `# description :`
  was parsed by nothing. It is read now.

### One recipe format, and how the tree got there

Every port is `kpkgbuild` + `build.sh`. There is no second
format and no dual mode: `kpkg` no longer looks at a recipe to decide how to
read it.

**Why the shell went into its own file rather than into the format.** An
earlier attempt made `[build]` a list of argv lines with no shell at all. It
converted 333 ports and refused 63 — heredocs, loops, redirects, globs,
`$(...)`. The two ways forward were to reimplement a shell inside kpkg, or to
embed one. Embedding was researched and rejected: there is no embeddable
evaluator. libdash is *"primarily to parse shell scripts"* and exposes an AST,
not execution; mksh is BSD/ISC and a quarter of bash's size but is a program
with `main()`, so embedding means forking ~30k lines of third-party C into
`src/`, which vendors none. And it would buy nothing — `08_bash.sh` runs before
`12_kpkg.sh`, so bash is in the sysroot before kpkg is compiled, and it ships
on the target anyway.

So the shell is a file. `bash -n`, shellcheck, highlighting and `git diff` all
work on it; none of them work on a blob inside a config format. And no parser
has to understand it, which matters more than it sounds: `ports/core/rust`
writes a `config.toml` heredoc whose body contains a line reading exactly
`[build]`, and `podman`'s contains `[engine]`, `[network]`, `[storage]`. Any
format carrying the shell inline would have had to tell those apart from its
own syntax.

**`kpkg convert` could not refuse a port**, because it interpreted nothing: it
lifted the `build()` body out, removed one level of indentation so it sat at
column 0, and wrote it. That is the whole transformation, which is why the
check that mattered is a `diff`. It was deleted with the last legacy recipe.

**Four checks, none of which needs a build**, all run before the sweep was
promoted:

1. **`build.sh` against the committed `build()` body**, line by line — 387 of
   387 `ports/core` recipes identical. (The 9 under `src/packages` were already
   modified in the working tree, so HEAD is not their baseline; they were
   checked by `bash -n` and by eye.)
2. **`bash -n`** over all 400 scripts. Now permanent in `testing/preflight.sh`.
3. **Metadata against bash** — `name`, `version`, `release` and the fully
   expanded `source` list, compared with what bash yields from the original
   recipe. 396 of 396 identical, which is what caught the helper-ordering bug:
   helpers emitted after `source` left `$_date` expanding to nothing and
   produced a URL that still looked plausible.
4. **`kpkgdepends` byte-identical** before and after, over all 396 ports
   individually and every `packages.txt` — the contract that is load-bearing
   for the whole build.

Then two ports were built for real, old recipe against new, and their package
file lists compared: `zlib` (16 members) and `uthash` (9, and its `install -m644
src/*.h` glob is one of the things the argv format had refused). Identical.

**`kpkg verify [--repro] <port>` is how a recipe CHANGE gets checked.** It
builds the port with its current `kpkgbuild` + `build.sh` and again with the
`kpkgbuild.new` / `build.sh.new` beside them, then compares the two packages.
Neither build happens in the ports tree: a scratch directory is filled with
symlinks to everything the port carries — tarballs, patches, vendor bundles —
and only the files being tested are real. An interrupted verify leaves the tree
untouched.

**It compares PAYLOAD, not the file list.** A file list answers "are the same
files there" and is silent about the same paths with different content, which is
the error that starts mattering the moment recipes change. The comparison is a
fingerprint — `mode  sha256  path` per member, sorted, symlink targets included
— and it prints the first differing lines. The archives are unpacked to build
it; streaming each member out of `tar` separately would be one process per file,
and zig has 20 831 of them.

**`--repro` builds the SAME recipe twice** and requires the two packages to be
byte-identical. That is the acceptance test for reproducible packaging, and it
is the same code path because it is the same question asked of two archives.

## The binhost — a signed index, and three equality tests

```
kpkg keygen builder                    # once, on the machine that builds
kpkg index /repo --sign builder.key    # PACKAGES + PACKAGES.sig + a .sig per package
cp builder.pub /etc/kdos/keys/         # on every machine that should trust it
kpkg binhost /repo zlib                # installs it, or says why it will not
```

**Two hashes replace Gentoo's whole USE-flag matching problem**, because KDOS has
no USE flags. A prebuilt package is usable when three things equal the client's
own: the architecture, `B:` the **build-config hash** (arch, libc, target,
compiler version, CFLAGS/CXXFLAGS/LDFLAGS) and `E:` the **recipe hash** (SHA-256
over kpkgbuild, build.sh, postinstall.sh and every `.patch`, sorted, each
contributing its name, its length and its bytes). Anything else builds from
source. There is no BUILD_ID counter and no "close enough" — the exit codes say
which happened: **0** used it, **1** no match so build it, **2** refused.

The index is Alpine's shape — single-character keys, one stanza per package, a
blank line between — because it parses in sixty lines of C and reads fine in a
pager. The KDOS part is `A:`/`B:`/`E:`.

**Signing is Ed25519 via Monocypher, and Monocypher is the ONE piece of vendored
third-party source in `src/`.** The exception is narrow and the alternatives are
all disqualified: libsodium is a shared library, BearSSL has no Ed25519
*signing*, TweetNaCl has been unmaintained since 2014, OpenSSL is the opposite of
"links nothing but musl". Monocypher is 4 files, ~120 KB of C99, public domain,
no dependencies, no libm, no malloc — which is exactly the `libk*` rule.
`src/libs/libksig/` holds it verbatim under `monocypher/` with its version and
hash in `UPSTREAM`; everything above it (file formats, keyring, policy) is ours.

**Sign the index, not 353 packages.** The index carries every package's SHA-256,
so one signature covers all of them transitively. The per-package `.sig` sidecar
exists for the other case — a package that travels on a USB stick with no index
beside it — and `kpkg verify-pkg` is what checks one.

Rules this design exists to keep, each one a way signing usually rots:

- **A key id is not a key.** The id inside a signature file selects which key to
  try; verification always uses a key from `/etc/kdos/keys`. A signature that
  could supply its own key verifies nothing.
- **A bad signature is not a missing one.** `kpkgadd` refuses to install a
  package whose `.sig` fails, and allows one with no sidecar — because the
  packages kpkg builds locally are the majority and are never signed.
  `KPKG_REQUIRE_SIG=1` is the stricter rule for a machine that only installs from
  a binhost.
- **`--insecure` says so out loud, every time.** A flag that prints nothing is a
  flag that gets left in a script.
- **The index is verified before it is believed, and a package's hash before it
  is unpacked.** Verifying after install is verifying nothing.
- **Multi-signature from day one.** A signature file is one line per signature,
  so during a key rollover both keys sign and a client trusting either keeps
  working. Retrofitting that is brutal; designing it in is one loop.
- **The signing key is written `0600` with `O_EXCL`**, and reading it back
  refuses if the mode ever loosened — signing with a key other users can read is
  signing with a key that must be assumed compromised.
- **KDOS ships no key in `/etc/kdos/keys`.** Shipping one would be asking you to
  trust whoever built the image, which is the question signing exists to let you
  answer yourself.

**Measured end to end**: a builder built zlib and uthash, keyed, indexed and
signed; a client with an empty keyring refused (2); the same client with the
public key installed zlib into its own root; an edited index, a package with one
byte appended, and an index signed by an untrusted key were each refused (2);
`CFLAGS="-O3 -march=native"` sent it back to source (1). `testing/selftest.sh`
runs all of that against a synthetic port.

### Deltas — an update that ships the difference

```
$ kpkg delta zlib-1.3-1.tar.xz zlib-1.3.1-1.tar.xz
zlib-1.3.1-1--from--zlib-1.3-1.kdelta: 3166 bytes, 26.8x smaller than the
package (84800 bytes)
```

`zstd --patch-from` is the engine; the two decisions around it are the work.

**The delta is taken over the UNCOMPRESSED tars.** Two `.tar.xz` files built from
nearly identical trees share almost no bytes — that is what a compressor does —
so a delta between them is the size of the whole package. Decompressing first is
the entire difference between 3 KB and 85 KB.

**A delta is never trusted, and never needs to be.** It is applied and the
RESULT is hashed against the `C:` the signed index already carries. A tampered
delta produces a package whose hash does not match and is discarded; it cannot
make a client install anything the index did not already name. So there is no
delta signature and no second trust path — `kpkg index` deliberately does not
write a sidecar for a `.kdelta`.

**The reconstruction is byte-identical, and that is P12 paying off.** Measured:
a zlib package rebuilt from a 3 166-byte delta compares equal to the builder's
own file and **verifies against the builder's Ed25519 sidecar**. Without
reproducible packaging, the reconstruction would differ in its metadata and the
signature would be worthless.

A delta stanza in the index is an ordinary stanza with `O:` — the package file it
applies to. There is no type field, because "it names what it patches" says the
same thing. The client uses one only when it still HAS that old package in
`PACKAGE_DIR`, which is the ordinary case for a machine that has been updating
rather than installing fresh; when it is gone, the full package is the answer and
nothing is lost.

## Vulnerability tracking — `kdos cve`

```
KDOS cve  —  Alpine secdb of 2026-08-12 (0 days old), the ports tree

  curl                   8.17.0       fixed in 8.21.0       CVE-2025-14017,CVE-2025-14524,... +29 more
  zlib                   1.3.1        fixed in 1.3.2        CVE-2026-22184,CVE-2026-27171

  389 checked, 262 not in the database, 28 behind a recorded fix
```

**The data is vendored, and the answer is offline.** `src/packages/kdos-tools/secdb/`
carries a host-only `vendor.py` and the committed table it produces — 270 KB,
798 packages, 4 099 fix records, merged from six Alpine branches
(main + community) and installed to `/usr/share/kdos/secdb.txt`. Same shape as
the kdos-icons and kdos-cursors vendoring, and for the same reason: the prune is
a KDOS decision and the artefact is diffable.

**Alpine, not NVD or OSV.** Alpine is the closest distro to KDOS that publishes a
machine-readable security database — musl, the same upstream tarballs,
comparable pins. OSV's `Alpine/all.zip` is the same facts in 4 MB; NVD is retired
feeds plus an API. Neither answers a question 270 KB answers.

**The question is a version comparison, not a scan.** *Is the version we pin
older than the version Alpine says fixed this CVE?* The comparator is
`kp_vercmp` — moved into **libkpkg** for this, because `kdos-portup` asks the
same question about the same strings ("is upstream newer than the pin") and two
implementations would eventually disagree.

Four details, each of which changes the answer:

- **Alpine's `-rN` is packaging, not upstream.** Comparing `1.2.12` against
  `1.2.12-r2` with the revision left on makes every pin look old. It is cut off.
- **`fixed in 0` means "never affected"** in that branch, and it falls out of the
  comparison for free — nothing is older than 0.
- **The newest fix a pin is behind is the one reported**, because it closes
  every hole below it too, and the CVEs from all the rows are merged and
  deduplicated (the same CVE is recorded by every branch that shipped the fix).
- **`secdb = <alpine-name>` in a recipe** maps a port whose name differs. Today
  that is exactly one: our `linux` is Alpine's `linux-lts`.

**A package Alpine does not carry is UNKNOWN, never clean.** 262 of 389 ports are
in that state and the summary says so; a checker that counted them as fine would
be reporting a number it had not earned. The vintage of the table is printed
with every run and a database over 180 days old says so.

**`ports/update --cve` is the ONLINE cross-check** — repology's per-version
`vulnerable` flag, one request per port at their one-per-second limit, six and a
half minutes over the whole tree. That cost is why it is a flag and why the
vendored table is the everyday answer. Measured agreement: zlib 1.3.1, curl
8.17.0 and expat 2.7.3 are flagged by repology and are the same three the
offline table reports.

`testing/fixtures/cve/` is four ports and a five-row table — a pin behind two
fixes, a pin that only looks behind one because of `-rN`, a `secdb =` mapping,
and a port Alpine never heard of.

## Reproducible packages

**A package built twice from the same tree is byte-identical.** That is a
property of ONE function — `roll_package()` in `build.c` — rather than of 396
recipes, which is the whole reason kpkg rolls the archive itself instead of
letting each `build.sh` do it. Every flag there is a specific source of drift:

| | |
|---|---|
| `--sort=name` | readdir order is filesystem order and is not stable, even between two copies of the same tree |
| `--mtime=@$SOURCE_DATE_EPOCH` | otherwise every file carries the second it was installed |
| `--owner=0 --group=0 --numeric-owner` | the builder's uid, and its NAME as text in the header |
| `--format=gnu` | pax headers carry atime and ctime, which are wall clock; ustar cannot hold a path over 255 bytes and some ports have them |
| `--use-compress-program=xz -9 -T1` | xz is deterministic single-threaded, and `XZ_OPT=-T0` in the environment silently changes the bytes |
| `umask(022)` before the build | a file `make install` creates without an explicit mode takes the builder's umask — the one source of drift that is not in the tar call |

The other half is five environment lines, now in **every** `script/*.env.sh`:
`SOURCE_DATE_EPOCH=1735689600` (pinned, not `date` and not git — the build
container has no git), `TZ=UTC`, `LC_ALL=C`, `-ffile-prefix-map` on CFLAGS and
CXXFLAGS, and `-Wl,--build-id=sha1` so the build-id is a function of the
contents rather than a random 128 bits.

**Measured**, on real ports built twice through kpkg: `zlib`, `uthash` and
`inih` each come out byte-identical, and zlib stays identical when the second
build runs under `umask 077`, `XZ_OPT=-T0` and `TZ=Asia/Kolkata` — the three
things that used to change it. `testing/selftest.sh` builds a synthetic
source-less port twice under exactly that hostile environment and also checks
the archive itself carries uid/gid 0 and the pinned epoch, so the test cannot
pass by two builds being equally wrong.

This is the precondition for P14 (A/B slots) and P15 (a signed binhost): a
package that is not reproducible cannot be verified against a second builder.

**A full `make build` is still what proves kpkg.** Everything else was verified
by diffing artefacts and by building two ports for real; a package manager can
only really be tested by building the distro with it.

---

## kpkg and recipe conventions

```sh
kpkg install foo       # build + install from ports
kpkgadd  foo.tar.xz    # install a pre-built package
kpkgdel  foo           # remove
kpkgdepends foo        # resolved install order (stdout is parsed — keep it clean)
kpkg meta  foo         # the recipe's fields as shell assignments
```

**A port is two files.** `kpkgbuild` is declarative metadata and is NOT a shell
script; `build.sh` beside it is the build and IS one.

```
# (banner header — keep verbatim)
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ...
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

name        = foo
version     = 1.2.3
release     = 1
source      = https://upstream.example/$name-$version.tar.gz
description = <one line>
homepage    = <URL>
depends     = <space-separated port names>
vendoring   = rust        # optional: go/node/python too
```

```bash
# build.sh — cwd is $SRC
./configure --prefix=/usr --libdir=/usr/lib
make
make DESTDIR=$PKG install
```

- **`kpkgbuild` has no shebang and is never sourced.** It is parsed. That is
  the point of the split: kpkg used to run bash just to READ a recipe
  (`. ./kpkgbuild` then `printf` the fields back) and to serialise
  `postinstall()` with `declare -f`. Now bash is exec'd only to build.
- **`description` / `homepage` / `depends` are KEYS, not comments.** They were
  `# depends : …` while the file was a shell script, which made a comment
  load-bearing. `kp_depends` still reads the old form as a fallback.
- **`build.sh` is a real script**, so `bash -n` checks all 396 of them in
  `testing/preflight.sh` — a syntax gate that was impossible while the build
  lived inside the recipe. `postinstall.sh` is the optional install-time hook
  (4 ports), and becomes `.POSTINSTALL` in the package.
- **Variables the build gets:** `$name` / `$version` / `$release`, every recipe
  helper, and `$PORT_SRC` (the port dir), `$SRC` (`$WORK_DIR/$name/$name-$version`,
  the cwd), `$SRC_ROOT` (`$WORK_DIR/$name`), `$PKG` (the DESTDIR staging tree).
  They are shell VARIABLES, not exports, and each is injected single-quoted.
- **`source` repeats** to add a second tarball. It auto-detects the extension
  and passes `--strip-components=1` for the first source only.
- **Any key that is not one of ours is a recipe helper** — `_tag`, `vrsn`,
  `_triplet` — declared between `release` and `source`, which is the only order
  that works: a helper may read `$version` and `source` may read the helper.
  Values expand `$var`, `${var}`, `${var#p}` `${var##p}` `${var%p}` `${var%%p}`
  `${var/a/b}` `${var//a/b}` `${var:off:len}`. A command substitution is NOT
  available — `unzip` spells its `$(echo $version | sed 's/\.//')` as
  `${version//./}`.
- **`secdb = <alpine-package>`** maps a port onto the name Alpine's security
  database uses, for `kdos cve`. Declared only when it differs — today only the
  kernel (`linux` here, `linux-lts` there).
- **`bench =` and `bench_setup =`** are how `kdos march` measures a port: the
  first is timed, the second runs once and is not. A port with neither cannot be
  measured and is never counted as a winner.
- **`vendoring = rust`** makes `ports/fetch` run `cargo vendor` and package
  `vendor/` + `.cargo/config.toml` as `<name>-vendor-<version>.tar.xz` beside
  the tarball; `build.sh` extracts it into `$SRC` and builds `--frozen --offline`.
- **`ports/fetch` cannot source a recipe any more.** It compiles kpkg on the
  host (a two-second `cc`, gitignored at `ports/.kpkg-meta`) and evals
  `kpkg meta`. One parser, in C, shared with the build.

### Port recipes — the canonical `build.sh` shapes

```bash
# meson
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
      --buildtype=release -Dtests=disabled -Ddocs=disabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# cmake
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
      -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF

# autotools
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib --disable-static

# rust
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export LIBCLANG_PATH=/usr/lib
export CARGO_NET_OFFLINE=true
cargo build --release --frozen --offline
```

---

## kdos-portup — upstream version checker

`src/tools/kdos-portup/` (invoked through `ports/update`, the same
compile-on-demand wrapper shape as `ports/fetch`: build to `ports/.portup`
when a source file is newer, then exec) answers one question per port —
"does upstream have a release newer than what `kpkgbuild` pins?" — for all
390 ports, without touching git. Host-only, like `ports/fetch`; nothing
ships on the target.

**The six-step pipeline**, and the constraint every step above it exists to
serve: `pu_list_upstream` (forge tag-feed / directory listing / repology, in
that order, first non-empty wins) → `pu_extract` every raw string into
version candidates → keep the ones whose `pu_shape` matches the CURRENT
version's shape → `pu_vercmp`, walking from the highest match down →
`pu_render_candidate` (copy the recipe to a temp dir, substitute the
candidate, expand it through `kpkg meta` — the same parser the build uses,
so a helper chain like ca-certificates' date rewrite falls out for free and
no probe ever touches the real tree) → `pu_http_head` the rendered URL. A
plain 404 drops that candidate and tries the next-highest; only a 200
proves `PU_NEWER`. **Correctness comes from that last HEAD, not from the
discovery step** — a forge feed or a directory listing can name a version
whose tarball lives somewhere the recipe's template does not expect, and
the tool would rather try the next candidate than report a version it never
confirmed the build could actually fetch.

**Three outcomes, never two.** `unknown` is never folded into `current`:
that would be a confident wrong answer, the one thing this tool must not
give. A listing that could not be reached at all (`pu_list_upstream`
returns -1), a directory listing whose real tail was cut off by the
adapter's own cap (`truncated`, since archive indexes sort ascending and
the dropped entries are the newest ones), and a listing that named
candidates but whose rendered URLs all 404 (a stale recipe template, see
`imagemagick` below) are all `unknown` — reported with a reason, not
silently averaged into "up to date".

**GitHub's tag feeds need no authentication.** `https://github.com/<owner>/<repo>/tags.atom`
has no rate limit worth worrying about, unlike the REST API's 60
requests/hour (which does not cover one run over the 137 ports this tool
can reach through a forge at all). Codeberg, sr.ht and GitLab (scoped to
the first path segment after the host — GitLab groups nest arbitrarily, and
under-grouping two projects that share an org is the dangerous direction,
not over-grouping one that doesn't) get the same treatment. Repology is the
fallback of last resort, rate-limited to one request/second and marked
`low_confidence` — it is never upstream itself.

**The group key is `<forge-org>@<current-version>`, deliberately not a name
prefix.** The example that settled it is gone from the tree but the rule is not:
a `cosmic-*` name rule missed the 17th member of that epoch, `pop-launcher`,
whose repo is `pop-os/launcher`, while all 17 resolved to the identical
`pop-os@1.4.0` and were only ever offered as a bump together. An explicit
`group =` key in a recipe overrides the derived one.

**Coverage: 382 of 390 ports recover their exact current version from their
own `source` URL** (verified against `kpkg meta`'s own expansion). The 8
that do not are genuinely unrecoverable from a filename alone, not a gap in
the extractor, and correctly report `unknown`.

**`make updates`** runs `ports/update` with `PORTUP_ARGS` passed through
(`make updates PORTUP_ARGS="--check curl"`, and `--cve` for the repology
vulnerability cross-check — see the CVE section). `--check` is the
non-interactive form: exit 0 means every named port is current, 1 means at
least one has an update, 2 means a bump was accepted but its tarball never
made it to disk (the one state this tool exists to prevent a build from
inheriting silently). With no port names it checks the whole tree. The tool
never runs `git`; a review's "yes" only rewrites a recipe's `version =`
line and re-fetches the tarball — committing the result is still a human
decision.

**`testing/fixtures/portup/`** is a corpus recorded live (never
hand-named — the file names are `url_slug`'s own output, or a fixture
silently reads as empty) against six ports, one per discovery path: `fuse`
(GitHub forge), `zlib` (a plain Apache-style directory listing),
`ca-certificates` (directory listing that comes back empty, falling through
to repology and only matching through the strip-separators/dot-collapse
normalisation — repology's dates need it to match `20251202`'s shape),
`aalib` (repology, genuinely `current` — upstream has not released since
2001), `mesa` (a large real directory listing), and `imagemagick`
(repology again, but `unknown`: its `source` template
(`imagemagick.org/archive/releases/…`) 404s even for its own pinned
version, so no rendered candidate ever proves — a real recipe bug, not a
tool bug, left unfixed here since it's outside this tool's scope).
`testing/selftest.sh` replays all six through `--fixture`, which makes
`pu_http_get`/`pu_http_head` read that directory instead of calling curl —
verified offline with `unshare --net`, not just by omission of a mock.

---

## Recurring build fixes

Apply the canonical fix when a build fails for one of these.

**Static-musl + Rust + bindgen → "Dynamic loading not supported".** Crates using
`bindgen`/`libloading` try to `dlopen libclang.so` at build time.
`export RUSTFLAGS="-C target-feature=-crt-static"; export LIBCLANG_PATH=/usr/lib`.
Affects librsvg, pipewire-sys and wayland-rs; it was first hit on the cosmic-*
ports, which are gone.

**CMake 4.x — "Compatibility with CMake < 3.5 has been removed".** Add
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.

**GCC 15 — incompatible-pointer-types is an error.**
`export CFLAGS="$CFLAGS -Wno-incompatible-pointer-types"`.

**"C compiler cannot create executables" from an old autotools port.** That
message blames the toolchain and is almost never about it: read
`$WORK/<port>/<src>/config.log` and the real error is on the failing conftest.
For anything with an autoconf 2.13-era `configure`, it is the K&R probe
`main(){return(0);}`, which GCC 14 promoted from warning to error. Suppress the
whole family at once — each one costs another hour-long round trip to find:
`-Wno-implicit-function-declaration -Wno-implicit-int -Wno-int-conversion
-Wno-incompatible-pointer-types -Wno-return-mismatch
-Wno-declaration-missing-parameter-type`. `ports/core/aalib` is the worked
example.

**Every meson `setup` needs `--prefix=/usr --libdir=lib`.** The default
`/usr/local/lib64` is not on the runtime linker's search path; symptom is
`Error loading shared library libxkbcommon.so.0`. The trap has a second edge:
**a stale `/usr/local/**/pkgconfig/*.pc` shadows the fixed one** — after fixing
a prefix, delete the old files *and* the old `.pc`, then rebuild the consumers.
Still outstanding: `libinput` installs to `/usr/local/lib64` and works only
because its consumers were linked against that path.

**libunwind — undefined `__unw_getcontext`.** The port builds without the
assembly files. Consumer workaround:
`export LDFLAGS="$LDFLAGS -Wl,--allow-shlib-undefined"`. Real fix (TODO):
rebuild `ports/core/libunwind` with the assembly enabled.

**ICU split — `ubrk_*` missing at link.** `libicuio.pc` doesn't propagate
`icu-uc`. `export LDFLAGS="$LDFLAGS -licuuc"`.

**gstreamer — `gst-ptp-helper` rust link fails.** `-Dptp-helper=disabled`.

**Meson option types.** boolean (`true`/`false`), feature
(`enabled`/`disabled`/`auto`), combo (a domain word list), string. On "Value
'foo' for option 'bar' is not one of the choices", read upstream's
`meson_options.txt`.

**URL / version landmines.** The GNOME mirror is stale for some projects
(NetworkManager current is on gitlab.freedesktop.org). GitHub projects split
between release assets (`releases/download/<tag>/<file>`) and auto-generated
archives (`archive/refs/tags/<tag>.tar.gz`). GitLab
`/releases/<v>/downloads/` needs manually-attached files — use
`/-/archive/<v>/<name>-<v>.tar.bz2`. `git.sr.ht` archives are rate-limited.
Tarball top-level dir naming varies — verify `tar -tzf | head -1` when the build
says source-not-found in `$SRC`. gitlab.freedesktop.org is anubis-blocked from
WebFetch.

---

## Init scripts

`fs/etc/init.d/` uses a numeric-prefix convention:

```
01_udev  02_modules  05_hostname  10_sysctl  15_userdirs  20_dmesg  22_syslog
30_network  35_chrony  40_dbus  42_networkmanager  45_avahi  45_seatd  50_alsa
55_powerd  55_tlp  56_energyd  60_bluetooth  70_sshd  80_cups
rcS  service_helper
```

Each sources `service_helper`, which is now three one-line wrappers around
**`ksvc`** — the C supervisor in `src/packages/kdos-tools`:

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

`supervise` runs the daemon under a respawn loop and writes `/run/<name>.pid`.
**The daemon must run in the foreground** — no daemonization. Note the argv:
`supervise "$NAME" "$DAEMON" --foreground`, not one quoted string. The old
helper did `local command="$@"` and then expanded it unquoted, so the
word-splitting was load-bearing and a daemon path with a space in it could not
be expressed.

**`ksvc` exists because the shell supervisor was broken.** The respawn loop
lived in a backgrounded subshell, and a subshell in a non-interactive shell
does not lead its own process group — so `stop_service`'s `kill -- -$pid`
addressed a group `$pid` did not lead, failed, fell through to a plain
`kill $pid`, and **killed the supervisor while orphaning the daemon**, all
while reporting success. The C supervisor calls `setsid()`, so the group kill
reaches both. Verified: after `ksvc stop`, supervisor and daemon are both gone
and the daemon's pgid was the supervisor's pid.

`ksvc` also refuses a service name that is not a plain name. `find_service`
used to interpolate the argument straight into a glob, so `service start '*'`
matched whatever it liked.

`/usr/sbin/service` and `/usr/local/sbin/kdos-getty` are **symlinks to
`/usr/sbin/ksvc`**, dispatched on basename. Neither is in `fs/` any more.

---

## Networking

Kernel: WireGuard built-in, nftables in tree. Userspace: `wpa_supplicant`,
`nftables`, `dnsmasq`, `dbus`; `basu` provides sd-bus. Manager: **NetworkManager
1.56** with `polkit=true`, `nft=/usr/sbin/nft`, `dnsmasq=/usr/sbin/dnsmasq`,
`iptables=` empty, no modemmanager/PPP/team/OVS, internal DHCP backend, gnutls
crypto. Auth: `polkit` with the `duktape` JS engine and `authfw=shadow` (no
PAM); `wheel` has admin privileges. VPN: `openvpn` + `networkmanager-openvpn`
(PKCS#11 off; cert/password/TOTP works).

---

## Users and login

The system ships one human user, **`kdos` / `kdos`** (uid 1000, `wheel`),
declared in `fs/etc/{passwd,group,shadow}`. `tty1` autologins as `kdos`; `tty2`
and the serial console give a root login. Homes are materialized from
`/etc/skel` by `06_packaging/00_user.sh`, `/run/user/<uid>` by
`fs/etc/init.d/15_userdirs.sh`. Alien apps run **rootless** under `kdos`, the
same on live ISO and installed disk. There is no first-boot wizard; renaming or
adding users is manual.

---

## Testing and the VM rig

- **`make run` (plain virtio-vga, no virgl) HAS a desktop but no CRT pass**:
  wlroots falls back to the pixman renderer there, and `kdos_crt_init()`
  declines anything that is not GLES2 (a fullscreen shader on software
  rendering is a slideshow). **`make run-hw`** (containerized Ubuntu-qemu 10,
  virgl+blob) is the GLES2 path with the pass on. Its machine string is
  `pc-i440fx-noble-v2`, easy to mistake for the host qemu.
- **Debug rig:** boot the ISO headless with `-serial unix:` + `-monitor unix:`
  sockets and **`-display egl-headless -vnc :N`** (virgl → GLES2, so the CRT
  pass runs; `KDOS_QEMU_DISPLAY` overrides the display in `run.sh`).
  `screendump` says "no surface" under GL — capture by reading the VNC
  framebuffer raw (RFB handshake + Raw encoding; `SetEncodings` is
  `type(1) pad(1) count(2)` then `count*int32` — an extra padding field
  desyncs the stream and every later read blocks), or run `grim` in-session:
  the fork carries wlr-screencopy.
- Monitor `sendkey` reaches the VT and focused Wayland windows (`sendkey
  meta_l-ret` opens foot). Drive the session from the serial root shell with
  `su - kdos -c '...'` — a **login** shell, because a plain `su kdos -c`
  leaves podman resolving HOME to `/` and every call fails with
  `mkdir /.local: permission denied`.
- Escape-only and space-only tty writes don't end fbcon's deferral.
- `testing/prepare_base.py` mini-builds a Phase 2 rootfs as a Docker base image;
  `testing/test_runner.py` builds individual ports against it; logs in
  `testing/logs/`.

---

## Working-state markers

```bash
ls ports/core | wc -l                                  # 405 ports
ls build/fs/var/lib/kpkg/db/ | wc -l                   # installed packages
git status --short | wc -l                             # tracked changes
ls build/logs/04_phase4/*.log                          # which packages have logs
tail -40 build/logs/04_phase4/<N>_<pkg>.install.log    # debug a failure
```

The user typically pastes a build log path when something fails — read its tail
and respond with the targeted fix.

---

## Outstanding gaps / TODO

- `libunwind` lacks its assembly files → undefined symbols; consumers work
  around with `--allow-shlib-undefined`.
- `libinput` still installs to `/usr/local/lib64` (port has no `--prefix`).
- No firewall rules shipped — `nftables` is installed, `/etc/nftables.conf` is
  empty.
- ~~cosmic-comp does not start Xwayland~~ — **closed by the rewrite.** wlroots
  runs Xwayland rootlessly and `kdos-comp` turns it on (`KDOS-DESKTOP.md` §7).
- **No `f2fs-tools`**, so `CONFIG_F2FS_FS=m` is a filesystem the kernel can
  mount and nothing here can create. (NTFS needs no port: `CONFIG_NTFS3_FS=m`
  is read-write in the kernel.)
  (`linux-firmware`, microcode, `man`, the clock, syslog, periodic jobs and
  mDNS were all on this list and are not any more. Source checksums too:
  `sha256 =` is a recipe key, preflight checks every archive against it, and
  only the three source-less ports of ours lack one.)
- **The tray does not render `com.canonical.dbusmenu`**, so an app that sets
  `ItemIsMenu` has no reachable menu. See the tray section.
- **A per-output panel shows EVERY window, not that output's.**
  wlr-foreign-toplevel reports `output_enter`/`output_leave` and the shell
  ignores them, so on two screens both taskbars list the same windows. That is
  GNOME 2's behaviour and not obviously wrong; filtering is a decision, not a
  fix, and it is not made.
- **libkwl does no HiDPI.** No `wl_surface.set_buffer_scale`, so on a scaled
  output the cell grid is upscaled by the compositor rather than drawn at the
  output's own scale. `chrome_font` in comp.conf is the mitigation, not the fix:
  it changes the pixel size the chrome is drawn at, which is the right answer on
  a machine with one screen and the wrong one on a machine with two of different
  densities.
- **libkwl binds no `wl_data_device` and no `wl_touch`**, so there is no
  clipboard and no drag-and-drop in any of the chrome — a file cannot be dragged
  off `kdos-desk` into a window, and nothing can be pasted into `kdos-run` or
  the file chooser's save name — and a touchscreen drives none of it.
- **`kdos-display` lays screens out edge to edge from x=0 in list order.** A
  vertical arrangement, an overlap, or a gap cannot be expressed. That is a
  deliberate narrowing rather than a missing feature: the protocol takes pixel
  coordinates and what people want is an order.
- ~~No FileChooser portal~~ — **closed.** `src/desktop/xdg-desktop-portal-kdos`
  serves FileChooser and Settings by spawning `kdos-pick` and reading its
  stdout; `kdos-portals.conf` routes both to it and ScreenCast/Screenshot to
  wlr. The remaining gap is `parent_window`, which is ignored: positioning a
  dialog over the window that asked for it needs xdg-foreign wiring that is not
  done, so it opens centred.
- **No `fcitx5-configtool`** — it is Qt Widgets, and with its browser also
  QtWebEngine. fcitx5 is configured through the text files under
  `~/.config/fcitx5/`.
- **No corefonts for wine.** winetricks fetches them from the network at run
  time and nothing in the image may depend on that, so a Windows program that
  wants Arial gets a substitute.
- **The baked appbox predates its own Containerfile.** `ports/appbox/image/` was
  packed on 2026-07-29; the KDE segment and the `kdos.qt-kde-theme` /
  `kdos.qt-gtk-theme` labels were added after. The shipped image therefore has
  91 apps, no KDE, and no labels — so `kdos-appbox` takes the
  `QT_QPA_PLATFORMTHEME=gtk3` branch and every Qt app in the box is grey. This
  is a re-bake (`make fetch-apps`, needs network), not a config bug, and until
  it happens the **appbox** section here describes the Containerfile rather than
  the ISO.
- **Occupancy in the panel's workspace strip is derived, not reported.**
  ext-workspace-v1 has ACTIVE, URGENT and HIDDEN and no "there are windows
  here", so `panel.c` marks the workspace being shown occupied when a toplevel
  is not minimized — which works because the fork re-reports a view on another
  workspace as MINIMIZED (`kdos_foreign_workspace_sync()`; stock labwc does
  not, and without the graft every visited workspace looked occupied). Right
  for every workspace the user has visited, silent about the rest.
- **Per-box `wayland.*` grants are gone.** The legacy compositor let a box
  profile grant an individual denied global back; labwc's security-context
  filter is a fixed allowlist — a client is sandboxed or it is not. Re-adding
  per-box grants means teaching `allow_for_sandbox()` to consult the boxsock
  profile, a deliberate piece of work.
  (Closed from this list by the labwc fork and the shell fixes:
  maximize/fullscreen, the five foot protocols incl. middle-click paste, the
  notifyd one-cell shrink — the layer surface is destroyed and recreated
  through the initial-commit handshake now — the kwl_pump EAGAIN/dead-
  compositor confusion — it uses the documented prepare_read/read_events
  sequence — and the cell grid's unpainted strip, padded with `KT_BG` by
  `kcell_paint` and asserted by `clipcheck`.)

---

## When the user says…

- **"fix"** + a build log path → read its tail, identify the root cause, apply
  the targeted fix in the relevant kpkgbuild. Don't speculate past the log.
- **"audit"** → grep across kpkgbuilds, packages.txt and `fs/` for residual
  references. Be systematic.
- **"add X"** → find the canonical upstream URL, pick the latest stable version,
  write a kpkgbuild matching the patterns above, wire it into `# depends` and
  `packages.txt`.
- **"why is X off"** → usually because deps weren't present when the port was
  added. Name the missing deps and offer to add them; don't pretend it was a
  deliberate choice.

Subagents in this harness can't run bash, so dispatch-and-review wastes time.
Just do the work inline.
