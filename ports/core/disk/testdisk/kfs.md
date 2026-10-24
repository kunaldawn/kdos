## About
[TestDisk]{.idx} recovers lost partitions and repairs partition tables and boot sectors;
[PhotoRec]{.idx}, built from the same source, recovers deleted files by carving them out of a disk,
a memory card or a disk image by their content, whatever the filesystem. Both are terminal
programs on ncurses; `qphotorec` is PhotoRec's Qt 6 window. The port builds a pinned commit of
upstream's development line (`_commit` in the recipe).

## Flags
- `--enable-qt` — build `qphotorec` with Qt 6; upstream's default is on, and it turns off quietly
  when Qt 6 is not found, which the `grep` on `src/Makefile` after the configure catches.
- `--enable-sudo=no` — leave out the *Sudo* menu entry, "Use the sudo command to restart as root",
  that the programs offer when they run as a user other than root. It is upstream's default,
  named.
- `--with-ncurses` — the terminal interface; with `--with-ext2fs` ([e2fsprogs](#port-e2fsprogs)'
  libext2fs, to list and recover files from ext2, ext3 and ext4), `--with-jpeg` (checking
  recovered JPEGs), `--with-iconv`, `--with-zlib` and `--with-uuid`, these name libraries the
  configure uses by default. ncurses is always required, and once named, ext2fs and jpeg stop the
  configure when they are missing.
- `--with-ntfs3g` — read NTFS through [ntfs-3g](#port-ntfs-3g)'s library, to list and undelete
  files on it. `--without-ntfs` leaves out the alternative, the older `libntfs` from ntfsprogs,
  which the tree does not carry.
- `--without-reiserfs` — no ReiserFS support, which needs `libreiserfs` from progsreiserfs; the
  tree does not carry it.
- `--with-ewf` — read and write Expert Witness (EnCase `.E01`) disk images through
  [libewf](#port-libewf).

## Notes
The source is the archive of one commit, which carries `configure.ac` but no generated
`configure`, so `build.sh` first runs `autoreconf -f -i`; [autoconf](#port-autoconf) and
[automake](#port-automake) are in `depends` for it.

The configure only warns when ntfs-3g or libewf is missing, and drops Qt 6 without a word, so
`build.sh` checks the result before it compiles: `config.h` must define
`HAVE_LIBNTFS3G` and the two libewf functions `HAVE_LIBEWF_HANDLE_READ_BUFFER_AT_OFFSET` and
`HAVE_LIBEWF_HANDLE_WRITE_BUFFER_AT_OFFSET`, the current libewf calls `src/ewf.c` reads and
writes images with (without them it falls back to the older `libewf_handle_read_random` family),
and `qphotorec_LDADD` in `src/Makefile` must link `Qt6Widgets`. A build that would
lack any of them stops with a message naming it.

Upstream installs `linux/qphotorec.desktop`; `build.sh` writes its own over it, adding
`StartupWMClass=qphotorec` and search keywords.
