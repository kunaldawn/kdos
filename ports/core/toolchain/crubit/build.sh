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

# CHROMIUM'S PIN. The commit is the CRUBIT_REVISION Chromium's
# tools/rust/update_rust.py names, and the chromium port builds against it:
# the headers cc_bindings_from_rs writes include the support library installed
# here, so the two must come from one commit. cc_bindings_from_rs is a
# rustc_private tool linked against librustc_driver, whose file name carries a
# hash of the rustc build; it runs only with the rust port it was built
# against.
tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

# RUSTC_BOOTSTRAP lets the stable compiler take #![feature(rustc_private)].
export RUSTFLAGS="-C target-feature=-crt-static"
export RUSTC_BOOTSTRAP=1
cargo build --release --frozen --offline --bin cc_bindings_from_rs \
	--manifest-path cargo/cc_bindings_from_rs/cc_bindings_from_rs/Cargo.toml
install -Dm755 target/release/cc_bindings_from_rs "$PKG/usr/bin/cc_bindings_from_rs"

# The tree Chromium's build_crubit.py copies beside the tool: the GN rules,
# the licence and the support headers and sources the generated code uses.
install -d "$PKG/usr/share/crubit"
cp -r BUILD.gn LICENSE crubit.gni support "$PKG/usr/share/crubit/"
