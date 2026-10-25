## About
GnuTLS is the TLS library of much of the GNU and GNOME side of the system:
[glib-networking](#port-glib-networking) and so every GIO program that opens a TLS connection,
[networkmanager](#port-networkmanager), [cups](#port-cups), [ffmpeg](#port-ffmpeg),
[qemu](#port-qemu) and [libvirt](#port-libvirt), [wireshark](#port-wireshark),
[gnupg](#port-gnupg)'s `dirmngr`, [samba](#port-samba), [msmtp](#port-msmtp), chrony's NTS and
[weechat](#port-weechat) among them. Its cryptography is [nettle](#port-nettle); its certificate
parsing is [libtasn1](#port-libtasn1); it reaches smart cards, and the system's trust anchors,
through [p11-kit](#port-p11-kit). The port also installs `certtool`, `gnutls-cli`,
`gnutls-serv`, `p11tool`, `ocsptool` and `psktool`.

## Flags
- `gnutls_cv_dlopen_soname_works=yes` — Pre-answer configure's test of whether a library can be
  `dlopen()`ed by its soname. The test dlopens the soname it finds libm linked under, and musl has
  no separate libm, so on musl it fails; the answer then decides that nothing may be dlopened, and
  `--with-tpm2=dlopen` stops configure with `Unable to dlopen TPM2`.
- `--with-default-trust-store-pkcs11="pkcs11:"` — Make the PKCS #11 URI `pkcs11:`, which matches
  every token p11-kit offers, the default trust store. p11-kit's trust module is one of those
  tokens, and with this switch it is GnuTLS's only source of trusted roots; see
  [TLS trust anchors](../../../../docs/kdos/03-architecture/security-model.md#tls-trust-anchors).
- `--with-zlib=link`, `--with-zstd=link`, `--with-brotli=link` — Link the certificate-compression
  libraries, [zlib](#port-zlib), [zstd](#port-zstd) and [brotli](#port-brotli), as ordinary
  dependencies of `libgnutls.so`. With dlopen working, each switch's default is to dlopen the
  library at run time instead.
- `--with-tpm2=dlopen` — TPM 2.0 keys through the [tpm2-tss](#port-tpm2-tss) libraries, which
  `libgnutls.so` opens the first time a TPM key is used, so a program that never uses one never
  loads the TPM stack.
- `--without-tpm` — No TPM 1.2 keys, which need TrouSerS; it is not in the tree.
- `--without-leancrypto` — No leancrypto, the post-quantum algorithm library; configure's default
  is the same, and the library is not in the tree.
- `--disable-libdane` — Build no `libgnutls-dane` and no `danetool`. DANE verification needs
  libunbound, which is not in the tree.
- `--enable-ktls` — Build in kernel TLS offload. It stays off at run time unless a system-wide
  configuration file, `/etc/gnutls/config`, sets `ktls = true` in its `[global]` section, and the
  image ships no such file.

## Notes
`build.sh` checks the TPM wiring twice. After configure it checks that `config.h`
defines the sonames of `libtss2-esys`, `libtss2-mu` and `libtss2-tctildr`, which configure records
only when it found each library; and after the install it fails if `libgnutls.so.30` NEEDs any
`libtss2` library. Between the two, the `sed` on the generated `libtool` adds
`-Wl,-O1,--as-needed` to its shared-library link, so a library named on the link line that the
code does not use is not recorded as NEEDED.

GnuTLS reads its system-wide priority policy from `/etc/gnutls/config`, a path compiled in from
`--sysconfdir=/etc`; the image ships none, so every program uses the library's `NORMAL`
priorities.
