## About
GNU Automake turns a project's `Makefile.am` files into the `Makefile.in` templates its
`configure` fills in, and `aclocal` collects the macros `configure.ac` uses into `aclocal.m4`. Like
[autoconf](#port-autoconf), it matters to a recipe that regenerates its build system with
`autoreconf`, which runs `aclocal` and `automake` from this port, and it is in the phase's opening
run because such a recipe calls it without naming automake in `depends`.

## Notes
`build.sh` deletes automake's own copy of `texinfo.tex`, `/usr/share/automake-<version>/texinfo.tex`,
after the install, and no other package installs one there. `automake --add-missing`, which
`autoreconf -i` runs, copies that file into a project whose `Makefile.am` lists `info_TEXINFOS` and
whose source carries no `texinfo.tex` of its own, in the top directory or in the directory
`AC_CONFIG_AUX_DIR` names. Without the copy, automake stops such a project with an error that the
required file `texinfo.tex` is not found, unless the project sets automake's `no-texinfo.tex`
option or names its own copy in `TEXINFO_TEX`.
