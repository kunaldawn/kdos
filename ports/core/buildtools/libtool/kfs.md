## About
GNU libtool hides the differences between platforms' ways of building and linking shared
libraries behind one script. A package that uses it carries `ltmain.sh` and libtool's macros,
copied in by `libtoolize`, and its configure writes a `libtool` script from them. A recipe that
runs `autoreconf -i` on a project whose `configure.ac` uses those macros runs `libtoolize` from
this port, whether or not it names libtool in `depends`, and that is why the phase's opening run
names it. The port also installs `libltdl`, a portable wrapper around `dlopen` through which some
programs load their plugins.

## Notes
The `.la` files libtool writes beside each library are removed from every package by kpkg itself,
so no recipe deletes them (see [Anatomy of a kpkg install](../../../../docs/kfs/04-chroot/kpkg-install.md)).

A configure script generated with libtool's macros looks for [file](#port-file), which the same run
names, and on x86-64 Linux runs it on a test object to learn whether the compiler produces 32-bit
code, which needs a different linker emulation. When `file` is missing, the check reads nothing and
the linker is left as it is.
