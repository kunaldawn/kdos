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
server). Desktop is **COSMIC** (System76 — Rust/smithay/iced), pinned at one
epoch release across all components. **No GTK and no Qt on the host.**
Everything fat runs in a Podman/distrobox glibc rootfs. Session entry:
`kdos-desktop` from a tty.

Four properties define the project. Everything else follows from them:

1. **Built from scratch.** Every host byte is compiled here from an upstream
   tarball by a `kpkgbuild` recipe in `ports/`. 387 ports, 353 installed.
2. **KDOS can build KDOS.** Phase 2 is a self-hosting bootstrap — the chroot
   rebuilds tar/musl/zlib/binutils/gcc with itself. The shipped system carries
   gcc, binutils, rust, cmake, meson, ninja, python3, make and `kpkg`, so a
   running KDOS can rebuild every port in the tree.
3. **The repo builds offline.** `make build` runs `--network none`. Every
   upstream tarball, every `cargo vendor` bundle and the whole ~4 GB alien-app
   container image live in-tree under Git LFS. Clone, `make build`, get an ISO,
   no network.
4. **Alien apps.** KDOS deliberately does not native-port browsers, office
   suites or IDEs. ~90 GUI apps ship pre-baked in a Debian container and behave
   like ordinary system apps — launchers, MIME handlers, terminal commands.

Mascot: `kdos.png`. Wallpaper: `fs/usr/share/backgrounds/kdos/default-wallpaper.png`.

---

## Hard rules — do not violate

1. **No systemd.** No `systemd-*` on the host. Replacements: `seatd` (seat
   management), `basu` (sd-bus), `eudev` (udev), `dbus` (not dbus-broker),
   `dnsmasq` (not systemd-resolved), `wpa_supplicant`/`NetworkManager` (not
   systemd-networkd).
2. **No Xorg server.** No `xorg-server`, no display manager, nothing X on the
   login path. **Xwayland is the one carve-out**, run rootlessly by cosmic-comp
   itself so X11-only alien apps work. It pulled in the client chain
   `xorgproto xtrans libXau libXdmcp xcb-proto libxcb libX11 libxkbfile xkbcomp
   libxshmfence libfontenc libXfont2 xcb-util{,-image,-renderutil,-cursor}` —
   those exist **only** to satisfy Xwayland/xkbcomp. A kpkgbuild that wants X
   for anything else gets pushed back. Mesa is `-Dglx=disabled
   -Dplatforms=wayland`, so Xwayland is built `-Dglx=false`: X clients get no
   OpenGL. Enabling it means rebuilding mesa with `glx=dri,platforms=wayland,x11`
   plus libXext/libXfixes/libXdamage/libXrandr/libXxf86vm — not done.
3. **No GTK and no Qt on the host.** GUI apps go in a container. COSMIC's iced
   toolkit needs neither.
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
| **GUI sliver** | also `ports/core/` (separate `packages.txt` block) | COSMIC desktop (17 `cosmic-*` Rust ports + pop-launcher), foot, Wayland CLI utils |
| **Outer ring** | the baked appbox container | Browsers, IDEs, office, media, CAD, games — full glibc apps |

**`src/packages/`** is the second port repo (`PORT_REPO="/ports/core
/kdos/src/packages"`) and holds what is OURS rather than an upstream tarball:
`kdos-splash`, `kdos-appbox`, `kdos-theme-helper`, and the three
vendored-and-remade art packages `kdos-cursors` (Bibata), `kdos-icons`
(Papirus), `kdos-gtk-theme` (adw-gtk3).

The three art packages share one shape, and new vendoring should copy it: a
host-only `vendor.py` that prunes upstream into a committed `art/`/`theme/`
tree, plus a `gen*.py` that the build **and** `kdos theme` run to recolour it.
That keeps the palette a KDOS decision and avoids carrying `.patch` files
against artwork.

---

## The appbox — offline alien apps

`ports/appbox/Containerfile` defines a **debian trixie** `kdos-apps` image:
the best open-source GUI app per segment, ~90 launchers (libreoffice, calibre,
gimp, krita, blender, freecad, prusa-slicer, openscad, kicad+gtkwave+ngspice,
octave, maxima, stellarium, ardour, hydrogen, lmms, kdenlive, obs-studio,
vscodium, wireshark, keepassxc, backup tools, games, emulators, firefox-esr, …).
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

- **Carry `MimeType` through from upstream.** Without it no alien app appears
  in any "Open with" dialog and none can be a default handler. The cache is
  written here rather than by `update-desktop-database` because the host has no
  desktop-file-utils.
- **The launcher FILENAME must be upstream's own desktop id** — not
  `kdos-<name>`, and not `StartupWMClass`. `cosmic-app-list` matches a running
  toplevel to a desktop entry by the entry's FILE ID and ignores
  `StartupWMClass`; a mismatch shows a second grey cog beside the pinned icon.
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
- **X11-only apps need `DISPLAY` pushed in explicitly.** cosmic-comp runs
  Xwayland rootlessly but exports DISPLAY only to what IT spawned;
  `kdos-appbox` probes `/tmp/.X11-unix/X*` (distrobox shares the host /tmp) and
  adds `DISPLAY=` to BOXENV.
- **Qt theming needs two things and one is gated.** `QT_QPA_PLATFORMTHEME=gtk3`
  is inert without debian's `qt{5,6}-gtk-platformtheme`, so an appbox baked
  before those were added leaves every Qt app grey — that is a re-bake, not a
  config bug. The platform theme alone is not enough either: the Breeze style
  that kdenlive and shotcut pull in paints from its own colour scheme, so
  `QT_STYLE_OVERRIDE=Fusion` is needed too. Fusion with NO platform theme falls
  back to Qt's built-in LIGHT palette — worse than nothing — so `kdos-appbox`
  sets it only when `podman image inspect` reports the `kdos.qt-gtk-theme=1`
  label the Containerfile declares in the same layer that installs the platform
  themes. The answer is cached in `$XDG_RUNTIME_DIR` (150 ms inspect vs a 300 ms
  warm launch; the image cannot change without a reboot).
- Audio and OBS screen capture come from the HOST: `kdos-desktop` execs
  `kdos-desktop-start` inside the session bus, which brings up pipewire +
  pipewire-media-session + pipewire-pulse and then execs cosmic-session. Portals
  are D-Bus-activated on demand; OBS captures via portal→ScreenCast→pipewire,
  with the sockets reaching the box through the shared `/run/user/1000`. The box
  needs debian's `obs-plugins` package — debian splits OBS's plugins out as a
  Recommends and `linux-pipewire.so` (the only Wayland capture path) lives there.
- **The `xdg-desktop-portal` main daemon snapshots its backends at startup.**
  `kdos-desktop-start` waits for the compositor socket, pushes WAYLAND_DISPLAY
  into the D-Bus activation environment, starts `xdg-desktop-portal-cosmic`,
  WAITS for it to own its bus name, and only then (re)starts the main portal —
  or ScreenCast stays empty all session.
- **Never answer a ScreenCast Start with zero streams.** OBS 30.2's
  `on_start_response_received_cb` does `if (n_streams != 1) for (size_t i = 0;
  i < n_streams - 1; i++)`: with `n_streams == 0` the unsigned subtraction wraps
  and it spins on an exhausted iterator at ~5 MB/s of log with the UI wedged
  (on the live ISO that log is tmpfs, so it eats RAM). The portal produced that
  because `screencast_dialog.rs` bound Enter to `Msg::Share` unconditionally
  while the Share *button* was gated on a non-empty selection.
  `ports/core/xdg-desktop-portal-cosmic/no-empty-streams.patch` fixes both ends:
  the Enter handler checks the selection, and `Start` answers `Cancelled` rather
  than an empty Success. Debug notes: OBS's stdout is block-buffered when
  redirected (use `stdbuf -oL` or read `~/.config/obs-studio/logs/*.txt`), and
  the host has no `dbus-monitor` — run debian's from inside the appbox, it
  shares the session bus.

### `kdos-appbox` is a C program (`src/packages/kdos-appbox`)

`/usr/local/bin/kdos-appbox` is C: `main.c` (CLI + launch path), `box.c`
(boxes and profiles), `app.c` (app table, install/refresh), `tui.c` (the
front end), `launchers.c` (`genlaunchers`), `image.c` (`image
pack|assemble|remap-uids`), `util.c` (the trace file and the notification).
It links
**libkbase + libktui + libkcolor and nothing else** — the process/path/lock
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
a11y stack is reachable in the box). Stage timings are appended to
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
clients — cosmic-session aborts with "Server GUID mismatch"), and never add a
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

## Theming — PHOSPHOR on COSMIC

COSMIC reads layered RON config: `/usr/share/cosmic` (system defaults) then
`~/.config/cosmic` (user). KDOS seeds the user side from
`fs/etc/skel/.config/cosmic/`:

| File (under `~/.config/cosmic/`) | Purpose |
|---|---|
| `com.system76.CosmicBackground/v1/all` | wallpaper → the penguin |
| `com.system76.CosmicTk/v1/icon_theme` | `"KDOS"` (kdos-icons) |
| `com.system76.CosmicPanel.Panel/v1/*` | floating top panel: phosphor bg Color, opacity 0.92, radius 12, trimmed wings, `keep_style_on_maximize` true (else a maximized window snaps the panel to edge-to-edge default styling) |
| `com.system76.CosmicPanel.Dock/v1/*` | dock: phosphor bg Color, size M, `keep_style_on_maximize` true, `plugins_center` = app-library button + workspaces + app list + minimize |
| `com.system76.CosmicAppList/v1/favorites` | dock pins that actually exist: foot, CosmicFiles, firefox-esr, org.xfce.mousepad, gimp, CosmicSettings |
| `com.system76.CosmicAppLibrary/v1/groups` | KDOS launcher groups: Internet, Graphics, Office, Media, Engineering, Science, System, Utilities — Categories-driven (`AppGroup` RON; field docs in upstream `app_group.rs` are swapped — `exclude` excludes, `include` force-includes) |

The applied theme is NOT a skel seed: `com.system76.CosmicTheme.Dark/v2/` is
generated at packaging time by `script/06_packaging/00_theme.sh`, which runs
**`kdos theme phosphor`** itself against `/etc/skel` (HOME + XDG_CONFIG_HOME
pointed there) rather than duplicating the palette — one generator, no drift.
That calls **kdos-theme-helper** (`src/packages` — drives cosmic-theme's own
ThemeBuilder; the Theme struct is `#[version = 2]`, hand-seeded v1 files are
silently ignored).

**Alien apps are themed through `$HOME`, not through COSMIC** — the appbox
shares the home directory and nothing else, so `/usr/share/themes` and
`/usr/share/icons` are invisible inside the box:

| Path | Written by | Read by |
|---|---|---|
| `~/.themes/KDOS/` | `write_gtk` → `kdos-theme gtk` | GTK3 and non-libadwaita GTK4 apps |
| `~/.icons/KDOS/` | `write_icons` → `kdos-theme icons` | every toolkit, host and box |
| `~/.config/gtk-{3,4}.0/gtk.css` | `write_gtk` | libadwaita (which ignores themes entirely) |
| `~/.icons/KDOS-cursors/` | kdos-cursors' `/etc/skel` copy | cursor lookup in the box |

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

**`kdos theme <phosphor|amber|ice|bone>`** owns everything else: it reruns
kdos-theme-helper, writes the panel+dock background Color RON
(`write_panel_colors`), regenerates `~/.themes/KDOS` and `~/.icons/KDOS`, writes
the two `gtk.css` files, `~/.config/foot/themes/kdos`,
`~/.config/btop/themes/kdos.theme`, and the palette block between the
`# >>> KDOS STARSHIP PALETTE >>>` markers in starship.toml. One palette table
inside `fs/usr/local/bin/kdos` drives all generators. COSMIC and cosmic-panel
repaint live; starship on next prompt; foot and btop on next start (foot cannot
reload its config, and KDOS's pkill has no `-x`).

Known quirk: a live `kdos theme` switch makes cosmic-panel restart its applets,
and the respawned cosmic-app-list collapses all pinned favorites into its
overflow button (panel-size renegotiation race; upstream 1.4 behaviour —
restarting cosmic-panel does not heal it, the next login does).

### kdos-icons

Theme `KDOS`, a **vendored, pruned, recoloured Papirus**,
`Inherits=Cosmic,Pop,hicolor`. Two scripts, same split as kdos-cursors:

- **`vendor.py`** is the maintenance tool, run by hand on the host with network:
  `vendor.py papirus-icon-theme-YYYYMMDD.tar.gz` rewrites `art/`. Papirus over
  Tela/Colloid/Qogir purely for coverage — the only free set with a real icon
  for essentially every mimetype, device and place COSMIC or the ~90 Debian apps
  will ask for, and flat single-fill SVG, so a palette remap is a substitution
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
context and every app icon — COSMIC's own included — would come through
untinted. So `kdos-theme icons` also recolours two directories into `scalable/apps`:
all of `Cosmic/`, and `com.system76.*` ONLY from `hicolor/` (packaging installs
the ALIEN apps' icons into that same tree, and a phosphor Firefox logo is
vandalism, not theming). **Sweep every size directory, not just `scalable/`** —
`com.system76.CosmicFiles` is an SVG filed under 24x24/128x128/256x256 and
nowhere else, so a scalable-only pass leaves exactly the dock buttons the user
looks at unthemed. Largest variant per name wins.

The KDOS marks (`distributor-logo-kdos` / `start-here`, and the tux over
`com.system76.CosmicAppLibrary` + `com.system76.CosmicPanelAppButton`, the
dock's app-library button) are installed by the GENERATOR, not the kpkgbuild —
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

The default wallpaper carries a baked CRT treatment (scanlines every 3rd row +
vignette, imagemagick multiply — regenerate from a clean render if replaced).
cosmic-comp has no custom-shader API, so the CRT identity lives in the boot
splash, the wallpaper, the TTY and the palette.

---

## The `kdos` command

`kdos` is the front door: `help` (commands + COSMIC keybind cheat sheet),
`theme`, `status`, `doctor`, `app`, `version`. `kdos doctor` checks the things
that have actually broken on this distro — including `readlink
/proc/self/root`, the switch_root trap above.

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

Static archives, `libk*`, **linking nothing but musl**. That is the constraint
the whole set exists under: libktui has to be usable in phase 1, before any
library exists to link against. If a lib ever needs a real `-l`, every phase-1
consumer moves to phase 4 with it.

| Lib | Prefix | Owns |
|---|---|---|
| `libkbase` | `kb_` | alloc + OOM hook, `die`/`warn`, strings, files, paths, flock, monotonic time, **the `KbArgv` builder and `kb_run`/`kb_run_capture`/`kb_run_detach`** |
| `libkcolor` | `kcol_` | **the palette table**, hex/HLS, `kcol_mix`, the hue-family classifier, `kcol_remap`, `kcol_retint_text` |
| `libktui` | `ktui_`, `KT_`, `KRect`/`KRgb`/`KtuiEvent` | terminal ownership, cell buffer + diff flush, key/mouse decoding, immediate-mode widgets, modals, text furniture |
| `libkxdg` | `kxdg_` | desktop entries, matching what `RawConfigParser(strict=False)` did with them |
| `libkpkg` | `kp_` | the package database, the ports tree, `# depends` parsing, the dependency solver |
| `libkbuild` | `kbuild_`, `kj_` | phase discovery, the phase-env metadata block, the build plan, the snapshot inventory, a read-only JSON scanner |

Dependency direction is `libktui → libkcolor → libkbase` and `libkxdg →
libkbase` and `libkbuild → libkbase`, and nothing points back up.

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

Known limit, unchanged from the python version: `kdos theme <accent>` does not
retint the cursors, because the cursor `art/` is not installed to the target.
The generator itself is fully parameterised now, so shipping `art/` is all
that stands in the way.

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

Known gaps, each for a missing port rather than a missing feature: no LUKS
(`cryptsetup` is not installed and the initramfs cannot unlock anything), no
btrfs/xfs/f2fs (no mkfs), and the time zone is written as a **POSIX TZ string**
into `/etc/profile.d/20-timezone.sh` because there is no `tzdata` — musl parses
those directly, DST rules included.

Iterate without booting: `kinstall --dry-run` logs every command and executes
none, and `--save`/`--config`/`--unattended` give it an answer file.

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
│   │   ├── libkxdg/             # kxdg_* desktop entries
│   │   ├── libkpkg/             # kp_*   db, ports tree, dependency solver
│   │   └── libkbuild/           # kbuild_* phases, plans, snapshot inventory
│   ├── build/
│   │   └── kdosbuild/           # the build orchestrator (C, host-only)
│   └── packages/                # ports that are OURS (see Three Rings)
│       ├── kdos-installer/      # the installer (C, zero libraries)
│       ├── kdos-kpkg/           # kpkg + the four names it answers to
│       ├── kdos-theme/          # the GTK/icon/cursor generators
│       └── kdos-tools/          # kdos, ksvc/service, kdos-getty, banner,
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
| `04_phase4` | Userland & GUI Sliver |
| `05_phase5` | Kernel |
| `06_packaging` | trim rootfs, theme, user, appbox, initramfs, ISO |

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
make build BUILD_ARGS="--phases 04_phase4,06_packaging --rebuild cosmic-comp,cosmic-panel"

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
ETA, the telemetry sampler) and `tui.c` (the four screens). It sits on
libkbuild for everything that only INSPECTS the tree.

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

It cannot prove the build works. It can prove the build will not fail for one
of the dull reasons. Run it after touching a `packages.txt`, a recipe, or
anything under `script/`.

### `testing/selftest.sh`

Host-only regression net for `src/libs/`. Compiles every library with the host
compiler under `-Werror`, runs `src/libs/selftest.c` against them, then
compiles all five consumers to prove the headers still agree, and resolves a
port to prove the ports tree still parses. No container, no network, seconds
to run.

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
It links libkbase and libkpkg and nothing else. There are no shell scripts in
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

Every one of the 396 ports is `kpkgbuild` + `build.sh`. There is no second
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

**`kpkg verify <port>` is how a recipe CHANGE gets checked.** It builds the
port with its current `kpkgbuild` + `build.sh` and again with the
`kpkgbuild.new` / `build.sh.new` beside them, then compares the two packages'
file lists. Neither build happens in the ports tree: a scratch directory is
filled with symlinks to everything the port carries — tarballs, patches,
vendor bundles — and only the files being tested are real. An interrupted
verify leaves the tree untouched.

It compares the file LIST, not payload bytes. That catches the dominant error
(a missing or extra file) and does not trip over `.POSTINSTALL`.

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

## Recurring build fixes

Apply the canonical fix when a build fails for one of these.

**Static-musl + Rust + bindgen → "Dynamic loading not supported".** Crates using
`bindgen`/`libloading` try to `dlopen libclang.so` at build time.
`export RUSTFLAGS="-C target-feature=-crt-static"; export LIBCLANG_PATH=/usr/lib`.
Affects the cosmic-* ports, librsvg, pipewire-sys, wayland-rs.

**CMake 4.x — "Compatibility with CMake < 3.5 has been removed".** Add
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.

**GCC 15 — incompatible-pointer-types is an error.**
`export CFLAGS="$CFLAGS -Wno-incompatible-pointer-types"`.

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
01_udev  02_modules  05_hostname  10_sysctl  15_userdirs  20_dmesg  30_network
40_dbus  42_networkmanager  45_seatd  50_alsa  60_bluetooth  70_sshd  80_cups
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

- **`make run` (plain virtio-vga, no virgl) has NO desktop**: smithay refuses
  software EGL renderers ("software EGL renderers are skipped" → "no allocator
  available"), so cosmic-comp gets no outputs. Only **`make run-hw`**
  (containerized Ubuntu-qemu 10, virgl+blob) shows the desktop. Its machine
  string is `pc-i440fx-noble-v2`, easy to mistake for the host qemu.
- **Debug rig:** boot the ISO headless with `-serial unix:` + `-monitor unix:`
  sockets and **`-display egl-headless -vnc :19`** (the `gtk,gl=es` window path
  can wedge the guest in a soft-lockup storm once cosmic-comp starts;
  `KDOS_QEMU_DISPLAY` overrides it in `run.sh`). `screendump` says "no surface"
  under GL, and cosmic-comp has NO wlr-screencopy so grim fails — capture by
  reading the VNC framebuffer raw (RFB handshake + Raw encoding; `SetEncodings`
  is `type(1) pad(1) count(2)` then `count*int32` — an extra padding field
  desyncs the stream and every later read blocks), or use `cosmic-screenshot`
  in-session.
- Monitor `sendkey` reaches the VT and reaches focused Wayland windows; the HMP
  mouse does not drive cosmic-comp, and a layer-shell surface launched
  standalone (e.g. `cosmic-app-library`) never takes keyboard focus. Drive the
  session from the serial root shell with `su - kdos -c '...'` — a **login**
  shell, because a plain `su kdos -c` leaves podman resolving HOME to `/` and
  every call fails with `mkdir /.local: permission denied`.
- Escape-only and space-only tty writes don't end fbcon's deferral.
- `testing/prepare_base.py` mini-builds a Phase 2 rootfs as a Docker base image;
  `testing/test_runner.py` builds individual ports against it; logs in
  `testing/logs/`.

---

## Working-state markers

```bash
ls ports/core | wc -l                                  # 387 ports
ls build/fs/var/lib/kpkg/db/ | wc -l                   # 353 installed
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
- **cosmic-comp does not start Xwayland.** `/tmp/.X11-unix` is absent although
  `/usr/bin/Xwayland` is installed and smithay's xwm is compiled in, so nothing
  X11-only can run. Next step is capturing the compositor's stderr, which needs
  a change to `kdos-desktop`.

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
