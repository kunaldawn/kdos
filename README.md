<p align="center">
  <img src="kdos.png" alt="KDOS Mascot" width="300"/>
</p>

**KDOS** is a handmade, artisan-crafted Linux distribution built from the absolute ground up, following the sacred texts of **Linux From Scratch (LFS)**.

Powered by **Musl Libc** and **Toybox**, KDOS aims to be the digital equivalent of a katana: minimal, sharp, and lightweight.

---

## The Philosophy

Most distros give you a house. KDOS gives you a pile of bricks, a trowel, and a blueprint written in Bash.

1.  **LFS Roots** — every byte of the OS is hand-compiled. If we didn't compile it, we don't trust it.
2.  **Musl Libc** — Glibc is a metropolitan city. Musl is a Zen garden. We choose the garden.
3.  **Toybox** — one binary that rules them all.
4.  **No SystemD** — we init like our ancestors did. Inittab, serial `init.d`, simple service helpers.
5.  **No GTK / GNOME / KDE / XFCE on the host** — fat GUI apps live in a Podman container alongside their glibc. *Qt 6 is the single carved-out exception, present only because the shell layer (Quickshell + noctalia) is QML.*
6.  **KPKG** — our own package manager. For fun, and because we earned it.

---

## The Outer Ring: Alien Apps via Distrobox

KDOS deliberately does not native-port browsers, IDEs, office suites, video editors, or chat clients. Those are the things upstream `apt`, `dnf`, and `pacman` already package well.

Instead, KDOS ships **Podman + distrobox**. On first boot you get a default `kdos-debian` distrobox (Debian glibc rootfs), and the helper:

```sh
kdos-fetch-app firefox        # apt-installs firefox in the box and exports it as a host launcher
kdos-fetch-app gimp           # same for gimp
kdos-fetch-app --remove gimp  # remove the export and uninstall in the box
```

A `.desktop` file lands in `~/.local/share/applications/` and is picked up by the niri launcher (`fuzzel`). The host musl tree never sees a single glibc dep.

For single-binary tools (zig, Go binaries, single-binary Rust apps), there's also:

```sh
kdos-fetch-static <name> <url> <sha256>
```

— curl + sha256-verify + chmod into `/usr/local/bin`. The most suckless answer for "I just want this one tool."

---

## The GUI Sliver: niri + noctalia (Wayland)

When a GUI is genuinely needed, run `niri-session` from your tty login — it `dbus-run-session`s niri with `--session`, and the autostart line in the skel config spawns noctalia.

**niri** is the compositor — Smithay-based, scrollable-tiling, no Xorg. **Noctalia** (QML on a Quickshell fork) is the shell layer: top bar, panels, notifications, lock screen, widgets. KDOS runs Wayland-only: no X server, no XWayland by default. Session/seat handled by `seatd` (auto-started at boot via `45_seatd.sh`). Portals via `xdg-desktop-portal-wlr`. Wayland env (XDG_RUNTIME_DIR, QT_QPA_PLATFORM, etc.) set on every login by `/etc/profile.d/10-wayland.sh`.

The GUI inventory is intentionally small:

- `foot` — terminal (Wayland-native, fast; primary daily driver)
- `fuzzel` — keyboard launcher (`Mod+D`; complements noctalia's UI launcher)
- `grim` + `slurp` — screenshot + region selector
- `wl-clipboard` — clipboard (`wl-copy` / `wl-paste`)
- `imv` — image viewer

Anything else (file manager, editor, calculator) is done in a terminal — `lf`, `nvim`, `bc`. Anything fatter than that is a `kdos-fetch-app` away.

---

## License

MIT. Go forth and segfault.
