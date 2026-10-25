# The build foundation

<!-- kfs:phase 30_foundation -->

`30_foundation` installs what every later recipe builds with: make, pkgconf, the autotools, CMake,
Meson and Ninja; Perl and Python with the modules their build backends need; the documentation
tools; the base libraries for compression, TLS, networking, XML and text; and the system's own
foundation of init, device manager, accounts and login records. It also turns the rest of the
bootstrap's userland into full packages. This part describes what the phase provides, in the
groups its list is written in, the pinned run that opens the list and why its order matters, and
the points in the phase at which the environment itself changes. The package pages follow, in
chapters by shelf, in the shelves' own order, and within a shelf in the order [kpkgdepends]{.idx} builds
them.

## One question decides the phase

`30_foundation` and [Part VII](../07-compilers/intro.md)'s `31_compilers` together install
everything needed to rebuild KDOS from KDOS. They are split by one question: does a port need one
of the large compilers? A port goes here when nothing in its [dependency closure]{.idx} does. The list's
own header says so:

<!-- kfs:include script/phases/30_foundation/packages.txt lines=12-20 pin=070fbee3dda2 -->

The check is [`testing/phaseclosure.py`]{.idx term="phaseclosure.py"}, which [preflight]{.idx term="preflight.sh"} runs. It fails when a dependency pulls a
port into the phase that the list does not name, and when a [shelf heading]{.idx} names a port an earlier
phase installs (see
[Every phase installs exactly its list](../../kdos/05-developer/build-system.md#every-phase-installs-exactly-its-list)).

## The environment

The phase's environment is `chroot.env` with one line added before it, in
`script/phases/30_foundation/phase.env`:

<!-- kfs:include script/phases/30_foundation/phase.env lines=23-27 pin=8a104b58a662 -->

The [`LD_LIBRARY_PATH`]{.idx} line gives the [dynamic loader]{.idx term="dynamic linker"} `/usr/lib`, `/usr/local/lib`, `/usr/lib64`
and `/usr/local/lib64` to search, followed by whatever the variable already held. Unlike `20_selfhost`, the phase keeps `chroot.env`'s
`CC=gcc` and `CXX=g++`: from here on every port is compiled by the `gcc` package that
[Part V](../05-selfhost/intro.md) installed. How the environment reaches each step is in
[The phase environment](../04-chroot/entering.md#the-phase-environment).

## The pinned run

The list opens with names written ahead of the first shelf heading. They are the phase's
[pinned run]{.idx}:

<!-- kfs:include script/phases/30_foundation/packages.txt lines=22-69 pin=7f079ad1f2db -->

They come first because recipes use them without naming them in `depends`. A configure script calls
`pkg-config` through `PKG_CHECK_MODULES`; a `build.sh` runs `autoreconf`, `meson` or `cmake`;
libtool's configure runs `file` to tell a shared library from a static one; and every `build.sh`
runs in bash with the GNU userland. A port further down the list that needs one of them finds it
installed, whatever its `depends` line says.

Three properties of the run matter when it is read:

**It is serial.** Under [`kdosbuild --port-jobs`]{.idx term="--port-jobs"}, the run is the phase's [serial prefix]{.idx}: it builds one
port at a time, and nothing after it starts until its last port is installed (see
[Building a package phase by level](../../kdos/05-developer/build-system.md#building-a-package-phase-by-level)).

**It brings its dependencies with it.** kpkgdepends puts each port's dependencies ahead of it, so
the run's names install far more than themselves. `file` depends on zlib, bzip2, xz, zstd and
libseccomp; `python3` on, among others, libffi, mpdecimal, SQLite, Expat, OpenSSL, gdbm and
util-linux; CMake on OpenSSL, nghttp2, libarchive, libuv and `python3-sphinx`, because its
configure is given `--sphinx-man` and builds its manual pages with Sphinx. Most of the compression and TLS libraries, the whole Python packaging chain under Sphinx,
util-linux and utmps are therefore built inside the run, before any shelf section starts. The
package pages show this: each sits at its place in the resolved order, not at the line that names
it.

**It re-lists earlier ports.** `fortify-headers`, `musl`, `gcc`, `binutils`, `m4`, `tar`,
`diffutils`, `gawk`, `ncurses`, `readline` and `zlib` were installed by `20_selfhost`. A name an
earlier phase installed from the same recipe is skipped, so on an unchanged tree they cost nothing.
When one of their recipes has changed, the port rebuilds at this point, before anything in the
phase uses it. A [re-list]{.idx} is allowed only in a pinned run; under a shelf heading it fails the
[closure check]{.idx term="phaseclosure.py"}.

Two ports of the run need a word of their own. [pkgconf](#port-pkgconf) is configured and built
with [muon](#port-muon), an implementation of Meson in C whose `bootstrap.sh` needs only a C
compiler, so muon can be built before any `pkg-config` exists; pkgconf's `depends` names muon, and
muon's names [scdoc](#port-scdoc), so both come in ahead of it. [bash](#port-bash) is built here as a package for the first time:
no phase before this one lists it, and the port adopts `/usr/bin/bash` and the `sh` link the
bootstrap left.

## When the environment changes

Two lines of `chroot.env` test for a program each time a step sources it, so the phase's
environment changes part-way through, at the step after each program is installed:

- **Ninja.** Once [ninja](#port-ninja) has installed `/usr/bin/ninja`, every later step has
  [`script/bin`]{.idx} first on `PATH`. Its `ninja` adds [`-j$KDOS_JOBS`]{.idx term="KDOS_JOBS"} to a call that names no job count;
  its `meson` adds [`--wrap-mode=nodownload`]{.idx term="--wrap-mode=nodownload"} to every `meson setup` that chooses no wrap mode, so a
  subproject the source does not carry fails the configure by name instead of reaching for the
  network. Before ninja is installed the wrappers stay off, because a `ninja` on `PATH` with no
  `/usr/bin/ninja` behind it would make a probe pick a generator that cannot run.
- **ccache.** Once [ccache](#port-ccache) is installed, every later CMake project is compiled
  through it, by way of `CMAKE_C_COMPILER_LAUNCHER` and `CMAKE_CXX_COMPILER_LAUNCHER`, with its
  cache in [`build/ccache`]{.idx}. CMake projects built before it, CMake itself among them, are compiled
  directly. What the cache keys on and why a hit is byte-identical are in
  [The compiler cache](../../kdos/05-developer/how-kdos-is-built.md#the-compiler-cache).

## What the phase provides

After the pinned run, the list is written in sections, one per shelf, each under a heading of the
form `# <shelf> — <description>`. The groups below cut across those shelves by what a later recipe
gets from them.

### Build systems and build helpers

make, `file`, muon, pkgconf, libtool, autoconf, automake, m4, bison, flex, CMake, Meson and Ninja,
from the pinned run; [ccache](#port-ccache), [intltool](#port-intltool) and
[itstool](#port-itstool) from the `buildtools` shelf; and from `toolchain`, the assemblers
[yasm](#port-yasm) and [nasm](#port-nasm), the generators [swig](#port-swig) and
[gperf](#port-gperf), and the ELF tools [elfutils](#port-elfutils) and
[patchelf](#port-patchelf).

### Perl, Python and Lua

[perl](#port-perl) and [python3](#port-python3) from the pinned run. Python's build backends follow
on the `python` shelf: setuptools, pip, flit-core, packaging, setuptools-scm, poetry-core,
hatchling and the modules they need: the build backends a Python port's `pyproject.toml`
names. The `python-libs` shelf adds Pygments, docutils, MarkupSafe, Jinja2, PyYAML and
Sphinx, which documentation builds run, and Jinja2 also because some C projects generate source
from Jinja templates at build time; the list's own comment names libplacebo's shaders. The
`perl-cpan` shelf carries [perl-xml-parser](#port-perl-xml-parser), which intltool needs, with the
modules it builds on. [lua](#port-lua) and [lua54](#port-lua54) install side by side, every path
of the second one versioned, with the four `lua54-*` modules without which Prosody does not start.

### The documentation toolchain

The `doctools` shelf: [texinfo](#port-texinfo) for info manuals, [mandoc](#port-mandoc), which
reads the manual pages on the image, [help2man](#port-help2man), [doxygen](#port-doxygen),
[asciidoc](#port-asciidoc), [xmlto](#port-xmlto) with [docbook-xml](#port-docbook-xml),
[docbook-xsl](#port-docbook-xsl) and [sgml-common](#port-sgml-common), and [scdoc](#port-scdoc).
With [libxml2](#port-libxml2) and [libxslt](#port-libxslt) from `formats`, and Sphinx above, these
are the tools a port's own documentation build calls. Their absence is not always survivable: the
list's comment on `help2man` notes that a Makefile rule piping `$(PROG) --help` into it exits 127
when it is missing, and takes the build with it.

### Init, devices, accounts and login records

The `base` and `auth` shelves hold the system's foundation:

- [sysvinit](#port-sysvinit), PID 1 and the commands that talk to it;
- [skalibs](#port-skalibs) and [utmps](#port-utmps), the utmp and wtmp service over musl;
  coreutils, sysvinit and util-linux are built against utmps, which the pinned run installs ahead
  of all three;
- [util-linux](#port-util-linux), [kmod](#port-kmod) and [eudev](#port-eudev), the device manager,
  which depends on both;
- [shadow](#port-shadow), the account tools, configured `--with-acl` and linked against utmps;
- the libraries they link from `base-libs`: [attr](#port-attr), [acl](#port-acl),
  [libcap](#port-libcap), [libcap-ng](#port-libcap-ng) and [libffi](#port-libffi), and
  [libseccomp](#port-libseccomp) from `containers`;
- [ca-certificates](#port-ca-certificates), the certificate store TLS clients read;
  [cyrus-sasl](#port-cyrus-sasl), the SASL mechanism loader, client side only; and
  [openldap](#port-openldap), whose `libldap` serves curl's `ldap://` URLs and dirmngr's directory
  lookups.

What these are on a running system is in [Boot and init](../../kdos/03-architecture/boot-and-init.md),
and why utmps is required of every reader and writer of the login records is in
[Login records](../../kdos/03-architecture/boot-and-init.md#login-records).

### The rest of the bootstrap's userland

[coreutils](#port-coreutils), [grep](#port-grep), [patch](#port-patch),
[findutils](#port-findutils), [sed](#port-sed) and [gzip](#port-gzip), together with bash, make and
xz above, are the programs the bootstrap installed by hand. Each port installs over the
bootstrap's files and adopts them, and several gain what the bootstrap build left out: coreutils
is built against ACLs, extended attributes, libcap and utmps, and grep with `--enable-perl-regexp`
against PCRE2. [which](#port-which), [less](#port-less) and [bc](#port-bc) join them as new
programs. [The base userland](../../kdos/03-architecture/packaging.md#the-base-userland) says which
package owns each command-line name and gives the constraints on coreutils' configure line.

### The musl shims

[]{.idx term="musl shims"}musl leaves out some interfaces that glibc carries and that ports expect, and these ports supply
them as libraries of their own:

| Port | What it supplies |
|---|---|
| [libintl](#port-libintl) | The gettext runtime, `libintl`, which musl does not carry in libc |
| [argp-standalone](#port-argp-standalone) | glibc's `argp` argument parser |
| [musl-fts](#port-musl-fts) | `fts(3)`, the file-tree walk |
| [musl-obstack](#port-musl-obstack) | glibc's `obstack` allocator |
| [musl-rpmatch](#port-musl-rpmatch) | `rpmatch(3)`, the yes/no answer matcher |
| [musl-utils](#port-musl-utils) | The `getconf`, `getent` and `iconv` programs, from Alpine's musl package, which musl's own source does not carry |

[elfutils](#port-elfutils) depends on the first four after libintl. `musl-utils` is in the pinned
run, with the userland every `build.sh` runs in.

### Libraries

The remaining sections install the libraries a large part of the tree links: compression and
archives (bzip2, xz, lz4, zstd, LZO, libarchive, Brotli), cryptography (OpenSSL, with
[openssl3](#port-openssl3) beside it as a runtime-only second version), networking (nghttp2,
ngtcp2, nghttp3, libidn2, libpsl, libssh2, curl, libuv, c-ares), formats and text (Expat, libyaml,
libxml2, libxslt, PCRE2), databases (gdbm, SQLite), internationalisation (libunistring, gettext,
ICU) and mpdecimal, which Python's `_decimal` module links. The `vcs` shelf adds [git](#port-git),
and [libgit2](#port-libgit2), which cargo and cargo-c link in `31_compilers` and which every Rust
port's `git2` crate would otherwise compile from a bundled copy.

How two versions of one library live side by side, as `openssl3` and `lua54` do, is in
[A second version beside the first](../../kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

## When the phase is over

By this point every bootstrap step's program has been built again as a port, except the two no phase
rebuilds, kpkg and kinstall; the Linux API headers wait for the `linux` port of `60_kernel`. The tree can configure, compile and document a project of any of the common build
systems with the C, C++ and Fortran compilers of `20_selfhost`. It has no LLVM, no Rust, no Go and
no Haskell yet; [Part VII](../07-compilers/intro.md) adds them. The phase's [snapshot]{.idx} is the restore
point in front of that part, so a failure among the compilers restores this tree instead of
building this phase again (see [Snapshots](../../kdos/05-developer/build-system.md#snapshots)).

## See also

- [The foundation and the compilers](../../kdos/05-developer/how-kdos-is-built.md#the-foundation-and-the-compilers-30_foundation-and-31_compilers): both phases in the whole journey
- [Package lists](../../kdos/05-developer/build-system.md#package-lists): pinned runs and shelf headings
- [Part V](../05-selfhost/intro.md): the ports this phase re-lists
- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [Part VII](../07-compilers/intro.md): the next phase

<!-- book-nav -->
---

*Part VI. The build foundation.* Previous: [Part V. Self-hosting](../05-selfhost/intro.md) · [Contents](../README.md) · Next: [Part VII. Compilers](../07-compilers/intro.md)
