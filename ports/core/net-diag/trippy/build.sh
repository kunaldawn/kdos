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
cargo build --release --frozen --offline
install -Dm755 target/release/trip $PKG/usr/bin/trip
install -d "$PKG/usr/share/man/man1"
target/release/trip --generate-man > "$PKG/usr/share/man/man1/trip.1"
install -d "$PKG/usr/share/bash-completion/completions" \
	"$PKG/usr/share/zsh/site-functions" "$PKG/usr/share/fish/vendor_completions.d"
target/release/trip --generate bash > "$PKG/usr/share/bash-completion/completions/trip"
target/release/trip --generate zsh > "$PKG/usr/share/zsh/site-functions/_trip"
target/release/trip --generate fish > "$PKG/usr/share/fish/vendor_completions.d/trip.fish"
