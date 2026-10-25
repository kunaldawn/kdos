## About
plocate finds files by name in a prebuilt index rather than by walking the disk: `updatedb` builds
the index, and `locate` (a link to `plocate`) searches it. Upstream's design is a setgid program reading one shared index of the whole machine.
KDOS keeps an index per user instead, built from that user's home directory into their own cache,
and ships the binary without a setgid bit; the pieces (`kdos-updatedb`, the nightly timer and the
`LOCATE_PATH` that `/etc/profile.d/40-plocate.sh` exports) are described in
[No setgid binaries, and a per-user locate index](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#no-setgid-binaries-and-a-per-user-locate-index).

## Flags
- `-Dinstall_systemd=false` — Install no systemd service and timer for `updatedb`. Upstream's
  default is on, and installs them when it finds systemd; KDOS has no systemd, and the per-user
  index is rebuilt by a session timer.
- `-Dinstall_cron=false` — Install no `/etc/cron.daily/plocate` script (it converts an mlocate
  database). Off is also upstream's default.

## Patches
- `musl-statx.patch` — Removes `#include <linux/stat.h>` from `io_uring_engine.h`. musl's `<sys/stat.h>`, which `liburing.h` includes, already declares `struct statx` and the `STATX_*` masks, so the kernel header adds a second definition and every file that includes `io_uring_engine.h` fails to compile.
- `locate-path-replaces-default.patch` — Makes a set, non-empty `LOCATE_PATH` replace the compiled-in database, `/var/lib/plocate/plocate.db`, instead of being searched after it. plocate exits on the first database it cannot open, so with the default kept in front every search would fail before the user's own index is read. A database given with `-d` still takes the default's place, and `LOCATE_PATH` is searched after it.

## Notes
The release tarball is flat, with no wrapping directory, so the top-level strip kpkg applies to a
first source would leave no `meson.build`; `build.sh` unpacks the tarball again into `$SRC` itself
(see [Sources, and what each one becomes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#sources-and-what-each-one-becomes)).

`meson install` installs `plocate` mode `rwxr-sr-x`, group `plocate`, and an install script creates
`/var/lib/plocate` with a `CACHEDIR.TAG` in it. `build.sh` resets the binary to 0755 and deletes the
directory afterwards: the image carries no setgid file and no `plocate` group, and a shared database
directory would hold nothing. The `--require-visibility no` that `kdos-updatedb` passes depends on
this: with the visibility check on, `updatedb` looks for the `plocate` group and writes nothing.

`plocate` uses io_uring through [liburing](#port-liburing) when meson finds it, and is built
without it otherwise, with no error; the `depends` line is what guarantees it.
