## About

Kwave is KDE's sound editor. It records, cuts, pastes and filters sound, and reads and writes
WAV, FLAC, MP3, Ogg Vorbis and Ogg Opus files; [audiofile](#port-audiofile) handles WAV and the
other uncompressed formats. It is a Qt 6 and KDE Frameworks program.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — compile none of the translation catalogues in `po/`; the package
  is English only under the
  [size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy),
  so this only saves build time.
- `-D WITH_DOC=ON` — build Kwave's handbook with [kdoctools](#port-kdoctools). On by default
  upstream.
- `-D WITH_ALSA=ON` — and `WITH_PULSEAUDIO`, `WITH_QT_AUDIO`: playback and recording through ALSA
  and PulseAudio, both of which PipeWire serves on KDOS, and playback through Qt Multimedia
  ([qt6-qtmultimedia](#port-qt6-qtmultimedia)).
- `-D WITH_OSS=OFF` — no playback or recording through the OSS device interface, which upstream
  builds by default.
- `-D WITH_FLAC=ON` — and `WITH_OGG_OPUS`, `WITH_OGG_VORBIS`: the FLAC, Opus and Vorbis codecs.
  Each switch makes its library a `REQUIRED` pkg-config check, so a missing library stops the
  configure instead of dropping the format.
- `-D WITH_MP3=ON` — MP3 support: decoding through [libmad](#port-libmad), tags through
  [id3lib](#port-id3lib), both `REQUIRED` once the switch is on, and encoding by running an
  external `lame` program, which is why [lame](#port-lame) is in `depends`.
- `-D DEBUG=OFF` — leave the debug plugin out of the menu. This is upstream's default.

## Notes

Upstream installs its icon as SVG only, which the panel does not draw, so the recipe rasterises
`org.kde.kwave.svg` to PNGs from 32 to 128 pixels. It also writes its own `org.kde.kwave.desktop`:
KAboutData sets the desktop file name, so the Wayland app id is `org.kde.kwave`, which the entry
names in `StartupWMClass`. The entry has no `MimeType` line, so Kwave is offered as the opener of
no file type.
