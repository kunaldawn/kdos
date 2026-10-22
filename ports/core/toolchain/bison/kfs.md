## About
GNU Bison generates parsers: from a grammar it writes C, C++ or Java code for a deterministic
LR or a generalised LR parser. It is a compatible replacement for `yacc`, and installs a `yacc`
command, a script that runs `bison -y`, and `liby.a`, the small library of the `main` and
`yyerror` that POSIX yacc programs may link.

In KDOS it is a build-time tool for many ports, from [linux](#port-linux) itself and
[binutils-avr](#port-binutils-avr) to [mesa](#port-mesa), [gobject-introspection](#port-gobject-introspection),
[nftables](#port-nftables), [iptables](#port-iptables), [wireshark](#port-wireshark),
[graphviz](#port-graphviz), [doxygen](#port-doxygen) and [vala](#port-vala). It is built in
`30_foundation` with the other build tools.

## Notes
Bison runs m4 every time it generates a parser. configure records the m4 it found as an absolute
path, `/usr/bin/m4`, in the program, so [m4](#port-m4) is a run-time dependency as well as a
build one: without it Bison cannot generate a parser.
