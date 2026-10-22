## About

The Rust compiler, `rustc`, with `cargo`, `clippy`, `rustdoc` and `rustfmt`. A large part of
KDOS is built with it: command-line programs such as `ripgrep`, `fd`, `bat` and `helix`, the
Mozilla browsers and Chromium, `librsvg`, `mesa`, `linux`, `typst` and the Python packages with
Rust extensions all name it in `depends`. It is built in `31_compilers`, the phase that holds the
long compiler builds, so that a failure there restores `30_foundation` and loses minutes, not the
foundation work before it (see
[How KDOS is built](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/how-kdos-is-built.md#the-foundation-and-the-compilers-30_foundation-and-31_compilers)).

KDOS builds Rust differently from upstream's release in three ways. It links the system LLVM, the
`llvm` port, instead of compiling the LLVM inside the source tarball. It links rustc and the
standard library dynamically against musl, where the musl target's default is static. And cargo
links the system libgit2, libssh2, curl, OpenSSL and zlib rather than compiling the copies their
`-sys` crates carry.

Rust is written in Rust, so the build starts from a released compiler: the previous stable
`rustc`, `rust-std` and `cargo` for `x86_64-unknown-linux-musl`, the three extra sources in the
recipe. They are the [stage0]{.idx} compiler that x.py starts from to compile this release.

## Flags

- `targets = "X86"` — the back ends x.py would build if it compiled LLVM. With `llvm-config` set
  below it compiles no LLVM, so this changes nothing: `librustc_driver` links whatever back ends
  the system LLVM has, which the `llvm` port builds with every target.
- `link-shared = true` — link rustc against LLVM's shared libraries (`libLLVM*.so` from the
  `llvm` port) rather than static archives.
- `docs = false` — build no HTML documentation. The image would not ship it: the size policy
  removes HTML manuals.
- `extended = true` — build the tools named in `tools` as well as the compiler.
- `locked-deps = true` — build with the `Cargo.lock` files exactly as the release ships them;
  cargo fails rather than resolve a different dependency.
- `vendor = true` — take every crate from the `vendor/` directory of the source tarball; nothing
  is downloaded.
- `python = "/usr/bin/python3"` — the Python that runs x.py's scripts.
- `tools` — `cargo`, `clippy`, `rustdoc` and `rustfmt`, plus two for the `rust-analyzer` port:
  `"src"` installs the standard library's source under `lib/rustlib/src`, without which
  rust-analyzer resolves nothing in `std` or `core`, and `"rust-analyzer-proc-macro-srv"` installs
  `libexec/rust-analyzer-proc-macro-srv`, without which it expands no derive or attribute macro.
- `description = "kdos"` — a string appended to the version, so `rustc -V` reads
  `rustc <version> (<commit> <date>) (kdos)`. Without it, x.py marks a build from a source
  tarball as "built from a source tarball".
- `profiler = true` — build the `profiler_builtins` runtime from the copy of compiler-rt in the
  tarball. `-C instrument-coverage`, `-C profile-generate` (and so profile-guided optimisation) and
  `cargo-llvm-cov` need it.
- `channel = "stable"` — build the stable channel, which allows only stable language features.
- `jemalloc = true` — link rustc against jemalloc, from the vendored `tikv-jemalloc-sys` crate,
  instead of the C library's `malloc`. musl's allocator is slow under rustc's allocation pattern.
- `codegen-units-std = 1` — compile the standard library, which every Rust program links, as one
  code-generation unit, as upstream's release builds do. rustc itself keeps the default of 16.
- `rustflags = ["-Cstrip=debuginfo"]` — pass `-C strip=debuginfo` to every rustc that x.py runs,
  so nothing the bootstrap links keeps debug information (see
  [What kpkg does around build.sh](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#what-kpkg-does-around-buildsh)).
- `llvm-config = "/usr/bin/llvm-config"` — build against the LLVM that this `llvm-config`
  describes, the `llvm` port, and do not compile the LLVM in `src/llvm-project`. This is why the
  recipe carries the two LLVM patches.
- `llvm-filecheck` — where LLVM's `FileCheck` is: `/usr/lib/llvm/utils/FileCheck`, where the
  `llvm` port installs its utilities. With an external LLVM, x.py's sanity check stops the build
  when it finds no `FileCheck` beside `llvm-config` or under the LLVM library directory, and
  neither is where this tree keeps it.
- `crt-static = false` — link rustc, its tools and the standard library for this target against
  the shared musl `libc.so`. The musl target's own default for programs rustc compiles stays
  static, which is why every Rust port passes `-C target-feature=-crt-static` (see
  [Writing ports](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#rust)).
- `RUST_BACKTRACE=1` — print a backtrace when any Rust program in the build panics, x.py's own
  helpers included, so a failure in the log says where it happened.
- `LIBSSH2_SYS_USE_PKG_CONFIG=1`, `LIBGIT2_NO_VENDOR=1` — make the `libssh2-sys` and
  `libgit2-sys` crates that cargo is built with link the `libssh2` and `libgit2` ports instead of
  compiling their bundled copies. `script/env/chroot.env` exports `LIBGIT2_NO_VENDOR` only from
  `40_lang` on, after this phase.
- `--jobs "$KDOS_JOBS"` — run x.py's builds with the tree's job count.

## Patches

- `rust-llvm23-amx-tf32.patch` — removes the `amx-tf32` x86 target feature from rustc's feature table, together with its intrinsics in `core::arch` (`_tile_mmultf32ps` and `__tile_mmultf32ps`), their tests and their run-time detection in `std_detect`; the system LLVM has no such feature, and a rustc that lists it fails on its first target query with "'+amx-tf32' is not a recognized feature".

## Notes

The stage0 tarballs are copied into `build/cache/<date>/`, where x.py looks before it downloads
anything. The recipe's `_date` and `_rust` must be the `compiler_date` and `compiler_version` that
`src/stage0` in the new source names; when they differ, x.py looks for a directory that is not
there, tries the network, and the build fails in the chroot. After a version bump, read
`src/stage0` and set both.

`x.py install` has no step for `rustc-dev`, the compiler's own crates (`rustc_driver`,
`rustc_middle` and the rest) as `.rmeta` files under `lib/rustlib/x86_64-unknown-linux-musl/lib`.
A tool built against rustc's internals with `#![feature(rustc_private)]` links them; `crubit`,
which Chromium needs, is one. `build.sh` makes the `rustc-dev` dist tarball and installs it. That
tarball carries its own copies of the compiler's shared libraries; each one `/usr/lib` already
holds is replaced with a symbolic link to it, so the two can never differ.

The rest of `build.sh` tidies the install: it deletes the installer's bookkeeping in
`lib/rustlib` (`components`, `install.log`, `manifest-*`, `uninstall.sh`,
`rust-installer-version`), which describes an installation kpkg already tracks, and moves cargo's
bash completion from `/etc/bash_completion.d` to `/usr/share/bash-completion/completions`, where
bash-completion loads it on demand.

The two patches are rustc's own changes for the LLVM release the `llvm` port carries, which is
newer than the LLVM this Rust release is built against. Its header says only that
`rust-llvm23-assign-guid.patch` reapplies an upstream commit; what it does is add LLVM's
`AssignGUIDPass` ahead of the two places rustc writes ThinLTO bitcode, when built against LLVM 23
or later. That LLVM assigns the global identifiers ThinLTO's summaries are keyed on only when the
pass pipeline asks for them.