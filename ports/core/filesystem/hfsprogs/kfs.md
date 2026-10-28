## About
hfsprogs is `mkfs.hfsplus` and `fsck.hfsplus`, the tools that create and check Apple's HFS+
filesystem. They are `newfs_hfs` and `fsck_hfs` from Apple's `diskdev_cmds`, in the Linux port that
Fedora distributes as hfsplus-tools, installed under the names `fsck(8)` and
[gparted](#port-gparted) look up. `fsck.hfs` is a link to `fsck.hfsplus`.

## Flags
- `-fblocks` — Enables C blocks, the closure extension Apple's code is written with. Only clang
  compiles it, which is why [clang](#port-clang) is in `depends`: upstream's makefile sets
  `CC := clang` itself, and the build does not override it.
- `-I$PWD/BlocksRunTime` — The blocks runtime headers. The source carries its own small runtime in
  `BlocksRunTime/`, built first as `libBlocksRunTime.a` and linked into `fsck_hfs` through the
  `-L$PWD/BlocksRunTime` in `LDFLAGS`.
- `-DLINUX=1`, `-DBSD=1` — `BSD` selects the Unix code over the classic Mac OS code that
  includes the Toolbox headers (`MacTypes.h`, `Files.h`), and `LINUX` selects the Linux paths over
  the Darwin ones within it (no IOKit, no CoreFoundation, no `sys/disk.h`).
- `'-D__P(x)=x'` — Defines `__P()`, the K&R prototype macro the sources declare their functions
  through. glibc's `<sys/cdefs.h>` provides it; the `<sys/cdefs.h>` this build finds is libbsd's,
  which does not. The single quotes keep the parentheses intact through the shell make runs each
  compile in.
- `-DVERSION` — The version string `fsck.hfsplus -v` prints, set to upstream's own spelling of the
  version, which the recipe's `_upver` helper rebuilds from `version`.
- `-D_FILE_OFFSET_BITS=64`, `-DDEBUG_BUILD=0` — Upstream's own makefile flags, repeated because
  the command line replaces the makefile's `CFLAGS` whole.

## Patches
- `fix-stdarg.patch` — Fedora's patch to `fsck_hfs`'s in-memory log, from the hfsplus-tools package its `Patch-Source:` line names. In upstream's source `print_to_mem()` takes a type code, a format, one string and a `va_list`, and each caller passes `NULL` for whichever of the last two it does not use. The patch splits it into `vprint_to_mem(fmt, ap)` and a variadic `print_to_mem(fmt, ...)` and moves every caller to the one that fits, so no `NULL` is ever passed where a `va_list` is expected.
- `musl-compat.patch` — Lets the sources compile against musl: removes every `#include <sys/sysctl.h>`, a header musl does not have; includes `<errno.h>` in place of `<sys/errno.h>`; adds `#include <sys/types.h>` to `cache.h` and `#include <sys/cdefs.h>` to `fsck_hfs.h`, `makehfs.c` and `newfs_hfs.c`; and spells glibc's `__va_copy` as the standard `va_copy`.

## Notes
`CFLAGS` and `LDFLAGS` on the `make` command line are the tree's exported flags followed by every
entry of upstream's own except `-g3`, plus the `__P` definition. The top-level makefile exports both to the per-directory
makefiles, so a flag left off this line is missing from every compile.

The tarball unpacks to `diskdev_cmds-<upstream version>`, and the source is fetched from Fedora's
lookaside cache, whose URL carries the file's MD5; the recipe keeps it in the `_md5` helper, and a
version bump changes both.

`<sys/cdefs.h>` is not part of musl. The copy the compile finds in `/usr/include/sys` is installed
by [libbsd](#port-libbsd), which the recipe's `depends` does not name.
