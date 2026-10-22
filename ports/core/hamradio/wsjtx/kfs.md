## About

WSJT-X is the weak-signal digital-mode program for amateur radio: FT8, FT4, JT65, JT9, Q65, MSK144,
FST4 and WSPR, for HF contacts, meteor scatter and moonbounce. It is built against Qt 5 with
Fortran decoders, and it controls the radio through [hamlib](#port-hamlib). The user guide is
built as HTML and opened from the Help menu.

## Flags

- `FFLAGS="$FFLAGS -ftrampoline-impl=heap"` — Put the trampolines gfortran makes for internal
  procedures passed as arguments in heap memory instead of on the stack. On the stack they need an
  executable stack, which the package lint refuses in `jt9`; the `readelf` check at the end of
  `build.sh` fails the build if `jt9` still asks for one (see
  [What the package lint refuses](../../../../docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
- `-DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG -Wno-error"` — Upstream appends `-Werror` to
  `CMAKE_CXX_FLAGS` after the environment's `CXXFLAGS`, and GCC warns that members of the HRD
  transceiver's message struct may be used uninitialised; upstream demotes that warning on Windows
  only. The release flags come last on the compiler
  line, so `-Wno-error` there turns the warnings back into warnings (see
  [An upstream -Werror](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-upstream--werror)).
- `-DCMAKE_POLICY_DEFAULT_CMP0167=NEW` — Find Boost through the config file
  [boost](#port-boost) installs, instead of CMake's retired FindBoost module.
- `-DWSJT_GENERATE_DOCS=ON` — Build the user guide as Asciidoctor HTML. It is upstream's default.
- `-DWSJT_SKIP_MANPAGES=OFF` — Build the manual pages, through `a2x` and DocBook, which is what
  [asciidoc](#port-asciidoc), [docbook-xsl](#port-docbook-xsl) and [libxslt](#port-libxslt) are
  in `depends` for.
- `-DWSJT_BUILD_UTILS=OFF` — Leave out the simulators and code demonstrators, which would install
  generic names such as `cablog`, `echosim` and `ft8sim` into `/usr/bin`. The manual pages are
  generated whatever this switch says, so the loop after the install deletes the pages of
  utilities that are not built (of upstream's pages, `fst4sim.1` and `jt65code.1`).
- `-DUPDATE_TRANSLATIONS=OFF` — Do not regenerate the translation sources; it is upstream's
  default.

## Notes

Upstream copies hamlib's `rigctl`, `rigctld` and `rigctlcom` into the package under `-wsjtx`
names. `build.sh` replaces each copy with a symbolic link to the program the hamlib port owns, so
there is one binary of each.

Upstream's two desktop entries carry no window class and name their icon under `pixmaps`.
`build.sh` replaces both, installs the icon into hicolor, and sets `StartupWMClass` to the
executable's name, which is Qt's Wayland app id when no organisation domain is set. The HTML user
guide in `/usr/share/doc/wsjtx` is exempt from the manual prune, because the Help menu opens it
(see [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
