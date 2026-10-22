## About

kdos-shell is the program behind almost everything a person clicks on the KDOS desktop: the
[panel]{.idx} along the bottom of the screen, the Start menu, the search palette, the desktop
icons, the file chooser, the control centre, the device managers, the notification daemon and the
small windows and popups around them. It is one binary that behaves as a different program
depending on the name it is started under. `build.sh` installs it once as `/usr/bin/kdos-shell`
and makes every other name a symbolic link to it; run as `kdos-shell` it is the panel. Every
surface is described in [kdos-shell](../../../docs/kdos/04-programs/kdos-shell.md).

It is a layer-shell client that draws a grid of character cells through KDOS's own libraries,
compiled from `src/libs` straight into the binary, and it links no toolkit. What it shows of other
programs' windows it learns from the standard `wlr-foreign-toplevel-management` and
`ext-workspace-v1` protocols. `xdg-desktop-portal-kdos` depends on it, because the portal answers
a file-chooser request by running `kdos-pick` and an access question by running `kdos-prompt`, two
of its names.

## Flags

- `-DKIMG_HAVE_PNG`, `-DKIMG_HAVE_JPEG`, `-DKIMG_HAVE_WEBP`, `-DKIMG_HAVE_GIF` — Compile
  libkimg's PNG, JPEG, WebP and GIF decoders in, against `libpng`, `libjpeg-turbo`, `libwebp` and
  `libnsgif`, for the surfaces that show a picture, `kdos-pix` and `kdos-peek` among them. Each
  decoder is optional: a format whose define is missing is refused, not a link error. The sixel
  decoder is left out, because a sixel picture arrives as a terminal escape sequence and never as a
  file; see [libkimg](../../../docs/kdos/05-developer/c-libraries.md#libkimg).

## Notes

- **Every name needs a link line.** `build.sh` makes one `ln -s kdos-shell` for each name in the
  dispatch table in `main.c`. A table entry with no entry point fails to link, but a table entry
  with no link is a program nothing can start; see
  [Invocation](../../../docs/kdos/04-programs/kdos-shell.md#invocation). The style picker is
  installed as `kdos-style` because `kdos-theme` is the name of the theme generator's port and
  program.
- **ICU is needed only at build time.** `tools/mkcharidx.c`, compiled and run by `build.sh`,
  links ICU to write `charnames.idx`, the Unicode character-name index installed in
  `/usr/share/kdos/`. `kdos-chars` maps that file and searches it in place. `kdos-shell` itself
  must not link ICU, a library and data file of tens of megabytes that every surface the binary is
  would then load.
- **wlroots is a dependency for its protocol files only.** The `wlr-*` protocols are
  generated from `/usr/share/wlroots/protocols/`, which the `wlroots` recipe installs; the shell
  links none of wlroots.
- **The whole protocol set is generated, used or not.** `libkwl` is compiled whole and its
  `kwl.c` includes the header of every protocol it can use, so each of its consumers generates
  the client header and the `private-code` of all of them. A missing one fails the compile or the
  link.
- **`fontconfig` is named beside `fcft`** on the pkg-config line because `libkcell` and `libkwl`
  call fontconfig themselves, and `fcft` lists it only as `Requires.private`, which does not put
  it on the link line.
- **Several external libraries serve a few surfaces each:** `libarchive` lets `kdos-peek` look
  inside archives, `alsa-lib` serves the volume controls of `kdos-audio` and `kdos-osd`,
  `pipewire` serves `kdos-audio` and the privacy indicator, and `basu` carries its D-Bus
  conversations, from the notification daemon to the tray. `libksig`, with the Monocypher sources
  it bundles, is compiled in for `kdos-about`, which reads the system's key rings to list the keys
  it trusts.
