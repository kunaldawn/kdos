## About

JetBrains Mono is the desktop's monospace face: Plasma's fixed-width font, Konsole's, Kate's
editor's and the monospace text in the panel clock, the splash and the login screen. It has a tall
x-height, and a slashed or dotted 0, an open 1 and a distinct l and I. The port installs the static
hinted TrueType files of both families: `JetBrains Mono`, which has programming ligatures, and
`JetBrains Mono NL`, the same design without them.

## Notes

The release is a zip, which `kpkg` copies into the source directory whole rather than unpacking,
so `build.sh` unpacks it from the port directory with [unzip](#port-unzip). Of what it holds, the static `fonts/ttf/`
faces are installed; the variable fonts in `fonts/variable/` and the web fonts are not. A variable
face beside the static ones registers the same family twice with fontconfig, and which file a
program then gets for a weight depends on fontconfig's match order. The static files carry
TrueType hinting for every weight, which is what a 10 pt face at 1 and 1.25 scale needs.

The faces are installed as upstream built them, under the compiled font data class of
[What is built from source](../../../../docs/kdos/05-developer/writing-ports.md#what-is-built-from-source):
upstream compiles them from the Glyphs sources in its repository with `gftools builder` and
`fontmake`, a Python toolchain that is not ported, and the release zip holds only the built faces.

The port ships no fontconfig rule. Making JetBrains Mono the first `monospace` family is the
desktop's choice, made by `/etc/fonts/conf.d/56-kdos-mono.conf` in the image, and Plasma's
`fixed` font key names it as well. `OFL.txt` sits at the top of the zip rather than of the source
directory, so `build.sh` installs it as the licence.
