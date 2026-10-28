## About

libpulse is the client half of the [PulseAudio]{.idx} tree and nothing else: the `libpulse`,
`libpulse-simple` and `libpulse-mainloop-glib` libraries, `pactl`, and `pacat` with its `paplay`,
`parec`, `parecord` and `pamon` links. The server on KDOS is `pipewire-pulse`, which speaks the
same protocol on `$XDG_RUNTIME_DIR/pulse/native`, so a program written for PulseAudio links this
library and talks to PipeWire without knowing it; see
[Audio](../../../../docs/kdos/03-architecture/session.md#audio).

## Flags

- `LDFLAGS="$LDFLAGS -lintl"` — Links `libintl` by name. The tree has no switch to leave
  translation out; its probe finds musl's own `dgettext()` and adds no library, while the
  `gettext` port's `libintl.h` renames every call to `libintl_dgettext`, which only the `libintl`
  port defines, so without it the first program fails to link. It rides `LDFLAGS` because
  `-Dc_link_args` would replace the build environment's `LDFLAGS` rather than add to them.
- `-Ddaemon=false` — Build no server. `-Ddaemon=true` would install a second PulseAudio server
  that fights `pipewire-pulse` for the socket.
- `-Dclient=true` — Build the client libraries and utilities, upstream's default, named because
  it is the half this port exists for.
- `-Ddoxygen=false`, `-Dtests=false`, `-Dgcov=false` — No API reference, no unit tests and no
  coverage instrumentation.
- `-Dman=true` — Build the manual pages of the client tools. They are converted from XML by
  upstream's `xmltoman`, a Perl script using `XML::Parser`, which is why `perl` and
  `perl-xml-parser` are dependencies.
- `-Dipv6=true` — Let a client reach a server address given as an IPv6 host.
- `-Ddbus=enabled` — Lets a client notice a PulseAudio server that appears on the session or
  system bus; `enabled` makes a missing `dbus` a setup error.
- `-Dglib=enabled` — Builds `libpulse-mainloop-glib`, the GLib main-loop adapter most graphical
  consumers link.
- `-Dasyncns=disabled` — Resolve server host names synchronously instead of through
  `libasyncns`, which the tree does not carry.
- `-Dgtk=disabled` — GTK 3 is used only by one of the unit tests, which are off.
- `-Dx11=disabled` — Leave out `pax11publish` and the client's reading of the server address and
  cookie from X11 root-window properties. The session is Wayland; see
  [no Xorg server](../../../../docs/kdos/01-philosophy/principles.md#no-xorg-server-and-one-carve-out).
- `-Dsystemd=disabled`, `-Delogind=disabled`, `-Dconsolekit=disabled` — KDOS has
  [no systemd](../../../../docs/kdos/01-philosophy/principles.md#no-systemd), and neither of the
  other two session trackers is a port.
- `-Doss-output=disabled` — Builds no `padsp`, the `LD_PRELOAD` wrapper that turns OSS calls into
  PulseAudio ones. It resolves the functions it passes through by their large-file alias names,
  which musl does not export.
- `-Dalsa=disabled`, with `-Davahi`, `-Dbluez5`, `-Dbluez5-gstreamer`, `-Dfftw`, `-Dgsettings`,
  `-Dgstreamer`, `-Djack`, `-Dlirc`, `-Dopenssl`, `-Dorc`, `-Dsamplerate`, `-Dsoxr`, `-Dspeex`,
  `-Dtcpwrap`, `-Dudev` and `-Dwebrtc-aec`, all `=disabled` — Every one of these is a feature of
  the daemon or its modules: card access, network discovery, Bluetooth, the equalizer,
  resamplers, echo cancellation and the rest. With the daemon off they have nothing to build;
  they are named all the same, for the reason in the note below.
- `-Dvalgrind=disabled` — Leave out the Valgrind memcheck annotations, which the code shared
  with the client libraries compiles in when `valgrind/memcheck.h` is found.
- `-Dbashcompletiondir=/usr/share/bash-completion/completions`,
  `-Dzshcompletiondir=/usr/share/zsh/site-functions` — Fix where the `pactl` and `pacat`
  completions go. Left empty, the bash directory is read from `bash-completion`'s `pkg-config`
  file when that port happens to be installed first.

## Notes

- **Every feature is named rather than left to `auto`.** An enabled one whose library is missing
  fails the setup instead of producing a narrower `libpulse` without a word, and a disabled one
  stays off whatever else happens to be installed when the port builds.
