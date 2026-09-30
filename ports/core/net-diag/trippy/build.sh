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

# IT NEEDS CAP_NET_RAW AND IS NOT GIVEN IT HERE, the same as bandwhich: raw
# sockets are a capability, and this ships as an ordinary binary to be run as
# root or granted the capability deliberately. There is no third setuid program
# on this system and a network probe is not the one to add.
#
# What it answers that ping and traceroute cannot: WHICH hop is losing packets
# and by how much, continuously, which is the question on a link that works
# except when it does not.
cargo build --release --frozen --offline
install -Dm755 target/release/trip $PKG/usr/bin/trip
install -d "$PKG/usr/share/man/man1"
target/release/trip --generate-man > "$PKG/usr/share/man/man1/trip.1"
install -d "$PKG/usr/share/bash-completion/completions" \
	"$PKG/usr/share/zsh/site-functions" "$PKG/usr/share/fish/vendor_completions.d"
target/release/trip --generate bash > "$PKG/usr/share/bash-completion/completions/trip"
target/release/trip --generate zsh > "$PKG/usr/share/zsh/site-functions/_trip"
target/release/trip --generate fish > "$PKG/usr/share/fish/vendor_completions.d/trip.fish"
