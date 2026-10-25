## About
Libgcrypt is the general-purpose cryptographic library that grew out of GnuPG: ciphers, hashes,
MACs, public-key algorithms, a random number generator and the S-expression format GnuPG keys are
kept in. [gnupg](#port-gnupg) is built on it, and so are many programs that need cryptography
without TLS, among them [libsecret](#port-libsecret), [kwallet](#port-kwallet),
[wireshark](#port-wireshark)'s decryption, [weechat](#port-weechat), [vlc](#port-vlc) and both
WebKitGTK builds. Errors are reported through [libgpg-error](#port-libgpg-error). The port also
installs `dumpsexp`, `hmac256` and `mpicalc`, and `libgcrypt-config`.

## Notes
The build is configure's defaults with `--prefix=/usr`, and the library picks its accelerated
code (AES-NI, SHA extensions, AVX2 and the rest) at run time from what the processor reports, so
one build serves every x86-64 machine.
