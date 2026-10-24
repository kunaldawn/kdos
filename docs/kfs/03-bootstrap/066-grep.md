# Grep

GNU grep searches text for lines that match a pattern. Nearly every configure script, makefile and
build script runs it. This step builds it without Perl-compatible regular expressions, because
their library is not in the root yet. This page shows the commands of
`script/phases/10_bootstrap/066_grep.sh` and explains them.

**Source:** the archive of the [grep](#port-grep) port\
**Installs into:** `build/fs/usr`\
**Approximate build time:** 0.4 SBU

## Installation of grep

<!-- kfs:include script/phases/10_bootstrap/066_grep.sh lines=23-35 pin=83e3c8e0f08a -->

### The meaning of the configure options

`--host=$KDOS_TARGET`, `--build=$(build-aux/config.guess)`, `--prefix=/usr`
: A cross build, installed under `/usr` (see [Part III](intro.md#the-shape-of-a-step)).

`--disable-nls`
: Build no message catalogues.

`--disable-perl-regexp`
: Build without `grep -P`. Perl-compatible patterns need PCRE2, which is not in the root; `-P`
  from this build reports that it is not supported. Basic, extended and fixed-string patterns work
  as usual.

## Contents of grep

**Installed programs:** [`grep`]{.idx}, and [`egrep`]{.idx} and [`fgrep`]{.idx}, which run `grep -E` and `grep -F`.

The [grep](#port-grep) port's page describes each program.

## Built again later

The [grep](#port-grep) port, in `30_foundation`, depends on [pcre2](#port-pcre2) and passes
`--enable-perl-regexp` in place of `--disable-perl-regexp`, so `grep -P` works and `grep` links
`libpcre2-8`. Otherwise it is the same build, natively, and kpkg adopts the files this step
installed.

## See also

- [Part III](intro.md): the bootstrap, which shows this step's script whole
- [Patch](067-patch.md): the next step

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 20.* Previous: [19. Gawk](065-gawk.md) · [Contents](../README.md) · Next: [21. Patch](067-patch.md)
