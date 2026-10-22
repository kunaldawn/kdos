## About

kdos-lock is the [lock screen]{.idx}. It is an `ext-session-lock-v1` client: it covers every output
with a lock surface, and the compositor, not the lock program, owns the locked state, so if
`kdos-lock` dies the screens stay covered until another lock client takes over. Once the
compositor confirms the lock it prints `locked` on standard output, which is what
`kdos-power suspend` waits for before the machine sleeps.

The same recipe builds `kdos-checkpass`, the lock screen's only privileged part, setuid
root. `/etc/shadow` is readable only by root and the lock screen
must not run as root, so `kdos-lock` hands the password to `kdos-checkpass` on standard input and
reads the answer from its exit status. Both are described in
[The daemons](../../../docs/kdos/04-programs/daemons.md#kdos-lock); the checker's exact contract is
in [kdos-checkpass](../../../docs/kdos/03-architecture/security-model.md#kdos-checkpass).

## Flags

- `-lcrypt` — Link the library that holds `crypt()`, which `kdos-checkpass` hashes the typed
  password with. On musl, `crypt()` is in the C library and `libcrypt.a` is an empty archive musl
  installs, so the flag resolves and adds nothing; on a glibc host, where `testing/selftest.sh`
  compiles the same file with the same flag, it is the library `crypt()` lives in.
- `-Dm4755` — Install `kdos-checkpass` owned by root with the setuid bit. The bit is a mode bit,
  not a file capability, because a mode bit survives the package archive and an extended
  attribute does not; see [setuid binaries](../../../docs/kdos/03-architecture/security-model.md#setuid-binaries).
  Without it the checker cannot read `/etc/shadow`, answers 2 ("could not tell") to every attempt,
  and the lock screen refuses every password. `kdos doctor` checks the bit and prints the repair.

## Notes

- **The two programs are compiled on separate lines**, and neither uses a glob over the port's
  directory, because `main.c` and `checkpass.c` each have a `main`. `kdos-checkpass` is
  `checkpass.c` and `-lcrypt` alone: none of KDOS's libraries and none of the Wayland stack runs
  in the setuid process.
- **wlroots is a dependency for its protocol files only.** The layer-shell and
  foreign-toplevel protocols are generated from `/usr/share/wlroots/protocols/`, which the
  `wlroots` recipe installs; the lock screen links none of wlroots.
- **The whole protocol set is generated, used or not.** `libkwl` is compiled whole and its
  `kwl.c` includes the header of every protocol it can use, so each consumer generates the client
  header and the `private-code` of all of them. A missing one fails the compile or the link.
