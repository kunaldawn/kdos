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

export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true
export GEN_ARTIFACTS="$PWD/gen"

cargo build --release --frozen --offline --bin typst \
	--no-default-features --features embedded-fonts,http-server
install -Dm755 target/release/typst $PKG/usr/bin/typst
install -Dm644 gen/*.1 -t "$PKG/usr/share/man/man1"
install -Dm644 gen/typst.bash "$PKG/usr/share/bash-completion/completions/typst"
install -Dm644 gen/_typst -t "$PKG/usr/share/zsh/site-functions"
install -Dm644 gen/typst.fish -t "$PKG/usr/share/fish/vendor_completions.d"
