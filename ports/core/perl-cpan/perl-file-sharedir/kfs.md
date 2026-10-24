## About

File::ShareDir is the run-time half of the shared-files convention for Perl: given a distribution or
a module name, it returns the directory that distribution's data files were installed in, under
`auto/share` in Perl's module tree. It uses [Class::Inspector](#port-perl-class-inspector) to find
an installed module's file, and is built with
[File::ShareDir::Install](#port-perl-file-sharedir-install), which installs its own sample files.

In the tree it is there for [perl-xml-parser](#port-perl-xml-parser): XML::Parser's Expat module
calls `File::ShareDir::dist_dir('XML-Parser')` to find its `.enc` encoding maps.

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
