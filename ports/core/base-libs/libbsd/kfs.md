## About

libbsd provides functions and headers found on the BSDs and missing from the Linux C libraries —
`strlcpy()`, `reallocarray()`, `arc4random()`, `setproctitle()`, `<sys/queue.h>`,
`<sys/tree.h>` and the rest — so that software written for them builds unchanged. In KDOS,
`samba` and its `talloc`, `tdb` and `tevent`, `xwayland`, `libICE`, `libXdmcp`, `libtirpc`,
`netcat`, `zeromq` and others link it. Its hash functions come from `libmd`.

## Flags

- `CFLAGS="$CFLAGS -Doff64_t=off_t"` — Defines `off64_t` as `off_t` for the whole build. musl
  defines no `off64_t`, and its `off_t` is 64 bits, so the two name the same type. No source file
  in this release names `off64_t` (`funopen.c` uses `off_t`), so the definition changes nothing
  in what is compiled.

## Patches

- `sys-cdefs-no-self-include.patch` — Not applied to the source but to the installed copy at `$PKG/usr/include/sys/cdefs.h` (see the note below): it comments out that file's own `#include <sys/cdefs.h>`. There the file is itself `<sys/cdefs.h>`, and the include, which comes before its include guard, would include itself without end.

## Notes

- **Three headers are copied into `/usr/include/sys`.** musl has no `<sys/cdefs.h>`,
  `<sys/queue.h>` or `<sys/tree.h>`, which a good deal of BSD-derived code includes by those
  names. `build.sh` copies libbsd's versions of the three from `/usr/include/bsd/sys` to
  `/usr/include/sys` after the install, so they resolve as system headers; the patch then makes
  the copied `cdefs.h` safe in that place.
