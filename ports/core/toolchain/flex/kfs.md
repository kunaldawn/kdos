## About
flex is a fast lexical analyser generator: from a set of regular expressions and actions it
writes a C scanner. It is the free implementation of `lex`. It installs `flex`, `flex++`,
the `FlexLexer.h` header for C++ scanners, and `libfl`, which supplies a default `main` and
`yywrap` for scanners that do not define their own.

In KDOS it is a build-time tool for many ports, most of them the same ones that need
[bison](#port-bison): [linux](#port-linux), [mesa](#port-mesa), [nftables](#port-nftables),
[pam](#port-pam), [doxygen](#port-doxygen), [wireshark](#port-wireshark) and others.

## Notes
`build.sh` adds `lex` as a link to `flex`, and `lex.1` as a link to `flex.1`. flex's own install
makes neither. GNU make's built-in rules and many makefiles call the scanner generator `lex`
(make's default is `LEX = lex`), and without the link such a build finds no command.

Like Bison, flex runs m4 as it generates a scanner, by the absolute path configure found,
`/usr/bin/m4`, so [m4](#port-m4) is a run-time dependency.
