## About

mpd, the Music Player Daemon, is the playing half of a music player: it holds the library and the
queue and drives the sound card, and separate client programs such as [rmpc](#port-rmpc) attach
to its socket to control it. The port ships no configuration file. The login session starts mpd
only when a configuration exists in one of the places mpd itself reads, and then starts the
panel's now-playing watcher beside it; the media keys reach it through `kdos-mpctl`, described in
[kdos-mpctl](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/kdos-command.md#kdos-mpctl).

Every feature option is named on or off. A meson feature option left at its default is `auto`, which
builds the plugin when its library happens to be in the build root, so naming each one keeps the
plugin set the same on every build and turns a missing library into a failed configure.

## Flags

- `-Dpipewire=enabled` — The PipeWire output plugin, the sound server of the session.
- `-Dalsa=enabled` — The ALSA output plugin, for a configuration that writes to a card directly.
- `-Djack=enabled` — The JACK output plugin. No JACK port exists; it builds against the
  `libjack` and `jack.pc` that [pipewire](#port-pipewire) installs with `-Dpipewire-jack=enabled`
  and `-Djack-devel=true`, so its output goes to PipeWire's JACK graph.
- `-Dpulse=disabled` — The PulseAudio output plugin. PulseAudio is not a port; PipeWire is reached
  through its own plugin.
- `-Dsndio=disabled`, `-Doss=disabled`, `-Dsolaris_output=disabled` — Output plugins for sndio,
  which is not a port, for OSS, and for Solaris `/dev/audio`.
- `-Dao=disabled`, `-Dopenal=disabled` — The libao and OpenAL output plugins. Both libraries are
  ports ([libao](#port-libao), [openal-soft](#port-openal-soft)) but neither is in `depends`.
- `-Dshout=disabled` — The Icecast/Shoutcast streaming output, whose libshout is not a port.
- `-Dhttpd=false`, `-Dsnapcast=false` — The HTTP streaming and Snapcast outputs, two boolean
  options that upstream builds by default. Off, the daemon opens no streaming listener of its own.
- `-Dnlohmann_json=disabled` — The JSON library; with it off the Qobuz client, which needs it,
  cannot be built either.
- `-Dffmpeg=enabled` — The FFmpeg decoder plugin, which plays every format FFmpeg reads, including
  the AAC, WavPack and Musepack files the dedicated decoders below would otherwise cover.
- `-Dflac=enabled`, `-Dvorbis=enabled`, `-Dopus=enabled`, `-Dsndfile=enabled`, `-Dmpg123=enabled`,
  `-Did3tag=enabled`, `-Dmikmod=enabled` — Decoders and the ID3 tag reader, each built against its
  port in `depends`.
- `-Dfluidsynth=enabled` — The MIDI decoder, through [fluidsynth](#port-fluidsynth). Its default
  soundfont is `/usr/share/sounds/sf2/FluidR3_GM.sf2`, which [fluidr3-gm-sf3](#port-fluidr3-gm-sf3)
  provides as a link, so a MIDI file plays with no `soundfont` line in `mpd.conf`.
- `-Dmad=disabled`, `-Dfaad=disabled`, `-Dwavpack=disabled`, `-Dadplug=disabled`,
  `-Daudiofile=disabled`, `-Dgme=disabled`, `-Dmodplug=disabled`, `-Dopenmpt=disabled`,
  `-Dsidplay=disabled` — Decoders whose libraries are ports but not in `depends`
  ([libmad](#port-libmad), [faad2](#port-faad2), [wavpack](#port-wavpack), [adplug](#port-adplug),
  [audiofile](#port-audiofile), [libgme](#port-libgme), [libmodplug](#port-libmodplug),
  [libopenmpt](#port-libopenmpt), [libsidplayfp](#port-libsidplayfp)).
- `-Dmpcdec=disabled`, `-Dtremor=disabled`, `-Dwildmidi=disabled` — Decoders for Musepack,
  fixed-point Vorbis and WildMIDI, whose libraries are not ports.
- `-Dchromaprint=enabled` — AcoustID fingerprinting, through [chromaprint](#port-chromaprint).
- `-Dlame=enabled`, `-Dvorbisenc=enabled`, `-Dtwolame=enabled` — The MP3, Vorbis and MP2 encoders,
  which the recorder output uses to write what is playing to a file.
- `-Dshine=disabled` — The shine fixed-point MP3 encoder, which is not a port.
- `-Dsoxr=enabled`, `-Dlibsamplerate=enabled` — Both resamplers.
- `-Dsqlite=enabled` — The sticker database, which stores the name/value pairs clients attach to songs.
- `-Dicu=enabled` — Unicode collation, case folding and character-set conversion through
  [icu](#port-icu).
- `-Diconv=disabled` — The `iconv()` conversion path. mpd's build reads this option only when ICU
  is absent, so with ICU enabled it has no effect.
- `-Dpcre=enabled` — Regular expressions in filter expressions, through [pcre2](#port-pcre2).
- `-Dbzip2=disabled`, `-Dzzip=disabled` — The bzip2 and ZIP archive plugins, which read music from
  inside an archive. Both libraries are ports but not in `depends`.
- `-Diso9660=enabled`, `-Dcdio_paranoia=enabled` — Reading music from ISO 9660 images and audio
  CDs, through [libcdio](#port-libcdio) and [libcdio-paranoia](#port-libcdio-paranoia).
- `-Dcurl=enabled` — The HTTP input plugin, for internet radio and remote files.
- `-Dmms=disabled`, `-Dupnp=disabled`, `-Dlibmpdclient=disabled` — MMS streams, the UPnP
  database and the proxy database; libmms, pupnp, npupnp and libmpdclient are not ports.
- `-Dnfs=disabled`, `-Dsmbclient=disabled`, `-Dwebdav=disabled` — NFS, SMB and WebDAV storage.
  [libnfs](#port-libnfs) and [samba](#port-samba) are ports but not in `depends`; upstream itself
  leaves `smbclient` off by default and documents in `meson_options.txt` that libsmbclient crashes
  mpd.
- `-Dqobuz=disabled` — The Qobuz streaming client.
- `-Dudisks=disabled`, `-Ddbus=disabled` — Removable media through udisks2, and the D-Bus support
  that needs. The daemon talks to no bus.
- `-Dzeroconf=disabled` — Announcing the daemon on the network over Avahi or Bonjour.
- `-Dio_uring=enabled` — Read files through io_uring, with [liburing](#port-liburing).
- `-Dsyslog=enabled` — Logging to syslog, chosen by `log_file "syslog"` in `mpd.conf` and taken
  when the file names no `log_file` at all.
- `-Dsystemd=disabled` — No systemd unit files and no socket activation; KDOS has no systemd
  ([principles](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).
- `-Ddocumentation=enabled` — Build the documentation with Sphinx, from
  [python3-sphinx](#port-python3-sphinx) in `depends`. As `enabled` rather than `auto`, a missing
  `sphinx-build` fails the configure instead of silently dropping the manual pages.
- `-Dhtml_manual=false` — Skip the HTML manual Sphinx would install under `/usr/share/doc/mpd`.
- `-Dmanpages=true` — Build and install `mpd.1` and `mpd.conf.5`.
