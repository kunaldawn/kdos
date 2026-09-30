#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------

# The system LLVM is newer than the one this release is built against. It has
# no AMX-TF32 extension, and a rustc that lists it fails on its first target
# query with "'+amx-tf32' is not a recognized feature"; the first patch drops
# the feature and its intrinsics. It also no longer assigns the global
# identifiers ThinLTO's summaries are keyed on unless the pipeline asks; the
# second adds that pass where rustc writes ThinLTO bitcode. Both are rustc's
# own changes for this LLVM.
patch -p1 -i $PORT_SRC/rust-llvm23-amx-tf32.patch
patch -p1 -i $PORT_SRC/rust-llvm23-assign-guid.patch

mkdir -p build/cache/$_date
cp $PORT_SRC/rust-std-$_rust-$_triplet.tar.xz build/cache/$_date/
cp $PORT_SRC/rustc-$_rust-$_triplet.tar.xz build/cache/$_date/
cp $PORT_SRC/cargo-$_cargo-$_triplet.tar.xz build/cache/$_date/

# "src" installs lib/rustlib/src, the standard library's source, and
# "rust-analyzer-proc-macro-srv" installs libexec/rust-analyzer-proc-macro-srv:
# the rust-analyzer port resolves nothing in std/core without the first and
# expands no derive or attribute macro without the second.
#
# profiler builds profiler_builtins from src/llvm-project/compiler-rt in this
# tarball; without it -C instrument-coverage, cargo-llvm-cov and PGO fail.
# jemalloc links the vendored tikv-jemalloc-sys into rustc in place of musl's
# malloc, which is slow under rustc's allocation pattern.
cat << EOF > config.toml
[llvm]
targets = "X86"
link-shared = true

[build]
docs = false
extended = true
locked-deps = true
vendor = true
python = "/usr/bin/python3"
tools = ["cargo", "clippy", "rustdoc", "rustfmt", "src", "rust-analyzer-proc-macro-srv"]
description = "kdos"
profiler = true

[install]
prefix = "/usr"

[rust]
channel = "stable"
jemalloc = true

[target.$_triplet]
llvm-config = "/usr/bin/llvm-config"
crt-static = false
EOF

mkdir "$SRC/rust"
export CARGO_HOME="$SRC/rust"
export RUST_BACKTRACE=1

export LIBSSH2_SYS_USE_PKG_CONFIG=1

python3 ./x.py build
DESTDIR=$PKG python3 ./x.py install -v
