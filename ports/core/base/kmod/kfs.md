## About
kmod loads and unloads kernel modules. It is one program, `/usr/bin/kmod`, and `modprobe`,
`insmod`, `rmmod`, `lsmod`, `modinfo` and `depmod` are links to it; `libkmod` is the library behind
them. On KDOS, [eudev](#port-eudev) links libkmod to load the driver for each device it sees, the
`02_modules` boot script loads the lists in `/etc/modules-load.d`, and the initramfs carries `kmod`
as `modprobe`, `insmod` and `depmod` and runs `depmod` over the modules it copies (see
[The initramfs](../../../../docs/kdos/03-architecture/boot-and-init.md#the-initramfs)).
The [linux](#port-linux) port depends on it.

## Flags
- `-include libgen.h` — Include `<libgen.h>` ahead of every source file, configure's tests among
  them. configure checks whether `basename` is declared after `<string.h>`; musl declares it only in
  `<libgen.h>`, so without this the answer is no and kmod defines its own `basename` in
  `shared/missing.h`. With it the answer is yes, and kmod calls musl's POSIX `basename()`, which
  takes a non-const string: the compiler warns about discarded `const` qualifiers in `depmod.c`,
  `util.c` and `libkmod-config.c`.
- `--with-xz`, `--with-zstd`, `--with-zlib` — Read modules compressed with xz, zstd or gzip. Each is
  off by default, and each, once asked for, stops configure if pkg-config cannot find its library.
  The kernel's own modules are compressed with zstd (`CONFIG_MODULE_COMPRESS_ZSTD` in the
  [linux](#port-linux) port's `kdos.config`), and `depmod` and `modinfo` must decompress a module to
  read it.
- `--without-openssl` — Build without libcrypto, upstream's default. kmod uses it only to decode
  the PKCS#7 signature appended to a module. The [linux](#port-linux) port signs every module
  (`CONFIG_SECURITY_LOCKDOWN_LSM` turns on `CONFIG_MODULE_SIG`), so `modinfo` does find a
  signature: it reports `sig_id: PKCS#7` and `sig_hashalgo: unknown`, and leaves the signer, key
  and signature fields empty. Loading is unaffected, because the kernel verifies the signature
  itself (see
  [Module signing](../../../../docs/kdos/03-architecture/security-model.md#module-signing)).

## Patches
- `relative-tool-links.patch` — Changes the install hook in `Makefile.in` that links the tools to `kmod`. Upstream runs `$(LN_S) --force --relative` from `$(DESTDIR)$(bindir)/kmod`; the patch writes each link with the literal relative target `../bin/kmod`, which is what `/usr/sbin/modprobe` and the others point to on an installed system.

## Notes
The manual pages are written in scdoc, and configure stops when `scdoc` is missing unless
`--disable-manpages` is passed; that is why the recipe depends on [scdoc](#port-scdoc).

musl's `basename()` writes into its argument when the path ends in `/`, stripping the slashes in
place; kmod's own replacement only reads it. The warnings about discarded `const` mark the
calls that pass a string kmod treats as read-only. Adding or removing `-include libgen.h` changes
which of the two functions every kmod program calls.
