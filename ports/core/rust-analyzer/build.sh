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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

export RUSTFLAGS="-C target-feature=-crt-static"

# WITHOUT THE rust-src COMPONENT IT FAILS SILENTLY, which is the one thing
# worth knowing about installing this. Navigation into `std` — go-to-definition
# on `Vec::push`, hover on a trait from core — needs the standard library's
# SOURCE, and with it absent rust-analyzer does not error: it simply reports
# nothing for anything in std, which reads as a language server that half
# works. `ports/core/rust` installs the source tree, so this is satisfied here
# and would not be against a toolchain from rustup with the default profile.
# Proc-macro expansion runs the sysroot's libexec/rust-analyzer-proc-macro-srv,
# which the rust port installs as well; without it every derive stays
# unresolved.
#
# --features jemalloc replaces musl's malloc, which a long-running,
# allocation-heavy server makes slow; tikv-jemalloc-sys is in the vendor bundle.
export CARGO_PROFILE_RELEASE_DEBUG=0
cargo build --release --frozen --offline -p rust-analyzer --features jemalloc
install -Dm755 target/release/rust-analyzer $PKG/usr/bin/rust-analyzer
