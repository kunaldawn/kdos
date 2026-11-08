## About

Audex rips audio CDs: it reads each track with [libcdio-paranoia](#port-libcdio-paranoia)'s
error correction, looks the disc up in CDDB through [libkcddb](#port-libkcddb), names files and
folders from a pattern, and encodes them with an external encoder. It also writes cue sheets,
playlists and checksum lists beside the files. Inserting an audio CD offers "Extract Digital
Audio with Audex" through a Solid device action.

## Flags

- `-D KF_SKIP_PO_PROCESSING=ON` — Skips building the interface translations, which the
  package-wide English-only rule would remove anyway (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).

## Patches

`app-id.patch` changes `StartupWMClass=audex` to `StartupWMClass=org.kde.audex` and drops
`MimeType=application/x-audex;` from upstream's desktop entry. Audex sets its desktop file name,
and so its Wayland app_id, to `org.kde.audex`, which the shorter class does not match; no file in
the shared MIME database defines `application/x-audex`, so the claim matches no file.

## Notes

Encoding runs a program by name from `PATH`: `flac` from [flac](#port-flac), `oggenc` from
[vorbis-tools](#port-vorbis-tools), `opusenc` from [opus-tools](#port-opus-tools) and `lame` from
[lame](#port-lame), all in `depends` for run time. The AAC profile runs `faac`, which is not a
port, so that profile reports a missing encoder. The CDDB lookup goes to the network when a disc
is read.

libkcddb, libcdio, `libcdio_cdda` and `libcdio_paranoia` are each `REQUIRED`. The three libcdio
libraries are found by upstream's own find modules, which take pkg-config's paths as hints when
pkg-config is installed and search the standard paths without it.
