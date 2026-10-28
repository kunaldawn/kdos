## About
NSS, Mozilla's Network Security Services, is the TLS, PKCS #11 and certificate library of the
Mozilla programs: [firefox-esr](#port-firefox-esr), [librewolf](#port-librewolf) and
[thunderbird](#port-thunderbird) build on it. [chromium](#port-chromium) and
[qt6-qtwebengine](#port-qt6-qtwebengine), whose TLS is BoringSSL, use it for the user's
certificate and key database and for PKCS #11 tokens, and [libreoffice](#port-libreoffice)'s
signing, [volume_key](#port-volume_key) and [libcacard](#port-libcacard) build on it too. It runs
on [nspr](#port-nspr) and keeps certificate databases in SQLite. The port installs the libraries, the headers, `nss.pc` and `nss-config`, and the tools
`certutil`, `cmsutil`, `crlutil`, `modutil`, `pk12util`, `signtool`, `signver` and `ssltap`.

## Flags
- `BUILD_OPT=1` — An optimised build (`-O2`) without the debug code. NSS's own build system has
  no configure; every choice is a make variable.
- `USE_64=1` — The 64-bit build.
- `NSPR_INCLUDE_DIR=/usr/include/nspr` — Use the installed NSPR's headers. With the patch,
  NSS builds against the [nspr](#port-nspr) port instead of an NSPR tree beside its own.
- `USE_SYSTEM_ZLIB=1`, `ZLIB_LIBS=-lz` — Link the system [zlib](#port-zlib) rather than compile
  the copy NSS carries; Linux's platform file sets the same values.
- `NSS_USE_SYSTEM_SQLITE=1` — Link the system [sqlite](#port-sqlite) for the certificate and key
  databases, rather than build NSS's own copy.
- `NSS_ENABLE_WERROR=0` — Do not turn compiler warnings into errors. Left unset, NSS turns on
  `-Werror` for any GCC from 4.8 on, so a warning a newer compiler adds stops the build.
- `NSS_DISABLE_GTESTS=1` — Build no GoogleTest suites.
- `DSO_LDOPTS="-shared -m64 -Wl,--gc-sections -Wl,-z,defs $LDFLAGS"` — The link options of every
  shared library: NSS's own Linux defaults, `-shared`, the architecture flag, `--gc-sections` and
  `-z defs`, with the tree's `LDFLAGS` added. NSS links its programs with `LDFLAGS`, but its shared
  libraries with `DSO_LDOPTS` alone.
- `XCFLAGS="$CFLAGS"` — Add the tree's `CFLAGS` to every compile; NSS's build reads compiler
  flags from this variable, not from `CFLAGS`.

## Patches
- `nss-standalone.patch` — Xi Ruoyao's patch, as its header says: it builds NSS without an NSPR source tree beside it (its `nss_build_all` target skips `build_nspr`), and adds a `config` directory that generates `nss.pc` and `nss-config`, with NSS's version read from `nss.h` and the minimum NSPR version from `automation/release/nspr-version.txt`. Upstream installs neither file, and a consumer's configure looks for NSS through them.

## Notes
NSS has no install target, so `build.sh` copies from `dist/`. The libraries are under a directory
named for the build host, `dist/Linux<kernel major.minor>_x86_64_gcc_glibc_PTH_64_OPT.OBJ/` (NSS
writes `glibc` whatever the C library), and the copies use the glob `Linux*` because the kernel
release in the name is the build machine's.

The `.chk` files are installed beside the libraries they sign. They hold the self-test signatures
of `libsoftokn3`, `libfreebl3`, `libfreeblpriv3` and `libnssdbm3`, and in FIPS mode NSS refuses
to load a library whose signature is missing.

`libnssckbi.so` is the module NSS loads as its Builtin Roots. The port removes NSS's own, which
carries Mozilla's root list compiled in, and installs in its place a symlink to p11-kit's trust
module, `pkcs11/p11-kit-trust.so`, so a program using NSS's builtin roots trusts the same anchors
as GnuTLS and the rest of the system; see
[TLS trust anchors](../../../../docs/kdos/03-architecture/security-model.md#tls-trust-anchors).
Firefox, LibreWolf and Thunderbird compile their builtin roots into the browser itself and keep
their own store.
