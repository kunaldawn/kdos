## About

[SoX]{.idx}, Sound eXchange, converts audio between formats and runs it through a chain of
effects — resampling, filtering, mixing, trimming, spectrograms — from the command line, as
`sox`, `play`, `rec` and `soxi`. Its library, `libsox`, is the same code for programs; in KDOS,
`k3b` and `mlt` link it.

## Flags

- `-Wno-implicit-function-declaration`, with `-Wno-implicit-int`, `-Wno-int-conversion`,
  `-Wno-incompatible-pointer-types` and `-Wno-return-mismatch` added to `CFLAGS` — Silence the
  diagnostics GCC 14 and later report as errors. The 14.4.2 sources predate that change, and the
  flags let them compile as released. See
  [Newer-compiler diagnostics as errors](../../../../docs/kdos/05-developer/build-troubleshooting.md#newer-compiler-diagnostics-as-errors).
- `--with-distro=KDOS` — The distribution name kept in libsox's version information. `sox`
  prints it on an `issue:` line of its version report at verbosity 4 or more, as in
  `sox -V4 --version`.
- `--with-libltdl` — Load external code through libtool's `libltdl`, which is why `libtool` is a
  dependency. The LADSPA effect needs it, and it is the one switch here that cannot fail loudly:
  when `configure` does not find `ltdl.h` and `libltdl`, it turns libltdl off without an error,
  and the LADSPA effect goes with it.
- `--with-ladspa` — Build the `ladspa` effect, which runs LADSPA plug-ins inside a sox effects
  chain. sox carries its own copy of `ladspa.h` and opens plug-ins at run time, so the `ladspa`
  port is not a build dependency; the plug-ins it installs are what the effect finds.
- `--with-magic`, `--with-png`, `--with-mad`, `--with-id3tag`, `--with-lame`, `--with-twolame=yes`
  — Each enables one library: `libmagic` (from `file`) to recognise a file's type by its
  contents, `libpng` for the `spectrogram` effect, `libmad` to decode MP3, `libid3tag` to read MP3
  tags, LAME to encode MP3 and TwoLAME to encode MP2.
  Named, with or without `=yes`, each makes a missing library a configure error rather than a
  feature dropped without a word.
- `--with-mp3=yes`, `--with-oggvorbis=yes`, `--with-opus=yes`,
  `--with-flac=yes`, `--with-wavpack=yes`, `--with-gsm=yes`, `--with-sndfile=yes` — The formats
  sox's `AC_OPTIONAL_FORMAT` macro handles. `yes` links each into `libsox` (where `dyn` would load
  it at run time) and makes a missing library a configure error. GSM is always available, since
  sox carries its own copy; the `gsm` port, found by its `gsm/gsm.h`, makes it use the system
  `libgsm` instead.
- `--with-alsa=yes` — Build the `alsa` audio device, through which `play` and `rec` reach sound.
  On KDOS the default ALSA device is PipeWire; see
  [Reaching the graph from a plain ALSA program on the host](../../../../docs/kdos/03-architecture/session.md#reaching-the-graph-from-a-plain-alsa-program-on-the-host).
- `--without-amrwb`, `--without-amrnb` — Leave out the AMR wide-band and narrow-band speech
  formats, which need opencore-amr; the tree has no port of it.
- `--without-ao`, `--without-pulseaudio` — Leave out the libao and PulseAudio output devices.
  Both libraries are ports, but PipeWire's ALSA device is where every program on the system
  reaches the card, so a PulseAudio device would be a second path to the same place.
- `--without-sndio`, `--without-oss`, `--without-sunaudio`, `--without-coreaudio`,
  `--without-waveaudio` — The audio interfaces of OpenBSD, the Open Sound System, Solaris, macOS
  and Windows.

## Patches

- `musl-rewind-pipe.patch` — Replaces the `#error FIX NEEDED HERE` in `src/formats.c` with the `NO_REWIND_PIPE` fallback. sox rewinds a pipe after sniffing its header by reaching into the C library's `FILE` buffer, and the source knows only glibc's and a few other layouts; musl's `FILE` is opaque.

## Notes

- **A pipe needs its type spelled out.** With `NO_REWIND_PIPE`, sox does not detect the format of
  standard input: `cat x.wav | sox --info -` fails, and `cat x.wav | sox -t wav - …` works.
