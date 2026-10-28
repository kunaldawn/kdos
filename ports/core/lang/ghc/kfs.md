## About
The Glasgow Haskell Compiler, [GHC]{.idx}, with GHCi, its boot libraries and the tools it ships
(`ghc-pkg`, `haddock`, `hsc2hs`, `hpc`, `runghc`). The ports built with it are
[cabal-install](#port-cabal-install) and the two Haskell programs built with both:
[pandoc](#port-pandoc), which renders this book and which [openjdk](#port-openjdk) and
[ripgrep-all](#port-ripgrep-all) depend on, and [shellcheck](#port-shellcheck).

GHC is written in Haskell, so the build starts from a [bootstrap seed]{.idx}: upstream's statically
linked GHC for Alpine, the second `source =` line. The seed is installed into a scratch prefix
under `$SRC_ROOT/seed`, compiles Hadrian (GHC's build system) and GHC's first stage, and is then
left behind; the first stage compiles the second, which is what ships. Hadrian's own Hackage
dependencies come from the vendor bundle, the third `sha256 =` line, so nothing is downloaded
during the build (see
[The Haskell bundle](../../../../docs/kdos/05-developer/writing-ports.md#the-haskell-bundle)).

Hadrian does not install. Its `binary-dist-dir` target lays out a binary distribution under
`_build/bindist`, and that distribution's own `configure` and `make install` put it into `$PKG`,
which is how upstream's release tarballs are installed too.

## Flags
- `--prefix="$SRC_ROOT/seed"` — Installs the seed into a directory under the work tree, never into
  the system; nothing under it reaches `$PKG`.
- `-w` — Tells Hadrian's `bootstrap.py` which GHC to compile Hadrian with: the seed.
- `-d` — The bootstrap plan `bootstrap.py` follows, `hadrian/bootstrap/plan-bootstrap-<seed>.json`
  from the GHC source, where `<seed>` is the recipe's `_seed` with its dots turned to underscores;
  the `hsplan` helper spells the name. The plan names every package Hadrian needs, with its
  version and hashes, solved for the seed's version of GHC.
- `--no-archive` — Builds Hadrian in place without packing it into a distribution archive; the
  build runs it straight from `hadrian/bootstrap/_build/bin`.
- `--build=x86_64-unknown-linux` — The platform triple GHC is configured for, given for build,
  host and target alike (also `--host` and `--target`). It is the triple in the binary
  distribution's directory name, `_build/bindist/ghc-<version>-x86_64-unknown-linux`, which the
  `cd` further down spells out, so the two change together.
- `--with-system-libffi` — Builds the runtime system against the [libffi](#port-libffi) port
  instead of the copy in GHC's source. The choice is made when the runtime is compiled; the binary
  distribution's `configure` has no libffi switch, and autoconf passes the repeated option over
  with a warning about an unrecognised option.
- `--disable-ld-override` — Keeps gcc's default linker, binutils' `ld`, the linker the rest of the
  tree links with. With the override on, GHC's `configure` tries `ld.lld`, then `ld.gold`, then
  `ld`, and tells gcc to use the first that works; [lld](#port-lld) is installed earlier in this
  phase, so GHC would link everything with lld. Both `configure` runs carry it, because the binary
  distribution's makes the same search and records the result in the installed `settings`.
- `--disable-numa` — Builds the runtime system without libnuma. The default, `auto`, links
  [numactl](#port-numactl)'s `libnuma` whenever it is installed, and numactl is a port of
  `41_system`, so a rebuild on a tree past that phase would make `ghc` need a library its recipe
  does not declare, which the `undeclared` gate in `70_image` fails.
- `CONF_GCC_LINKER_OPTS_STAGE2="-Wl,-z,now"` — The link flags of the target toolchain. The first
  configure writes them into Hadrian's `default.target`, which the first-stage compiler links
  with, so the second-stage `ghc` that ships and its libraries bind every symbol at load time
  like the rest of the tree. They are also recorded in the installed compiler's `settings` file
  as the C compiler link flags, so every Haskell program GHC builds, `pandoc` and `shellcheck`
  among them, is linked with `-z,now`. It is passed to both
  `configure` runs, because the binary distribution's `configure` writes the `settings` file that
  is installed. GHC links its programs without PIE; why, and what that leaves out, is in
  [The release flags](../../../../docs/kdos/05-developer/writing-ports.md#the-release-flags).
- `GHC="$_seedghc"` — The compiler GHC's `configure` builds the first stage with: the seed.
- `--flavour=release+no_profiled_libs` — `release` is Hadrian's flavour for a distributed
  compiler: `perf`, which compiles the compiler and the libraries with `-O2` and splits sections
  so a static link can drop unused code, with Haddock documentation (turned off by the next switch)
  and late cost centres for profiling added. `no_profiled_libs` builds no profiling variant of any
  library, so a program on this system cannot be compiled for profiling: the boot libraries have
  no profiled objects for `cabal --enable-profiling` to link against.
- `--docs=no-haddocks` — Builds no Haddock HTML for the libraries, so the system carries no local
  API reference for GHC's boot libraries, and no locally built Haddock page links into them.
- `--docs=no-sphinx-pdfs` — Builds no PDF of the User's Guide.
- `--docs=no-sphinx-html` — Builds no HTML User's Guide. Of GHC's documentation, only the manual
  page `ghc(1)` is built and installed, which is why [python3-sphinx](#port-python3-sphinx) is in
  `depends`.

## Notes
`rmdir`, not `rm -r`, removes `/usr/share/doc/ghc-<version>`, the directory the binary
distribution creates for the documentation that is not built. It fails, and fails the build, if
anything was installed into it, so a change that brings documentation back shows up here.

The plan named by `hsplan` is solved for the seed's GHC version: `bootstrap.py` takes each
package the plan marks as coming with GHC from the seed's own package database, at exactly the
version the plan names, and stops when the seed has another. A new seed therefore needs a plan for it in the GHC
source, and the vendor bundle is generated again from that plan by `ports/fetch`.

The seed's archive unpacks as `ghc-<seed>-x86_64-unknown-linux`, not under the name of the file
it came in, and its `configure` and `make install` run inside that directory.
