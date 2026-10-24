## About
[bindgen]{.idx} reads C and C++ headers through libclang and writes the Rust declarations that
call into them. This port is its command-line program, `bindgen`, for builds that run it as a
tool rather than as a crate: the kernel's Rust support ([linux](#port-linux)), which needs it for
`CONFIG_RUST`, [mesa](#port-mesa) and [chromium](#port-chromium), and [waypipe](#port-waypipe).
Rust crates that use bindgen as a library vendor their own copy and do not need this port.

It is a Rust program, built from its vendor bundle with cargo offline, and is built with
upstream's `runtime` feature: `libclang` is not linked into it but loaded when `bindgen` runs,
which is why [clang](#port-clang) is in `depends`.

## Flags
- `LIBCLANG_PATH=/usr/lib/` — The directory clang-sys searches first for `libclang.so`. With the
  `runtime` feature clang-sys's build script links nothing, so the search runs when `bindgen` is
  run, and reads the variable from that environment, not this one.
- `LIBCLANG_STATIC_PATH=/usr/lib/` — Where clang-sys looks for `libclang.a` and the LLVM and clang
  archives for a static link. Only the `static` feature reads it, and that feature is not enabled
  here, so it has no effect on this build.
- `-C link-arg=-lzstd` — Adds `-lzstd` to the final link. Nothing in `bindgen` calls zstd, and the
  tree's `LDFLAGS` link with `--as-needed`, which drops a library no object uses: the installed
  binary's `NEEDED` entries are `libgcc_s.so.1` and `libc.so` alone.
- `-C target-feature=-crt-static` — Links against musl dynamically. Rust's musl target links
  statically by default, and a static program cannot load `libclang.so` at run time; see
  [rust](https://github.com/kunaldawn/kdos/blob/main/docs/kdos/05-developer/writing-ports.md#rust).
- `-p bindgen-cli` — Builds the command-line package of the workspace, not the library alone.
- `--no-default-features` — Drops the package's default features, so the next switch alone
  decides them.
- `--features logging,runtime` — The features to build: `logging`, for `RUST_LOG` diagnostics, and
  `runtime`, which loads `libclang` with `dlopen` when `bindgen` runs. These are the package's
  default features, named.
