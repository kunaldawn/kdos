## About

The GStreamer "bad" plugin set: plugins upstream considers less mature or less reviewed, and the
libraries other GStreamer components build on, among them `gstreamer-play`, the VA-API, Vulkan
and Wayland video libraries, WebRTC (`webrtcbin`), and the HLS and DASH elements. [gtk4](#port-gtk4)
plays video through its `gstreamer-play` library, and the WebKitGTK ports, Nheko, Dino, Kamoso and
Strawberry depend on it.

## Flags

Every plugin that links an outside library is named, `enabled` or `disabled`, never left at
meson's `auto`: an `enabled` library that goes missing fails the configure instead of giving a
thinner package, and a `disabled` one stays off whatever else is installed first.

- `-Dintrospection=enabled` — Generate the GIR files for the libraries, such as `GstPlay-1.0`
  and `GstWebRTC-1.0`. They include the `Gst-1.0` and `GstBase` GIRs, so
  [gstreamer](#port-gstreamer) and [gst-plugins-base](#port-gst-plugins-base) are built with
  introspection too.
- `-Dx11=disabled`, `-Dgtk3=disabled` — No X11 support in the Vulkan, GL and RFB plugins, and no
  `gtkwaylandsink`, the GTK 3 Wayland video sink. [gst-plugins-good](#port-gst-plugins-good)
  builds the GTK 3 `gtksink`.
- `-Dorc=enabled`, `-Dorc-compiler=disabled` — Use [orc](#port-orc) for the SIMD kernels, from the
  pre-generated C files in the source.
- `-Dgpl=enabled` — Allow the plugins whose libraries are GPL. Upstream's default is off, and
  with it off, `x265`, `faad` or `resindvd` named `enabled` stops the configure.
- `-Dx265=enabled` — HEVC encoding through [x265](#port-x265).
- `-Dfaad=enabled` — AAC decoding through [faad2](#port-faad2).
- `-Dsvtav1=enabled` — AV1 encoding through [svt-av1](#port-svt-av1).
- `-Dfdkaac=enabled` — AAC encoding through [fdk-aac](#port-fdk-aac).
- `-Dopenjpeg=enabled` — JPEG 2000 through [openjpeg](#port-openjpeg).
- `-Dassrender=enabled` — SSA/ASS subtitles drawn over video, through [libass](#port-libass).
- `-Dopus=enabled`, `-Dwebp=enabled`, `-Drsvg=enabled`, `-Dsndfile=enabled`,
  `-Dopenexr=enabled`, `-Dbz2=enabled` — Opus parsing, WebP, SVG decoding and overlays, the
  formats [libsndfile](#port-libsndfile) reads (`sfdec`; upstream builds no encoder), OpenEXR
  decoding, and bzip2.
- `-Dzbar=enabled` — The `zbar` element, which reads barcodes and QR codes from video through
  [zbar](#port-zbar).
- `-Dfluidsynth=enabled` — MIDI synthesis through [fluidsynth](#port-fluidsynth).
- `-Dcolormanagement=enabled` — Colour-management correction through [lcms2](#port-lcms2).
- `-Danalyticsoverlay=enabled` — Draws the results of video-analytics elements over the video,
  with Cairo and Pango.
- `-Dwebrtcdsp=enabled` — Echo cancellation and noise suppression, through
  [webrtc-audio-processing](#port-webrtc-audio-processing).
- `-Dwebrtc=enabled`, `-Dsrtp=enabled`, `-Ddtls=enabled`, `-Dsctp=enabled` — `webrtcbin`, for
  peer-to-peer calls: ICE through [libnice](#port-libnice), media as SRTP through
  [libsrtp](#port-libsrtp), keys negotiated over DTLS with [openssl](#port-openssl), and data
  channels over SCTP.
- `-Dsctp-internal-usrsctp=enabled` — Build the SCTP plugin on the copy of usrsctp in the source,
  upstream's default; usrsctp is not a port.
- `-Dresindvd=enabled` — `rsndvdbin`, DVD playback with menus, through
  [libdvdnav](#port-libdvdnav) and [libdvdread](#port-libdvdread).
- `-Dchromaprint=enabled` — Audio fingerprints through [chromaprint](#port-chromaprint), for
  taggers that look a track up.
- `-Dopenmpt=enabled` — Tracker modules (MOD, XM, IT, S3M) through
  [libopenmpt](#port-libopenmpt).
- `-Dva=enabled`, `-Dudev=enabled`, `-Ddrm=enabled` — The VA-API hardware decoders and encoders,
  through [libva](#port-libva), with [libdrm](#port-libdrm) and [libgudev](#port-libgudev) to find
  and open the GPU.
- `-Dkms=enabled`, `-Dwayland=enabled`, `-Dgl=enabled` — The KMS and Wayland video sinks and the
  OpenGL integration.
- `-Dvulkan=enabled`, `-Dvulkan-video=enabled`, `-Dvulkan-windowing=wayland` — The Vulkan elements
  and Vulkan Video decoding and encoding, with Wayland as their only window system. The shaders
  are compiled at build time with `glslc` from [shaderc](#port-shaderc) when it is installed, and
  otherwise with [glslang](#port-glslang), the one in `depends`.
- `-Ddash=enabled`, `-Dhls=enabled`, `-Dsmoothstreaming=enabled`, `-Dttml=enabled` — The older
  adaptive-streaming demuxers and the TTML subtitle parser and renderer.
- `-Dhls-crypto=openssl` — Decrypt AES-encrypted HLS segments with [openssl](#port-openssl).
- `-Dcurl=enabled`, `-Dcurl-ssh2=enabled` — The curl network source and sink, with SFTP and SCP
  through [libssh2](#port-libssh2).
- `-Daes=enabled` — AES encryption and decryption elements.
- `-Dsrt=enabled` — SRT (Secure Reliable Transport) streaming, through [libsrt](#port-libsrt).
- `-Daom=disabled` — No AV1 elements through libaom, which is not a port; svt-av1 is the tree's
  AV1 encoder.
- `-Dbluez=disabled`, `-Dsbc=disabled`, `-Dldac=disabled`, `-Dopenaptx=disabled`,
  `-Dlc3=disabled` — No Bluetooth audio elements or Bluetooth codecs. Bluetooth audio is
  [pipewire](#port-pipewire)'s, built with its bluez5 codecs.
- `-Dlibde265=disabled` — No HEVC decoder through libde265; [gst-libav](#port-gst-libav) decodes
  HEVC.
- `-Dcodec2json=disabled`, `-Dqroverlay=disabled`, `-Dv4l2codecs=disabled`,
  `-Duvch264=disabled`, `-Duvcgadget=disabled` — No codec-to-JSON debugging element, no QR-code
  overlay, no Video4Linux stateless decoders, no UVC H.264 camera source and no UVC gadget sink.
- `-Davtp=disabled` through `-Daja=disabled` — The remaining plugins that link an outside library,
  each pinned off: AVTP, bs2b, dc1394, DirectFB, DTS, FAAC, Flite, GME, Ghostscript, GSM, IQA, iSAC,
  LADSPA, LCEVC, LV2, microdns, modplug, mpeg2enc, MPEG-H, mplex, Intel Media SDK, Musepack, neon,
  the NVIDIA plugins, ONNX, OpenAL, OpenCV, OpenH264, OpenNI2, RTMP, SoundTouch, spandsp, the SVT
  HEVC and JPEG XS encoders, teletext, TensorFlow Lite, tinyalsa, VMAF, the VisualOn encoders,
  WildMIDI, WPE, ZXing and AJA.
