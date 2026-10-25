## About
GNU patch applies a diff to the files it names. In KDOS it is the program that applies every patch
kept beside a recipe: kpkg applies none by itself, and a `*.patch` reaches its source only through
a `patch -p1` line in the port's own `build.sh` (see
[Anatomy of a kpkg install](#04-chroot-kpkg-install)). Until this port is installed in
`30_foundation`, the copy the bootstrap built does that job; this port adopts its file.

The port differs from the bootstrap build in one feature: it links libattr, so a file `patch`
rewrites keeps its extended attributes.

## Flags
- `--enable-xattr` — Copy the original file's extended attributes onto the file `patch` writes in
  its place, through libattr's `attr_copy_file` and `attr_copy_fd`. Those copy every attribute
  except the ones `/etc/xattr.conf` marks `skip` (see [attr](#port-attr)). It is upstream's
  default, and configure only warns and builds without it when libattr is missing; the recipe's
  `depends = attr` is what makes the library present, so the switch takes effect.

## Notes
When a hunk applies only at an offset or with fuzz, or fails, `patch` keeps the original as
`<file>.orig` beside the patched file. A recipe whose `make install` copies a whole source directory then ships that `.orig`, and
the package lint refuses it; such a recipe applies its patch with
`patch -p1 --no-backup-if-mismatch`, as [cmake](#port-cmake) does (see
[What the package lint refuses](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).
