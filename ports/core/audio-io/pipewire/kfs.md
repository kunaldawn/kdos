## About
PipeWire is the media graph every sound and every captured screen on KDOS goes through. It is the
sound server, with `pipewire-pulse` standing in for PulseAudio and its own `libjack.so.0` standing
in for JACK, so PulseAudio, JACK and plain ALSA programs all land in the same graph; and it
carries video, which is how a screencast reaches a recorder: `kdos-record` asks the ScreenCast
portal for a node and reads it through PipeWire's GStreamer element. The session starts
`pipewire`, then [wireplumber](#port-wireplumber), then `pipewire-pulse`, once per user; see
[Audio](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#audio).
Many ports depend on it, from [kdos-shell](#port-kdos-shell) and the portals to the
browsers and the music software.

The KDOS build differs from a distribution's in three ways. It builds no session manager of its
own (WirePlumber is a separate port), it carries no systemd, logind or X11 code, and it names
every optional feature on or off instead of leaving it to `auto`, because a meson feature left on
`auto` turns itself off without a word when its library is missing, and the resulting PipeWire
then lacks a codec or a module with nothing to say why. The `depends` line is the list of those
libraries.

## Flags
- `-frandom-seed=pipewire` — Fix the number GCC names each object's link-time optimisation
  sections with, which is otherwise random, so an object built twice comes out the same. Every port
  built with LTO adds it; see the LTO rule in [Every port](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#every-port).
- `-Db_lto=true` — Build with link-time optimisation. PipeWire is one of the few hot libraries the
  tree gives LTO to, because the desktop spends much of its time in it.
- `-Ddocs=disabled`, `-Dman=enabled` — Build the manual pages and not the HTML API reference.
  The manual pages are generated with doxygen and python3, which is why those two are in
  `depends`; with either missing, `-Dman=enabled` stops the configure step.
- `-Dtests=disabled`, `-Dexamples=disabled` — Build neither upstream's tests nor its example
  programs.
- `-Dffmpeg=enabled` — Build the SPA plugin that decodes and encodes compressed audio with
  [ffmpeg](#port-ffmpeg)'s libraries.
- `-Dpw-cat-ffmpeg=enabled` — Let `pw-cat`, `pw-play` and `pw-record` read and write any format
  ffmpeg knows, not only those libsndfile does.
- `-Dalsa=enabled` — The SPA plugin that drives sound cards through [alsa-lib](#port-alsa-lib).
- `-Dpipewire-alsa=enabled` — The ALSA plugin that lets a plain ALSA program play into PipeWire.
  PipeWire installs its configuration drop-ins where alsa-lib does not look, so KDOS reaches it
  through `/etc/alsa/conf.d/99-kdos-pipewire.conf`; see
  [Reaching the graph from a plain ALSA program on the host](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#reaching-the-graph-from-a-plain-alsa-program-on-the-host).
- `-Dcompress-offload=enabled` — The ALSA compress-offload sink, which hands compressed audio to a
  sound card's own decoder where the hardware has one.
- `-Dudev=enabled` — Find sound cards and cameras through [eudev](#port-eudev).
- `-Ddbus=enabled` — Build the parts that talk over D-Bus: the Bluetooth plugin's link to BlueZ,
  real-time scheduling through RealtimeKit, the portal module and `pw-reserve`'s device
  reservation.
- `-Dlibusb=enabled` — Link [libusb](#port-libusb) into the Bluetooth plugin, which uses it for
  adapter quirks.
- `-Dselinux=disabled` — No SELinux integration; KDOS runs no SELinux.
- `-Dbluez5=enabled` — The Bluetooth audio plugin, over [bluez](#port-bluez), with SBC from
  [sbc](#port-sbc) always present.
- `-Dbluez5-codec-aptx=enabled`, `-Dbluez5-codec-ldac=enabled`, `-Dbluez5-codec-aac=enabled`,
  `-Dbluez5-codec-lc3=enabled`, `-Dbluez5-codec-opus=enabled`, `-Dbluez5-codec-g722=enabled` —
  aptX from [libfreeaptx](#port-libfreeaptx), LDAC from [ldacbt](#port-ldacbt), AAC from
  [fdk-aac](#port-fdk-aac), LC3 for LE Audio from [liblc3](#port-liblc3), Opus from
  [opus](#port-opus), and G.722. Named one by one because each is `auto` upstream, and a headset
  whose codec was quietly left out falls back to SBC.
- `-Dbluez5-codec-lc3plus=disabled`, `-Dbluez5-codec-ldac-dec=disabled`,
  `-Dbluez5-plc-spandsp=disabled` — The LC3plus codec, LDAC decoding and SpanDSP's packet loss
  concealment. Their libraries are not in the tree.
- `-Dreadline=enabled` — Line editing in `pw-cli`'s interactive mode, with
  [readline](#port-readline).
- `-Dlibpulse=enabled` — Build `module-pulse-tunnel`, which sends a stream to a PulseAudio server
  on another machine. [libpulse](#port-libpulse) is the client library only; the server on this
  machine stays `pipewire-pulse`.
- `-Dfftw=enabled` — Give the filter-chain convolver [fftw](#port-fftw)'s single-precision
  transforms, for room correction and headphone impulse responses; without it the convolver uses
  its bundled pffft.
- `-Dopus=enabled` — [opus](#port-opus) for the Bluetooth codec above and, through Opus's custom
  modes, for the NetJack2 streaming modules.
- `-Dgstreamer=enabled` — Build the GStreamer elements, `pipewiresrc` and `pipewiresink`. The
  first is how anything reads a PipeWire node from a pipeline; `kdos-record` depends on it to read
  the desktop. The cost is that every image with sound also carries
  [gstreamer](#port-gstreamer) and [gst-plugins-base](#port-gst-plugins-base), because a port is
  one package.
- `-Dgstreamer-device-provider=enabled` — The GStreamer device provider, which lists PipeWire's
  nodes to a GStreamer application as devices.
- `-Djack=disabled` — No SPA plugin that makes PipeWire a client of a separate JACK server; there
  is no JACK server.
- `-Dpipewire-jack=enabled`, `-Djack-devel=true`, `-Dlibjack-path=/usr/lib` — Build PipeWire's own
  `libjack.so.0` and install it, with JACK's headers and `jack.pc`, in `/usr/lib`, so a JACK
  program loads it from the default search path and needs no `pw-jack`; see
  [JACK programs on the host](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#jack-programs-on-the-host).
- `-Dpipewire-v4l2=disabled` — No `pw-v4l2` shim. It is an `LD_PRELOAD` library that looks up
  `openat64` and `mmap64` with `dlsym`; musl exports no such large-file aliases, so both lookups
  return NULL, the shim calls them anyway, and every process started under it crashes on its first
  `open`.
- `-Dv4l2=enabled` — The SPA plugin that puts a V4L2 camera on the graph, which WirePlumber's
  `api.v4l2.*` rules manage.
- `-Dlibcamera=enabled` — The SPA plugin for cameras driven through [libcamera](#port-libcamera).
- `-Dvulkan=disabled` — No Vulkan compute source or blit filter. No graph or session
  configuration on KDOS loads them.
- `-Droc=disabled` — No ROC network streaming modules. roc-toolkit is not in the tree.
- `-Dlv2=enabled` — Let a filter chain host LV2 plugins through [lilv](#port-lilv).
- `-Dsndfile=enabled`, `-Dpw-cat=enabled` — Build `pw-cat`, `pw-play` and `pw-record`, which need
  [libsndfile](#port-libsndfile), and `spa-resample`, which resamples a sound file with
  PipeWire's own resampler.
- `-Davahi=enabled` — Publish and discover PipeWire and PulseAudio servers and AirPlay receivers on
  the local network with [avahi](#port-avahi).
- `-Draop=enabled` — The RAOP sink, which plays to an AirPlay receiver; its encryption comes from
  [openssl](#port-openssl).
- `-Decho-cancel-webrtc=enabled` — The echo canceller for calls, built on
  [webrtc-audio-processing](#port-webrtc-audio-processing).
- `-Dlibmysofa=enabled` — The filter chain's `sofa` spatializer, which places a sound around a
  listener on headphones from a SOFA head-related transfer function file, read with
  [libmysofa](#port-libmysofa).
- `-Debur128=enabled` — The EBU R128 loudness meter and normaliser, from
  [libebur128](#port-libebur128).
- `-Donnxruntime=disabled` — No neural-model filter node. No graph on KDOS loads one.
- `-Dlibffado=disabled` — No FireWire audio driver module. libffado is not in the tree.
- `-Dlibsystemd=disabled`, `-Dlogind=disabled`, `-Dsystemd-system-service=disabled`,
  `-Dsystemd-user-service=disabled` — No systemd code and no unit files. KDOS has no systemd, and
  the session starts PipeWire itself.
- `-Dsdl2=disabled` — SDL 2 is used only by the examples, which are off.
- `-Dx11=disabled`, `-Dx11-xfixes=disabled`, `-Dlibcanberra=disabled` — No X11 bell module and
  nothing else that serves an X session. The desktop is Wayland, and libcanberra feeds only that
  X11 bell.
- `-Dflatpak=disabled`, `-Dsnap=disabled` — No sandbox detection for Flatpak or Snap, neither of
  which KDOS uses; boxed applications reach PipeWire through the shared runtime directory.
- `-Dgsettings=disabled`, `-Dgsettings-pulse-schema=disabled` — No GSettings module and no
  GNOME schema for it.
- `-Davb=enabled` — The AVB module, which streams audio over Ethernet with IEEE 1722.
- `-Drlimits-install=false` — Install no PAM `limits.d` file. Nothing on KDOS reads one: `login`
  is built without PAM, and the file's rule matches a group, `@pipewire`, that does not exist.
  The real-time limits PipeWire needs are raised by `kdos-getty` instead, before the login, and
  every process of the session inherits them; see
  [The console](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#the-console).
- `"-Dsession-managers=[]"` — Build no session manager as a meson subproject. Upstream's default,
  `['wireplumber']`, builds WirePlumber from a git checkout inside this build, which an offline
  build cannot fetch; KDOS builds it as its own port, [wireplumber](#port-wireplumber).

## Notes
Once `jack.pc` is installed, any port that leaves JACK support on auto-detection gets it or not
according to whether it happens to build after this one. Every port that can link JACK therefore
names it on or off, and a port this one depends on, such as ffmpeg, cannot have it at all. The
list is in
[JACK programs on the host](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/session.md#jack-programs-on-the-host).

The daemon routes nothing by itself. Without WirePlumber running, devices are not discovered and
no stream reaches a sink.
