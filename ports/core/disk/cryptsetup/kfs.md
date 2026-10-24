## About

cryptsetup sets up [LUKS]{.idx} and other dm-crypt encrypted block devices: `cryptsetup
luksFormat` creates an encrypted container and `cryptsetup open` maps it to a plain device under
`/dev/mapper`, through the kernel's device mapper and crypto API. `libcryptsetup` is the library
under it.

It is on the ISO, not only on an installed system, for two reasons. `kinstall` creates the LUKS
container for an encrypted install, and `script/phases/70_image/090_initramfs.sh` copies
`cryptsetup` and its libraries into the initramfs, where the boot unlocks an encrypted root
before it looks for a filesystem; see
[Unlocking an encrypted root](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#unlocking-an-encrypted-root).
The mount daemon runs `/usr/sbin/cryptsetup` to unlock an encrypted volume for the desktop; see
[Encrypted volumes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/daemons.md#encrypted-volumes).
[libblockdev](#port-libblockdev), [volume_key](#port-volume_key) and [gparted](#port-gparted)
depend on it.

## Flags

- `--with-crypto_backend=openssl` — Takes hashing, ciphers and key derivation from OpenSSL's
  libcrypto. It is upstream's default, named here; the library is one the initramfs carries.
- `--enable-udev` — Uses libdevmapper's udev synchronisation, so libcryptsetup waits for udev to
  finish with a device-mapper node it creates or removes. It is upstream's default.
- `--enable-blkid` — Uses util-linux's libblkid to detect an existing filesystem or other
  signature on a device before formatting it, and to wipe it. It is upstream's default.
- `--disable-pwquality`, `--disable-passwdqc` — Check no passphrase against a quality library.
  Both are off by default.
- `--without-tmpfilesdir` — Installs no systemd `tmpfiles.d` fragment. KDOS has no systemd.
- `--disable-asciidoc` — Generates no manual pages with asciidoctor. The switch is on by default,
  and configure then stops when `asciidoctor` is not installed; [asciidoctor](#port-asciidoctor)
  is not in `depends`. The release tarball carries the pages already built, and configure reuses
  those, so they are installed either way.
- `--disable-rpath` — Turns off the run path that gettext's library search macros in configure
  add for the libraries they find, an entry of `/usr/lib`. The tree's package lint warns on such
  an entry, since `/usr/lib` is the default search path; see
  [What the package lint refuses](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses).
- `--disable-ssh-token` — Builds no LUKS2 SSH token plugin, which unlocks a container with a key
  fetched over SSH through libssh.

## Notes

The initramfs carries `cryptsetup` with an explicit list of libraries, because there is no `ldd`
in the image phase: `libcryptsetup`, `libdevmapper`, `libjson-c`, `libpopt`, OpenSSL's two
libraries, `libargon2`, `libuuid`, `libblkid` and `libz`, each copied when it exists. A switch here
that makes `cryptsetup` or `libcryptsetup` link a further library has to add it to that list in
`090_initramfs.sh`; otherwise the initramfs `cryptsetup` cannot start, and an encrypted root
cannot be unlocked at boot.

`--disable-nls` builds no translations, which the package prune would remove anyway, and it means
the build needs no libintl; see
[Gettext on musl](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/build-troubleshooting.md#gettext-on-musl).
