## About

kdos-record is the desktop's [screen recorder]{.idx}, on `Alt+Print`. It asks the ScreenCast
portal for one monitor, takes the PipeWire node the portal hands back, and runs
`gst-launch-1.0 -e pipewiresrc ! videoconvert ! <encoder> ! matroskamux ! filesink` into
`~/Videos`, with `x264enc` when that element exists and `vp8enc` otherwise. Running it again while
it records stops the recording, with an interrupt the pipeline turns into end-of-stream so the
file gets its index.

It is a program rather than a shell script because `xdg-desktop-portal` ties a session to the bus
connection that created it and closes the session when that connection goes: three `gdbus call`
invocations are three connections, and the second is told `Invalid session`. Going through the
portal rather than round it keeps one answer to "who may record this screen". The whole sequence,
its deadlines and its traps are in
[Recording](../../../docs/kdos/03-architecture/session.md#recording).

## Notes

- **It links only `basu`.** GStreamer is a dependency at run time and not at build time: the
  pipeline is a `gst-launch-1.0` child process, so a program that only reads one PipeWire node
  does not take on GStreamer's initialisation as well. The elements come from the ports in `depends`: `pipewiresrc` from `pipewire`
  (built with its GStreamer plugin), `videoconvert` from `gst-plugins-base`, `vp8enc` and
  `matroskamux` from `gst-plugins-good`, and `x264enc` from `gst-plugins-ugly`.
- **The screen is chosen by a person**, through the ScreenCast backend `xdg-desktop-portal-wlr`
  and the chooser its per-user configuration names, so `SelectSources` is given minutes, not
  seconds, before the program gives up.
- **A recording of a still screen depends on a wlroots patch.** `wlroots`'
  `idle-capture-frame.patch` makes every capture request ask for a frame; without it a screen
  where nothing moves yields one frame and the file ends with no header.
- **Any recording at all depends on an `xdg-desktop-portal-wlr` patch.** `pipewiresrc` pauses
  and resumes the stream on its way to playing, while it still holds both buffers;
  `stream-start-retry.patch` makes the backend retry the capture rather than stop, and without it
  every recording is a header with at most one frame, on any renderer.
