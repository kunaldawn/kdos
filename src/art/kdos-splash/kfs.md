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
Two commands end the splash, and both return only once it has stopped drawing. While it draws,
the splash holds the foreground VT in `KD_GRAPHICS`, so no console text lands on the picture.
`quit` puts it back in text mode with its cursor shown, for a text prompt next: the initramfs's
rescue shell and a single-user boot's `sulogin`; the VT then redraws the boot messages it
buffered. The `quit` client does the same after the splash has gone, but only on a VT in
`VT_AUTO` mode, so a splash killed without its outro does not leave the rescue shell or getty
drawing nothing. `handoff`, which `rcS` sends on every other boot, runs no power-off: it draws
the settled frame once more and leaves it on the framebuffer, with tty1 in graphics mode, so that
frame stays up until the greeter compositor's first mode set and KSplash's bar carries on from
its hold; `kdos-getty` puts the VT back into text mode when it has to show a prompt there
instead. The splash then pans the framebuffer every 100 ms. Once a pan is refused with `EBUSY`
(a DRM master holds the device, which after the hand-off only a compositor is), it opens the DRM
card (`dri/card0` to `dri/card3`, the first with a lit CRTC), asks `DROP_MASTER` so a stray
`EBUSY` leaves no master behind, and records the framebuffer each lit CRTC scans out. When one
scans out another, the compositor's first frame is on the display, and the splash zeroes the
framebuffer unseen and exits, so a compositor that lets go of the display later, at a login or a
logout, uncovers black. The blank has to land before that hand-back, since the kernel shows
fbdev's buffer again in the instant the compositor drops master. The `EBUSY` comes before the
compositor's first mode set, and fbdev's deferred-I/O flush writes into the buffer still on the
display whoever is master, so a blank on the first `EBUSY` would show black for the rest of the
compositor's start. On virtio-gpu the greeter's KWin draws its first frame about 0.6 to 0.7 s
after it (a live session's about 1.0 to 1.1 s), and the greeter's picture 2.6 to 3.4 s after it. With no card or lit CRTC to read, the splash waits 5 s
past the first `EBUSY` instead, which ends 1.6 to 2.4 s after the greeter appears; a login typed
inside that window puts tty1 back in text mode, and the splash blanks on that exit a frame after
fbdev's buffer is back. tty1's `VT_PROCESS` mode is logged and ends nothing:
the greeter's session start hangs tty1 up, the descriptor opened in the initramfs then answers
`EIO`, and the splash reopens tty1 through a descriptor on the devtmpfs `/dev` it opened before
`switch_root`. It blanks and exits after 60 s whatever it has seen. It exits without blanking
when tty1 returns to text mode before any `EBUSY` (`kdos-getty`'s prompt) and with the blank
when it returns after one (a login inside the margin), and without blanking when
`FBIOGET_VSCREENINFO` fails (a native GPU driver replaced the
firmware framebuffer, and its mode set has replaced the frame). Every way out writes one line to
the kernel log, `kdos-splash: hold ended by <frame|margin|timeout|signal|fb-gone|text> after …`, with
the times of `VT_PROCESS` and the first `EBUSY` counted from the hand-off; the self-test drives
the hold through a preloaded stand-in for `/dev` (`testing/fixtures/splash-hold/shim.c`).

A stage closed with `skip` reads `SKIP` in the tagline's colour, which holds 4.5:1 on the ground.
The tagline is text, so it holds WCAG AA, 4.5:1, on the ground in every scheme: it is the scheme's `pdark` (`KT_MID`'s slot) where that reaches the
floor, and otherwise `pdark` moved toward `primary` one percent at a time until it does (Amber and
Ice, whose `pdark` is 3.56:1 and 4.42:1). The walk aims at 4.52 because the splash measures with
the WCAG formula in floating point and libkcolor's `kcol_contrast`, which the self-test judges
with, rounds each channel through a table. The WHY detail lines are in the scheme's text colour.

A GPU driver loaded after `switch_root` (`vmwgfx`, `i915`, `amdgpu` and `nouveau` are modules
the initramfs does not carry) replaces the firmware framebuffer under the running splash, and
every call on the old `fb0` descriptor then fails. The splash checks the descriptor once a frame,
and when it has gone it opens `fb0` again through its descriptor on the devtmpfs `/dev` until the
driver's framebuffer registers, lays the picture out for that size and draws on, so the boot is
not black from the device manager to the greeter. The self-test replaces `fb0` with a larger one
under a running splash through the same stand-in for `/dev`.

The motion comes from the kernel command line because the power-on runs in the initramfs, before
any root filesystem with a setting in it is mounted. `KDOS_SPLASH_CMDLINE` names a file read in
its place, and `kdos-splash preview` takes the motion and then an accent as optional last
arguments, so both motions and every scheme can be rendered and compared on a development host.

The line under the wordmark names the release: `Kool Desktop Operating System 0.2.1`, with
`+<BUILD_ID>` on any build but a release one. It comes from a root's `/etc/kdos-release` (its
`VERSION`), or from `/etc/os-release`'s `VERSION_ID` and `BUILD_ID` when there is no record, so
the splash, `kdos version`, the boot menu, the installer and the greeter give one form. The splash
reads the initramfs's copy when it starts, which `script/phases/70_image/090_initramfs.sh` takes
from the image where `script/phases/70_image/045_identity.sh` wrote it. That copy is the release
of the image that built the initramfs, and an installed machine keeps one initramfs across every
update that is not a slot image, so the initramfs's `sp_root_release` runs
`KDOS_SPLASH_ROOT=/newroot kdos-splash release` before `switch_root`: the client reads the mounted
root's record and sends it as `R<release>`, and the running splash draws that instead. The
running splash cannot read it itself, because after `switch_root` every path it opens is in the
emptied initramfs. A root with neither file gives the name alone. A release too wide to share
the name's line within nine tenths of the screen takes its own line under the tagline, in the
tagline's colour, and the self-test holds that line to 4.5:1 too. `KDOS_SPLASH_ROOT` names a
directory whose `etc/` is read in place of `/etc`, and in `preview` `KDOS_SPLASH_NEWROOT` names
the root whose release arrives at the sample boot's `switch_root`.

KSplash's copy of the picture (`kdos-plasma-look`) draws the name without the release, and keeps
the rule two lines under the tagline. At the login the release leaves the name's line, and when it
had a line of its own the rule and the bar move up by two lines.

The same command line decides whether the splash runs at all: `run` without `quiet` exits at
once, so the verbose and single-user entries show the boot's text instead.

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
