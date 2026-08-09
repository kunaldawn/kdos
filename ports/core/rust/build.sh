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

mkdir -p build/cache/$_date
cp $PORT_SRC/rust-std-$_rust-$_triplet.tar.xz build/cache/$_date/
cp $PORT_SRC/rustc-$_rust-$_triplet.tar.xz build/cache/$_date/
cp $PORT_SRC/cargo-$_cargo-$_triplet.tar.xz build/cache/$_date/

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
tools = ["cargo", "clippy", "rustdoc", "rustfmt"]
description = "kdos"

[install]
prefix = "/usr"

[rust]
channel = "stable"

[target.$_triplet]
llvm-config = "/usr/bin/llvm-config"
crt-static = false
EOF

mkdir "$SRC/rust"
export CARGO_HOME="$SRC/rust"
export RUST_BACKTRACE=1

{ [ ! -e /usr/include/libssh2.h ] || export LIBSSH2_SYS_USE_PKG_CONFIG=1; }

python3 ./x.py build
DESTDIR=$PKG python3 ./x.py install -v
