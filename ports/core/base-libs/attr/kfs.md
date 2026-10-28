## About
attr is the library and tools for [extended attributes]{.idx}, the name-value pairs a file system
keeps beside a file's data. `getfattr`, `setfattr` and `attr` read and write them; `libattr`'s
`attr_copy_file` and `attr_copy_fd` copy them from one file to another. Those two functions are why
the base userland depends on it: [coreutils](#port-coreutils) (`cp -a`, `mv` across file
systems), [sed](#port-sed) and [patch](#port-patch) call them so that a file they copy or rewrite
keeps its attributes, and [acl](#port-acl)'s configure requires its `attr/error_context.h`. The
bootstrap's builds of those programs have no libattr; see
[A minimal KDOS: `10_bootstrap`](../../../../docs/kdos/05-developer/how-kdos-is-built.md#a-minimal-kdos-10_bootstrap).

## Flags
- `--bindir=/bin` — Install the tools in `/bin`, which is a link to `/usr/bin`; the package
  records them as `./bin/...`, and kpkg folds that link when it compares ownership.
- `--sysconfdir=/etc` — The directory compiled into libattr as `SYSCONFDIR`, from which
  `attr_copy_file` and `attr_copy_fd` read `/etc/xattr.conf`, and into which the package installs
  that file. The tree's `config.site` already turns a `/usr` prefix's `${prefix}/etc` into `/etc`, so
  the switch names that choice; without either, both the file and the path libattr reads would be
  `/usr/etc/xattr.conf`, outside `/etc`, where the system's configuration is kept.

## Notes
`/etc/xattr.conf` decides what every caller of the copy functions carries over. Each line names an
attribute pattern and an action: `permissions` marks the ACL attributes, which a caller copies only
when it preserves permissions, and `skip` marks attributes no copy should carry, such as XFS's
internal ones and `security.evm`, which only the kernel may write. An attribute the file does not
name is copied.
