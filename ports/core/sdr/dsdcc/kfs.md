## About

[DSDcc]{.idx} is a C++ library and command-line decoder, `dsdccx`, for the digital voice modes
D-STAR, DMR, dPMR, NXDN and YSF. It is built with [mbelib](#port-mbelib) for the AMBE and IMBE
voice codecs and links [serialdv](#port-serialdv) to drive an AMBE hardware dongle.

## Notes

The source's only licence file is `debian/copyright`, which states the GPL-3.0-or-later terms and
points at a Debian path for the full text. `build.sh` installs it as `copyright` and, as
`GPL-3.0`, the full GPL version 3 that serialdv, a dependency under the same licence, installs in
`/usr/share/licenses/serialdv/LICENSE`, so the package carries a complete text.

`build.sh` ends by checking that `/usr/bin/dsdccx` is executable, that `libdsdcc.pc` is in the
package and that `dsdccx` links libserialdv.
