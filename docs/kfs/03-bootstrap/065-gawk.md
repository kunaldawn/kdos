# Gawk

gawk is GNU awk, the pattern-scanning language, and on KDOS it is also `/usr/bin/awk`. Configure
scripts and makefiles run awk constantly, and GCC's build does too. This step builds it with no
optional library. This page shows the commands of `script/phases/10_bootstrap/065_gawk.sh` and
explains them.

**Source:** the archive of the [gawk](#port-gawk) port\
**Installs into:** `build/fs/usr` and `build/fs/etc/profile.d`\
**Approximate build time:** 0.3 SBU

## Installation of gawk

<!-- kfs:include script/phases/10_bootstrap/065_gawk.sh lines=23-42 pin=e9118a5b8311 -->

### The meaning of the configure options

`--host=$KDOS_TARGET`, `--build=$(build-aux/config.guess)`, `--prefix=/usr`
: A cross build, installed under `/usr` (see [Part III](intro.md#the-shape-of-a-step)).

`--sysconfdir=/etc`
: Install gawk's login-shell profile scripts in `/etc/profile.d`, where the system's profile reads
  them, rather than under `/usr/etc`.

`--without-readline`
: Build gawk's debugger, `gawk -D`, with plain line input. readline is built in step 070, after
  this one.

`--without-mpfr`
: Build without arbitrary-precision arithmetic, so `gawk -M` is not available. MPFR is not in the
  root.

`--disable-nls`
: Build no message catalogues.

The `rm` deletes `gawk.csh`, the C-shell variant of the profile script. The gawk port leaves it out
of its package, so a copy installed here would never be replaced or removed (see
[Part III](intro.md#what-the-bootstrap-leaves-behind)). `gawk.sh` stays, and the port installs over
it.

## Contents of gawk

**Installed programs:** [`gawk`]{.idx}, `gawk-` followed by the version, [`awk`]{.idx} (a link to `gawk`) and
[`gawkbug`]{.idx}; [`grcat`]{.idx} and [`pwcat`]{.idx} in `/usr/libexec/awk`.

**Installed libraries:** the dynamic extensions in `/usr/lib/gawk`, such as [`filefuncs.so`]{.idx},
[`inplace.so`]{.idx}, [`readdir.so`]{.idx} and [`time.so`]{.idx}.

**Installed files:** [`/etc/profile.d/gawk.sh`]{.idx}, and the awk library functions in `/usr/share/awk`.

The [gawk](#port-gawk) port's page describes each program.

## Built again later

The [gawk](#port-gawk) port, in `20_selfhost`, is built inside the chroot with readline, MPFR and
GMP installed before it, and kpkg adopts the files this step installed. It swaps the two `without`
switches for `--with-mpfr` and `--with-readline`, so its `gawk` links libreadline, libmpfr and
libgmp. It deletes `gawk.csh` from its package as this step does, and `gawknotes.info` with it.

## See also

- [Part III](intro.md): the bootstrap
- [Grep](066-grep.md): the next step
- [The base userland](../../kdos/03-architecture/packaging.md#the-base-userland): which package owns `awk`

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 19.* Previous: [18. Findutils](064-findutils.md) · [Contents](../README.md) · Next: [20. Grep](066-grep.md)
