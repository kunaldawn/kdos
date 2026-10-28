## About
jfsutils is the user-space toolset for IBM's JFS filesystem: `mkfs.jfs` (`jfs_mkfs`) to create
one, `fsck.jfs` (`jfs_fsck`) to check and repair it, `jfs_tune` to change its parameters,
`jfs_debugfs`, and `jfs_fscklog` and `jfs_logdump` to read the check log and the journal.
[gparted](#port-gparted) runs these tools for the JFS operations it offers. Each patch makes the
source build with a current compiler or against musl, or fixes a small defect; `build.sh` applies
them in the order its `for` loop names them.

## Patches
- `format-security.patch` — Passes the message text in `fscklog` and `logdump` to `printf` as an argument (`printf("%s", msg)`) rather than as the format string. As a format, any `%` in the text is read as a conversion and `printf` reads arguments that were never passed; `-Wformat-security` warns about every such call.
- `musl-fix-includes.patch` — Includes `<paths.h>` in `libfs/fssubs.c` unconditionally rather than only on systems with `getmntinfo()`. Its `<mntent.h>` code opens `_PATH_MNTTAB`, which glibc's `<mntent.h>` brings in through `<paths.h>` and musl's does not.
- `jfsutils-include-sysmacros.patch` — Includes `<sys/sysmacros.h>` in `libfs/devices.c`, where `major()` and `minor()` are declared; `<sys/types.h>` does not provide them.
- `missing-stdinth.patch` — Includes `<stdint.h>` in `fscklog/extract.c`, for the fixed-width integer types it uses.
- `gcc-10.patch` — Debian's patch, from the source its `Origin:` line names: marks the global variables in `fscklog/display.c` and `libfs/logredo.c` `static`, and makes `mntcnt` an `extern` declaration. Several files define the same names, which links only under `-fcommon`; GCC 10 and later default to `-fno-common` and stop with "multiple definition".
- `fflush-stdout.patch` — Flushes standard output after `mkfs.jfs` prints its "Continue? (Y/N)" prompt, so the prompt appears before the program waits for the answer instead of sitting in the buffer.
- `gnu-basename.patch` — Defines `basename()` as a macro, in `fsck/fsck_message.c` and `fscklog/fscklog.c`, that returns the part of a path after its last `/`. The code calls it on `const` file names in the manner of glibc's GNU `basename`, which `<string.h>` declares under `_GNU_SOURCE`; musl has only the POSIX `basename` in `<libgen.h>`, which may modify its argument.

## Notes
configure stops unless `<uuid/uuid.h>` is present; it comes from libuuid in
[util-linux](#port-util-linux), the one entry in `depends`.

`make -j1 DESTDIR=$PKG install` runs the install serially; the build itself uses the tree's
parallel `make`.
