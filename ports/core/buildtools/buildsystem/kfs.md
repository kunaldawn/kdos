## About
The NetSurf project's shared build framework: no program, only the makefile fragments that each
NetSurf library's own `Makefile` includes (`Makefile.top`, `Makefile.tools`, `Makefile.subdir`,
`Makefile.pkgconfig` and the per-compiler ones), the `testrunner.pl` test runner and upstream's
CI script, `jenkins-build.sh`. [libnsgif](#port-libnsgif),
[libcss](#port-libcss), [libdom](#port-libdom), [libhubbub](#port-libhubbub) and the other NetSurf
libraries build with it.

## Notes
`make install` copies the files into `$PREFIX/share/netsurf-buildsystem`, and the Makefile's
own `PREFIX` is `/opt/netsurf`, so `PREFIX=/usr` is what puts them in
`/usr/share/netsurf-buildsystem`. A NetSurf library finds them through `NSSHARED`, which each
of their recipes exports as that directory. The plain `make` target only prints usage; there is
nothing to compile.
