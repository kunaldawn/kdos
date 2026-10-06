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
# malloc, which is slow under rustc's allocation pattern. codegen-units-std = 1
# compiles the standard library, which every Rust program links, as one unit,
# the setting of upstream's and Alpine's release builds; rustc itself keeps
# the default of 16.
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
codegen-units-std = 1

[target.$_triplet]
llvm-config = "/usr/bin/llvm-config"
crt-static = false
EOF

mkdir "$SRC/rust"
export CARGO_HOME="$SRC/rust"
export RUST_BACKTRACE=1

export LIBSSH2_SYS_USE_PKG_CONFIG=1

python3 ./x.py build --jobs "$KDOS_JOBS"
DESTDIR=$PKG python3 ./x.py install -v --jobs "$KDOS_JOBS"

# rustc-dev is the compiler's own crates (rustc_driver, rustc_middle and the
# rest) as .rmeta files in lib/rustlib/$_triplet/lib, what a rustc_private tool
# links against; crubit's cc_bindings_from_rs, which Chromium needs, is one.
# x.py install has no step for it, so it comes from its dist tarball. The
# tarball carries its own copies of the compiler's shared libraries; they are
# made links to the ones /usr/lib already holds, so the two cannot differ.
python3 ./x.py dist rustc-dev --jobs "$KDOS_JOBS"
mkdir "$SRC/rustc-dev"
tar -xf build/dist/rustc-dev-$version-$_triplet.tar.xz -C "$SRC/rustc-dev" --strip-components=1
"$SRC/rustc-dev/install.sh" --prefix=/usr --destdir="$PKG" --disable-ldconfig
for f in "$PKG"/usr/lib/rustlib/$_triplet/lib/*.so; do
	[ -e "$PKG/usr/lib/${f##*/}" ] || continue
	ln -sf "../../../${f##*/}" "$f"
done
