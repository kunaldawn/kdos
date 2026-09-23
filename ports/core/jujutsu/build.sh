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

# THE OPERATION LOG IS THE OFFLINE ARGUMENT. Every jj command is recorded and
# `jj op restore` puts the whole repository back to any earlier moment — which
# matters far more here than on a connected machine, because the usual recovery
# from a bad rebase is to re-clone from the remote and there is no remote. It
# reads and writes ordinary git repositories, so nothing has to be converted
# and git keeps working on the same tree.
#
# The default features stay on: `git` is the whole point, and `watchman` only
# acts when fsmonitor is configured. OPENSSL_NO_VENDOR makes any openssl-sys in
# the graph link the port rather than build a private copy.
export OPENSSL_NO_VENDOR=1
cargo build --release --frozen --offline --bin jj
install -Dm755 target/release/jj $PKG/usr/bin/jj
target/release/jj util install-man-pages "$PKG/usr/share/man"
for _sh in bash zsh fish; do
	target/release/jj util completion $_sh > jj.$_sh
done
install -Dm644 jj.bash $PKG/usr/share/bash-completion/completions/jj
install -Dm644 jj.zsh  $PKG/usr/share/zsh/site-functions/_jj
install -Dm644 jj.fish $PKG/usr/share/fish/vendor_completions.d/jj.fish
