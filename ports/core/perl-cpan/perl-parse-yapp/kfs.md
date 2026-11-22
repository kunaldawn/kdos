## About

Parse::Yapp is an LALR parser generator written in Perl, the Perl counterpart of yacc: `yapp`
compiles a grammar into a Perl parser module. In KDOS it is a build dependency of samba, whose IDL
compiler `pidl` is built with it.

## Notes

The source keeps its licence in the `COPYRIGHT` section of `lib/Parse/Yapp.pm`, which grants use
under the terms of Perl itself (the GPL or the Artistic License), and ships neither text. The last
lines of `build.sh` install that module beside the `README` under
`/usr/share/licenses/perl-parse-yapp`, with perl's own `/usr/share/licenses/perl/Copying`, the GPL
text, which is there because `perl` is in `depends`.
