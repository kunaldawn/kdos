## About

Tlf is a contest logger for amateur radio, drawn with curses in a terminal. It checks each contact
for duplicates, tracks multipliers and scores, controls the radio through
[hamlib](#port-hamlib), and writes the Cabrillo file a contest asks for at the end. Everything
except the DX-cluster feed works with no network: the log is local, and the Cabrillo file is a
local file.

## Flags

- `-largp` — Link `libargp`, from [argp-standalone](#port-argp-standalone). tlf parses its command
  line with `argp_parse()`, which is a glibc extension that musl does not implement. The flag goes
  in `LIBS`, not `LDFLAGS`: automake's link rule puts `$(LDFLAGS)` before the object files and
  `$(LIBS)` after them, and `libargp` is a static archive, which resolves only the symbols that
  objects named before it need. In `LDFLAGS` the link fails with `undefined reference to
  argp_parse` with the library on the command line. `configure` reads `LIBS` for its own test
  links too.
- `--enable-fldigi-xmlrpc` — Talk to fldigi over XML-RPC, through [xmlrpc-c](#port-xmlrpc-c), so
  tlf can read fldigi's carrier frequency in the RTTY and digital modes. It is off by default.

## Patches

- `hamlib4-and-musl.patch` — Two changes, neither available as a flag. In `src/sendqrg.c`, it replaces `FILPATHLEN` with `HAMLIB_FILPATHLEN`: hamlib's public macros carry a `HAMLIB_` prefix, and tlf's release uses the unprefixed name, which the installed `rig.h` does not define. In `src/sockserv.c`, it adds `#include <sys/select.h>` for `FD_SETSIZE` and `fd_set`, which glibc's `<sys/socket.h>` brings in on the way and musl's does not.

## Notes

The release is a tag archive with `configure.ac` and no generated `configure`, so `build.sh` runs
`autoreconf -fi` first.
