## About

PortAudio is a portable C library for real-time audio input and output, with a C++ binding.
Programs that use it, such as Audacity, reach the sound card through one of its host APIs; this
build has two, ALSA and JACK. PipeWire serves both: ALSA through PipeWire's ALSA plugin, and JACK
through the `libjack` that [pipewire](#port-pipewire) installs, which is why the port is listed in
`42_graphics` after `pipewire` and not in `41_system`. Audacity, Tenacity, LMMS, GNU Radio, Octave
and fldigi are among the ports that link it.

## Flags

- `--with-alsa` — The ALSA host API, against [alsa-lib](#port-alsa-lib).
- `--with-jack` — The JACK host API, against PipeWire's JACK library. configure leaves the host API
  out with no error when it finds no `jack` pkg-config module, so `build.sh` checks that the
  generated `Makefile` compiles `src/hostapi/jack/pa_jack.o` and stops otherwise.
- `--without-oss`, `--without-asihpi` — The OSS host API and the one for AudioScience HPI sound
  cards. Both are detected by default; naming them keeps them out.
- `--enable-cxx` — Build the C++ binding, `libportaudiocpp`.
- `-j1` — Build and install serially. The top `Makefile`'s `all` names the library and the
  recursive build of `bindings/cpp` as independent prerequisites, so under a parallel make the C++
  binding can link against `lib/libportaudio.la` before the library is written.
