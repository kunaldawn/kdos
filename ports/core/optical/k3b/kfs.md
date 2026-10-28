## About

[K3b]{.idx} is KDE's disc-burning program: data, audio and video CDs, DVDs and Blu-ray discs,
disc copies, image writing, and ripping of audio CDs and video DVDs. It is a Qt 6 and KDE
Frameworks application, and it writes discs through external programs it finds on `PATH`:
`cdrskin` from [libburn](#port-libburn) for CD and DVD, `growisofs` from
[dvd+rw-tools](#port-dvdxrw-tools) for DVD and Blu-ray, [cdrdao](#port-cdrdao) for disc-at-once
writing and copies, and `mkisofs`, which [libisoburn](#port-libisoburn) installs as a link to
`xorriso`, for data images.

## Flags

- `-D K3B_DOC=ON`, `-D K3B_DEBUG=OFF`, `-D K3B_BUILD_API_DOCS=OFF` — Build the user handbook,
  without K3b's extra debugging support and without the API documentation of its libraries; all
  three are upstream's defaults.
- `-D K3B_ENABLE_DVD_RIPPING=ON`, `-D K3B_ENABLE_TAGLIB=ON` — Video DVD ripping through
  [libdvdread](#port-libdvdread), and audio-file metadata through [taglib](#port-taglib).
- `-D K3B_BUILD_FFMPEG_DECODER_PLUGIN=ON` — and the other `K3B_BUILD_*_PLUGIN=ON` switches: the
  decoder plugins for FFmpeg, Ogg Vorbis, MP3 (libmad), FLAC, libsndfile and WAV, and the encoder
  plugins for Ogg Vorbis, LAME, SoX and external programs. These decide which audio files can go
  onto an audio CD and which formats a ripped CD can be saved in. The SoX and external-program
  encoders run programs at run time and link no codec library.
- `-D K3B_BUILD_MUSE_DECODER_PLUGIN=OFF` — No Musepack decoder; libmpcdec is not a port.
- `-D CMAKE_REQUIRE_FIND_PACKAGE_DvdRead=ON` — and the other `CMAKE_REQUIRE_FIND_PACKAGE_*=ON`
  switches, for Taglib, FFmpeg, Flac, Flac++, Mad, Sndfile, Lame and OggVorbis. Upstream marks
  each of these libraries optional and drops its plugin when it is absent; required, a missing
  library stops the configuration instead of shipping a K3b that cannot burn an MP3 or a FLAC file
  as an audio CD.
- `-D CMAKE_DISABLE_FIND_PACKAGE_Qt6WebEngineWidgets=ON` — Do not use Qt WebEngine even though
  [qt6-qtwebengine](#port-qt6-qtwebengine) is a port. K3b uses it only to restyle the disk-info
  pane, which is a plain Qt view without it, and it would make K3b load a Chromium engine for that.
- `-D KF_SKIP_PO_PROCESSING=ON` — Build and install no translations. The packager removes every
  non-English catalogue and translated handbook in any case (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)),
  so the switch only saves build time.

## Notes

Audio-CD ripping opens [libcdio-paranoia](#port-libcdio-paranoia) at run time with `dlopen()`. The
KAuth helper `k3bhelper` runs as root behind two polkit actions, each of which asks for an
administrator's password: `org.kde.k3b.updatepermissions` sets the group and mode of the burner
devices and of the burning programs K3b runs, setuid root where K3b asks for it, and
`org.kde.k3b.addtogroup` adds a user to a group with `gpasswd`.

`build.sh` replaces upstream's desktop entry. `KAboutData` sets the desktop file name, so the
Wayland app id is `org.kde.k3b`, not the `k3b` upstream's entry names in `StartupWMClass`, and the
new entry names `org.kde.k3b` there. It also claims only K3b's own project type: disc images and
blank media are types other programs open too, and a default among them is chosen in
`mimeapps.list`.
