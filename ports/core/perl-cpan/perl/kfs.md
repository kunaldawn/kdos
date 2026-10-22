## About

Perl 5, the interpreter, its core modules and its documentation tools (`perldoc`, `pod2man`, `cpan`,
`h2xs` and the rest). Much of the build runs on it without naming it: automake, autoconf's helper
scripts, OpenSSL's `Configure`, `pod2man` for manual pages, and the generators in many upstream
build systems. `perl` is one of the tools `30_foundation` installs first, before any shelf: the
phase list names it among "the build tools recipes reach for without naming them in `depends`"; see [the pinned
run](#06-foundation--the-pinned-run) in that phase's part. Every `perl-*` port, such as
[perl-xml-parser](#port-perl-xml-parser), builds its modules against this interpreter.

The module directories come in three sets: the core library in `core_perl`, packaged CPAN modules in
`vendor_perl`, and modules installed by hand in `site_perl`, each with a `/usr/share/perl5` half for
pure-Perl files and a `/usr/lib/perl5` half for compiled ones. The interpreter is threaded and
shared. `build.sh` also writes a `config.over` that fixes the host name, kernel version and build
time `Configure` would otherwise take from the build machine, so the recorded configuration is the
same wherever it is built. Why each of these is set, and what it means for a `perl-*` recipe, is
under [Python, Perl, Haskell, Node and OCaml
modules](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#python-perl-haskell-node-and-ocaml-modules)
and in the [perl-cpan
notes](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/06-reference/ports-catalogue.md#perl-cpan)
of the ports catalogue.

## Flags

- `BUILD_ZLIB` — set to `False`, with `BUILD_BZIP2=0` beside it, in the environment `Configure` and
  `make` inherit. The core modules Compress::Raw::Zlib and Compress::Raw::Bzip2 then link the
  system's `libz` and `libbz2`, from [zlib](#port-zlib) and [bzip2](#port-bzip2), instead of
  compiling the copies of those libraries bundled in their source directories.
- `-des` — three `Configure` options: `-d` takes the default answer to every question, `-e` goes on
  past writing `config.sh` without stopping, and `-s` prints only the essentials. Together they run
  `Configure` unattended.
- `-Dprefix=/usr` — with `-Dvendorprefix=/usr`, the install prefix of the interpreter and of
  packaged modules.
- `-Dprivlib` — with `-Darchlib`, `-Dvendorlib`, `-Dvendorarch`, `-Dsitelib` and `-Dsitearch`: the
  six module directories. `privlib`, `vendorlib` and `sitelib` hold pure-Perl modules under
  `/usr/share/perl5/{core,vendor,site}_perl`; `archlib`, `vendorarch` and `sitearch` hold compiled
  ones, and the `CORE` directory with `libperl.so` and the headers, under
  `/usr/lib/perl5/{core,vendor,site}_perl`. A `perl-*` port installs into the vendor pair with
  `INSTALLDIRS=vendor`; the site pair is left to the machine's owner, and no package writes there.
- `-Dman1dir=/usr/share/man/man1` — with `-Dman3dir=/usr/share/man/man3`: where the program and
  module manual pages go.
- `-Dman3ext=3pm` — module pages get the section suffix `3pm`, so `File::Spec`'s page cannot share a
  name with a C library's section 3 page.
- `-Doptimize="$CFLAGS"` — with `-Dldflags="$LDFLAGS"` and `-Dlddlflags="-shared $LDFLAGS"`: the
  tree's compiler and linker flags. `Configure` reads neither `CFLAGS` nor `LDFLAGS` from the
  environment, and the values given here are recorded in `Config`, so every XS module built later
  compiles and links with them as well.
- `-Duseshrplib` — build the interpreter as a shared `libperl.so` instead of the default static
  `libperl.a`. `perl` links it, and so can programs that embed Perl: in this tree,
  [postgresql](#port-postgresql)'s PL/Perl (`--with-perl`) and [perf](#port-perf) (`LIBPERL=1`).
- `-Dusethreads` — a threaded interpreter, archname `x86_64-linux-thread-multi`. An XS module is
  compiled for that ABI, so with this switch changed every `perl-*` port with C in it has to be
  rebuilt.
- `-Dloclibpth=' '` — with `-Dlocincpth=' '` and `-Dglibpth='/usr/lib'`: the library and include
  directories `Configure` searches and records. Its defaults list `/usr/local/lib`,
  `/usr/local/include`, `/opt/local` and the GNU directories (`/usr/gnu`, `/opt/gnu`), and its
  `glibpth` adds `/usr/ccs/lib`, `/usr/ucblib` and `/usr/local/lib` to `/lib` and `/usr/lib`; set to blank and to `/usr/lib` alone,
  they keep every such path out of the recorded flags that each XS build reuses.
- `-Ui_db` — leave Berkeley DB out: `Configure` would build the `DB_File` module whenever it finds
  `db.h`, so the result would depend on what the build tree held. `DB_File` is the
  [perl-db-file](#port-perl-db-file) port, built once [db](#port-db) exists.
- `-Ud_eaccess` — record that the C library has no `eaccess()`. Under the `use filetest 'access'`
  pragma, Perl's `-r`, `-w` and `-x` tests for the effective user then use its own emulation, which
  swaps the real and effective IDs around a plain `access()` call, instead of calling `eaccess()`.
  `Configure` has no variable named `d_euidaccess`, so `-Ud_euidaccess` beside it only adds an
  unused entry to `config.sh`.

## Patches

- `liblist-default-dirs.patch` — changes ExtUtils::MakeMaker's `ExtUtils/Liblist/Kid.pm`, the code that turns an XS module's `-l` flags into a link line. It adds the directory of every shared library it finds to `LD_RUN_PATH`, which becomes the module's run path; the patch skips `/lib` and `/usr/lib`, where the dynamic linker looks anyway. Without it every XS module linked against a system library would carry a `/usr/lib` run-path entry, which the package lint warns on (see [What the package lint refuses](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-the-package-lint-refuses)).

## Notes

`config.over` is a file `Configure` reads last, after its own probes, so the values in it win.
`build.sh` writes the host name and domain (`localhost`, `.localdomain`), the administrator's
address, `cf_by='kdos'`, the kernel version taken from the installed kernel headers
(`/usr/include/linux/version.h`) rather than from `uname`, and `cf_time` from `SOURCE_DATE_EPOCH`.
Without it, `Config_heavy.pl` and `CORE/config.h` would record the build machine's name, running
kernel and the time of the build, and two builds of the same recipe would differ.

The final `ln -s` puts `libperl.so` in `/usr/lib` as a link to the real library in `CORE/`, so a
program that embeds Perl links with a plain `-lperl` and loads it at run time with no run path of
its own.

GDBM_File, NDBM_File and ODBM_File are built only when `Configure` finds the headers they need
(`i_gdbm` for the first; `ndbm.h` and `dbm.h`, which gdbm's compatibility layer provides, for the
other two), so like `DB_File` they follow what is installed. [gdbm](#port-gdbm) is in `depends`,
so it is installed first: GDBM_File links `libgdbm`, and NDBM_File and ODBM_File link
`libgdbm_compat`.
