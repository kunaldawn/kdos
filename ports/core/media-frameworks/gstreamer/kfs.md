## About

GStreamer is the pipeline-based media framework: elements that read, decode, convert, encode and
write media are linked into pipelines, and an application drives the pipeline instead of handling
codecs itself. This port is the core: `libgstreamer-1.0`, the base, controller, net and check
libraries, the `gst-launch-1.0`, `gst-inspect-1.0` and `gst-typefind-1.0` tools, and the plugin
scanner. The elements themselves come from [gst-plugins-base](#port-gst-plugins-base) and the good,
bad, ugly and libav sets. GTK's media widgets, WebKitGTK's audio and video, Qt Multimedia and the
GStreamer-based players, editors and recorders are built on it.

## Flags

- `--libexecdir=/usr/lib` — The helper programs, `gst-plugin-scanner` and `gst-completion-helper`,
  are installed into `<libexecdir>/gstreamer-1.0`, so here into `/usr/lib/gstreamer-1.0`, and the
  scanner's path is compiled into the library. Every GStreamer port in the tree passes the same
  directory.
- `-Dintrospection=enabled` — Installs `Gst-1.0.gir` and the `GstBase`, `GstCheck`, `GstController`
  and `GstNet` GIRs, which every other GStreamer GIR includes and which Python programs load
  through `gi.require_version('Gst', '1.0')`.
- `-Ddoc=disabled`, `-Dexamples=disabled`, `-Dtests=disabled` — No documentation, examples or
  tests.
- `-Dptp-helper=disabled` — Builds no `gst-ptp-helper`, the program behind GStreamer's PTP network
  clock. It is installed setuid root or with network capabilities, so that it can bind PTP's
  privileged ports, and it is not among the setuid binaries the tree produces (see [setuid
  binaries](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#setuid-binaries)).
- `-Dlibunwind=enabled` — Walks the stack with libunwind, from
  [libunwind-nongnu](#port-libunwind-nongnu), for `gst_debug_get_stack_trace()` and the leaks
  tracer's stack traces. musl has no `backtrace()`, so without it both print nothing.
- `-Dlibdw=enabled` — Adds the source file and line to each frame through elfutils' libdw.
  `enabled` turns both backtrace switches from probes into requirements, so a missing library fails
  setup.
- `-Dbash-completion=enabled` — Installs the bash completion for the tools, which runs
  `gst-completion-helper`, against [bash-completion](#port-bash-completion).
- `-Dnls=disabled` — Builds and installs no translated messages. KDOS ships English only (see [the
  size and language
  policy](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#size-and-language-policy)).
- `-Dgst_debug=true` — Keeps the debugging subsystem (`GST_DEBUG` categories and logs), upstream's
  default, named here.
