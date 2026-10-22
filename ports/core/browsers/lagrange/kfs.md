## About

Lagrange is a browser for the small internet: Gemini, Gopher, Finger, Spartan and Nex. It draws
its own interface through SDL and does its own text layout. The package installs two programs
built from the one source, `lagrange`, the windowed browser, and `clagrange`, the same browser in
a terminal on ncurses. Lagrange builds its own utility library, the_Foundation, and the terminal
build's curses layer, sealcurses, from copies under `lib/`.

## Flags

- `-DENABLE_GUI=ON`, `-DENABLE_TUI=ON` — Build both programs. The terminal one is off by default
  upstream.
- `-DENABLE_X11_XLIB=OFF` — Compile none of `src/platform/x11.c`, which calls Xlib directly when
  the window is on X11 (the `_NET_WM_DESKTOP` workspace hints and the `_GTK_THEME_VARIANT` dark
  title bar). SDL puts the window on Wayland here, and with this off the program links no libX11
  of its own.
- `-DENABLE_POPUP_MENUS=OFF` — Draw context menus inside the main window rather than opening
  each as a separate popup window, which upstream does by default.
- `-DENABLE_RESIZE_DRAW=OFF` — Do not force a redraw of the window during a resize; on by default
  everywhere but Haiku and Android.
- `-DENABLE_FRIBIDI=ON`, `-DENABLE_FRIBIDI_BUILD=OFF`, `-DENABLE_HARFBUZZ=ON`,
  `-DENABLE_HARFBUZZ_MINIMAL=OFF` — Shape text with HarfBuzz and lay out bidirectional text with
  FriBidi, both the system's ([harfbuzz](#port-harfbuzz), [fribidi](#port-fribidi)) through
  pkg-config rather than the copies under `lib/` that the `_BUILD` and `_MINIMAL` switches would
  compile. All four values are upstream's defaults.
- `-DENABLE_MPG123=ON`, `-DENABLE_OPUS=ON`, `-DENABLE_WEBP=ON`, `-DENABLE_JXL=ON` — Play MPEG and
  Opus audio and show WebP and JPEG XL images, through [mpg123](#port-mpg123),
  [opusfile](#port-opusfile), [libwebp](#port-libwebp) and [libjxl](#port-libjxl). Each is on by
  default and quietly left out when its library is not found; naming it does not change that, so
  it is the library being in `depends` that puts each one in.
- `-DENABLE_STATIC=OFF` — Link the dependencies shared; upstream's default.
- `-DTFDN_ENABLE_WARN_ERROR=OFF` — the_Foundation compiles with `-Werror` by default, so a warning
  the tree's compiler raises would stop the build.
- `-DTFDN_ENABLE_SSE41=OFF` — the_Foundation's default is whether the build machine has SSE 4.1,
  and on adds `-msse4.1` to its compile lines, so the package would need that instruction set
  wherever it ran. KDOS ships no feature level above baseline x86-64 (see
  [`-march` measured per machine](../../../../docs/kdos/01-philosophy/decisions.md#-march-measured-per-machine-not-chosen-for-a-population)).

## Notes

`zip` is in `depends` because the build packs the interface's fonts, translations and help pages
into `/usr/share/lagrange/resources.lgr`, which is a zip archive; configure stops without it.

Upstream writes a desktop entry for `clagrange` named plain `clagrange`, with an absolute `Exec`
path and the same URL-scheme `MimeType=` list as the windowed browser's. `build.sh` writes over
it: the name is `Lagrange (terminal)`, so the menu does not show two rows a person cannot tell
apart, `Exec` is bare, and there is no `MimeType=`, so `gemini:` and the other schemes open in
the windowed browser alone (see
[desktop entries](../../../../docs/kdos/05-developer/writing-ports.md#desktop-entries-belong-to-the-port)).
