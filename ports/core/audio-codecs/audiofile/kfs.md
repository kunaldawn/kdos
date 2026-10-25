## About

The [Audio File Library]{.idx} reads and writes AIFF, AIFF-C, WAVE, NeXT/Sun, IRCAM, AVR, CAF and
FLAC files through one C API, `libaudiofile`, and installs the `sfconvert` and `sfinfo` commands.
The patches beside it are Debian's series for this release: overflow, division-by-zero and
out-of-bounds checks in the decoders and in `sfconvert` and `sfinfo`, and one change that lets it
compile with GCC 6 and later. In KDOS, `libspectrum` and `kwave` link it.

## Flags

- `--enable-flac` — Build the FLAC reader and writer against `libFLAC`, which is why `flac` is a
  dependency. It is upstream's default, and it does not make a missing `libFLAC` fatal: when
  `pkg-config` cannot find `flac >= 1.2.1`, configure turns FLAC off without an error.
- `--disable-examples` — Leave out the platform playback examples. On Linux these are `linuxtest`,
  which opens the OSS device through `<linux/soundcard.h>`, and `alsaplay`, which links ALSA, and
  neither is installed; `alsa-lib` is not a dependency of this port.
- `--disable-werror` — Do not add `-Werror` to the compile. This is upstream's default as well, so
  the switch only states the choice.
- `--disable-coverage`, `--disable-valgrind` — The `gcov` instrumentation and the Valgrind run of
  the test suite, both off by default upstream and both off here.

## Patches

- `01_gcc6.patch` — Writes the shifted minimum values in `SimpleModule.h` and three tests as `0-(1U<<n)` instead of `-1 << n`, which is undefined for a signed left shift, and declares the NeXT test data as `signed char`, so the source compiles with GCC 6 and later.
- `03_CVE-2015-7747.patch` — Passes the output format, not the input format, to the channel-matrix module when a conversion changes both the sample format and the channel count, which overflowed a buffer (CVE-2015-7747), and adds the `sixteen-stereo-to-eight-mono` test for it.

## Notes

- **None of the patches is optional.** The library parses whatever file an editor or a converter
  is handed, and most of them are bounds and overflow checks in its decoders.
- **The patches are a list, applied in Debian's order.** `build.sh` names each one in a `for`
  loop; a glob would sort `0013` before `01`, and a patch file dropped into the directory without
  a line in that loop is never applied.
- **`autoreconf -fi` runs before `configure`.** `03_CVE-2015-7747.patch` adds a test to
  `test/Makefile.am`, which makes the release's generated `Makefile.in` stale; without
  regenerating up front, `make` would call the automake version the tarball was made with.
- The manual pages ship prebuilt in the tarball and are installed as they are, so the
  documentation tools are not dependencies.
