# Readline

GNU readline is the line-editing library behind an interactive shell prompt: cursor movement, the
history list, completion. The [bootstrap]{.idx} builds it for one consumer, the bash of the
[next step](080-bash.md), which links this copy instead of the one bundled in its own source.
`070_readline.sh` cross-compiles the shared libraries from the [readline](#port-readline) port's
source tarball against the ncurses that [`040_ncurses.sh`](040-ncurses.md) installed.

**Approximate build time:** 0.1 SBU

## Installation of readline

The step unpacks the port's tarball and builds in a `build` directory inside it, so the source tree
itself is never written:

<!-- kfs:include script/phases/10_bootstrap/070_readline.sh lines=24-39 pin=e48860606062 -->

`extract_port_source` unpacks the tarball the recipe names first and nothing else, so the official
upstream patch series the port carries beside it (`readline<ver>-NNN`, numbered from 001) is not
applied here.

### The meaning of the configure options

`--host=$KDOS_TARGET`
: Build for KDOS with the cross compiler. readline's configure is given no `--build` and works out
  the build machine with its own `config.guess`.

`--prefix=/usr`
: The libraries go to `/usr/lib`, the headers to `/usr/include/readline`.

`--disable-static`
: Install only `libreadline.so` and `libhistory.so`, with no `.a` archives beside them. Nothing in
  the bootstrap links readline statically.

### The meaning of the make variable

`SHLIB_LIBS="-lncursesw"`
: Link `libreadline.so` against ncurses' wide-character library, so the shared library itself
  records that it needs `libncursesw.so`. readline calls the terminal functions, and built without
  this it leaves them for every program that links readline to supply. Given on the `make` command
  line, the variable overrides the value configure wrote into the makefile.

### Removing the HTML manuals

The `rm` deletes the HTML manuals the install writes under `/usr/share/doc/readline`. kpkg prunes
HTML manuals from every package (see
[Size and language policy](../../kdos/05-developer/writing-ports.md#size-and-language-policy)), so
the readline port never installs these files again, and a copy left here would be adopted by no
package and outlive every install (see [Part III](intro.md#what-the-bootstrap-leaves-behind)).

The step then removes the source and touches `build/mark/bootstrap/readline`.

## Built again later

`30_foundation` installs the [readline](#port-readline) port, which adopts both libraries and the
headers. The port build:

- applies the upstream patch series in order before configuring;
- reaches the same link to ncurses through configure instead of `make`, with `--with-curses`,
  `--with-shared-termcap-library` and `bash_cv_termcap_lib=libncursesw`;
- overrides `SHLIB_XLDFLAGS` with the soname alone. readline's default for Linux, which the
  bootstrap build keeps, also passes `-Wl,-rpath,$(libdir)`, so the bootstrap libraries carry a
  run-time search path naming `/usr/lib`. The port fails its build if `readelf` finds an `RPATH` or
  `RUNPATH` in `libreadline.so`;
- passes `--disable-install-examples`.

## See also

- [Sed](068-sed.md), the step before this one, and [Bash](080-bash.md), the step after it
- [The readline port](#port-readline): its version, source and contents
- [What a build verifies](../../kdos/03-architecture/packaging.md#what-a-build-verifies): the checks
  every package passes, which these hand-installed libraries never meet

<!-- book-nav -->
---

*Part III. The bootstrap, chapter 23.* Previous: [22. Sed](068-sed.md) · [Contents](../README.md) · Next: [24. Bash](080-bash.md)
