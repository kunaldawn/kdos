# Languages

<!-- kfs:phase 40_lang -->

`40_lang` is the first of the five userland phases. It installs the language modules and the
developer tools whose dependencies are all in place once [Part VII](../07-compilers/intro.md) is
done: the `python3-*` and `perl-*` modules, a few more language implementations, build helpers,
debuggers and tracers, language servers, documentation tools, and the version-control tools that
work beside git. This part describes what decides that a port belongs here, how the phase's list is
filed by shelf, the one change the environment makes from this phase on, and which of its ports
have a full page. The package pages follow, in chapters by shelf, in the shelves' own order, and
within a shelf in the order [kpkgdepends]{.idx} builds them.

## What belongs here

Every file of the phase's list opens with the same statement of the rule:

<!-- kfs:include script/phases/40_lang/packages.d/buildtools.txt lines=15-17 pin=a5463e187cae -->

Two things decide a port's phase: its subject, and how far its dependency closure reaches. A Python
module whose [closure]{.idx term="dependency closure"} stops at the compilers is a `40_lang` port. A Python module that needs GTK is
not, whatever its shelf, because a phase may build only on what the phases before it installed;
the `python-gui` modules therefore have their pages in Parts IX to XI, beside the libraries they
bind. The rule, with the phase each kind of port goes to, is in
[Which phase lists a port](../../kdos/05-developer/writing-ports.md#which-phase-lists-a-port), and
[`testing/phaseclosure.py`]{.idx term="phaseclosure.py"} enforces it (see
[Every phase installs exactly its list](../../kdos/05-developer/build-system.md#every-phase-installs-exactly-its-list)).

The ports that every recipe builds with are not here. Perl and Python, the autotools, CMake,
Meson and Ninja, the Python packaging chain under Sphinx, and git itself are
[Part VI](../06-foundation/intro.md)'s; this phase adds to them.

## The list, by shelf

The list is a directory, [`script/phases/40_lang/packages.d/`]{.idx term="packages.d"}, with one file per shelf, named after
it: `devtools.txt` holds the `devtools` ports this phase installs. The files are read in byte order
as if they were one list (see
[Package lists](../../kdos/05-developer/build-system.md#package-lists)). The phase has no
[`00-order.txt`]{.idx} and so no [pinned run]{.idx}: every port's position comes from its dependencies and from
where its name stands in the list.

The chapters below take the shelves in the order of [`ports/shelves`]{.idx}. The ones that carry the phase
are these:

`toolchain`
: Additions to the compilers rather than compilers: `musl-ldd` and `musl-locales` for the C
  library, LLVM's `libclc`, `libunwind-nongnu`, which unwinds another process or a core file as
  well as its own, and Crubit's `cc_bindings_from_rs`, which writes C++ headers for calling Rust
  crates.

`buildtools`, `lang`
: Build helpers that only some recipes use, such as SCons, GN, `autoconf-archive` and `unifdef`,
  and the language implementations whose closure ends at the compilers: LuaJIT, Duktape, OCaml,
  and the JavaScript tooling esbuild, yarn and `typescript-go`.

`devtools`
: Debuggers, tracers and analysers (`strace`, `ltrace`, Valgrind, and the BPF tools `libbpf`,
  `bpftool` and `bcc`), the language servers `rust-analyzer`, `gopls` and `zls`, and command-line
  developer tools such as `just`, `hyperfine` and `difftastic`.

`vcs`, `doctools`
: Version-control front ends and extensions (`tig`, `lazygit`, `gh`, `jujutsu`, `git-lfs`), and
  documentation tools: the Linux `man-pages`, `typst`, `cmark`, `discount` and `gtk-doc`.

`python` and the `python-*` shelves
: `python` holds the build backends and extension tooling a module's build runs: `wheel`,
  `cython`, `maturin`, `meson-python`, `scikit-build-core`, `pybind11` and the rest.
  `python-libs`, `python-net`, `python-sci`, `python-dev` and `python-hw` hold runtime modules,
  by subject. The recipes follow the module shape of
  [Canonical build shapes](../../kdos/05-developer/writing-ports.md#python-perl-haskell-node-and-ocaml-modules).

`perl-cpan`
: CPAN modules beyond the ones Part VI installed.

A chapter lists its ports in build order, but the build does not go shelf by shelf: kpkgdepends
walks the whole list and installs each port after what it depends on, so a `python` port can build
before a `devtools` one that comes earlier in the book. The *Phase* row of each package page gives
the port's place in the phase's real order.

## The environment

The phase's environment is in `script/phases/40_lang/phase.env`, and the other userland phases'
files differ from it only in their title and description:

<!-- kfs:include script/phases/40_lang/phase.env lines=23-26 pin=50eb5ec04ff4 -->

It differs from `31_compilers`' in two ways. It has no `LD_LIBRARY_PATH` line. And [`PORT_REPO`]{.idx}
names `src/system` and `src/art` beside `ports/core`, so kpkg can find KDOS's own programs from
here on (see
[Where kpkg looks for a port](../../kdos/05-developer/build-system.md#where-kpkg-looks-for-a-port)).
This phase builds none of them; Parts IX, X and XII do.

The other change comes from the shared `script/env/chroot.env`, which every chroot phase sources.
From this phase on it exports the switches for Rust's [`-sys` crates]{.idx}:

<!-- kfs:include script/env/chroot.env lines=185-202 pin=27d26b31dbd0 -->

`BASH_SOURCE[1]` is the path of the `phase.env` that sourced the file, so the `case` keeps the
switches out of `20_selfhost`, `30_foundation` and `31_compilers` and exports them in every later
phase. With them set, a Rust program that would compile its own copy of libgit2, oniguruma, SQLite
or zstd links the system's library instead, found through its pkg-config file, so the program
carries a [`DT_NEEDED`]{.idx} entry for the shared library and takes its security fixes with it. A port
whose crate asks for a version the installed library does not satisfy fails in the crate's build
script, which names the library.

## Which ports have full pages

No port of this phase is named in `docs/kfs/data/tier-a.txt`, so none is [Tier A]{.idx}. A port here has a
full page when it is [Tier B]{.idx} (see
[The three tiers](../00-front/reading-a-package-page.md#the-three-tiers)), and in this phase that
is mostly for one reason: it [vendors]{.idx term="vendoring"} its dependencies. The Rust and Go tools
(`rust-analyzer`, `ruff`, `gh`, `jujutsu` and others) and some Python modules carry a second
tarball, a [bundle]{.idx term="vendor bundle"} of their dependencies made in advance so that the build needs no network, and
the recipe's `vendoring =` key names its kind (see
[Vendoring](../../kdos/05-developer/writing-ports.md#vendoring)). The others carry a patch, or
are dependencies of many ports, as `python3-wheel`, `python3-cython` and
`python3-typing-extensions` are of Python modules across the tree. Every other port has a card.

## See also

- [The userland: `40_lang` to `44_apps`](../../kdos/05-developer/how-kdos-is-built.md#the-userland-40_lang-to-44_apps): the five phases in the whole journey
- [Phases, package lists and shelves](../../kdos/03-architecture/packaging.md#phases-package-lists-and-shelves): how a phase and a shelf differ
- [Anatomy of a kpkg install](../04-chroot/kpkg-install.md): the steps every port below goes through
- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [Part IX](../09-system/intro.md): the next phase

<!-- book-nav -->
---

*Part VIII. Languages.* Previous: [Part VII. Compilers](../07-compilers/intro.md) · [Contents](../README.md) · Next: [Part IX. System](../09-system/intro.md)
