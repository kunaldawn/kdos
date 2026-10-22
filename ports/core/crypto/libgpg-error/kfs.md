## About
libgpg-error defines the error codes and messages shared by every GnuPG component, and carries
`gpgrt`, the small portability runtime they also share: estream I/O, argument parsing, logging and
the `gpgrt-config` script. [libgcrypt](#port-libgcrypt), [libassuan](#port-libassuan),
[libksba](#port-libksba), [gpgme](#port-gpgme), [gnupg](#port-gnupg) and
[pinentry](#port-pinentry) are all built on it. It has no dependency of its own.

## Flags
- `--without-readline` — Skip configure's readline probe. No source file in this release reads its
  result, so it changes only what `config.h` records.
- `--enable-install-gpg-error-config` — Also install `gpg-error-config`, the older query script
  that upstream has deprecated in favour of `gpgrt-config` with `gpg-error.pc`, for a consumer's
  configure that asks for it by name.
