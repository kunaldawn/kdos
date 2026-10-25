## About

Dasher writes text without a keyboard. Letters stream towards the pointer in boxes sized by a
predictive language model, and steering into a box writes its letter, so a mouse, an eye tracker,
a head pointer or a single switch is enough to type. It is a GTK 3 application. Its direct mode
types into other applications through AT-SPI ([at-spi2-core](#port-at-spi2-core)), and it can
speak what has been written through [speech-dispatcher](#port-speech-dispatcher). The source is
upstream's git tree at the commit `_commit` names.

## Flags

- `--enable-atspi` — Direct mode sends its characters to the focused application through AT-SPI 2
  rather than through the X test extension. Named, configure stops with "atspi 2 requested but not
  found" when the library is missing, instead of falling back.
- `--enable-speech=speechdispatcher` — Text-to-speech through speech-dispatcher's `libspeechd`.
  Left unset, configure only warns when no speech module is found and builds without one; naming
  the module makes its absence an error.
- `--with-gsettings` — Keep the user's preferences in GSettings. Named, configure stops with
  "GSettings requested but not found" instead of falling back to GConf or, without it, to a Dasher
  that stores no preferences.
- `--with-cairo` — Draw with cairo; upstream's default, named so it cannot change.
- `--without-gnome` — Leave out `Data/Help/Gnome`, the GNOME help: a DocBook manual whose
  makefile is built from yelp-tools' `YELP_HELP_RULES`. yelp-tools is not a port.
- `--disable-japanese`, `--disable-joystick`, `--disable-tilt` — Leave out Kanji entry (which needs
  the Canna library, not a port), Linux joystick input and the experimental tilt-sensor input.
  All three are off by default upstream; naming them keeps them off whatever those defaults become.

## Patches

- `atspi-no-extern-c.patch` — A git commit, 5eed251f, carried with its `commit` and `Author:` lines rather than a mail header: drops the `extern "C" { }` wrapped around `#include <atspi/atspi.h>` in `Src/Gtk2/dasher_editor_external_atspi.cpp`. GLib's headers use C++ templates when compiled as C++, and a template inside C linkage is a compile error.
- `x11-backend.patch` — Debian's patch (its header is DEP-3, not a mail header): calls `gdk_set_allowed_backends("x11")` in `Src/main.cc` before GDK starts. Dasher reads the pointer position with X11 calls, which do not work on the Wayland backend, so the window always opens under Xwayland. Direct mode reaches only X11 windows even then.

## Notes

The tarball is a git snapshot with no `configure`, so `build.sh` runs `autoreconf -fi`. Before
that it writes `$version` into `.tarball-version`: `AC_INIT` takes the version from
`build-aux/mkversion`, which prints that file when it exists and otherwise runs `git describe`,
which has no repository to read here.

`build.sh` writes its own `dasher.desktop` over the one upstream installs, with `TryExec`,
`StartupWMClass=dasher` and search keywords.
