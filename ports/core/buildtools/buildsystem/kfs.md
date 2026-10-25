## About
The NetSurf project's shared build framework: no program, only the makefile fragments
(`Makefile.top`, `Makefile.tools`, `Makefile.pkgconfig` and the per-compiler ones) and a test
runner that each NetSurf library's own `Makefile` includes. [libnsgif](#port-libnsgif),
[libcss](#port-libcss), [libdom](#port-libdom), [libhubbub](#port-libhubbub) and the other NetSurf
libraries build with it.

## Notes
`make install` copies the files into `$PREFIX/share/netsurf-buildsystem`, and the Makefile's
own `PREFIX` is `/opt/netsurf`, so `PREFIX=/usr` is what puts them in
`/usr/share/netsurf-buildsystem`. A NetSurf library finds them through `NSSHARED`, which each
of their recipes exports as that directory. The plain `make` target only prints usage; there is
nothing to compile.
