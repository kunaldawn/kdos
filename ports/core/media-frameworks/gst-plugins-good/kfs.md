## About

The GStreamer "good" plugin set: well-maintained plugins under the LGPL, on top of
[gst-plugins-base](#port-gst-plugins-base). It carries the common containers (Matroska, MP4,
AVI, FLV), RTP and RTSP, the Video4Linux camera source, the PulseAudio sink and source, the HTTP
source, the HLS, DASH and Smooth Streaming demuxers, and the decoders and encoders for FLAC, MP3,
WavPack, Speex, VP8 and VP9, PNG and JPEG. Browsers built on WebKitGTK, Rhythmbox, Strawberry,
Kamoso, Nheko and Dino play and record through it.

## Flags

- `-Dximagesrc=disabled` — No `ximagesrc`, the X11 screen-capture source.
- `-Dgtk3=enabled` — Build `gtksink` and `gtkglsink`, which draw video into a GTK 3 widget.
- `-Dqt6=enabled`, `-Dqt5=disabled` — Build `qml6glsink` and the `GstGLQt6VideoItem` type of the
  `org.freedesktop.gstreamer.Qt6GLVideoItem` QML module, which [kamoso](#port-kamoso) and
  [nheko](#port-nheko) draw camera and call video into; without it their windows show no picture.
  No Qt 5 plugin.
- `-Dqt-method=qmake` — Find Qt through `qmake6`, so the tools are found where it reports them:
  `qsb` from [qt6-qtshadertools](#port-qt6-qtshadertools) in `QT_HOST_BINS`, `/usr/lib/qt6/bin`,
  and `moc`, `rcc` and `uic` in `QT_HOST_LIBEXECS`.
- `-Dqt-wayland=enabled`, `-Dqt-x11=disabled`, `-Dqt-egl=disabled` — Build only the Qt plugin's
  Wayland window-system support. Its X11 support needs GLX in gstreamer-gl, which
  [gst-plugins-base](#port-gst-plugins-base) is built without (`-Dgl_platform=egl`), and its EGL
  support is for EGLFS, which [qt6-qtbase](#port-qt6-qtbase) is built without
  (`-DFEATURE_eglfs=OFF`).
- `-Drpicamsrc=disabled` — No Raspberry Pi camera module source.
- `-Dv4l2=enabled`, `-Dv4l2-libv4l2=enabled`, `-Dv4l2-gudev=enabled` — The Video4Linux 2 camera
  source and sink, with libv4l2 from [v4l-utils](#port-v4l-utils) for format conversions a camera
  needs and [libgudev](#port-libgudev) for finding devices.
- `-Dsoup=enabled`, `-Dadaptivedemux2=enabled` — `souphttpsrc` and the second-generation HLS,
  DASH and Smooth Streaming demuxers. Both load libsoup 3 with `dlopen()` at run time rather than
  linking it, so the build does not notice its absence: without [libsoup3](#port-libsoup3)
  installed both plugins load and register no element. It is in `depends` for that reason.
- `-Dhls-crypto=openssl` — Decrypt AES-encrypted HLS segments with [openssl](#port-openssl), named
  rather than chosen by what is found first.
- `-Dorc=enabled`, `-Dorc-compiler=disabled` — Use the [orc](#port-orc) run-time compiler for the
  SIMD kernels, and use the pre-generated orc C files shipped in the source instead of
  regenerating them.
- `-Dasm=enabled` — The deinterlacer's hand-written x86-64 assembly, assembled with
  [nasm](#port-nasm); `enabled` makes a missing `nasm` stop the configure.
- `-Dflac=enabled`, `-Dlame=enabled`, `-Dmpg123=enabled`, `-Dwavpack=enabled`, `-Dvpx=enabled`,
  `-Dpng=enabled`, `-Djpeg=enabled`, `-Dcairo=enabled`, `-Dbz2=enabled`, `-Daalib=enabled`,
  `-Dpulse=enabled`, `-Dgdk-pixbuf=enabled`, `-Dspeex=enabled`, `-Dtaglib=enabled`,
  `-Dtwolame=enabled` — Each plugin that links an outside library, named `enabled` so a missing
  one stops the configure: FLAC, LAME (MP3 encode), mpg123 (MP3 decode), WavPack, VP8 and VP9
  through [libvpx](#port-libvpx), PNG, JPEG, Cairo overlays, bzip2, the AAlib text-mode video
  sink, PulseAudio, GdkPixbuf image decoding, Speex, TagLib tag writing, and `twolamemp2enc`,
  MPEG-1 layer 2 audio encoding.
- `-Doss=disabled`, `-Doss4=disabled`, `-Djack=disabled` — No OSS or JACK audio elements. The
  PulseAudio elements are GStreamer's audio path, and `pipewire-pulse` serves them (see
  [Audio](../../../../docs/kdos/03-architecture/session.md#audio)).
- `-Dlibcaca=disabled`, `-Dshout2=disabled`, `-Ddv=disabled`, `-Ddv1394=disabled`,
  `-Damrnb=disabled`, `-Damrwbdec=disabled` — No libcaca video sink, no Icecast streaming, no DV
  video or FireWire capture, and no AMR codecs; each is pinned off, so it stays off whatever else
  is installed.

## Notes

After the install `build.sh` checks that the taglib, speex, gdkpixbuf and gtk plugins are in the
package.
