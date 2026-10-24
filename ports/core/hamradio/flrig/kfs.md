## About

flrig controls an amateur radio transceiver over its CAT interface: frequency, mode, filters,
power and PTT, from an FLTK window. It also serves the rig over XML-RPC, which is how
[fldigi](#port-fldigi) and the other W1HKJ programs share one radio.

## Patches

- `no-error-h.patch` — Remove the `#include <error.h>` from `src/cmedia/cmedia.cxx` and `src/cmedia/tmate2.cxx`. The header is glibc's and musl has none; neither file calls anything it declares.

## Notes

Upstream's icon is an XPM, which the panel does not read, and its desktop entry carries no window
class. `build.sh` converts the XPM to a hicolor PNG at its own size with ImageMagick and replaces
the entry. FLTK takes the Wayland app id from the main window's X class, which for flrig is
`Flrig`, so that is the entry's `StartupWMClass`.
