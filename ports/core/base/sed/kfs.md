## About
GNU sed, the stream editor. Configure scripts and Makefiles run it throughout the build, and the
recipe's description names the reason the tree carries GNU's: the extensions upstream build systems
assume (see
[The base userland](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#the-base-userland)).
It is also one of the programs the initramfs carries for the rescue shell, where the image build
stops unless its `--version` names GNU (see
[The initramfs](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#the-initramfs)).

The port replaces the copy the bootstrap built and adds what that one has no libraries for: `sed -i`
gives the file it writes the original's access control lists.

## Flags
- `--bindir=/bin` — Install `sed` as `/bin/sed`. `/bin` is a link to `/usr/bin`, so the program
  lands in the same directory as with the default; the package records it as `./bin/sed`, and kpkg
  compares ownership on the path with that link folded (see
  [Who owns a file](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#who-owns-a-file)).
- `--enable-acl` — Copy the input file's access control list to the file `sed -i` writes, through
  gnulib's `qcopy_acl`. Upstream's default is `auto`, which warns and builds without ACLs when
  libacl is missing; `yes` makes configure stop with "ACLs enabled but support not detected"
  instead. The recipe depends on [acl](#port-acl), so the library is there.
- `--enable-xattr` — Let that copy go through libattr: `qcopy_acl` then copies the ACL attributes
  (`system.posix_acl_access`, `system.posix_acl_default`, `system.nfs4_acl`) whole with
  `attr_copy_fd`, which carries an NFSv4 ACL as well as a POSIX one. It is upstream's default, and
  configure only warns and builds without it when libattr is missing; the recipe depends on
  [attr](#port-attr).
- `--without-libsmack` — Do not look for libsmack, the Smack security module's library, which
  gnulib's ACL code uses to read a file's Smack label. It is not a port in the tree.
- `--without-selinux` — Build without libselinux. With it, `sed -i` gives the file it writes the
  original's SELinux context. It is not a port in the tree.
