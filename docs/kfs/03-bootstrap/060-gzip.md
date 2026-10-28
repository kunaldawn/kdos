# Gzip

gzip compresses and decompresses the `.gz` format. Many upstream sources still ship as `.tar.gz`,
and `tar` calls `gzip` to read them, so it is in the root before the first port is built. This
page shows the commands of `script/phases/10_bootstrap/060_gzip.sh` and explains them.

**Source:** the archive of the [gzip](#port-gzip) port\
**Installs into:** `build/fs/usr`\
**Approximate build time:** 0.2 SBU

## Installation of gzip

<!-- kfs:include script/phases/10_bootstrap/060_gzip.sh lines=24-33 pin=651f8ca9bfd0 -->

gzip is configured in its source directory, as its own build expects.

### The meaning of the configure options

`--host=$KDOS_TARGET`, `--build=$(build-aux/config.guess)`
: A cross build: compiled by the cross toolchain, with configure running no test program (see
  [Part III](intro.md#the-shape-of-a-step)). gzip keeps `config.guess` in `build-aux/`, as every
  gnulib-based package from here to `068_sed.sh` does.

`--prefix=/usr`
: Install under `/usr`.

## Contents of gzip

**Installed programs:** [`gzip`]{.idx}, [`gunzip`]{.idx}, [`zcat`]{.idx}, [`uncompress`]{.idx}, [`gzexe`]{.idx}, [`zcmp`]{.idx}, [`zdiff`]{.idx},
[`zegrep`]{.idx}, [`zfgrep`]{.idx}, [`zforce`]{.idx}, [`zgrep`]{.idx}, [`zmore`]{.idx} and [`znew`]{.idx}. Most of them are shell scripts
around `gzip`.

`zless` and `zmore` are installed only when configure finds `less` and `more` on the `PATH` it runs
with. Here that is the container's, whose busybox provides both, so `make install` puts both in
the root. The step then deletes `/usr/bin/zless` and its manual page `zless.1`, and keeps `zmore`.

The [gzip](#port-gzip) port's page describes each program.

## Built again later

The [gzip](#port-gzip) port, in `30_foundation`, rebuilds it natively with `--prefix=/usr` alone,
and kpkg adopts the files this step installed. Apart from the two switches of the cross build, the
two builds differ only where configure looks for `less` and `more`: the port's configure looks in
the chroot, and installs `zless` or `zmore` only if that program is installed there when gzip is
built. `less` is not, so the package carries no `zless`, and a `zless` left by this step would be
adopted by no package (see [What the bootstrap leaves behind](intro.md#what-the-bootstrap-leaves-behind)).
That is why the step deletes it. `zmore` is the one of the two left for the port to adopt.

## See also

- [Part III](intro.md): the bootstrap
- [Tar](061-tar.md): the program that calls this one

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 14.* Previous: [13. Xz](050-xz.md) · [Contents](../README.md) · Next: [15. Tar](061-tar.md)
