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

# WITHOUT THE rust-src COMPONENT IT FAILS SILENTLY, which is the one thing
# worth knowing about installing this. Navigation into `std` — go-to-definition
# on `Vec::push`, hover on a trait from core — needs the standard library's
# SOURCE, and with it absent rust-analyzer does not error: it simply reports
# nothing for anything in std, which reads as a language server that half
# works. `ports/core/rust` installs the source tree, so this is satisfied here
# and would not be against a toolchain from rustup with the default profile.
export CARGO_PROFILE_RELEASE_DEBUG=0
cargo build --release --frozen --offline -p rust-analyzer
install -Dm755 target/release/rust-analyzer $PKG/usr/bin/rust-analyzer
