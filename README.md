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
5.  **No GTK / Qt / GNOME / KDE / XFCE on the host** — fat GUI apps live in a Podman container alongside their glibc.
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

A `.desktop` file lands in `~/.local/share/applications/` and the app appears in the Window Maker root menu. The host musl tree never sees a single glibc dep.

For single-binary tools (zig, Go binaries, single-binary Rust apps), there's also:

```sh
kdos-fetch-static <name> <url> <sha256>
```

— curl + sha256-verify + chmod into `/usr/local/bin`. The most suckless answer for "I just want this one tool."

---

## The GUI Sliver: Window Maker

When a GUI is genuinely needed, `startx` brings up **Window Maker** — the NeXTSTEP-style stacking WM. Dock on the right edge, Clip in the top-left, root-click for the app menu. The default theme is from `wmaker-extra` (NeXTSTEP / NeXTSTEP-orig / Sehnsucht).

The GUI inventory is intentionally small:

- `st` — terminal (Suckless-style; primary daily driver)
- `nsxiv` — image viewer
- `mupdf` — PDF reader (its own X frontend, no GTK)
- `scrot` — screenshot
- `dunst` — notifications
- `xclip` — clipboard
- `xdotool` — automation

Anything else (file manager, editor, calculator) is done in a terminal — `lf`, `nvim`, `bc`. Anything fatter than that is a `kdos-fetch-app` away.

---

## License

MIT. Go forth and segfault.
