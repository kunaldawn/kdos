## About
[libsndfile]{.idx} reads and writes files of sampled sound through one API: WAV, AIFF, AU, CAF,
W64 and the other PCM containers, and FLAC, Ogg Vorbis, Opus and MP3 through the codec libraries.
Many programs on the host that open a sound file go through it, among them
[pipewire](#port-pipewire), [libpulse](#port-libpulse), [sox](#port-sox),
[speech-dispatcher](#port-speech-dispatcher), [openal-soft](#port-openal-soft),
[fluidsynth](#port-fluidsynth) and the audio editors. It also installs the `sndfile-*` commands: `sndfile-info`,
`sndfile-convert`, `sndfile-play` and the rest.

## Flags
- `-DENABLE_EXTERNAL_LIBS=ON` — FLAC, Ogg, Vorbis and Opus. Off, libsndfile compiles all four
  out, whatever is installed, and keeps only the formats and codecs built into it. It is upstream's default, named.
- `-DENABLE_MPEG=ON` — MP3, read through [mpg123](#port-mpg123) and written through
  [lame](#port-lame). It is turned on only when both are found; either one missing turns MP3 off
  in both directions. Upstream's default, named.
- `-DCMAKE_REQUIRE_FIND_PACKAGE_Ogg=ON` — and the same switch for `Vorbis`, `FLAC`, `Opus`,
  `mp3lame`, `mpg123` and `ALSA`: turn each `find_package` into a required one. libsndfile
  answers a missing codec library by building without it, so without these a library missing at
  build time gives a libsndfile narrower than its `depends` say, and nothing reports it.
  `ALSA` is what `sndfile-play` plays through; without it the program is built to write to OSS's
  `/dev/dsp`, which exists only while the kernel's OSS emulation is loaded and bypasses the sound
  server.
- `-DBUILD_PROGRAMS=ON` — build and install the `sndfile-*` commands and their manual pages,
  upstream's default, named.
- `-DBUILD_EXAMPLES=OFF` — skip the example programs, which upstream builds by default.
