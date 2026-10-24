## About
libcdio gives programs one interface to CD-ROM drives and CD images: it reads audio CD tracks and
the table of contents, and with its `libiso9660` part reads ISO 9660 file systems. Audio CD
playback in [mpv](#port-mpv), [mpd](#port-mpd), [gst-plugins-ugly](#port-gst-plugins-ugly) and
[kodi](#port-kodi) goes through it, and [libcdio-paranoia](#port-libcdio-paranoia) builds its
error-correcting ripper on it. The package also installs `cd-info`, `cd-read`, `cd-drive`,
`iso-info`, `iso-read`, `cdda-player` and `mmc-tool`.

## Flags
- `--disable-cddb` — Build `cd-info` and `cdda-player` without CDDB lookups of a disc's track
  titles. configure takes them only from libcddb, which the tree has no port for, and would
  otherwise warn and leave them out.
- `--disable-vcd-info` — Build `cd-info` without Video CD details, which come from libvcdinfo;
  the tree has no port for it. It is the default.
- `--enable-cxx` — Build the C++ bindings, `libcdio++` and `libiso9660++`, the default.
- `--disable-example-progs` — Build none of the C sample programs under `example/`, and with
  `--disable-cpp-progs` none of the C++ ones.

## Notes
The manual pages of `cd-drive`, `cd-info`, `cd-read`, `iso-info` and `iso-read` are made by
running each freshly built tool through [help2man](#port-help2man): the make rule depends on the
program, so the copies in the tarball are always out of date and are rebuilt. That is why
`help2man` is in `depends`; [ncurses](#port-ncurses) is
there for `cdda-player`'s screen.
