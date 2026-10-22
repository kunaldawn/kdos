## About
[SMPlayer]{.idx} is a Qt front end for [mpv](#port-mpv) that remembers the settings of each file it
has played, so a file played again resumes at the point it stopped, with the same settings. It runs mpv as a child process and has it draw
into SMPlayer's own window by X11 window id, so on Linux `main.cpp` sets `QT_QPA_PLATFORM=xcb`
for itself: SMPlayer runs under Xwayland, and the mpv it starts uses its X11 video output.

## Flags
- `QMAKE=qmake-qt5` — Build against Qt 5, with Qt 5's `lrelease` for the translations
  (`LRELEASE=/usr/lib/qt5/bin/lrelease`). The project file accepts Qt 6, but the code uses Qt 5
  interfaces with no Qt 6 replacement (`QTextStream::setCodec`, `QRegExp` without its header);
  see [A Qt 5 program built against Qt 6](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-qt-5-program-built-against-qt-6).
- `DEFINES+=NO_MPLAYER` — Upstream's switch that removes the MPlayer and MPlayer2 back ends,
  leaving mpv the one player SMPlayer drives. MPlayer is not a port.
- `QMAKE_CFLAGS_RELEASE` — With `QMAKE_CXXFLAGS_RELEASE` and `QMAKE_LFLAGS_RELEASE`, hands the
  tree's compiler and linker flags to qmake, which reads none from the environment. Without them
  the binary is built with qmake's own release flags and none of the tree's.
- `CFLAGS_EXTRA=-Wno-error` — The top-level `all` target also builds `simple_web_server`, the
  web server of the Chromecast sender, whose makefile compiles with `-Werror` followed by
  `$(CFLAGS_EXTRA)`, so `-Wno-error` lands after it and a warning does not stop the build. The
  server itself is not shipped.

## Patches
- `offline.patch` — Comments out four `DEFINES` in `src/smplayer.pro`, compiling out the features that exist only to reach the internet: `UPDATE_CHECKER`, `FIND_SUBTITLES` (the OpenSubtitles search), `YOUTUBE_SUPPORT` (the URL resolver, whose code downloader fetches a `yt-dlp` binary) and `CHROMECAST_SUPPORT`. The project file sets them unconditionally, and qmake has no switch for any one of them.

## Notes
- **What `build.sh` removes.** `simple_web_server` from `/usr/bin`, since it serves only the
  Chromecast sender, which is not built; every translation catalogue but English; every manual
  directory under `/usr/share/doc/packages/smplayer` but `en`; and the `smplayer_enqueue.desktop`
  entry, a file-manager action rather than a program. The manual page is decompressed, because
  the install gzips it and gzip records the time it ran in the file.
- **The data paths stay upstream's.** `DOC_PATH` and the other paths reach the compiler as quoted
  string defines through the top-level Makefile's environment; set on the `make` line, they would
  reach the sub-make unquoted. The manual SMPlayer's About dialog shows is under
  `/usr/share/doc/packages/smplayer`, which the package-time prune of HTML manuals exempts (see
  [Size and language policy](../../../../docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- **The menu entry is replaced.** Qt's xcb backend sets `WM_CLASS` from the program name,
  `smplayer`, which is the `StartupWMClass`. It claims no MIME type: mpv's entry claims the video
  and audio types, and `mimeapps.list` is where the default player is chosen.
