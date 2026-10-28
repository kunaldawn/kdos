## About
`kdos-theme` is the generator that paints KDOS's borrowed artwork in an accent: it reads the
vendored adw-gtk3 stylesheet, Papirus icons and Bibata cursors and writes a GTK theme, an icon
theme and an Xcursor theme in that accent's colours. It is a plain text-and-bytes rewrite,
hex colours swapped in the stylesheet and the SVGs and pixels remapped in the Xcursor files, so it
parses no SVG, CSS or PNG and links nothing beyond the C library.

It runs at two different times. At build time the three theme ports,
[kdos-icons](#port-kdos-icons), [kdos-cursors](#port-kdos-cursors) and
[kdos-gtk-theme](#port-kdos-gtk-theme), depend on it and run it from their `build.sh` to make the
system copies under `/usr/share`. On the running system `kdos theme`, and the `kdos-style` picker
through it, runs it on every accent switch to rewrite the copies in your home directory, which is
why it is a package and not a build tool: written in C, it lets the shipped system regenerate its
themes without python3. Its commands and defaults are in
[How the theme is generated](../../../docs/kdos/02-user-guide/theming.md#how-the-theme-is-generated).

## Flags
- `-std=gnu11`, `-D_GNU_SOURCE` — The C dialect, and the macro that exposes the C library's GNU and
  POSIX extensions to the program and the two libraries compiled with it. The tree's `CFLAGS`
  already names the same `-std`.
- `-I"$LIBS/libkbase"`, `-I"$LIBS/libkcolor"`, `-I"$PORT_SRC"` — The headers of the two
  libraries it uses and of the program itself. `$LIBS` is `src/libs`, reached from the port
  directory.
- `"$LIBS"/libkbase/*.c`, `"$LIBS"/libkcolor/*.c` — Compile both libraries into the binary. A
  `libk*` library is a directory of sources, never an installed archive; see
  [How the libraries are built](../../../docs/kdos/05-developer/c-libraries.md#how-the-libraries-are-built).
  The palette and the hue mapping come from libkcolor, so a theme generated here and the colours
  the desktop draws in are the same numbers.
- `-O2`, `-Wall`, `-Wextra` — The tree's optimisation level, named again, and the compiler's
  common and extra warnings.

## Notes
The three theme ports call it at build time, so it has to be installed first; each names it in
`depends`. A change to its output reaches the system copies only when those three ports rebuild,
and reaches a home directory only on the next `kdos theme`. The recipe hash of a port of KDOS's
own covers all of `src/libs`, so an edit to libkcolor's palette rebuilds this port and the three
theme ports together.

`--src` beats the environment, which beats the installed path: `KDOS_GTK_SRC`, `KDOS_ICON_ART`,
`KDOS_ICON_MARKS` and `KDOS_CURSOR_ART` let a caller point it at a tree that is not under
`/usr/share` yet, and the theme ports pass `--src "$PORT_SRC/…"` for that reason.
