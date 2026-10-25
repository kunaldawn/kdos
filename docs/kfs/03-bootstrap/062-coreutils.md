# Coreutils

GNU coreutils is the basic command line: `ls`, `cp`, `mv`, `rm`, `cat`, `mkdir`, `chmod`, `sort`,
`install` and about a hundred more. Every `build.sh` that `20_selfhost` runs uses them, so they are
built here with no optional library at all, and replaced by the full port in `30_foundation`. This
page shows the commands of `script/phases/10_bootstrap/062_coreutils.sh` and explains each switch.

**Source:** the archive of the [coreutils](#port-coreutils) port\
**Installs into:** `build/fs/usr`\
**Approximate build time:** 0.9 SBU

## Installation of coreutils

<!-- kfs:include script/phases/10_bootstrap/062_coreutils.sh lines=23-41 pin=6f6bfeeb1a74 -->

### The meaning of the configure options

`FORCE_UNSAFE_CONFIGURE=1`
: coreutils' configure refuses to run as root unless this is set. As for
  [tar](061-tar.md#the-meaning-of-the-configure-options), the refusal comes out of a test program a
  cross build never runs, so here it changes nothing; the port, built natively as root, needs it.

`fu_cv_sys_stat_statvfs=n`
: Answer configure's `statvfs` check in advance with "no", so `df` and `stat -f` read file system
  figures with `statfs` instead. musl's `statvfs` keeps only 32 bits of the file system id, so with
  it `stat -f -c %i` would print an id whose high half is zero, matching no other tool's id for the
  same file system. The port sets the same answer; see
  [The base userland](../../kdos/03-architecture/packaging.md#the-base-userland).

`--host=$KDOS_TARGET`, `--build=$(build-aux/config.guess)`, `--prefix=/usr`
: A cross build, installed under `/usr` (see [Part III](intro.md#the-shape-of-a-step)).

`--disable-nls`
: Build no message catalogues.

`--without-openssl`
: Use coreutils' own digest code in `sha256sum` and the rest. configure links libcrypto into every
  program, not only the checksum ones, whenever it finds it.

`--without-selinux`
: Build no SELinux support.

`--without-libgmp`
: Build `expr` and `factor` with gnulib's own arbitrary-precision arithmetic. GMP is not in the
  root.

`--disable-acl`, `--disable-xattr`, `--disable-libcap`
: Build without access control lists, extended attributes and file capabilities. Their libraries,
  libacl, libattr and libcap, are not in the root. `cp -a` from this build copies data, modes,
  owners and times, and nothing more.

`--enable-install-program=arch,hostname`
: Also install `arch` and `hostname`, which coreutils leaves out by default.

## Contents of coreutils

**Installed programs:** `[`, [`arch`]{.idx}, [`b2sum`]{.idx}, [`base32`]{.idx}, [`base64`]{.idx}, [`basename`]{.idx}, [`basenc`]{.idx}, [`cat`]{.idx},
[`chgrp`]{.idx}, [`chmod`]{.idx}, [`chown`]{.idx}, [`chroot`]{.idx}, [`cksum`]{.idx}, [`comm`]{.idx}, [`cp`]{.idx}, [`csplit`]{.idx}, [`cut`]{.idx}, [`date`]{.idx}, [`dd`]{.idx}, [`df`]{.idx},
[`dir`]{.idx}, [`dircolors`]{.idx}, [`dirname`]{.idx}, [`du`]{.idx}, [`echo`]{.idx}, [`env`]{.idx}, [`expand`]{.idx}, [`expr`]{.idx}, [`factor`]{.idx}, [`false`]{.idx}, [`fmt`]{.idx},
[`fold`]{.idx}, [`groups`]{.idx}, [`head`]{.idx}, [`hostid`]{.idx}, [`hostname`]{.idx}, [`id`]{.idx}, [`install`]{.idx}, [`join`]{.idx}, [`link`]{.idx}, [`ln`]{.idx}, [`logname`]{.idx},
[`ls`]{.idx}, [`md5sum`]{.idx}, [`mkdir`]{.idx}, [`mkfifo`]{.idx}, [`mknod`]{.idx}, [`mktemp`]{.idx}, [`mv`]{.idx}, [`nice`]{.idx}, [`nl`]{.idx}, [`nohup`]{.idx}, [`nproc`]{.idx},
[`numfmt`]{.idx}, [`od`]{.idx}, [`paste`]{.idx}, [`pathchk`]{.idx}, [`pinky`]{.idx}, [`pr`]{.idx}, [`printenv`]{.idx}, [`printf`]{.idx}, [`ptx`]{.idx}, [`pwd`]{.idx}, [`readlink`]{.idx},
[`realpath`]{.idx}, [`rm`]{.idx}, [`rmdir`]{.idx}, [`seq`]{.idx}, [`sha1sum`]{.idx}, [`sha224sum`]{.idx}, [`sha256sum`]{.idx}, [`sha384sum`]{.idx}, [`sha512sum`]{.idx},
[`shred`]{.idx}, [`shuf`]{.idx}, [`sleep`]{.idx}, [`sort`]{.idx}, [`split`]{.idx}, [`stat`]{.idx}, [`stdbuf`]{.idx}, [`stty`]{.idx}, [`sum`]{.idx}, [`sync`]{.idx}, [`tac`]{.idx}, [`tail`]{.idx},
[`tee`]{.idx}, [`test`]{.idx}, [`timeout`]{.idx}, [`touch`]{.idx}, [`tr`]{.idx}, [`true`]{.idx}, [`truncate`]{.idx}, [`tsort`]{.idx}, [`tty`]{.idx}, [`uname`]{.idx}, [`unexpand`]{.idx},
[`uniq`]{.idx}, [`unlink`]{.idx}, [`users`]{.idx}, [`vdir`]{.idx}, [`wc`]{.idx}, [`who`]{.idx}, [`whoami`]{.idx} and [`yes`]{.idx}, each a program of its own.

The [coreutils](#port-coreutils) port's page describes each program.

## Built again later

The [coreutils](#port-coreutils) port, in `30_foundation`, installs the same programs under the
same names, and kpkg adopts the files this step installed. It keeps
`fu_cv_sys_stat_statvfs=n`, `--without-openssl`, `--without-selinux` and the two extra programs,
and turns on what this build leaves out:

- `--with-libgmp` builds `expr` and `factor` against GMP, and stops configure if GMP is missing.
- `--enable-acl`, `--enable-xattr` and `--enable-libcap` let `cp -a` and `mv` carry ACLs and
  extended attributes, and `ls` show file capabilities.
- `CPPFLAGS=-I/usr/include/utmps` and `LIBS="-lutmps -lskarnet"` build `who`, `users` and `pinky`
  against utmps, so they read the login records `utmpd` keeps. In this build they use musl's own
  utmp functions, which are stubs and report no one.

The reasons for each are in [The base userland](../../kdos/03-architecture/packaging.md#the-base-userland).

## See also

- [Part III](intro.md): the bootstrap
- [The base userland](../../kdos/03-architecture/packaging.md#the-base-userland): the constraints on the port's configure line
- [Login records](../../kdos/03-architecture/boot-and-init.md#login-records): utmps and `utmpd`

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 16.* Previous: [15. Tar](061-tar.md) · [Contents](../README.md) · Next: [17. Diffutils](063-diffutils.md)
