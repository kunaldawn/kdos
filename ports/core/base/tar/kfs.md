## About

GNU tar creates, lists and extracts tar archives. It is the `tar` every KDOS build depends on
without naming it: [kpkg](../../../../docs/kdos/03-architecture/packaging.md#kpkg) runs it to unpack
each port's source, to roll the staged tree into a package, and again to place a package's files
over the root. Every package of the tree therefore passes through this program twice.

That is why it is the second port `20_selfhost` builds, straight after
[fortify-headers](#port-fortify-headers): every later source in the phase is unpacked by the tar
this port installs, not by the hand-installed copy of [Part III](#03-bootstrap-061-tar). The reasons
the phase rebuilds the bootstrap's programs as packages are in
[The toolchain rebuilt as packages](../../../../docs/kdos/05-developer/how-kdos-is-built.md#the-toolchain-rebuilt-as-packages-20_selfhost).

## Flags

- `FORCE_UNSAFE_CONFIGURE=1` — tar's configure stops with `you should not run configure as root`
  unless this is set. kpkg builds every port as root inside the chroot.
- `--disable-acl` — skip gnulib's check for an ACL library (`sys/acl.h` and `acl_get_file` in
  libacl). tar's source compiles its ACL code only when that check succeeded, so this alone leaves
  `--acls` out. The [acl](#port-acl) port is built in `30_foundation`, after this one.
- `--without-posix-acls` — skip tar's own check for the POSIX.1e ACL functions, which `--acls`
  uses to store and restore access control lists in the archive. With both checks off, the program
  does not depend on whether acl happens to be installed when the recipe is next rebuilt.
- `--without-selinux` — build without `--selinux`, which stores SELinux contexts. KDOS runs no
  SELinux and carries no libselinux (see
  [What is not protected](../../../../docs/kdos/03-architecture/security-model.md#what-is-not-protected)).
- `--with-xattrs` — build tar's `--xattrs` option, which stores and restores extended attributes.
  configure looks for `getxattr`, `setxattr` and `listxattr` and their `f` and `l` variants; musl
  provides all of them, so no extra library is linked. Without them configure would leave the
  option out without an error, so the switch states the intent rather than enforcing it.

## Notes

The port names no dependencies, and the switches above keep it that way: it links musl alone, as
the bootstrap build does, in whichever phase it is rebuilt. `30_foundation` names it again so that
a changed recipe is rebuilt before that phase's shelf sections use it.
