## About
shadow is the account suite: `login`, `su`, `passwd`, `useradd` and the other `user*` and `group*`
programs, `chsh`, `chfn`, `newgrp`, `vipw`, `lastlog`, and `newuidmap` and `newgidmap` with their
library `libsubid`. It owns `/etc/login.defs`, the defaults those programs read.

On KDOS it is the only owner of these names. util-linux is built without its own `login`, `su`,
`nologin`, `chsh` and `chfn` (see [util-linux](#port-util-linux)), so a console login ends in
util-linux's `agetty` handing over to shadow's `login`. `newuidmap` and `newgidmap` are what every
boxed application depends on: the container engine runs them to write a rootless container's
user-namespace map, and without their setuid bits no box starts
([setuid binaries](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#setuid-binaries)).

The build links no PAM: [pam](#port-pam) is a `41_system` port, and `login`, `su` and `passwd` here
check `/etc/shadow` themselves.

## Flags
- `--disable-logind` — Build no logind integration. Left on, which is upstream's default, configure
  stops unless it finds `libsystemd`; KDOS has no systemd
  ([No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--disable-rpath` — Embed no run-time library search path in the installed programs.
- `--enable-lastlog` — Record each login in `/var/log/lastlog` and install the `lastlog` program.
  Upstream's default is off; it needs `<lastlog.h>`, which musl provides.
- `--with-bcrypt` — Accept `BCRYPT` as `ENCRYPT_METHOD` in `/etc/login.defs`. Hashing goes through
  musl's `crypt()`, which implements bcrypt.
- `--without-yescrypt` — Do not offer `YESCRYPT`: musl's `crypt()` has no yescrypt, so a hash
  written with it could never be checked.
- `--without-libbsd` — Compile shadow's own copies of `readpassphrase()` and `freezero()`
  instead of taking them from libbsd. Left at its default, configure looks for `readpassphrase()`
  in the C library or in libbsd and stops when neither has it; musl does not, and
  [libbsd](#port-libbsd) is a `41_system` port, not installed when this builds.
- `--without-libpam` — Check passwords against `/etc/shadow` directly, with no PAM. PAM is built
  in a later phase.
- `--with-acl` — Copy POSIX ACLs along with the files when `useradd -m` fills a new home from
  `/etc/skel` and `usermod -m` moves one, through `perm_copy_fd()` from [acl](#port-acl);
  configure also needs `<attr/error_context.h>` from [attr](#port-attr). Named, a missing header
  or library stops configure instead of dropping the feature.
- `--with-btrfs` — Let `useradd` create a home directory as a btrfs subvolume
  (`--btrfs-subvolume-home`, or `BTRFS_SUBVOLUME_HOME` in `login.defs`). The installer can put the
  root on btrfs ([kinstall](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/04-programs/kinstall.md)).
- `--without-audit`, `--without-selinux`, `--without-tcb` — No Linux audit records, no SELinux
  labels and no per-user `tcb` shadow files. None of the three libraries is in the tree.
- `--with-group-name-max-length=32` — Refuse group names longer than 32 characters. It is
  upstream's default value, stated.
- `shadow_cv_maildir=none`, `shadow_cv_mailfile=none` — Compile in no mail spool directory and
  no mailbox file name. configure otherwise takes the first of `/var/mail`, `/var/spool/mail` and
  two others that exists in the build root, and the first of `Mailbox`, `mail`, `.mail` and two
  others that exists in the builder's home, and the tools fall back to them whenever `login.defs`
  names neither, which with `no-mail-spool.patch` (see Notes) would bring back the mailbox the
  image has no directory for, or make `login` print `No mail.`.
- `CPPFLAGS=-I/usr/include/utmps`, `LIBS="-lutmps -lskarnet"` — Build `login` against
  [utmps](#port-utmps), so it writes the login records `who` and `last` read. musl's own `utmpx`
  functions are stubs; see
  [Login records](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#login-records).

## Notes
`no-mail-spool.patch` comments out `MAIL_DIR` in `/etc/login.defs`, and `MAIL_FILE` stays
commented, so no tool keeps a mailbox. Nothing on KDOS delivers local mail and the image has no
spool directory: with the line in place `useradd` prints `Creating mailbox file: No such file or
directory` for every account it makes, and `userdel -r` prints `mail spool
(/var/spool/mail/<user>) not found` for every one it removes. `login` then also sets no `MAIL`, so
it prints no `No mail.`. `useradd`'s compiled `CREATE_MAIL_SPOOL` is `yes`, and no configure
switch changes it; with no `MAIL_DIR` it creates no mailbox all the same, and the base file
system's `/etc/default/useradd` sets the key to `no`, so `useradd -D` reports what `useradd`
does.

`build.sh` ends by setting mode 4755 on `newuidmap` and `newgidmap` itself. `make install` sets the
bit already, unless shadow is configured `--with-fcaps` to rely on file capabilities instead;
repeating it means no configure change can leave the two without it. A file capability would not
survive kpkg's package archive, which is written without extended attributes
([setuid binaries](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#setuid-binaries)).
`kdos doctor` checks the two bits, because losing them is silent until a box fails to start.

The hash `passwd` and `chpasswd` write is set by `ENCRYPT_METHOD` in `/etc/login.defs`. The
installed file leaves that line commented out, so they use shadow's built-in choice, SHA-512.
Setting it to `BCRYPT` works because of `--with-bcrypt`; `YESCRYPT` is taken for an invalid value,
with a warning, and SHA-512 is used instead.
