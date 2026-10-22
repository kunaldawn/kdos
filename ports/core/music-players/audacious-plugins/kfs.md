## About
audacious-plugins is everything [audacious](#port-audacious) plays and shows through: the decoders,
the audio outputs, the effects, the visualisations, the playlist formats and the Qt window itself.
Without this port the player opens no window and plays nothing. It is built against the Qt half of
`libaudqt`, matching the audacious port.

## Flags
Every plugin is a meson boolean, almost all of them `true` by default. A plugin switched on whose
library is missing is still dropped, with only a line in meson's summary, so that summary is where
a missing dependency shows. The recipe names every one, so the set it asks for is explicit.

- `-Dqt=true` — with `-Dqt5=false`, `-Dgtk=false` and `-Dgtk2=false`: build the Qt 6 plugins and no
  GTK ones, matching how audacious built its support libraries. The GTK-only plugins (the GTK
  interface, the LADSPA host and the on-screen display) go with them.
- `-Dqtui=true` — with `-Dskins=true`: the Qt interface and the Winamp-skinned one.
- `-Dasx=true` — this switch and every other `true` switch after it in `build.sh`: the playlist
  formats (ASX, M3U, PLS, XSPF, cue sheets), GIO transports (FTP, SFTP, SMB, MTP), the decoders
  (raw AAC through [faad2](#port-faad2), MIDI through [fluidsynth](#port-fluidsynth), audio CD, game
  music, FFmpeg, FLAC, ModPlug, mpg123, Opus, PlayStation and Nintendo DS sound, libsndfile,
  Vorbis, Vortex, WavPack, SID, OpenMPT and AdPlug), the ALSA and PipeWire outputs, the file
  writer with its FLAC, MP3 and Ogg encoders, the general plugins (album art, MPRIS 2,
  notifications, the status icon, the playlist manager, search and the rest), every effect,
  [libbs2b](#port-libbs2b)'s crossfeed, LIRC remotes, and the four visualisations.
- `-Dsid=true` — the SID plugin needs [libsidplayfp](#port-libsidplayfp) built against
  [libresidfp](#port-libresidfp); without it meson drops the plugin with only a warning.
- `-Dpipewire=true` — PipeWire is the output, with `-Dalsa=true` as the fallback. `-Dpulse=false`,
  `-Dsdlout=false`, `-Dqtaudio=false`, `-Doss=false` and `-Dsndio=false` leave out further routes
  to the same sound server, `-Djack=false` the JACK output, and `-Dcoreaudio=false` and
  `-Dmac-now-playing=false` the macOS plugins.
- `-Dscrobbler2=false` — with `-Dlyrics=false`, `-Dstreamtuner=false`, `-Dampache=false`,
  `-Dcdaudio-cddb=false`, `-Dneon=false` and `-Dmms=false`: plugins that work only online
  (Last.fm scrobbling, lyrics sites, radio directories, Ampache, CDDB lookups) and the HTTP and MMS
  stream transports.
- `-Dhotkey=false` — no global hotkey plugin. It grabs keys on an X server, which a Wayland session
  never hands it.
- `-Dladspa=false` — with `-Daosd=false` and `-Dgtkui=false`: the GTK-only plugins, named off.
