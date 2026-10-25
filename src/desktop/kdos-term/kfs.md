## About

kdos-term is KDOS's own [terminal emulator]{.idx}. It runs as a window under `kdos-comp`, inside
another terminal with `--tty` (a Linux virtual console included), or offscreen with `--dump`,
which prints the final screen as text. Its state machine is `libkvt`, a fork of libtsm, the one
kmscon uses, and
`libkimg` turns sixel, iTerm2's OSC 1337 and kitty's APC pictures into pixels, so programs can
draw images in the cell grid. It also gives clickable links, prompt jumping and a font size per
window.

It ships beside `foot` and replaces nothing: `foot` stays the default terminal in the shipped
`rc.xml`, `menu.xml`, favourites and desktop entries. A desktop entry asks for this one with
`X-KDOS-Term=kdos-term`. Everything about it is in
[kdos-term](../../../docs/kdos/04-programs/kdos-term.md).

## Flags

- `-DKDOS_TERM_VERSION` — The string `libkvt` gives the child as `TERM_PROGRAM_VERSION`, here the
  recipe's `version`. A program built on `libkvt` without the define reports `0`.
- `-DHAVE_KIMG` — Compile the picture protocols of `pic.c` in. Without it the terminal leaves
  them off entirely rather than parsing and discarding them: `libkvt` ignores a sixel dump and an
  APC goes nowhere, so no buffer fills with bytes nothing can draw.
- `-DKIMG_HAVE_PNG`, `-DKIMG_HAVE_JPEG`, `-DKIMG_HAVE_WEBP`, `-DKIMG_HAVE_SIXEL`,
  `-DKIMG_HAVE_GIF` — Compile every one of `libkimg`'s optional decoders in, against `libpng`,
  `libjpeg-turbo`, `libwebp`, `libsixel` and `libnsgif`. A decoder that is left out refuses its
  format instead of failing to link. See
  [libkimg](../../../docs/kdos/05-developer/c-libraries.md#libkimg).

## Notes

- **ncurses is a dependency for its terminfo database**, not to link. The child gets
  `TERM=xterm-256color`, an entry ncurses ships, because a private `TERM` breaks the first time
  somebody types `ssh`; see
  [What the child sees](../../../docs/kdos/04-programs/kdos-term.md#what-the-child-sees).
- **wlroots is a dependency for its protocol files only.** The layer-shell and
  foreign-toplevel protocols are generated from `/usr/share/wlroots/protocols/`, which the
  `wlroots` recipe installs; the terminal links none of wlroots.
- **The whole protocol set is generated, used or not.** `libkwl` is compiled whole and its
  `kwl.c` includes the header of every protocol it can use, so the session-lock and layer-shell
  code is generated here too. A missing one fails the compile or the link.
- **`kdos-term.desktop` is installed by the port**, so the menu's **KDOS Terminal** row comes and
  goes with the package.
