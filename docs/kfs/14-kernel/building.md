# Building the kernel

With the configuration resolved, the [linux](#port-linux) port's [`build.sh`]{.idx} compiles the kernel
and its modules and stages them in the package. This chapter covers what the configuration chapter
leaves out: the values the build pins so that two builds carry the same identity, the
module-signing key it keeps between builds, the logo it draws on the panic screen, the tools that
Rust and BTF need, and the commands that compile and stage everything.

**Source:** the archive of the [linux](#port-linux) port\
**Installs into:** `/boot`, `/lib/modules/<version>` and `/usr/include`\
**Depends on:** [dwarves](#port-dwarves), built just before it in the same phase, and ports that
earlier phases installed

## Running the build

`60_kernel` installs `dwarves` and then `linux`. Like every package step, each is one
`kpkg install` in the chroot, run from the top of the repository:

```bash
script/chroot/exec.sh bash -c 'source script/phases/60_kernel/phase.env && kpkg install dwarves'
script/chroot/exec.sh bash -c 'source script/phases/60_kernel/phase.env && kpkg install linux'
```

`kpkg` unpacks the source, runs `build.sh` with the working directory at the top of the kernel
tree, `PORT_SRC` set to the port directory and `PKG` to the staging directory, and then runs the
port's `postinstall.sh` on the root it installed into; [Installing the kernel](installing.md)
covers that script. The whole sequence is in
[Anatomy of a kpkg install](../04-chroot/kpkg-install.md). The `make` calls take their job count
from `MAKEFLAGS`, which `script/env/common.env` sets to `-j$KDOS_JOBS`.

## Pinned build identity

The first lines of `build.sh` fix four values kbuild otherwise takes from the machine and the
moment of the build:

<!-- kfs:include ports/core/boot/linux/build.sh lines=12-15 pin=47c1e5fe537c -->

### The meaning of the variables

`KBUILD_BUILD_TIMESTAMP`
: The date in the kernel's version string, which `uname -v` and the boot banner show. kbuild
  uses the time of the build unless this is set. The recipe writes `SOURCE_DATE_EPOCH`, which
  `script/env/common.env` sets for every phase, as a date, so every build of the same tree
  carries the same string.

`KBUILD_BUILD_USER`, `KBUILD_BUILD_HOST`
: The `user@host` part of `/proc/version`. Left unset they are the account and the host name of
  the build container. Both are `kdos`.

`KBUILD_BUILD_VERSION`
: The `#N` at the start of `uname -v`, which kbuild otherwise counts up in a `.version` file in
  the tree on every build. Pinned to `1`.

The package is still not byte-identical between two builds of the same tree: every module carries
an ECDSA signature, and an ECDSA signature uses a fresh random number each time it is made. See
[Reproducible packages](../../kdos/03-architecture/packaging.md#reproducible-packages).

## The module-signing key

The configuration turns module signing on (see
[Options the file does not set](configuring.md#options-the-file-does-not-set)), so the build
needs a [module-signing key]{.idx} to sign with. Left alone, kbuild generates a new one for every build, and the modules
of one build then fail verification under the kernel of another. Inside the KDOS build `build.sh`
uses one persistent key instead:

<!-- kfs:include ports/core/boot/linux/build.sh lines=33-43 pin=ba22fa4327c2 -->

`/kdos/build` is the repository's `build/` directory as the chroot sees it, so the key lives on
the machine that builds KDOS at `build/keys/linux-module-signing.pem`. `make clean` and
`make cleanbuild` both keep `build/keys`, and git ignores all of `build/`, so the private key never
enters the repository.

The first build that finds no key writes one with `openssl req`:

`-new -x509 -nodes -batch`
: Make a new self-signed certificate, with an unencrypted private key, without asking anything:
  the subject comes from the configuration file.

`-config $PORT_SRC/x509.genkey`
: The request settings in the port directory. Its subject is `O = KDOS`,
  `CN = KDOS kernel module signing key`, and its extensions mark the certificate as a signing key
  that is not a certificate authority. Its `default_bits` is a setting for RSA keys and plays no
  part with `-newkey ec`.

`-newkey ec -pkeyopt ec_paramgen_curve:secp384r1`
: An elliptic-curve key on the P-384 curve, the ECDSA key type [`kdos.config`]{.idx} selects.

`-sha512`, `-days 36500`, `-utf8`
: Sign the certificate with SHA-512, make it valid for a hundred years, and read the subject as
  UTF-8.

`-outform PEM -out "$modkey.new" -keyout "$modkey.new"`
: Write the certificate and the private key into one PEM file, the form `CONFIG_MODULE_SIG_KEY`
  expects.

`umask 077`, then `mv "$modkey.new" "$modkey"`
: The file is readable by its owner alone, and appears under its real name only once it is
  complete, so an interrupted [`openssl`]{.idx} leaves no half-written key for the next build to use.

`./scripts/config --set-str CONFIG_MODULE_SIG_KEY "$modkey"` then points the configuration at the
key. This is the one setting the recipe makes outside `kdos.config`, and it is made after the
checks of [How the build applies it](configuring.md#how-the-build-applies-it), so they never see
it. The certificate is built into the kernel, and every module installed later is signed with the
key.

A build where `/kdos/build` does not exist, such as one on an installed machine, skips the whole
block and keeps the default, `certs/signing_key.pem`, which kbuild generates for that build alone.
What the signature does and does not protect is in
[Module signing](../../kdos/03-architecture/security-model.md#module-signing).

## The logo

`build.sh` copies `kdos-logo-mono.pbm` from the port directory to
`drivers/video/logo/kdos-logo-mono.pbm`, and `kdos.config` names that path in
`CONFIG_LOGO_LINUX_MONO_FILE`. kbuild converts the image into C with its own `pnmtologo` while it
compiles the kernel, so the file has to be in place before `make bzImage`.

The panic screen draws this logo in its top-left corner at one screen pixel per image pixel, and
leaves it out where it would overlap the message or the QR code. It takes only a one-bit logo: it
asks for the logo of depth 1, which is the monochrome one. When it has none, because the logo it
gets is of another kind, `CONFIG_LOGO` is off, or DRM is loaded as a module rather than built into
the kernel, it draws a small ASCII-art Tux in its place. That is why `kdos.config` sets both `CONFIG_LOGO=y`
and `CONFIG_DRM=y`.

The `.pbm` is a committed file, in the plain-text P1 form so that a change to it reads as a diff.
`genlogo-mono.py` beside the recipe cuts it from the KDOS artwork, `kdos.png` at the top of the
repository, and writes it over the committed file:

```bash
ports/core/boot/linux/genlogo-mono.py kdos.png
```

The script needs the Python imaging library (PIL), which is not part of the build container, so it
runs on a host; nothing in the build runs it. It keeps the penguin and drops the wordmark, which is
unreadable at that size, and draws the mascot's dark line art rather than a filled silhouette, so
the face survives.

## What Rust and BTF need

Two features of the configuration exist only when their tools are present when `olddefconfig`
runs, and `olddefconfig` drops them without a word when they are not. `build.sh`'s checks turn
that into a failed build ([How the build applies it](configuring.md#how-the-build-applies-it)),
and the recipe's `depends` line names the tools:

Rust (`CONFIG_RUST`)
: The kernel runs `scripts/rust_is_available.sh` while it reads its Kconfig files. It needs
  `rustc` and [`bindgen`]{.idx}, each new enough for this kernel, the libclang that `bindgen` loads, and
  the source of Rust's `core` library under `rustc --print sysroot`, at
  `lib/rustlib/src/rust/library`. The [rust](#port-rust) port builds its `src` component, which
  installs that source; [bindgen](#port-bindgen) and [clang](#port-clang) provide the other two.
  `make rustavailable` runs the same script and prints what it found. Without Rust there is no
  QR-code panic screen, which is written in Rust.

BTF (`CONFIG_DEBUG_INFO_BTF`)
: The [BTF]{.idx} type information is made from the build's DWARF [debug information]{.idx} by `pahole`, from the
  [dwarves](#port-dwarves) port, and Kconfig reads `pahole`'s version to decide whether BTF can be
  on at all. BTF also governs Rust: with BTF on, `CONFIG_RUST` additionally depends on
  `CONFIG_PAHOLE_HAS_LANG_EXCLUDE`, a `pahole` new enough to leave Rust's compilation units out of
  the BTF it writes. Without BTF there is also no sched_ext and no BPF-based queueing discipline,
  both of which depend on it.

Of the other dependencies, `bc`, `bison`, `flex` and `perl` serve kbuild's own tools,
`elfutils` provides the `libelf` that `objtool` links, `openssl` writes the key and signs the
modules, and [`zstd`]{.idx} compresses the kernel image and every module. `kmod` and `cpio` are for
`postinstall.sh`, which also uses `zstd` to decompress modules.

## Compiling and staging

The rest of `build.sh` compiles, installs into the staging directory and removes what must not be
packaged:

<!-- kfs:include ports/core/boot/linux/build.sh lines=45-55 pin=8b40360422ca -->

### The meaning of the options

`HOSTCFLAGS="-D__attribute_const__= -D__always_inline=inline"`
: Extra flags for the programs kbuild compiles to run on the build machine. Both names are macros
  that glibc's `<sys/cdefs.h>` defines and musl's headers do not, and the kernel's `linux/swab.h`
  uses them. The recipe defines the first as nothing and the second as `inline`.

`bzImage modules`
: Build the compressed kernel image, `arch/x86/boot/bzImage`, and every module.

`INSTALL_MOD_PATH=$PKG`
: Install the modules under `$PKG/lib/modules/<version>/` rather than on the build root.

`INSTALL_MOD_STRIP=1`
: Strip each module of its debug information as it is installed. BTF needs the full DWARF while
  the kernel is built, which would otherwise stay in every module. The `.BTF` section is not
  debug information and stays, and kbuild signs each module after stripping it, so the signature
  covers the stripped file.

`DEPMOD=/bin/true`
: `modules_install` runs depmod through this variable; `/bin/true` makes that step do nothing, so
  the package carries no `modules.dep` or other module index. `postinstall.sh` builds the index in
  the root the package is installed into.

`rm -f "$PKG/lib/modules/$version/build"` and `…/source`
: Both are links into the directory the kernel was built in, which is gone once the package is
  made. `70_image`'s checks stop the build on a dangling link.

`cp arch/x86/boot/bzImage $PKG/boot/vmlinuz-kdos`, `cp System.map $PKG/boot/System.map-$version`
: The kernel under one constant name, and its symbol map under a versioned one. Nothing boots
  [`/boot/vmlinuz-kdos`]{.idx} where it lies: the image copies it onto the medium, and on an installed
  machine `kdos-bootctl deploy` copies it to the ESP.

`INSTALL_HDR_PATH=$PKG/usr headers_install`
: The kernel's user-space API headers, sanitised, into `$PKG/usr/include`. They are the same
  files the bootstrap's [Linux API headers](../03-bootstrap/010-linux-headers.md) step wrote from
  the same source, and the package takes them over.

## Contents of linux

**Installed files:** `/boot/vmlinuz-kdos`, `/boot/System.map-<version>`, the module tree
`/lib/modules/<version>/` (`kernel/`, `modules.order`, `modules.builtin` and
`modules.builtin.modinfo`; the indexes depmod writes appear after `postinstall.sh`), and the API
headers under `/usr/include`.

The [linux](#port-linux) port's page lists every file the package owns.

## See also

- [Configuring the kernel](configuring.md): `kdos.config`, the checks and the option tables
- [Installing the kernel](installing.md): `postinstall.sh`
- [The kernel phase](../../kdos/05-developer/how-kdos-is-built.md#the-kernel-60_kernel): the phase in the build
- [Module signing](../../kdos/03-architecture/security-model.md#module-signing): what the signature means

<!-- book-nav -->
---

*Part XIV. The kernel, chapter 337.* Previous: [336. Configuring the kernel](configuring.md) · [Contents](../README.md) · Next: [338. Installing the kernel](installing.md)
