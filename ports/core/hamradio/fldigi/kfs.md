## About

fldigi is a sound-card modem for amateur radio's digital modes: PSK31 and its relatives, RTTY,
Olivia, Contestia, MFSK, Thor, Hellschreiber, CW and many more, with a logbook, a macro editor and
rig control through [hamlib](#port-hamlib) or [flrig](#port-flrig). The package also builds
flarq, its ARQ file and message transfer companion. Both are FLTK programs.

## Flags

- `--disable-oss` — Leave out the OSS audio back end. Audio goes through
  [portaudio](#port-portaudio) and PulseAudio, which PipeWire answers.
- `--enable-tls` — Require compiler thread-local storage (`__thread`) rather than probe for
  it. This TLS is not transport security.
- `--with-sndfile` — Require each of these libraries instead of autodetecting it, so a missing
  one fails the configure: [libsndfile](#port-libsndfile) for audio files, `--with-pulseaudio`
  for [libpulse](#port-libpulse), `--with-hamlib` for rig control, and `--with-flxmlrpc` for
  the [flxmlrpc](#port-flxmlrpc) port instead of the XML-RPC copy in the source.
- `--without-libmbedtls` — Build the mbed TLS copy bundled in `src/` instead of linking the
  [mbedtls](#port-mbedtls) port. The network code is written against the 2.x API, which has
  `mbedtls/net.h` and `mbedtls/certs.h`; the port is the 3.x line, which has neither.
- `--without-asciidoc` — Build no manual through asciidoc.

## Patches

- `nullptr-def.patch` — Gentoo's patch: delete the block in `src/logbook/table.cxx` that defines `nullptr` as a macro for `NULL`, a fallback for compilers older than GCC 4.6. The keyword is then the compiler's own.
- `x11-hints-on-x11-only.patch` — In flarq, make the Xlib window-hint calls that set the icon's transparency mask only when `fl_x11_display()` reports an open X11 display. FLTK chooses Wayland or X11 when the program starts, and under Wayland there is no X11 display: `fl_display` is NULL.

## Notes

Upstream's icons are XPM and its entries carry no window class, so `build.sh` converts both icons
to hicolor PNGs and replaces both entries. FLTK takes the Wayland app id from the window's X class,
and fldigi and flarq both set it to the package name, `fldigi`, so flarq's entry also says
`StartupWMClass=fldigi`: an entry naming any other class would never match its own window.
