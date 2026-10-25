# Compilers

<!-- kfs:phase 31_compilers -->

`31_compilers` adds the large compilers and the language toolchains the rest of the tree is
written in: LLVM and clang with their runtimes, a second LLVM for Zig, Rust and its binding
generators, Go, Zig, GHC and cabal-install, Node.js and Ruby, and the document converters pandoc
and asciidoctor that are built with them. This part explains why these ports are a phase of their
own, how they depend on one another, why LLVM is installed twice, and, in its last section, which
of them start from a compiler that was not built here, and what that means for a system that
calls itself built from source. The package pages follow, in chapters by shelf, in the shelves'
own order, and within a shelf in the order [kpkgdepends]{.idx} builds them.

## Why a phase of its own

The list's header gives the rule:

<!-- kfs:include script/phases/31_compilers/packages.txt lines=12-20 pin=eb5a3b6d42fb -->

A port belongs here when its dependency closure reaches one of the large compilers, and nothing in
[Part VI](../06-foundation/intro.md) does. The split is about time. These ports take most of the
hours between the chroot and `40_lang`, and `30_foundation`'s snapshot sits in front of them, so a
failure here restores that snapshot instead of building the foundation again (see
[The foundation and the compilers](../../kdos/05-developer/how-kdos-is-built.md#the-foundation-and-the-compilers-30_foundation-and-31_compilers)).

The phase's environment is the same as `30_foundation`'s: its `phase.env` sets `CHROOT=1` and the
same `LD_LIBRARY_PATH`, and sources `chroot.env`. Two lines of `chroot.env` matter more here than
anywhere before:

- **The compiler is named.** `CC=gcc` and `CXX=g++` hold through this phase and every later one.
  clang is installed here, and some configure scripts put clang ahead of gcc in the list they
  search; without the two variables, every such port would change compiler from this point on.
- **No `-sys` crate switches yet.** The variables that make Rust's [`-sys` crates]{.idx} link the system
  library are exported from `40_lang` on, never in `20_selfhost` to `31_compilers`. A recipe here
  that wants one sets it itself: `rust`'s `build.sh` sets [`LIBGIT2_NO_VENDOR=1`]{.idx term="LIBGIT2_NO_VENDOR"} and
  `LIBSSH2_SYS_USE_PKG_CONFIG=1`.

## How the ports depend on one another

The order kpkgdepends gives the phase follows from a few dependencies:

- **LLVM comes first.** [rust](#port-rust) depends on [llvm](#port-llvm): its `config.toml`
  points [`llvm-config`]{.idx} at `/usr/bin/llvm-config` and links LLVM as a shared library, so Rust uses
  the system LLVM and builds none of its own. [clang](#port-clang), [lld](#port-lld),
  [compiler-rt](#port-compiler-rt), [openmp](#port-openmp) and [libunwind](#port-libunwind) build
  on it too.
- **Rust comes before most of the rest.** [cbindgen](#port-cbindgen), [bindgen](#port-bindgen),
  which also needs clang's library, and [cargo-c](#port-cargo-c) are Rust programs.
  [ruby](#port-ruby) depends on Rust because it is configured `--enable-yjit`, and [YJIT]{.idx}, Ruby's
  JIT compiler, is written in Rust; Ruby's `configure` checks for a working `rustc` to build it.
  [nodejs](#port-nodejs) depends on Rust because it is configured `--v8-enable-temporal-support`,
  and V8's `Temporal` support is the `temporal_capi` crate that Node's source carries under
  `deps/crates`, compiled with cargo.
- **The Haskell chain is linear.** [ghc](#port-ghc), then [cabal-install](#port-cabal-install),
  which is compiled with that GHC, then [pandoc](#port-pandoc), which is built with both.
- **The documentation tools follow their interpreter.** [asciidoctor](#port-asciidoctor) runs on Ruby, and
  [ccache-manual](#port-ccache-manual) is ccache's manual page, which ccache's own
  `generate-manpage` script renders from `doc/manual.adoc` with asciidoctor. ccache itself is a
  `30_foundation` port, so its manual page is a port of its own in the phase that first has
  asciidoctor.

The list adds one constraint the recipes do not state. [libunwind](#port-libunwind), LLVM's
unwinder, is built in this phase, ahead of `40_lang`, whose `libunwind-nongnu` owns
`/usr/include/libunwind.h` and `libunwind.so`. The `libunwind` package installs under a private
prefix, its recipe's `_prefix` of `/usr/lib/llvm-libunwind`, so by the time `libunwind-nongnu` is
installed it holds none of those paths; if it held them, that install would be a [file conflict]{.idx}.

## LLVM and clang

The current LLVM series is a set of ports that all carry upstream's single `llvm-project` source
archive, each configuring its own directory of it. The `llvm` port sets LLVM's host and default target to
the system's own, [`$(cc -dumpmachine)`]{.idx term="-dumpmachine"}, which is `x86_64-pc-linux-musl` since
[Part V](../05-selfhost/intro.md), and `compiler-rt` builds its runtimes for the same triplet.
The list's comments say what each is for: clang and llvm serve the projects that prefer LLVM,
`openmp` is the `libomp` runtime that `clang -fopenmp` links, and `compiler-rt` is clang's own
runtime libraries: builtins, the profile runtime for source-based coverage, and the
UndefinedBehaviorSanitizer. `lldb` is not here; it is a `41_system` port.

The C and C++ compiler of the system stays GCC. Nothing in the environment makes clang the default,
and a port uses clang only when its recipe asks for it.

## A second LLVM for Zig

Zig is built against one major series of LLVM, and its own CMake check, `cmake/Findllvm.cmake` in
the Zig source, refuses any `llvm-config` outside that series. The tree's LLVM moves on with
upstream, so Zig gets a [slot]{.idx term="version slot"}: [llvm21](#port-llvm21), [clang21](#port-clang21) and
[lld21](#port-lld21) install a second LLVM, clang and lld under a private prefix,
`/usr/lib/llvm21`, that nothing else searches. The prefix is the [recipe helper]{.idx} [`_prefix`]{.idx}, derived
from the version as `/usr/lib/llvm${version%%.*}`, so a bump to another series moves it.

[zig](#port-zig) asks for the slot by name, in its `depends` line and on its CMake line:

<!-- kfs:include ports/core/lang/zig/build.sh lines=12-27 pin=07e1f1fe8c19 -->

`CMAKE_PREFIX_PATH=/usr/lib/llvm21` makes Zig's `llvm-config` search look in the prefix before
`PATH`, so it finds the slot's `llvm-config` and not the system's. `ZIG_STATIC_LLVM=OFF` links the
slot's shared libraries, and the `zig` binary records `/usr/lib/llvm21/lib` as its [run path]{.idx}. Every
other port that looks for LLVM, on `PATH` or through CMake's default search, still
finds the system one. How the slot is kept from colliding with the system LLVM, file by file, is in
[A second version beside the first](../../kdos/05-developer/writing-ports.md#a-second-version-beside-the-first).

## Trust and binary bootstraps

Rust, Go, Zig, GHC, OpenJDK and OCaml are each written in their own language, so building one
needs a working copy of it first. That copy is the [bootstrap seed]{.idx}. KDOS builds every
shipped compiler from source, but these start from a seed that upstream compiled. This section
names every port in the tree that does, in this phase and in later ones. The list was taken from
the recipes of every phase, by reading each `source =` line that fetches an upstream build rather
than source, and from the source archives of the self-hosting compilers, which can carry a compiler
image of their own. Prebuilt files that are not compilers, such as firmware or Wine's Gecko engine,
are another class, listed in the inventory linked at the end of this section.

### Seeds fetched beside the source

| Port | Phase | The seed | What it builds |
|---|---|---|---|
| [rust](#port-rust) | `31_compilers` | Upstream's stage-0 `rustc`, `rust-std` and `cargo` for `x86_64-unknown-linux-musl`, three `source =` lines | The Rust compiler and cargo. The seed's date and version are the ones the Rust source names in its own `src/stage0` file |
| [go](#port-go) | `31_compilers` | Upstream's `linux-amd64` Go release, given to `make.bash` as `GOROOT_BOOTSTRAP` | The Go toolchain |
| [ghc](#port-ghc) | `31_compilers` | Upstream's statically linked GHC for Alpine, installed into the build directory and never further | Hadrian, GHC's build system, and GHC's first stage, which then compiles the second |
| [openjdk](#port-openjdk) | `42_graphics` | Adoptium's Temurin JDK for Alpine (musl), given to configure as `--with-boot-jdk` | The class library and tools of the JDK that ships |
| [kodi](#port-kodi) | `44_apps` | The Apache Groovy binary distribution and the Commons Lang and Commons Text jars | Nothing that ships: Kodi generates its Python add-on bindings with a Groovy script, and Groovy is not a port |

Rust's recipe shows the shape. The helpers name the seed's date and version, and the seed is three
more `source =` lines with three more hashes:

<!-- kfs:include ports/core/lang/rust/kpkgbuild lines=15-27 pin=7222ba2ba420 -->

The first `sha256 =` line names `rust-<version>.tar.xz`, not the URL's `rustc-…-src.tar.xz`: kpkg
saves the first source as `<name>-<version>.<ext>` whatever the URL's basename is, and a later one
under its basename (see
[Sources, and what each one becomes](../../kdos/05-developer/writing-ports.md#sources-and-what-each-one-becomes)).

Its `build.sh` copies the three archives to where Rust's bootstrap looks before it would download
them:

<!-- kfs:include ports/core/lang/rust/build.sh lines=15-18 pin=31b248894fc7 -->

### Seeds inside the source archive

| Port | Phase | The seed | What it builds |
|---|---|---|---|
| [zig](#port-zig) | `31_compilers` | `stage1/zig1.wasm`, a WebAssembly build of the compiler | The build translates it to C with the `wasm2c` in the same directory, compiles that, and uses the result to start Zig's own bootstrap |
| [ocaml](#port-ocaml) | `40_lang` | `boot/ocamlc` and `boot/ocamllex`, bytecode images of the compiler and the lexer generator | They run on the `ocamlrun` the build compiles first, and the build compiles every compiler it installs from source |

Two archives carry precompiled code that is not a seed, and their ports keep it out of the package.
[guile](#port-guile)'s tarball ships bytecode for its own Scheme modules under `prebuilt/`, and the
port's `build.sh` deletes every one of those `.go` files before configure, so Guile compiles its
modules itself. [go](#port-go)'s source tarball carries upstream-compiled `.syso` objects for the
race detector and the BoringCrypto module, and its `build.sh` deletes them from what it installs.

### What is built from the seeds

No seed is installed. [What is built from source](../../kdos/05-developer/writing-ports.md#what-is-built-from-source)
makes it a rule that a seed never reaches the package: what ships is what the seed built, the
`rustc`, `cargo`, `go`, `zig`, `ghc`, `java` and `ocamlc` of the packages, and everything they compile in
turn. In this phase that is [cabal-install](#port-cabal-install) and [pandoc](#port-pandoc),
compiled by the packaged GHC from the Hackage sources their recipes vendor;
[cbindgen](#port-cbindgen), [bindgen](#port-bindgen) and [cargo-c](#port-cargo-c), compiled by the
packaged Rust; and Ruby's YJIT and Node's `Temporal` support, compiled by the same Rust. In later
phases it is every Rust, Go, Haskell, Java and OCaml port in the tree. None of those has a seed of
its own; each trusts the seed of its compiler.

### What "from source" means, then

A seed is a source like any other in two ways. Each is pinned by a `sha256 =` line in its recipe,
its own line for a seed fetched beside the source and the source archive's line for one inside it,
and each is fetched by [`make fetch`]{.idx} and kept in the [source archive]{.idx} under that hash (see
[Sources and the archive](../01-preparing/sources.md)), so the build stays offline and a changed
seed fails the checksum. And no seed's own bytes are in the image.

It differs in one way that matters. A seed is a program nobody here compiled, and it compiles the
first stage of a compiler that is then shipped. What the seed did to that stage can be checked
only as far as the compiler's own bootstrap checks it; it cannot be read from this tree. Every
shipped Rust, Go, Haskell, Java, OCaml and Zig program is built by a compiler whose ancestry
passes through one of these seeds. The C toolchain has an ancestor of the same kind: the
compiler of the [build container]{.idx}, which compiles the [cross compiler]{.idx term="cross toolchain"} of
[Part II](../02-cross/intro.md), and through it everything after.

The complete inventory of what the tree installs without compiling it here, of which the seeds
are one class, is in
[What is not built from source](../../kdos/01-philosophy/why-kdos.md#what-is-not-built-from-source),
with the seeds under [Bootstrap seeds](../../kdos/01-philosophy/why-kdos.md#bootstrap-seeds).

## See also

- [The foundation and the compilers](../../kdos/05-developer/how-kdos-is-built.md#the-foundation-and-the-compilers-30_foundation-and-31_compilers): both phases in the whole journey
- [Canonical build shapes](../../kdos/05-developer/writing-ports.md#canonical-build-shapes): how a Rust, Go, Zig or Haskell port is written
- [The Haskell bundle](../../kdos/05-developer/writing-ports.md#the-haskell-bundle): how GHC, cabal-install and pandoc are built offline
- [Part VI](../06-foundation/intro.md): the phase before
- [Reading a package page](../00-front/reading-a-package-page.md): how the pages below are laid out
- [Part VIII](../08-lang/intro.md): the next phase

<!-- book-nav -->
---

*Part VII. Compilers.* Previous: [Part VI. The build foundation](../06-foundation/intro.md) · [Contents](../README.md) · Next: [Part VIII. Languages](../08-lang/intro.md)
