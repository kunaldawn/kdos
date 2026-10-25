## About
[Net::SSLeay]{.idx} is the Perl binding to OpenSSL's `libssl` and `libcrypto`, an XS module
compiled against the [openssl](#port-openssl) port. [perl-io-socket-ssl](#port-perl-io-socket-ssl)
is built on it, and the two are what `git send-email` loads to send over TLS.

## Flags
- `OPENSSL_PREFIX=/usr` — Where `Makefile.PL` finds OpenSSL. Unset, it looks for an
  `openssl` program in a list of places and takes the first it finds, and Homebrew's,
  LinuxBrew's, `/usr/local` and MacPorts' `/opt/local` all come before `/usr`.
- `PERL_MM_USE_DEFAULT=1` — Answer every `prompt()` in `Makefile.PL` with its default. The one
  question asks whether to run the external tests, which need the network; the default is no.
- `INSTALLDIRS=vendor` — Install into perl's vendor directories, the place for modules that the
  distribution packages, rather than the site directories kept for modules a user installs
  later.

## Patches
- `openssl-4.patch` — Builds Net::SSLeay against OpenSSL 4, in `SSLeay.xs` and the documentation in `lib/Net/SSLeay.pod`. It reads `ASN1_STRING` contents through the accessor functions (`ASN1_STRING_get0_data`, `ASN1_STRING_length`, `ASN1_STRING_type`) instead of the structure's fields, in `ASN1_TIME_timet`, `X509_get_subjectAltNames` and `P_ASN1_TIME_get_isotime`, and sets times with `ASN1_TIME_set_string`. It drops the bindings of the single-version methods (`SSLv3_method`, `TLSv1_method`, `TLSv1_1_method`, `TLSv1_2_method` and their client and server forms) and `CTX_v3_new` when OpenSSL is 4 or later, and keeps `CTX_tlsv1_new`, `CTX_tlsv1_1_new` and `CTX_tlsv1_2_new` by building each from `TLS_method` with the minimum and maximum protocol version set to that one version.

## Notes
`Makefile.PL` exits with status 0 and writes no `Makefile` when it cannot find OpenSSL, so such
a failure shows up at the next step, where `make` finds no makefile.

The `find` lines after the install remove `.packlist` and `perllocal.pod`, MakeMaker's records
of what it installed, which kpkg's own database replaces, and the empty directories they leave.
