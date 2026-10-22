## About
GNU coreutils is the everyday command line: `ls`, `cp`, `mv`, `install`, `sort`, `stat`, `date`,
`df` and the rest, each installed as a program of its own. KDOS has no multi-call
userland, so every one of these names on the image is GNU's, and every configure script and
`build.sh` meets the behaviour upstream build systems were written against
([The base userland](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#the-base-userland)).

`10_bootstrap` cross-compiles the same source with no optional libraries
([Coreutils](#03-bootstrap-062-coreutils)). This port rebuilds it natively in the first run of
`30_foundation`, with GMP, ACLs, extended attributes, capabilities and utmps, and takes over the
files the bootstrap installed.

## Flags
- `FORCE_UNSAFE_CONFIGURE=1` — Let configure run as root. Its `mknod` test stops with "you should
  not run configure as root" unless this is set, and kpkg builds every port as root inside the
  chroot.
- `fu_cv_sys_stat_statvfs=n` — Answer configure's `statvfs` check with "no", so `df` and `stat -f`
  use `statfs`. musl's `statvfs` keeps only 32 bits of the file system id, and `stat -f -c %i`
  would print an id no other tool agrees with.
- `--disable-nls` — Install no message catalogues; the programs speak English only.
- `--without-openssl` — Use gnulib's own digest code in `md5sum`, `sha256sum`, `cksum` and the
  other checksum programs, and in `sort --random-sort`. Left at its default, `auto-gpl-compat`,
  configure takes those hashes from OpenSSL's `libcrypto` whenever it finds OpenSSL 3 or later.
- `--without-selinux` — No SELinux contexts in `ls -Z`, `cp --context` and the rest.
- `--with-libgmp` — Build `expr` and `factor` against [gmp](#port-gmp), and stop configure if
  GMP is missing rather than build them against gnulib's smaller arbitrary-precision copy.
- `--enable-acl` — Preserve POSIX ACLs in `cp -a`, `cp --preserve` and `mv` across file systems,
  through [acl](#port-acl), and let `ls -l` mark a file that carries one with `+`.
- `--enable-xattr` — Preserve extended attributes in the same copies. Without it they copy the data
  and drop the attributes without a message.
- `--enable-libcap` — Let `ls --color` colour files that carry file capabilities (the `ca`
  entry of `LS_COLORS`), through [libcap](#port-libcap).
- `--enable-install-program=arch,hostname` — Also install `arch` and `hostname`, the two programs
  coreutils leaves out unless asked for.
- `CPPFLAGS=-I/usr/include/utmps`, `LIBS="-lutmps -lskarnet"` — Build `who`, `users` and `pinky`
  against [utmps](#port-utmps), so they read the login records its daemon keeps. musl's
  own utmp functions are stubs that report no one
  ([Login records](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#login-records)).

## Notes
The libraries the bootstrap build goes without (GMP, acl, attr, libcap, utmps) are not in
`build/fs` while `10_bootstrap` runs: that build switches off the first four and passes no utmps
flags. The recipe's `depends` installs all five before this port in `30_foundation`.
