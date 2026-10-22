## About

v4l-utils is the userspace half of [Video4Linux]{.idx}: `libv4l2` and `libv4lconvert`, which let a
program read a webcam in a pixel format it understands whatever the camera sends; `libdvbv5` for DVB
tuners; and the tools `v4l2-ctl`, `media-ctl`, `v4l2-compliance`, `cec-ctl`, `ir-keytable` and
`v4l2-tracer`. GStreamer's camera source in gst-plugins-good, obs-studio, vlc, zbar, wine and
sane-backends link it.

## Flags

- `-D_LARGEFILE64_SOURCE` — In `CFLAGS` and `CXXFLAGS`: musl declares `open64` and `mmap64`, which
  `v4l2-tracer`'s preload library wraps and its retrace calls, only under this macro; see [A glibc-only call on
  musl](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#a-glibc-only-call-on-musl).
- `-Dudevdir=/lib/udev` — Install the udev rules and `ir-keytable`'s remote-control keymaps where
  eudev reads them; `/lib` is `/usr/lib` on KDOS.
- `-Dbpf=enabled` — Build `ir-keytable`'s BPF protocol decoders, compiled with clang for the BPF
  target and loaded through libbpf; `clang`, `libbpf` and `elfutils` are dependencies for this.
- `-Dgconv=disabled` — Build no gconv modules for the ARIB STD-B24 and EN 300 468 character sets DVB
  broadcasts use. gconv is glibc's converter-plugin interface, and musl has no `gconv.h`.
- `-Djpeg=enabled` — Let `libv4lconvert` decode JPEG and Motion-JPEG cameras through libjpeg-turbo.
- `-Dlibdvbv5=enabled` — Build `libdvbv5` and the `dvb*` tools.
- `-Dqv4l2=disabled` — With `-Dqvidcap=disabled`, build neither Qt program here. The
  [qv4l2](#port-qv4l2) port builds them from the same source in `44_apps`; this phase has no
  toolkit.
- `-Dv4l2-tracer=enabled` — Build `v4l2-tracer`, which records and replays a program's V4L2 calls,
  through json-c.
- `-Dv4l-plugins=true` — With `-Dv4l-utils=true`, `-Dv4l-wrappers=true`,
  `-Dv4l2-compliance-libv4l=true`, `-Dv4l2-ctl-libv4l=true` and `-Dv4l2-ctl-stream-to=true`, build
  libv4l's plugin support, the utilities, the `v4l1compat` and `v4l2convert` preload wrappers, the
  libv4l paths in `v4l2-compliance` and `v4l2-ctl`, and `v4l2-ctl --stream-to`. All are on wherever
  their options are left unset; the recipe names them.
- `-Ddoxygen-doc=disabled` — Build no Doxygen documentation.
