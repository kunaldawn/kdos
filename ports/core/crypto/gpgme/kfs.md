## About

GPGME (GnuPG Made Easy) is the C library through which programs use GnuPG: it runs `gpg`,
`gpgsm` and the other GnuPG tools as child processes, talks to them over pipes and the Assuan
protocol, and gives the caller an API for encrypting, decrypting, signing, verifying and managing
keys. Mail and chat
clients ([notmuch](#port-notmuch), [gmime](#port-gmime), [dino](#port-dino),
[profanity](#port-profanity)) and the container tools that verify image signatures
([podman](#port-podman), [skopeo](#port-skopeo), [buildah](#port-buildah)) link it. The C++ and
Qt bindings are the separate [gpgmepp](#port-gpgmepp) and [qgpgme](#port-qgpgme) ports.

## Flags

- `--disable-linux-getdents` — Before running a GnuPG tool, GPGME closes every inherited file
  descriptor in the child. On Linux it normally finds the highest open descriptor by reading
  `/proc/self/fd` with the raw `getdents64` system call, because `readdir` is not safe between
  `fork` and `exec` in a threaded process. That code declares its directory entry with `ino64_t`
  and `off64_t`, names musl's headers define only under `_LARGEFILE64_SOURCE`. With the switch,
  GPGME takes the hard descriptor limit from `getrlimit(RLIMIT_NOFILE)` instead and closes up to it,
  which costs more `close` calls per spawned process and behaves the same.
- `--disable-gpg-test`, `--disable-gpgsm-test`, `--disable-g13-test` — Build none of the
  regression test programs; the port runs no tests. `--disable-gpg-test` alone drops `tests/`
  from the build, whose `run-*` programs a plain `make` would otherwise compile.
  `--disable-gpgsm-test` gates only the `gpgsm` and `opassuan` directories inside `tests/`, and
  `--disable-g13-test` gates nothing in this release, so both restate what the first removes.
