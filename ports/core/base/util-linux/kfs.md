## About
util-linux is the collection of Linux system programs that are not part of coreutils: `mount` and
`umount`, `blkid`, `fdisk` and `sfdisk`, `losetup`, `swapon`, `switch_root`, `agetty` and
`sulogin`, `dmesg`, `lsblk`, `setpriv`, `last`, `wall`, `kill` and many more, and the
libraries `libblkid`, `libmount`, `libuuid`, `libfdisk`, `libsmartcols` and `liblastlog2`.

On KDOS these are load-bearing at boot. The initramfs copies `switch_root`, `mount`, `umount`,
`losetup`, `dmesg`, `setsid` and `blkid` and checks that each is util-linux's
([Tools that must be util-linux's](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/boot-and-init.md#tools-that-must-be-util-linuxs)),
every console login ends in `agetty`, single-user mode is `sulogin`, and service scripts such as
`86_kiwix-serve.sh` drop privileges with `setpriv`. Many ports depend on its libraries, among them
[python3](#port-python3), [glib](#port-glib), [eudev](#port-eudev), [e2fsprogs](#port-e2fsprogs)
and [lvm2](#port-lvm2).

The build leaves out every program another package owns here. `login`, `su`, `nologin`, `chsh` and
`chfn` are [shadow](#port-shadow)'s, and kpkg refuses to install a path a second package already
claims ([Who owns a file](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#who-owns-a-file)).

## Flags
- `--disable-chfn-chsh`, `--disable-login`, `--disable-nologin`, `--disable-su` — Build none of
  these. shadow installs `chfn`, `chsh`, `login`, `nologin` and `su`, so util-linux's would collide
  with its files; the `su` on the image is shadow's alone
  ([setuid binaries](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#setuid-binaries)).
- `--disable-runuser` — Build no `runuser`. Like util-linux's `su` and `login`, it needs PAM, which
  is a `41_system` port; configure would otherwise build it or skip it depending on whether PAM
  happened to be installed.
- `--enable-setpriv`, `--with-cap-ng` — Build `setpriv`, which runs a program with other user and
  group IDs, capabilities and securebits, against [libcap-ng](#port-libcap-ng). The init scripts
  of `llama-server`, `kiwix-serve` and `kolibri` start their daemons through it. Named, a missing
  libcap-ng stops configure rather than dropping the program.
- `--disable-pylibmount`, `--without-python` — No Python binding for libmount. [python3](#port-python3)
  depends on util-linux, so it cannot be installed when this builds.
- `--without-systemd`, `--without-systemdsystemunitdir` — No `libsystemd` journal and session
  support and no unit files. KDOS has no systemd
  ([No systemd](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/01-philosophy/principles.md#no-systemd)).
- `--without-udev`, `--disable-libmount-udev-support` — Do not ask `libudev` for device properties
  in `lsblk` and `libmount`. [eudev](#port-eudev), which provides `libudev`, depends on util-linux and
  is built after it. `lsblk` then reads labels, UUIDs and file system types by probing the device
  through `libblkid`.
- `--with-libz` — Link zlib, the one library `fsck.cramfs` and `mkfs.cramfs` require; configure
  leaves both out without it.
- `--with-btrfs` — Let `libmount` take btrfs's default subvolume into account when it matches a
  mounted btrfs file system against an `fstab` entry. Named, a missing `<linux/btrfs.h>` stops
  configure. The installer can put the root on btrfs.
- `--with-readline` — Line editing at the `fdisk` and `sfdisk` prompts, through
  [readline](#port-readline).
- `--with-ncursesw` — Build the terminal programs that use curses (`cfdisk`, `more`, `pg`, `ul`,
  `setterm`) against the wide-character ncurses.
- `--enable-liblastlog2` — Build `liblastlog2` and `lastlog2`, the login-time database kept in
  SQLite, through [sqlite](#port-sqlite).
- `--disable-pam-lastlog2` — No PAM module for lastlog2; PAM is not built yet.
- `--without-libmagic` — Do not link libmagic into `more`. `more` uses it only to decide that a
  file is not text; without it, it recognises ELF and a.out headers itself and pages everything
  else.
- `--without-econf`, `--without-user` — No libeconf configuration parsing and no libuser remote
  `chsh`. Neither library is in the tree.
- `--disable-asciidoc`, `--disable-poman` — Generate no manual pages with asciidoctor and no
  translated ones with po4a. The release tarball carries the pages already built, and they are
  installed as they are.
- `--disable-makeinstall-chown` — Do no ownership changes during `make install`: no `chgrp tty`
  and `chmod g+s` on `wall`, and no `chown root:root` on `mount` and `umount`. `wall` is installed
  as an ordinary program, mode 755, so it writes only to terminals its caller may already write
  to. `write`, the other program the hook would touch, is off by upstream's default and is not
  built.
- `--disable-makeinstall-setuid` — Do not make `mount` and `umount` setuid. Removable media go
  through `kdos-mountd` instead, and these two are on the list of what is deliberately not setuid
  ([setuid binaries](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/security-model.md#setuid-binaries)).
- `CPPFLAGS=-I/usr/include/utmps` — Compile against [utmps](#port-utmps)' `<utmpx.h>` instead of
  musl's, whose functions are stubs, so `agetty`, `last` and `utmpdump` read and write real login
  records. `make` and `make install` are both given `LIBS="-lutmps -lskarnet"`, the library and
  the skalibs it is built on.

## Patches
- `utmps-paths.patch` — In `include/pathnames.h`, when utmps' `<utmpx.h>` is the one included, redefines `_PATH_UTMP` as utmps' `UTMPX_FILE` (`/run/utmps/utmp`) and `_PATH_WTMP` as `/var/log/wtmp`. musl's `<paths.h>` puts both under `/dev/null/`, a path that cannot exist. Writers do not mind, since utmps sends their records to its daemons whatever file they name, but `last` and `lslogins` open the history file themselves and, with musl's path, find none.

## Notes
`blkid`, `losetup` and `switch_root` live in `/usr/sbin`. A second copy of any of them under
`/usr/bin`, which `$PATH` searches first, would shadow util-linux's everywhere except in the
initramfs, which copies them by full path.
