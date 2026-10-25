## About
[mpg123]{.idx} decodes MPEG audio, MP3 above all: `libmpg123` is the decoder, `libout123` plays
audio through an output module, and `libsyn123` generates and converts sample formats, with the
`mpg123` player, the `out123` command and the `mpg123-id3dump` and `mpg123-strip` tools on top. [lame](#port-lame) decodes through it, and
[libsndfile](#port-libsndfile), [gst-plugins-good](#port-gst-plugins-good),
[sdl2-mixer](#port-sdl2-mixer), [mpd](#port-mpd) and [vlc](#port-vlc) read MP3 with it.

## Flags
- `--enable-components` — build the three libraries and the programs, `mpg123`, `out123`,
  `mpg123-id3dump` and `mpg123-strip`, with their default components; configure's default, named.
- `--enable-modules` — build the audio outputs as modules that `libout123` loads at run time, the
  default for a shared-library build, named.
- `--with-audio=alsa` — build only the ALSA output module, against [alsa-lib](#port-alsa-lib), the
  one audio library in `depends`. The PulseAudio and PortAudio outputs would need
  [libpulse](#port-libpulse) or [portaudio](#port-portaudio), and each of those depends on
  [libsndfile](#port-libsndfile), directly or through [pipewire](#port-pipewire), which depends on
  this port.
  ALSA's default device reaches the session's PipeWire (see
  [Reaching the graph from a plain ALSA program on the host](../../../../docs/kdos/03-architecture/session.md#reaching-the-graph-from-a-plain-alsa-program-on-the-host)).
- `--with-default-audio=alsa` — the modules to try at run time: ALSA alone, so no module the
  package does not carry is probed. The package carries `output_alsa.so` and the silent
  `output_dummy.so`.
- `--with-network=exec` — streams over HTTPS go through an `exec` of `curl` or `wget`; plain HTTP,
  as Shoutcast uses, goes through mpg123's internal code, which is always built.
- `--enable-ipv6` — IPv6 support in that internal network code.
