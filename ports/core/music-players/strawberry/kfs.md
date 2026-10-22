## About
[Strawberry]{.idx} is a music player and collection organiser: it indexes a local music collection,
edits tags, keeps playlists, play counts and ratings, normalises loudness, and plays audio CDs and
music on MTP and iPod devices. It plays through [gstreamer](#port-gstreamer). It is the program
audio files open in on KDOS, in `/etc/xdg/kdos-mimeapps.list`.

## Flags
Strawberry declares each feature as an optional component, `ENABLE_<NAME>`, and every one is on by
default. A component switched on whose library is missing is still dropped, with only a line in
the configure summary, so that summary is where a missing dependency shows. The recipe names
every component offered on Linux, so the set it asks for is explicit.

- `-D CMAKE_DISABLE_FIND_PACKAGE_GTest=ON` — the test suite is built whenever GoogleTest and
  QtTest are found, so the lookup is turned off.
- `-D BUILD_WERROR=OFF` — no `-Werror`. It is upstream's default, named.
- `-D ENABLE_ALSA=ON` — this switch and the other `ENABLE_` switches set `ON`: ALSA and PulseAudio
  output, D-Bus and the MPRIS2 interface, global shortcuts through
  [kglobalaccel](#port-kglobalaccel), device discovery through [udisks2](#port-udisks2) and GIO,
  audio CDs through [libcdio](#port-libcdio), MTP through [libmtp](#port-libmtp), the moodbar and
  its FFTW spectrum, the waveform seek bar, EBU R 128 loudness normalisation through
  [libebur128](#port-libebur128), Qt's private native interface, the stream tag reader on
  [sparsehash](#port-sparsehash), Chromaprint, and the iPod classic through
  [libgpod](#port-libgpod), whose artwork is read and written with gdk-pixbuf.
- `-D ENABLE_SONGTRACKING=ON` — fingerprint the collection locally with
  [chromaprint](#port-chromaprint), so a moved or renamed file keeps its play counts and ratings.
- `-D ENABLE_TAGFETCHER=OFF` — no tag fetcher: it sends fingerprints to AcoustID and MusicBrainz.
- `-D ENABLE_X11_GLOBALSHORTCUTS=OFF` — no global shortcuts grabbed on an X server, which a
  Wayland session never delivers; the media keys arrive over MPRIS.
- `-D ENABLE_SUBSONIC=OFF` — with `ENABLE_TIDAL`, `ENABLE_SPOTIFY` and `ENABLE_QOBUZ` off: no
  account-backed streaming services, and with `ENABLE_DISCORD_RPC=OFF` no Discord presence. The
  collection, playlists, tags and playback all work with no network.
- `-D ENABLE_TRANSLATIONS=OFF` — with `-D INSTALL_TRANSLATIONS=OFF`: no translations. When on,
  Strawberry compiles them into the binary unless told to install them as files, and KDOS ships
  English only.

## Notes
The desktop entry is KDOS's own, written over upstream's. Strawberry's `main()` sets the desktop
file name, so the Wayland app id is `org.strawberrymusicplayer.strawberry`, which
`StartupWMClass` repeats where upstream's says `strawberry`. The entry drops upstream's `tidal:`
scheme handler along with the Tidal service, and keeps the Play/Pause, Stop, Previous and Next
actions of upstream's five, leaving out Stop After Current.
