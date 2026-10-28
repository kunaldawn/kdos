## About
acl is the library and tools for POSIX [access control lists]{.idx}, the per-user and per-group
permissions a file can carry beyond its owner, group and mode bits. `getfacl` and `setfacl` read and
write them, and `chacl` is an IRIX-compatibility command for the same. libacl is what lets
[coreutils](#port-coreutils) and [sed](#port-sed) copy a file's ACL along with its data, and
[libarchive](#port-libarchive) store ACLs in an archive; [samba](#port-samba), [cups](#port-cups)
and [udisks2](#port-udisks2) link it too.

## Flags
- `--bindir=/bin` — Install the tools in `/bin`, which is a link to `/usr/bin`; the package
  records them as `./bin/...`, and kpkg folds that link when it compares ownership.
- `--libexecdir=/usr/lib` — A directory nothing in acl's build installs into, so it places no
  file.

## Notes
The recipe depends on [attr](#port-attr) because configure stops with "FATAL ERROR:
attr/error_context.h does not exist" without attr's headers: libacl's `perm_copy_file` and
`perm_copy_fd`, which copy a file's permissions and ACL, report errors through attr's error
context.
