## About

aubio is a library for [audio labelling]{.idx}: it detects onsets, pitch, beats and tempo, and
extracts features such as MFCCs, from a sound file or a live stream. It installs `libaubio` and
the command-line tools built on it (`aubioonset`, `aubiopitch`, `aubiotrack`, `aubionotes`,
`aubioquiet` and `aubiomfcc`). In KDOS, `ardour` links it.

## Flags

- `WAFDIR`, `WAF` — The build runs a separate release of waf, the second `source =` in the
  recipe, instead of the copy in the tarball. The bundled waf imports Python's `imp` module,
  which Python 3.12 removed. `WAFDIR` points `waf-light` at its own `waflib`; without it, waf
  would import the bundled one from the current directory.
- `-g0` — The `wscript` prepends `-g` to `CFLAGS` in every GCC build, release included; `-g0`, last on the line,
  takes it back, so the objects carry no debug information.
- `--disable-jack` — Leave out the JACK input and output of the tools, so they read and write
  files only. JACK's library on KDOS is PipeWire's, and every port that can link it names it on
  or off rather than leaving it to build order; see
  [JACK programs on the host](../../../../docs/kdos/03-architecture/session.md#jack-programs-on-the-host).
- `--disable-avcodec` — Leave out the FFmpeg-based file reader. It is written against an older
  FFmpeg API than the `ffmpeg` port provides, and libsndfile reads the formats the tools take.
- `--enable-fftw3f` — Compute FFTs with FFTW's single-precision library, which matches aubio's
  float samples, instead of the bundled Ooura FFT.
- `--enable-sndfile`, `--enable-samplerate` — Read and write sound files through libsndfile, and
  resample through libsamplerate. Given explicitly, a missing library stops the configure step
  instead of being skipped.
- `--disable-docs` — The manual pages need `txt2man`, which is not a port.
- `--disable-tests` — Do not build the test programs, which are on by default, or run them
  during the build.
- `--enable-examples` — Build the command-line tools, which aubio's build calls examples.

## Patches

- `waf-2.1.0-compat.patch` — Writes the `--with-target-platform` option in `wscript` with `type=str` instead of `type='string'`. waf from 2.1.0 on parses options with `argparse`, which takes a type, not a type name; it is the one change the `wscript` needs to run under the newer waf.

## Notes

- **The switches are passed three times.** waf reads the feature switches again at build and at
  install time, and a switch given only to `configure` reverts to its default there; that is why
  `$OPTS` is on all three lines. Dropped from the build line, `--disable-tests` would be lost and
  the tests would build.
