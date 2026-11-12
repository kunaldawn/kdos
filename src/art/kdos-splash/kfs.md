## About
`kdos-splash` is the boot animation, drawn straight to `/dev/fb0`: the KDOS mark fading in from
black, or with `kdos.splash=phosphor` on the kernel command line Phosphor mode's CRT power-on,
then the list of boot stages under a progress bar, until `rcS` hands the screen to the display
manager. One process covers the whole boot. The initramfs starts it, it survives `switch_root`,
and later stages reach it through the FIFO `/dev/.kdos-splash`. What it draws, the commands it
takes and the three facts about the framebuffer that constrain it are in
[The splash](../../../docs/kdos/03-architecture/boot-and-init.md#the-splash).

The build compiles one C file and installs one font. `build.sh` compiles `kdos-splash.c` to a
static binary and decompresses Terminus' 16-pixel console font from
[terminus-font](#port-terminus-font) to `/usr/share/kdos/splash.psf`, the file the program loads
its glyphs from. The image step
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
Two commands end the splash, and both return only once it has stopped drawing. `quit` leaves the
console in text mode with its cursor shown, for a text prompt next: the initramfs's rescue shell
and a single-user boot's `sulogin`. `handoff`, which `rcS` sends on every other boot, also puts
tty1 into graphics mode with its text cleared and its cursor hidden, so nothing is drawn between
the splash's last frame and the greeter compositor's first; `kdos-getty` puts the VT back into
text mode when it has to show a prompt there instead. The client sets the VT's mode, not the
running splash, because the splash was started in the initramfs and every path it opens after
`switch_root` resolves in the emptied initramfs root.

The motion comes from the kernel command line because the power-on runs in the initramfs, before
any root filesystem with a setting in it is mounted. `KDOS_SPLASH_CMDLINE` names a file read in
its place, and `kdos-splash preview` takes the motion as an optional last argument, so both
motions can be rendered and compared on a development host. The same command line decides whether
the splash runs at all: `run` without `quiet` exits at once, so the verbose and single-user entries
show the boot's text instead.

The progress bar gives the initramfs a fixed first share and `rcS` the rest. The splash finds the
boundary from its own root: `/init` is there while the initramfs runs and gone once
`switch_root` has emptied it, so the first `total` that arrives with `/init` missing is `rcS`'s.
`preview` replays a whole boot through the same commands, its failed stage and the stages after it
included, so one render shows the bar, the pinned failure and the reason panel together.

`splash.psf` is decompressed at build time because `font_load()` reads a bare PSF1 or PSF2 file:
the static binary carries no decompressor. It falls back to
`/usr/share/consolefonts/ter-v16n.psf` and then exits without drawing, which is what every
failure path in the program does, so a missing font costs the splash and never the boot.

The three Python scripts beside the C file run on a development host, never in the build
(`genbanner.py` needs Pillow, and `genbackdrop.py` Pillow and NumPy): `genlogo.py` writes the
terminal banner `fs/usr/share/kdos/logo.txt` from `penguin.h`, `genbanner.py` restamps the two
caption lines of the boot menu's banner, `fs/usr/share/kdos/boot/kdos-banner.png`, and
`genbackdrop.py` draws the menu's backdrop, `fs/usr/share/kdos/boot/kdos-backdrop.png`, from that
banner. Their output is committed, so changing the mascot or a caption means running the script
and committing what it writes.
