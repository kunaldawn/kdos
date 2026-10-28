## About

XML::Parser is Perl's binding to [expat](#port-expat), the stream-oriented XML parser in C: an XS
module, `XML::Parser::Expat`, compiled against `libexpat`, with the higher-level `XML::Parser` and
its parsing styles on top. Beyond the encodings expat knows itself, it reads the others from
encoding maps, `.enc` files shipped in the release and installed as the distribution's shared files
under `auto/share/dist/XML-Parser`, where it finds them with
[File::ShareDir](#port-perl-file-sharedir). Those maps are among the
[data files with no other source form](../../../../docs/kdos/01-philosophy/why-kdos.md#data-with-no-other-source-form)
the tree carries.

[intltool](#port-intltool), the tool that extracts translatable strings from source files, needs it,
and intltool is in `30_foundation`, so this module and the modules it needs are built there too.
Later on, [xmltoman](#port-xmltoman), [libpulse](#port-libpulse),
[perl-xml-simple](#port-perl-xml-simple), Xfce's [exo](#port-exo), [libxfce4ui](#port-libxfce4ui)
and [xfconf](#port-xfconf), and GTK applications such as [gnumeric](#port-gnumeric) and
[brasero](#port-brasero) depend on it.

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

`Makefile.PL` finds expat through `pkg-config` and then checks that `expat.h` and `libexpat` compile
and link. When they do not, it prints "Expat must be installed prior to building XML::Parser" and
exits successfully without writing a `Makefile`, so the failure shows up in the log one step later,
as `make` finding nothing to build. Read the `perl Makefile.PL` output first.

`Expat.so` carries no run path even though it links `libexpat`, because the [perl](#port-perl)
port's `liblist-default-dirs.patch` keeps `/usr/lib` out of MakeMaker's `LD_RUN_PATH`.
