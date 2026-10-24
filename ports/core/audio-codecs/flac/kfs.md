## About
[FLAC]{.idx} is the Free Lossless Audio Codec: the libraries `libFLAC` and `libFLAC++` and the
`flac` and `metaflac` commands. Programs read and write `.flac` files through it, among them
[libsndfile](#port-libsndfile), [gst-plugins-good](#port-gst-plugins-good),
[sdl2-mixer](#port-sdl2-mixer), [audacity](#port-audacity) and [chromium](#port-chromium).

It is one of the hot libraries the tree builds with `-O3` and link-time optimisation (see
[Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port)).

## Flags
- `${CFLAGS/-O2/-O3}` — raise the exported `CFLAGS` and `CXXFLAGS` from `-O2` to `-O3`. flac's
  `configure` puts its own `-O3 -funroll-loops` ahead of the caller's `CFLAGS`, so the tree's
  `-O2` would otherwise win.
- `-flto=auto` — compile and link with link-time optimisation, in `CFLAGS`, `CXXFLAGS` and
  `LDFLAGS`.
- `-frandom-seed=flac` — fix the seed GCC names each object's `.gnu.lto_*` sections with, so the
  objects are the same bytes on every build.
- `AR=gcc-ar` — with `NM=gcc-nm` and `RANLIB=gcc-ranlib`: index archives through GCC's LTO plugin.
  flac links its programs against internal static archives (`libgrabbag`, `libutf8` and the
  rest) whose objects hold LTO bytecode only. binutils' own `ar` and `nm` load no plugin here, so
  they would leave each archive's index empty and the link of `flac` would fail on every symbol in
  it (see [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port)).
- `--enable-ogg` — Ogg FLAC, through [libogg](#port-libogg). It is configure's default, named;
  configure still only warns, and builds without it, when libogg is missing.
- `--disable-version-from-git` — take the version from `configure` alone, not from the tag,
  commit hash and date git reports for a `.git` directory in the source tree. The release archive
  carries no `.git`, so the switch makes the choice explicit and changes nothing in this build.
- `--disable-doxygen-docs` — with `--disable-examples`: no API reference built with Doxygen and no
  example programs.
