## About

File::ShareDir::Install is the build-time half of File::ShareDir. A distribution's `Makefile.PL`
calls it to declare a directory of data files, such as XML::Parser's encoding maps, and it adds the
rules that install them under `auto/share/dist/<distribution>` in Perl's module tree, where
[File::ShareDir](#port-perl-file-sharedir) finds them at run time. It is pure Perl and installs no
program.

Two ports in the tree name it in `depends`. [perl-xml-parser](#port-perl-xml-parser)'s `Makefile.PL`
loads it with a plain `use`, so XML::Parser cannot even be configured without it.
[perl-file-sharedir](#port-perl-file-sharedir) loads it through `inc::latest`, which takes the newer
of this installed copy and the one bundled in File::ShareDir's own `inc/` directory, to install its
sample files.

## Flags

- `INSTALLDIRS=vendor` — install into Perl's vendor directories rather than MakeMaker's default, the
  site ones. The [perl](#port-perl) port sets them to `/usr/share/perl5/vendor_perl` for pure-Perl
  modules and `/usr/lib/perl5/vendor_perl` for compiled ones; the site directories are left to
  modules a machine's owner installs by hand, and no package writes there.

## Notes

`make install` writes two records of its own, and `build.sh` deletes both from `$PKG`: a `.packlist`
listing the files the distribution installed, and `perllocal.pod`, a file in Perl's `archlib` to
which every MakeMaker install appends an entry. kpkg's database already records what the package
installed, and `perllocal.pod` is one path that every `perl-*` package would carry, which kpkg
refuses once a second package claims it. The last `find` then removes the directories those
deletions leave empty.
