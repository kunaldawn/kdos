## About

Hamlib is the common control library for amateur radio equipment: one API, and one command-line
program per kind of device (`rigctl` for radios, `rotctl` for antenna rotators, `ampctl` for
amplifiers), over backends for a few hundred models. The radio-control features of
[wsjtx](#port-wsjtx), [fldigi](#port-fldigi), [gpredict](#port-gpredict), [direwolf](#port-direwolf)
and the other SDR and digital-mode ports go through it. A radio is usually on a USB serial adapter,
which the `dialout` group and the FTDI, Prolific, CP210x and CH341 udev rules in the tree already
cover;
hamlib is the program those rules are there for on the radio side.

The C library and its programs are the whole package: every language binding is off.

## Flags

- `-include sys/types.h` — Compile every file as though it began with `#include <sys/types.h>`.
  The Harris backend's `harris.h` declares a function taking a `mode_t` and includes no header
  that defines the type. glibc defines it through another header on the way; musl does not, so
  without this that backend fails to compile.
- `ac_cv_func_select=yes` — Tell `configure` that `select()` exists instead of letting it probe.
  `configure` checks for a function by compiling a program that declares it as `char select ();`.
  With `<sys/types.h>` forced into every compile, musl's `sys/types.h` includes `<sys/select.h>`,
  whose real declaration of `select()` conflicts with that one, so the probe fails to compile and
  `configure` concludes there is no `select()`. The `grep` for `#define HAVE_SELECT 1` in
  `include/hamlib/config.h` after `configure` stops the build if the answer is ever not yes.
- `--without-cxx-binding`, `--without-python-binding`, `--without-tcl-binding`,
  `--without-perl-binding`, `--without-lua-binding` — Build none of hamlib's language bindings.
  The C++ one is built by default whenever a C++ compiler is found; the others go through SWIG.
- `--with-libusb` — Build the backends for USB-attached devices, linking [libusb](#port-libusb).
- `--with-xml-support` — Build `rigmem` with XML support, through [libxml2](#port-libxml2), so it
  can save a radio's memory channels to a file and load them back. It is off by default.
- `--without-indi` — Build no INDI rotator backend; the INDI library is not a port. Its default
  is to detect it.
- `--disable-html-matrix` — Do not generate the HTML feature matrix of the supported radios, which
  needs libgd.
- `--disable-pytest` — Changes nothing in this release: `configure.ac` handles the option by
  setting it to `check` whatever value is given, so `configure` still looks for `pytest`. The
  result is harmless here, because `pytest` only runs the Python binding's tests under
  `make check`, the binding is off, and `build.sh` runs no `make check`.
