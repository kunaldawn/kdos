## About
`kdos-splash` is the boot animation: a CRT power-on drawn straight to `/dev/fb0`, then the list of
boot stages under a progress bar, until `rcS` quits it just before the login prompt. One process
covers the whole boot. The initramfs starts it, it survives `switch_root`, and later stages reach
it through the FIFO `/dev/.kdos-splash`. What it draws, the commands it takes and the three facts
about the framebuffer that constrain it are in
[The splash](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#the-splash).

The build compiles one C file and installs one font. `build.sh` compiles `kdos-splash.c` to a static binary and
decompresses Terminus' 16-pixel console font from [terminus-font](#port-terminus-font) to
`/usr/share/kdos/splash.psf`, the file the program loads its glyphs from. The image step
`script/phases/70_image/090_initramfs.sh` copies both into the initramfs; when the binary is
missing the build warns and the machine boots with no splash.

## Flags
- `-static-pie` — Link a static, position-independent executable. The initramfs step copies
  `kdos-splash` with a plain `cp`, without the shared libraries it gathers for its other programs,
  so the binary must need none.
- `-I"$LIBS/libkcolor"` — Find `kcolor.h` in the tree's colour library. The splash takes only the
  header's table of accent schemes, compiled in because no `/etc/kdos/accent` can be read when the
  initramfs starts it; it links no code from libkcolor.
- `-lm` — The maths library, for the `sin` and `pow` the animation's curves are computed with.
- `-O2`, `-Wall` — The tree's optimisation level, named again, and the usual warnings.

## Notes
`splash.psf` is decompressed at build time because `font_load()` reads a bare PSF1 or PSF2 file:
the static binary carries no decompressor. It falls back to
`/usr/share/consolefonts/ter-v16n.psf` and then exits without drawing, which is what every
failure path in the program does, so a missing font costs the splash and never the boot.

The three Python scripts beside the C file run on a development host, never in the build
(`genbanner.py` needs Pillow, and `genbackdrop.py` Pillow and NumPy): `genlogo.py` writes the terminal banner `fs/usr/share/kdos/logo.txt` from `penguin.h`,
`genbanner.py` restamps the two caption lines of the boot menu's banner,
`fs/usr/share/kdos/boot/kdos-banner.png`, and `genbackdrop.py` draws the menu's backdrop,
`fs/usr/share/kdos/boot/kdos-backdrop.png`, from that banner. Their output is committed, so changing the mascot or a
caption means running the script and committing what it writes.
