## About

FFmpeg records, converts and streams audio and video: the `ffmpeg` and `ffprobe` programs and the
`libav*` libraries (codecs, formats, filters, devices, scaling and resampling) that mpv,
GStreamer's `gst-libav`, PipeWire, OBS Studio, Blender and most other media programs here are
built on. It is built with the full codec set: H.264, HEVC, VP8/VP9 and AV1 encode and decode, JPEG
XL, MP3, Opus, Vorbis, FLAC and LC3, with subtitle burn-in, HDR tone mapping, high-quality
resampling, time-stretch, OCR, QR codes, the OpenCL filters, DVD and Blu-ray input, and hardware
acceleration through VA-API and Vulkan. What a user gets from it is in
[Media, colour and time](../../../../docs/kdos/02-user-guide/administration.md#media-colour-and-time).

The binary and libraries are GPL-3.0-or-later. `LICENSE.notice`, beside the recipe, is the record
of that choice and of every component's licence, and the notice a redistributor should read.

## Flags

- `-frandom-seed=ffmpeg` — With LTO, fixes the name GCC gives each object's `.gnu.lto_*` sections,
  which is otherwise random on every build. FFmpeg is one of the hot libraries the tree builds with
  link-time optimisation, and every one of them carries the seed (see
  [Every port](../../../../docs/kdos/05-developer/writing-ports.md#every-port)).
- `--enable-lto=auto` — Link-time optimisation, through FFmpeg's own switch.
- `--disable-debug` — Compiles without `-g`; no port ships DWARF.
- `--disable-stripping` — Installs the programs and libraries as linked, without running `strip` on
  them: a package carries what the port's flags compiled, and kpkg strips only the symbol table of
  the largest objects (see
  [What kpkg does around build.sh](../../../../docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).
- `--enable-shared`, `--enable-pic`, `--enable-pthreads` — Shared `libav*` libraries, built as
  position-independent code, with POSIX threads.
- `--enable-gpl`, `--enable-version3` — Admits the GPL components (x264, x265, rubberband, vid.stab
  and the DVD pair require `--enable-gpl`), and with `--enable-version3` configure licenses the
  result as GPL version 3 or later. Leaving out the GPL components is the only way back to LGPL,
  and it costs H.264 and HEVC encoding.
- `--enable-gnutls` — TLS for the network protocols (`https`, `tls`, `rtmps`) through
  [gnutls](#port-gnutls).
- `--enable-libx264`, `--enable-libx265`, `--enable-libvpx`, `--enable-libsvtav1`,
  `--enable-libdav1d`, `--enable-libopus`, `--enable-libvorbis`, `--enable-libmp3lame`,
  `--enable-liblc3`, `--enable-libjxl` — The external codecs: H.264 and HEVC encoding, VP8/VP9, AV1
  encoding through SVT-AV1 and decoding through dav1d, Opus, Vorbis, MP3 encoding, LC3 and JPEG XL.
  libaom and libopenjpeg are not enabled: libaom would be a second AV1 encoder beside SVT-AV1, and
  libopenjpeg a second JPEG 2000 encoder beside FFmpeg's native one.
- `--enable-libfreetype`, `--enable-libfontconfig`, `--enable-libharfbuzz`, `--enable-libfribidi`,
  `--enable-libass` — Text: `drawtext` with font lookup, shaping and bidirectional layout, and
  subtitle burn-in through libass.
- `--enable-libwebp`, `--enable-libxml2`, `--enable-bzlib`, `--enable-zlib`, `--enable-lzma`,
  `--enable-iconv` — WebP; the XML the DASH and IMF demuxers read; bzip2, zlib and LZMA
  decompression inside containers and image formats; and character-set conversion of subtitles.
- `--enable-libplacebo`, `--enable-lcms2`, `--enable-libzimg` — GPU rendering and tone mapping
  through `libplacebo`, ICC colour management, and the `zscale` filter.
- `--enable-librubberband`, `--enable-libsoxr` — Time-stretch and pitch-shift (`rubberband`), and
  high-quality resampling.
- `--enable-libtesseract`, `--enable-libqrencode`, `--enable-librsvg`, `--enable-libvidstab` — The
  `ocr` filter; the `qrencode` filter and `qrencodesrc` source; SVG decoding; and the two-pass
  `vidstabdetect` and `vidstabtransform` stabiliser.
- `--enable-libbluray`, `--enable-libdvdnav`, `--enable-libdvdread` — The `bluray:` protocol, and
  the `dvdvideo` demuxer, so `ffmpeg -f dvdvideo -i /dev/sr0` rips a title with its chapters.
- `--enable-libdrm`, `--enable-vaapi`, `--enable-v4l2-m2m` — DRM frame sharing and `kmsgrab`,
  VA-API hardware decode and encode, and the kernel's V4L2 memory-to-memory codecs.
- `--enable-vulkan` — The Vulkan hardware context, filters, encoders and decoders.
- `--glslc=glslang` — The SPIR-V compiler the Vulkan filters, encoders and hwaccels are compiled
  with, from [glslang](#port-glslang). Its probe only disables them when it fails, so `build.sh`
  checks `config_components.h` for the Vulkan FFV1 encoder after configure and stops when it is
  missing.
- `--enable-opencl` — The `*_opencl` filters (`tonemap`, `nlmeans`, `unsharp`, `overlay` and the
  rest), against [ocl-icd](#port-ocl-icd)'s loader. They run on Mesa's rusticl, which offers a
  device only for the drivers `RUSTICL_ENABLE` names, set in `/etc/profile.d/50-opencl.sh`;
  `-init_hw_device opencl` fails with no device where it names none.
- `--enable-alsa` — ALSA capture and playback devices.
- `--enable-manpages`, `--disable-htmlpages`, `--disable-txtpages` — The manual pages only.
- `--disable-sdl2` — Builds no SDL output device and, with it, no `ffplay`. sdl2-compat reaches
  back to FFmpeg through sdl3 and pipewire, so depending on it would be a dependency cycle.
- `--disable-vdpau`, `--disable-sndio`, `--disable-ffnvcodec`, `--disable-cuda-llvm`,
  `--disable-amf` — VDPAU, sndio, NVIDIA's NVENC/NVDEC and CUDA, and AMD's AMF have no port; each
  is autodetected, and VDPAU's probe links libX11.
- `--disable-libxcb`, `--disable-xlib` — Builds neither the `x11grab` capture device nor the Xlib
  output device. Both are autodetected, and libxcb (installed in an earlier phase) and libX11
  (built in this one) are ports, so each is named off and the result does not follow build order.

## Notes

Every autodetected library is named on or off, so what ships does not depend on which ports happen
to be installed first. FFmpeg's `--enable-libjack` is off unless named, and it stays off: pipewire
links FFmpeg, so FFmpeg cannot have PipeWire's JACK, and its `jack` capture device is not built
(see
[JACK programs on the host](../../../../docs/kdos/03-architecture/session.md#jack-programs-on-the-host)).

Everything that links FFmpeg, among them pipewire and gst-libav, takes on its GPL with it.
