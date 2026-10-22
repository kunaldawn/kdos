## About
Mbed TLS is a small TLS and cryptography library, written to be embedded. KDOS carries it for the
programs written against it rather than against OpenSSL or GnuTLS: [obs-studio](#port-obs-studio)
uses it for its streaming outputs' TLS, [librist](#port-librist) for RIST's encryption, and
[imhex](#port-imhex) for its hashing and cryptography. The recipe stays on the long-term support
line its `series` key names; see
[Filtering](../../../../docs/kdos/05-developer/writing-ports.md#filtering) for what the key does.
The port installs the shared libraries `libmbedcrypto`, `libmbedx509` and `libmbedtls`, the
headers and the CMake package.

## Flags
- `-DUSE_SHARED_MBEDTLS_LIBRARY=ON`, `-DUSE_STATIC_MBEDTLS_LIBRARY=OFF` — Build the shared
  libraries only. Upstream's default is the static ones only.
- `-DGEN_FILES=OFF` — Use the generated sources that ship in the release instead of generating
  them again, which takes Python with Jinja2 and `jsonschema`; upstream's default, written out.
- `-DENABLE_PROGRAMS=OFF` — Build none of the sample and utility programs under `programs/`.
- `-DENABLE_TESTING=OFF` — Build none of upstream's test suites.
- `-DMBEDTLS_FATAL_WARNINGS=OFF` — Do not add `-Werror`, which upstream's default does, so a
  warning a newer compiler adds does not stop the build.

## Patches
- `threading-pthread.patch` — Turns on `MBEDTLS_THREADING_C` and `MBEDTLS_THREADING_PTHREAD` in `include/mbedtls/mbedtls_config.h`, the configuration header that is installed, so the library locks its shared state with pthread mutexes and can be used from several threads at once.

## Notes
Threading is set in the installed configuration header, never with a `-D` on the compiler's command
line. `MBEDTLS_THREADING_C` adds a mutex to the RSA context and to the contexts of the random number
generators, the entropy source and the SSL cookie and ticket modules, structures that consumers allocate
themselves, so a consumer compiled without the define would disagree with the library about their
layout. With it in `mbedtls_config.h`, every program
that includes Mbed TLS's headers sees the same value the library was built with.
