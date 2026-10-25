## About
[cmus]{.idx} is a music player for the terminal, with a library view, playlists and a play queue,
driven entirely from the keyboard. It decodes through one input plugin per format and plays
through one output plugin, all chosen when it is configured.

## Flags
cmus's `configure` takes `CONFIG_<NAME>=y` or `=n` for every plugin. Most of them default to
`auto`, built whenever the library happens to be found; with `y`, a library that is not found stops
the configure. The recipe names every one, so a missing codec port stops the build instead of
leaving that format out.

- `CONFIG_ALSA=y` — the one output plugin. Sound on KDOS reaches PipeWire through ALSA's default
  device, so the ALSA plugin is the whole route.
- `CONFIG_FLAC=y` — with `CONFIG_VORBIS`, `CONFIG_OPUS`, `CONFIG_MAD`, `CONFIG_WAV`, `CONFIG_CUE`,
  `CONFIG_AAC`, `CONFIG_WAVPACK` and `CONFIG_MIKMOD`: the dedicated input plugins, on
  [flac](#port-flac), [libvorbis](#port-libvorbis), [opusfile](#port-opusfile),
  [libmad](#port-libmad) for MP3, [faad2](#port-faad2) for raw ADTS AAC, [wavpack](#port-wavpack)
  and [libmikmod](#port-libmikmod) for tracker modules, plus WAV files and cue sheets.
- `CONFIG_FFMPEG=y` — the catch-all input through [ffmpeg](#port-ffmpeg). It plays what no dedicated
  plugin does, such as `.m4a` (AAC or ALAC in an MP4 container), `.wma`, `.ape` and `.shn`. cmus
  ranks its dedicated plugins above it, so MP3 still goes through libmad. See `ffmpeg8.patch`.
- `CONFIG_MPRIS=y` — publish the player on the session bus over MPRIS, for media keys and the
  panel. configure looks for the sd-bus API in libsystemd, then libelogind, then
  [basu](#port-basu); KDOS has neither of the first two, so it is basu.
- `CONFIG_CDIO=y` — play audio CDs, read through `libcdio_cdda` from
  [libcdio-paranoia](#port-libcdio-paranoia), on [libcdio](#port-libcdio).
- `CONFIG_PULSE=n` — no PulseAudio output. cmus ranks it above ALSA, so building it would send
  every cmus stream to PipeWire's PulseAudio service instead, a second route to the same server.
- `CONFIG_JACK=n` — with `CONFIG_AO`, `CONFIG_ARTS`, `CONFIG_ROAR`, `CONFIG_SNDIO`, `CONFIG_SUN`,
  `CONFIG_OSS`, `CONFIG_COREAUDIO`, `CONFIG_AAUDIO` and `CONFIG_WAVEOUT` set to `n`: the other
  output plugins, and `CONFIG_SAMPLERATE=n` leaves out the libsamplerate resampling that only the
  JACK output uses.
- `CONFIG_MP4=n` — no second `.m4a` reader: it needs mp4v2, which is not a port, and ffmpeg reads
  the container.
- `CONFIG_MPC=n` — with `CONFIG_MODPLUG`, `CONFIG_BASS`, `CONFIG_VTX` and `CONFIG_TREMOR` set to
  `n`: the remaining input plugins, for Musepack, libmodplug's modules, BASS, VTX chiptunes and the
  integer Vorbis decoder.
- `CONFIG_CDDB=n` — no CDDB lookup of a disc's track names. It needs libcddb, which is not a port,
  so an audio CD lists as numbered tracks. `CONFIG_DISCID=n` leaves out the disc identification
  through [libdiscid](#port-libdiscid) as well.

## Patches
- `ffmpeg8.patch` — in `ip/ffmpeg.c`, call `avcodec_close()` only on a libavcodec older than 57.48.101 and otherwise rely on `avcodec_free_context()`, which closes the context itself. `avcodec_close()` is absent from ffmpeg 8's libavcodec, and cmus calls it unconditionally, so the ffmpeg input plugin would not build against the tree's ffmpeg.

## Notes
The desktop entry is KDOS's own, with `Terminal=true`; upstream ships none.
