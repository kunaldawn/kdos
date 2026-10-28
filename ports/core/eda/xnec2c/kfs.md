## About

xnec2c models antennas with the NEC2 method of moments: it reads a `.nec` description of the
wires and patches, computes currents, radiation patterns, impedance, gain and SWR over a frequency
sweep, and draws them in a GTK 3 window.

## Flags

- `-g0` — Appended to `CFLAGS` to cancel the `-O2 -g` that `src/Makefile.am` puts in its own
  `xnec2c_CFLAGS`. automake places a target's own flags ahead of `CFLAGS` on the compile line, so
  the last `-g` option wins and the program carries no DWARF (see
  [What kpkg does around build.sh](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).

## Notes

The release archive carries `configure.ac` and `autogen.sh` but no `configure` script, so
`build.sh` runs `autoreconf -fi` first. That needs [autoconf](#port-autoconf),
[automake](#port-automake), [libtool](#port-libtool) and, for the gettext macros, `autopoint` from
[gettext](#port-gettext).

The linear-algebra backend, [openblas](#port-openblas), is not linked: xnec2c `dlopen`s it when
the user chooses it in the program. It is in `depends` so that the choice works.

`build.sh` writes its own menu entry over upstream's, which has no `StartupWMClass`. It claims
`application/x-nec2`, the type `files/x-nec2.xml` defines; `make install` puts that file in
`/usr/share/mime/packages`, and the last line of `build.sh` checks that it did. `make install`
installs the icon as one 256×256 PNG and the SVG in `/usr/share/pixmaps`; `build.sh` rasterises
the SVG to PNG at 48, 64 and 128 pixels in the `hicolor` theme.
