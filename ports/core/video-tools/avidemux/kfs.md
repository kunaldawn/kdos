## About
[Avidemux]{.idx} is a video editor for cutting, filtering and re-encoding: it trims and joins
clips, applies filters, and writes the result through its encoders or copies the streams
untouched. The port builds the Qt 6 application `avidemux3_qt6`, the command-line front end and
the plugins for both. Its core carries a private FFmpeg release under
`avidemux_core/ffmpeg_package`, patched by Avidemux and linked as its own `libADM6av*`
libraries, separate from the [ffmpeg](#port-ffmpeg) port.

## Flags
- `-DFAKEROOT="$PKG"` — Where the later parts look for the core's headers and CMake files. Each
  part is installed into `$PKG` before the next is configured.
- `-DENABLE_QT6=True` — With `-DQT6=ON` and `-DLRELEASE_EXECUTABLE=/usr/lib/qt6/bin/lrelease`:
  build the GUI and the Qt plugins against Qt 6, with Qt 6's `lrelease`.
- `-DOPENGL=ON` — and the rest of `_features`, the one list of features every part's configure
  gets. Each is named, on or off, because a missing library otherwise drops its plugin with a
  configure message only. On: OpenGL, ALSA and PulseAudio output, x264, x265, libvpx, Opus with
  its encoder, LAME, Vorbis, FAAD, TwoLAME, FreeType, Fontconfig, FriBidi, the built-in tinyPy
  script engine, and the system's libass and libmad in place of Avidemux's copies. Off: SDL;
  JACK, aRts, ESD and OSS sound; VDPAU, NVENC and XVideo; VA-API, because Avidemux's VA-API code
  needs libva's X11 back end, which the [libva](#port-libva) port does not build; gettext; FDK
  AAC, left out so the build stays redistributable; and VapourSynth, AOM, FAAC, Aften, dcaenc,
  libdca, Xvid and OpenCORE AMR, which are not ports.

## Patches
- `no-update-check.patch` — Sets the default of the `update.enabled` preference to 0, so Avidemux does not ask avidemux.org for a newer release at every start. The preference turns it back on.
- `ffmpeg-mathops-shift.patch` — Adds a patch to `avidemux_core/ffmpeg_package/patches/upstream/`, the list Avidemux applies to its FFmpeg, that hands x86 inline assembly in `libavcodec/x86/mathops.h` a masked immediate when the shift count is a constant and `%cl` otherwise. Under the link-time optimisation that FFmpeg build uses, a constant count outside 0 to 31 reaches the assembler, which rejects it ([A shift count out of range in inline assembly](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-shift-count-out-of-range-in-inline-assembly)).
- `musl-glibc-calls.patch` — musl has no `<execinfo.h>` and no `canonicalize_file_name()`. The crash handler keeps its backtrace for glibc; on musl a crash reports its message and source line and exits. The data directories are resolved with `realpath(path, NULL)`, the POSIX equivalent ([A glibc-only call on musl](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-glibc-only-call-on-musl)).
- `qt6-moc-headers-only.patch` — Leaves only headers in two `ADM_QT_WRAP_CPP` lists, the job control's and the updater's. Qt 6 refuses a `.cpp` there unless a target is named, and these two declare no `Q_OBJECT` class, so moc has nothing to generate from them ([A source file in a moc list](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-source-file-in-a-moc-list)).
- `qt6-no-cxx11.patch` — Drops the `-std=c++11` the GUI's `CMakeLists.txt` adds, which lands after the C++17 that Qt 6 needs and wins ([An older standard after CMake's](../../../../docs/kdos/05-developer/build-troubleshooting.md#an-older-standard-after-cmakes)).
- `mp4-menu-sentinel.patch` — Ends the MP4 muxer's format menu with an empty entry `{}` instead of `NULL`, which musl's C++ headers define as `nullptr` and which cannot initialise the entry's integer field ([`NULL` used as an integer](../../../../docs/kdos/05-developer/build-troubleshooting.md#null-used-as-an-integer)).
- `x265-stat-filename.patch` — x265's `param.rc.statFileName` is a fixed `char` array; the patch writes the two-pass log name into it with `snprintf` instead of assigning a `strdup`ed pointer ([A library member whose type changed](../../../../docs/kdos/05-developer/build-troubleshooting.md#a-library-member-whose-type-changed)).
- `fluxsmooth-table-size.patch` — Gives the FluxSmooth filter's `scaletab_MMX` table 65536 entries, the number `initScaleTab()` fills; declared with 65535, the table takes the loop's last write past its end.

## Notes
- **The build order.** `_part` configures, builds and installs one part into `$PKG`, and it is run
  in the order of upstream's `bootStrap.bash`, each part against those installed before it: the
  core, the Qt 6 GUI, the command-line front end, then the plugins once for each interface
  (`COMMON`, `QT4`, `CLI` and `SETTINGS`; `QT4` is Avidemux's name for its Qt interface, whatever
  the Qt version).
- **English only.** The Qt translations under `/usr/share/avidemux6/qt6/i18n` are removed; the
  interface falls back to its source strings.
- **The menu entry is replaced.** Upstream's runs `avidemux3_qt5`, which this build does not make,
  and has no `StartupWMClass`. With no organisation domain set, Qt takes the Wayland app_id from
  the program name, `avidemux3_qt6`. It claims no MIME type: the video types belong to the
  players.
