## About

`kdos-kwin-motion` is the motion of Phosphor mode: three KWin effects written in JavaScript, each a
KWin/Effect package under `/usr/share/kwin/effects/`, that move windows the way the KDOS boot splash
moves the screen.

- `kdos_unfold` opens a window out of a hairline across its middle and closes it by folding it to
  a line and pinching the line to a dot. It sits in the window open/close category beside Scale,
  Fade and Glide, and takes each window's open and close grabs, so whichever of those is left on
  steps aside instead of animating the same window twice.
- `kdos_login` replaces KWin's `login` effect: when KSplash's window closes, the desktop opens out
  of a line across the splash, with a hot seam on the edge of the opening, instead of a fade.
- `kdos_quit` replaces KWin's `sessionquit` effect: while the session quits, each window it closes
  folds away, and the desktop window is held on screen until KWin exits.

All three are off by default and are switched on and off together by `kdos phosphor`, which also
turns the stock `login` and `sessionquit` off while Phosphor mode is on. The collapse of the whole
screen at lock and logout is not here: a JavaScript effect draws only windows, so that belongs to
the `kdos_crt` effect that [kwin](#port-kwin) carries.

The port is KDOS's own data with no upstream source. `build.sh` configures `CMakeLists.txt` with
extra-cmake-modules and KPackage, which installs each package directory and copies the one fold
shader into both effects that use it.

## Notes

The effects have no tie to KWin's C++ interface, so a KWin release does not rebuild or break them.
What they rely on is the scripted-effect interface: `addFragmentShader()`, `ShaderUniform`
animations, the open and close grab roles, and the uniforms KWin sets on a window shader
(`modulation`, `saturation`, the colour-management block). A change there shows as a script error
in KWin's log and a window that opens without the motion, never as a window that fails to open.

Every duration goes through `animationTime()`, so `AnimationDurationFactor` in `kdeglobals` scales
it and `0` turns the motion off; the desktop hold in `kdos_quit` is not motion and keeps its 30
seconds. KWin gives a scripted effect `animationsSupported()` as its `supported()`, which is false
when KWin's EGL device reports itself as software. A software device that does not say so, as
Mesa's `kms_swrast` may not, would load them, so `kdos phosphor on` switches them on only after
`kdos_crt`, which also refuses llvmpipe, softpipe and Software Rasterizer by the renderer's name,
has loaded: they are seen only with GPU rendering, `make run-hw` or the rig's `--gl`.

Each shader works in KWin's colour-managed space, decoding the window to nits and encoding it again
for the output, so the beam's light is the output's own white and an HDR or night-light output
draws it correctly.
