## About

Class::Inspector answers questions about a Perl class at run time: whether it is loaded, where its
file is, which methods and subroutines it defines. It is pure Perl, with no compiled part.

It is in the tree for one reason: [perl-file-sharedir](#port-perl-file-sharedir) requires it, and
uses it to find the file of an installed module when it looks up that module's shared files.
File::ShareDir in turn is what [perl-xml-parser](#port-perl-xml-parser) finds its encoding maps
with, which is why all three are built in `30_foundation`, with the [intltool](#port-intltool) that
needs XML::Parser.

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
