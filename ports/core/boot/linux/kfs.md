## About

The [Linux kernel]{.idx}. Its whole configuration is one file beside the recipe, `kdos.config`,
in the minimal form `make savedefconfig` writes: every symbol whose value differs from the
kernel's default, plus the hardening settings that are written out even where they equal it.
`build.sh` copies the file to `.config`, lets `make olddefconfig` fill in the rest, and then
refuses to build a kernel that differs from what the file says. The three checks, and the way to
change the file without losing settings, are in
[The kernel configuration](../../../../docs/kdos/05-developer/how-kdos-is-built.md#the-kernel-configuration);
what the hardening lines remove is in
[The kernel](../../../../docs/kdos/03-architecture/security-model.md#the-kernel).

The package installs the kernel as `/boot/vmlinuz-kdos` with its `System.map`, the modules under
`/lib/modules/<version>`, and the kernel's user-space API headers under `/usr/include`. Those
headers are the same files the bootstrap's Linux API headers step wrote from the same source, and
this package adopts them. Nothing boots `/boot/vmlinuz-kdos` where it lies: `70_image` copies it
into the ISO, and on an installed machine `kdos-bootctl deploy` copies it into the root slot's
directory on the ESP.

Several of the build dependencies are there for one configuration option each. `rust`, `bindgen` and
`clang` are what `CONFIG_RUST` needs, and with it the QR-code panic screen; `dwarves` provides
`pahole`, without which there is no BTF; `openssl` writes the module-signing key and signs the
modules. `zstd` compresses the kernel image (`CONFIG_KERNEL_ZSTD`) and every module as
`modules_install` writes it (`CONFIG_MODULE_COMPRESS_ZSTD`), and `postinstall.sh` uses it again to
decompress the modules it puts into the initramfs. `cpio` is for `postinstall.sh` alone, which
lists the image's initramfs and writes the archive of this kernel's modules it appends.

## Flags

- `KBUILD_BUILD_TIMESTAMP` — The date kbuild puts in `UTS_VERSION`, which `uname -v` and the boot
  banner show; by default it is the moment of the build. The recipe writes `SOURCE_DATE_EPOCH` as
  a date, so two builds of the same tree carry the same string. See
  [Reproducible packages](../../../../docs/kdos/03-architecture/packaging.md#reproducible-packages).
- `KBUILD_BUILD_USER` — The `user` of the `user@host` string in `/proc/version` and the boot
  banner, by default the output of `whoami`. Pinned to `kdos`.
- `KBUILD_BUILD_HOST` — The `host` of the same string, by default the build machine's host name,
  which in the build container is the container's. Pinned to `kdos`.
- `KBUILD_BUILD_VERSION` — The `#N` at the start of `uname -v`. Without it kbuild takes the
  number from a build counter it keeps in a `.version` file in the tree. Pinned to `1`.
- `KCONFIG_WERROR=1` — Kconfig treats its warnings as errors, so `olddefconfig` stops on a value a
  symbol cannot take or a selected symbol whose dependencies are not met, instead of printing a
  warning and carrying on.
- `KCONFIG_WARN_UNKNOWN_SYMBOLS=1` — Kconfig warns about every symbol in `kdos.config` that this
  kernel does not have. With `KCONFIG_WERROR` that is an error, so a symbol renamed or removed by
  a new kernel release stops the build instead of disappearing from the configuration.
- `CONFIG_MODULE_SIG_KEY` — Set with `scripts/config --set-str` to
  `build/keys/linux-module-signing.pem`, but only inside the KDOS build, where `/kdos/build`
  exists. The first build writes the key with the subject in `x509.genkey` (an ECDSA P-384 key,
  valid for a hundred years) and every later build signs with the same one. A build anywhere else
  keeps the kernel's default, a key generated for that build alone. See
  [Module signing](../../../../docs/kdos/03-architecture/security-model.md#module-signing).
- `HOSTCFLAGS` — Extra flags for the programs kbuild compiles to run on the build machine. Some of
  them include the kernel's `swab.h`, which uses `__attribute_const__` and `__always_inline`. glibc
  defines both in `<sys/cdefs.h>`; musl has no such header, so the recipe defines the first as
  nothing and the second as `inline`.
- `INSTALL_MOD_PATH=$PKG` — The prefix `make modules_install` puts in front of `/lib/modules`, so
  the modules land in the package and not on the build root.
- `INSTALL_MOD_STRIP=1` — Strip each module as it is installed, with `--strip-debug`. BTF needs
  `CONFIG_DEBUG_INFO`, which would otherwise leave full DWARF in every module. The `.BTF` section
  is not a debug section and stays, and kbuild signs each module after stripping it.
- `DEPMOD=/bin/true` — `modules_install` runs `scripts/depmod.sh`, which calls `$DEPMOD`; naming
  `/bin/true` makes that step do nothing, so the package carries no `modules.dep` or other module
  index. `postinstall.sh` runs `depmod -b` against the root the package was installed into.
- `INSTALL_HDR_PATH=$PKG/usr` — Where `make headers_install` writes the sanitised API headers:
  `$PKG/usr/include`.

## Notes

- **The panic-screen logo is a file the kernel tarball does not have.** `kdos.config` sets
  `CONFIG_LOGO` and points `CONFIG_LOGO_LINUX_MONO_FILE` at
  `drivers/video/logo/kdos-logo-mono.pbm`, and the logo's makefile names that path as an input of
  the build, so `build.sh` copies the file there from the port before anything is compiled. The
  `.pbm` is committed; `genlogo-mono.py` regenerates it on a host with PIL, and nothing in the
  build runs the script.
- **Two checks after `olddefconfig` stop the build.** Every `CONFIG_…=` and
  `# CONFIG_… is not set` line of `kdos.config` must be in the resolved `.config` unchanged, and
  `CONFIG_RUST`, `CONFIG_DRM_PANIC_SCREEN_QR_CODE`, `CONFIG_DEBUG_INFO_BTF`,
  `CONFIG_DEBUG_INFO_BTF_MODULES`, `CONFIG_SQUASHFS_ZSTD` and `CONFIG_SQUASHFS_XZ` must be `=y`.
  `olddefconfig` drops an option whose compiler or tool is missing without a word; these checks
  turn a missing `rustc`, `bindgen`, `libclang` or `pahole` into a failed build.
- **Regenerate `kdos.config` inside the chroot**, or anywhere with the same `rustc`, `clang` and
  `pahole`. Elsewhere `olddefconfig` drops `CONFIG_RUST` and BTF, and `savedefconfig` writes a file
  without them.
- **The `build` and `source` links are removed** from `lib/modules/<version>/`. Both point into
  the work directory the kernel was built in, which is gone once the package is made, and
  `70_image`'s gates stop the build on a dangling link.
- **`postinstall.sh` keeps the boot path consistent.** Working on the root it was installed into
  (`PKG_ROOT`, the inactive slot during `kdos update`), it removes the module trees of kernels
  that neither run nor sit on the ESP, runs `depmod`, builds `/boot/initramfs-kdos.cpio.gz` from the image's
  initramfs with this kernel's modules appended, and, when it was installed into the running
  system with the ESP mounted, runs `kdos-bootctl deploy /`. Each step and the reason for it is in
  [A new kernel](../../../../docs/kdos/03-architecture/boot-and-init.md#a-new-kernel).
- **The package is not byte-identical between builds.** An ECDSA signature uses a fresh random
  number each time, so every module's appended signature differs even with the same key.
