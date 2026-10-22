# Diffutils

GNU diffutils compares files: `diff` and `cmp`, and `diff3` and `sdiff` for three-way and
side-by-side work. Configure scripts and test suites call them, and `patch` reads what `diff`
writes. This page shows the commands of `script/phases/10_bootstrap/063_diffutils.sh` and explains
them, including the one configure test the step answers in advance.

**Source:** the archive of the [diffutils](#port-diffutils) port\
**Installs into:** `build/fs/usr`\
**Approximate build time:** 0.4 SBU

## Installation of diffutils

<!-- kfs:include script/phases/10_bootstrap/063_diffutils.sh lines=23-35 pin=7c7562f85d32 -->

### The meaning of the configure options

`--host=$KDOS_TARGET`, `--build=$(build-aux/config.guess)`, `--prefix=/usr`
: A cross build, installed under `/usr` (see [Part III](intro.md#the-shape-of-a-step)).

`--disable-nls`
: Build no message catalogues.

`gl_cv_func_strcasecmp_works=y`
: Preset gnulib's answer to `checking whether strcasecmp works`. The check has to run a test
  program, and in a cross build it cannot, so without an answer configure stops with
  `cannot run test program while cross compiling`. Any preset value skips the test. gnulib accepts
  only a value ending in `yes` as a working `strcasecmp`, so `y` makes configure take musl's
  `strcasecmp` as faulty and compile gnulib's replacement into the programs. The configure goes
  through either way, and the native port, which runs the test, needs no answer.

## Contents of diffutils

**Installed programs:** [`cmp`]{.idx}, [`diff`]{.idx}, [`diff3`]{.idx} and [`sdiff`]{.idx}.

The [diffutils](#port-diffutils) port's page describes each program.

## Built again later

The [diffutils](#port-diffutils) port, in `20_selfhost`, is built inside the chroot, where the
`strcasecmp` test runs, so it presets nothing. It adds `--enable-threads=posix`, which names POSIX
threads for gnulib's locking code. kpkg adopts the files this step installed.

## See also

- [Part III](intro.md): the bootstrap
- [Findutils](064-findutils.md): the next step
- [The toolchain rebuilt as packages](../../kdos/05-developer/how-kdos-is-built.md#the-toolchain-rebuilt-as-packages-20_selfhost): diffutils among the tools the GCC build runs

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 17.* Previous: [16. Coreutils](062-coreutils.md) · [Contents](../README.md) · Next: [18. Findutils](064-findutils.md)
