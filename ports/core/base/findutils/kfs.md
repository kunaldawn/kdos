## About
GNU findutils, of which KDOS installs `find`, which walks a directory tree and tests
or acts on each file, and `xargs`, which builds command lines from its input. They are part of the
GNU command line every KDOS system carries (see
[The base userland](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#the-base-userland)).

The other half of the package, `locate`, `updatedb` and their helper `frcode`, is deleted after the
install. `locate` and `updatedb` on KDOS are [plocate](#port-plocate)'s, which read and write a
different index format (see
[Finding a file by name](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/02-user-guide/administration.md#finding-a-file-by-name)).

## Flags
- `--localstatedir=/var/lib/locate` — The directory `updatedb` would keep its database in, which
  `make install` creates. With the locate half deleted, the directory is the switch's only trace
  in the staged tree, and the last line of `build.sh` removes it with the rest of `$PKG/var`.
- `--without-selinux` — Build without libselinux, even when it is installed. `find`'s `-context`
  test and its `%Z` format need it. libselinux is not a port in the tree.

## Patches
- `updatedb-copyright-pipe.patch` — Adds a missing `|` to the rule in `locate/Makefile.am` and `locate/Makefile.in` that generates the `updatedb` script. The rule reads the copyright line out of `updatedb.sh` with `sed` and means to pipe it into `grep .`, so that an empty line fails the build; as released, the line continuation hands `grep` and `.` to `sed` as two more input files instead. The patch changes both the `.am` and the generated `.in`, so the build needs no automake to pick it up.

## Notes
The rule the patch touches runs during `make` even though the program it generates is deleted from
the package afterwards.

The `rm` and `rmdir` lines after the install must match what upstream installs. A `locate` left in
the package would own `/usr/bin/locate`, which plocate installs in a later phase, and kpkg refuses
a package that installs over a path another package owns (see
[Who owns a file](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/03-architecture/packaging.md#who-owns-a-file)).
`rmdir` fails, and the build with it, if `/usr/libexec` or `/usr/share/man/man5` still holds a
file after the deletions.
